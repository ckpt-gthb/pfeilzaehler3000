// Bluetooth link to the PfeilZaehler3000 pod and all counting logic.
//
// Pod GATT service  a7c10000-5b1e-4c5a-9d1e-3b7a0c0f0ee0   (pod firmware 2.0)
//   a7c10001  TIME   write  u32 unix seconds + i32 local time offset (pod counts "today" in local time)
//   a7c10003  CTRL   write  [0x05,0,0,0,0] delete newest arrow   [0x07, u32 time] new end mark
//   a7c10005  STATS  read/notify 20 bytes: arrows u32 @0 (lifetime, without deleted), today u16 @4,
//                    week u16 @6, month u16 @8, battery mV u16 @10, last end mark u32 @12,
//                    last arrow u32 @16
//   a7c10006  SETTINGS read/write/notify (pod firmware 2.0): 8 bytes, arrows per end u8 @1,
//                    new end after u16 seconds @2, flags u8 @4 (bits 2-3 mounting point),
//                    calibration u8 @5; write [arrows][u16 seconds][u8 mounting point]
// The pod notifies STATS after every arrow, deletion or end mark (from the watch or the phone),
// so today / week / month are the same on the watch and on the phone page. SETTINGS are shared
// the same way: a change on the watch or the phone reaches the other one, the last change counts.
//
// Several pods at one range: every pod is named "PfeilZaehler3000-XXXX" and sends
// the same 16-bit ID as manufacturer data in its advertising. The watch remembers the first pod it
// connects to and from then on only connects to that one (hold UP to choose another).
//
// Connect IQ allows one outstanding BLE request at a time, so the setup runs as a chain:
//   connect -> STATS notifications on -> SETTINGS notifications on -> write TIME -> read STATS
//   -> read SETTINGS -> live (queued CTRL / SETTINGS writes).
// If the pod goes out of range, the watch stays paired and reconnects by itself; after
// RECONNECT_S without success it searches again from scratch.

import Toybox.BluetoothLowEnergy;
import Toybox.Lang;
import Toybox.WatchUi;
import Toybox.Time;
import Toybox.Time.Gregorian;
import Toybox.Attention;
import Toybox.Activity;
import Toybox.ActivityRecording;
import Toybox.FitContributor;
import Toybox.Timer;
import Toybox.Application.Storage;

const POD_NAME    = "PfeilZaehler3000";

// DEMO: 0 = normal app (always use 0 for the real watch),
// 1–4 = example screens without a pod, for screenshots
const DEMO = 0;
const RECONNECT_S = 30;      // after this long without the pod: search again

class PodLink extends BluetoothLowEnergy.BleDelegate {

    // ---- Bluetooth ----
    var svcUuid;
    var timeUuid;
    var ctrlUuid;
    var statsUuid;
    var setUuid;
    var device = null;
    var statsCh = null;
    var setCh = null;
    var timeCh = null;
    var ctrlCh = null;
    var status = :starting;      // :starting :scanning :connecting :lost :connected :error
    var scanSince = 0;
    var lostSince = 0;
    var setupStep = 0;           // 1 STATS notif., 5 SETTINGS notif., 2 time, 3 read STATS, 6 read SETTINGS, 4 live
    var busy = false;
    var queue = [];              // [characteristic id, bytes] waiting to be sent (:ctrl or :set)
    var timer = null;
    var myPod = null;            // ID of this watch's pod (null = take the first one found)
    var candId = -1;             // ID of the pod being connected

    // ---- from the pod (shared with the phone page) ----
    var podArrows = -1;          // pod's lifetime arrows, without deleted ones
    var today = -1;
    var week = -1;
    var month = -1;
    var batteryMv = 0;
    var podEndMark = 0;
    var podLastArrow = 0;        // newest "last arrow" time seen: a smaller one means a restored arrow

