// Main screen: status, end number and time since the last arrow, big "arrows this end",
// session / today / week, month, and the recording state or button help at the bottom.

import Toybox.Graphics;
import Toybox.Lang;
import Toybox.WatchUi;
import Toybox.Timer;

class PfeilView extends WatchUi.View {

    var timer = null;

    function initialize() {
        View.initialize();
    }

    function onShow() {
        timer = new Timer.Timer();
        timer.start(method(:tick), 1000, true);   // refresh clocks once a second
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

    function onUpdate(dc) {
        var w = dc.getWidth();
        var h = dc.getHeight();
        var cx = w / 2;
        var pod = getPod();

        dc.setColor(Graphics.COLOR_BLACK, Graphics.COLOR_BLACK);
        dc.clear();

        // ---- status line (top) ----
        var statusText = "Starting...";
        var statusColor = Graphics.COLOR_LT_GRAY;
        if (pod != null) {
            if (pod.status == :scanning) {
                statusText = (pod.searchingSeconds() > 20) ? "Pod in range?" : "Looking for pod...";
            } else if (pod.status == :connecting) {
                statusText = "Connecting...";
            } else if (pod.status == :lost) {
                statusText = "Reconnecting...";
                statusColor = Graphics.COLOR_ORANGE;
            } else if (pod.status == :error) {
                statusText = "Pod firmware 1.5 needed";
                statusColor = Graphics.COLOR_RED;
            } else if (pod.status == :connected) {
                statusText = (pod.batteryMv > 0) ? ("Pod " + (pod.batteryMv / 1000.0).format("%.2f") + " V") : "Pod connected";
                statusColor = Graphics.COLOR_GREEN;
                if (pod.batteryMv > 0 && pod.batteryMv < 3500) {
                    statusText = "Pod battery low " + (pod.batteryMv / 1000.0).format("%.2f") + " V";
                    statusColor = Graphics.COLOR_RED;
                } else if (pod.batteryMv > 0 && pod.batteryMv < 3650) {
                    statusColor = Graphics.COLOR_ORANGE;
                }
            }
        }
        dc.setColor(statusColor, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, h * 0.10, Graphics.FONT_XTINY, statusText, Graphics.TEXT_JUSTIFY_CENTER);

        if (pod == null) {
            return;
        }

        // ---- end number + time since the last arrow ----
        var since = pod.secondsSinceShot();
        var endLine = "END " + (pod.endsDone + 1);
        if (since >= 0) {
            endLine += "   " + mmss(since);
        }
        dc.setColor(Graphics.COLOR_LT_GRAY, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, h * 0.19, Graphics.FONT_XTINY, endLine, Graphics.TEXT_JUSTIFY_CENTER);

        // ---- this end (big) ----
        var endColor = Graphics.COLOR_WHITE;
        if (pod.endCount == ARROWS_END) {
            endColor = Graphics.COLOR_GREEN;
        } else if (pod.endCount > ARROWS_END) {
            endColor = Graphics.COLOR_ORANGE;
        }
        dc.setColor(endColor, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, h * 0.39, Graphics.FONT_NUMBER_THAI_HOT, pod.endCount.toString(),
                    Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
        dc.setColor(Graphics.COLOR_LT_GRAY, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, h * 0.51, Graphics.FONT_XTINY, "of " + ARROWS_END, Graphics.TEXT_JUSTIFY_CENTER);

        // ---- session / today / week ----
        dc.setColor(Graphics.COLOR_WHITE, Graphics.COLOR_TRANSPARENT);
        dc.drawText(w * 0.25, h * 0.59, Graphics.FONT_SMALL, pod.sessionCount.toString(), Graphics.TEXT_JUSTIFY_CENTER);
        dc.drawText(w * 0.50, h * 0.59, Graphics.FONT_SMALL, num(pod.today), Graphics.TEXT_JUSTIFY_CENTER);
        dc.drawText(w * 0.75, h * 0.59, Graphics.FONT_SMALL, num(pod.week), Graphics.TEXT_JUSTIFY_CENTER);
        dc.setColor(Graphics.COLOR_LT_GRAY, Graphics.COLOR_TRANSPARENT);
        dc.drawText(w * 0.25, h * 0.68, Graphics.FONT_XTINY, "session", Graphics.TEXT_JUSTIFY_CENTER);
        dc.drawText(w * 0.50, h * 0.68, Graphics.FONT_XTINY, "today", Graphics.TEXT_JUSTIFY_CENTER);
        dc.drawText(w * 0.75, h * 0.68, Graphics.FONT_XTINY, "week", Graphics.TEXT_JUSTIFY_CENTER);
        dc.drawText(cx, h * 0.76, Graphics.FONT_XTINY, "month " + num(pod.month), Graphics.TEXT_JUSTIFY_CENTER);

        // ---- recording / help (bottom) ----
        if (pod.isRecording()) {
            dc.setColor(Graphics.COLOR_RED, Graphics.COLOR_TRANSPARENT);
            dc.fillCircle(cx - 44, h * 0.875, 6);
            dc.drawText(cx - 32, h * 0.85, Graphics.FONT_XTINY, "REC " + mmss(pod.recordingSeconds()), Graphics.TEXT_JUSTIFY_LEFT);
        } else {
            dc.setColor(Graphics.COLOR_DK_GRAY, Graphics.COLOR_TRANSPARENT);
            dc.drawText(cx, h * 0.85, Graphics.FONT_XTINY, "START rec  DOWN end  UP undo", Graphics.TEXT_JUSTIFY_CENTER);
        }
    }
}
