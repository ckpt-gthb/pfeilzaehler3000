/*
  PfeilZaehler3000 – bow-mounted automatic shot logger
  ------------------------------------------------
  Board   : Seeed Studio XIAO nRF52840 **Sense**
            Arduino core: "Seeed nRF52 Boards" (the NON-mbed one – needed for Bluefruit + LittleFS)
  Sensors : onboard LSM6DS3TR-C (gyro + accel, internal I2C bus Wire1)
            ADXL375 high-g accelerometer breakout on Wire (D4 = SDA, D5 = SCL)
  Storage : internal flash (LittleFS), ~1400 shots with details between syncs
  Sync    : Bluetooth LE, custom service, read by the PfeilZaehler3000 web app

  A shot is counted when ALL of these happen:
    1. the bow was held still (aiming) for at least MIN_AIM_MS
    2. a sharp jolt above SPIKE_G hits the riser (string stops -> release shock)
    3. the riser keeps ringing (vibration above RING_G) for the next ~150 ms
  Other archers' shots never reach this sensor, so a shared target is not a problem.

  Shot details (v1.7): the last 30 s of bow angle and rotation are kept in RAM. At every shot the
  pod looks back and finds the raise (bow lowered -> aiming angle), draw and set, and the final
  hold; it also measures the aim movement in the last second, the bow angle and cant at release
  and the follow-through (how the bow moves in the first 0.5 s after release).
  Assumed mounting: bolt in a front bushing, so the sensor's Z axis points along the arrow,
  lid towards the target.

  No libraries to install – everything used ships with the Seeed nRF52 core.
*/

#include <Arduino.h>
#include <Wire.h>
#include <bluefruit.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>

using namespace Adafruit_LittleFS_Namespace;

// Data records – defined here, before any function, because the Arduino IDE
// inserts function prototypes above the first function when it compiles.
struct __attribute__((packed)) ShotRec {
  uint32_t t;        // unix seconds (or seconds since boot if the clock was never set)
  uint16_t peak10;   // release jolt, g * 10
  uint8_t  aim100;   // aiming stillness before release, units of 100 ms (max 25.5 s)
  uint8_t  flags;    // bit0: clock set by phone since boot  bit1: clock restored after reboot (approx.)
                     // bit6: end mark ("new end" pressed on phone or watch), not an arrow
                     // bit7: arrow deleted on phone or watch
};
#define FLAG_ENDMARK 0x40
#define FLAG_DELETED 0x80

// Shot details, one per record in a second file (all zero for end marks and old records).
// Times: units of 100 ms, 0 = not found, 255 = 25.5 s or more.
struct __attribute__((packed)) ShotDetail {
  uint8_t raise100;  // raise: from the lowered bow (pre-draw) until the aiming angle is reached
  uint8_t draw100;   // draw and set: from reaching the aiming angle until the final hold starts
  uint8_t hold100;   // hold: the bow almost still (below HOLD_DPS) until the release
  uint8_t wander;    // aim movement in the last second before release, RMS in 0.01 degrees
  int8_t  pitch2;    // bow angle at release, 0.5 degrees, + = aiming up
  int8_t  cant2;     // cant at release, 0.5 degrees, + = top limb to the right (after "set level")
  int8_t  dropDeg;   // follow-through 0.5 s after release: + = bow tips forward / down, degrees
  int8_t  sideDeg;   // follow-through sideways: + = to the right, degrees
};

struct __attribute__((packed)) Settings {    // shared by phone page and watch, last change wins
  uint8_t  magic;            // SETTINGS_MAGIC
  uint8_t  arrowsPerEnd;     // 1–24, default 6
  uint16_t endPauseS;        // pause that starts a new end, 20–900 s, default 100
  uint8_t  flags;            // bit0: cant zero set ("set level")
  uint8_t  rsv;
  int16_t  cantRef10;        // roll angle of the pod when the bow is level, 0.1 degrees
};
#define SETTINGS_MAGIC 0x5E

struct __attribute__((packed)) MetaV1 {      // v1.0–1.4 layout, read once and converted
  uint32_t magic;
  uint32_t baseIndex;
  uint32_t lastEpoch;
};

struct __attribute__((packed)) Meta {
  uint32_t magic;
  uint32_t baseIndex;        // number of records already cleared from the pod after syncs
  uint32_t lastEpoch;        // last known time, restores the clock after a reboot
  int32_t  tzOffset;         // local time offset in seconds (sent by phone / watch) for "today"
  uint32_t lastEndMarkT;     // time of the last "new end" mark
  uint32_t ringFromIndex;    // day counters only count records from this index ("reset all stats")
  uint32_t lifetimeArchived; // arrows in records that were already cleared from the pod
};

struct __attribute__((packed)) DayCnt {      // arrows per local day, for today / week / month
  int32_t  day;              // days since 1 Jan 1970, local time
  uint16_t n;
};

struct __attribute__((packed)) HistSample {  // motion history, 50 Hz (see shot detection)
  int16_t pitch10;   // angle of the pod's Z axis (= arrow) above horizontal, 0.1 degrees
  uint8_t rate2;     // smoothed rotation, 0.5 deg/s (max 127 deg/s)
};

struct CtrlOp {                              // control command waiting for the main loop
  uint8_t b[8];
  uint8_t len;
};

// =====================================================================
//  TUNING – adjust after the calibration session (see build guide)
// =====================================================================
#define SPIKE_G           12.0f    // release jolt threshold, g above resting value
#define RING_G             2.0f    // vibration level that counts as "ringing"
#define RING_MIN_SAMPLES   6       // ringing samples needed 10–150 ms after the jolt
#define STILL_DPS         25.0f    // rotation below this (deg/s) = holding still
#define MIN_AIM_MS        600      // minimum still time right before the release
#define REFRACTORY_MS     1500     // dead time after a counted shot
#define IDLE_AFTER_MS     180000UL // back to power-saving idle after 3 min without movement
#define WAKE_DELTA_G      0.08f    // movement that wakes the pod from idle
#define MOVE_DPS          15.0f    // rotation (after gyro offset removal) that keeps the pod awake
#define MOVE_G             1.5f    // ADXL jolt that keeps the pod awake (sensor noise peaks ~0.5 g)
#define CALIBRATION_LOG   1        // 1 = print every jolt to USB serial (115200 baud)
#define USE_MOTOR         0        // 1 = buzz a coin vibration motor on each counted shot
#define MOTOR_PIN         D2

// shot phases (first estimates – check them with the Serial Monitor, see build guide)
#define HOLD_DPS           8.0f    // smoothed rotation below this = final hold
#define AWAY_DEG          15.0f    // bow angle this far from the release angle = lowered (pre-draw)
#define LEVEL_DEG          4.0f    // bow angle within this of the release angle = raised to aim
#define REST_DPS           5.0f    // slower than this for 0.3 s while lowered = resting, raise not begun
#define FOLLOW_MS          500     // follow-through measured over this time after the release

#define MAX_CONNECTIONS   3        // phone page + Garmin watch, plus one spare for a stale link

#define FW_VERSION        17   // v1.7: shot details (raise, draw, hold, aim movement, angle, cant,
                               //       follow-through), settings stored in the pod, no wake-up counter
                               // v1.6: own name per pod ("PfeilZaehler3000-A3F2"), pod ID in the advertising
                               // v1.5: shared today/week/month counters, delete arrows, end marks, 3 links
                               // v1.4: phone page and Garmin watch can be connected at the same time
                               // v1.3: reports wake-ups and awake time to the app (battery diagnosis)
                               // v1.2: pod really goes idle after 3 min, battery re-measured on every sync

// =====================================================================
//  I2C helpers
// =====================================================================
#if defined(WIRE_INTERFACES_COUNT) && (WIRE_INTERFACES_COUNT > 1)
  #define IMU_WIRE Wire1
#else
  #error "This board variant has no second I2C bus. Select 'Seeed XIAO nRF52840 Sense' in the non-mbed Seeed nRF52 core."
#endif

static void i2cWrite(TwoWire &w, uint8_t addr, uint8_t reg, uint8_t val) {
  w.beginTransmission(addr);
  w.write(reg);
  w.write(val);
  w.endTransmission();
}

