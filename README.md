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

`app/index.html` is the whole phone app. It needs to be served over https. With GitHub Pages
switched on for this repository (Settings → Pages → Deploy from branch `main`, folder `/ (root)`)
it is available at `https://<user>.github.io/<repository>/app/`.
Use Chrome or Edge (Android, desktop) or the Bluefy browser on iPhone.

## Repository

| Path | Content |
|---|---|
| `firmware/PfeilZaehler3000/` | Pod firmware 1.5 (Arduino, Seeed nRF52 core) |
| `firmware/PfeilZaehler3000Debug/` | Same firmware with diagnostic output, never sleeps |
| `app/index.html` | Phone page 1.7 |
| `garmin/PfeilZaehler3000Watch/` | Garmin Connect IQ watch app 1.1 |
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
