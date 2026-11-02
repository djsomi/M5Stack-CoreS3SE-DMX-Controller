# Preset Backup: Export and Import

The **Backup** card in web Presets transfers the saved P1–P8 collection as JSON.
It contains presets only: no device settings, Wi-Fi credentials or raw NVS data.
It works with Unit and Base profiles and never implicitly loads live DMX output.

## Export

Choose Export and keep the tab open and active until the progress dialog finishes.
The file uses the browser's local time: `DMX-Presets-YY-MM-DD-HH-mm.json`.
One coherent authoritative stored collection is captured. Empty/deleted slots are
omitted; exporting an empty collection is supported. An unreadable/corrupt occupied
slot fails the export rather than silently omitting data. Export does not mutate
live values, blackout, presets or their revisions.

## Import

Choose Import, select a JSON file and review the custom confirmation:

> This will replace the entire saved P1–P8 preset collection, including empty slots.
> Live output will remain unchanged.

Import / Cancel expresses the final choice; selecting a file alone writes nothing.
An empty collection intentionally clears all logical slots after confirmation.
Validation occurs in the browser and again in firmware. Duplicate transfers are
blocked. The animated progress dialog stays open through the backend result; it
shows no artificial percentage. Success refreshes authoritative occupancy for
all clients. Meaningful validation, conflict, persistence and connection errors
remain visible.

**Keep this browser tab open and active until the operation completes.** Moving
to another tab/backgrounding suspends the WebSocket and cancels the transfer.
Returning reconnects and obtains fresh state; writes are never automatically
replayed. If the response is lost near atomic selection, the Import may already
have committed. Inspect/export the current collection before deciding to retry.

## File format

The format is unchanged version 1. Empty example:

```json
{"format":"cores3se-dmx-presets","version":1,"presets":[]}
```

An occupied entry has exactly `slot`, `name`, `values`: slot 1–8; name P1–P8
matching the slot; exactly 512 unsigned integer values 0–255 in channel order.
Start code is implicit zero. Empty slots have no entry. Arbitrary preset names,
duplicate keys/slots, unknown fields, wrong lengths/ranges, unsupported versions,
malformed JSON and files above 32,768 bytes are rejected. JSON whitespace/key
order and ASCII escapes are supported; an initial UTF-8 BOM is accepted and
invalid UTF-8 is rejected. No hardware pins/profile identifiers are serialized.

## Persistence and safety

Every Save/Delete/Copy or Import uses current-format CRC-checked generation banks
and atomic selection. Only the inactive bank is reused. A failed or cancelled
publication leaves the selected collection recoverable; uncertain outcomes are
not claimed as success and must not be blindly replayed. Storage-full is reported
explicitly while selected data is retained. There is no automatic NVS erase.

Old v0.2 raw preset/tombstone migration is removed. From v0.2 use the destructive
factory installation; for the tested current-format v0.3 app update, do not erase
or write the complete factory image. See [installation](V0_3_0_INSTALLATION.md).

Host regression tests use actual ESP-IDF NVS with RAM-backed NOR flash, including
interruption/error injection, capacity and reboot recovery. They do not certify
physical brownouts or RS-485 electrical timing. See [testing](TESTING.md).
