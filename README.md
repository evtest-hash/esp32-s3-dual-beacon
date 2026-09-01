# ESP32-S3 Dual Beacon Firmware

Makes a MuseLab ESP32-S3 Dongle broadcast two static beacons at once:

- **WiFi side**: a scannable SSID (`S3-BEACON-XXXXXX`, suffix derived from the chip's MAC)
- **BLE side**: a non-connectable iBeacon (UUID / Major / Minor), answering to the same name as the SSID

Both run continuously with static content. The AP offers no services and DHCP is disabled -- its only purpose is to be visible to a scan.

The BLE side is scannable but not connectable (`ADV_SCAN_IND`): it answers a scanner's request with its name, so it appears in a generic tool as `S3-BEACON-XXXXXX` rather than an anonymous MAC. Passive scanners, which is what iBeacon consumers are, see exactly the same advertisement either way. The name lives in the scan response because the advertisement is full -- 30 of its 31 bytes are the Flags and the iBeacon payload.

The onboard blue LED blinks at 1 Hz while the firmware is running. It is a liveness indicator only and carries no fault information; a boot loop shows up as a repeatedly interrupted blink.

> **Verified on hardware.** Both beacons have been observed transmitting from a MuseLab ESP32-S3 Dongle, and each radio's transmit power has been measured against the value it was commanded. That measurement found a bug this firmware had shipped for three releases; see [Hardware verification status](#hardware-verification-status) for what is and is not established.

## Layout

| | |
|---|---|
| [`hardware/`](hardware/) | Pin assignments, component values and flashing notes for the board, transcribed from the vendor schematic |
| [`firmware/`](firmware/) | The ESP-IDF project: sources, config and host unit tests |
| [`web/`](web/) | The browser installer page (English and Chinese), published to GitHub Pages on each tagged release |

## Changing parameters

Every tunable value lives in a single file, **`firmware/main/beacon_config.h`**: SSID prefix, AP password, channel, the iBeacon UUID and reference transmit power, advertising interval, heartbeat period. Edit, rebuild, and reflash.

`SSID_PREFIX`, `AP_PASSWORD`, `AP_CHANNEL`, `WIFI_BEACON_INTERVAL_TU`, `BLE_ADV_INTERVAL_MS` and `WIFI_MAX_TX_POWER_QDBM` are protected by compile-time asserts -- an out-of-range value fails the build with an explanatory message instead of silently producing a broken firmware image.

### Transmit power

`BLE_TX_POWER_LEVEL` and `WIFI_MAX_TX_POWER_QDBM` set the two radios' transmit power; both default to the maximum the hardware accepts. The firmware logs what each driver reports back, but **only the WiFi read-back is worth believing**.

`esp_ble_tx_power_get()` reports the level the last advertising instance came up at, not the level just requested. Called before `ble_gap_adv_start()` it returns the previous value, and it will happily log a level the radio is not using. That is exactly how this firmware advertised at the +9 dBm controller default for three releases while logging `controller reports 20 dBm`. Only a measured RSSI delta settles what a radio is actually doing.

`IBEACON_TX_POWER` is **not** a transmit power. It is the iBeacon "Measured Power" field -- the RSSI a scanner should see at 1 m -- and scanners divide by it to estimate distance. The shipped `-51` is measured on this board: the median of 239 advertisements received at 1 m, line of sight, on a USB charger clear of metal. **Changing `BLE_TX_POWER_LEVEL`, or reworking the antenna matching network, invalidates it** -- re-measure at 1 m and write the new value back.

Note: the AP is an **open network** (`AP_PASSWORD` is `""`). This is intentional, not a placeholder -- see the comment in `firmware/main/beacon_config.h` for why (the AP offers no services worth protecting, and a cosmetic password would only be misleading in a public repo).

## Local build

```bash
cd firmware
. $IDF_PATH/export.sh
idf.py build
```

Flash and watch the serial log (hold the onboard BOOT button while plugging in the Dongle):

```bash
idf.py -p /dev/cu.usbmodem* flash monitor
```

This project was built and tested against ESP-IDF v5.5.5.

Host unit tests (no ESP-IDF required, runs in about a second):

```bash
./firmware/test/run.sh
```

## How device identity is derived

The SSID suffix and the iBeacon's Major/Minor all come from the last three bytes of the chip's softAP MAC, one-to-one. See [`hardware/`](hardware/) for the pin and part details this depends on.

**Note**: BLE advertising uses the chip's Bluetooth MAC (base MAC + 2), while Major/Minor are derived from the softAP MAC (base MAC + 1). So the device address you see in a tool like nRF Connect will **not** match Major/Minor numerically -- that's expected, not a bug.

## Flashing from the browser

The quickest path, and the one that needs nothing installed:

**<https://evtest-hash.github.io/esp32-s3-dual-beacon/>**

Plug in the Dongle, click *Connect and install*, pick the port. The page flashes the binaries
from the most recent tagged release.

The page follows your browser's language, with an EN / 中文 switch in the header. The install
dialog itself comes from ESP Web Tools and is English only.

This needs a browser that implements Web Serial -- Chrome, Edge, Opera, or Firefox 151+, on a
desktop. Safari and every browser on iOS are out; use the esptool path below.

Two things worth knowing about this board specifically. It has no USB-to-UART bridge, so flashing
goes through the chip's built-in USB Serial/JTAG: the port shows up as `usbmodem`, not `usbserial`.
And if the chip was put into download mode by holding BOOT, it can stay there after a successful
install, because the reset the tooling can issue over USB Serial/JTAG does not re-sample the boot
strapping pin. Unplugging and replugging once is the fix.

## Flashing from a release

This path needs `esptool` on your PATH; if you have not installed ESP-IDF, `pip install esptool` is
enough.

1. Download `bootloader.bin`, `partition-table.bin` and `dual_beacon.bin` from the
   [latest release](https://github.com/evtest-hash/esp32-s3-dual-beacon/releases)
2. Hold the onboard BOOT button while plugging in the Dongle, then confirm the port: `ls /dev/cu.usbmodem*`
3. Flash it (replace `<PORT>` with the actual device):

```bash
esptool.py --chip esp32s3 -p <PORT> -b 460800 \
  --before default_reset --after hard_reset write_flash \
  --flash_mode dio --flash_size 8MB --flash_freq 80m \
  0x0     bootloader.bin \
  0x8000  partition-table.bin \
  0x10000 dual_beacon.bin
```

The three offsets are fixed by the partition table and must not be changed.

For an untagged build, the `build` workflow produces the same three files as a run artifact. It is
manual-dispatch only, so a run has to have been started from the Actions page for one to exist.

## Releasing

Tagging is what publishes. Pushing a `v*` tag runs `release.yml`, which builds the firmware, creates
a GitHub release with the three binaries attached, and deploys the installer page pointing at them:

```bash
git tag v1.0.0 && git push origin v1.0.0
```

`build.yml` builds the same firmware on demand and publishes nothing.

## License

MIT -- see [LICENSE](LICENSE).

## Hardware verification status

Run on a MuseLab ESP32-S3 Dongle (ESP32-S3FN8, chip rev v0.2) under ESP-IDF v5.5.5.

**Verified**

- Both beacons transmit. The softAP is scannable as `S3-BEACON-XXXXXX` and the iBeacon is received with the expected UUID, Major and Minor.
- The scan response carries the name, so the device shows up as `S3-BEACON-XXXXXX` rather than an anonymous MAC.
- WiFi and BLE coexist without either dropping out; advertisement and beacon rates were unchanged between a USB-host supply and a USB charger.
- The blue LED blinks at 1 Hz.
- **WiFi transmits at the power it is asked for.** `esp_wifi_get_max_tx_power()` reports 80 quarter-dBm, and a sweep of the whole ladder from +2 to +20 dBm moved the received RSSI by 20 dB, slope 1.07, with no flattening at the top.
- **BLE now does too.** The same sweep from 0 to +20 dBm moved RSSI by 22 dB, slope 1.10. Before the fix in v0.4.0 the advertisement went out at the +9 dBm controller default: median RSSI -41 dBm (n=67), against -29 dBm (n=79) after, board and receiver fixed in place.

**Known limitation, and it is the board, not the firmware**

At 1 m line of sight the board is received about 20 dB weaker than a commercial 20 dBm access point several metres away. Since the power ladder stays linear to the top of its range, the loss is entirely after the power amplifier -- in the antenna matching network, which [`hardware/`](hardware/) shows does not implement the network Espressif's ESP32-S3 design guidelines call for. No firmware change reaches it.

**Not verified**

Long-term stability beyond a few hours, behaviour across a batch of boards, and anything about range in a deployment. RSSI here was read with a MacBook, which is a consumer receiver: the relative measurements above are solid, the absolute dBm figures carry the usual few dB of uncalibrated offset.