    // ---- settings (stored in the pod; kept on the watch too) ----
    var arrowsEnd = 6;           // arrows per end
    var endGapS = 100;           // longer pause than this = new end (walk to the target)
    var mount = 0;               // mounting point: 0 back of the riser, 1 front, 2 side bushing
    var bigNum = 0;              // big number on the screen: 0 end, 1 session, 2 today (watch only)
    var showSunset = false;      // time to sunset at the bottom (watch only)
    var langPref = 0;            // 0 automatic, 1 English, 2 German, 3 Polish (watch only)

    // ---- this session on the watch ----
    var sessionCount = 0;
    var endCount = 0;            // arrows in the current end
    var endsDone = 0;            // finished ends
    var endHist = [];            // arrows of the finished ends (for undo)
    var lastShotSec = 0;
    var forceNewEnd = false;     // "new end" was pressed on the phone

    // ---- activity recording ----
    var session = null;
    var fArrowsSession = null;
    var fEndsSession = null;
    var fArrowsRecord = null;
    var fArrowsLap = null;
    var recStartSec = 0;
    var demoRec = false;         // DEMO: recording shown without a real activity

    function initialize() {
        BleDelegate.initialize();
        svcUuid   = BluetoothLowEnergy.stringToUuid("a7c10000-5b1e-4c5a-9d1e-3b7a0c0f0ee0");
        timeUuid  = BluetoothLowEnergy.stringToUuid("a7c10001-5b1e-4c5a-9d1e-3b7a0c0f0ee0");
        ctrlUuid  = BluetoothLowEnergy.stringToUuid("a7c10003-5b1e-4c5a-9d1e-3b7a0c0f0ee0");
        statsUuid = BluetoothLowEnergy.stringToUuid("a7c10005-5b1e-4c5a-9d1e-3b7a0c0f0ee0");
        setUuid   = BluetoothLowEnergy.stringToUuid("a7c10006-5b1e-4c5a-9d1e-3b7a0c0f0ee0");
        var v = Storage.getValue("podId");
        myPod = (v instanceof Number) ? v as Number : null;
        arrowsEnd = numPref("arrowsEnd", 6, 1, 24);
        endGapS = numPref("endGapS", 100, 20, 900);
        bigNum = numPref("bigNum", 0, 0, 2);
        showSunset = Storage.getValue("sunset") == true;
        langPref = numPref("lang", 0, 0, 3);
        mount = numPref("mount", 0, 0, 2);
    }

    function numPref(key, def, lo, hi) {
        var v = Storage.getValue(key);
        if (v instanceof Number) {
            var n = v as Number;
            if (n >= lo && n <= hi) {
                return n;
            }
        }
        return def;
    }

    // ---- settings menu ----
    function setArrowsEnd(n) {
        arrowsEnd = n;
        Storage.setValue("arrowsEnd", n);
        sendSettings();
        WatchUi.requestUpdate();
    }

    function setEndGap(sec) {
        endGapS = sec;
        Storage.setValue("endGapS", sec);
        sendSettings();
        WatchUi.requestUpdate();
    }

    function setBigNum(n) {
        bigNum = n;
        Storage.setValue("bigNum", n);
        WatchUi.requestUpdate();
    }

    function setMount(m) {
        mount = m;
        Storage.setValue("mount", m);
        sendSettings();
        WatchUi.requestUpdate();
    }

    function setLangPref(n) {
        langPref = n;
        Storage.setValue("lang", n);
        WatchUi.requestUpdate();
    }

    function setShowSunset(on) {
        showSunset = on;
        Storage.setValue("sunset", on);
        WatchUi.requestUpdate();
    }

    // to the pod; the phone page takes it over from there
    function sendSettings() {
        var b = new [4]b;
        b[0] = arrowsEnd;
        b.encodeNumber(endGapS, Lang.NUMBER_FORMAT_UINT16, { :offset => 1, :endianness => Lang.ENDIAN_LITTLE });
        b[3] = mount;
        for (var i = queue.size() - 1; i >= 0; i--) {   // only the newest settings matter
            if ((queue[i] as Array)[0] == :set) {
                queue.remove(queue[i]);
            }
        }
        queue.add([:set, b]);
        sendNext();
    }

