# Hardware notes: MuseLab ESP32-S3 Dongle

Facts this firmware depends on, read off the vendor's v1.0 schematic. The
schematic, datasheet and board photos are not redistributed here -- see
[Sources](#sources).

Nothing below has been measured on a board. It is a transcription of a
document, so treat a disagreement with real hardware as this file being wrong.

## Chip

| | |
|---|---|
| Part | ESP32-S3FN8 |
| Flash | 8 MB, in package |
| PSRAM | none |

No PSRAM matters twice: it is why `CONFIG_ESPTOOLPY_FLASHSIZE_8MB` is correct,
and it is why GPIO33-37 are free for the microSD slot instead of being taken by
octal PSRAM.

## Pin assignments

| Pin | Function | Circuit |
|---|---|---|
| GPIO0 | BOOT button | S1 to GND. Hold while plugging in to enter download mode |
| GPIO1 | Status LED | R3 1K to D3 (blue) to GND, so a **high level lights it** |
| GPIO4 | -- | R5 10K pull-down |
| GPIO19 / GPIO20 | Native USB D- / D+ | R2 / R4 22R in series to the USB connector |
| GPIO34-38 | microSD | D3 / CMD / CLK / D0 / D1 |

The firmware uses GPIO1 only. It never touches the microSD pins.

There is no USB-to-UART bridge on this board: USB D+/D- go straight to the chip,
so flashing uses the built-in USB Serial/JTAG and the device appears as
`/dev/cu.usbmodem*` -- not `/dev/cu.usbserial*`.

`J1` is a 5-pin PROGRAM header exposing 3V3, EN, GPIO0, RXD0 and TXD0, for UART
flashing if the USB path is unavailable.

## Clock

X1 40 MHz, with C1 / C2 20 pF loading caps and L3 24 nH in series on XTAL_P.
40 MHz is the only crystal frequency the ESP32-S3 supports, so ESP-IDF needs no
configuration for it.

## Power

```
USB 5V -- F1 fuse -- 5V -- ME6211C33M5G (U2, CE tied high) -- 3V3
                     |                                        |
                   C10 10uF                                  C9 22uF
```

Plus 0.1 uF decoupling at VDD3P3_CPU and VDD3P3_RTC.

The LDO's rating is not on the schematic. WiFi TX peaks in the mid-300 mA range
on this chip, and because WiFi and BLE share one radio they never transmit at
the same time, so the peak draw is the WiFi peak. This firmware's duty cycle is
very low either way -- a beacon frame every ~102 ms and an advertisement every
100 ms -- so it is a mild load compared to a station streaming data.

## Antenna

```
E1 (ceramic) --+-- L1 --+-- L2 --+-- LNA_IN
             C16 1.2pF  C17      C18
               |         |        |
              GND       GND      GND
```

**The matching network is under-specified in the v1.0 schematic**: `L1` is
marked `TBD`, `L2` is `0R` (a jumper), and C17 / C18 are not populated. A real
board must have something in the L1 position or there would be no path to
LNA_IN at all, but the value is not documented and is not derivable from the
schematic.

This affects radiated efficiency and therefore range. It does not affect whether
the radio functions.

## Sources

The vendor's schematic, the ESP32-S3 datasheet and the board photos are
copyrighted by MuseLab and Espressif respectively and are **not** included in
this repository. The upstream repository carries no license, so it grants no
redistribution rights.

- Board repository: https://github.com/wuxx/esp32-s3-dongle
- ESP32-S3 datasheet and technical reference: https://www.espressif.com/en/products/socs/esp32-s3
