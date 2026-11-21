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
| UP / DOWN | Tune by step (hold to repeat) |
| SIDE keys | SSB/CW: fine tune with the BFO, AM/FM: change step |
| F | Mode picker: F/UP/DOWN choose, MENU or EXIT applies (no wait while choosing) |
| * | Next step |
| MENU | HF setup menu |
| EXIT | Back to the normal VHF/UHF radio |

Frequency entry fills in the blanks of `__.___` MHz and accepts by itself when the
last blank is filled: `14230` = 14.230, `7150` = 7.150 (a first digit 4-9 is a single
MHz digit), `3573` = 3.573 (3 not followed by 0 too). `*` jumps to the kHz part for
the rest: `1*840` = 1.840, `*531` = 0.531 (MW). F switches to an FM template `___._`
(`945` = 94.5, `1011` = 101.1). MENU accepts early, EXIT deletes the last digit
(cancels when empty). HF frequencies typed while in FM switch to the band's usual mode.

Screen: status line has the band (20M HAM / 25M BC), step and AGC. In SSB/CW the
frequency is shown to 10 Hz (`14.229.88` = dial plus BFO fine tune) and the bottom
shows the filter passband next to the carrier, with the bandwidth.

### HF setup menu

UP/DOWN select, MENU edits (UP/DOWN or SIDE keys change the value live; the mode is
only switched when you confirm), MENU or EXIT finishes editing, 1-9 jump to an item,
EXIT closes the menu. The bottom line explains the selected item.

| Item | Values |
| --- | --- |
| Mode | FM, AM, LSB, USB, CW |
| Band | 20 HF bands, each remembers its last frequency; the band mode is applied when you finish editing |
| Step | 1, 5, 10, 50, 100, 500, 1000 kHz |
| AGC | AUTO, or ATT 0/1/5/15/26 (AGC off). Now applied in AM and FM too, not only SSB |
| SSB BW | 1.2, 2.2, 3.0, 4.0, 0.5, 1.0 kHz |
| Fine step | 10, 25, 50, 100 Hz (SIDE key fine tune step) |
| BFO tune | SSB/CW fine tune below 1 kHz (same as the SIDE keys), folds into the dial at +-500 Hz |
| Boot HF | OFF / ON (same setting as HFBoot in the normal menu) |
| Exit HF | MENU leaves HF mode |