    // SETTINGS from the pod (changed on the phone or by this watch)
    function handleSettings(v) {
        if (v == null || v.size() < 4) {
            return;
        }
        var a = v[1] as Number;
        var g = v.decodeNumber(Lang.NUMBER_FORMAT_UINT16, { :offset => 2, :endianness => Lang.ENDIAN_LITTLE }).toNumber();
        if (a >= 1 && a <= 24 && g >= 20 && g <= 900) {
            arrowsEnd = a;
            endGapS = g;
            Storage.setValue("arrowsEnd", a);
            Storage.setValue("endGapS", g);
        }
        if (v.size() >= 5) {
            var m = ((v[4] as Number) >> 2) & 3;
            if (m <= 2) {
                mount = m;                       // set on the phone or found by its calibration
                Storage.setValue("mount", m);
            }
        }
    }

    // hold UP: forget the remembered pod and take the next one found
    function forgetPod() {
        if (DEMO > 0) {
            status = :scanning;                  // demo: just show the search
            scanSince = Time.now().value();
            myPod = null;
            WatchUi.requestUpdate();
            return;
        }
        Storage.deleteValue("podId");
        myPod = null;
        var d = device;
        device = null;                           // a disconnect now is wanted, not a loss
        if (d != null) {
            BluetoothLowEnergy.unpairDevice(d);
        }
        statsCh = null;
        setCh = null;
        timeCh = null;
        ctrlCh = null;
        busy = false;
        setupStep = 0;
        podArrows = -1;
        podLastArrow = 0;
        today = -1;
        week = -1;
        month = -1;
        batteryMv = 0;
        startScan();
    }

    function podLabel() {
        if (myPod != null) {
            return "Pod " + (myPod as Number).format("%04X");
        }
        return "Pod";
    }

    function start() {
        if (DEMO > 0) {
            startDemo();
            return;
        }
        BluetoothLowEnergy.setDelegate(self);
        try {
            BluetoothLowEnergy.registerProfile({
                :uuid => svcUuid,
                :characteristics => [
                    { :uuid => statsUuid, :descriptors => [ BluetoothLowEnergy.cccdUuid() ] },
                    { :uuid => setUuid, :descriptors => [ BluetoothLowEnergy.cccdUuid() ] },
                    { :uuid => timeUuid },
                    { :uuid => ctrlUuid }
                ]
            });
        } catch (e) {
            status = :error;
        }
        timer = new Timer.Timer();
        timer.start(method(:watchdog), 5000, true);
        // scanning starts in onProfileRegister()
    }

    function shutdown() {
        if (timer != null) {
            timer.stop();
            timer = null;
        }
        stopRecording(true);
        BluetoothLowEnergy.setScanState(BluetoothLowEnergy.SCAN_STATE_OFF);
        if (device != null) {
            BluetoothLowEnergy.unpairDevice(device);
            device = null;
        }
    }

    // ================= Bluetooth =================

    function onProfileRegister(uuid, st) {
        startScan();
    }

    function startScan() {
        if (status != :scanning) {
            scanSince = Time.now().value();
        }
        status = :scanning;
        BluetoothLowEnergy.setScanState(BluetoothLowEnergy.SCAN_STATE_SCANNING);
        WatchUi.requestUpdate();
    }

    // every 5 s: if the pod didn't come back by itself, forget it and search again
    function watchdog() as Void {
        var now = Time.now().value();
        if ((status == :lost || status == :connecting) && now - lostSince > RECONNECT_S) {
            if (device != null) {
                BluetoothLowEnergy.unpairDevice(device);
                device = null;
            }
            startScan();
        }
        WatchUi.requestUpdate();
    }

    function onScanResults(results) {
        if (status != :scanning) {
            return;
        }
        for (var r = results.next(); r != null; r = results.next()) {
            var sr = r as BluetoothLowEnergy.ScanResult;
            if (isPod(sr)) {
                BluetoothLowEnergy.setScanState(BluetoothLowEnergy.SCAN_STATE_OFF);
                status = :connecting;
                lostSince = Time.now().value();
                candId = podIdOf(sr);
                try {
                    device = BluetoothLowEnergy.pairDevice(sr);
                    if (device == null) {
                        startScan();
                    }
                } catch (e) {
                    startScan();
                }
                WatchUi.requestUpdate();
                return;
            }
        }
    }

