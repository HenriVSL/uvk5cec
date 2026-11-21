# UV-K5 CEC HF (SI4732-A10) source

This folder is the **HF full-band receive** firmware line (`CEC_0.51HF`).

It is separate from the main CEC firmware in the repository root (`CEC_0.3V`).

## Hardware note

Requires the SI4732-A10 hardware modification (BK1080 removed / replaced).  
See: http://www.hamskey.com/

## Build

This tree reuses the parent `external/` directory.

### Windows (cmd)

```bat
mklink /J external ..\external
set PATH=C:\Program Files (x86)\GNU Arm Embedded Toolchain\10 2021.10\bin;%PATH%
set PATH=C:\Program Files (x86)\GnuWin32\bin;%PATH%
make
```

### Linux / macOS

```bash
ln -s ../external external
make
```

Verified build on 2026-09-15 (produces `firmware.bin`).

## HENE_1.0 firmware : menu driven HF (based on KD8CEC CEC_0.51HF)

This fork replaces the "remember which key does what" HF controls with a setup menu
and on-screen key hints.

Features compiled out by default to make room in flash (set to 1 in the Makefile, or
`make ENABLE_xxx=1`, to bring one back):

| Option | Removes | Saves |
| --- | --- | --- |
| ENABLE_VOX | VOX | ~750 B |
| ENABLE_FLASHLIGHT | Flashlight | ~200 B |
| ENABLE_SCANNER | CTCSS/DCS code scanner (F+*, * in RxCTCS/RxDCS menu) and the F+4 frequency scanner. Channel/VFO scanning stays | ~2.9 KB |
| ENABLE_ROGER | Roger / MDC end of TX beep, Roger menu | ~360 B |
| ENABLE_WELCOME | Power on message / voltage screen, POnMsg menu | ~400 B |
| ENABLE_DTMF_TX | DTMF sending: PTT ID menu, DTMF input with *, keypad tones while transmitting. SIDE2 1750 Hz tone burst during TX stays | ~920 B |

### Getting into HF mode

* Normal menu, first item **HF Rx**: press MENU to start the HF receiver (MENU, MENU from the main screen).
* Normal menu **HFBoot** = ON: the radio powers on straight into HF. EXIT still returns to the normal radio.
* Side buttons (F1Shrt/F1Long/F2Shrt/F2Long/M Long) can be set to **HF RADIO**.
* F + 0 still works.

### HF main screen

| Key | Action |
| --- | --- |
| 0-9 | Frequency entry (the first digit counts) |
| UP / DOWN | Tune by step (hold to repeat). SSB/CW at step FINE: tune by the Fine step (10-100 Hz) |
| * / SIDE1 | Step up / step down. SSB/CW steps: FINE, 1k ... 1000k |
| SIDE2 (lower) | Seek: short press up, hold down. Any key stops. AM and FM only (the chip can not seek SSB) |
| F | Mode picker: F/UP/DOWN choose, MENU or EXIT applies (no wait while choosing) |
| MENU | HF setup menu |
| EXIT | Back to the normal VHF/UHF radio |

Frequency entry fills in the blanks of `__.___` MHz and accepts by itself when the
last blank is filled: `14230` = 14.230, `07150` = 7.150, `00531` = 0.531 (MW).
`*` jumps past the point (`7*150`). F switches to the FM template `___._`
(`0945` = 94.5, `1011` = 101.1). MENU accepts early, EXIT deletes the last digit
(cancels when empty). HF frequencies typed while in FM switch to the band's usual mode.

Screen: status line has the band (20M HAM / 25M BC), step and AGC. In SSB/CW the
frequency is shown to 10 Hz (`14.229.88` = dial plus BFO fine tune) and the bottom
shows the filter passband next to the carrier, with the bandwidth.

### HF setup menu

UP/DOWN select, MENU edits (UP/DOWN or SIDE keys change the value live; the mode is
only switched when you confirm), MENU or EXIT finishes editing, 1-9 and 0 (= 10) jump to an item,
EXIT closes the menu. The bottom line explains the selected item.
AVC, Soft mute and Seek lvl are stored at EEPROM 0x0F28. FM broadcast uses 50 us
de-emphasis (`HF_FM_DEEMPHASIS` in cec_radio.c, 2 = 75 us for the Americas).

| Item | Values |
| --- | --- |
| Mode | FM, AM, LSB, USB, CW |
| Band | 20 HF bands, each remembers its last frequency; the band mode is applied when you finish editing |
| Step | 1, 5, 10, 50, 100, 500, 1000 kHz, plus FINE in SSB/CW |
| Seek | MENU seeks up from here (same as SIDE2) |
| AGC | AUTO, or ATT 0/1/5/15/26 (AGC off). Now applied in AM and FM too, not only SSB |
| Filter | AM: 1.0, 1.8, 2.0, 2.5, 3.0, 4.0, 6.0 kHz channel filter. SSB/CW: 1.2, 2.2, 3.0, 4.0, 0.5, 1.0 kHz. FM: automatic |
| AVC | Automatic volume control max gain: OFF (SSB off, AM 12 dB), 30, 50, 70, 90 dB. Default 90 (now also on in SSB) |
| Soft mute | Max attenuation of the hiss on weak signals: OFF, 8 (default), 16, 32 dB |
| Line NR | AM power line (mains hum/buzz) noise filter, OFF / ON |
| Seek lvl | Signal (RSSI) a seek stops on: 10-35 dBuV, default 20 |
| Fine step | 10, 25, 50, 100 Hz (UP/DOWN step at step FINE) |
| BFO tune | SSB/CW fine tune below 1 kHz (same as UP/DOWN at step FINE), folds into the dial at +-500 Hz |
| Boot HF | OFF / ON (same setting as HFBoot in the normal menu) |
| Exit HF | MENU leaves HF mode |
