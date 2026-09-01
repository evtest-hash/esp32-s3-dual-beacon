# Hardware notes: MuseLab ESP32-S3 Dongle

Facts this firmware depends on, read off the vendor's v1.0 schematic. The
schematic, datasheet and board photos are not redistributed here -- see
[Sources](#sources).

Most of this is a transcription of a document rather than a measurement, so
treat a disagreement with real hardware as this file being wrong. The two
exceptions are marked **measured**: the antenna section below, and the pin and
supply facts the firmware exercises every boot.

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

This section is **measured**, not transcribed. It is the reason this board's
range is poor, and no firmware change reaches it.

```
LNA_IN --+-- L2 --+-- L1 --+-- E1 (ceramic)
        C18      C17      C16
         |        |        |
        GND      GND     1.2pF
                           |
                          GND
```

Read off the v1.0 schematic: `L2` is `0R` (a jumper), `L1` is marked `TBD`,
`C17` and `C18` are `NC`, and only `C16` at 1.2 pF is populated.

### It does not match Espressif's design guidelines

The ESP32-S3 schematic checklist asks for a CLC matching network at the chip
end, and gives values for it. Mapping that onto this board:

| Guideline, chip-end CLC | Recommended | Here | Actual |
|---|---|---|---|
| shunt C nearest the chip | 1.2 - 1.8 pF | `C18` | **NC** |
| series L | **2.0 - 3.0 nH** | `L2` | **0R jumper** |
| shunt C | 1.2 - 1.8 pF | `C17` | **NC** |

None of the three is implemented. The series position holds a zero-ohm resistor
where an inductor belongs, and both shunt capacitors are absent. What is left
between the chip and the antenna is a jumper, an inductor of undocumented
value, and one shunt capacitor at the antenna.

The same schematic *does* follow the guideline elsewhere -- `L3` on XTAL_P is
24 nH, exactly the value the checklist gives for crystal harmonic suppression.
The RF matching is the part that was left unfinished.

### What was measured

- **The series path is intact.** DC resistance from the ceramic antenna's feed
  terminal to the trace entering LNA_IN is 0 - 0.2 ohm, which is a 0R jumper
  plus a small inductor's DCR. `L1` is populated; the empty pads visible on the
  board are `C17` and `C18`, which are `NC` by design.
- **The loss is after the power amplifier.** Sweeping both radios across their
  full transmit power ladders moved received RSSI 1:1 with the commanded value,
  slope 1.07 (WiFi) and 1.10 (BLE), with no flattening at the top. The PA
  reaches the top of its range; something after it throws the power away.
- **About 20 dB of it.** At 1 m line of sight the board is received roughly
  20 dB weaker than a commercial 20 dBm access point several metres away. This
  is a lower bound and does not depend on the receiver's absolute calibration.

A pure impedance mismatch cannot account for 20 dB -- even a 10:1 VSWR costs
under 5 dB. So the missing CLC is one contributor among others, the rest being
the radiating efficiency available to a ceramic chip antenna on a ground plane
this small, inside a plastic USB stick.

### Rework, if you want the matching network the guideline asks for

Populate the chip-end CLC: `C18` and `C17` at 1.5 pF C0G/NP0, and replace the
`L2` jumper with a 2.4 nH high-Q inductor. Leave `L1` and `C16` alone to start
with. Match the package to the footprints already on the board.

Two warnings. Without a vector network analyser this is guesswork -- the right
way is to measure S11 at the LNA_IN pad and tune from there. And expect a few
dB, not twenty: the antenna's own efficiency is the larger term, and recovering
that means a different antenna, not different component values.

Reworking any of this invalidates `IBEACON_TX_POWER` in
`firmware/main/beacon_config.h`, which is a measured RSSI at 1 m. Re-measure it.

## Sources

The vendor's schematic, the ESP32-S3 datasheet and the board photos are
copyrighted by MuseLab and Espressif respectively and are **not** included in
this repository. The upstream repository carries no license, so it grants no
redistribution rights.

- Board repository: https://github.com/wuxx/esp32-s3-dongle
- ESP32-S3 datasheet and technical reference: https://www.espressif.com/en/products/socs/esp32-s3
- ESP32-S3 schematic checklist, which the antenna section above is measured against:
  https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/schematic-checklist.html
