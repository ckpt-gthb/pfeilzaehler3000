# Changelog

## Release 1.9 – 15 October 2026

- Pod firmware 1.7: shot details for every arrow – raise, draw & set and hold times, aim movement in
  the last second, bow angle and cant at the release, follow-through. Stored in a second file next
  to the records (about 1,400 arrows with details between syncs) and sent with every sync.
- Pod firmware 1.7: arrows per end and the end pause are stored in the pod and shared by the phone
  page and the watch (last change counts); "set cant to 0" command.
- Pod firmware 1.7: wake-up counter and awake-time statistics removed.
- Debug firmware rebuilt on firmware 1.7; prints the shot details and a 12 s motion trace per shot.
- Phone page 1.9: arrow details, averages per end and session, first time then split times in the
  arrow list, red delete symbols, status symbols for connection, battery, pod memory and last sync,
  shooting distance and cant zero in the settings, settings shared with the watch, CSV with the
  details; removed the connection hint and the awake/idle diagnostics; day chart labels no longer
  overlap. Demo mode: `app/?demo` shows example data and a simulated pod.
- Watch app 1.3: settings menu on hold UP (arrows per end, new end after, big number, time to
  sunset, use a different pod); time of day; optional time to sunset; choice of the big number
  (end, session or today); the touchscreen no longer starts a recording; arrows deleted on the
  phone are taken off the session and the end, restored arrows come back without a vibration.
  Demo mode for simulator screenshots (`DEMO` in `PodLink.mc`).
- Update order: pod firmware first. Phone page 1.9 and watch app 1.3 also work with older pod
  firmware, without details and shared settings.

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
