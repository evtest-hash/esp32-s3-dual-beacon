# ESP32-S3 Dual Beacon Firmware

Makes a MuseLab ESP32-S3 Dongle broadcast two static beacons at once:

- **WiFi side**: a scannable SSID (`S3-BEACON-XXXXXX`, suffix derived from the chip's MAC)
- **BLE side**: a non-connectable iBeacon (UUID / Major / Minor)

Both run continuously, with static content, transmit-only. The AP offers no services and DHCP is disabled -- its only purpose is to be visible to a scan.

The onboard blue LED blinks at 1 Hz while the firmware is running. It is a liveness indicator only and carries no fault information; a boot loop shows up as a repeatedly interrupted blink.

> **This firmware has never run on real hardware.** The host unit tests pass and it compiles under ESP-IDF v5.5.5, but no radio behavior has ever been observed on an actual chip. See [Hardware verification status](#hardware-verification-status).

## Layout

| | |
|---|---|
| [`hardware/`](hardware/) | Pin assignments, component values and flashing notes for the board, transcribed from the vendor schematic |
| [`firmware/`](firmware/) | The ESP-IDF project: sources, config and host unit tests |

## Changing parameters

Every tunable value lives in a single file, **`firmware/main/beacon_config.h`**: SSID prefix, AP password, channel, the iBeacon UUID and reference transmit power, advertising interval, heartbeat period. Edit, rebuild, and reflash.

`SSID_PREFIX`, `AP_PASSWORD`, `AP_CHANNEL`, `WIFI_BEACON_INTERVAL_TU` and `BLE_ADV_INTERVAL_MS` are protected by compile-time asserts -- an out-of-range value fails the build with an explanatory message instead of silently producing a broken firmware image.

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

## Flashing from CI artifacts

This path needs `esptool` on your PATH; if you have not installed ESP-IDF, `pip install esptool` is enough. Note the CI workflow is manual-dispatch only, so a run has to have been started from the Actions page for an artifact to exist.

1. Open the most recent successful `build` run on the repo's Actions page
2. Download and unzip the `dual-beacon-firmware` artifact
3. Hold the onboard BOOT button while plugging in the Dongle, then confirm the port: `ls /dev/cu.usbmodem*`
4. Flash it (replace `<PORT>` with the actual device):

```bash
esptool.py --chip esp32s3 -p <PORT> -b 460800 \
  --before default_reset --after hard_reset write_flash \
  --flash_mode dio --flash_size 8MB --flash_freq 80m \
  0x0     bootloader.bin \
  0x8000  partition-table.bin \
  0x10000 dual_beacon.bin
```

The three offsets are fixed by the partition table and must not be changed.

## License

MIT -- see [LICENSE](LICENSE).

## Hardware verification status

The radio behavior described above (WiFi AP visibility, iBeacon advertising, coexistence stability) has **never been verified on real hardware**. Only the host unit tests and the ESP-IDF compile have been verified. Treat this firmware as unflashed and unproven until someone runs it on an actual ESP32-S3 device and confirms it behaves as described.
