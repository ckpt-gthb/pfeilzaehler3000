# PfeilZaehler3000 – Build and User Guide

Automatic arrow counter for recurve, barebow and compound bows.
Release 1.7 · 11 October 2026

| Part | Version |
|---|---|
| Pod firmware | 1.5 (`firmware/PfeilZaehler3000`) |
| Debug firmware | 1.5-debug (`firmware/PfeilZaehler3000Debug`) |
| Phone page | 1.7 (`app/index.html`) |
| Garmin watch app | 1.1 (`garmin/PfeilZaehler3000Watch`) |
| Housing | v3 (`housing/`) |

![The assembled pod](images/pod-closed.png)

## 1. What it is

PfeilZaehler3000 is a small sensor pod that is screwed into a stabilizer bushing of the bow. It
detects each shot from the vibration of the riser and stores the time of every arrow. A web page on
the phone reads the pod over Bluetooth and shows arrows per end, session, day, week and month. An
optional Garmin watch app shows the current end and vibrates on every arrow.

The pod only measures the bow it is mounted on. Shots from other archers on the same target or
the same line are not counted. It does not record scores or arrow positions.

| | |
|---|---|
| Size | 65 × 35 × 27 mm |
| Weight | about 35–45 g including the bolt |
| Mount | 5/16"-24 UNF bolt into a stabilizer bushing |
| Battery | LiPo 3.7 V, 300–320 mAh, charged over USB-C |
| Memory | about 2,800 records between two syncs |
| Connection | Bluetooth Low Energy, up to 3 devices at the same time |
| Approx. cost | € 55–70 for all parts (2026 prices) |

World Archery rules do not allow electronic devices on the bow in competition. Remove the pod for
tournaments.

## 2. How a shot is detected

![Shot pattern seen by the pod](images/detection.svg)

The pod contains two sensors: the gyroscope of the XIAO board and a high-g accelerometer
(ADXL375, ±200 g). An arrow is counted only when these three things happen in this order:

1. **Holding still.** The bow turns less than 25 °/s for at least 0.6 s (aiming).
2. **Release jolt.** The accelerometer sees a peak above 12 g when the string stops.
3. **Ringing.** The riser keeps vibrating above 2 g for at least 6 samples within 10–150 ms after
   the peak.

After a counted shot, the pod ignores further jolts for 1.5 s. Putting the bow on a stand, bumping
it or carrying it does not produce this sequence. All thresholds can be changed in the firmware
(see section 11).

**Power saving.** After 3 minutes without movement the pod switches the sensors to a low-power
mode and only checks for movement 4 times per second. Picking up the bow wakes it. Bluetooth stays
available while it is idle, so the phone and the watch can connect at any time.

**Ends and sessions.** The phone page groups arrows into ends and sessions by the pauses between
them: by default a pause longer than 100 s starts a new end, and a pause longer than 45 minutes
starts a new session. "New end" on the phone or DOWN on the watch also starts a new end.

## 3. Bow compatibility

The pod is fastened with a **5/16"-24 UNF** bolt, the standard thread of stabilizer and weight
bushings on target bows. It fits any riser with a free bushing of this thread, for example the
front stabilizer bushing of the **Kinetic Sovren** barebow riser, and equally the stabilizer,
V-bar or weight bushings of other recurve, barebow and compound risers.

Check before building:

- **Thread.** The bushing must be 5/16"-24 UNF. Metric bushings (for example M6 or M8) need a
  different bolt and a different hole in the bolt column.
- **Depth.** About 11 mm of the 5/8" (16 mm) bolt sticks out of the pod. If the bolt bottoms out
  before the pod sits tight, use a 1/2" (12.7 mm) bolt.
- **Space.** The bolt is at one end of the pod, so the pod reaches about 50 mm to one side of the
  bushing. It must not touch the limbs, the arrow rest, the plunger or the string.
- **Flat seat.** The pod sits on the flat face around the bushing. Its underside has ribs that keep
  it from turning once the bolt is tight.

The pod works in any orientation. Its alignment only matters for looks and clearance. The extra
35–45 g changes the balance of the bow, so other weights may need adjusting.

## 4. Parts and tools

