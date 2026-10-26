# Changelog

## Release 2.3.2

Bug fixes from the code review: no arrow is lost when a sync breaks off or the pod memory is full.
Pod firmware, phone page and watch app are version 2.3.2.

**Pod firmware 2.3.2**
- The end of a sync says how far it really got; a sync that broke off can no longer make the phone
  skip arrows or clear them from the pod (#25).
- Memory full (e.g. only the watch is used for weeks): the oldest half of the records makes room,
  their arrows stay in the counters. A new arrow is never refused (#26).
- The clock from the phone or watch is stored in the main loop, not in the Bluetooth callback (#29).
- New command `0F`: the pod continues its numbering from the phone's count when it is behind (#29).
- Calibration: each step waits until the bow is still before it measures, so the bow can be held in
  one hand and the phone tapped with the other (#38).
- The blue LED counts connections per link and can no longer show the wrong state (#33).
- A change of arrows per end or end pause is stored with its time, so the phone page judges the ends
  before and after it correctly, also when it was changed on the watch while the phone was away.

**Phone page 2.3.2**
- Takes over only arrows that arrived without a gap, fetches the rest at once, and clears the pod
  only after a complete sync (#25).
- An arrow shot during an update is fetched right after it (#28).
- A pod whose numbering is behind the phone is renumbered, and an arrow is never imported twice
  (#29). Says when older records were no longer on the pod (#26).
- Connecting can't run twice at once, and every notification is handled once (#32). The page keeps
  its own "connected" state instead of the browser's, gives up on a pod that doesn't answer after
  6 s instead of 30 s and connects again once by itself.
- `?log` after the address shows a log of the connection to copy for bug reports (e.g. Bluefy).
- Calibration: "Hold the bow still…", and "The bow was not still" repeats the step (#38).
- The open end is worked out from the last 12 hours only, not the whole history every 10 s (#36).
- Ends are judged with the arrows per end that applied when they were shot, even if the change was
  made on the watch and the phone learned it only later (sessions already synced stay as they are).

**Watch app 2.3.2**
- Status line "Pod 85% full: sync phone" from 80 % pod memory (#26).
- Delete last arrow only while the pod is connected; otherwise "Pod not connected" (#27).
- *Use a different pod* drops everything still waiting for the old pod and starts a new session (#30).
- The score entry opens by itself only over the main screen, and not again for an end that was
  already opened by hand (#31).
- Ends of more than 6 arrows: the values as a text row in the score entry, two rows in the summary (#35).

**Documentation**
- Time-zone change: at most one evening's arrows can move to the neighbouring day in today/week/month
  (build guide, troubleshooting) (#34). Version texts and protocol comments updated (#37).

## Release 2.3.1

The owner lock of 2.3 is removed: it locked phones out of their own pod, made "Update now" hang
and was not understandable. A proper version is planned in #23. Pod firmware, phone page and watch
app are version 2.3.1.

**Pod firmware 2.3.1**
- Owner lock removed; a lock set with 2.3 is deleted at the first start. Every phone and watch can
  change the pod again, as up to 2.2 (#20).
- INFO byte 13 carries the third digit of the version, so the watch shows "Pod firmware 2.3.1".

**Phone page 2.3.1**
- *Settings → Owner lock* removed (#20).
- "Update now" no longer stays on "Updating…": an update already running is waited for.
- The calibration status says only whether and where the pod is calibrated.

**Watch app 2.3.1**
- No more "Pod … locked" in the status line (#20). The settings show the version as 2.3.1.

**Documentation**
- The safety, privacy and rules notes moved from the README to the build guide (section 16); the
  README keeps the two that matter most.

## Release 2.3

The owner lock, a washer against a turning pod, and safety notes. Pod firmware, phone page and
watch app are version 2.3; the housing gets an optional washer.

**Pod firmware 2.3**
- Owner lock: a pod with owners accepts changes and the download of the arrows only from its owner
  phones and watches; others see only the counts. Owners are added while the pod is on USB power,
  up to 4. A pod without owners works as before (#20).

**Phone page 2.3**
- *Settings → Owner lock*: lock to this phone, only this phone, unlock, with the current state. A
  phone that is not an owner shows "Locked to other devices" and syncs nothing (#20).

**Watch app 2.3**
- Logs in to a locked pod by itself, and adds itself as an owner while the pod is on USB and
  already locked on the phone. A pod locked to other devices shows as "Pod A3F2 locked" (#20).
- The settings show the watch app version and the pod's firmware (#19).

**Housing**
- Optional washer (`PfeilZaehler3000-washer.stl`, TPU, or a rubber washer 8.4 × 18 × 1 mm) between
  pod and riser, so the pod can't turn on its bolt (#21).

**Licence**
- PfeilZaehler3000 is now free for personal, non-commercial use only: the software under the
  PolyForm Strict License 1.0.0 with permissions for personal changes and for sharing unchanged
  copies, the housing and documentation under CC BY-NC-ND 4.0.
  Contributions: `CONTRIBUTING.md`.

**Documentation**
- README: safety, privacy and rules (mounting check, battery, building with an adult, sharing data,
  competition rules); `SECURITY.md` for reporting security problems privately.

## Release 2.2 – 18 October 2026

Scores per end, a report page and finer shot details. All three parts are updated: pod firmware,
phone page and watch app are all version 2.2. The pod must be calibrated again (three steps now).

**Pod firmware 2.2**
- A jolt counts as an arrow only after 3 to 20 s of stillness and with at least 25 g: assembling,
  carrying or knocking the bow no longer adds arrows (#13).
- Bow angle and cant in 0.1° steps instead of 0.5° (#14).
- Bow push: how far the bow moves down and sideways in the first 0.1 s after the release, in mm
  (#16). Needs the new third calibration step (bow tilted to the side), which works out how the
  two accelerometers sit relative to each other.
- End scores and hit positions are stored in the pod and passed between phone page and watch;
  the target face (full face or 3-spot) is a shared setting (#1).
- The pod tells the watch when the bow rests (e.g. on the stand) so the scoring screen can open.
- Details of arrows recorded with 2.0 and 2.1 are kept. About 1,100 arrows fit between syncs.

**Phone page 2.2**
- Scores per end: tap an end and enter its arrows (X, 10…1, M). Session and end lists show the
  score and the average per arrow; after a sync the page mentions ends without a score (#1).
- Target face in the settings: full face or 3-spot (only X, 10–6 and M are offered).
- "At aiming height" (draw + hold) is shown as the main figure; draw and hold alone are not
  reliable with a barebow (#15).
- Shot details show the bow push and angles in 0.1°; CSV with bow push and end scores.
- Calibration with three steps.
- **Report page** (Report button under Export & backup): Day, Week, Month and All time with charts
  of cant, aim movement, hold and aim movement by arrow in the end, time at aiming height,
  follow-through, bow push, scores and false counts, short notes next to the charts, and a table.
  Reads the arrows of the phone page or a backup file; *Save report* creates one HTML file to pass
  on, e.g. to a coach (#18).
- Restoring a backup also restores the end scores.

**Watch app 2.2**
- Scoring screen at the end of an end (#1): it opens by itself 3 s after the last arrow of an end (or when the bow is put on the stand),
  or by holding the screen; automatic, by hand only, or off in the settings. Three
  designs, with a preview after choosing one: numpad (default), dial and PiMalDaumen (tap
  where the arrow hit on a target face; the hits are saved with the score). After saving, a summary shows the end, the total and the
  average per arrow, compared with the session so far.
- The main screen shows the session score once ends are scored. Scores are written to the FIT
  file: end score per lap, session score and average per arrow.
- Settings: scoring (automatic, manual, off), design, target face, "score last end".

## Release 2.1 – 17 October 2026

Bug fixes. All three parts are updated: pod firmware, phone page and watch app are all version 2.1.

**Pod firmware 2.1**
- The shot details are worked out once a jolt is confirmed as a shot, no longer during the jolt:
  the peak of the jolt is measured completely, and a bump can no longer set the cant zero (#3).
- Calibration wakes the sensors from power-saving idle first, so both steps use the full-rate
  accelerometer (#8).
- Aim movement uses the measured time between gyro readings (#10).

**Phone page 2.1**
- The pod's memory is cleared after a sync only if the arrows were really stored on the phone (#4).
- The page asks the browser to keep its data, and after a sync it reminds you to make a backup when
  there is none or the last one is more than 30 days old (#4).
- Arrows take about 60 % less space in the browser (room for well over 100,000), with a warning
  before the storage is full (#5). Data of earlier versions is taken over and the old copies are
  removed (#9).
- The colour of "this end" is also updated when a pause starts a new end (#12).

**Watch app 2.1**
- Arrows shot while the watch was out of range are counted from the pod's time of the last arrow:
  no vibration when they arrive, a pause before them starts a new end, arrows from before the
  recording are not counted, and "time since the last arrow" is right (#6).
- Arrows deleted or restored on the phone change the session only if they are from today (#7).
- The time to sunset switches language straight away (#11).

**Phone page and watch app**
- The mounting point is sent to the pod only when it is changed on that device, so a mounting
  point found by the calibration is no longer overwritten (#2).

## Release 2.0 – 16 October 2026

All three parts are updated: pod firmware, phone page and watch app are all version 2.0.

**Pod firmware 2.0**
- Shot details for every arrow: raise, draw & set and hold times, aim movement in the last second,
  bow angle and cant at the release, follow-through. Stored next to the records (about 1,400 arrows
  with details between syncs) and sent with every sync.
- Mounting point: back of the riser (archer's side), front (target side) or side bushing. Bow
  angle, cant and follow-through are measured on the back and the front; on a side bushing the pod
  measures arrows, phase times and aim movement.
- Two-step calibration (bow upright with the arrow level, then pointed at the ground): sets the
  zero of the cant and of the bow angle (so a tilted bushing does not matter) and finds the
  mounting point by itself.
- Arrows per end, end pause and mounting point are stored in the pod and shared by the phone page
  and the watch; the last change counts.
- Wake-up counter and awake-time statistics removed.
- Debug firmware: prints the shot details and a 12 s motion trace after every arrow.

**Phone page 2.0**
- English, German and Polish (automatic or chosen in the settings).
- Arrow details, averages per end and session, split times in the arrow list, red delete symbols.
- Status symbols for connection, battery, pod memory and last sync.
- Settings: language, shooting distance, mounting point, calibration.
- CSV with the shot details; `app/?demo` shows the page with example data, without a pod.

**Watch app 2.0**
- English, German and Polish (as the watch, or chosen in the settings).
- Settings menu on hold UP: arrows per end, new end after, big number (end, session or today),
  time to sunset, mounting point, language, use a different pod.
- Time of day and optional time to sunset on the screen.
- The touchscreen no longer starts a recording.
- Arrows deleted on the phone are taken off the session and the end; restored arrows come back
  without a vibration.

## Release 1.8 – 12 October 2026

- Pod firmware 1.6: every pod has its own Bluetooth name `PfeilZaehler3000-XXXX` (from its
  Bluetooth address) and sends the ID in its advertising; the name is printed on the Serial
  Monitor.
- Debug firmware rebuilt on firmware 1.6.
- Phone page 1.8: remembers the chosen pod and only reconnects to that one; shows the pod's name.
- Watch app 1.2: remembers the first pod it connects to and ignores other pods; hold UP to use a
  different pod; shows the pod ID.
- Documentation: shared web address and own copies, finding your pod, Garmin Archery activity.

## Release 1.7 – 11 October 2026

- Pod firmware 1.5: today/week/month counters in the pod (local time), deleting and restoring
  arrows, end marks, up to 3 Bluetooth connections, connection log on the Serial Monitor.
- Debug firmware rebuilt on firmware 1.5.
- Phone page 1.7: delete single arrows or whole ends (with undo), "Reset today" and "New end"
  reach the pod and the watch, time zone sent to the pod, CSV with a "deleted" column.
- Watch app 1.1: today/week/month from the pod, DOWN = new end, UP = delete last arrow,
  one lap per end, double vibration at a complete end, battery warning, automatic reconnect.

## Release 1.6 – 11 October 2026

- Pod firmware 1.4: phone page and watch can be connected at the same time.
- Watch app 1.0 for the Garmin Epix (Gen 2).

## Release 1.5 – 10 October 2026

- Pod firmware 1.3: reports wake-ups and time awake. Phone page 1.5 shows them.

## Release 1.4 – 8 October 2026

- Pod firmware 1.2: the pod now goes into power saving after 3 minutes without movement (1.1 did
  not, because sensor noise counted as movement); battery voltage measured on every sync.
- Phone page 1.4: battery voltage with time of measurement, new button layout.

## Release 1.3 – 6 October 2026

- First published combination: housing v3, firmware 1.1, phone page 1.3.
