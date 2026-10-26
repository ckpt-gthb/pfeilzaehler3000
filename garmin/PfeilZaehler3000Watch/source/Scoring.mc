// PfeilZaehler3000 (c) 2026 ckpt-gthb – free for personal, non-commercial use; PolyForm Strict 1.0.0 with permissions, see LICENSE
// Score entry for one end, and the summary shown after it.
//
// Three designs (settings, watch only):
//   0 Numpad       X 10 9 8 / 7 6 5 4 / 3 2 1 M  (3-spot faces: X 10 9 8 7 6 M)
//   1 Dial         the values around the edge of the round screen like a clock face
//   2 PiMalDaumen  tap where each arrow hit on a drawn face; the value follows from the ring
//                  (inner rings drawn wider so they can be hit with a finger – a rough guess,
//                  hence the name). The hits are sent along, so the phone page can show the group.
// Values: 11 = X (counts 10), 10…1, 0 = M. Entered in any order, kept sorted high to low.
// Side buttons always work too: UP/DOWN choose a value, START enters it (and saves a full end),
// BACK removes the last value (and leaves when nothing is entered).
// The delegates are plain InputDelegates: a BehaviorDelegate turns a tap into "select", which
// entered the value under the button focus instead of the one tapped.

import Toybox.Graphics;
import Toybox.Lang;
import Toybox.WatchUi;
import Toybox.Timer;
import Toybox.Math;
import Toybox.Attention;
import Toybox.System;

const C_GOLD  = 0xFFE552;
const C_RED   = 0xF65058;
const C_BLUE  = 0x00B4E4;
const C_BLACK = 0x1A1A1A;
const C_WHITE = 0xF2F2F2;
const C_MISS  = 0x3A3A3A;
const C_HIT   = 0x00E676;


function valText(v) {
    if (v == 11) { return "X"; }
    if (v == 0) { return "M"; }
    return v.toString();
}

function valPoints(v) {
    return (v == 11) ? 10 : v;
}

function valColor(v) {
    if (v >= 9) { return C_GOLD; }
    if (v >= 7) { return C_RED; }
    if (v >= 5) { return C_BLUE; }
    if (v >= 3) { return C_BLACK; }
    if (v >= 1) { return C_WHITE; }
    return C_MISS;
}

function valInk(v) {
    var c = valColor(v);
    return (c == C_GOLD || c == C_WHITE) ? Graphics.COLOR_BLACK : Graphics.COLOR_WHITE;
}

function sumOf(vals) {
    var s = 0;
    for (var i = 0; i < vals.size(); i++) {
        s += valPoints(vals[i] as Number);
    }
    return s;
}

// values offered for the target face (3-spot: X, 10–6, M)
function padValues(spot) {
    return spot ? [11, 10, 9, 8, 7, 6, 0] : [11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0];
}

// insert keeping high-to-low order
function insertSorted(vals, v, pos, positions) {
    var i = 0;
    while (i < vals.size() && (vals[i] as Number) >= v) {
        i++;
    }
    var out = [];
    var outPos = [];
    for (var k = 0; k < vals.size(); k++) {
        if (k == i) {
            out.add(v);
            outPos.add(pos);
        }
        out.add(vals[k]);
        outPos.add(positions[k]);
    }
    if (i >= vals.size()) {
        out.add(v);
        outPos.add(pos);
    }
    return [out, outPos];
}

// ---------------- PiMalDaumen face geometry ----------------
// Drawn ring edges as a share of the screen radius, outermost first (40 cm face: 1…10, then X).
// The real face has rings of equal width; the drawing widens the inner ones.
const PMD_FULL = [1.0, 0.913, 0.827, 0.740, 0.654, 0.567, 0.471, 0.375, 0.279, 0.183, 0.087];
// 3-spot face: rings 6…10, then X
const PMD_SPOT = [1.0, 0.82, 0.64, 0.46, 0.28, 0.12];

class ScoreView extends WatchUi.View {

    var design = 0;          // 0 numpad, 1 dial, 2 PiMalDaumen
    var n = 6;               // arrows to enter
    var endNo = 1;
    var endsTotal = 0;       // 0 = open (training)
    var spot = false;
    var vals = [];           // entered values, high to low
    var hits = [];           // PiMalDaumen: [x, y] per value (-127…127 of the face radius), or null
    var focus = 1;           // side buttons: index into the offered values
    var preview = false;     // settings: a static example
    var sessTotal = 0;       // the session's score before this end
    var sessArrows = 0;