| Qty | Part | Notes |
|---|---|---|
| 1 | Seeed Studio XIAO nRF52840 **Sense** | Must be the "Sense" version (built-in gyroscope). Bluetooth, battery charger and USB-C on board. |
| 1 | Adafruit ADXL375 breakout (STEMMA QT, ±200 g) | High-g accelerometer. Normal ±16 g sensors clip at the release jolt. |
| 1 | LiPo cell 3.7 V, 300–320 mAh, size 402535 (4 × 25 × 35 mm), with protection circuit | Fits the pocket in the lid. A 502535 also fits without the foam pad. |
| 4 | Silicone wire AWG 28–30, about 6 cm each (red, black, yellow, blue) | Sensor to XIAO. The STEMMA QT plugs do not fit in the closed pod. |
| 1 | 5/16"-24 UNF socket head cap screw, 5/8" (16 mm), steel or A2 stainless (DE: Zylinderschraube mit Innensechskant 5/16-24 UNF × 5/8) | Mounting bolt. Turns freely in the pod. |
| 8 | M2 brass heat-set inserts, 4 mm long, 3.2 mm outside diameter | 4 for the sensor, 4 for the lid. |
| 8 | M2 × 12 mm countersunk screws (DIN 965 / ISO 7046) | 4 hold the sensor, 4 hold the lid. |
| 1 | 1 mm foam sheet (EVA or neoprene), 35 × 25 mm | Pad on the battery. |
| — | Heat-shrink tubing, thread locker (Loctite 243, optional) | |
| ~35 g | PETG-CF and plain PETG filament | Base and lid in PETG-CF, clamp frame in plain PETG. |
| 1 | STEMMA QT to jumper-wire cable (optional) | Only for the bench test before soldering. |

Optional vibration motor (a buzz on every counted arrow): 10 mm coin vibration motor 3 V,
NPN transistor (2N2222 or S8050), 1 kΩ resistor, 1N4148 diode.

**Tools:** 3D printer with a hardened nozzle (for PETG-CF), soldering iron with a fine tip, side
cutters, 1/4" hex key with a long arm (at least 30 mm), USB-C data cable, a computer with the
Arduino IDE, and a phone with Chrome (Android) or the Bluefy browser (iPhone).

## 5. Print the housing

| File | Material | Infill | Orientation |
|---|---|---|---|
| `PfeilZaehler3000-base.stl` | PETG-CF | 100 % rectilinear | open side up |
| `PfeilZaehler3000-frame.stl` | PETG (no carbon fibre) | 40 % gyroid | fingers up |
| `PfeilZaehler3000-lid.stl` or `PfeilZaehler3000-lid-2color.3mf` | PETG-CF | 40 % gyroid | top face down |

- 0.2 mm layers, 4 walls, no supports.
- **Base with 100 % infill.** With a lower infill, the change to solid layers inside the 8 mm floor
  leaves a visible line on the outside wall at about 7 mm.
- **Frame in plain PETG**, so its fingers stay slightly springy. Carbon-filled PETG is too stiff and
  brittle for them.
- **Lid.** `PfeilZaehler3000-lid.stl` has the logo and "PFEILZÄHLER 3000" engraved 0.6 mm deep.
  For a two-colour lid (e.g. Bambu Lab with AMS), open `PfeilZaehler3000-lid-2color.3mf`, split the
  object into parts and put the small parts on the second filament. The text looks mirrored in the
  slicer because the lid prints face down.
- The insert holes are 2.9 mm. `PfeilZaehler3000-insert-test.stl` (in the repository) has holes
  from 2.7 to 3.0 mm for checking other inserts.
- `pod.scad` (OpenSCAD) is the source of all parts. Sizes such as the insert holes or the USB-C slot
  can be changed there.

![Exploded view: base with sensor and XIAO, clamp frame, battery and foam pad, lid](images/pod-exploded.png)

## 6. How the parts fit together

No tape or glue is used inside the pod.

- **Base:** bolt column at one end, 4 bosses with inserts for the sensor, inserts in the 4 corner
  posts for the lid, and a cradle for the XIAO with 4 support pads, corner guides and an end stop.
