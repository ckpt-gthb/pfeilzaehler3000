// Main screen, top to bottom: time of day, pod status, end number and time since the last arrow,
// the big number (this end, session or today – chosen in the settings) with the other counters
// below it, month, recording state, and optionally the time to sunset.

import Toybox.Graphics;
import Toybox.Lang;
import Toybox.WatchUi;
import Toybox.Timer;
import Toybox.System;
import Toybox.Time;
import Toybox.Weather;
import Toybox.Position;

class PfeilView extends WatchUi.View {

    var timer = null;
    var sunsetAt = null;         // Time.Moment of today's sunset, null = unknown
    var sunsetMsg = "";          // shown instead when there is no sunset time
    var sunsetCheck = 0;         // last calculation (unix seconds)

    function initialize() {
        View.initialize();
    }

    function onShow() {
        timer = new Timer.Timer();
        timer.start(method(:tick), 1000, true);   // refresh clocks once a second
        sunsetCheck = 0;
    }

    function onHide() {
        if (timer != null) {
            timer.stop();
            timer = null;
        }
    }

    function tick() as Void {
        WatchUi.requestUpdate();
    }

    function num(v) {
        return (v < 0) ? "-" : v.toString();
    }

    function mmss(s) {
        return (s / 60).format("%d") + ":" + (s % 60).format("%02d");
    }

    function clockText() {
        var ct = System.getClockTime();
        var hr = ct.hour;
        if (!System.getDeviceSettings().is24Hour) {
            hr = hr % 12;
            if (hr == 0) {
                hr = 12;
            }
        }
        return hr.format("%d") + ":" + ct.min.format("%02d");
    }

    // sunset from the weather position (synced from the phone), else from the last GPS position
    function updateSunset() {
        var now = Time.now().value();
        if (sunsetCheck != 0 && now - sunsetCheck < 60) {
            return;
        }
        sunsetCheck = now;
        sunsetAt = null;
        if (DEMO > 0) {                          // example screens: sunset in 1:23
            sunsetAt = new Time.Moment(now + 83 * 60);
            return;
        }
        if (!(Toybox has :Weather) || !(Weather has :getSunset)) {
            sunsetMsg = tr(:noSunset);
            return;
        }
        var loc = null;
        var cc = Weather.getCurrentConditions();
        if (cc != null) {
            loc = cc.observationLocationPosition;
        }
        if (loc == null) {
            var pi = Position.getInfo();
            if (pi != null && pi.accuracy != Position.QUALITY_NOT_AVAILABLE) {
                loc = pi.position;
            }
        }
        if (loc == null) {
            sunsetMsg = tr(:noPosition);
            return;
        }
        sunsetAt = Weather.getSunset(loc, Time.now());
        sunsetMsg = (sunsetAt == null) ? tr(:noSunset) : "";
    }

    function sunsetText() {
        updateSunset();
        if (sunsetAt == null) {
            return sunsetMsg;
        }
        var d = (sunsetAt as Time.Moment).value() - Time.now().value();
        if (d <= 0) {
            return tr(:afterSunset);
        }
        var hrs = d / 3600;
        var mins = (d % 3600) / 60;
        return trf(:sunsetIn, [hrs.format("%d") + ":" + mins.format("%02d")]);
    }

    function endColor(pod) {
        if (pod.endCount == pod.arrowsEnd) {
            return Graphics.COLOR_GREEN;
        } else if (pod.endCount > pod.arrowsEnd) {
            return Graphics.COLOR_ORANGE;
        }
        return Graphics.COLOR_WHITE;
    }

