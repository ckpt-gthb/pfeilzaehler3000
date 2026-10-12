// Bluetooth link to the PfeilZaehler3000 pod and all counting logic.
//
// Pod GATT service  a7c10000-5b1e-4c5a-9d1e-3b7a0c0f0ee0   (pod firmware v1.5 or newer)
//   a7c10001  TIME   write  u32 unix seconds + i32 local time offset (pod counts "today" in local time)
//   a7c10003  CTRL   write  [0x05,0,0,0,0] delete newest arrow   [0x07, u32 time] new end mark
//   a7c10005  STATS  read/notify 20 bytes: arrows u32 @0 (lifetime, without deleted), today u16 @4,
//                    week u16 @6, month u16 @8, battery mV u16 @10, last end mark u32 @12,
//                    last arrow u32 @16
// The pod notifies STATS after every arrow, deletion or end mark (from the watch or the phone),
// so today / week / month are the same on the watch and on the phone page.
//
// Several pods at one range: every pod (firmware 1.6+) is named "PfeilZaehler3000-XXXX" and sends
// the same 16-bit ID as manufacturer data in its advertising. The watch remembers the first pod it
// connects to and from then on only connects to that one (hold UP to choose another).
//
// Connect IQ allows one outstanding BLE request at a time, so the setup runs as a chain:
//   connect -> enable STATS notifications -> write TIME -> read STATS -> live (queued CTRL writes).
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
const END_GAP_S   = 100;     // longer pause than this = new end (walk to the target)
const ARROWS_END  = 6;
const RECONNECT_S = 30;      // after this long without the pod: search again

class PodLink extends BluetoothLowEnergy.BleDelegate {

    // ---- Bluetooth ----
    var svcUuid;
    var timeUuid;
    var ctrlUuid;
    var statsUuid;
    var device = null;
    var statsCh = null;
    var timeCh = null;
    var ctrlCh = null;
    var status = :starting;      // :starting :scanning :connecting :lost :connected :error
    var scanSince = 0;
    var lostSince = 0;
    var setupStep = 0;           // 1 notifications, 2 time, 3 first read, 4 live
    var busy = false;
    var queue = [];              // CTRL commands waiting to be sent
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

    function initialize() {
        BleDelegate.initialize();
        svcUuid   = BluetoothLowEnergy.stringToUuid("a7c10000-5b1e-4c5a-9d1e-3b7a0c0f0ee0");
        timeUuid  = BluetoothLowEnergy.stringToUuid("a7c10001-5b1e-4c5a-9d1e-3b7a0c0f0ee0");
        ctrlUuid  = BluetoothLowEnergy.stringToUuid("a7c10003-5b1e-4c5a-9d1e-3b7a0c0f0ee0");
        statsUuid = BluetoothLowEnergy.stringToUuid("a7c10005-5b1e-4c5a-9d1e-3b7a0c0f0ee0");
        var v = Storage.getValue("podId");
        myPod = (v instanceof Number) ? v as Number : null;
    }

    // hold UP: forget the remembered pod and take the next one found
    function forgetPod() {
        Storage.deleteValue("podId");
        myPod = null;
        var d = device;
        device = null;                           // a disconnect now is wanted, not a loss
        if (d != null) {
            BluetoothLowEnergy.unpairDevice(d);
        }
        statsCh = null;
        timeCh = null;
        ctrlCh = null;
        busy = false;
        setupStep = 0;
        podArrows = -1;
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
        BluetoothLowEnergy.setDelegate(self);
        try {
            BluetoothLowEnergy.registerProfile({
                :uuid => svcUuid,
                :characteristics => [
                    { :uuid => statsUuid, :descriptors => [ BluetoothLowEnergy.cccdUuid() ] },
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
                status = :error;           // pod firmware older than 1.5
                WatchUi.requestUpdate();
                return;
            }
            statsCh = svc.getCharacteristic(statsUuid);
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
        if (st == BluetoothLowEnergy.STATUS_SUCCESS) {
            handleStats(value);
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
        handleStats(value);
        WatchUi.requestUpdate();
    }

    function sendNext() {
        if (busy || setupStep != 4 || ctrlCh == null || queue.size() == 0) {
            return;
        }
        var b = queue[0];
        queue = queue.slice(1, null);
        busy = true;
        try {
            ctrlCh.requestWrite(b, { :writeType => BluetoothLowEnergy.WRITE_TYPE_WITH_RESPONSE });
        } catch (e) {
            busy = false;
        }
    }

    function sendCtrl(cmd, value) {
        var b = new [5]b;
        b[0] = cmd;
        b.encodeNumber(value, Lang.NUMBER_FORMAT_UINT32, { :offset => 1, :endianness => Lang.ENDIAN_LITTLE });
        queue.add(b);
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

        if (podArrows >= 0 && mark != podEndMark && mark > lastShotSec) {
            forceNewEnd = true;                  // "new end" on the phone (or this watch)
        }
        podEndMark = mark;

        if (podArrows < 0) {                     // first value: nothing to add
            podArrows = arrows;
            return;
        }
        var n = arrows - podArrows;
        podArrows = arrows;
        if (n > 0) {
            addArrows(n);
        }
    }

    function addArrows(n) {
        var now = Time.now().value();
        if (endCount > 0 && (now - lastShotSec > END_GAP_S || forceNewEnd)) {
            finishEnd();
        }
        forceNewEnd = false;
        endCount += n;
        sessionCount += n;
        lastShotSec = now;

        if (Attention has :vibrate) {
            if (endCount == ARROWS_END) {       // end complete: double buzz
                Attention.vibrate([ new Attention.VibeProfile(80, 150), new Attention.VibeProfile(0, 100),
                                    new Attention.VibeProfile(80, 150) ]);
            } else {
                Attention.vibrate([ new Attention.VibeProfile(60, 120) ]);
            }
        }
        updateFit();
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
        sessionCount -= 1;
        if (podArrows > 0) {
            podArrows -= 1;                      // the pod's count will drop by one, that's expected
        }
        if (endCount > 0) {
            endCount -= 1;
        } else if (endHist.size() > 0) {
            endCount = (endHist[endHist.size() - 1] as Number) - 1;
            endHist = endHist.slice(0, endHist.size() - 1);
            endsDone -= 1;
        }
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
        return session != null && session.isRecording();
    }

    function toggleRecording() {
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
}