- **ADXL375:** screwed onto the bosses next to the bolt column, so the release jolt reaches it
  without damping.
- **XIAO:** lies on 2.5 mm pads with its USB-C socket in the wall slot. The battery wires run
  underneath.
- **Clamp frame:** sits on the shoulders of the corner posts. Two fingers press on the XIAO's corners
  and a third on its USB-C socket.
- **Battery and foam pad:** lie on the frame inside the pocket of the lid.
- **Lid:** the walls of the battery pocket press the frame down when the 4 lid screws are tightened.
  A hole above the bolt column lets the hex key through.

![Top view without frame and battery, to scale](images/topview.svg)

![Section through the middle of the pod](images/section.svg)

## 7. Wiring

![Wiring](images/wiring.svg)

| From | To | Wire |
|---|---|---|
| ADXL375 VIN (header pin 1) | XIAO 3V3 | red |
| ADXL375 GND (pin 3) | XIAO GND | black |
| ADXL375 SCL (pin 4) | XIAO D5 | yellow |
| ADXL375 SDA (pin 5) | XIAO D4 | blue |
| LiPo red | XIAO BAT+ pad (back of the board) | check polarity twice |
| LiPo black | XIAO BAT− pad | |
| Motor (optional) | 3V3 → motor → transistor collector; emitter → GND; D2 → 1 kΩ → base | diode across the motor, stripe to 3V3; set `USE_MOTOR 1` |

## 8. Build steps

### 8.1 Inserts and bolt

1. Set the soldering iron to about 230–240 °C. Put each insert straight on its hole and let it sink
   in with light pressure until it is flush. First the 4 sensor bosses, then the 4 corner posts.
   Keep the iron vertical.
2. Drop the bolt into the bolt column from the top, thread first. It must turn freely. About 11 mm
   of thread sticks out of the bottom.
3. Test-fit the empty base on the bow (see section 3).

### 8.2 Arduino IDE

1. Install the Arduino IDE 2.
2. In **File → Preferences → Additional boards manager URLs** add
   `https://files.seeedstudio.com/arduino/package_seeeduino_boards_index.json`
3. In the Boards Manager install **Seeed nRF52 Boards**. Do not use the package called
   "Seeed nRF52 mbed-enabled Boards".
4. Select the board **Seeed XIAO nRF52840 Sense**. No additional libraries are needed.

### 8.3 Flash and bench test

1. Connect the ADXL375 to the XIAO with the STEMMA QT cable or 4 jumper wires (no soldering yet) and
   connect the XIAO to the computer.
2. Open `firmware/PfeilZaehler3000/PfeilZaehler3000.ino` and click **Upload**. If the upload fails,
   double-tap the reset button on the XIAO and upload again.
3. A long green flash means both sensors were found. Repeating red blinks mean a sensor is missing
   (see section 13).
4. Open the **Serial Monitor** at 115200 baud. Hold the board still for a second, then tap it
   sharply with a screwdriver handle. The monitor shows `jolt peak=… aim=… ring=… -> SHOT` or
   `-> ignored`. A light tap on the table is usually ignored, which is correct.
5. Leave the board untouched for 3 minutes. The monitor shows `-> idle`, and `-> active` when it
   is moved again.

### 8.4 Assembly

1. Screw the ADXL375 onto its bosses with 4 M2 × 12 mm screws, component side up, with the header
   row facing the XIAO. Tighten until snug. The board must not rock, and it must not be bent.
2. Solder the 4 wires into the ADXL375 header holes (VIN, GND, SCL, SDA). Keep the joints flat.
3. Solder the battery to the BAT+ and BAT− pads on the back of the XIAO and cover the joints with
   heat-shrink.
4. Put the XIAO into its cradle, USB-C first into the wall slot, with the battery wires underneath.
5. Route the sensor wires between the corner guides and solder them to 3V3, GND, D4 and D5. Cut them
   short enough that they stay below the height of the frame.
6. Place the clamp frame, fingers down, onto the post shoulders. Lead the battery wires through the
   side opening.
7. Put the battery on the frame and the foam pad on the battery. Fit the lid so the battery sits in
   its pocket, then tighten the 4 lid screws evenly.