static bool i2cRead(TwoWire &w, uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t n) {
  w.beginTransmission(addr);
  w.write(reg);
  if (w.endTransmission(false) != 0) return false;
  if (w.requestFrom(addr, n) != n) return false;
  for (uint8_t i = 0; i < n; i++) buf[i] = w.read();
  return true;
}

static uint8_t i2cRead8(TwoWire &w, uint8_t addr, uint8_t reg) {
  uint8_t v = 0xFF;
  i2cRead(w, addr, reg, &v, 1);
  return v;
}

// =====================================================================
//  LSM6DS3TR-C (onboard) – gyro for "aiming stillness", accel for wake-up
// =====================================================================
const uint8_t LSM_ADDR = 0x6A;
bool lsmOk = false;

static void lsmActive() {            // 104 Hz, ±4 g, 500 dps
  i2cWrite(IMU_WIRE, LSM_ADDR, 0x15, 0x00);  // CTRL6_C: high-performance accel
  i2cWrite(IMU_WIRE, LSM_ADDR, 0x10, 0x48);  // CTRL1_XL
  i2cWrite(IMU_WIRE, LSM_ADDR, 0x11, 0x44);  // CTRL2_G
}

static void lsmIdle() {              // accel 12.5 Hz low-power, gyro off
  i2cWrite(IMU_WIRE, LSM_ADDR, 0x15, 0x10);  // CTRL6_C: XL_HM_MODE = low power
  i2cWrite(IMU_WIRE, LSM_ADDR, 0x10, 0x18);  // CTRL1_XL: 12.5 Hz, ±4 g
  i2cWrite(IMU_WIRE, LSM_ADDR, 0x11, 0x00);  // CTRL2_G: power down
}

// Free an I2C bus that a sensor holds stuck (e.g. after a reset in the middle of a transfer):
// clock SCL up to 9 times until the slave releases SDA, then send a STOP.
static void i2cRecover(uint8_t sda, uint8_t scl) {
  pinMode(sda, INPUT_PULLUP);
  pinMode(scl, OUTPUT);
  for (int i = 0; i < 9 && digitalRead(sda) == LOW; i++) {
    digitalWrite(scl, LOW);  delayMicroseconds(5);
    digitalWrite(scl, HIGH); delayMicroseconds(5);
  }
  pinMode(sda, OUTPUT);
  digitalWrite(sda, LOW);  delayMicroseconds(5);
  digitalWrite(scl, HIGH); delayMicroseconds(5);
  digitalWrite(sda, HIGH); delayMicroseconds(5);
  pinMode(sda, INPUT);
  pinMode(scl, INPUT);
}

// true if both I2C lines are high (free). Never start a transfer on a blocked bus:
// the nRF52 Wire library would wait forever.
static bool i2cLinesFree(uint8_t sda, uint8_t scl) {
  pinMode(sda, INPUT_PULLUP);
  pinMode(scl, INPUT_PULLUP);
  delayMicroseconds(20);
  return digitalRead(sda) == HIGH && digitalRead(scl) == HIGH;
}

static bool lsmBegin() {
#ifdef PIN_LSM6DS3TR_C_POWER
  // The onboard IMU is powered from this pin: high drive (as in Seeed's LSM6DS3 library),
  // and a full power cycle, because with a battery attached the IMU never loses power and
  // can stay stuck after a reset. Its I2C lines are pulled low while it is off.
  nrf_gpio_cfg(g_ADigitalPinMap[PIN_LSM6DS3TR_C_POWER], NRF_GPIO_PIN_DIR_OUTPUT, NRF_GPIO_PIN_INPUT_DISCONNECT,
               NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_H0H1, NRF_GPIO_PIN_NOSENSE);
  digitalWrite(PIN_LSM6DS3TR_C_POWER, LOW);
  pinMode(PIN_WIRE1_SDA, OUTPUT); digitalWrite(PIN_WIRE1_SDA, LOW);
  pinMode(PIN_WIRE1_SCL, OUTPUT); digitalWrite(PIN_WIRE1_SCL, LOW);
  delay(100);
  pinMode(PIN_WIRE1_SDA, INPUT);
  pinMode(PIN_WIRE1_SCL, INPUT);
  digitalWrite(PIN_LSM6DS3TR_C_POWER, HIGH);
  delay(50);
  i2cRecover(PIN_WIRE1_SDA, PIN_WIRE1_SCL);
  if (!i2cLinesFree(PIN_WIRE1_SDA, PIN_WIRE1_SCL)) return false;
#endif
  IMU_WIRE.begin();
  IMU_WIRE.setClock(400000);
  if (i2cRead8(IMU_WIRE, LSM_ADDR, 0x0F) != 0x6A) return false;   // WHO_AM_I
  i2cWrite(IMU_WIRE, LSM_ADDR, 0x12, 0x44);  // CTRL3_C: BDU + auto-increment
  lsmActive();
  return true;
}

static bool lsmReadAccel(float a[3]) {       // g
  uint8_t b[6];
  if (!i2cRead(IMU_WIRE, LSM_ADDR, 0x28, b, 6)) return false;
  for (int i = 0; i < 3; i++) a[i] = (int16_t)(b[2 * i] | (b[2 * i + 1] << 8)) * 0.000122f;
  return true;
}

static bool lsmReadGyro(float g[3]) {        // deg/s
  uint8_t b[6];
  if (!i2cRead(IMU_WIRE, LSM_ADDR, 0x22, b, 6)) return false;
  for (int i = 0; i < 3; i++) g[i] = (int16_t)(b[2 * i] | (b[2 * i + 1] << 8)) * 0.0175f;
  return true;
}

// =====================================================================
//  ADXL375 (external) – ±200 g, catches the release shock without clipping
// =====================================================================
uint8_t adxlAddr = 0x53;
bool adxlOk = false;

static bool adxlBegin() {
  i2cRecover(PIN_WIRE_SDA, PIN_WIRE_SCL);     // free the external bus too
  if (!i2cLinesFree(PIN_WIRE_SDA, PIN_WIRE_SCL)) return false;
  Wire.begin();
  Wire.setClock(400000);
  const uint8_t candidates[2] = {0x53, 0x1D};
  bool found = false;
  for (uint8_t i = 0; i < 2 && !found; i++) {
    if (i2cRead8(Wire, candidates[i], 0x00) == 0xE5) { adxlAddr = candidates[i]; found = true; }
  }
  if (!found) return false;
  i2cWrite(Wire, adxlAddr, 0x31, 0x0B);  // DATA_FORMAT
  i2cWrite(Wire, adxlAddr, 0x2C, 0x0D);  // BW_RATE: 800 Hz
  i2cWrite(Wire, adxlAddr, 0x2D, 0x08);  // POWER_CTL: measure
  return true;
}

static void adxlStandby() { i2cWrite(Wire, adxlAddr, 0x2D, 0x00); }
static void adxlMeasure() { i2cWrite(Wire, adxlAddr, 0x2D, 0x08); }

static bool adxlRead(float a[3]) {           // g
  uint8_t b[6];
  if (!i2cRead(Wire, adxlAddr, 0x32, b, 6)) return false;
  for (int i = 0; i < 3; i++) a[i] = (int16_t)(b[2 * i] | (b[2 * i + 1] << 8)) * 0.049f;
  return true;
}

// =====================================================================
//  Time keeping (phone sets the clock on every sync)
// =====================================================================
uint32_t epochBase = 0;      // unix time at uptime 0
bool timeExact = false;      // true once the phone has set the clock since boot

static uint32_t uptimeSec() {  // overflow-safe (call at least every 49 days – the loop does)
  static uint32_t prev = 0;
  static uint64_t acc = 0;
  uint32_t m = millis();
  acc += (uint32_t)(m - prev);
  prev = m;
  return (uint32_t)(acc / 1000ULL);
}

static uint32_t nowEpoch() { return epochBase + uptimeSec(); }

// =====================================================================
//  Shot log in internal flash
// =====================================================================
#define LOG_FILE    "/shots.bin"
#define META_FILE   "/meta.bin"
#define DETAIL_FILE "/detail.bin"     // [u32 first local index with details][ShotDetail ...]
#define SET_FILE    "/settings.bin"
#define LOG_LIMIT_BYTES (22 * 1024)   // shots + details together

