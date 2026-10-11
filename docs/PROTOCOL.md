# Bluetooth interface (pod firmware 1.5)

The pod is a Bluetooth Low Energy peripheral named `PfeilZaehler3000`. It accepts up to
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
| 12 | u8 | firmware version (15 = 1.5, 95 = debug build) |
| 13 | u16 | wake-ups since start |
| 15 | u8 | share of time awake since start (%) |
| 16 | u32 | seconds since start |

Notified after every record, deletion, end mark, wake-up and power-saving switch.

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

Commands 04–07 are processed in the main loop, up to 8 can be queued.

## DATA

Each notification: `u32 index of the first record` followed by up to 2 records of 8 bytes.
After the last record the pod sends the end marker `FF FF FF FF` + `u32 total`.

Record (8 bytes):

| Offset | Type | Content |
|---|---|---|
| 0 | u32 | unix time (seconds since start if the clock was never set) |
| 4 | u16 | release jolt × 10 (g) |
| 6 | u8 | aiming time × 10 (s) |
| 7 | u8 | flags: bit0 clock set, bit1 clock restored after a restart, bit6 end mark, bit7 deleted |
