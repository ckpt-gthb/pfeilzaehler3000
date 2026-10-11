# PfeilZaehler3000 watch app – Garmin Epix Gen 2

Watch app version 1.1 (11 Oct 2026). Needs pod firmware v1.5 or newer.

The watch connects straight to the pod over Bluetooth, no phone needed while shooting.

**Screen:** pod battery at the top (orange when getting low, red when it needs charging), end number
and time since the last arrow, arrows in the current end (big, green at 6, orange above 6), then
arrows this session, today, this week and this month.
**Each arrow:** a short vibration, a double vibration when the end has 6 arrows. A pause of more than
100 s starts a new end.

**Buttons**
* **START:** start / stop a Garmin activity "Bogenschiessen". Arrows and ends are saved in it, and
  every end is a lap with its arrow count. **BACK** saves a running activity; press again to leave.
* **DOWN:** start a new end now (e.g. after a miss you don't want to shoot again).
* **UP:** delete the last arrow (asks first), e.g. after a bump that was counted.

## Shared with the phone page

Today, week and month come from the pod, so the watch and the phone page show the same numbers,
including arrows shot while the watch app was closed. An arrow deleted on the watch is also deleted
on the phone page, and the other way round. "New end" on the phone also starts a new end on the
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

The simulator (`Monkey C: Run`) only checks the screen layout; it can't reach the real pod.

## Good to know

* Garmin has no archery sport, so the activity is saved as a generic activity named
  "Bogenschiessen". Arrows and ends appear in its summary in Garmin Connect, plus an arrow chart and one lap per end.
* If the watch stays at "Looking for pod…" or shows "Pod in range?", check that the pod has
  firmware v1.5 and is within a few metres. "Pod firmware 1.5 needed" means the pod still has an
  older firmware.