// (ShotRec and Meta are defined at the top of the file)

#define META_MAGIC_V1 0xA11C0DE5
#define META_MAGIC    0xA11C0DE6
Meta meta = {META_MAGIC, 0, 0, 0, 0, 0, 0};
uint32_t localCount = 0;
uint32_t detailFrom = 0;      // local index of the first record with details (older firmware had none)
uint32_t detailCount = 0;     // details stored in DETAIL_FILE
bool clockRestored = false;
Settings settings = {SETTINGS_MAGIC, 6, 100, 0, 0, 0};

static void saveSettings() {
  InternalFS.remove(SET_FILE);
  File f(InternalFS);
  if (f.open(SET_FILE, FILE_O_WRITE)) {
    f.write((const uint8_t *)&settings, sizeof(settings));
    f.close();
  }
}

static uint32_t storedBytes() {
  return localCount * sizeof(ShotRec) + (detailCount ? 4 + detailCount * sizeof(ShotDetail) : 0);
}

static void saveMeta() {
  InternalFS.remove(META_FILE);
  File f(InternalFS);
  if (f.open(META_FILE, FILE_O_WRITE)) {
    f.write((const uint8_t *)&meta, sizeof(meta));
    f.close();
  }
}

static void loadStorage() {
  InternalFS.begin();
  File f(InternalFS);
  if (f.open(META_FILE, FILE_O_READ)) {
    uint8_t raw[sizeof(Meta)];
    int n = f.read(raw, sizeof(raw));
    f.close();
    Meta m;
    MetaV1 m1;
    memcpy(&m, raw, sizeof(m));
    memcpy(&m1, raw, sizeof(m1));
    if (n == (int)sizeof(Meta) && m.magic == META_MAGIC) {
      meta = m;
    } else if (n == (int)sizeof(MetaV1) && m1.magic == META_MAGIC_V1) {   // update from v1.4 or older
      meta.baseIndex = m1.baseIndex;
      meta.lastEpoch = m1.lastEpoch;
      meta.lifetimeArchived = m1.baseIndex;   // cleared records were arrows
      saveMeta();
    }
  }
  if (meta.lastEpoch > 1600000000UL) {       // restore approximate clock
    epochBase = meta.lastEpoch + 60;
    clockRestored = true;
  }
  if (f.open(LOG_FILE, FILE_O_READ)) {
    localCount = f.size() / sizeof(ShotRec);
    f.close();
  }
  detailFrom = localCount;                    // no details yet: they start with the next record
  detailCount = 0;
  if (f.open(DETAIL_FILE, FILE_O_READ)) {
    uint32_t from = 0;
    if (f.read((uint8_t *)&from, 4) == 4 && from <= localCount) {
      uint32_t n = (f.size() - 4) / sizeof(ShotDetail);
      if (from + n == localCount) { detailFrom = from; detailCount = n; }   // in step with the log
    }
    f.close();
  }
  if (!detailCount) InternalFS.remove(DETAIL_FILE);
  Settings s;
  if (f.open(SET_FILE, FILE_O_READ)) {
    if (f.read((uint8_t *)&s, sizeof(s)) == (int)sizeof(s) && s.magic == SETTINGS_MAGIC) settings = s;
    f.close();
  }
}

static bool appendShot(const ShotRec &r, const ShotDetail &d) {
  if (storedBytes() + sizeof(ShotRec) + sizeof(ShotDetail) + 4 > LOG_LIMIT_BYTES) return false;
  File f(InternalFS);
  if (!f.open(LOG_FILE, FILE_O_WRITE)) return false;
  f.seek(f.size());
  bool ok = f.write((const uint8_t *)&r, sizeof(r)) == sizeof(r);
  f.close();
  if (!ok) return false;
  localCount++;
  // detail file: kept in step with the log (record localCount-1 = detail detailFrom+detailCount)
  if (f.open(DETAIL_FILE, FILE_O_WRITE)) {
    if (f.size() < 4) {
      detailFrom = localCount - 1;
      detailCount = 0;
      f.seek(0);
      f.write((const uint8_t *)&detailFrom, 4);
    }
    f.seek(4 + detailCount * sizeof(ShotDetail));
    if (f.write((const uint8_t *)&d, sizeof(d)) == (int)sizeof(d)) detailCount++;
    f.close();
  }
  if (detailFrom + detailCount != localCount) {     // out of step (flash full?): no details from here
    InternalFS.remove(DETAIL_FILE);
    detailFrom = localCount;
    detailCount = 0;
  }
  return true;
}

// ---------------- day counters (today / week / month, shared with phone and watch) ----------------
#define RING_DAYS    40
#define ARCHIVE_FILE "/days.bin"
DayCnt   ring[RING_DAYS];
uint32_t lifetime  = 0;      // arrows ever counted, minus deleted ones
uint32_t lastShotT = 0;      // time of the last arrow

static int32_t dayOf(uint32_t t) { return (int32_t)(((int64_t)t + meta.tzOffset) / 86400); }
static bool isArrow(const ShotRec &r) { return !(r.flags & (FLAG_DELETED | FLAG_ENDMARK)); }

static void ringAdd(uint32_t t, int d) {
  if (t < 1600000000UL) return;               // no date, not in any day
  int32_t day = dayOf(t);
  int slot = -1, oldest = 0;
  for (int i = 0; i < RING_DAYS; i++) {
    if (ring[i].n && ring[i].day == day) { slot = i; break; }
    if (!ring[i].n) { if (slot < 0) slot = -2 - i; continue; }
    if (ring[oldest].n && ring[i].day < ring[oldest].day) oldest = i;
  }
  if (slot >= 0) {
    int v = (int)ring[slot].n + d;
    ring[slot].n = v < 0 ? 0 : (v > 65535 ? 65535 : v);
    return;
  }
  if (d <= 0) return;
  int i = slot <= -2 ? -2 - slot : oldest;    // free slot, else replace the oldest day
  if (slot > -2 && ring[oldest].day > day) return;
  ring[i].day = day;
  ring[i].n = d;
}

static void saveArchive() {
  InternalFS.remove(ARCHIVE_FILE);
  File f(InternalFS);
  if (f.open(ARCHIVE_FILE, FILE_O_WRITE)) {
    f.write((const uint8_t *)ring, sizeof(ring));
    f.close();
  }
}

// rebuild counters: archived days + every record still in the log
static void rebuildCounts() {
  memset(ring, 0, sizeof(ring));
  lifetime = meta.lifetimeArchived;
  lastShotT = 0;
  File f(InternalFS);
  if (f.open(ARCHIVE_FILE, FILE_O_READ)) {
    DayCnt tmp[RING_DAYS];
    if (f.read((uint8_t *)tmp, sizeof(tmp)) == (int)sizeof(tmp)) memcpy(ring, tmp, sizeof(ring));
    f.close();
  }
  if (f.open(LOG_FILE, FILE_O_READ)) {
    ShotRec r;
    for (uint32_t i = 0; i < localCount; i++) {
      if (f.read((uint8_t *)&r, sizeof(r)) != (int)sizeof(r)) break;
      uint32_t idx = meta.baseIndex + i;
      if (r.flags & FLAG_ENDMARK) { if (r.t > meta.lastEndMarkT) meta.lastEndMarkT = r.t; continue; }
      if (r.flags & FLAG_DELETED) continue;
      lifetime++;
      if (r.t > lastShotT) lastShotT = r.t;
      if (idx >= meta.ringFromIndex) ringAdd(r.t, 1);
    }
    f.close();
  }
}

// "reset all stats" on the phone: start the day counters from zero
static void resetDays() {
  memset(ring, 0, sizeof(ring));
  InternalFS.remove(ARCHIVE_FILE);
  meta.ringFromIndex = meta.baseIndex + localCount;
  saveMeta();
}

