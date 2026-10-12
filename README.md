# PfeilZaehler3000

Automatic arrow counter for recurve, barebow and compound bows.

A small sensor pod screws into a 5/16"-24 UNF stabilizer bushing of the riser (e.g. the front
bushing of a Kinetic Sovren) and detects every shot from the vibration of the bow. A web page on the
phone reads the pod over Bluetooth and shows arrows per end, session, day, week and month. An
optional Garmin watch app shows the current end and vibrates on every arrow.

![PfeilZaehler3000 pod](docs/images/pod-closed.png)

## Features

- Detects shots from the riser's motion: aiming stillness (gyroscope), release jolt and ringing
  (±200 g accelerometer). Shots of other archers are not counted.
- Stores about 2,800 records in the pod; works without a phone during training.
- Phone page (single HTML file, Web Bluetooth): live count, ends, sessions, totals, charts,
  deleting single arrows or ends, CSV export, backup and restore.
- Garmin watch app (Epix Gen 2): current end, session, today/week/month, vibration per arrow,
  new end and undo buttons, Garmin activities with one lap per end.
- Today/week/month counters are kept in the pod, so phone and watch show the same numbers.
- Power saving after 3 minutes without movement; Bluetooth stays available.

## Building one

Everything is described in the build guide:
[docs/BUILD.md](docs/BUILD.md) · [PDF](docs/PfeilZaehler3000-Build-Guide.pdf)

Main parts: Seeed XIAO nRF52840 Sense, Adafruit ADXL375 breakout, 300 mAh LiPo (402535),
5/16"-24 UNF × 5/8" socket head cap screw, M2 heat-set inserts and screws, PETG-CF and PETG
filament. Approx. € 55–70 in parts.

## Phone page

**Open the phone page: https://ckpt-gthb.github.io/pfeilzaehler3000/app/**

The page only contains the program. It runs in the phone's browser: each phone connects to its
own pod over Bluetooth and keeps its history locally. Nothing is sent to this repository or to
anyone else, so everyone can use this address without seeing other people's data.
Use Chrome or Edge (Android, desktop) or the Bluefy browser on iPhone.

<p>
  <img src="docs/images/app-status.png" width="260" alt="Phone page: connection status, battery, current end">
  <img src="docs/images/app-history.png" width="260" alt="Phone page: totals, history chart and sessions with ends">
  <img src="docs/images/app-end-detail.png" width="260" alt="Phone page: arrows of one end, with delete buttons">
</p>

*Connection and current end · totals, history and sessions · the arrows of one end (time, jolt, aiming time) with delete buttons*

To host your own copy, fork this repository and switch on GitHub Pages for the fork
(Settings → Pages → Deploy from branch `main`, folder `/ (root)`); the page is then at
`https://<your-user>.github.io/<repository>/app/`. The history belongs to the web address, so use
Backup / Restore when changing to another address.

Every pod has its own Bluetooth name, `PfeilZaehler3000-XXXX`. Write the 4 characters on your pod
and connect the phone and the watch once with only your pod nearby: both remember it, so several
pods can be used at the same range.

## Garmin watch app

The watch app (Garmin Epix Gen 2) connects to the pod directly. It shows the current end, the time
since the last arrow, arrows this session, today, this week and this month, and vibrates on every
arrow. START records a Garmin activity with one lap per end, DOWN starts a new end, UP deletes the
last arrow. Build and install: [garmin/README.md](garmin/README.md).

<p>
  <img src="docs/images/watch-recording.png" width="260" alt="Watch app: end 8 with 3 of 6 arrows, recording for 25 minutes">
  <img src="docs/images/watch-extra-arrow.png" width="260" alt="Watch app: 7 of 6 arrows in orange, pod battery low">
  <img src="docs/images/watch-end-complete.png" width="260" alt="Watch app: end complete with 6 of 6 in green, reconnecting to the pod">
</p>

*Recording an activity · one arrow too many, pod battery low · end complete while the pod reconnects (simulator, example values)*

## Repository

| Path | Content |
|---|---|
| `firmware/PfeilZaehler3000/` | Pod firmware 1.6 (Arduino, Seeed nRF52 core) |
| `firmware/PfeilZaehler3000Debug/` | Same firmware with diagnostic output, never sleeps |
| `app/index.html` | Phone page 1.8 |
| `garmin/PfeilZaehler3000Watch/` | Garmin Connect IQ watch app 1.2 |
| `housing/` | STL / 3MF print files and the OpenSCAD source `pod.scad` |
| `docs/` | Build guide, Bluetooth protocol, images |

The Bluetooth interface is documented in [docs/PROTOCOL.md](docs/PROTOCOL.md), changes in
[CHANGELOG.md](CHANGELOG.md).

## Licence

- Software (firmware, phone page, watch app): [MIT](LICENSE)
- Housing models, drawings and documentation: [CC BY-SA 4.0](LICENSE-hardware)

World Archery rules do not allow electronic devices on the bow in competition. This is a hobby
project without warranty; handle LiPo cells with care. Kinetic, Garmin, Epix, Seeed, Adafruit and
Bluefy are trademarks of their respective owners; this project is not affiliated with them.