    // pod ID from the advertising (manufacturer data) or from the name, -1 if unknown
    function podIdOf(r) {
        var md = r.getManufacturerSpecificData(0xFFFF);
        if (md != null && md.size() >= 2) {
            return ((md[md.size() - 2] as Number) << 8) | (md[md.size() - 1] as Number);
        }
        var name = r.getDeviceName();
        if (name != null && name.length() == POD_NAME.length() + 5 && name.find(POD_NAME + "-") == 0) {
            var hex = name.substring(POD_NAME.length() + 1, name.length());
            if (hex != null) {
                var n = hex.toNumberWithBase(16);
                if (n != null) {
                    return n;
                }
            }
        }
        return -1;
    }

    function isPod(r) {
        var ours = false;
        var name = r.getDeviceName();
        if (name != null && name.find(POD_NAME) == 0) {
            ours = true;
        } else {
            var want = svcUuid.toString();
            var it = r.getServiceUuids();
            for (var u = it.next(); u != null; u = it.next()) {
                if (u.toString().equals(want)) {
                    ours = true;
                    break;
                }
            }
        }
        if (!ours) {
            return false;
        }
        if (myPod == null) {
            return true;                         // no pod remembered yet: take this one
        }
        return podIdOf(r) == myPod;              // only this watch's pod
    }

    function onConnectedStateChanged(dev, state) {
        if (state == BluetoothLowEnergy.CONNECTION_STATE_CONNECTED) {
            device = dev;
            busy = false;
            var svc = dev.getService(svcUuid);
            if (svc == null) {
                status = :error;           // pod without the PfeilZaehler3000 service: old firmware
                WatchUi.requestUpdate();
                return;
            }
            statsCh = svc.getCharacteristic(statsUuid);
            setCh = svc.getCharacteristic(setUuid);      // null with an old pod firmware
            timeCh = svc.getCharacteristic(timeUuid);
            ctrlCh = svc.getCharacteristic(ctrlUuid);
            var cccd = (statsCh != null) ? statsCh.getDescriptor(BluetoothLowEnergy.cccdUuid()) : null;
            if (cccd != null) {
                status = :connecting;
                setupStep = 1;
                cccd.requestWrite([0x01, 0x00]b);        // step 1: notifications on
            } else {
                status = :error;
            }
        } else {
            if (device == null) {                // let go on purpose (new search running)
                WatchUi.requestUpdate();
                return;
            }
            // out of range or pod restarted: stay paired, the watch reconnects by itself
            statsCh = null;
            setCh = null;
            timeCh = null;
            ctrlCh = null;
            busy = false;
            setupStep = 0;
            status = :lost;
            lostSince = Time.now().value();
        }
        WatchUi.requestUpdate();
    }

    function onDescriptorWrite(descriptor, st) {
        // step 1b: SETTINGS notifications too
        if (setupStep == 1 && setCh != null) {
            var cccd = setCh.getDescriptor(BluetoothLowEnergy.cccdUuid());
            if (cccd != null) {
                setupStep = 5;
                cccd.requestWrite([0x01, 0x00]b);
                return;
            }
        }
        // step 2: pod clock and local time offset from the watch
        if (timeCh != null) {
            setupStep = 2;
            var b = new [8]b;
            b.encodeNumber(Time.now().value(), Lang.NUMBER_FORMAT_UINT32,
                           { :offset => 0, :endianness => Lang.ENDIAN_LITTLE });
            b.encodeNumber(localOffset(), Lang.NUMBER_FORMAT_SINT32,
                           { :offset => 4, :endianness => Lang.ENDIAN_LITTLE });
            timeCh.requestWrite(b, { :writeType => BluetoothLowEnergy.WRITE_TYPE_WITH_RESPONSE });
        } else if (statsCh != null) {
            setupStep = 3;
            statsCh.requestRead();
        }
    }

