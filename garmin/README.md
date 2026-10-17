# PfeilZaehler3000 watch app – Garmin Epix Gen 2

Watch app version 2.1 (17 Oct 2026), for pod firmware 2.0 and later. English, German and Polish.

The watch connects straight to the pod over Bluetooth, no phone needed while shooting.

**Screen, from the top:** time of day; pod ID and battery, e.g. "Pod A3F2 3.95 V" (orange when
getting low, red when it needs charging); end number and time since the last arrow; the big number;
below it the other counters (this end, session, today, week); this month; the recording state; and,
if switched on, the time to sunset.
The big number is the current end by default (green when the end is complete, orange with an extra
arrow). It can be changed to the session or today's arrows in the settings.
**Each arrow:** a short vibration, a double vibration when the end is complete. A pause longer than
the "new end after" setting (100 s by default) starts a new end.

**Buttons**
* **START:** start / stop a Garmin activity "Bogenschiessen". Arrows and ends are saved in it, and
  every end is a lap with its arrow count. **BACK** saves a running activity; press again to leave.
* **DOWN:** start a new end now (e.g. after a miss you don't want to shoot again).
* **UP:** delete the last arrow (asks first), e.g. after a bump that was counted.
* **Hold UP:** settings (see below).
* The touchscreen is not used, so touching the screen can't start a recording by accident.

## Settings (hold UP)

| Setting | Values | Stored |
|---|---|---|
| Arrows per end | 3–12 (default 6) | in the pod, shared with the phone page |
| New end after | 30 s – 5 min (default 100 s) | in the pod, shared with the phone page |
| Big number | this end, session, today | on the watch |
| Time to sunset | on / off | on the watch |
| Mounting point | back of riser, front of riser, side bushing | in the pod, shared with the phone page |
| Language | Automatic (as the watch), English, Deutsch, Polski | on the watch |
| Use a different pod | asks first | on the watch |

"Arrows per end" and "new end after" are the same values as on the phone page: a change on either
one reaches the other through the pod, and the last change counts. The same goes for the mounting
point, which the calibration on the phone page also finds by itself (see the build guide).

The sunset time comes from the watch's weather position (synced by Garmin Connect), otherwise from
the last GPS position. The app asks for the location permission for this.

## Your own pod

The watch remembers the first pod it connects to and from then on only connects to that one, even
when other archers' pods are nearby. Open the app for the first time with only your own pod nearby
(its name, `PfeilZaehler3000-XXXX`, is shown on the Serial Monitor when the pod starts). To change
to another pod, hold UP, confirm, and keep only the new pod nearby.

## Shared with the phone page

Today, week and month come from the pod, so the watch and the phone page show the same numbers,
including arrows shot while the watch app was closed. An arrow deleted on the watch is also deleted
on the phone page, and the other way round: the watch then takes it off the session and the end
too. An arrow restored on the phone comes back without a vibration. "New end" on the phone also starts a new end on the
watch, and DOWN on the watch shows up on the phone. The watch and the phone page can be connected
at the same time.

If the pod goes out of range, the watch shows "Reconnecting..." and connects again by itself when
the pod is back. There's no need to restart the app.

## Build and install (about 30 minutes the first time; steps for a Mac)

1. **Java:** the Garmin compiler needs Java 17 or newer, e.g. the free Temurin JDK from
   adoptium.net. Check with `/usr/libexec/java_home -V` on a Mac.
2. **SDK:** download the *Connect IQ SDK Manager* from developer.garmin.com/connect-iq/sdk
   and sign in with your Garmin account. Under *SDK*, download the newest SDK and click
   *Use as Active SDK*. Under *Devices*, download **epix™ (Gen 2)** (listed under API level 5.2).
3. **VS Code:** install Visual Studio Code and, in its Extensions view, the extension
   **Monkey C** by Garmin.
4. **Check:** in VS Code press Cmd+Shift+P → `Monkey C: Verify Installation`.
5. **Developer key:** Cmd+Shift+P → `Monkey C: Generate a Developer Key`. Save it somewhere safe;
   every later update of the app must be built with the same key.
6. **Open** the folder `garmin/PfeilZaehler3000Watch` (File → Open Folder).
7. **Build:** Cmd+Shift+P → `Monkey C: Build for Device` → choose **epix2** → pick an output
   folder. You get a `.prg` file.
8. **Copy to the watch:** connect the Epix by USB. On a Mac it only shows up through an MTP app:
   install the free **OpenMTP** (openmtp.ganeshrvel.com) and quit Garmin Express first (also its
   menu-bar helper). If the watch's folders stay empty, switch OpenMTP between its legacy and Kalam
   mode in the settings. The watch shows its charging screen while connected; that is normal.
   Copy the `.prg` file into `GARMIN/APPS/` on the watch, then unplug.
9. **On the watch:** press START to open the activity list, scroll to the bottom, choose
   **Add**, and add **PfeilZaehler3000**. From then on it's in the list like an activity.

Never publish the developer key, and keep `.prg` files out of the repository.

## Good to know

* Garmin watches have their own Archery activity, but Connect IQ apps cannot record with it (the
  Connect IQ API has no archery sport type). The activity is therefore saved as a generic activity
  named "Bogenschiessen", with arrows and ends in its summary, an arrow chart and one lap per end.
  The activity type can be changed afterwards in Garmin Connect.
* If the watch stays at "Looking for pod…" or shows "Pod … in range?", check that the pod is
  charged and within a few metres. "Update pod firmware" means the pod has an old firmware: flash
  the current firmware.
* German and Polish texts use the watch's own fonts. The language follows the watch unless it is
  chosen in the settings.
