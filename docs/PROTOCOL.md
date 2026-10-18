# Bluetooth interface (pod firmware 2.0 to 2.3)

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
| `a7c10006-…` | SETTINGS | read, write, notify | 11 bytes (10 up to 2.1), see below |
| `a7c10007-…` | DETAIL | notify | shot details in the layout of 2.0/2.1, sent with DATA |
| `a7c10008-…` | DETAIL2 | notify | (2.2) shot details with finer angles and the bow push, sent with DATA |

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
| 11 | u8 | flags: bit0 clock set since start, bit1 awake, bit2 (2.2) bow resting: turning slower than 1.5 °/s for 4 s, at least 4 s after the last arrow (e.g. on the stand), bit3 (2.3) the pod has owners, bit4 (2.3) on USB power (owners can be added) |
| 12 | u8 | firmware version (23 = 2.3, 123 = debug build of 2.3; 22 / 122 = 2.2; 21 / 121 = 2.1; 20 / 120 = 2.0) |
| 13 | u8 | (2.3) 1 = this connection is logged in as an owner; only in notifications, a read gives 0 |
| 14 | u8 | (2.3) number of owners (0–4) |
| 15 | 5 bytes | reserved, 0 |

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
| `0E` 00 00 00 00 | (2.2) Calibration step 3: the bow is tilted far to the side; the pod works out how the ADXL375 sits relative to the other sensor (needed for the bow push) |
| `0B` u32 start, 3 bytes values, u8 part — or u32 start, 3 bytes values, 12 bytes positions | (2.2) End score, see below |
| `10` 8-byte key | (2.3) Log in as an owner |
| `11` 8-byte key, u8 mode | (2.3) Become an owner; only while the pod is on USB power. Mode 1 = remove all other owners first |
| `12` 8-byte key | (2.3) Keep only this owner (logged in only) |
| `13` 00 00 00 00 | (2.3) Remove all owners: the pod is open again (logged in only) |

Commands 04–07, 09–0B and 0E are processed in the main loop, up to 8 can be queued (20 bytes each).

**End score (`0B`).** `start` is the time of the first arrow of the end. The values are 6 nibbles,
low nibble first: 0 = miss, 1–10, 11 = X (counts 10), 15 = no arrow. Ends of more than 6 arrows
are sent in several writes with `part` 0–3. A 20-byte write (ends of up to 6 arrows) has no part
byte; the 12 position bytes follow the values instead: x and y of each hit as i8, −127…127 of the face radius, + = right / up.
The pod stores the score as a record with flag bit5 (see DATA) and the positions as two more
records with flag bit4. Saving a score again for the same end replaces it on the phone page and
the watch (the newest record counts).

## SETTINGS (11 bytes; 10 up to 2.1)

| Offset | Type | Content |
|---|---|---|
| 0 | u8 | 0x5E |
| 1 | u8 | arrows per end (1–24, default 6) |
| 2 | u16 | new end after a pause of this many seconds (20–900, default 100) |
| 4 | u8 | flags: bit0 cant 0 set by the calibration, bit1 cant 0 taken from the first shot, bits 2–3 mounting point (0 back of the riser, 1 front, 2 side bushing) |
| 5 | u8 | calibration: 0 not done, 1 step 1 done, 2 steps 1 and 2 done, 3 failed (bow not tilted far enough), 4 (2.2) all three steps done, 5 (2.2) step 3 failed (bow not tilted far enough to the side) |
| 6 | i16 | roll angle of the pod at cant 0 (0.1°) |
| 8 | i16 | angle of the pod's Z axis with the arrow level, from the calibration (0.1°): tilt of the bushing |
| 10 | u8 | (2.2) target face for scoring: 0 full face, 1 3-spot |

Write `[arrows per end][u16 seconds]`, optionally `[u8 mounting point 0–2]` (4 bytes) and
`[u8 target face 0–1]` (5 bytes) to change the settings; 0xFF or a shorter write leaves the
mounting point or the target face unchanged. They are stored in flash and
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
| 7 | u8 | flags: bit0 clock set, bit1 clock restored after a restart, bit4 hit positions (2.2), bit5 end score (2.2), bit6 end mark, bit7 deleted |

End score record (bit5): offset 0 = start of the end, offsets 4–6 = the 6 value nibbles of command
`0B`, bits 0–1 of the flags = part. Hit positions (bit4): two records right after the score, 7
position bytes each (offsets 0–6), bit0 of the flags = first or second half.

## DETAIL (2.0 layout)

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

From 2.2 on, DETAIL carries the same layout for older phone pages: angles rounded to 0.5°, no bow
push.

## DETAIL2 (2.2)

If notifications on DETAIL2 are switched on, the pod sends one notification per record of every
DATA packet: `u32 index` followed by 12 bytes. All zero for end marks, end scores, hit positions
and records of older firmware.

| Offset | Type | Content |
|---|---|---|
| 0 | u8 | raise × 10 (s), 0 = not found |
| 1 | u8 | draw and set × 10 (s), 0 = not found |
| 2 | u8 | hold × 10 (s), 0 = not found |
| 3 | u8 | aim movement in the last second, RMS × 100 (°), 0 = not measured |
| 4 | i16 | bow angle at the release × 10 (°), + = aiming up, −32768 = not measured |
| 6 | i16 | cant at the release × 10 (°), + = top limb to the right, −32768 = not measured |
| 8 | i8 | follow-through 0.5 s after the release, forward / down (°), −128 = not measured |
| 9 | i8 | follow-through sideways, + = right (°), −128 = not measured |
| 10 | i8 | bow push in the first 0.1 s after the release, + = down (mm), −128 = not measured |
| 11 | i8 | bow push sideways, + = right (mm), −128 = not measured |

The bow push needs the pod on the back or the front of the riser and all three calibration steps
(calibration status 4). The details of records stored by 2.0 or 2.1 are taken over once when 2.2
starts (angles in 0.5°, no bow push).

## Shot check (2.2)

A jolt counts as an arrow only if the bow was still for 3 to 20 s right before it and the jolt
reached at least 25 g. Shorter stillness (bow being assembled or set down), longer stillness (pod
lying on a table) and weaker bumps are not stored.

## Owner lock (2.3)

A pod without owners works as before: every connection may write and download. A pod with owners
accepts writes to CTRL, SETTINGS and TIME, and the download (`01`), only from a connection that
logged in with `10` and an owner's key; reads and notifications of INFO and STATS stay open, so
others still see the counts. Each phone or watch makes its own random 8-byte key and logs in right
after connecting, before it writes the clock. The pod answers every `10`–`13` with an INFO
notification whose byte 13 tells each connection whether it is logged in. Owners are added with
`11` only while VBUS is present (INFO bit 4); up to 4 keys are kept, a fifth replaces the oldest.
The keys are stored in flash (`/owners.bin`).