    function onUpdate(dc) {
        var w = dc.getWidth();
        var h = dc.getHeight();
        var cx = w / 2;
        var pod = getPod();

        dc.setColor(Graphics.COLOR_BLACK, Graphics.COLOR_BLACK);
        dc.clear();

        // ---- time of day (top) ----
        dc.setColor(Graphics.COLOR_WHITE, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, h * 0.045, Graphics.FONT_TINY, clockText(), Graphics.TEXT_JUSTIFY_CENTER);

        // ---- status line ----
        var statusText = tr(:starting);
        var statusColor = Graphics.COLOR_LT_GRAY;
        if (pod != null) {
            if (pod.status == :scanning) {
                statusText = (pod.searchingSeconds() > 20) ? trf(:inRange, [pod.podLabel()]) : trf(:looking, [pod.podLabel()]);
            } else if (pod.status == :connecting) {
                statusText = tr(:connecting);
            } else if (pod.status == :lost) {
                statusText = tr(:reconnecting);
                statusColor = Graphics.COLOR_ORANGE;
            } else if (pod.status == :error) {
                statusText = tr(:updateFw);
                statusColor = Graphics.COLOR_RED;
            } else if (pod.status == :connected) {
                statusText = (pod.batteryMv > 0) ? (pod.podLabel() + "  " + (pod.batteryMv / 1000.0).format("%.2f") + " V") : trf(:connected, [pod.podLabel()]);
                statusColor = Graphics.COLOR_GREEN;
                if (pod.batteryMv > 0 && pod.batteryMv < 3500) {
                    statusText = trf(:low, [pod.podLabel(), (pod.batteryMv / 1000.0).format("%.2f")]);
                    statusColor = Graphics.COLOR_RED;
                } else if (pod.batteryMv > 0 && pod.batteryMv < 3650) {
                    statusColor = Graphics.COLOR_ORANGE;
                }
            }
        }
        dc.setColor(statusColor, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, h * 0.14, Graphics.FONT_XTINY, statusText, Graphics.TEXT_JUSTIFY_CENTER);

        if (pod == null) {
            return;
        }

        // ---- end number + time since the last arrow ----
        var since = pod.secondsSinceShot();
        var endLine = tr(:endLine) + " " + (pod.endsDone + 1);
        if (since >= 0) {
            endLine += "   " + mmss(since);
        }
        dc.setColor(Graphics.COLOR_LT_GRAY, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, h * 0.21, Graphics.FONT_XTINY, endLine, Graphics.TEXT_JUSTIFY_CENTER);

        // ---- the big number, and the other counters in the row below ----
        // end / session / today: the chosen one is big, the other two go into the row (in this order),
        // the row always ends with "week"
        var vals = [pod.endCount, pod.sessionCount, pod.today];
        var names = [tr(:end), tr(:session), tr(:today)];
        var big = pod.bigNum;
        var bigColor = (big == 0) ? endColor(pod) : Graphics.COLOR_WHITE;
        dc.setColor(bigColor, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, h * 0.39, Graphics.FONT_NUMBER_THAI_HOT, num(vals[big] as Number),
                    Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
        dc.setColor(Graphics.COLOR_LT_GRAY, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, h * 0.51, Graphics.FONT_XTINY, (big == 0) ? trf(:of, [pod.arrowsEnd]) : (names[big] as String),
                    Graphics.TEXT_JUSTIFY_CENTER);

        var col = 0;
        for (var i = 0; i < 4; i++) {
            if (i == big) {
                continue;
            }
            var x = w * (0.25 + 0.25 * col);
            var value = (i == 3) ? num(pod.week) : num(vals[i] as Number);
            var label = (i == 3) ? tr(:week) : (names[i] as String);
            if (i == 0) {
                value = pod.endCount.toString() + "/" + pod.arrowsEnd.toString();
            }
            dc.setColor((i == 0) ? endColor(pod) : Graphics.COLOR_WHITE, Graphics.COLOR_TRANSPARENT);
            dc.drawText(x, h * 0.59, Graphics.FONT_SMALL, value, Graphics.TEXT_JUSTIFY_CENTER);
            dc.setColor(Graphics.COLOR_LT_GRAY, Graphics.COLOR_TRANSPARENT);
            dc.drawText(x, h * 0.68, Graphics.FONT_XTINY, label, Graphics.TEXT_JUSTIFY_CENTER);
            col += 1;
        }
        dc.drawText(cx, h * 0.76, Graphics.FONT_XTINY, trf(:month, [num(pod.month)]), Graphics.TEXT_JUSTIFY_CENTER);

        // ---- recording (bottom), time to sunset ----
        if (pod.isRecording()) {
            dc.setColor(Graphics.COLOR_RED, Graphics.COLOR_TRANSPARENT);
            dc.fillCircle(cx - 44, h * 0.865, 6);
            dc.drawText(cx - 32, h * 0.84, Graphics.FONT_XTINY, "REC " + mmss(pod.recordingSeconds()), Graphics.TEXT_JUSTIFY_LEFT);
        } else {
            dc.setColor(Graphics.COLOR_DK_GRAY, Graphics.COLOR_TRANSPARENT);
            dc.drawText(cx, h * 0.84, Graphics.FONT_XTINY, tr(:startRec), Graphics.TEXT_JUSTIFY_CENTER);
        }
        if (pod.showSunset) {
            dc.setColor(Graphics.COLOR_YELLOW, Graphics.COLOR_TRANSPARENT);
            dc.drawText(cx, h * 0.905, Graphics.FONT_XTINY, sunsetText(), Graphics.TEXT_JUSTIFY_CENTER);
        }
    }
}
