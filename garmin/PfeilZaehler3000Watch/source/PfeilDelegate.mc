// Buttons:
//   START    start / stop (and save) a Garmin activity
//   BACK     saves a running activity first; pressed again it leaves the app
//   DOWN     start a new end now
//   UP       delete the last arrow (asks first)
//   hold UP  settings: arrows per end, new end after, big number, time to sunset, mounting point,
//            language, different pod
// The touchscreen does nothing: taps, holds and swipes are ignored, so a touch can't start
// a recording or change anything by accident.

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
                WatchUi.pushView(new WatchUi.Confirmation(tr(:deleteLastQ)), new UndoDelegate(), WatchUi.SLIDE_IMMEDIATE);
            }
            return true;
        }
        return false;           // START / BACK / hold UP: handled below
    }

    // touchscreen: ignored (a tap would otherwise count as START)
    function onTap(clickEvent) {
        return true;
    }

    function onHold(clickEvent) {
        return true;
    }

    function onSwipe(swipeEvent) {
        return true;
    }

    function onMenu() {
        openSettings();
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

// ================= settings menu (hold UP) =================

const ARROW_CHOICES = [3, 4, 5, 6, 7, 8, 9, 10, 12];
const GAP_CHOICES   = [30, 45, 60, 90, 100, 120, 150, 180, 240, 300];

function gapText(sec) {
    if (sec % 60 == 0) {
        return (sec / 60).format("%d") + " min";
    }
    return sec.format("%d") + " s";
}

function bigName(n) {
    return tr(n == 1 ? :bigSession : (n == 2 ? :bigToday : :bigEnd));
}

function mountName(m) {
    return tr(m == 1 ? :mount1 : (m == 2 ? :mount2 : :mount0));
}

function langName(n) {
    if (n == 1) { return "English"; }
    if (n == 2) { return "Deutsch"; }
    if (n == 3) { return "Polski"; }
    return tr(:langAuto);
}

function openSettings() {
    var pod = getPod();
    if (pod == null) {
        return;
    }
    var m = new WatchUi.Menu2({ :title => tr(:settings) });
    m.addItem(new WatchUi.MenuItem(tr(:arrowsEnd), pod.arrowsEnd.toString(), :arrows, null));
    m.addItem(new WatchUi.MenuItem(tr(:newEndAfter), gapText(pod.endGapS), :gap, null));
    m.addItem(new WatchUi.MenuItem(tr(:bigNumber), bigName(pod.bigNum), :big, null));
    m.addItem(new WatchUi.ToggleMenuItem(tr(:timeSunset), null, :sunset, pod.showSunset, null));
    m.addItem(new WatchUi.MenuItem(tr(:mounting), mountName(pod.mount), :mount, null));
    m.addItem(new WatchUi.MenuItem(tr(:language), langName(pod.langPref), :lang, null));
    m.addItem(new WatchUi.MenuItem(tr(:otherPod), pod.podLabel(), :forget, null));
    WatchUi.pushView(m, new SettingsDelegate(), WatchUi.SLIDE_UP);
}

class SettingsDelegate extends WatchUi.Menu2InputDelegate {

    function initialize() {
        Menu2InputDelegate.initialize();
    }

    function onSelect(item) {
        var pod = getPod();
        if (pod == null) {
            return;
        }
        var id = item.getId();
        if (id == :arrows) {
            pushChoice(tr(:arrowsEnd), ARROW_CHOICES, pod.arrowsEnd, :arrows, item);
        } else if (id == :gap) {
            pushChoice(tr(:newEndAfter), GAP_CHOICES, pod.endGapS, :gap, item);
        } else if (id == :big) {
            pushChoice(tr(:bigNumber), [0, 1, 2], pod.bigNum, :big, item);
        } else if (id == :mount) {
            pushChoice(tr(:mounting), [0, 1, 2], pod.mount, :mount, item);
        } else if (id == :lang) {
            pushChoice(tr(:language), [0, 1, 2, 3], pod.langPref, :lang, item);
        } else if (id == :sunset) {
            pod.setShowSunset((item as WatchUi.ToggleMenuItem).isEnabled());
        } else if (id == :forget) {
            WatchUi.pushView(new WatchUi.Confirmation(tr(:otherPodQ)), new ForgetPodDelegate(), WatchUi.SLIDE_IMMEDIATE);
        }
    }

    function onBack() {
        WatchUi.popView(WatchUi.SLIDE_DOWN);
    }
}

function choiceText(kind, v) {
    if (kind == :gap) {
        return gapText(v);
    }
    if (kind == :big) {
        return bigName(v);
    }
    if (kind == :mount) {
        return mountName(v);
    }
    if (kind == :lang) {
        return langName(v);
    }
    return v.toString();
}

// a list of values; the current one is marked and has the focus
function pushChoice(title, values, current, kind, parentItem) {
    var m = new WatchUi.Menu2({ :title => title });
    var focus = 0;
    for (var i = 0; i < values.size(); i++) {
        var v = values[i] as Number;
        m.addItem(new WatchUi.MenuItem(choiceText(kind, v), (v == current) ? tr(:current) : null, v, null));
        if (v == current) {
            focus = i;
        }
    }
    m.setFocus(focus);
    WatchUi.pushView(m, new ChoiceDelegate(kind, parentItem), WatchUi.SLIDE_LEFT);
}

class ChoiceDelegate extends WatchUi.Menu2InputDelegate {

    var kind;
    var parentItem;

    function initialize(k, p) {
        Menu2InputDelegate.initialize();
        kind = k;
        parentItem = p;
    }

    function onSelect(item) {
        var pod = getPod();
        var v = item.getId() as Number;
        if (pod != null) {
            if (kind == :arrows) {
                pod.setArrowsEnd(v);
            } else if (kind == :gap) {
                pod.setEndGap(v);
            } else if (kind == :big) {
                pod.setBigNum(v);
            } else if (kind == :mount) {
                pod.setMount(v);
            } else if (kind == :lang) {
                pod.setLangPref(v);
                WatchUi.popView(WatchUi.SLIDE_IMMEDIATE);   // close the list
                WatchUi.popView(WatchUi.SLIDE_IMMEDIATE);   // and the settings, written in the old language
                openSettings();                             // open them again in the new one
                return;
            }
        }
        (parentItem as WatchUi.MenuItem).setSubLabel(choiceText(kind, v));
        WatchUi.popView(WatchUi.SLIDE_RIGHT);
    }

    function onBack() {
        WatchUi.popView(WatchUi.SLIDE_RIGHT);
    }
}