    function initialize(d, arrows, no, ends, isSpot) {
        View.initialize();
        design = d;
        n = arrows;
        endNo = no;
        endsTotal = ends;
        spot = isSpot;
    }

    function full() {
        return vals.size() >= n;
    }

    function offered() {
        return padValues(spot);
    }

    function addValue(v, pos) {
        if (full()) {
            return;
        }
        var r = insertSorted(vals, v, pos, hits);
        vals = r[0] as Array;
        hits = r[1] as Array;
        WatchUi.requestUpdate();
    }

    function removeAt(i) {
        if (i < 0 || i >= vals.size()) {
            return;
        }
        var nv = [];
        var nh = [];
        for (var k = 0; k < vals.size(); k++) {
            if (k != i) {
                nv.add(vals[k]);
                nh.add(hits[k]);
            }
        }
        vals = nv;
        hits = nh;
        WatchUi.requestUpdate();
    }

    function headline() {
        return (endsTotal > 0) ? trf(:scoreEndOf, [endNo, endsTotal]) : trf(:scoreEnd, [endNo]);
    }

    function onUpdate(dc) {
        dc.setColor(Graphics.COLOR_BLACK, Graphics.COLOR_BLACK);
        dc.clear();
        if (dc has :setAntiAlias) {
            dc.setAntiAlias(true);
        }
        if (design == 1) {
            drawDial(dc);
        } else if (design == 2) {
            drawFace(dc);
        } else {
            drawPad(dc);
        }
    }

