# Changelog

## Release 1.8 – 12 October 2026

- Allow for multiple archers using the pod at the same range at the same time
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
