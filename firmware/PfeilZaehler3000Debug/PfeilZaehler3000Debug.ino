/*
  PfeilZaehler3000 – DEBUG BUILD (firmware v2.3.2 + diagnostics)
  (c) 2026 ckpt-gthb – free for personal, non-commercial use; PolyForm Strict 1.0.0 with permissions, see LICENSE
  - same functions and Bluetooth interface as the normal firmware v2.3.2 (phone page and watch work)
  - prints every startup step, I2C scans and a status line every 2 s to the Serial Monitor (115200)
  - never goes to sleep (drains the battery in a few days): for diagnosis only, then flash the normal one
  - blue LED blinks briefly every 2 s while nothing is connected

  PfeilZaehler3000 – bow-mounted automatic shot logger
  ------------------------------------------------
  Board   : Seeed Studio XIAO nRF52840 **Sense**
            Arduino core: "Seeed nRF52 Boards" (the NON-mbed one – needed for Bluefruit + LittleFS)
  Sensors : onboard LSM6DS3TR-C (gyro + accel, internal I2C bus Wire1)
            ADXL375 high-g accelerometer breakout on Wire (D4 = SDA, D5 = SCL)
  Storage : internal flash (LittleFS), ~1100 arrows with details between syncs
  Sync    : Bluetooth LE, custom service, read by the PfeilZaehler3000 web app

  A shot is counted when ALL of these happen:
    1. the bow was held still (aiming) for at least MIN_AIM_MS
    2. a sharp jolt above SPIKE_G hits the riser (string stops -> release shock)
    3. the riser keeps ringing (vibration above RING_G) for the next ~150 ms
  Other archers' shots never reach this sensor, so a shared target is not a problem.

  Shot details: the last 30 s of bow angle and rotation are kept in RAM. At every shot the
  pod looks back and finds the raise (bow lowered -> aiming angle), draw and set, and the final
  hold; it also measures the aim movement in the last second, the bow angle and cant at release
  and the follow-through (how the bow moves in the first 0.5 s after release).
  Mounting point (a setting, flags bits 2-3):
    back  – bushing on the back of the riser (archer's side), lid towards the archer
    front – bushing on the front of the riser (target side), lid towards the target
    side  – a bushing on the side of the riser (e.g. V-bar): arrows and phase times only
  On the back and the front the bolt (sensor Z axis) points roughly along the arrow (a bushing
  tilted by a few degrees is fine: the calibration measures the tilt), so bow angle, cant
  and follow-through can be measured. The calibration on the phone page (bow upright with the
  arrow level, then pointed at the ground) finds the mounting point and sets the zero of the cant
  and of the bow angle.

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
                     // bit5: end score, not an arrow (t = start of the end, peak10 + aim100 = 6 values
                     //       of 4 bits, bits 0-1 = part 0-3 for ends of more than 6 arrows)
                     // bit4: hit positions of the end score just before it (7 bytes = 3.5 positions)
                     // bits 4-6 all set (2.3.2): settings change, t = when, peak10 = end pause in s,
                     //       aim100 = arrows per end (so ends are judged with the setting valid then)
};
#define FLAG_ENDMARK 0x40
#define FLAG_DELETED 0x80
#define FLAG_SCORE   0x20
#define FLAG_POS     0x10
#define FLAG_NOARROW (FLAG_ENDMARK | FLAG_SCORE | FLAG_POS)
#define FLAG_SETTING (FLAG_ENDMARK | FLAG_SCORE | FLAG_POS)   // all three: a settings change (2.3.2)

// Shot details, one per record in a second file (all zero for end marks, scores and old records).
// Times: units of 100 ms, 0 = not found, 255 = 25.5 s or more.
struct __attribute__((packed)) ShotDetail {
  uint8_t raise100;  // raise: from the lowered bow (pre-draw) until the aiming angle is reached
  uint8_t draw100;   // draw and set: from reaching the aiming angle until the final hold starts
  uint8_t hold100;   // hold: the bow almost still (below HOLD_DPS) until the release
  uint8_t wander;    // aim movement in the last second before release, RMS in 0.01 degrees
  int16_t pitch10;   // bow angle at release, 0.1 degrees, + = aiming up
  int16_t cant10;    // cant at release, 0.1 degrees, + = top limb to the right
  int8_t  dropDeg;   // follow-through 0.5 s after release: + = bow tips forward / down, degrees
  int8_t  sideDeg;   // follow-through sideways: + = to the right, degrees
  int8_t  pushDown;  // bow push in the first 0.1 s after release: + = down, mm
  int8_t  pushRight; // bow push sideways: + = to the right, mm
};
struct __attribute__((packed)) ShotDetailV1 {  // detail record of firmware 2.0 / 2.1 (8 bytes)
  uint8_t raise100, draw100, hold100, wander;
  int8_t  pitch2, cant2, dropDeg, sideDeg;     // angles in 0.5 degrees
};

struct __attribute__((packed)) Settings {    // shared by phone page and watch, last change wins
  uint8_t  magic;            // SETTINGS_MAGIC
  uint8_t  arrowsPerEnd;     // 1–24, default 6
  uint16_t endPauseS;        // pause that starts a new end, 20–900 s, default 100
  uint8_t  flags;            // bit0: cant zero set by the calibration, bit1: cant zero from the first shot,
                             // bits 2-3: mounting point, MOUNT_BACK / MOUNT_FRONT / MOUNT_SIDE
  uint8_t  calStatus;        // calibration: 0 not done, 1 step 1 done (bow upright), 2 done, 3 failed (tilt too small),
                             // 4 step 3 done, 5 step 3 failed; 6 = the bow was not still (only notified, never stored)
  int16_t  cantRef10;        // roll angle of the pod when the bow is level, 0.1 degrees
  int16_t  pitchRef10;       // Z angle of the pod with the arrow level (tilt of the bushing), 0.1 degrees
  uint8_t  round;            // target face for scoring (shared): 0 full face, 1 3-spot (0-5 accepted)
};
#define SETTINGS_MAGIC 0x5E
#define MOUNT_BACK  0                        // back of the riser, archer's side (default)
#define MOUNT_FRONT 1                        // front of the riser, target side
#define MOUNT_SIDE  2                        // side bushing: no bow angle, cant, follow-through
#define DETAIL_NA   (-128)                   // detail not measured (side mount, no alignment)
#define DETAIL_NA16 (-32768)                 // same for the 16-bit angles
#define ROUND_MAX   5

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
  int8_t  gx, gy, gz;  // direction of gravity in the pod (unit vector x 127): orientation of the bow
  uint8_t rate2;       // smoothed rotation, 0.5 deg/s (max 127 deg/s)
};

struct CtrlOp {                              // control command waiting for the main loop
  uint8_t b[20];
  uint8_t len;
};

// =====================================================================
//  TUNING – adjust after the calibration session (see build guide)
// =====================================================================
#define SPIKE_G           12.0f    // release jolt threshold, g above resting value
#define RING_G             2.0f    // vibration level that counts as "ringing"
#define RING_MIN_SAMPLES   6       // ringing samples needed 10–150 ms after the jolt
#define STILL_DPS         25.0f    // rotation below this (deg/s) = holding still
#define MIN_AIM_MS        3000     // minimum still time right before the release (a real shot: 4 s or more)
#define MAX_AIM_MS        20000    // longer = the pod was lying still and got knocked, not a shot
#define MIN_PEAK_G        25.0f    // weaker jolts are bumps (real releases: 29 g or more)
#define REFRACTORY_MS     1500     // dead time after a counted shot
#define IDLE_AFTER_MS     0xFFFFFFFFUL // DEBUG: never go to sleep
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
#define PUSH_MS            100     // bow push measured over this time after the release
#define REST_DPS_STAND     1.5f    // smoothed rotation below this for REST_MS = bow on the stand
#define REST_MS            4000

#define MAX_CONNECTIONS   3        // phone page + Garmin watch, plus one spare for a stale link

#define FW_VERSION        123  // debug build of v2.3.2
#define FW_PATCH          2    // third digit, sent in INFO byte 13 (2.3.2)

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
// ---------------- debug helpers ----------------
#define DBG(...) do { Serial.printf(__VA_ARGS__); Serial.flush(); } while (0)

static void i2cScan(TwoWire &w, const char *name) {
  DBG("  I2C scan %s:", name);
  int found = 0;
  for (uint8_t a = 0x08; a < 0x78; a++) {
    w.beginTransmission(a);
    if (w.endTransmission() == 0) { DBG(" 0x%02X", a); found++; }
  }
  DBG(found ? "\n" : " nothing found\n");
}

static bool i2cLinesFree(uint8_t sda, uint8_t scl, const char *name) {
  pinMode(sda, INPUT_PULLUP);
  pinMode(scl, INPUT_PULLUP);
  delayMicroseconds(20);
  bool ok = digitalRead(sda) == HIGH && digitalRead(scl) == HIGH;
  DBG("  %s lines: SDA=%d SCL=%d -> %s\n", name, digitalRead(sda), digitalRead(scl), ok ? "free" : "BLOCKED");
  return ok;
}

static bool lsmBegin() {
#ifdef PIN_LSM6DS3TR_C_POWER
  // The onboard IMU is powered from this pin: high drive (as in Seeed's LSM6DS3 library),
  // and a full power cycle, because with a battery attached the IMU never loses power and
  // can stay stuck after a reset. Its I2C lines are pulled low while it is off.
  nrf_gpio_cfg(g_ADigitalPinMap[PIN_LSM6DS3TR_C_POWER], NRF_GPIO_PIN_DIR_OUTPUT, NRF_GPIO_PIN_INPUT_DISCONNECT,
               NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_H0H1, NRF_GPIO_PIN_NOSENSE);
  digitalWrite(PIN_LSM6DS3TR_C_POWER, LOW);
  DBG("  IMU power off\n");
  pinMode(PIN_WIRE1_SDA, OUTPUT); digitalWrite(PIN_WIRE1_SDA, LOW);
  pinMode(PIN_WIRE1_SCL, OUTPUT); digitalWrite(PIN_WIRE1_SCL, LOW);
  delay(100);
  pinMode(PIN_WIRE1_SDA, INPUT);
  pinMode(PIN_WIRE1_SCL, INPUT);
  digitalWrite(PIN_LSM6DS3TR_C_POWER, HIGH);
  DBG("  IMU power on\n");
  delay(50);
  i2cRecover(PIN_WIRE1_SDA, PIN_WIRE1_SCL);
  if (!i2cLinesFree(PIN_WIRE1_SDA, PIN_WIRE1_SCL, "internal bus")) return false;
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
  if (!i2cLinesFree(PIN_WIRE_SDA, PIN_WIRE_SCL, "external bus")) return false;
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
#define DETAIL_FILE "/detail2.bin"    // [u32 first local index with details][ShotDetail ...]
#define DETAIL_V1   "/detail.bin"     // 8-byte details of firmware 2.0 / 2.1, converted once
#define ADXL_FILE   "/adxl.bin"       // [u32 magic][float 3x3 ADXL375 -> main sensor][float 3 bow right][float 3 bow up]
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
Settings settings = {SETTINGS_MAGIC, 6, 100, 0, 0, 0, 0, 0};
#define ADXL_MAGIC 0xAD175A12
float adxlRot[9];             // ADXL375 reading -> LSM6DS3 axes, from calibration step 3
float bowRight[3], bowUp[3];  // the bow's right and up axes in the main sensor's axes (steps 1 and 2)
bool  adxlAligned = false;

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
  if (!InternalFS.exists(DETAIL_FILE) && f.open(DETAIL_V1, FILE_O_READ)) {   // update from 2.0 / 2.1
    uint32_t from = 0;
    File g(InternalFS);
    if (f.read((uint8_t *)&from, 4) == 4 && g.open(DETAIL_FILE, FILE_O_WRITE)) {
      g.write((const uint8_t *)&from, 4);
      ShotDetailV1 o;
      while (f.read((uint8_t *)&o, sizeof(o)) == (int)sizeof(o)) {
        ShotDetail d;
        d.raise100 = o.raise100; d.draw100 = o.draw100; d.hold100 = o.hold100; d.wander = o.wander;
        d.pitch10 = o.pitch2 == DETAIL_NA ? DETAIL_NA16 : o.pitch2 * 5;
        d.cant10 = o.cant2 == DETAIL_NA ? DETAIL_NA16 : o.cant2 * 5;
        d.dropDeg = o.dropDeg; d.sideDeg = o.sideDeg;
        d.pushDown = d.pushRight = DETAIL_NA;
        g.write((const uint8_t *)&d, sizeof(d));
      }
      g.close();
    }
    f.close();
    InternalFS.remove(DETAIL_V1);
  }
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
  memset(&s, 0, sizeof(s));
  if (f.open(SET_FILE, FILE_O_READ)) {           // 10 bytes up to 2.1, 11 since 2.2 (target face)
    if (f.read((uint8_t *)&s, sizeof(s)) >= 10 && s.magic == SETTINGS_MAGIC) {
      if (s.round > ROUND_MAX) s.round = 0;
      settings = s;
    }
    f.close();
  }
  uint32_t magic = 0;
  if (f.open(ADXL_FILE, FILE_O_READ)) {
    if (f.read((uint8_t *)&magic, 4) == 4 && magic == ADXL_MAGIC && f.read((uint8_t *)adxlRot, sizeof(adxlRot)) == (int)sizeof(adxlRot) &&
        f.read((uint8_t *)bowRight, 12) == 12 && f.read((uint8_t *)bowUp, 12) == 12)
      adxlAligned = true;
    f.close();
  }
}

static void saveAdxlRot() {
  InternalFS.remove(ADXL_FILE);
  File f(InternalFS);
  uint32_t magic = ADXL_MAGIC;
  if (f.open(ADXL_FILE, FILE_O_WRITE)) {
    f.write((const uint8_t *)&magic, 4);
    f.write((const uint8_t *)adxlRot, sizeof(adxlRot));
    f.write((const uint8_t *)bowRight, 12);
    f.write((const uint8_t *)bowUp, 12);
    f.close();
  }
}

static bool trimOldest(uint32_t k);           // memory full: the oldest records go (below)

static bool appendShot(const ShotRec &r, const ShotDetail &d) {
  if (storedBytes() + sizeof(ShotRec) + sizeof(ShotDetail) + 4 > LOG_LIMIT_BYTES) {
    // nobody synced for a long time (e.g. only the watch is used): a new arrow always counts,
    // the oldest half of the records makes room
    if (!trimOldest(localCount / 2)) return false;
    if (storedBytes() + sizeof(ShotRec) + sizeof(ShotDetail) + 4 > LOG_LIMIT_BYTES) return false;
  }
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
static bool isArrow(const ShotRec &r) { return !(r.flags & (FLAG_DELETED | FLAG_NOARROW)); }

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
      if ((r.flags & FLAG_SETTING) == FLAG_ENDMARK) { if (r.t > meta.lastEndMarkT) meta.lastEndMarkT = r.t; continue; }
      if (r.flags & (FLAG_DELETED | FLAG_SCORE | FLAG_POS)) continue;
      lifetime++;
      if (r.t > lastShotT) lastShotT = r.t;
      if (idx >= meta.ringFromIndex) ringAdd(r.t, 1);
    }
    f.close();
  }
}

// Memory full: the k oldest records leave the log, as if the phone had synced and cleared them.
// Their arrows stay in the counters (days go into the archive, lifetime into lifetimeArchived);
// a phone that syncs later sees that the pod's oldest index moved past what it has.
// Keep a file from byte skip on (after an optional new header). The kept part goes through RAM:
// the internal flash has no room for a second copy of a full log. false = file unchanged.
static bool keepFileFrom(const char *path, uint32_t skip, const uint8_t *head, uint32_t headLen) {
  File f(InternalFS);
  if (!f.open(path, FILE_O_READ)) return false;
  uint32_t size = f.size();
  uint32_t n = skip < size ? size - skip : 0;
  uint8_t *buf = (uint8_t *)malloc(n ? n : 1);
  if (!buf) { f.close(); return false; }
  f.seek(skip);
  bool ok = n == 0 || f.read(buf, n) == (int)n;
  f.close();
  if (!ok) { free(buf); return false; }
  InternalFS.remove(path);
  for (int attempt = 0; attempt < 2; attempt++) {   // write again once if the flash refused it
    File g(InternalFS);
    if (!g.open(path, FILE_O_WRITE)) continue;
    ok = (!headLen || g.write(head, headLen) == headLen) && (!n || g.write(buf, n) == n);
    g.close();
    if (ok) break;
    InternalFS.remove(path);
  }
  free(buf);
  return true;                                 // changed (if writing failed, the file is gone: see callers)
}

static bool trimOldest(uint32_t k) {
  if (k == 0 || k >= localCount) return false;
  File f(InternalFS);
  if (!f.open(LOG_FILE, FILE_O_READ)) return false;
  DayCnt live[RING_DAYS];                      // the counters as they are now
  memcpy(live, ring, sizeof(ring));
  memset(ring, 0, sizeof(ring));               // archive = archived days + the records that leave
  File a(InternalFS);
  if (a.open(ARCHIVE_FILE, FILE_O_READ)) {
    DayCnt tmp[RING_DAYS];
    if (a.read((uint8_t *)tmp, sizeof(tmp)) == (int)sizeof(tmp)) memcpy(ring, tmp, sizeof(ring));
    a.close();
  }
  uint32_t arrows = 0;
  ShotRec r;
  bool ok = true;
  for (uint32_t i = 0; i < k; i++) {
    if (f.read((uint8_t *)&r, sizeof(r)) != (int)sizeof(r)) { ok = false; break; }
    if (!isArrow(r)) continue;
    arrows++;
    if (meta.baseIndex + i >= meta.ringFromIndex) ringAdd(r.t, 1);
  }
  f.close();
  if (!ok || !keepFileFrom(LOG_FILE, k * sizeof(ShotRec), nullptr, 0)) { memcpy(ring, live, sizeof(ring)); return false; }
  saveArchive();
  memcpy(ring, live, sizeof(ring));
  // the log as it is now (normally localCount - k records)
  uint32_t left = 0;
  if (f.open(LOG_FILE, FILE_O_READ)) { left = f.size() / sizeof(ShotRec); f.close(); }
  // details: drop the ones of the removed records, keep the file in step with the log
  if (detailCount) {
    uint32_t skip = k > detailFrom ? k - detailFrom : 0;
    uint32_t from = detailFrom > k ? detailFrom - k : 0;
    if (skip < detailCount && keepFileFrom(DETAIL_FILE, 4 + skip * sizeof(ShotDetail), (const uint8_t *)&from, 4) &&
        InternalFS.exists(DETAIL_FILE)) {
      detailFrom = from;
      detailCount -= skip;
    } else {
      InternalFS.remove(DETAIL_FILE);
      detailCount = 0;
    }
  }
  meta.baseIndex += k + (localCount - k - left);   // (more than k only if writing the log failed)
  localCount = left;
  if (!detailCount || detailFrom + detailCount != localCount) {
    InternalFS.remove(DETAIL_FILE);
    detailCount = 0;
    detailFrom = localCount;
  }
  meta.lifetimeArchived += arrows;
  saveMeta();
#if CALIBRATION_LOG
  Serial.printf("memory full: %lu oldest records removed (%lu arrows kept in the counters)\n",
                (unsigned long)k, (unsigned long)arrows);
#endif
  return true;
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
    if (r.flags & FLAG_NOARROW) continue;
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

// End score from the watch or the phone: [u32 start of the end][3 bytes: 6 values of 4 bits, low
// nibble first: 0 = M, 1-10, 11 = X, 15 = no arrow][u8 part][optional 12 bytes: 6 hit positions,
// x and y as i8 in 1/127 of the face radius]. Stored as one score record, followed by two position
// records when positions came along. A newer score for the same end replaces the older one (the
// phone page reads the last); all values 15 = score deleted.
static void addScore(uint32_t t, const uint8_t *vals, uint8_t part, const uint8_t *pos) {
  ShotRec s;
  s.t = t;
  s.peak10 = vals[0] | (vals[1] << 8);
  s.aim100 = vals[2];
  s.flags = FLAG_SCORE | (part & 3);
  ShotDetail none;
  memset(&none, 0, sizeof(none));
  if (!appendShot(s, none) || !pos) return;
  for (int h = 0; h < 2; h++) {                // 12 position bytes in two records of 7
    uint8_t raw[8] = {0};
    for (int k = 0; k < 7 && h * 7 + k < 12; k++) raw[k] = pos[h * 7 + k];
    raw[7] = FLAG_POS | h;
    ShotRec p;
    memcpy(&p, raw, 8);
    appendShot(p, none);
  }
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
//                          storage % u8, flags u8 (bit0 clock exact, bit1 active, bit2 bow resting
//                          still, e.g. on the stand, for 4 s), fw u8,
//                          7 bytes reserved (0)
//   0003 CTRL  write       [0x01, u32 fromIndex] = send records from global index
//                          [0x02, u32 expectedTotal] = clear pod log if total matches
//                          [0x04, u32 from, u16 count, u8 del] = mark arrows deleted (1) / restore (0)
//                          [0x05, 0] = delete the newest arrow   [0x06, 0] = day counters to zero
//                          [0x07, u32 time] = new end mark
//                          [0x09, 0] = calibration step 1: the bow is upright and level now (cant zero)
//                          [0x0A, 0] = calibration step 2: the bow points at the ground now
//                                      (sets the mounting point from the direction of the tilt)
//                          [0x0E, 0] = calibration step 3: the bow is tilted far to the side now
//                                      (works out how the ADXL375 sits, needed for the bow push)
//                          [0x0B, u32 start of the end, 3 bytes values, u8 part] = end score, or
//                          [0x0B, u32 start, 3 bytes values, 12 bytes positions] (20 bytes, part 0)
//                          [0x0F, u32 index] = continue the numbering from index (only forward, 2.3.2)
//   0004 DATA  notify      [u32 firstIndex][ShotRec][ShotRec]   end marker: [0xFFFFFFFF][u32 reached]
//                          [u32 total][u32 requested from] (2.3.2; up to 2.3.1 8 bytes, total at offset 4)
//                          (records with flag bit6 are end marks, bit7 = deleted; bits 4-6 all set = settings change)
//   0005 STATS read/notify 20 bytes: arrows u32, today u16, week u16, month u16, battery mV u16,
//                          last end mark u32, last arrow u32   (see updateStats)
//   0006 SETTINGS read/write/notify  11 bytes (struct Settings; 10 bytes up to 2.1);
//                          write [u8 arrows per end][u16 end pause s][u8 mounting point 0 back, 1 front, 2 side,
//                          0xFF unchanged (optional)][u8 target face, 0xFF unchanged (optional)]
//   0007 DETAIL notify     [u32 firstIndex][8-byte details of 2.0/2.1][..], for older phone pages
//   0008 DETAIL2 notify    [u32 index][ShotDetail], one packet per record of every DATA packet
//                          (to devices that switched on its notifications)
static const uint8_t uuidBytes[16] = {0xE0, 0x0E, 0x0F, 0x0C, 0x7A, 0x3B, 0x1E, 0x9D,
                                      0x5A, 0x4C, 0x1E, 0x5B, 0x00, 0x00, 0xC1, 0xA7};
static uint8_t uuidStore[9][16];

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
BLECharacteristic chDetail2(makeUuid(8));

volatile CtrlOp   opQueue[8];             // delete / undo / end mark / reset, done in loop()
volatile uint8_t  opHead = 0, opTail = 0;
volatile bool     tzChanged = false;
volatile bool     timePending = false;      // clock from phone / watch, applied in loop() (no flash work in a callback)
volatile uint32_t newTime = 0;
volatile int32_t  newTz = 0;
volatile bool     newTzOk = false;
volatile bool     settingsPending = false;  // new settings from phone / watch, saved in loop()

volatile uint8_t  newArrows = 6;
volatile uint16_t newPause = 100;
volatile int8_t   newMount = -1;            // -1 = unchanged
volatile int8_t   newRound = -1;            // -1 = unchanged

volatile bool     dumpPending  = false;
volatile uint32_t dumpFrom     = 0;
volatile uint16_t dumpConn     = BLE_CONN_HANDLE_INVALID;   // the connection that asked for the shots
volatile bool     connUp[BLE_MAX_CONNECTION];   // per connection handle: phone / watch connected
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

extern bool bowResting;
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
  b[11] = (timeExact ? 1 : 0) | (isActive ? 2 : 0) | (bowResting ? 4 : 0);
  b[12] = FW_VERSION;
  b[13] = FW_PATCH;
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

static void onSettingsWrite(uint16_t conn, BLECharacteristic *, uint8_t *data, uint16_t len) {
  if (len < 3) return;
  uint16_t p;
  memcpy(&p, data + 1, 2);
  newArrows = data[0] < 1 ? 1 : (data[0] > 24 ? 24 : data[0]);
  newPause = p < 20 ? 20 : (p > 900 ? 900 : p);
  newMount = (len >= 4 && data[3] <= MOUNT_SIDE) ? data[3] : -1;   // 3 bytes / 0xFF: mounting point unchanged
  newRound = (len >= 5 && data[4] <= ROUND_MAX) ? data[4] : -1;
  settingsPending = true;                      // flash write in loop()
}

static void onTimeWrite(uint16_t conn, BLECharacteristic *, uint8_t *data, uint16_t len) {
  if (len < 4) return;
  uint32_t t;
  int32_t tz = 0;
  memcpy(&t, data, 4);
  if (len >= 8) memcpy(&tz, data + 4, 4);      // [u32 unix time][i32 local offset in seconds]
  newTime = t;
  newTz = tz;
  newTzOk = len >= 8 && tz > -14 * 3600 && tz < 15 * 3600;
  timePending = true;                          // set in loop(): flash and the uptime counter only there
}

static void applyTime() {
  timePending = false;
  uint32_t t = newTime;
  if (newTzOk && newTz != meta.tzOffset) { meta.tzOffset = newTz; tzChanged = true; }
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
  if ((data[0] >= 0x04 && data[0] <= 0x07) || (data[0] >= 0x09 && data[0] <= 0x0B) || data[0] == 0x0E || data[0] == 0x0F) {   // queue for loop(): no flash work in a callback
    uint8_t next = (opHead + 1) % 8;
    if (next == opTail) return;                // queue full, drop
    for (int i = 0; i < 20; i++) opQueue[opHead].b[i] = i < len ? data[i] : 0;
    opQueue[opHead].len = len;
    opHead = next;
  }
}

static void setLevel();                       // calibration, in the shot detection part
static void setTilt();
static void setSide();
static void enterActive();
extern uint32_t lastMotionMs;

// The phone already has records up to a higher index than this pod (pod memory lost its numbering,
// or another pod was used before): the pod's records get numbers from newBase on, so they are new
// for the phone. Records and counters themselves don't change.
static void rebaseIndex(uint32_t newBase) {
  uint32_t old = meta.baseIndex;
  meta.ringFromIndex = meta.ringFromIndex >= old ? meta.ringFromIndex - old + newBase : newBase;
  meta.baseIndex = newBase;
  saveMeta();
}

static void processOps() {
  bool changed = false;
  while (opTail != opHead) {
    uint8_t b[20];
    for (int i = 0; i < 20; i++) b[i] = opQueue[opTail].b[i];
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
      case 0x09:                               // [0x09, 0] calibration 1: bow upright and level
      case 0x0A:                               // [0x0A, 0] calibration 2: bow pointed at the ground
      case 0x0E:                               // [0x0E, 0] calibration 3: bow tilted to the side
        if (!isActive && lsmOk && adxlOk) enterActive();   // idle: accelerometer at 12.5 Hz low power
        lastMotionMs = millis();               // and not back to idle between the steps
        if (b[0] == 0x09) setLevel(); else if (b[0] == 0x0A) setTilt(); else setSide();
        break;
      case 0x0F:                               // [0x0F, u32 index] (2.3.2): numbering continues from here
        if (v > totalShots() && v < 0xF0000000UL) rebaseIndex(v);   // only forward (phone's own count is higher)
        break;
      case 0x0B:                               // end score; a full 20-byte write carries the hit positions
        if (v > 1600000000UL) {                  // instead of the part byte (part 0, ends of up to 6)
          if (len >= 20) addScore(v, b + 5, 0, b + 8);
          else addScore(v, b + 5, b[8], nullptr);
        }
        break;
    }
    changed = true;
  }
  if (changed) updateInfo(true);
}

static uint8_t connCount() {                 // live connections (counted per handle, so it can't drift)
  uint8_t n = 0;
  for (int i = 0; i < BLE_MAX_CONNECTION; i++) if (connUp[i]) n++;
  return n;
}
static void onConnect(uint16_t h) {
  if (h < BLE_MAX_CONNECTION) connUp[h] = true;
#if CALIBRATION_LOG
  Serial.printf("BLE connect #%u (%u connected)\n", h, connCount());
#endif
  digitalWrite(LED_BLUE, LOW);
  if (connCount() < MAX_CONNECTIONS) Bluefruit.Advertising.start(0);   // stay visible for the 2nd device
}
static void onDisconnect(uint16_t h, uint8_t reason) {
  if (h < BLE_MAX_CONNECTION) connUp[h] = false;
#if CALIBRATION_LOG
  Serial.printf("BLE disconnect #%u reason 0x%02X (%u connected)\n", h, reason, connCount());
#endif
  if (connCount() == 0) digitalWrite(LED_BLUE, HIGH);
}

uint16_t podId = 0;
char     podName[24] = "PfeilZaehler3000";

static void bleBegin() {
  bool ok = Bluefruit.begin(MAX_CONNECTIONS, 0);
  DBG("  Bluefruit.begin -> %s\n", ok ? "OK" : "FAILED");
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
  chCtrl.setMaxLen(20);
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

  chDetail2.setProperties(CHR_PROPS_NOTIFY);
  chDetail2.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  chDetail2.setMaxLen(20);
  chDetail2.begin();

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
  bool adv = Bluefruit.Advertising.start(0);
  DBG("  Advertising.start -> %s, name %s, MAC %02X:%02X:%02X:%02X:%02X:%02X\n",
      adv ? "OK" : "FAILED", podName, mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
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
  bool withDetails2 = chDetail2.notifyEnabled(dumpConn);
  bool withDetails = !withDetails2 && chDetail.notifyEnabled(dumpConn);   // older phone pages
  File f(InternalFS);
  File df(InternalFS);
  bool dfOpen = (withDetails || withDetails2) && detailCount && df.open(DETAIL_FILE, FILE_O_READ);
  uint32_t idx = from;                         // next record to send = how far the dump got
  if (localStart < localCount && f.open(LOG_FILE, FILE_O_READ)) {
    f.seek(localStart * sizeof(ShotRec));
    while (idx < totalShots()) {
      uint8_t pkt[20];
      memcpy(pkt, &idx, 4);
      int n = f.read(pkt + 4, 2 * sizeof(ShotRec));
      int recs = n / (int)sizeof(ShotRec);
      if (recs < 1) break;
      if (!notifyRetry(chData, pkt, 4 + recs * sizeof(ShotRec))) break;
      if (withDetails || withDetails2) {        // details of the same records
        uint8_t dp[20] = {0};
        memcpy(dp, &idx, 4);
        bool sent = true;
        for (int k = 0; k < recs && sent; k++) {
          ShotDetail d;
          memset(&d, 0, sizeof(d));
          uint32_t local = idx - base + k;
          if (dfOpen && local >= detailFrom && local < detailFrom + detailCount) {
            df.seek(4 + (local - detailFrom) * sizeof(ShotDetail));
            df.read((uint8_t *)&d, sizeof(d));
          }
          if (withDetails2) {                    // one record per packet: [u32 index][12 bytes]
            uint32_t i2 = idx + k;
            memcpy(dp, &i2, 4);
            memcpy(dp + 4, &d, sizeof(d));
            sent = notifyRetry(chDetail2, dp, 4 + sizeof(d));
          } else {                               // 2.0 layout: two records of 8 bytes per packet
            ShotDetailV1 o;
            o.raise100 = d.raise100; o.draw100 = d.draw100; o.hold100 = d.hold100; o.wander = d.wander;
            o.pitch2 = d.pitch10 == DETAIL_NA16 ? DETAIL_NA : (int8_t)constrain(lroundf(d.pitch10 / 5.0f), -127, 127);
            o.cant2 = d.cant10 == DETAIL_NA16 ? DETAIL_NA : (int8_t)constrain(lroundf(d.cant10 / 5.0f), -127, 127);
            o.dropDeg = d.dropDeg; o.sideDeg = d.sideDeg;
            memcpy(dp + 4 + k * sizeof(o), &o, sizeof(o));
          }
        }
        if (!sent) break;
        if (withDetails && !notifyRetry(chDetail, dp, 4 + recs * sizeof(ShotDetailV1))) break;
      }
      idx += recs;
    }
    f.close();
  }
  if (dfOpen) df.close();
  updateInfo(false);                       // fresh battery reading for the app's "Update now"
  // end marker (2.3.2): [0xFFFFFFFF][u32 reached][u32 total][u32 requested from]. "reached" is the
  // first record NOT sent: equal to total when nothing went missing. Phone pages up to 2.3.1 read
  // offset 4 as the total, so a broken-off dump never makes them skip or clear records.
  uint8_t end[16];
  uint32_t marker = 0xFFFFFFFF, total = totalShots();
  memcpy(end, &marker, 4);
  memcpy(end + 4, &idx, 4);
  memcpy(end + 8, &total, 4);
  memcpy(end + 12, &req, 4);
  notifyRetry(chData, end, 16);
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

// 100 Hz: rotation since the previous sample (offset removed, degrees) of the last 1.28 s, for the aim movement
#define GYR_N 128
float    gyrHist[GYR_N][3];
uint16_t gyrHead = 0, gyrCount = 0;
uint16_t gyrHeadAtJolt = 0, gyrCountAtJolt = 0;  // position in it at the release jolt
float    gAtJolt[3] = {0, 0, 1};       // gravity 30 ms before the jolt

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
float      pushVel[2], pushPos[2];   // bow push after the release: P (right) and U (up), m/s and m
float      pushLp[2][2];             // two low-pass stages (~15 Hz): the hand pushes slowly, the riser rings fast
uint32_t   lastAdxlUs = 0;
bool       bowResting = false;       // no turning at all for REST_MS (on the stand), for the watch
bool       restChanged = false;
uint32_t   calmSinceMs = 0;

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

// angle between the bow's orientation in a history sample and at the release (degrees)
static float awayDeg(const HistSample &h, const float gr[3]) {
  float n = sqrtf((float)h.gx * h.gx + (float)h.gy * h.gy + (float)h.gz * h.gz);
  if (n < 1) return 0;
  float dot = (h.gx * gr[0] + h.gy * gr[1] + h.gz * gr[2]) / n;
  return acosf(dot > 1 ? 1 : (dot < -1 ? -1 : dot)) * 57.29578f;
}

static int16_t clamp16(float v) {
  long r = lroundf(v);
  return r > 32767 ? 32767 : (r < -32767 ? -32767 : (int16_t)r);
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
  Serial.println("-> active");
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
  Serial.println("-> idle");
#endif
  delay(100);
  lsmReadAccel(idleRef);
  meta.lastEpoch = nowEpoch();
  saveMeta();
  updateInfo(true);
}

static uint8_t mountOf() { return (settings.flags >> 2) & 3; }
static const char *mountName(uint8_t m) {
  return m == MOUNT_FRONT ? "front of the riser" : (m == MOUNT_SIDE ? "side of the riser" : "back of the riser");
}

// Calibration from the phone page, two steps:
//  1. bow upright, arrow level, as when aiming: this roll angle is cant 0, this Z angle is
//     bow angle 0 (it contains the tilt of the bushing), gravity is remembered
//  2. arrow pointed at the ground: the bow tilted forward. If the Z axis went down with the arrow,
//     Z points to the target (front of the riser); if it went up, Z points to the archer (back);
//     if the bow tilted but Z hardly changed its angle, the pod sits in a side bushing.
//  3. bow tilted far to the side: the change of gravity seen by both sensors in steps 2 and 3
//     tells how the ADXL375 is turned against the main sensor (needed for the bow push).
float    calPitch = 0;                         // Z angle in step 1
float    calGrav[3] = {0, 0, 1};               // gravity direction in step 1
float    calL[3][3], calA[3][3];               // gravity in steps 1-3: main sensor, ADXL375 (g)
uint32_t calAtMs = 0;                          // time of step 1 (the other steps must follow within 2 minutes)
uint8_t  calProgress = 0;                      // steps done in this calibration: 1 after step 1, 2 after step 2

// Before each calibration step: wait until the bow has been still for 1 s (2.3.2), at most 4 s, so
// tapping the phone with the other hand doesn't go into the measurement. Still = every gyro reading
// of the last second within 8 deg/s of their average, as steady as an aiming hold (the gyro's own
// offset cancels out); tapping the phone moves it by far more.
#define STILL_WIN 100                          // 1 s at 100 Hz
static bool waitStill() {
  static float w[STILL_WIN][3];
  uint32_t t0 = millis();
  int n = 0;
  while (millis() - t0 < 4000) {
    float g[3];
    if (lsmReadGyro(g)) {
      memcpy(w[n % STILL_WIN], g, sizeof(g));
      n++;
      if (n >= STILL_WIN) {
        float m[3] = {0, 0, 0};
        for (int i = 0; i < STILL_WIN; i++) for (int k = 0; k < 3; k++) m[k] += w[i][k] / STILL_WIN;
        bool still = true;
        for (int i = 0; i < STILL_WIN && still; i++) {
          float d0 = w[i][0] - m[0], d1 = w[i][1] - m[1], d2 = w[i][2] - m[2];
          if (d0 * d0 + d1 * d1 + d2 * d2 > 64.0f) still = false;
        }
        if (still) return true;
      }
    }
    delay(10);
  }
  return false;
}

// the bow was not still: tell the phone (status 6), but keep the stored calibration as it was
static void notStill() {
  uint8_t keep = settings.calStatus;
  settings.calStatus = 6;
  publishSettings(true);
  settings.calStatus = keep;
  publishSettings(false);
#if CALIBRATION_LOG
  Serial.println("calibration: the bow was not still, step not measured");
#endif
}

static bool readGravity(float s[3], float sa[3]) {   // average over 0.5 s, the bow held still (sa: ADXL375, may be null)
  float a[3];
  int n = 0, na = 0;
  s[0] = s[1] = s[2] = 0;
  if (sa) sa[0] = sa[1] = sa[2] = 0;
  for (int i = 0; i < 25; i++) {
    if (lsmReadAccel(a)) { for (int k = 0; k < 3; k++) s[k] += a[k]; n++; }
    if (sa && adxlRead(a)) { for (int k = 0; k < 3; k++) sa[k] += a[k]; na++; }
    delay(20);
  }
  if (n) for (int k = 0; k < 3; k++) s[k] /= n;
  if (sa && na) for (int k = 0; k < 3; k++) sa[k] /= na;
  return n > 0 && (!sa || na > 0);
}

static void cross3(const float a[3], const float b[3], float c[3]) {
  c[0] = a[1] * b[2] - a[2] * b[1];
  c[1] = a[2] * b[0] - a[0] * b[2];
  c[2] = a[0] * b[1] - a[1] * b[0];
}
static bool norm3(float v[3]) {
  float n = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  if (n < 1e-6f) return false;
  for (int k = 0; k < 3; k++) v[k] /= n;
  return true;
}
// orthonormal basis (columns) from two directions: e1 along d1, e2 normal to both, e3 = e1 x e2
static bool basis3(const float d1[3], const float d2[3], float B[9]) {
  float e1[3] = {d1[0], d1[1], d1[2]}, e2[3], e3[3];
  if (!norm3(e1)) return false;
  cross3(d1, d2, e2);
  if (!norm3(e2)) return false;
  cross3(e1, e2, e3);
  for (int r = 0; r < 3; r++) { B[r * 3] = e1[r]; B[r * 3 + 1] = e2[r]; B[r * 3 + 2] = e3[r]; }
  return true;
}

static void setLevel() {
  float s[3];
  if (!waitStill()) { notStill(); return; }
  if (!readGravity(s, calA[0])) return;
  calProgress = 1;
  memcpy(calL[0], s, sizeof(s));
  settings.cantRef10 = (int16_t)lroundf(rollOf(s) * 10.0f);
  settings.flags = (settings.flags | 1) & ~2;  // bit0 calibrated, bit1 (from the first shot) off
  settings.calStatus = 1;
  calPitch = pitchOf(s);
  settings.pitchRef10 = (int16_t)lroundf(calPitch * 10.0f);   // arrow level now: bow angle 0
  float n = sqrtf(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]);
  for (int k = 0; k < 3; k++) calGrav[k] = n > 0.2f ? s[k] / n : 0;
  calAtMs = millis();
  saveSettings();
  publishSettings(true);
#if CALIBRATION_LOG
  Serial.printf("calibration 1: cant zero set (roll %.1f deg, Z angle %.1f deg)\n", settings.cantRef10 / 10.0f, calPitch);
#endif
}

static void setTilt() {
  float s[3];
  if (!waitStill()) { notStill(); return; }
  if (!readGravity(s, calA[1])) return;
  memcpy(calL[1], s, sizeof(s));
  float d = pitchOf(s) - calPitch;             // change of the Z angle
  float n = sqrtf(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]);
  float dot = n > 0.2f ? (s[0] * calGrav[0] + s[1] * calGrav[1] + s[2] * calGrav[2]) / n : 1;
  float tilt = acosf(dot > 1 ? 1 : (dot < -1 ? -1 : dot)) * 57.29578f;   // how far the bow tilted
  if (calProgress < 1 || millis() - calAtMs > 120000UL || tilt < 25.0f) {
    settings.calStatus = 3;                    // step 1 missing, too late, or the bow hardly moved
  } else {
    uint8_t m = fabsf(d) < 0.6f * tilt ? MOUNT_SIDE : (d < 0 ? MOUNT_FRONT : MOUNT_BACK);
    settings.flags = (settings.flags & ~0x0C) | (m << 2);
    settings.calStatus = 2;
    calProgress = 2;
  }
  saveSettings();
  publishSettings(true);
#if CALIBRATION_LOG
  Serial.printf("calibration 2: tilted %.1f deg, Z angle changed by %.1f deg -> %s\n", tilt, d,
                settings.calStatus == 2 ? mountName(mountOf()) : "failed");
#endif
}

// Step 3: the gravity changes from step 1 to 2 and from step 1 to 3 are seen by both sensors.
// Differences cancel the ADXL375's offset; two different directions fix the turn between the two
// sensors completely (TRIAD method). calStatus 4 = done, 5 = failed (too little sideways tilt).
static void setSide() {
  float s[3];
  if (!waitStill()) { notStill(); return; }
  if (!readGravity(s, calA[2])) return;
  memcpy(calL[2], s, sizeof(s));
  float dL1[3], dL2[3], dA1[3], dA2[3], BL[9], BA[9];
  for (int k = 0; k < 3; k++) {
    dL1[k] = calL[1][k] - calL[0][k]; dL2[k] = calL[2][k] - calL[0][k];
    dA1[k] = calA[1][k] - calA[0][k]; dA2[k] = calA[2][k] - calA[0][k];
  }
  float n1 = sqrtf(dL2[0] * dL2[0] + dL2[1] * dL2[1] + dL2[2] * dL2[2]);   // ~ 2 sin(tilt/2) g
  float c[3];
  cross3(dL1, dL2, c);
  float spread = sqrtf(c[0] * c[0] + c[1] * c[1] + c[2] * c[2]);
  bool ok = calProgress >= 2 && millis() - calAtMs < 180000UL && n1 > 0.5f && spread > 0.15f &&
            basis3(dL1, dL2, BL) && basis3(dA1, dA2, BA);
  // the bow's own axes: step 1 = upright with the arrow level (gravity = straight down the bow),
  // step 2 = tipped forward about the bow's right axis
  float up[3] = {calL[0][0], calL[0][1], calL[0][2]}, right[3];
  cross3(calL[0], calL[1], right);             // up x (tipped towards the ground) = right
  ok = ok && norm3(up) && norm3(right);
  if (ok) {
    float d = right[0] * up[0] + right[1] * up[1] + right[2] * up[2];   // make "up" exactly square to "right"
    for (int k = 0; k < 3; k++) up[k] -= d * right[k];
    ok = norm3(up);
  }
  if (ok) {
    memcpy(bowRight, right, 12);
    memcpy(bowUp, up, 12);
    for (int r = 0; r < 3; r++)                  // R = BL * BA^T maps ADXL375 axes to main sensor axes
      for (int col = 0; col < 3; col++)
        adxlRot[r * 3 + col] = BL[r * 3] * BA[col * 3] + BL[r * 3 + 1] * BA[col * 3 + 1] + BL[r * 3 + 2] * BA[col * 3 + 2];
    adxlAligned = true;
    saveAdxlRot();
    settings.calStatus = 4;
  } else {
    settings.calStatus = 5;
  }
  saveSettings();
  publishSettings(true);
#if CALIBRATION_LOG
  Serial.printf("calibration 3: %s\n", ok ? "ADXL375 aligned" : "failed (tilt the bow further to the side)");
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

  // ---- bow on the stand: no turning at all for a while (the watch opens its score entry) ----
  if (turn < REST_DPS_STAND) { if (!calmSinceMs) calmSinceMs = ms; } else calmSinceMs = 0;
  bool r = calmSinceMs && ms - calmSinceMs >= REST_MS && ms - lastShotMs >= REST_MS;
  if (r != bowResting) { bowResting = r; restChanged = true; }

  // ---- shot details ----
  float dt = lastGyroMs ? (ms - lastGyroMs) / 1000.0f : 0.01f;
  if (dt > 0.05f) dt = 0.05f;
  lastGyroMs = ms;
  rateSm += 0.2f * (turn - rateSm);            // ~50 ms smoothing
  gyrHist[gyrHead][0] = b0 * dt; gyrHist[gyrHead][1] = b1 * dt; gyrHist[gyrHead][2] = b2 * dt;
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
    float gn = sqrtf(gLp[0] * gLp[0] + gLp[1] * gLp[1] + gLp[2] * gLp[2]);
    if (gn < 0.2f) gn = 1;
    h.gx = (int8_t)lroundf(gLp[0] / gn * 127.0f);
    h.gy = (int8_t)lroundf(gLp[1] / gn * 127.0f);
    h.gz = (int8_t)lroundf(gLp[2] / gn * 127.0f);
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

// At every jolt (quick, the jolt is still being measured): the bow's axes at the release, for the
// follow-through, and where the history stands. Nothing is changed here, it may not be a shot.
static void markJolt() {
  memcpy(gAtJolt, gravHist[(gravHead + GRAV_N - 3) % GRAV_N], sizeof(gAtJolt));   // 30 ms before
  gyrHeadAtJolt = gyrHead;
  gyrCountAtJolt = gyrCount;
  float *g = gAtJolt;
  float n = sqrtf(g[0] * g[0] + g[1] * g[1] + g[2] * g[2]);
  if (n < 0.2f) n = 1;
  for (int k = 0; k < 3; k++) axU[k] = g[k] / n;          // up
  // arrow direction = +Z on the front of the riser (lid towards the target), -Z on the back
  float zs = mountOf() == MOUNT_FRONT ? 1.0f : -1.0f;
  float px = -zs * axU[1], py = zs * axU[0];                // arrow x up = horizontal, to the right
  float pn = sqrtf(px * px + py * py);
  if (pn < 0.05f) { px = 1; py = 0; pn = 1; }               // bow pointing straight up or down
  axP[0] = px / pn; axP[1] = py / pn; axP[2] = 0;
}

// Once the jolt is confirmed as a shot: look back over the history from just before the jolt and
// work out the phases of the shot. (The 50 Hz history and the gravity stop at the jolt; the gyro
// history goes on, so it is read from the position marked at the jolt.)
static void analyseShot() {
  ShotDetail &d = pendingDetail;
  memset(&d, 0, sizeof(d));

  // gravity 30 ms before the jolt -> bow angle and cant at release
  float *g = gAtJolt;
  bool side = mountOf() == MOUNT_SIDE;
  if (side) {                                   // arrow direction unknown: no angles
    d.pitch10 = d.cant10 = DETAIL_NA16;
  } else {
    float zs = mountOf() == MOUNT_FRONT ? 1.0f : -1.0f;
    float ref = (settings.flags & 1) ? settings.pitchRef10 / 10.0f : 0;   // bushing tilt from the calibration
    float pitch = zs * (pitchOf(g) - ref);
    d.pitch10 = clamp16(pitch * 10.0f);
    float roll = rollOf(g);
    if (!(settings.flags & 3)) {                // not calibrated: the first shot counts as level
      settings.cantRef10 = (int16_t)lroundf(roll * 10.0f);
      settings.flags |= 2;
      settingsChanged = true;                   // flash write later: not during the jolt
    }
    float cant = settings.cantRef10 / 10.0f - roll;
    while (cant >= 180) cant -= 360;
    while (cant < -180) cant += 360;
    cant *= zs;                                 // seen from the archer: + = top limb to the right
    d.cant10 = clamp16(cant * 10.0f);
  }

  // aim movement in the last second (from 30 ms before the jolt): rotation summed to an angle
  // around the two aiming axes (side mount: all three axes), slow drift and gyro offset removed
  // by a straight-line fit
  const int W = 100;
  uint16_t since = (gyrHead + GYR_N - gyrHeadAtJolt) % GYR_N;   // gyro samples after the jolt
  if (gyrCountAtJolt >= W + 3 && since + W + 3 <= GYR_N) {
    float ax[3][3] = {{axP[0], axP[1], axP[2]}, {axU[0], axU[1], axU[2]}, {0, 0, 0}};
    if (side) {
      for (int a = 0; a < 3; a++) for (int b = 0; b < 3; b++) ax[a][b] = a == b ? 1.0f : 0.0f;
    }
    float sx = 0, sxx = 0, ssum = 0;
    for (int i = 0; i < W; i++) { sx += i; sxx += (float)i * i; }
    for (int a = 0; a < 3; a++) {
      float ang = 0, sv = 0, sxv = 0, svv = 0;
      for (int i = 0; i < W; i++) {
        int k = W + 2 - i;                      // oldest first
        float *w = gyrHist[(gyrHeadAtJolt + GYR_N - 1 - k) % GYR_N];
        ang += w[0] * ax[a][0] + w[1] * ax[a][1] + w[2] * ax[a][2];
        sv += ang; sxv += i * ang; svv += ang * ang;
      }
      float den = W * sxx - sx * sx;
      float b = (W * sxv - sx * sv) / den, c = (sv - b * sx) / W;
      // residual sum of squares of the straight-line fit
      float r = svv - 2 * c * sv - 2 * b * sxv + W * c * c + 2 * c * b * sx + b * b * sxx;
      ssum += r > 0 ? r : 0;
    }
    float u = sqrtf(ssum / W) * 100.0f;
    d.wander = u < 1 ? 1 : (u > 255 ? 255 : (uint8_t)lroundf(u));
  }

  // phases, from the newest history sample backwards (k = samples of 20 ms back)
  const uint16_t K0 = 2;                        // ~40 ms before the jolt = release position
  if (histCount <= K0 + 5) return;
  // orientation at the release; every other moment is compared with it (any mounting point)
  const HistSample &h0 = histAt(K0);
  float gr[3] = {(float)h0.gx, (float)h0.gy, (float)h0.gz};
  float grn = sqrtf(gr[0] * gr[0] + gr[1] * gr[1] + gr[2] * gr[2]);
  if (grn < 1) return;
  for (int i = 0; i < 3; i++) gr[i] /= grn;

  // hold: the bow almost still until the release
  uint16_t kHold = K0;
  while (kHold + 1 < histCount && histAt(kHold + 1).rate2 < HOLD_DPS * 2) kHold++;
  bool holding = histAt(K0).rate2 < HOLD_DPS * 2;
  if (holding) d.hold100 = enc100((uint32_t)(kHold + 1) * HIST_MS);

  // lowered: the last time the bow was tilted far from its position at the release
  uint16_t kAway = K0;
  while (kAway < histCount && awayDeg(histAt(kAway), gr) <= AWAY_DEG) kAway++;
  if (kAway >= histCount) return;               // the bow was not lowered (or too long ago)

  // raised: the first time after that the angle came within LEVEL_DEG of the release angle
  uint16_t kLevel = kAway - 1;
  while (kLevel > K0 && awayDeg(histAt(kLevel), gr) >= LEVEL_DEG) kLevel--;
  if (holding && kHold > kLevel) {              // still from the moment it was raised: no separate draw
    kHold = kLevel;
    d.hold100 = enc100((uint32_t)(kHold + 1) * HIST_MS);
  }

  // start of the raise: going further back, the lowest point, or where the bow was resting
  uint16_t kStart = kAway, k = kAway, rest = 0;
  float dmax = awayDeg(histAt(kAway), gr);
  while (k + 1 < histCount && k < kAway + 750) {          // up to 15 s further back
    k++;
    float dk = awayDeg(histAt(k), gr);
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
  bool side = mountOf() == MOUNT_SIDE;
  pendingDetail.dropDeg = side ? DETAIL_NA : clamp8(-followP);   // + = bow tips forward / down
  pendingDetail.sideDeg = side ? DETAIL_NA : clamp8(-followU);   // + = to the right
  bool push = !side && adxlAligned;
  pendingDetail.pushDown = push ? clamp8(-pushPos[1] * 1000.0f) : DETAIL_NA;   // + = down, mm
  pendingDetail.pushRight = push ? clamp8(pushPos[0] * 1000.0f) : DETAIL_NA;   // + = to the right, mm
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
                "follow-through down %+d right %+d deg | push down %+d right %+d mm\n",
                d.raise100 / 10.0f, d.draw100 / 10.0f, d.hold100 / 10.0f, d.wander / 100.0f,
                d.pitch10 / 10.0f, d.cant10 / 10.0f, d.dropDeg, d.sideDeg, d.pushDown, d.pushRight);
#endif
  // DEBUG: the last 12 s before the release, 5 values per second: "tilt/rotation" = how far the
  // bow was tilted away from its position at the release (deg) and its rotation (deg/s), oldest
  // first. Copy these lines when sending a shot for tuning the phase thresholds.
  if (histCount > 2) {
    const HistSample &h0 = histAt(2);
    float gr[3] = {(float)h0.gx, (float)h0.gy, (float)h0.gz};
    float grn = sqrtf(gr[0] * gr[0] + gr[1] * gr[1] + gr[2] * gr[2]);
    if (grn < 1) grn = 1;
    for (int i = 0; i < 3; i++) gr[i] /= grn;
    DBG("  trace (tilt/rotation, 0.2 s steps, oldest first):\n   ");
    for (int k = 600; k >= 0; k -= 10) {
      if (k >= histCount) continue;
      DBG(" %.0f/%.0f", awayDeg(histAt(k), gr), histAt(k).rate2 / 2.0f);
      if (k % 100 == 0) DBG("\n   ");
    }
    DBG("\n");
  }
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
  if (followActive && ms - spikeMs <= PUSH_MS) {   // bow push: acceleration across the arrow, integrated
    float dtp = (uint32_t)(us - lastAdxlUs) / 1e6f;
    if (dtp > 0.01f) dtp = 0.00125f;
    if (adxlAligned) {
      float v[3];
      for (int r = 0; r < 3; r++) v[r] = adxlRot[r * 3] * d0 + adxlRot[r * 3 + 1] * d1 + adxlRot[r * 3 + 2] * d2;
      float acc[2] = {(v[0] * bowRight[0] + v[1] * bowRight[1] + v[2] * bowRight[2]) * 9.81f,
                      (v[0] * bowUp[0] + v[1] * bowUp[1] + v[2] * bowUp[2]) * 9.81f};
      float al = dtp / (0.0106f + dtp);          // RC for 15 Hz
      for (int k = 0; k < 2; k++) {
        pushLp[k][0] += al * (acc[k] - pushLp[k][0]);
        pushLp[k][1] += al * (pushLp[k][0] - pushLp[k][1]);
        pushVel[k] += pushLp[k][1] * dtp;
        pushPos[k] += pushVel[k] * dtp;
      }
    }
  }
  lastAdxlUs = us;
  if (++sampleNo % 8 == 0) updateStillness(ms);   // ~100 Hz gyro
  if (restChanged) { restChanged = false; updateInfo(true); }   // the watch may open its score entry
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
      markJolt();                              // axes at the release; details once it is a shot
      followP = followU = 0;
      pushVel[0] = pushVel[1] = pushPos[0] = pushPos[1] = 0;
      memset(pushLp, 0, sizeof(pushLp));
      followActive = true;
    }
  } else {
    uint32_t dt = ms - spikeMs;
    if (dt < 10 && dev > peakG) peakG = dev;
    if (dt >= 10 && dev > RING_G) ringCount++;
    if (dt >= 150) {
      detect = WAITING;
      bool isShot = aimAtSpike >= MIN_AIM_MS && aimAtSpike <= MAX_AIM_MS && peakG >= MIN_PEAK_G &&
                    ringCount >= RING_MIN_SAMPLES;
#if CALIBRATION_LOG
      Serial.printf("jolt peak=%.1fg aim=%lums ring=%u -> %s\n", peakG,
                    (unsigned long)aimAtSpike, (unsigned)ringCount, isShot ? "SHOT" : "ignored");
#endif
      if (isShot) {
        lastShotMs = ms;
        analyseShot();                         // only now: a bump can't set the cant zero
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
  while (!Serial && millis() < 5000) delay(10);      // give the Serial Monitor time to attach
  uint32_t rr = NRF_POWER->RESETREAS; NRF_POWER->RESETREAS = 0xFFFFFFFF;
  DBG("\n=== PfeilZaehler3000 DEBUG (v2.3.2) === reset reason 0x%08lX\n", (unsigned long)rr);

  DBG("[1] Bluetooth\n");
  bleBegin();
  DBG("[2] Storage\n");
  loadStorage();
  InternalFS.remove("/owners.bin");            // left over from the owner lock of 2.3: removed
  rebuildCounts();
  DBG("  records on pod: %lu (base %lu, details from %lu), arrows lifetime %lu, time offset %ld s\n",
      (unsigned long)localCount, (unsigned long)meta.baseIndex, (unsigned long)detailFrom,
      (unsigned long)lifetime, (long)meta.tzOffset);
  DBG("  settings: %u arrows per end, new end after %u s, calibration %s, mounted on the %s\n",
      settings.arrowsPerEnd, settings.endPauseS,
      settings.calStatus >= 2 ? (settings.calStatus == 3 ? "failed" : (settings.calStatus == 4 ? "done, ADXL375 aligned" : "done")) : "not done", mountName(mountOf()));
  updateInfo(false);
  publishSettings(false);

  DBG("[3] Onboard IMU (LSM6DS3)\n");
  lsmOk = lsmBegin();
  DBG("  -> %s\n", lsmOk ? "found" : "MISSING");
  if (lsmOk) i2cScan(IMU_WIRE, "internal bus (Wire1)");
  DBG("[4] ADXL375 on D4/D5\n");
  adxlOk = adxlBegin();
  if (adxlOk) i2cScan(Wire, "external bus (Wire, D4=SDA D5=SCL)");
  DBG("  -> %s (address 0x%02X)\n", adxlOk ? "found" : "MISSING", adxlAddr);

  if (lsmOk && adxlOk) { led(LED_GREEN, 300); enterActive(); DBG("[5] running, tap the ADXL board\n"); }
  else                 { DBG("[5] sensor missing - Bluetooth still works, no shot detection\n"); }
}

void loop() {
  if (timePending) applyTime();                // first: records and end marks get the new time
  if (opTail != opHead) processOps();
  if (tzChanged) {                             // local day boundaries moved: count again
    tzChanged = false;
    saveMeta();
    rebuildCounts();
    updateInfo(true);
  }
  if (settingsPending) {                       // arrows per end / end pause from phone or watch
    settingsPending = false;
    if (newArrows != settings.arrowsPerEnd || newPause != settings.endPauseS) {
      // remember when it changed: a phone that syncs later judges the ends before and after correctly
      ShotRec c;
      c.t = nowEpoch();
      c.peak10 = newPause;
      c.aim100 = newArrows;
      c.flags = FLAG_SETTING | (timeExact ? 1 : 0);
      ShotDetail none;
      memset(&none, 0, sizeof(none));
      if (appendShot(c, none)) updateInfo(true);
    }
    settings.arrowsPerEnd = newArrows;
    settings.endPauseS = newPause;
    if (newMount >= 0) settings.flags = (settings.flags & ~0x0C) | (newMount << 2);
    if (newRound >= 0) settings.round = newRound;
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

  static uint32_t lastBeat = 0;
  if (millis() - lastBeat > 2000) {
    lastBeat = millis();
    float a[3] = {0, 0, 0};
    if (adxlOk) adxlRead(a);
    uint16_t td, wk, mo;
    dayStats(td, wk, mo);
    DBG("[status] up %lus | lsm=%d adxl=%d | active=%d | BLE links=%u adv=%d | records=%lu arrows=%lu "
        "today=%u week=%u month=%u | batt %umV | adxl g=(%.1f %.1f %.1f) | angle %+.1f rot %.1f dps\n",
        (unsigned long)(millis() / 1000), lsmOk, adxlOk, isActive, Bluefruit.connected(),
        Bluefruit.Advertising.isRunning(), (unsigned long)totalShots(), (unsigned long)lifetime,
        td, wk, mo, batteryMv(), a[0], a[1], a[2], pitchOf(gLp), rateSm);
    if (!Bluefruit.connected()) led(LED_BLUE, 20);              // blink = advertising heartbeat
    if (!lsmOk || !adxlOk) led(LED_RED, 20);
  }
  if (!lsmOk || !adxlOk) { delay(10); return; }               // no sensors: keep Bluetooth alive only
  if (isActive) activeStep();
  else idleStep();
}