// mark (or unmark) records as deleted; end marks are left alone
static void setDeleted(uint32_t from, uint32_t count, bool del) {
  File f(InternalFS);
  if (!f.open(LOG_FILE, FILE_O_WRITE)) return;
  for (uint32_t idx = from; idx < from + count; idx++) {
    if (idx < meta.baseIndex || idx >= meta.baseIndex + localCount) continue;   // not on the pod any more
    uint32_t pos = (idx - meta.baseIndex) * sizeof(ShotRec);
    ShotRec r;
    f.seek(pos);
    if (f.read((uint8_t *)&r, sizeof(r)) != (int)sizeof(r)) break;
    if (r.flags & FLAG_ENDMARK) continue;
    bool was = r.flags & FLAG_DELETED;
    if (was == del) continue;
    r.flags = del ? (r.flags | FLAG_DELETED) : (r.flags & ~FLAG_DELETED);
    f.seek(pos);
    f.write((const uint8_t *)&r, sizeof(r));
    if (del) { if (lifetime) lifetime--; } else lifetime++;
    if (idx >= meta.ringFromIndex) ringAdd(r.t, del ? -1 : 1);
  }
  f.close();
}

// "undo" on the watch: delete the newest arrow that is not deleted yet
static void deleteLatest() {
  File f(InternalFS);
  if (!f.open(LOG_FILE, FILE_O_READ)) return;
  int32_t found = -1;
  ShotRec r;
  for (int32_t i = (int32_t)localCount - 1; i >= 0 && i >= (int32_t)localCount - 200; i--) {
    f.seek(i * sizeof(ShotRec));
    if (f.read((uint8_t *)&r, sizeof(r)) != (int)sizeof(r)) break;
    if (isArrow(r)) { found = i; break; }
  }
  f.close();
  if (found >= 0) setDeleted(meta.baseIndex + found, 1, true);
}

static void addEndMark(uint32_t t) {
  ShotRec m;
  m.t = t;
  m.peak10 = 0;
  m.aim100 = 0;
  m.flags = FLAG_ENDMARK | (timeExact ? 1 : 0);
  ShotDetail none;
  memset(&none, 0, sizeof(none));
  appendShot(m, none);
  if (t > meta.lastEndMarkT) meta.lastEndMarkT = t;
}

// days since 1970 -> year, month (proleptic Gregorian, H. Hinnant's algorithm)
static void civilFromDays(int32_t z, int32_t &y, uint32_t &m) {
  z += 719468;
  int32_t era = (z >= 0 ? z : z - 146096) / 146097;
  uint32_t doe = (uint32_t)(z - era * 146097);
  uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  y = (int32_t)yoe + era * 400;
  uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  uint32_t mp = (5 * doy + 2) / 153;
  m = mp < 10 ? mp + 3 : mp - 9;
  if (m <= 2) y++;
}

static void dayStats(uint16_t &today, uint16_t &week, uint16_t &month) {
  today = week = month = 0;
  uint32_t now = nowEpoch();
  if (now < 1600000000UL) return;
  int32_t d = dayOf(now);
  int32_t wd = ((d + 3) % 7 + 7) % 7;          // 1 Jan 1970 was a Thursday; 0 = Monday
  int32_t y, yy;
  uint32_t m, mm;
  civilFromDays(d, y, m);
  uint32_t t = 0, w = 0, mo = 0;
  for (int i = 0; i < RING_DAYS; i++) {
    if (!ring[i].n) continue;
    if (ring[i].day == d) t += ring[i].n;
    if (ring[i].day <= d && ring[i].day >= d - wd) w += ring[i].n;
    civilFromDays(ring[i].day, yy, mm);
    if (yy == y && mm == m) mo += ring[i].n;
  }
  today = t > 65535 ? 65535 : t;
  week = w > 65535 ? 65535 : w;
  month = mo > 65535 ? 65535 : mo;
}

static void clearLog() {
  meta.lifetimeArchived = lifetime;           // arrows so far, the log is about to go
  saveArchive();                              // day counters keep the cleared arrows
  meta.baseIndex += localCount;
  localCount = 0;
  InternalFS.remove(LOG_FILE);
  InternalFS.remove(DETAIL_FILE);
  detailFrom = 0;
  detailCount = 0;
  saveMeta();
}

// =====================================================================
//  Battery + LEDs + motor
// =====================================================================
static uint16_t batteryMv() {
#ifdef PIN_VBAT
  analogReadResolution(12);
  uint32_t raw = 0;
  for (int i = 0; i < 8; i++) raw += analogRead(PIN_VBAT);   // average 8 readings
  return (uint16_t)((raw / 8.0f) * 3600.0f / 4096.0f * (1510.0f / 510.0f));
#else
  return 0;
#endif
}

static void led(uint8_t pin, uint16_t ms) {      // onboard RGB LED is active LOW
  digitalWrite(pin, LOW);
  delay(ms);
  digitalWrite(pin, HIGH);
}

static void shotFeedback(bool stored) {
  led(stored ? LED_GREEN : LED_RED, 40);
#if USE_MOTOR
  digitalWrite(MOTOR_PIN, HIGH);
  delay(80);
  digitalWrite(MOTOR_PIN, LOW);
#endif
}

// =====================================================================
//  Bluetooth LE
// =====================================================================
//  Service  a7c10000-5b1e-4c5a-9d1e-3b7a0c0f0ee0
//   0001 TIME  write/read  u32 unix seconds [+ i32 local time offset in seconds]
//   0002 INFO  read/notify 20 bytes: total u32 (records), baseIndex u32, battery mV u16,
//                          storage % u8, flags u8 (bit0 clock exact, bit1 active), fw u8,
//                          7 bytes reserved (0)
//   0003 CTRL  write       [0x01, u32 fromIndex] = send records from global index
//                          [0x02, u32 expectedTotal] = clear pod log if total matches
//                          [0x04, u32 from, u16 count, u8 del] = mark arrows deleted (1) / restore (0)
//                          [0x05, 0] = delete the newest arrow   [0x06, 0] = day counters to zero
//                          [0x07, u32 time] = new end mark
//                          [0x09, 0] = the bow is level now: zero point for the cant
//   0004 DATA  notify      [u32 firstIndex][ShotRec][ShotRec]   end marker: [0xFFFFFFFF][u32 total]
//                          (records with flag bit6 are end marks, bit7 = deleted)
//   0005 STATS read/notify 20 bytes: arrows u32, today u16, week u16, month u16, battery mV u16,
//                          last end mark u32, last arrow u32   (see updateStats)
//   0006 SETTINGS read/write/notify  8 bytes (struct Settings); write [u8 arrows per end][u16 end pause s]
//   0007 DETAIL notify     [u32 firstIndex][ShotDetail][ShotDetail], sent with every DATA packet
//                          of a dump (to devices that switched on its notifications)
static const uint8_t uuidBytes[16] = {0xE0, 0x0E, 0x0F, 0x0C, 0x7A, 0x3B, 0x1E, 0x9D,
                                      0x5A, 0x4C, 0x1E, 0x5B, 0x00, 0x00, 0xC1, 0xA7};
static uint8_t uuidStore[8][16];

static BLEUuid makeUuid(uint8_t id) {
  memcpy(uuidStore[id], uuidBytes, 16);
  uuidStore[id][12] = id;
  return BLEUuid(uuidStore[id]);
}

BLEService        svc(makeUuid(0));
BLECharacteristic chTime(makeUuid(1));
BLECharacteristic chInfo(makeUuid(2));
BLECharacteristic chCtrl(makeUuid(3));
BLECharacteristic chData(makeUuid(4));
BLECharacteristic chStats(makeUuid(5));
BLECharacteristic chSettings(makeUuid(6));
BLECharacteristic chDetail(makeUuid(7));

volatile CtrlOp   opQueue[8];             // delete / undo / end mark / reset, done in loop()
volatile uint8_t  opHead = 0, opTail = 0;
volatile bool     tzChanged = false;
volatile bool     settingsPending = false;  // new settings from phone / watch, saved in loop()
volatile uint8_t  newArrows = 6;
volatile uint16_t newPause = 100;

volatile bool     dumpPending  = false;
volatile uint32_t dumpFrom     = 0;
volatile uint16_t dumpConn     = BLE_CONN_HANDLE_INVALID;   // the connection that asked for the shots
volatile uint8_t  connCount    = 0;
volatile bool     clearPending = false;
volatile uint32_t clearExpect  = 0;
bool              isActive     = false;

static uint32_t totalShots() { return meta.baseIndex + localCount; }

