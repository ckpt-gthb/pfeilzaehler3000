# PfeilZaehler3000

Automatic arrow counter for recurve, barebow and compound bows.

A small sensor pod screws into a 5/16"-24 UNF stabilizer bushing of the riser (e.g. the lowest
bushing on the back of a Kinetic Sovren riser) and detects every shot from the vibration of the
bow. For every arrow it also measures the shot phases (raise, draw, hold), how much the bow moved while aiming, the bow
angle and cant at the release and the follow-through. A web page on the phone reads the pod over
Bluetooth and shows arrows per end, session, day, week and month, and the details of every arrow.
An optional Garmin watch app shows the current end and vibrates on every arrow.

<p>
  <img src="docs/images/pod-closed.png" height="320" alt="PfeilZaehler3000 pod, model of the closed housing">
  <img src="docs/images/pod-on-riser.jpg" height="320" alt="Kinetic Sovren 25 inch riser seen from the archer's side, with the pod in the lowest bushing">
  <img src="docs/images/pod-mounted.jpg" height="320" alt="Close-up of the mounted pod with the PfeilZaehler 3000 lid">
</p>

*The pod · mounted on a Kinetic Sovren 25" riser, on the archer's side in the lowest bushing below
the grip · close-up of the lid with the hole for the hex key*

## Features

- Detects shots from the riser's motion: aiming stillness (gyroscope), release jolt and ringing
  (±200 g accelerometer). Shots of other archers are not counted.
- Shot details from the gyroscope and accelerometer: raise, draw & set and hold times, aim
  movement in the last second (degrees and cm at the target), bow angle, cant and follow-through.
- Stores about 1,400 arrows with details in the pod; works without a phone during training.
- Phone page (single HTML file, Web Bluetooth): live count, ends, sessions with averages, totals,
  charts, arrow details, deleting single arrows or ends, CSV export, backup and restore.
- Garmin watch app (Epix Gen 2): time of day, current end, session, today/week/month, time to
  sunset, vibration per arrow, new end and undo buttons, settings menu, Garmin activities with one
  lap per end.
- Three mounting points: back of the riser (archer's side), front (target side) or a side bushing.
  A 10-second calibration on the phone finds the mounting point and sets the cant zero.
- Today/week/month counters and the settings (arrows per end, end pause, mounting point) are kept
  in the pod, so phone and watch show the same numbers.
- Phone page and watch app in English, German and Polish, chosen automatically or in the settings.
- Power saving after 3 minutes without movement; Bluetooth stays available.

## Building one

Everything is described in the build guide:
[docs/BUILD.md](docs/BUILD.md) · [PDF](docs/PfeilZaehler3000-Build-Guide.pdf)

Main parts: Seeed XIAO nRF52840 Sense, Adafruit ADXL375 breakout, 300 mAh LiPo (402535),
5/16"-24 UNF × 5/8" socket head cap screw, M2 heat-set inserts and screws, PETG-CF and PETG
filament. Approx. € 55–70 in parts.

## Phone page

**Open the phone page: https://ckpt-gthb.github.io/pfeilzaehler3000/app/**

**Try it without a pod:** https://ckpt-gthb.github.io/pfeilzaehler3000/app/?demo shows the page with
example data. Nothing is saved.

The page only contains the program. It runs in the phone's browser: each phone connects to its
own pod over Bluetooth and keeps its history locally. Nothing is sent to this repository or to
anyone else, so everyone can use this address without seeing other people's data.
Use Chrome or Edge (Android, desktop) or the Bluefy browser on iPhone.

<p>
  <img src="docs/images/app-status.png" width="260" alt="Phone page: status symbols, current end and totals">
  <img src="docs/images/app-history.png" width="260" alt="Phone page: totals and arrows per week">
  <img src="docs/images/app-sessions.png" width="260" alt="Phone page: sessions with ends and averages">
</p>
<p>
  <img src="docs/images/app-end-detail.png" width="260" alt="Phone page: the arrows of one end with split times, one arrow's details opened">
  <img src="docs/images/app-delete.png" width="260" alt="Phone page: one arrow deleted, with the restore symbol">
  <img src="docs/images/app-settings.png" width="260" alt="Phone page: settings with language, mounting point and calibration">
</p>

*Status, current end and totals · arrows per week · sessions with averages · the arrows of one end
with split times and one arrow's details · a deleted arrow (restore with the arrow symbol) ·
settings*

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
last arrow, holding UP opens the settings (arrows per end, end pause, which number is shown big,
time to sunset, pod). Build and install: [garmin/README.md](garmin/README.md).

<p>
  <img src="docs/images/watch-recording.png" width="200" alt="Watch app: recording, end 8 with 4 of 6 arrows, time to sunset">
  <img src="docs/images/watch-end-complete.png" width="200" alt="Watch app: end complete, 6 of 6 in green">
  <img src="docs/images/watch-extra-arrow.png" width="200" alt="Watch app: session as the big number, an extra arrow, pod battery getting low">
</p>
<p>
  <img src="docs/images/watch-reconnecting.png" width="200" alt="Watch app: today as the big number, pod reconnecting">
  <img src="docs/images/watch-settings.png" width="200" alt="Watch app: settings menu">
  <img src="docs/images/watch-different-pod.png" width="200" alt="Watch app: use a different pod, with a confirmation">
</p>

*Recording with time to sunset · end complete · session as the big number with an extra arrow ·
today as the big number while the pod is out of range · settings (hold UP) · changing to a different
pod*

## Repository

| Path | Content |
|---|---|
| `firmware/PfeilZaehler3000/` | Pod firmware 2.0 (Arduino, Seeed nRF52 core) |
| `firmware/PfeilZaehler3000Debug/` | Same firmware with diagnostic output, never sleeps |
| `app/index.html` | Phone page 2.0 |
| `garmin/PfeilZaehler3000Watch/` | Garmin Connect IQ watch app 2.0 |
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
