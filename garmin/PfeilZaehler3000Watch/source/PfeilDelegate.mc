// Buttons:
//   START  start / stop (and save) a Garmin activity
//   BACK   saves a running activity first; pressed again it leaves the app
//   DOWN   start a new end now
//   UP     delete the last arrow (asks first)
//   hold UP  use a different pod (asks first; the watch remembers the pod it connected to)
// Only the physical UP / DOWN buttons are used, so swiping the touchscreen does nothing.

import Toybox.Lang;
import Toybox.WatchUi;

class PfeilDelegate extends WatchUi.BehaviorDelegate {

    function initialize() {
        BehaviorDelegate.initialize();
    }

    function onKey(keyEvent) {
        var pod = getPod();
        var key = keyEvent.getKey();
        if (pod == null) {
            return false;
        }
        if (key == WatchUi.KEY_DOWN) {
            pod.newEnd();
            return true;
        }
        if (key == WatchUi.KEY_UP) {
            if (pod.sessionCount > 0) {
                WatchUi.pushView(new WatchUi.Confirmation("Delete last arrow?"), new UndoDelegate(), WatchUi.SLIDE_IMMEDIATE);
            }
            return true;
        }
        return false;           // START / BACK: handled below
    }

    function onMenu() {
        WatchUi.pushView(new WatchUi.Confirmation("Use a different pod?"), new ForgetPodDelegate(), WatchUi.SLIDE_IMMEDIATE);
        return true;
    }

    function onSelect() {
        var pod = getPod();
        if (pod != null) {
            pod.toggleRecording();
        }
        return true;
    }

    function onBack() {
        var pod = getPod();
        if (pod != null && pod.isRecording()) {
            pod.stopRecording(true);
            WatchUi.requestUpdate();
            return true;        // first BACK only saves; a second BACK leaves the app
        }
        return false;           // default: exit the app
    }
}

class UndoDelegate extends WatchUi.ConfirmationDelegate {

    function initialize() {
        ConfirmationDelegate.initialize();
    }

    function onResponse(response) {
        if (response == WatchUi.CONFIRM_YES) {
            var pod = getPod();
            if (pod != null) {
                pod.undoLast();
            }
        }
        return true;
    }
}

class ForgetPodDelegate extends WatchUi.ConfirmationDelegate {

    function initialize() {
        ConfirmationDelegate.initialize();
    }

    function onResponse(response) {
        if (response == WatchUi.CONFIRM_YES) {
            var pod = getPod();
            if (pod != null) {
                pod.forgetPod();
            }
        }
        return true;
    }
}