    function onCharacteristicWrite(ch, st) {
        if (setupStep == 2) {
            setupStep = 3;                       // step 3: read the counters once
            if (statsCh != null) {
                statsCh.requestRead();
            }
            return;
        }
        busy = false;                            // a queued command went through
        sendNext();
    }

    function onCharacteristicRead(ch, st, value) {
        var isSettings = setCh != null && ch.getUuid().equals(setUuid);
        if (st == BluetoothLowEnergy.STATUS_SUCCESS) {
            if (isSettings) {
                handleSettings(value);
            } else {
                handleStats(value);
            }
        }
        if (setupStep == 3 && setCh != null && !isSettings) {
            setupStep = 6;                       // step 3b: the pod's settings
            setCh.requestRead();
            return;
        }
        setupStep = 4;
        if (myPod == null && candId >= 0) {     // first pod: remember it
            myPod = candId;
            Storage.setValue("podId", candId);
        }
        status = :connected;
        busy = false;
        sendNext();
        WatchUi.requestUpdate();
    }

    function onCharacteristicChanged(ch, value) {
        if (setCh != null && ch.getUuid().equals(setUuid)) {
            handleSettings(value);
        } else {
            handleStats(value);
        }
        WatchUi.requestUpdate();
    }

    function sendNext() {
        while (!busy && setupStep == 4 && queue.size() > 0) {
            var item = queue[0] as Array;
            queue = queue.slice(1, null);
            var ch = (item[0] == :set) ? setCh : ctrlCh;
            if (ch == null) {
                continue;                        // older pod firmware: nothing to send this to
            }
            busy = true;
            try {
                (ch as BluetoothLowEnergy.Characteristic).requestWrite(item[1] as ByteArray, { :writeType => BluetoothLowEnergy.WRITE_TYPE_WITH_RESPONSE });
            } catch (e) {
                busy = false;
            }
        }
    }

    function sendCtrl(cmd, value) {
        var b = new [5]b;
        b[0] = cmd;
        b.encodeNumber(value, Lang.NUMBER_FORMAT_UINT32, { :offset => 1, :endianness => Lang.ENDIAN_LITTLE });
        queue.add([:ctrl, b]);
        sendNext();
    }

    // local time offset in seconds (incl. summer time), rounded to whole minutes
    function localOffset() {
        var now = Time.now();
        var l = Gregorian.info(now, Time.FORMAT_SHORT);
        var asUtc = Gregorian.moment({ :year => l.year, :month => l.month, :day => l.day,
                                       :hour => l.hour, :minute => l.min, :second => l.sec });
        var off = asUtc.value() - now.value();
        return ((off + 30) / 60) * 60;
    }

    // ================= counting =================

    function handleStats(v) {
        if (v == null || v.size() < 20) {
            return;
        }
        var opt = { :endianness => Lang.ENDIAN_LITTLE };
        opt[:offset] = 0;
        var arrows = v.decodeNumber(Lang.NUMBER_FORMAT_UINT32, opt).toNumber();
        opt[:offset] = 4;
        today = v.decodeNumber(Lang.NUMBER_FORMAT_UINT16, opt).toNumber();
        opt[:offset] = 6;
        week = v.decodeNumber(Lang.NUMBER_FORMAT_UINT16, opt).toNumber();
        opt[:offset] = 8;
        month = v.decodeNumber(Lang.NUMBER_FORMAT_UINT16, opt).toNumber();
        opt[:offset] = 10;
        batteryMv = v.decodeNumber(Lang.NUMBER_FORMAT_UINT16, opt).toNumber();
        opt[:offset] = 12;
        var mark = v.decodeNumber(Lang.NUMBER_FORMAT_UINT32, opt).toNumber();
        opt[:offset] = 16;
        var lastArrow = v.decodeNumber(Lang.NUMBER_FORMAT_UINT32, opt).toNumber();

        if (podArrows >= 0 && mark != podEndMark && mark > lastShotSec) {
            forceNewEnd = true;                  // "new end" on the phone (or this watch)
        }
        podEndMark = mark;

        if (podArrows < 0) {                     // first value: nothing to add
            podArrows = arrows;
            podLastArrow = lastArrow;
            return;
        }
        var n = arrows - podArrows;
        podArrows = arrows;
        // a new shot has the newest time; a restored arrow brings back an older one
        var newShot = lastArrow > podLastArrow || (Time.now().value() - lastArrow).abs() < 10;
        if (lastArrow > podLastArrow) {
            podLastArrow = lastArrow;
        }
        if (n > 0 && newShot) {
            addArrows(n);
        } else if (n > 0) {
            restoreArrows(n);                    // deleted by mistake on the phone and restored
        } else if (n < 0) {
            removeArrows(-n);                    // deleted on the phone page
        }
    }