    // ---- shared pieces ----
    function chip(dc, x, y, w, h, v) {
        dc.setColor(valColor(v), Graphics.COLOR_TRANSPARENT);
        dc.fillRoundedRectangle(x - w / 2, y - h / 2, w, h, h / 4);
        if (v == 3 || v == 4) {
            dc.setColor(Graphics.COLOR_LT_GRAY, Graphics.COLOR_TRANSPARENT);
            dc.drawRoundedRectangle(x - w / 2, y - h / 2, w, h, h / 4);
        }
        dc.setColor(valInk(v), Graphics.COLOR_TRANSPARENT);
        dc.drawText(x, y, Graphics.FONT_SMALL, valText(v), Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
    }

    // row of entered values with empty slots; more than 6 arrows don't fit as slots on the round
    // screen: then the values as text with "entered / arrows"
    function slots(dc, cy, w) {
        if (n > 6) {
            var txt = "";
            var from = (vals.size() > 8) ? vals.size() - 8 : 0;     // the newest 8 fit
            if (from > 0) {
                txt = ".. ";
            }
            for (var k = from; k < vals.size(); k++) {
                txt += valText(vals[k] as Number) + " ";
            }
            txt += " " + vals.size().toString() + "/" + n.toString();
            dc.setColor(Graphics.COLOR_WHITE, Graphics.COLOR_TRANSPARENT);
            dc.drawText(w / 2, cy, Graphics.FONT_XTINY, txt, Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
            return;
        }
        var gap = w * 0.12;
        var sw = w * 0.105;
        var x0 = w / 2 - (n - 1) * gap / 2;
        for (var i = 0; i < n; i++) {
            var x = x0 + i * gap;
            if (i < vals.size()) {
                chip(dc, x, cy, sw, w * 0.095, vals[i] as Number);
            } else {
                dc.setColor(Graphics.COLOR_DK_GRAY, Graphics.COLOR_TRANSPARENT);
                dc.drawRoundedRectangle(x - sw / 2, cy - w * 0.0475, sw, w * 0.095, w * 0.02);
            }
        }
    }

    // ✓ / ✗ once the end is full
    function confirmButtons(dc, cy, w) {
        var bw = w * 0.30;
        var bh = w * 0.15;
        dc.setColor(Graphics.COLOR_DK_GRAY, Graphics.COLOR_TRANSPARENT);
        dc.fillRoundedRectangle(w / 2 - bw - w * 0.02, cy - bh / 2, bw, bh, bh / 3);
        dc.setColor(Graphics.COLOR_GREEN, Graphics.COLOR_TRANSPARENT);
        dc.fillRoundedRectangle(w / 2 + w * 0.02, cy - bh / 2, bw, bh, bh / 3);
        dc.setColor(Graphics.COLOR_WHITE, Graphics.COLOR_TRANSPARENT);
        dc.drawText(w / 2 - bw / 2 - w * 0.02, cy, Graphics.FONT_MEDIUM, tr(:clearShort), Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
        dc.setColor(Graphics.COLOR_BLACK, Graphics.COLOR_TRANSPARENT);
        dc.drawText(w / 2 + bw / 2 + w * 0.02, cy, Graphics.FONT_MEDIUM, tr(:saveShort), Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
    }

    // which confirm button was tapped: 1 save, -1 clear, 0 none
    function confirmHit(x, y, cy, w) {
        var bw = w * 0.30;
        var bh = w * 0.15;
        if (y < cy - bh / 2 || y > cy + bh / 2) {
            return 0;
        }
        if (x >= w / 2 + w * 0.02 && x <= w / 2 + w * 0.02 + bw) {
            return 1;
        }
        if (x <= w / 2 - w * 0.02 && x >= w / 2 - w * 0.02 - bw) {
            return -1;
        }
        return 0;
    }

    // ---- numpad ----
    function padGeom(w) {
        var vals0 = offered();
        var cols = 4;
        var rows = (vals0.size() + cols - 1) / cols;
        var kw = w * 0.19;
        var kh = w * 0.14;
        var gx = w * 0.02;
        var x0 = w / 2 - (cols * kw + (cols - 1) * gx) / 2;
        var y0 = w * 0.30;
        return [cols, rows, kw, kh, gx, x0, y0];
    }

    function drawPad(dc) {
        var w = dc.getWidth();
        dc.setColor(Graphics.COLOR_LT_GRAY, Graphics.COLOR_TRANSPARENT);
        dc.drawText(w / 2, w * 0.08, Graphics.FONT_XTINY, headline(), Graphics.TEXT_JUSTIFY_CENTER);
        slots(dc, w * 0.215, w);
        if (full()) {
            dc.setColor(Graphics.COLOR_WHITE, Graphics.COLOR_TRANSPARENT);
            dc.drawText(w / 2, w * 0.45, Graphics.FONT_NUMBER_HOT, sumOf(vals).toString(), Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
            confirmButtons(dc, w * 0.70, w);
        } else {
            var g = padGeom(w);
            var vs = offered();
            for (var i = 0; i < vs.size(); i++) {
                var c = i % (g[0] as Number);
                var r = i / (g[0] as Number);
                var x = (g[5] as Float) + c * ((g[2] as Float) + (g[4] as Float)) + (g[2] as Float) / 2;
                var y = (g[6] as Float) + r * ((g[3] as Float) + (g[4] as Float)) + (g[3] as Float) / 2;
                chip(dc, x, y, (g[2] as Float), (g[3] as Float), vs[i] as Number);
                if (i == focus) {
                    dc.setColor(Graphics.COLOR_WHITE, Graphics.COLOR_TRANSPARENT);
                    dc.setPenWidth(3);
                    dc.drawRoundedRectangle(x - (g[2] as Float) / 2 - 3, y - (g[3] as Float) / 2 - 3, (g[2] as Float) + 6, (g[3] as Float) + 6, 8);
                    dc.setPenWidth(1);
                }
            }
        }
        dc.setColor(Graphics.COLOR_LT_GRAY, Graphics.COLOR_TRANSPARENT);
        var line = trf(:endSum, [sumOf(vals)]);
        if (sessArrows + vals.size() > 0) {
            line += "  " + trf(:totalAvg, [sessTotal + sumOf(vals),
                    ((sessTotal + sumOf(vals)).toFloat() / (sessArrows + vals.size())).format("%.1f")]);
        }
        dc.drawText(w / 2, w * 0.835, Graphics.FONT_XTINY, line, Graphics.TEXT_JUSTIFY_CENTER);
    }

    function padHit(x, y, w) {
        var g = padGeom(w);
        var vs = offered();
        for (var i = 0; i < vs.size(); i++) {
            var c = i % (g[0] as Number);
            var r = i / (g[0] as Number);
            var kx = (g[5] as Float) + c * ((g[2] as Float) + (g[4] as Float));
            var ky = (g[6] as Float) + r * ((g[3] as Float) + (g[4] as Float));
            if (x >= kx && x <= kx + (g[2] as Float) && y >= ky && y <= ky + (g[3] as Float)) {
                return vs[i] as Number;
            }
        }
        return -1;
    }

    // tap on an entered value: its index, else -1 (more than 6 arrows: a tap on the row removes the newest)
    function slotHit(x, y, cy, w) {
        var gap = w * 0.12;
        var x0 = w / 2 - (n - 1) * gap / 2;
        if ((y - cy).abs() > w * 0.06) {
            return -1;
        }
        if (n > 6) {
            return vals.size() - 1;
        }
        for (var i = 0; i < vals.size(); i++) {
            if ((x - (x0 + i * gap)).abs() <= gap / 2) {
                return i;
            }
        }
        return -1;
    }

    // ---- dial ----
    function dialValues() {
        return [11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0];
    }

    function drawDial(dc) {
        var w = dc.getWidth();
        var cx = w / 2;
        var r1 = w * 0.335;
        var r2 = w * 0.495;
        var rm = (r1 + r2) / 2;
        var pen = (r2 - r1).toNumber();
        var vs = dialValues();
        for (var i = 0; i < 12; i++) {
            var v = vs[i] as Number;
            var a = 90 - i * 30;
            var usable = !spot || v >= 6 || v == 0;
            if (v == 3 || v == 4) {
                dc.setColor(Graphics.COLOR_LT_GRAY, Graphics.COLOR_TRANSPARENT);
                dc.setPenWidth(pen + 2);
                dc.drawArc(cx, cx, rm, Graphics.ARC_COUNTER_CLOCKWISE, a - 14, a + 14);
            }
            dc.setColor(usable ? valColor(v) : 0x222222, Graphics.COLOR_TRANSPARENT);
            dc.setPenWidth(pen);
            dc.drawArc(cx, cx, rm, Graphics.ARC_COUNTER_CLOCKWISE, a - 14, a + 14);
            if (i == focus) {
                dc.setColor(Graphics.COLOR_WHITE, Graphics.COLOR_TRANSPARENT);
                dc.setPenWidth(4);
                dc.drawArc(cx, cx, r1 - 4, Graphics.ARC_COUNTER_CLOCKWISE, a - 14, a + 14);
            }
            var rad = Math.toRadians(a);
            var lx = cx + rm * Math.cos(rad);
            var ly = cx - rm * Math.sin(rad);
            dc.setColor(usable ? valInk(v) : Graphics.COLOR_DK_GRAY, Graphics.COLOR_TRANSPARENT);
            dc.drawText(lx, ly, Graphics.FONT_SMALL, valText(v), Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
        }
        dc.setPenWidth(1);
        dc.setColor(Graphics.COLOR_LT_GRAY, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, w * 0.30, Graphics.FONT_XTINY, headline(), Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
        if (full()) {
            dc.setColor(Graphics.COLOR_WHITE, Graphics.COLOR_TRANSPARENT);
            dc.drawText(cx, w * 0.42, Graphics.FONT_NUMBER_MEDIUM, sumOf(vals).toString(), Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
            var bw = w * 0.15;
            var bh = w * 0.11;
            dc.setColor(Graphics.COLOR_DK_GRAY, Graphics.COLOR_TRANSPARENT);
            dc.fillRoundedRectangle(cx - bw - w * 0.015, w * 0.60 - bh / 2, bw, bh, bh / 3);
            dc.setColor(Graphics.COLOR_GREEN, Graphics.COLOR_TRANSPARENT);
            dc.fillRoundedRectangle(cx + w * 0.015, w * 0.60 - bh / 2, bw, bh, bh / 3);
            dc.setColor(Graphics.COLOR_WHITE, Graphics.COLOR_TRANSPARENT);
            dc.drawText(cx - bw / 2 - w * 0.015, w * 0.60, Graphics.FONT_SMALL, tr(:clearShort), Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
            dc.setColor(Graphics.COLOR_BLACK, Graphics.COLOR_TRANSPARENT);
            dc.drawText(cx + bw / 2 + w * 0.015, w * 0.60, Graphics.FONT_SMALL, tr(:saveShort), Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
        } else {
            dc.setColor(Graphics.COLOR_WHITE, Graphics.COLOR_TRANSPARENT);
            dc.drawText(cx, w * 0.42, Graphics.FONT_NUMBER_MEDIUM, sumOf(vals).toString(), Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
            var txt = "";
            for (var k = 0; k < vals.size(); k++) {
                txt += (k > 0 ? " " : "") + valText(vals[k] as Number);
            }
            dc.setColor(C_GOLD, Graphics.COLOR_TRANSPARENT);
            dc.drawText(cx, w * 0.56, Graphics.FONT_XTINY, txt, Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
            dc.setColor(Graphics.COLOR_LT_GRAY, Graphics.COLOR_TRANSPARENT);
            dc.drawText(cx, w * 0.64, Graphics.FONT_XTINY, vals.size().toString() + " / " + n.toString(), Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
        }
    }

    // value under a tap on the dial ring, -1 if none
    function dialHit(x, y, w) {
        var cx = w / 2;
        var dx = x - cx;
        var dy = cx - y;
        var d = Math.sqrt(dx * dx + dy * dy);
        if (d < w * 0.30 || d > w * 0.52) {
            return -1;
        }
        var a = Math.toDegrees(Math.atan2(dy, dx));       // 0 = 3 o'clock, counter-clockwise
        var i = Math.floor((90 - a) / 30.0 + 0.5).toNumber();
        i = ((i % 12) + 12) % 12;
        var v = dialValues()[i] as Number;
        if (spot && v < 6 && v != 0) {
            return -1;
        }
        return v;
    }

    // ✓ / ✗ in the middle of the dial
    function dialConfirmHit(x, y, w) {
        var cx = w / 2;
        var bw = w * 0.15;
        var bh = w * 0.11;
        if (y < w * 0.60 - bh / 2 || y > w * 0.60 + bh / 2) {
            return 0;
        }
        if (x >= cx + w * 0.015 && x <= cx + w * 0.015 + bw) {
            return 1;
        }
        if (x <= cx - w * 0.015 && x >= cx - w * 0.015 - bw) {
            return -1;
        }
        return 0;
    }

    // ---- PiMalDaumen ----
    function ringEdges() {
        return spot ? PMD_SPOT : PMD_FULL;
    }

    function ringColor(k) {       // k = 0 outermost drawn ring
        var v = spot ? 6 + k : 1 + k;
        return valColor(v);
    }

    function drawFace(dc) {
        var w = dc.getWidth();
        var cx = w / 2;
        var R = w / 2.0;
        var e = ringEdges();
        for (var k = 0; k < e.size() - 1; k++) {
            var col = ringColor(k);
            dc.setColor(col, Graphics.COLOR_TRANSPARENT);
            dc.fillCircle(cx, cx, R * (e[k] as Float));
            dc.setColor(col == C_BLACK ? Graphics.COLOR_WHITE : Graphics.COLOR_BLACK, Graphics.COLOR_TRANSPARENT);
            dc.drawCircle(cx, cx, R * (e[k] as Float));
        }
        dc.setColor(Graphics.COLOR_BLACK, Graphics.COLOR_TRANSPARENT);
        dc.drawCircle(cx, cx, R * (e[e.size() - 1] as Float));      // X ring
        for (var i = 0; i < hits.size(); i++) {
            var h = hits[i];
            if (h != null) {
                dc.setColor(Graphics.COLOR_BLACK, Graphics.COLOR_TRANSPARENT);
                dc.fillCircle(drawnX(h as Array, w), drawnY(h as Array, w), 11);
                dc.setColor(C_HIT, Graphics.COLOR_TRANSPARENT);
                dc.fillCircle(drawnX(h as Array, w), drawnY(h as Array, w), 8);
            }
        }
        // M button at the top, result at the bottom
        dc.setColor(C_MISS, Graphics.COLOR_TRANSPARENT);
        dc.fillCircle(cx, w * 0.075, w * 0.055);
        dc.setColor(Graphics.COLOR_WHITE, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, w * 0.075, Graphics.FONT_XTINY, "M", Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
        var txt = "";
        for (var k2 = 0; k2 < vals.size(); k2++) {
            txt += (k2 > 0 ? " " : "") + valText(vals[k2] as Number);
        }
        txt += (vals.size() > 0 ? " = " : "") + sumOf(vals).toString();
        if (full()) {
            confirmButtons(dc, w * 0.80, w);
        } else {
            dc.setColor(Graphics.COLOR_BLACK, Graphics.COLOR_TRANSPARENT);
            dc.fillRoundedRectangle(cx - w * 0.30, w * 0.84, w * 0.60, w * 0.10, w * 0.05);
            dc.setColor(Graphics.COLOR_WHITE, Graphics.COLOR_TRANSPARENT);
            dc.drawText(cx, w * 0.89, Graphics.FONT_XTINY, txt, Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
        }
    }

    // stored position (real face, -127…127) -> drawn screen point
    function drawnR(rReal) {
        // real ring edges: value v ring outer edge = (11 - v) / 10 of the face radius, X = 0.05
        var e = ringEdges();
        var realEdges = spot ? [1.0, 0.8, 0.6, 0.4, 0.2, 0.1] : [1.0, 0.9, 0.8, 0.7, 0.6, 0.5, 0.4, 0.3, 0.2, 0.1, 0.05];
        for (var k = 0; k < e.size() - 1; k++) {
            var ro = realEdges[k] as Float;
            var ri = realEdges[k + 1] as Float;
            if (rReal >= ri) {
                var f = (rReal - ri) / (ro - ri);
                return (e[k + 1] as Float) + f * ((e[k] as Float) - (e[k + 1] as Float));
            }
        }
        return rReal / (realEdges[e.size() - 1] as Float) * (e[e.size() - 1] as Float);
    }

    function realR(rDrawn) {
        var e = ringEdges();
        var realEdges = spot ? [1.0, 0.8, 0.6, 0.4, 0.2, 0.1] : [1.0, 0.9, 0.8, 0.7, 0.6, 0.5, 0.4, 0.3, 0.2, 0.1, 0.05];
        for (var k = 0; k < e.size() - 1; k++) {
            var ro = e[k] as Float;
            var ri = e[k + 1] as Float;
            if (rDrawn >= ri) {
                var f = (rDrawn - ri) / (ro - ri);
                return (realEdges[k + 1] as Float) + f * ((realEdges[k] as Float) - (realEdges[k + 1] as Float));
            }
        }
        return rDrawn / (e[e.size() - 1] as Float) * (realEdges[e.size() - 1] as Float);
    }

    function drawnX(h, w) {
        var x = (h[0] as Number).toFloat() / 127.0;
        var y = (h[1] as Number).toFloat() / 127.0;
        var r = Math.sqrt(x * x + y * y);
        var f = (r > 0.0001) ? drawnR(r) / r : 0.0;
        return w / 2.0 + x * f * w / 2.0;
    }

    function drawnY(h, w) {
        var x = (h[0] as Number).toFloat() / 127.0;
        var y = (h[1] as Number).toFloat() / 127.0;
        var r = Math.sqrt(x * x + y * y);
        var f = (r > 0.0001) ? drawnR(r) / r : 0.0;
        return w / 2.0 - y * f * w / 2.0;
    }

    // tap on the face: [value, [x, y]] or null
    function faceHit(x, y, w) {
        var cx = w / 2.0;
        if ((x - cx).abs() < w * 0.06 && y < w * 0.13) {
            return [0, null];                                   // M button
        }
        var dx = (x - cx) / cx;
        var dy = (cx - y) / cx;
        var d = Math.sqrt(dx * dx + dy * dy);
        if (d > 1.0) {
            return null;
        }
        var e = ringEdges();
        var v = 0;
        for (var k = 0; k < e.size(); k++) {
            if (d <= (e[k] as Float)) {
                v = spot ? 6 + k : 1 + k;
            }
        }
        if (v > 10) {
            v = 11;                                             // inside the X ring
        }
        var rr = realR(d);
        var f = (d > 0.0001) ? rr / d : 0.0;
        var px = (dx * f * 127.0).toNumber();
        var py = (dy * f * 127.0).toNumber();
        return [v, [px, py]];
    }
}

class ScoreDelegate extends WatchUi.InputDelegate {

    var view;

    function initialize(v) {
        InputDelegate.initialize();
        view = v;
    }

    function width() {
        return System.getDeviceSettings().screenWidth;
    }

    function onTap(clickEvent) {
        if (view.preview) {
            WatchUi.popView(WatchUi.SLIDE_RIGHT);     // a design preview: any tap closes it
            return true;
        }
        var xy = clickEvent.getCoordinates();
        var x = xy[0];
        var y = xy[1];
        var w = width();
        if (view.design == 1) {
            if (view.full()) {
                var c = view.dialConfirmHit(x, y, w);
                if (c == 1) { save(); } else if (c == -1) { view.vals = []; view.hits = []; WatchUi.requestUpdate(); }
                return true;
            }
            var v = view.dialHit(x, y, w);
            if (v >= 0) {
                view.addValue(v, null);
            }
            return true;
        }
        if (view.design == 2) {
            if (view.full()) {
                var c2 = view.confirmHit(x, y, w * 0.80, w);
                if (c2 == 1) { save(); } else if (c2 == -1) { view.vals = []; view.hits = []; WatchUi.requestUpdate(); }
                return true;
            }
            var hit = view.faceHit(x, y, w);
            if (hit != null) {
                view.addValue((hit as Array)[0] as Number, (hit as Array)[1]);
            }
            return true;
        }
        // numpad
        var si = view.slotHit(x, y, w * 0.215, w);
        if (si >= 0) {
            view.removeAt(si);
            return true;
        }
        if (view.full()) {
            var c3 = view.confirmHit(x, y, w * 0.70, w);
            if (c3 == 1) { save(); } else if (c3 == -1) { view.vals = []; view.hits = []; WatchUi.requestUpdate(); }
            return true;
        }
        var pv = view.padHit(x, y, w);
        if (pv >= 0) {
            view.addValue(pv, null);
        }
        return true;
    }

    function onHold(clickEvent) {
        return true;
    }

    function onSwipe(swipeEvent) {
        return true;
    }

    // side buttons: UP / DOWN choose, START enters (or saves), BACK removes or leaves
    function onKey(keyEvent) {
        var key = keyEvent.getKey();
        if (key == WatchUi.KEY_ENTER) {
            return doSelect();
        }
        if (key == WatchUi.KEY_ESC) {
            return doBack();
        }
        if (key == WatchUi.KEY_MENU) {
            return true;
        }
        var vs = (view.design == 1) ? view.dialValues() : view.offered();
        if (key == WatchUi.KEY_UP || key == WatchUi.KEY_DOWN) {
            var step = (key == WatchUi.KEY_DOWN) ? 1 : -1;
            for (var k = 0; k < vs.size(); k++) {
                view.focus = (view.focus + step + vs.size()) % vs.size();
                var v = vs[view.focus] as Number;
                if (!(view.design == 1 && view.spot && v < 6 && v != 0)) {
                    break;
                }
            }
            WatchUi.requestUpdate();
            return true;
        }
        return true;
    }

    function doSelect() {
        if (view.preview) {
            WatchUi.popView(WatchUi.SLIDE_RIGHT);
            return true;
        }
        if (view.full()) {
            save();
            return true;
        }
        var vs = (view.design == 1) ? view.dialValues() : view.offered();
        view.addValue(vs[view.focus % vs.size()] as Number, null);
        return true;
    }

    function doBack() {
        if (!view.preview && view.vals.size() > 0) {
            view.removeAt(view.vals.size() - 1);
            return true;
        }
        WatchUi.popView(WatchUi.SLIDE_DOWN);
        return true;
    }

    function save() {
        var pod = getPod();
        if (pod == null) {
            return;
        }
        var withHits = view.design == 2;
        for (var i = 0; i < view.hits.size(); i++) {
            if (view.hits[i] == null) {
                withHits = false;                 // some values came from the M button or side buttons
            }
        }
        var res = pod.saveScore(view.vals, withHits ? view.hits : null);
        if (Attention has :vibrate) {
            Attention.vibrate([ new Attention.VibeProfile(50, 100) ]);
        }
        var sv = new SummaryView(res as Array, view.endNo);
        WatchUi.switchToView(sv, new SummaryDelegate(), WatchUi.SLIDE_UP);
    }
}

// ---------------- end summary ----------------
class SummaryView extends WatchUi.View {

    var vals;
    var endNo;
    var sum;
    var maxSum;
    var avgEnd;              // average end of the session before this one, null = none
    var ends;                // end totals of the session (this one last)
    var total;
    var arrows;
    var timer = null;

    // r: [values, end totals of the session, session total, session arrows, average end before or null]
    function initialize(r, no) {
        View.initialize();
        vals = r[0] as Array;
        ends = r[1] as Array;
        total = r[2] as Number;
        arrows = r[3] as Number;
        avgEnd = r[4];
        endNo = no;
        sum = sumOf(vals);
        maxSum = vals.size() * 10;
    }

    function onShow() {
        if (DEMO == 8) {
            return;                              // example screen: stays
        }
        timer = new Timer.Timer();
        timer.start(method(:close), 6000, false);
    }

    function onHide() {
        if (timer != null) {
            timer.stop();
            timer = null;
        }
    }

    function close() as Void {
        timer = null;
        WatchUi.popView(WatchUi.SLIDE_DOWN);
    }

    function onUpdate(dc) {
        var w = dc.getWidth();
        var cx = w / 2;
        dc.setColor(Graphics.COLOR_BLACK, Graphics.COLOR_BLACK);
        dc.clear();
        if (dc has :setAntiAlias) {
            dc.setAntiAlias(true);
        }
        // arc: end score of the maximum, 300 degrees from 7:30 clockwise to 4:30
        var pen = (w * 0.045).toNumber();
        var r = w / 2 - pen;
        dc.setPenWidth(pen);
        dc.setColor(0x2A2A2A, Graphics.COLOR_TRANSPARENT);
        dc.drawArc(cx, cx, r, Graphics.ARC_CLOCKWISE, 240, -60);
        if (maxSum > 0 && sum > 0) {
            dc.setColor(C_GOLD, Graphics.COLOR_TRANSPARENT);
            var endA = 240 - 300.0 * sum / maxSum;
            dc.drawArc(cx, cx, r, Graphics.ARC_CLOCKWISE, 240, endA.toNumber());
        }
        dc.setPenWidth(1);
        dc.setColor(Graphics.COLOR_LT_GRAY, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, w * 0.20, Graphics.FONT_XTINY, trf(:scoreEnd, [endNo]), Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
        dc.setColor(C_GOLD, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, w * 0.38, Graphics.FONT_NUMBER_HOT, sum.toString(), Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
        var sub = trf(:ofMax, [maxSum]);
        if (avgEnd != null) {
            var d = sum - (avgEnd as Float);
            sub += "  " + trf(:vsAvg, [(d >= 0 ? "+" : "") + d.format("%.0f")]);
        }
        dc.setColor(Graphics.COLOR_LT_GRAY, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, w * 0.52, Graphics.FONT_XTINY, sub, Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
        // values: one row up to 8, above that two rows (up to 24 arrows)
        var k = vals.size();
        var rows = (k > 8) ? 2 : 1;
        var per = (k + rows - 1) / rows;
        var gap = (per > 6) ? w * 0.075 : w * 0.11;
        var rr = (per > 6) ? w * 0.032 : w * 0.048;
        if (per > 10) {
            gap = w * 0.068;
            rr = w * 0.029;
        }
        for (var i = 0; i < k; i++) {
            var row = i / per;
            var inRow = (row == rows - 1) ? k - per * (rows - 1) : per;
            var x0 = cx - (inRow - 1) * gap / 2;
            var px = x0 + (i % per) * gap;
            var py = (rows == 1) ? w * 0.63 : w * (0.595 + 0.07 * row);
            var v = vals[i] as Number;
            dc.setColor(valColor(v), Graphics.COLOR_TRANSPARENT);
            dc.fillCircle(px, py, rr);
            dc.setColor(valInk(v), Graphics.COLOR_TRANSPARENT);
            dc.drawText(px, py, Graphics.FONT_XTINY, valText(v), Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
        }
        // a bar per end of the session (last 10)
        var m = ends.size();
        var from = (m > 10) ? m - 10 : 0;
        var bw = w * 0.04;
        var bg = w * 0.015;
        var cnt = m - from;
        var bx0 = cx - (cnt * bw + (cnt - 1) * bg) / 2;
        var lo = 1000;
        var hi = 0;
        for (var j = from; j < m; j++) {
            var e = ends[j] as Number;
            if (e < lo) { lo = e; }
            if (e > hi) { hi = e; }
        }
        for (var j2 = from; j2 < m; j2++) {
            var e2 = ends[j2] as Number;
            var h = (hi > lo) ? w * 0.03 + (e2 - lo) * w * 0.07 / (hi - lo) : w * 0.06;
            dc.setColor(j2 == m - 1 ? C_GOLD : Graphics.COLOR_DK_GRAY, Graphics.COLOR_TRANSPARENT);
            dc.fillRoundedRectangle(bx0 + (j2 - from) * (bw + bg), w * 0.80 - h, bw, h, 3);
        }
        dc.setColor(Graphics.COLOR_LT_GRAY, Graphics.COLOR_TRANSPARENT);
        if (arrows > 0) {
            dc.drawText(cx, w * 0.86, Graphics.FONT_XTINY, trf(:totalAvg, [total, (total.toFloat() / arrows).format("%.1f")]), Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
        }
    }
}

class SummaryDelegate extends WatchUi.InputDelegate {

    var closed = false;          // one close only, whatever arrives first

    function initialize() {
        InputDelegate.initialize();
    }

    function close() {
        if (!closed) {
            closed = true;
            WatchUi.popView(WatchUi.SLIDE_DOWN);
        }
        return true;
    }

    function onTap(clickEvent) {
        return close();
    }

    function onKey(keyEvent) {
        return close();
    }

    function onSwipe(swipeEvent) {
        return true;
    }

    function onHold(clickEvent) {
        return true;
    }
}