8. Shake the pod. Nothing may rattle. Charge it over USB-C (about 3–4 hours from empty, at 100 mA).

Never pinch or puncture the LiPo cell. If the lid does not close without force, a wire is lying
under the battery.

### 8.5 Mounting on the bow

1. Hold the pod against the bushing and start the bolt by hand with the hex key through the lid.
2. Align the pod, hold it in place and tighten the bolt firmly. The pod must not move.
3. To remove the pod, loosen the bolt through the lid. The pod does not need to be opened.

## 9. Phone page

The phone page is a single file, `app/index.html`. Web Bluetooth only works on pages loaded over
https, so the file has to be hosted once, for example:

- **GitHub Pages:** in this repository the page is available at
  `https://<user>.github.io/<repository>/app/` after Pages is switched on (Settings → Pages →
  Deploy from branch → `main`, folder `/ (root)`).
- **Netlify Drop:** drag the `app` folder onto app.netlify.com/drop.

Supported browsers: Chrome or Edge on Android, Windows, macOS, Linux and ChromeOS; on iPhone and iPad
the **Bluefy** browser (Safari and the other iOS browsers have no Web Bluetooth). Add the page to the
home screen for quick access. If Bluefy asks for "Paste" permission at every start, set
**Settings → Apps → Bluefy → Paste from Other Apps → Deny**.

All data is stored in the browser (local storage of that web address). Use **Backup** regularly.

### Functions

**Connection**

- **Connect pod & sync** – first connection: choose *PfeilZaehler3000* in the list. The page sets
  the pod's clock and time zone and downloads all new arrows.
- **Update now** – while connected: downloads new arrows and reads the battery voltage again.
- **Reconnect & update** – when the connection was lost.
- **Choose pod** – pick a different pod.
- **Disconnect** – ends the connection.
- While connected, the page updates by itself after every arrow. When the page is opened again, it
  reconnects automatically.

**Status line:** the pod's record counter, battery voltage with the time it was measured, pod
memory used, and whether the pod is awake or idle, how long it has been running, the share of
time it was awake and how often it woke up (for checking battery use).

**This end:** arrows in the current end out of the set number per end (green when complete,
orange when more). **New end** starts a new end immediately, without confirmation. The pod and
the watch start a new end too.

**Totals:** arrows today, this week (from Monday), this month and all time.

**History:** bar chart of arrows per day (30 days), per week (12 weeks) or per month (12 months).

**Sessions:** the last 40 sessions with date, time span, number of ends and arrows. Ends with a
different number of arrows than set are highlighted. **Tap an end** to see its arrows with time,
jolt strength (g) and aiming time:

- **Delete / Restore** a single arrow, e.g. a bump that was counted.
- **Delete whole end**, with **Undo** in the status line.
- Deleted arrows are removed from all statistics and are also deleted on the pod and on the watch.

**Settings**

| Setting | Default | Effect |
|---|---|---|
| Arrows per end | 6 | Target for the end display and highlighting |
| New end after a pause of (s) | 100 | Pause that separates ends |
| New session after a pause of (min) | 45 | Pause that separates sessions |
| Clear pod memory after sync when fuller than (%) | 50 | Frees the pod's memory once the arrows are safely on the phone |

Changes to the first two settings only apply from the moment they are changed; earlier ends keep
their grouping.

**Export & backup**

- **Export CSV** – one line per arrow: index, local date and time, unix time, jolt (g), aiming
  time (s), clock exact (1/0), deleted (1/0). On iPhone it opens the share sheet.
- **Backup** – the complete history as a JSON file.
- **Restore** – reads a backup from a file or from pasted text and merges it with the existing data.

**Reset** (both ask for confirmation)

- **Reset today** – deletes today's arrows on the phone, the pod and the watch.
- **Reset all stats** – deletes the whole history on this phone and, if the pod is connected, clears
  the pod's memory and its day counters. Make a backup first.

## 10. Garmin watch app

The watch app has been built for the **Garmin Epix (Gen 2)**. It connects to the pod directly; the
phone is not needed while shooting. Other Garmin watches with Connect IQ 3.2 or newer and Bluetooth
Low Energy support may work after adding them in `manifest.xml`; this has not been tested.

