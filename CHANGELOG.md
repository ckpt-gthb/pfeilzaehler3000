# Changelog

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