    function addArrows(n) {
        var now = Time.now().value();
        if (endCount > 0 && (now - lastShotSec > endGapS || forceNewEnd)) {
            finishEnd();
        }
        forceNewEnd = false;
        endCount += n;
        sessionCount += n;
        lastShotSec = now;

        if (Attention has :vibrate) {
            if (endCount == arrowsEnd) {        // end complete: double buzz
                Attention.vibrate([ new Attention.VibeProfile(80, 150), new Attention.VibeProfile(0, 100),
                                    new Attention.VibeProfile(80, 150) ]);
            } else {
                Attention.vibrate([ new Attention.VibeProfile(60, 120) ]);
            }
        }
        updateFit();
    }

    // arrows restored on the phone: count them again, without a vibration
    function restoreArrows(n) {
        endCount += n;
        sessionCount += n;
        updateFit();
    }

    // arrows deleted on the phone: take them off the session and the end (newest first)
    function removeArrows(n) {
        for (var i = 0; i < n && sessionCount > 0; i++) {
            dropOne();
        }
        updateFit();
    }

    function dropOne() {
        sessionCount -= 1;
        if (endCount > 0) {
            endCount -= 1;
        } else if (endHist.size() > 0) {
            endCount = (endHist[endHist.size() - 1] as Number) - 1;
            endHist = endHist.slice(0, endHist.size() - 1);
            endsDone -= 1;
        }
    }

    function finishEnd() {
        if (isRecording() && fArrowsLap != null) {
            fArrowsLap.setData(endCount);
            session.addLap();                   // every end becomes a lap in Garmin Connect
            fArrowsLap.setData(0);
        }
        endHist.add(endCount);
        endsDone += 1;
        endCount = 0;
    }

    // DOWN button: start a new end now (also tells the pod, so the phone page sees it)
    function newEnd() {
        if (endCount > 0) {
            finishEnd();
        }
        forceNewEnd = false;
        sendCtrl(0x07, Time.now().value());
        if (Attention has :vibrate) {
            Attention.vibrate([ new Attention.VibeProfile(40, 80) ]);
        }
        updateFit();
        WatchUi.requestUpdate();
    }

    // UP button (after confirming): delete the last arrow on the pod, the phone page follows
    function undoLast() {
        if (sessionCount <= 0) {
            return;
        }
        sendCtrl(0x05, 0);
        if (podArrows > 0) {
            podArrows -= 1;                      // the pod's count will drop by one, that's expected
        }
        dropOne();
        updateFit();
        WatchUi.requestUpdate();
    }

    function secondsSinceShot() {
        if (lastShotSec == 0) {
            return -1;
        }
        return Time.now().value() - lastShotSec;
    }

    function searchingSeconds() {
        if (status != :scanning) {
            return 0;
        }
        return Time.now().value() - scanSince;
    }

    // ================= activity recording =================

    function isRecording() {
        if (DEMO > 0) {
            return demoRec;
        }
        return session != null && session.isRecording();
    }

    function toggleRecording() {
        if (DEMO > 0) {                          // demo: no real activity
            demoRec = !demoRec;
            recStartSec = Time.now().value();
            WatchUi.requestUpdate();
            return;
        }
        if (isRecording()) {
            stopRecording(true);
        } else {
            startRecording();
        }
        WatchUi.requestUpdate();
    }