// STATS (20 bytes, for the watch and the phone): arrows u32 @0 (lifetime, without deleted),
// today u16 @4, week u16 @6, month u16 @8, battery mV u16 @10, last end mark u32 @12, last arrow u32 @16
static void updateStats(bool notify, uint16_t mv) {
  uint8_t b[20] = {0};
  uint16_t td, wk, mo;
  dayStats(td, wk, mo);
  memcpy(b, &lifetime, 4);
  memcpy(b + 4, &td, 2);
  memcpy(b + 6, &wk, 2);
  memcpy(b + 8, &mo, 2);
  memcpy(b + 10, &mv, 2);
  memcpy(b + 12, &meta.lastEndMarkT, 4);
  memcpy(b + 16, &lastShotT, 4);
  chStats.write(b, 20);
  if (!notify) return;
  for (uint16_t c = 0; c < BLE_MAX_CONNECTION; c++)
    if (Bluefruit.connected(c) && chStats.notifyEnabled(c)) chStats.notify(c, b, 20);
}

static void updateInfo(bool notify) {
  uint8_t b[20] = {0};
  uint32_t total = totalShots();
  uint16_t mv = batteryMv();
  updateStats(notify, mv);
  uint32_t pct32 = (storedBytes() * 100UL) / LOG_LIMIT_BYTES;
  uint8_t pct = pct32 > 100 ? 100 : (uint8_t)pct32;
  memcpy(b, &total, 4);
  memcpy(b + 4, &meta.baseIndex, 4);
  memcpy(b + 8, &mv, 2);
  b[10] = pct;
  b[11] = (timeExact ? 1 : 0) | (isActive ? 2 : 0);
  b[12] = FW_VERSION;
  chInfo.write(b, 20);
  if (!notify) return;
  for (uint16_t c = 0; c < BLE_MAX_CONNECTION; c++)          // every connected phone / watch
    if (Bluefruit.connected(c) && chInfo.notifyEnabled(c)) chInfo.notify(c, b, 20);
}

static void publishSettings(bool notify) {
  chSettings.write((const uint8_t *)&settings, sizeof(settings));
  if (!notify) return;
  for (uint16_t c = 0; c < BLE_MAX_CONNECTION; c++)
    if (Bluefruit.connected(c) && chSettings.notifyEnabled(c)) chSettings.notify(c, (const uint8_t *)&settings, sizeof(settings));
}

static void onSettingsWrite(uint16_t, BLECharacteristic *, uint8_t *data, uint16_t len) {
  if (len < 3) return;
  uint16_t p;
  memcpy(&p, data + 1, 2);
  newArrows = data[0] < 1 ? 1 : (data[0] > 24 ? 24 : data[0]);
  newPause = p < 20 ? 20 : (p > 900 ? 900 : p);
  settingsPending = true;                      // flash write in loop()
}

static void onTimeWrite(uint16_t, BLECharacteristic *, uint8_t *data, uint16_t len) {
  if (len < 4) return;
  uint32_t t;
  memcpy(&t, data, 4);
  if (len >= 8) {                              // [u32 unix time][i32 local offset in seconds]
    int32_t tz;
    memcpy(&tz, data + 4, 4);
    if (tz > -14 * 3600 && tz < 15 * 3600 && tz != meta.tzOffset) { meta.tzOffset = tz; tzChanged = true; }
  }
  if (t < 1600000000UL) return;
  epochBase = t - uptimeSec();
  timeExact = true;
  clockRestored = false;
  meta.lastEpoch = t;
  saveMeta();
  updateInfo(false);
}

static void onCtrlWrite(uint16_t conn, BLECharacteristic *, uint8_t *data, uint16_t len) {
  if (len < 5) return;
  uint32_t v;
  memcpy(&v, data + 1, 4);
  if (data[0] == 0x01) { dumpFrom = v; dumpConn = conn; dumpPending = true; }
  if (data[0] == 0x02) { clearExpect = v; clearPending = true; }
  if ((data[0] >= 0x04 && data[0] <= 0x07) || data[0] == 0x09) {   // queue for loop(): no flash work in a callback
    uint8_t next = (opHead + 1) % 8;
    if (next == opTail) return;                // queue full, drop
    for (int i = 0; i < 8; i++) opQueue[opHead].b[i] = i < len ? data[i] : 0;
    opQueue[opHead].len = len;
    opHead = next;
  }
}

static void setLevel();                       // in the shot detection part

static void processOps() {
  bool changed = false;
  while (opTail != opHead) {
    uint8_t b[8];
    for (int i = 0; i < 8; i++) b[i] = opQueue[opTail].b[i];
    uint8_t len = opQueue[opTail].len;
    opTail = (opTail + 1) % 8;
    uint32_t v;
    memcpy(&v, b + 1, 4);
    switch (b[0]) {
      case 0x04: {                             // [0x04, u32 from][u16 count][u8 1=delete 0=restore]
        uint16_t cnt = 1;
        if (len >= 7) memcpy(&cnt, b + 5, 2);
        bool del = len >= 8 ? b[7] != 0 : true;
        setDeleted(v, cnt, del);
        break;
      }
      case 0x05: deleteLatest(); break;        // [0x05, 0,0,0,0] undo the newest arrow (watch)
      case 0x06: resetDays(); break;           // [0x06, 0,0,0,0] day counters back to zero
      case 0x07:                               // [0x07, u32 time] new end from here on
        addEndMark(v > 1600000000UL ? v : nowEpoch());
        saveMeta();
        break;
      case 0x09:                               // [0x09, 0] the bow is level now
        setLevel();
        break;
    }
    changed = true;
  }
  if (changed) updateInfo(true);
}

static void onConnect(uint16_t h) {
  if (connCount < MAX_CONNECTIONS) connCount++;
#if CALIBRATION_LOG
  Serial.printf("BLE connect #%u (%u connected)\n", h, connCount);
#endif
  digitalWrite(LED_BLUE, LOW);
  if (connCount < MAX_CONNECTIONS) Bluefruit.Advertising.start(0);   // stay visible for the 2nd device
}
static void onDisconnect(uint16_t h, uint8_t reason) {
  if (connCount > 0) connCount--;
#if CALIBRATION_LOG
  Serial.printf("BLE disconnect #%u reason 0x%02X (%u connected)\n", h, reason, connCount);
#endif
  if (connCount == 0) digitalWrite(LED_BLUE, HIGH);
}

uint16_t podId = 0;
char     podName[24] = "PfeilZaehler3000";

static void bleBegin() {
  Bluefruit.begin(MAX_CONNECTIONS, 0);
  // Own name per pod from the last 4 hex digits of its Bluetooth address (fixed in the chip),
  // e.g. "PfeilZaehler3000-A3F2", so several pods at one range can be told apart.
  uint8_t mac[6];
  Bluefruit.getAddr(mac);
  podId = ((uint16_t)mac[1] << 8) | mac[0];
  snprintf(podName, sizeof(podName), "PfeilZaehler3000-%04X", podId);
  Bluefruit.setTxPower(0);
  Bluefruit.setName(podName);
  Bluefruit.autoConnLed(false);
  Bluefruit.Periph.setConnectCallback(onConnect);
  Bluefruit.Periph.setDisconnectCallback(onDisconnect);

  svc.begin();

  chTime.setProperties(CHR_PROPS_READ | CHR_PROPS_WRITE);
  chTime.setPermission(SECMODE_OPEN, SECMODE_OPEN);
  chTime.setMaxLen(8);
  chTime.setWriteCallback(onTimeWrite);
  chTime.begin();

  chInfo.setProperties(CHR_PROPS_READ | CHR_PROPS_NOTIFY);
  chInfo.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  chInfo.setFixedLen(20);
  chInfo.begin();

  chCtrl.setProperties(CHR_PROPS_WRITE);
  chCtrl.setPermission(SECMODE_NO_ACCESS, SECMODE_OPEN);
  chCtrl.setMaxLen(8);
  chCtrl.setWriteCallback(onCtrlWrite);
  chCtrl.begin();

  chData.setProperties(CHR_PROPS_NOTIFY);
  chData.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  chData.setMaxLen(20);
  chData.begin();

  chStats.setProperties(CHR_PROPS_READ | CHR_PROPS_NOTIFY);
  chStats.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  chStats.setFixedLen(20);
  chStats.begin();

  chSettings.setProperties(CHR_PROPS_READ | CHR_PROPS_WRITE | CHR_PROPS_NOTIFY);
  chSettings.setPermission(SECMODE_OPEN, SECMODE_OPEN);
  chSettings.setFixedLen(sizeof(Settings));
  chSettings.setWriteCallback(onSettingsWrite);
  chSettings.begin();

  chDetail.setProperties(CHR_PROPS_NOTIFY);
  chDetail.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  chDetail.setMaxLen(20);
  chDetail.begin();

  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(svc);
  // the pod ID also goes into the advertising itself (manufacturer data, company ID 0xFFFF =
  // "none / testing"), so the watch can recognise its own pod without the scan response
  uint8_t md[4] = {0xFF, 0xFF, (uint8_t)(podId >> 8), (uint8_t)(podId & 0xFF)};
  Bluefruit.Advertising.addManufacturerData(md, sizeof(md));
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(160, 1600);   // 100 ms fast, then 1 s slow (0.625 ms units)
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(0);
}

