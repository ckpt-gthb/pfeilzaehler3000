# Bluetooth interface (pod firmware 2.0)

The pod is a Bluetooth Low Energy peripheral named `PfeilZaehler3000-XXXX`, where `XXXX` is the
pod ID: the two lowest bytes of its Bluetooth address in hex (firmware 1.5 and older: just
`PfeilZaehler3000`). The name is in the scan response. The advertising packet contains the
service UUID and manufacturer data `FF FF <ID high byte> <ID low byte>` (company ID 0xFFFF), so the
ID can be read without a scan response. It accepts up to
3 connections at the same time and keeps advertising while a connection slot is free. No pairing
or bonding is required. All numbers are little-endian.

Service UUID: `a7c10000-5b1e-4c5a-9d1e-3b7a0c0f0ee0`
Characteristic UUIDs: same, with the 8th hex digit replaced (`a7c1000N-…`).

| UUID | Name | Properties | Content |
|---|---|---|---|
| `a7c10001-…` | TIME | read, write | u32 unix time [+ i32 local time offset in seconds] |
| `a7c10002-…` | INFO | read, notify | 20 bytes, see below |
| `a7c10003-…` | CTRL | write | commands, see below |
| `a7c10004-…` | DATA | notify | records sent after command 0x01 |
| `a7c10005-…` | STATS | read, notify | 20 bytes, see below |
| `a7c10006-…` | SETTINGS | read, write, notify | 10 bytes, see below |
| `a7c10007-…` | DETAIL | notify | shot details, sent with DATA |

## TIME

Write 4 bytes (unix time) or 8 bytes (unix time + local offset including summer time, e.g. 7200
for CEST). The offset is needed for the pod's today/week/month counters and is stored in flash.
The phone page and the watch write it on every connection.

## INFO (20 bytes)

| Offset | Type | Content |
|---|---|---|
| 0 | u32 | total records ever stored (index of the next record) |
| 4 | u32 | index of the oldest record still in the pod |
| 8 | u16 | battery voltage (mV) |
| 10 | u8 | memory used (%) |
| 11 | u8 | flags: bit0 clock set since start, bit1 awake |
| 12 | u8 | firmware version (20 = 2.0, 120 = debug build of 2.0) |
| 13 | 7 bytes | reserved, 0 |

Memory used counts records and shot details together. Notified after every record, deletion, end
mark, wake-up and power-saving switch.

## STATS (20 bytes)

| Offset | Type | Content |
|---|---|---|
| 0 | u32 | arrows ever counted, without deleted ones |
| 4 | u16 | arrows today (local time) |
| 6 | u16 | arrows this week (from Monday) |
| 8 | u16 | arrows this month |
| 10 | u16 | battery voltage (mV) |
| 12 | u32 | time of the last end mark (unix) |
| 16 | u32 | time of the last arrow (unix) |

Notified together with INFO.

## CTRL commands

| Bytes | Command |
|---|---|
| `01` u32 from | Send all records from global index `from` on DATA |
| `02` u32 total | Clear the pod's record memory, only if `total` equals the current total |
| `04` u32 from, u16 count, u8 del | Mark records as deleted (`del` = 1) or restore them (0) |
| `05` 00 00 00 00 | Delete the newest arrow that is not deleted |
| `06` 00 00 00 00 | Reset the today/week/month counters |
| `07` u32 time | Store an end mark ("new end") with this time |
| `09` 00 00 00 00 | Calibration step 1: the bow is upright, the arrow level; roll angle = cant 0, Z angle = bow angle 0 |
| `0A` 00 00 00 00 | Calibration step 2: the arrow points at the ground; the pod finds its mounting point |

Commands 04–07, 09 and 0A are processed in the main loop, up to 8 can be queued.

## SETTINGS (10 bytes)

| Offset | Type | Content |
|---|---|---|
| 0 | u8 | 0x5E |
| 1 | u8 | arrows per end (1–24, default 6) |
| 2 | u16 | new end after a pause of this many seconds (20–900, default 100) |
| 4 | u8 | flags: bit0 cant 0 set by the calibration, bit1 cant 0 taken from the first shot, bits 2–3 mounting point (0 back of the riser, 1 front, 2 side bushing) |
| 5 | u8 | calibration: 0 not done, 1 step 1 done, 2 done, 3 failed (bow not tilted far enough) |
| 6 | i16 | roll angle of the pod at cant 0 (0.1°) |
| 8 | i16 | angle of the pod's Z axis with the arrow level, from the calibration (0.1°): tilt of the bushing |

Write `[arrows per end][u16 seconds]` and optionally `[u8 mounting point 0–2]` (4 bytes) to
change the settings; with 3 bytes the mounting point stays unchanged. They are stored in flash and
notified to every connected device, so the phone page and the watch always use the same values;
the last change counts.

## DATA

Each notification: `u32 index of the first record` followed by up to 2 records of 8 bytes.
After the last record the pod sends the end marker `FF FF FF FF` + `u32 total`.

Record (8 bytes):

| Offset | Type | Content |
|---|---|---|
| 0 | u32 | unix time (seconds since start if the clock was never set) |
| 4 | u16 | release jolt × 10 (g) |
| 6 | u8 | still time before the release × 10 (s): rotation below 25 °/s |
| 7 | u8 | flags: bit0 clock set, bit1 clock restored after a restart, bit6 end mark, bit7 deleted |

## DETAIL

If notifications on DETAIL are switched on, the pod sends a DETAIL notification right after every
DATA notification of a dump: `u32 index of the first record` followed by one 8-byte detail per
record of the DATA packet. End marks and records stored by older firmware have all-zero details.

| Offset | Type | Content |
|---|---|---|
| 0 | u8 | raise × 10 (s), 0 = not found |
| 1 | u8 | draw and set × 10 (s), 0 = not found |
| 2 | u8 | hold × 10 (s), 0 = not found |
| 3 | u8 | aim movement in the last second, RMS × 100 (°), 0 = not measured |
| 4 | i8 | bow angle at the release × 2 (°), + = aiming up |
| 5 | i8 | cant at the release × 2 (°), + = top limb to the right |
| 6 | i8 | follow-through 0.5 s after the release, forward / down (°) |
| 7 | i8 | follow-through sideways, + = right (°) |

Times above 25.4 s are stored as 255. Bow angle, cant and follow-through need the pod on the back
or the front of the riser (sensor Z axis roughly along the arrow) and the mounting point set in SETTINGS;
with the pod in a side bushing they are -128 (not measured).
