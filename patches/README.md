# Submodule patches

`src/lib/*` are git submodules, so anything edited in place is lost the next time
`make init` runs (`git submodule update --recursive`, plus the pinned pico-sdk and
TinyUSB checkouts). Patches live here instead and `make init` applies them; run
`patches/apply.sh` by hand after any other submodule update.

## 0001 — hids_client: 16-bit report map index

BTstack copies the HID report map out of
`GATT_EVENT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT` with a `uint8_t` loop index,
while the event's `value_length` is a `uint16_t`:

```c
uint8_t i;
...
for (i = 0; i < value_len; i++){
    hids_client_descriptor_storage_store(client, client->service_index, value[i]);
}
```

A report map of 256 bytes or more delivered in a single chunk makes `i` wrap at
255, so the first 256 bytes are stored over and over until the descriptor storage
fills. The client then reports `hid_descriptor_len == storage size` (1024 here)
holding four copies of the first 256 bytes, and the tail of the real descriptor
is never stored at all.

Two conditions have to line up, which is why this hid for so long:

- **A report map of at least 256 bytes.** Of our BLE output modes only Xbox
  qualifies (`xbox_hid_descriptor` is 283 bytes; the others are 129 and 146).
- **An ATT MTU large enough to deliver it in one chunk.** With the default 23
  the map arrives in 20-byte pieces and each loop is far below the wrap. This
  repo negotiates up to 512 (`JP_ATT_MTU_CEILING`, needed for Switch 2 output),
  so the whole map lands in one event.

Downstream the damage is not subtle: the duplicated descriptor parses into
hundreds of report items, which used to exhaust the HID parser's item pool and
reset the board (fixed separately so the parse fails cleanly instead), and no
usable report map means a connected controller sends input that maps to nothing.

Worth reporting upstream — the fix is the index type, nothing else.
