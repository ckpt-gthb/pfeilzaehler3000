// PfeilZaehler3000 – Garmin watch app (Epix Gen 2) for the PfeilZaehler3000 pod
// Watch app version 1.2 (12 Oct 2026). Needs pod firmware v1.5 or newer (v1.6 for own-pod pairing).
// Shows arrows per end / session / today live and vibrates on every arrow.
// START button: record the training as a Garmin activity (arrows saved in the activity).

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
        return [ new PfeilView(), new PfeilDelegate() ];
    }
}

function getPod() {
    return (Application.getApp() as PfeilApp).pod;
}