static bool notifyRetry(BLECharacteristic &ch, const uint8_t *b, uint16_t len) {
  for (int i = 0; i < 50; i++) {
    if (!Bluefruit.connected(dumpConn)) return false;
    if (ch.notify(dumpConn, b, len)) return true;
    delay(10);
  }
  return false;
}

static void doDump() {
  dumpPending = false;
  uint32_t base = meta.baseIndex;
  uint32_t req  = dumpFrom;
  uint32_t from = req < base ? base : req;
  uint32_t localStart = from - base;
  bool withDetails = chDetail.notifyEnabled(dumpConn);
  File f(InternalFS);
  File df(InternalFS);
  bool dfOpen = withDetails && detailCount && df.open(DETAIL_FILE, FILE_O_READ);
  if (localStart < localCount && f.open(LOG_FILE, FILE_O_READ)) {
    f.seek(localStart * sizeof(ShotRec));
    uint32_t idx = from;
    while (idx < totalShots()) {
      uint8_t pkt[20];
      memcpy(pkt, &idx, 4);
      int n = f.read(pkt + 4, 2 * sizeof(ShotRec));
      int recs = n / (int)sizeof(ShotRec);
      if (recs < 1) break;
      if (!notifyRetry(chData, pkt, 4 + recs * sizeof(ShotRec))) break;
      if (withDetails) {                         // details of the same records, same packet layout
        uint8_t dp[20] = {0};
        memcpy(dp, &idx, 4);
        for (int k = 0; k < recs; k++) {
          uint32_t local = idx - base + k;
          if (dfOpen && local >= detailFrom && local < detailFrom + detailCount) {
            df.seek(4 + (local - detailFrom) * sizeof(ShotDetail));
            df.read(dp + 4 + k * sizeof(ShotDetail), sizeof(ShotDetail));
          }
        }
        if (!notifyRetry(chDetail, dp, 4 + recs * sizeof(ShotDetail))) break;
      }
      idx += recs;
    }
    f.close();
  }
  if (dfOpen) df.close();
  updateInfo(false);                       // fresh battery reading for the app's "Update now"
  uint8_t end[8];
  uint32_t marker = 0xFFFFFFFF, total = totalShots();
  memcpy(end, &marker, 4);
  memcpy(end + 4, &total, 4);
  notifyRetry(chData, end, 8);
}

// =====================================================================
//  Shot detection
// =====================================================================
float ema[3] = {0, 0, 0};          // resting ADXL reading (gravity + sensor offset)
uint32_t stillStartMs = 0, stillEndMs = 0, lastStillLen = 0;
bool     still = false;
uint32_t lastMotionMs = 0, lastShotMs = 0;
uint32_t sampleNo = 0, lastSampleUs = 0;
float    idleRef[3] = {0, 0, 1};
float    gyroBias[3] = {0, 0, 0};    // gyro zero-rate offset, learned while the pod lies still

enum DetectState { WAITING, CAPTURING };
DetectState detect = WAITING;
uint32_t spikeMs = 0, aimAtSpike = 0;
float    peakG = 0;
uint16_t ringCount = 0;

// ---------------- motion history for the shot details ----------------
// 50 Hz: bow angle and smoothed rotation of the last 30 s (struct HistSample at the top)
#define HIST_N   1500                // 30 s at 50 Hz
#define HIST_MS  20
HistSample hist[HIST_N];
uint16_t histHead = 0, histCount = 0;

// 100 Hz: rotation (offset removed) of the last 1.28 s, for the aim movement
#define GYR_N 128
float    gyrHist[GYR_N][3];
uint16_t gyrHead = 0, gyrCount = 0;

// 100 Hz: low-pass accelerometer (= gravity while the bow is held) of the last 80 ms
#define GRAV_N 8
float    gravHist[GRAV_N][3];
uint8_t  gravHead = 0;
float    gLp[3] = {0, 0, 1};
float    rateSm = 0;                 // smoothed rotation, deg/s
uint32_t lastGyroMs = 0;

// follow-through after a release
bool       followActive = false, shotPending = false;
float      axP[3], axU[3];           // body-frame axes at release: P = horizontal right, U = up
float      followP = 0, followU = 0; // rotation summed since the release, degrees
ShotDetail pendingDetail;
bool       settingsChanged = false;  // cant zero taken from the first shot: save after the shot

static HistSample &histAt(uint16_t k) {        // k = samples back, 0 = newest
  return hist[(histHead + HIST_N - 1 - k) % HIST_N];
}

static float pitchOf(const float g[3]) {       // degrees, + = arrow pointing up
  float n = sqrtf(g[0] * g[0] + g[1] * g[1] + g[2] * g[2]);
  if (n < 0.2f) return 0;
  float s = g[2] / n;
  if (s > 1) s = 1;
  if (s < -1) s = -1;
  return asinf(s) * 57.29578f;
}

static float rollOf(const float g[3]) {        // degrees, roll of the pod around the arrow axis
  return atan2f(g[1], g[0]) * 57.29578f;
}

static uint8_t enc100(uint32_t ms) {           // duration -> units of 100 ms, 1..255 (0 = not found)
  uint32_t v = (ms + 50) / 100;
  return v < 1 ? 1 : (v > 255 ? 255 : (uint8_t)v);
}

static int8_t clamp8(float v) {
  long r = lroundf(v);
  return r > 127 ? 127 : (r < -127 ? -127 : (int8_t)r);
}

static void seedEma() {
  float s[3] = {0, 0, 0}, a[3];
  int n = 0;
  for (int i = 0; i < 20; i++) {
    if (adxlRead(a)) { for (int k = 0; k < 3; k++) s[k] += a[k]; n++; }
    delay(2);
  }
  if (n) for (int k = 0; k < 3; k++) ema[k] = s[k] / n;
}

static void enterActive() {
  lsmActive();
  adxlMeasure();
  delay(15);
  seedEma();
  isActive = true;
#if CALIBRATION_LOG
  Serial.printf("-> active (%s)\n", podName);
#endif
  still = false;
  detect = WAITING;
  followActive = shotPending = false;
  histCount = gyrCount = 0;                    // history starts again after a sleep
  if (lsmReadAccel(gLp)) for (int i = 0; i < GRAV_N; i++) memcpy(gravHist[i], gLp, sizeof(gLp));
  rateSm = 0;
  lastGyroMs = 0;
  lastMotionMs = millis();
  updateInfo(true);
}

static void enterIdle() {
  lsmIdle();
  adxlStandby();
  isActive = false;
#if CALIBRATION_LOG
  Serial.printf("-> idle (%s)\n", podName);
#endif
  delay(100);
  lsmReadAccel(idleRef);
  meta.lastEpoch = nowEpoch();
  saveMeta();
  updateInfo(true);
}