    function startRecording() {
        session = ActivityRecording.createSession({
            :name => "Bogenschiessen",
            :sport => Activity.SPORT_GENERIC,
            :subSport => Activity.SUB_SPORT_GENERIC
        });
        fArrowsSession = session.createField("arrows", 0, FitContributor.DATA_TYPE_UINT16,
                                             { :mesgType => FitContributor.MESG_TYPE_SESSION, :units => "arrows" });
        fEndsSession   = session.createField("ends", 1, FitContributor.DATA_TYPE_UINT16,
                                             { :mesgType => FitContributor.MESG_TYPE_SESSION, :units => "ends" });
        fArrowsRecord  = session.createField("arrows_count", 2, FitContributor.DATA_TYPE_UINT16,
                                             { :mesgType => FitContributor.MESG_TYPE_RECORD, :units => "arrows" });
        fArrowsLap     = session.createField("end_arrows", 3, FitContributor.DATA_TYPE_UINT16,
                                             { :mesgType => FitContributor.MESG_TYPE_LAP, :units => "arrows" });
        // count from zero for this activity
        sessionCount = 0;
        endCount = 0;
        endsDone = 0;
        endHist = [];
        lastShotSec = 0;
        forceNewEnd = false;
        updateFit();
        session.start();
        recStartSec = Time.now().value();
        if (Attention has :vibrate) {
            Attention.vibrate([ new Attention.VibeProfile(80, 300) ]);
        }
    }

    function stopRecording(save) {
        if (session == null) {
            return;
        }
        updateFit();
        if (session.isRecording()) {
            session.stop();
        }
        if (save) {
            session.save();
        } else {
            session.discard();
        }
        session = null;
        fArrowsLap = null;
        if (Attention has :vibrate) {
            Attention.vibrate([ new Attention.VibeProfile(80, 300), new Attention.VibeProfile(0, 150), new Attention.VibeProfile(80, 300) ]);
        }
    }

    function updateFit() {
        if (session == null) {
            return;
        }
        var ends = endsDone + (endCount > 0 ? 1 : 0);
        fArrowsSession.setData(sessionCount);
        fEndsSession.setData(ends);
        fArrowsRecord.setData(sessionCount);
        if (fArrowsLap != null) {
            fArrowsLap.setData(endCount);
        }
    }

    function recordingSeconds() {
        if (!isRecording()) {
            return 0;
        }
        return Time.now().value() - recStartSec;
    }

    // ================= example screens (DEMO) =================
    // 1: recording, end 4 of 6, time to sunset        2: end complete 6/6, not recording
    // 3: big number = session, extra arrow 7/6, pod battery getting low, recording
    // 4: big number = today, pod out of range (reconnecting), 3 arrows per end
    // In every demo: DOWN = new end, UP = delete last arrow, START = recording on/off,
    // hold UP = settings menu, all without a pod.
    function startDemo() {
        var now = Time.now().value();
        status = :connected;
        myPod = 0xA3F2;
        batteryMv = 3980;
        arrowsEnd = 6;
        endGapS = 100;
        bigNum = 0;
        showSunset = true;
        demoRec = true;
        recStartSec = now - 38 * 60 - 12;
        endsDone = 7;
        endHist = [6, 6, 6, 6, 6, 6, 6];
        endCount = 4;
        sessionCount = 46;
        today = 46;
        week = 212;
        month = 734;
        lastShotSec = now - 42;
        if (DEMO == 2) {
            endCount = 6;
            sessionCount = 48;
            today = 48;
            week = 214;
            demoRec = false;
            showSunset = false;
            lastShotSec = now - 7;
        } else if (DEMO == 3) {
            bigNum = 1;
            endsDone = 11;
            endHist = [6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6];
            endCount = 7;
            sessionCount = 73;
            today = 73;
            week = 239;
            month = 761;
            batteryMv = 3620;
            recStartSec = now - 61 * 60 - 5;
            lastShotSec = now - 18;
        } else if (DEMO == 4) {
            bigNum = 2;
            arrowsEnd = 3;
            endsDone = 20;
            endHist = [];
            endCount = 2;
            sessionCount = 62;
            today = 108;
            week = 304;
            month = 826;
            status = :lost;
            lostSince = now;
            demoRec = false;
            lastShotSec = now - 95;
        }
    }
}