### Screen

- Top: pod battery voltage (orange when getting low, red when it needs charging), or the
  connection state: *Looking for pod…*, *Pod in range?*, *Connecting…*, *Reconnecting…*.
- End number and time since the last arrow.
- Arrows in the current end, large (green at 6, orange above 6).
- Arrows this session, today, this week and this month. Today, week and month come from the pod
  and match the phone page, including arrows shot while the watch app was closed.
- Bottom: recording time, or a short button help.

### Buttons

| Button | Function |
|---|---|
| START | Start / stop recording a Garmin activity "Bogenschiessen" |
| BACK | Saves a running activity; pressed again, leaves the app |
| DOWN | Start a new end now (also shown on the phone page) |
| UP | Delete the last arrow, after a confirmation (also deleted on the phone page) |

Swiping the touchscreen has no function, to prevent accidental changes.

- **Vibration:** short on every arrow, double when the end reaches 6 arrows.
- **Ends:** a pause of more than 100 s or DOWN starts a new end (fixed values on the watch).
- **Recording:** a recorded activity contains duration, heart rate and calories like any other
  activity, plus the number of arrows and ends in the summary, a chart of the arrow count and one
  lap per end with its number of arrows. Garmin has no archery sport type, so it is saved as a
  generic activity.
- **Reconnecting:** if the pod is out of range, the watch reconnects by itself when it is back.

### Build and install

1. Install **Java** (JDK 17 or newer, e.g. Temurin from adoptium.net). Check with
   `/usr/libexec/java_home -V` on a Mac.
2. Install the **Connect IQ SDK Manager** (developer.garmin.com/connect-iq/sdk), sign in, download
   the newest SDK and set it as active. Under *Devices* download **epix (Gen 2)** (listed under API
   level 5.2).
3. Install **Visual Studio Code** and the extension **Monkey C** (Garmin).
4. In VS Code: *Monkey C: Verify Installation*, then *Monkey C: Generate a Developer Key*. Keep the
   key private and safe; updates of the app must be built with the same key.
5. Open the folder `garmin/PfeilZaehler3000Watch` and run *Monkey C: Build for Device* → **epix2**.
   The result is a `.prg` file.
6. Copy the `.prg` file to `GARMIN/APPS` on the watch. On a Mac this needs an MTP tool such as
   **OpenMTP**; quit Garmin Express first. If the watch's folders do not show, switch OpenMTP between
   its legacy and Kalam mode.
7. Unplug the watch. Press START, scroll to the end of the activity list, choose **Add** and add
   **PfeilZaehler3000**.

## 11. Calibration and tuning

1. Shoot 3–4 normal ends and sync. Check whether every end shows the right number of arrows.
2. Use **Export CSV** and look at the `peak_g` column: it shows how hard the releases hit the riser.
   Light draw weights give smaller values.
3. **Arrows missing:** set `SPIKE_G` to about half of the lowest `peak_g` and upload the firmware
   again.
4. **Extra arrows:** raise `MIN_AIM_MS` (e.g. to 1000) or `SPIKE_G`.

The values are at the top of the firmware file:

| Name | Default | Meaning |
|---|---|---|
| `SPIKE_G` | 12 | Release jolt threshold (g) |
| `RING_G` | 2 | Ringing level (g) |
| `RING_MIN_SAMPLES` | 6 | Ringing samples needed 10–150 ms after the jolt |
| `STILL_DPS` | 25 | Rotation below this counts as holding still (°/s) |
| `MIN_AIM_MS` | 600 | Minimum holding time before the release (ms) |
| `REFRACTORY_MS` | 1500 | Dead time after a counted shot (ms) |
| `IDLE_AFTER_MS` | 180000 | Time without movement before power saving (ms) |
| `CALIBRATION_LOG` | 1 | Print every jolt to the Serial Monitor |
| `USE_MOTOR` | 0 | 1 = vibration motor on D2 |

## 12. Daily use and charging

- Shoot as usual. A short green flash on the pod confirms a counted arrow.
- Open the phone page after training (or keep it connected). The watch app can run at the same
  time.
