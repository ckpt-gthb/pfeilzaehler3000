// PfeilZaehler3000 (c) 2026 ckpt-gthb – free for personal, non-commercial use; PolyForm Strict 1.0.0 with permissions, see LICENSE
// Buttons:
//   START    start / stop (and save) a Garmin activity
//   BACK     saves a running activity first; pressed again it leaves the app
//   DOWN     start a new end now
//   UP       delete the last arrow (asks first)
//   hold UP  settings: arrows per end, new end after, big number, time to sunset, scoring, score
//            design, target face, score last end, mounting point, language, different pod
// The touchscreen does nothing on the main screen, so a touch can't start a recording or change
// anything by accident – except a long touch, which opens the score entry for the last end.
// On the score entry, touch is used (numpad, dial or PiMalDaumen face).

import Toybox.Lang;
import Toybox.WatchUi;
import Toybox.Attention;

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
            if (pod.sessionCount > 0 && !pod.live()) {
                notConnected();                  // undo needs the pod right now (see PodLink.live)
            } else if (pod.sessionCount > 0) {
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

    // long touch: score the last end (unless scoring is off)
    function onHold(clickEvent) {
        var pod = getPod();
        if (pod != null && pod.scoreMode != 2) {
            pod.openScoring(false);
        }
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

// short message "Pod not connected" (toast where the watch has it, else a buzz)
function notConnected() {
    if (WatchUi has :showToast) {
        WatchUi.showToast(tr(:undoOffline), null);
    } else if (Attention has :vibrate) {
        Attention.vibrate([ new Attention.VibeProfile(30, 60), new Attention.VibeProfile(0, 60), new Attention.VibeProfile(30, 60) ]);
    }
}

class UndoDelegate extends WatchUi.ConfirmationDelegate {

    function initialize() {
        ConfirmationDelegate.initialize();
    }

    function onResponse(response) {
        if (response == WatchUi.CONFIRM_YES) {
            var pod = getPod();
            if (pod != null && pod.live()) {
                pod.undoLast();
            } else if (pod != null) {
                notConnected();                  // the link dropped while the question was open
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

function faceName(f) {
    return tr(f == 1 ? :face1 : :face0);
}

function scoreModeName(m) {
    return tr(m == 1 ? :scoreManual : (m == 2 ? :scoreOff : :scoreAuto));
}

function designName(d) {
    return tr(d == 1 ? :dDial : (d == 2 ? :dPiMal : :dNumpad));
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
    m.addItem(new WatchUi.MenuItem(tr(:scoring), scoreModeName(pod.scoreMode), :scoreMode, null));
    m.addItem(new WatchUi.MenuItem(tr(:design), designName(pod.scoreDesign), :design, null));
    m.addItem(new WatchUi.MenuItem(tr(:faceL), faceName(pod.face), :face, null));
    m.addItem(new WatchUi.MenuItem(tr(:scoreLast), null, :scoreNow, null));
    m.addItem(new WatchUi.MenuItem(tr(:mounting), mountName(pod.mount), :mount, null));
    m.addItem(new WatchUi.MenuItem(tr(:language), langName(pod.langPref), :lang, null));
    m.addItem(new WatchUi.MenuItem(tr(:otherPod), pod.podLabel(), :forget, null));
    var fw = (pod.podFw % 100 > 0) ? trf(:podFw, [(pod.podFw % 100 / 10) + "." + (pod.podFw % 10) + (pod.podPatch > 0 ? "." + pod.podPatch : "")]) : null;
    m.addItem(new WatchUi.MenuItem(tr(:version) + " " + APP_VERSION, fw, :about, null));
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
        } else if (id == :scoreMode) {
            pushChoice(tr(:scoring), [0, 1, 2], pod.scoreMode, :scoreMode, item);
        } else if (id == :design) {
            pushChoice(tr(:design), [0, 1, 2], pod.scoreDesign, :design, item);
        } else if (id == :face) {
            pushChoice(tr(:faceL), [0, 1], pod.face, :face, item);
        } else if (id == :scoreNow) {
            if (pod.scoreTarget() == null) {
                item.setSubLabel(tr(:noEndYet));
                return;
            }
            WatchUi.popView(WatchUi.SLIDE_IMMEDIATE);   // leave the settings, then open the score entry
            pod.openScoring(false);
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
    if (kind == :face) {
        return faceName(v);
    }
    if (kind == :scoreMode) {
        return scoreModeName(v);
    }
    if (kind == :design) {
        return designName(v);
    }
    return v.toString();
}

// after choosing a score design: an example of it, closed by any button or a tap
function showDesignPreview(d) {
    var pod = getPod();
    var v = new ScoreView(d, 6, 1, 0, pod != null && pod.face == 1);
    v.preview = true;
    v.vals = [10, 9, 9, 8, 8];
    v.hits = (d == 2) ? [[-22, -8], [30, 25], [-26, 20], [40, -14], [-12, 43]] : [null, null, null, null, null];
    WatchUi.pushView(v, new ScoreDelegate(v), WatchUi.SLIDE_LEFT);
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
            } else if (kind == :face) {
                pod.setFace(v);
            } else if (kind == :scoreMode) {
                pod.setScoreMode(v);
            } else if (kind == :design) {
                pod.setScoreDesign(v);
                (parentItem as WatchUi.MenuItem).setSubLabel(choiceText(kind, v));
                WatchUi.popView(WatchUi.SLIDE_IMMEDIATE);   // close the list
                showDesignPreview(v);                       // and show what it looks like
                return;
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