// "set level" from the phone: the pod's roll right now becomes cant 0
static void setLevel() {
  float s[3] = {0, 0, 0}, a[3];
  int n = 0;
  for (int i = 0; i < 8; i++) {
    if (lsmReadAccel(a)) { for (int k = 0; k < 3; k++) s[k] += a[k]; n++; }
    delay(20);
  }
  if (!n) return;
  settings.cantRef10 = (int16_t)lroundf(rollOf(s) * 10.0f);
  settings.flags = (settings.flags | 1) & ~2;  // bit0 set by hand, bit1 (from the first shot) off
  saveSettings();
  publishSettings(true);
#if CALIBRATION_LOG
  Serial.printf("cant zero set (roll %.1f deg)\n", settings.cantRef10 / 10.0f);
#endif
}

// ~100 Hz: gyro (stillness, hold, aim movement, follow-through) and accelerometer (bow angle)
static void updateStillness(uint32_t ms) {
  float g[3];
  if (!lsmReadGyro(g)) return;
  float rate = sqrtf(g[0] * g[0] + g[1] * g[1] + g[2] * g[2]);
  // Motion for the idle timer: rotation minus the gyro's own offset (up to ~10 dps on this chip),
  // so a pod lying on a table no longer counts as "moving".
  float b0 = g[0] - gyroBias[0], b1 = g[1] - gyroBias[1], b2 = g[2] - gyroBias[2];
  float turn = sqrtf(b0 * b0 + b1 * b1 + b2 * b2);
  if (turn < 12.0f) for (int k = 0; k < 3; k++) gyroBias[k] += 0.01f * (g[k] - gyroBias[k]);
  if (turn > MOVE_DPS) lastMotionMs = ms;
  bool nowStill = rate < STILL_DPS;
  if (nowStill && !still) stillStartMs = ms;
  if (!nowStill && still) { stillEndMs = ms; lastStillLen = ms - stillStartMs; }
  still = nowStill;

  // ---- shot details ----
  float dt = lastGyroMs ? (ms - lastGyroMs) / 1000.0f : 0.01f;
  if (dt > 0.05f) dt = 0.05f;
  lastGyroMs = ms;
  rateSm += 0.2f * (turn - rateSm);            // ~50 ms smoothing
  gyrHist[gyrHead][0] = b0; gyrHist[gyrHead][1] = b1; gyrHist[gyrHead][2] = b2;
  gyrHead = (gyrHead + 1) % GYR_N;
  if (gyrCount < GYR_N) gyrCount++;

  float a[3];
  if (!followActive && lsmReadAccel(a)) {      // the release shock would spoil the angle: skip it
    for (int k = 0; k < 3; k++) gLp[k] += 0.15f * (a[k] - gLp[k]);
    memcpy(gravHist[gravHead], gLp, sizeof(gLp));
    gravHead = (gravHead + 1) % GRAV_N;
  }
  if (followActive) {                          // rotation after the release
    if (ms - spikeMs <= FOLLOW_MS) {
      followP += (b0 * axP[0] + b1 * axP[1] + b2 * axP[2]) * dt;
      followU += (b0 * axU[0] + b1 * axU[1] + b2 * axU[2]) * dt;
    }
  }
  static uint8_t half = 0;
  if (!followActive && (++half & 1)) {         // 50 Hz history
    HistSample &h = hist[histHead];
    h.pitch10 = (int16_t)lroundf(pitchOf(gLp) * 10.0f);
    float r2 = rateSm * 2.0f;
    h.rate2 = r2 > 255 ? 255 : (uint8_t)r2;
    histHead = (histHead + 1) % HIST_N;
    if (histCount < HIST_N) histCount++;
  }
}

static uint32_t aimTimeAt(uint32_t ms) {
  if (still) return ms - stillStartMs;
  if (ms - stillEndMs < 120) return lastStillLen;   // release motion may break stillness a few ms early
  return 0;
}

// At the release jolt: look back over the history and work out the phases of the shot.
static void analyseShot() {
  ShotDetail &d = pendingDetail;
  memset(&d, 0, sizeof(d));

  // gravity 30 ms before the jolt -> axes, bow angle and cant at release
  float *g = gravHist[(gravHead + GRAV_N - 3) % GRAV_N];
  float n = sqrtf(g[0] * g[0] + g[1] * g[1] + g[2] * g[2]);
  if (n < 0.2f) n = 1;
  for (int k = 0; k < 3; k++) axU[k] = g[k] / n;          // up
  float px = -axU[1], py = axU[0];                          // Z x U = horizontal, to the right
  float pn = sqrtf(px * px + py * py);
  if (pn < 0.05f) { px = 1; py = 0; pn = 1; }               // bow pointing straight up or down
  axP[0] = px / pn; axP[1] = py / pn; axP[2] = 0;
  float pitch = pitchOf(g);
  d.pitch2 = clamp8(pitch * 2.0f);
  float roll = rollOf(g);
  if (!(settings.flags & 3)) {                  // no zero point yet: the first shot counts as level
    settings.cantRef10 = (int16_t)lroundf(roll * 10.0f);
    settings.flags |= 2;
    settingsChanged = true;                     // flash write later: not during the jolt
  }
  float cant = settings.cantRef10 / 10.0f - roll;
  while (cant >= 180) cant -= 360;
  while (cant < -180) cant += 360;
  d.cant2 = clamp8(cant * 2.0f);

  // aim movement in the last second (from 30 ms before the jolt): rotation summed to an angle
  // around the two aiming axes, slow drift (and gyro offset) removed by a straight-line fit
  const int W = 100;
  if (gyrCount >= W + 3) {
    float sv = 0, sh = 0, sx = 0, sxx = 0, sxv = 0, sxh = 0, svv = 0, shh = 0;
    float av = 0, ah = 0;
    for (int i = 0; i < W; i++) {
      int k = W + 2 - i;                        // oldest first
      float *w = gyrHist[(gyrHead + GYR_N - 1 - k) % GYR_N];
      av += (w[0] * axP[0] + w[1] * axP[1] + w[2] * axP[2]) * 0.01f;
      ah += (w[0] * axU[0] + w[1] * axU[1] + w[2] * axU[2]) * 0.01f;
      float x = i;
      sx += x; sxx += x * x; sv += av; sh += ah; sxv += x * av; sxh += x * ah; svv += av * av; shh += ah * ah;
    }
    float den = W * sxx - sx * sx;
    float bv = (W * sxv - sx * sv) / den, bh = (W * sxh - sx * sh) / den;
    float cv = (sv - bv * sx) / W, ch = (sh - bh * sx) / W;
    // residual sum of squares of a straight-line fit
    float rv = svv - 2 * cv * sv - 2 * bv * sxv + W * cv * cv + 2 * cv * bv * sx + bv * bv * sxx;
    float rh = shh - 2 * ch * sh - 2 * bh * sxh + W * ch * ch + 2 * ch * bh * sx + bh * bh * sxx;
    float rms = sqrtf((rv > 0 ? rv : 0) / W + (rh > 0 ? rh : 0) / W);
    float u = rms * 100.0f;
    d.wander = u < 1 ? 1 : (u > 255 ? 255 : (uint8_t)lroundf(u));
  }

  // phases, from the newest history sample backwards (k = samples of 20 ms back)
  const uint16_t K0 = 2;                        // ~40 ms before the jolt = release position
  if (histCount <= K0 + 5) return;
  float pr = histAt(K0).pitch10 / 10.0f;

  // hold: the bow almost still until the release
  uint16_t kHold = K0;
  while (kHold + 1 < histCount && histAt(kHold + 1).rate2 < HOLD_DPS * 2) kHold++;
  bool holding = histAt(K0).rate2 < HOLD_DPS * 2;
  if (holding) d.hold100 = enc100((uint32_t)(kHold + 1) * HIST_MS);

  // lowered: the last time the bow angle was far from the release angle
  uint16_t kAway = K0;
  while (kAway < histCount && fabsf(histAt(kAway).pitch10 / 10.0f - pr) <= AWAY_DEG) kAway++;
  if (kAway >= histCount) return;               // the bow was not lowered (or too long ago)

  // raised: the first time after that the angle came within LEVEL_DEG of the release angle
  uint16_t kLevel = kAway - 1;
  while (kLevel > K0 && fabsf(histAt(kLevel).pitch10 / 10.0f - pr) >= LEVEL_DEG) kLevel--;
  if (holding && kHold > kLevel) {              // still from the moment it was raised: no separate draw
    kHold = kLevel;
    d.hold100 = enc100((uint32_t)(kHold + 1) * HIST_MS);
  }

  // start of the raise: going further back, the lowest point, or where the bow was resting
  uint16_t kStart = kAway, k = kAway, rest = 0;
  float dmax = fabsf(histAt(kAway).pitch10 / 10.0f - pr);
  while (k + 1 < histCount && k < kAway + 750) {          // up to 15 s further back
    k++;
    float dk = fabsf(histAt(k).pitch10 / 10.0f - pr);
    if (dk < dmax - 1.0f) break;                // the bow was higher before: lowest point passed
    if (dk > dmax) { dmax = dk; kStart = k; }
    if (histAt(k).rate2 < REST_DPS * 2) {
      if (++rest >= 15) { kStart = k - 14; break; }        // resting in the pre-draw position
    } else {
      rest = 0;
    }
  }
  d.raise100 = enc100((uint32_t)(kStart - kLevel) * HIST_MS);
  d.draw100 = enc100(kLevel > kHold ? (uint32_t)(kLevel - kHold) * HIST_MS : 0);
}