- Charge when the voltage is below about 3.6 V. The watch shows this in orange and red.
- Battery life depends on how long the pod is moved and awake. The status line of the phone page
  shows the share of time the pod was awake since its last start; a pod lying unused should be
  awake only a few percent of the time.

**Pod LED**

| LED | Meaning |
|---|---|
| Green, 0.3 s after switching on | Both sensors found |
| Short green flash | Arrow counted and stored |
| Short red flash after a shot | Arrow not stored: memory full, sync with the phone |
| Red, 1 blink repeating | Gyroscope (XIAO) not found |
| Red, 2 blinks repeating | ADXL375 not found |
| Blue | Phone or watch connected |

## 13. Troubleshooting

| Problem | Solution |
|---|---|
| Upload fails, or no port appears | Double-tap reset (bootloader mode). Use a USB cable that carries data. |
| macOS: `exec: "python" not found` during upload | In Terminal: `sudo ln -sf "$(which python3)" /usr/local/bin/python` |
| macOS: error about ASCII / locale during upload | `launchctl setenv LANG en_US.UTF-8` and `launchctl setenv LC_ALL en_US.UTF-8`, then restart the Arduino IDE |
| Red, 1 blink repeating | Wrong board selected. Use "Seeed XIAO nRF52840 Sense" from the non-mbed package. |
| Red, 2 blinks repeating | Check the ADXL375 wiring: SDA to D4, SCL to D5, VIN to 3V3. |
| Pod hangs after the battery was soldered | Flash the debug firmware and read the Serial Monitor (section 14). |
| Ends often one arrow short | Lower `SPIKE_G`. Check that the sensor screws and the mounting bolt are tight. |
| Extra arrows | Raise `MIN_AIM_MS` or `SPIKE_G`. Putting the bow down hard can look like a shot. |
| Battery empty after a few days | Check on the Serial Monitor that the pod shows `-> idle` after 3 minutes. The debug firmware never sleeps. |
| "No Web Bluetooth" on the phone page | Open it over https in Chrome or Edge, or in Bluefy on iPhone. |
| Arrows listed without a date | The pod restarted with an empty battery and had no clock. Connect the phone page after charging. |
| Watch shows "Pod firmware 1.5 needed" | Flash the current pod firmware. |
| Watch stays at "Pod in range?" | Check the distance to the pod and that the pod is charged. |

## 14. Debug firmware

`firmware/PfeilZaehler3000Debug` has the same functions and Bluetooth interface as firmware 1.5,
so the phone page and the watch work with it. In addition it:

- waits up to 5 s for the Serial Monitor and prints the reset reason,
- prints every start-up step, the state of both I2C buses and the I2C addresses found,
- prints a status line every 2 s: sensors, awake/idle, Bluetooth connections and advertising,
  records, arrows today/week/month, battery voltage and the current accelerometer values,
- blinks the blue LED briefly every 2 s while nothing is connected,
- never switches to power saving, so it empties the battery within a few days.

Use it for diagnosis only, then upload the normal firmware again.

## 15. Files

| Path | Content |
|---|---|
| `firmware/PfeilZaehler3000/` | Pod firmware 1.5 (Arduino) |
| `firmware/PfeilZaehler3000Debug/` | Debug firmware |
| `app/index.html` | Phone page 1.7 |
| `garmin/PfeilZaehler3000Watch/` | Garmin watch app 1.1 (Connect IQ project) |
| `housing/` | STL and 3MF files, OpenSCAD source `pod.scad`, insert test |
| `docs/` | This guide, Bluetooth protocol, images |

## 16. Licence and safety

Software (firmware, phone page, watch app) is published under the **MIT License**; the housing
models, drawings and this guide under **Creative Commons Attribution-ShareAlike 4.0 (CC BY-SA 4.0)**.
See `LICENSE` and `LICENSE-hardware`.

This is a hobby project without any warranty. Lithium polymer cells can catch fire if they are
damaged, short-circuited or charged incorrectly: use cells with a protection circuit, never puncture
or squeeze them, and do not charge the pod unattended. Kinetic, Garmin, Epix, Seeed, Adafruit and
Bluefy are trademarks of their respective owners; this project is not affiliated with them.
