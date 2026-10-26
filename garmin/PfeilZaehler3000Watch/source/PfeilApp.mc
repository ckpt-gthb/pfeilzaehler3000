// PfeilZaehler3000 (c) 2026 ckpt-gthb – free for personal, non-commercial use; PolyForm Strict 1.0.0 with permissions, see LICENSE
// PfeilZaehler3000 – Garmin watch app (Epix Gen 2) for the PfeilZaehler3000 pod
// Watch app version 2.3.2, for pod firmware 2.0 and later (scoring needs firmware 2.2).
// Shows arrows per end / session / today live and vibrates on every arrow.
// START button: record the training as a Garmin activity (arrows saved in the activity).
// Hold UP: settings (also scoring, the mounting point of the pod and the language: English, German, Polish).
// Long touch on the screen, or by itself when the end is complete and the bow rests on the stand:
// score the end.

import Toybox.Application;
import Toybox.Lang;
import Toybox.WatchUi;

class PfeilApp extends Application.AppBase {

    var pod = null;        // PodLink: Bluetooth connection to the pod + counters

    function initialize() {
        AppBase.initialize();
    }

    function onStart(state) {
        pod = new PodLink();
        pod.start();
    }

    function onStop(state) {
        if (pod != null) {
            pod.shutdown();
        }
    }

    function getInitialView() as [WatchUi.Views] or [WatchUi.Views, WatchUi.InputDelegates] {
        if (DEMO >= 5 && DEMO <= 8 && pod != null) {          // example score screens for screenshots
            var sc = pod.demoScoreScreen() as Array;
            return [ sc[0] as WatchUi.View, sc[1] as WatchUi.InputDelegate ];
        }
        return [ new PfeilView(), new PfeilDelegate() ];
    }
}

function getPod() {
    return (Application.getApp() as PfeilApp).pod;
}