static void recordShot() {
  ShotRec r;
  r.t = nowEpoch();
  float p = peakG * 10.0f;
  r.peak10 = p > 65535.0f ? 65535 : (uint16_t)p;
  uint32_t aim = aimAtSpike / 100;
  r.aim100 = aim > 255 ? 255 : (uint8_t)aim;
  r.flags = (timeExact ? 1 : 0) | (clockRestored ? 2 : 0);
  pendingDetail.dropDeg = clamp8(-followP);     // + = bow tips forward / down
  pendingDetail.sideDeg = clamp8(-followU);     // + = to the right
  bool ok = appendShot(r, pendingDetail);
  if (settingsChanged) {
    settingsChanged = false;
    saveSettings();
    publishSettings(true);
  }
  if (ok) {
    lifetime++;
    ringAdd(r.t, 1);
    lastShotT = r.t;
  }
#if CALIBRATION_LOG
  const ShotDetail &d = pendingDetail;
  Serial.printf("  details: raise %.1fs draw %.1fs hold %.1fs | aim movement %.2f deg | angle %+.1f cant %+.1f | "
                "follow-through down %+d right %+d deg\n",
                d.raise100 / 10.0f, d.draw100 / 10.0f, d.hold100 / 10.0f, d.wander / 100.0f,
                d.pitch2 / 2.0f, d.cant2 / 2.0f, d.dropDeg, d.sideDeg);
#endif
  shotFeedback(ok);
  updateInfo(true);
}

static void activeStep() {
  uint32_t us = micros();
  if ((uint32_t)(us - lastSampleUs) < 1250) return;   // ~800 Hz
  lastSampleUs = us;

  float a[3];
  if (!adxlRead(a)) return;
  float d0 = a[0] - ema[0], d1 = a[1] - ema[1], d2 = a[2] - ema[2];
  float dev = sqrtf(d0 * d0 + d1 * d1 + d2 * d2);
  if (dev < 1.0f) for (int k = 0; k < 3; k++) ema[k] += 0.002f * (a[k] - ema[k]);

  uint32_t ms = millis();
  if (++sampleNo % 8 == 0) updateStillness(ms);   // ~100 Hz gyro
  if (dev > MOVE_G) lastMotionMs = ms;   // was 0.3 g: ADXL375 noise alone crossed that, so it never idled

  if (detect == WAITING) {
    if (shotPending && ms - spikeMs >= FOLLOW_MS) {   // follow-through measured: store the shot
      shotPending = followActive = false;
      recordShot();
    }
    if (!shotPending && dev > SPIKE_G && (ms - lastShotMs) > REFRACTORY_MS) {
      aimAtSpike = aimTimeAt(ms);
      spikeMs = ms;
      peakG = dev;
      ringCount = 0;
      detect = CAPTURING;
      analyseShot();                           // history up to just before the jolt
      followP = followU = 0;
      followActive = true;
    }
  } else {
    uint32_t dt = ms - spikeMs;
    if (dt < 10 && dev > peakG) peakG = dev;
    if (dt >= 10 && dev > RING_G) ringCount++;
    if (dt >= 150) {
      detect = WAITING;
      bool isShot = aimAtSpike >= MIN_AIM_MS && ringCount >= RING_MIN_SAMPLES;
#if CALIBRATION_LOG
      Serial.printf("jolt peak=%.1fg aim=%lums ring=%u -> %s\n", peakG,
                    (unsigned long)aimAtSpike, (unsigned)ringCount, isShot ? "SHOT" : "ignored");
#endif
      if (isShot) {
        lastShotMs = ms;
        shotPending = true;                    // stored when the follow-through is measured
      } else {
        followActive = false;
      }
    }
  }

  if (ms - lastMotionMs > IDLE_AFTER_MS && detect == WAITING && !shotPending) enterIdle();
}

static void idleStep() {
  delay(250);                                  // CPU sleeps here
  float a[3];
  if (!lsmReadAccel(a)) return;
  float d0 = a[0] - idleRef[0], d1 = a[1] - idleRef[1], d2 = a[2] - idleRef[2];
  if (sqrtf(d0 * d0 + d1 * d1 + d2 * d2) > WAKE_DELTA_G) enterActive();
}

// =====================================================================
//  Arduino entry points
// =====================================================================
void setup() {
  pinMode(LED_RED, OUTPUT);   digitalWrite(LED_RED, HIGH);
  pinMode(LED_GREEN, OUTPUT); digitalWrite(LED_GREEN, HIGH);
  pinMode(LED_BLUE, OUTPUT);  digitalWrite(LED_BLUE, HIGH);
#ifdef VBAT_ENABLE
  pinMode(VBAT_ENABLE, OUTPUT);
  digitalWrite(VBAT_ENABLE, LOW);             // keep LOW – protects the battery-sense pin while charging
#endif
#ifdef PIN_CHARGING_CURRENT
  pinMode(PIN_CHARGING_CURRENT, OUTPUT);
  digitalWrite(PIN_CHARGING_CURRENT, LOW);    // 100 mA charging
#endif
#if USE_MOTOR
  pinMode(MOTOR_PIN, OUTPUT);
  digitalWrite(MOTOR_PIN, LOW);
#endif
  Serial.begin(115200);

  bleBegin();                                  // Bluetooth first: the pod stays reachable whatever happens
  loadStorage();
  rebuildCounts();
  updateInfo(false);
  publishSettings(false);
  lsmOk = lsmBegin();
  adxlOk = adxlBegin();
  if (lsmOk && adxlOk) {
    led(LED_GREEN, 300);                       // hello: both sensors found
    Serial.printf("%s ready\n", podName);
    enterActive();
  } else {
    Serial.printf("%s: sensor missing: LSM6DS3=%d ADXL375=%d\n", podName, lsmOk, adxlOk);
  }
}

void loop() {
  if (opTail != opHead) processOps();
  if (tzChanged) {                             // local day boundaries moved: count again
    tzChanged = false;
    saveMeta();
    rebuildCounts();
    updateInfo(true);
  }
  if (settingsPending) {                       // arrows per end / end pause from phone or watch
    settingsPending = false;
    settings.arrowsPerEnd = newArrows;
    settings.endPauseS = newPause;
    saveSettings();
    publishSettings(true);                     // the other devices follow
  }
  if (dumpPending) doDump();
  if (clearPending) {
    clearPending = false;
    if (clearExpect == totalShots()) clearLog();
    updateInfo(true);
  }

  static uint32_t lastSave = 0;
  if (millis() - lastSave > 1800000UL) {      // every 30 min remember the time
    lastSave = millis();
    meta.lastEpoch = nowEpoch();
    saveMeta();
  }

  if (!lsmOk || !adxlOk) {                     // sensor missing: blink red (1x = IMU, 2x = ADXL375), sync still works
    static uint32_t lastBlink = 0;
    if (millis() - lastBlink > 2500) {
      lastBlink = millis();
      for (int i = 0; i < (!lsmOk ? 1 : 2); i++) { led(LED_RED, 150); delay(150); }
    }
    delay(10);
    return;
  }
  if (isActive) activeStep();
  else idleStep();
}
