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

## Menu driven HF mod (CEC_0.51HF-M1)

This fork replaces the "remember which key does what" HF controls with a setup menu
and on-screen key hints. VOX and the flashlight are disabled to make room in flash.

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
| F | Mode AM -> LSB -> USB -> CW -> AM (FM -> AM) |
| * | Next step |
| MENU | HF setup menu |
| EXIT | Back to the normal VHF/UHF radio |

Frequency entry: type kHz (`7074`, `14074`) or MHz with `*` as the decimal point
(`7*074`, `93*1`). Short numbers are MHz (`7`, `14`, `93`, `100`). MENU accepts,
EXIT deletes the last digit (cancels when empty); 5 digits, or 3 digits after the
point, are accepted automatically. FM broadcast frequencies (70-108 MHz) switch to FM.
HF frequencies typed while in FM switch to the band's usual mode.

SSB fine tune folds the BFO into the dial frequency every 1 kHz, so the shown
frequency plus the BFO offset is where you are listening.

### HF setup menu

UP/DOWN select, MENU edits (UP/DOWN or SIDE keys change the value live), MENU or EXIT
finishes editing, 1-9 jump to an item, EXIT closes the menu.

| Item | Values |
| --- | --- |
| Mode | FM, AM, LSB, USB, CW |
| Band | 20 HF bands, each remembers its last frequency; the band mode is applied when you finish editing |
| Step | 1, 5, 10, 50, 100, 500, 1000 kHz |
| AGC | AUTO, or ATT 0/1/5/15/26 (AGC off). Now applied in AM and FM too, not only SSB |
| SSB BW | 1.2, 2.2, 3.0, 4.0, 0.5, 1.0 kHz |
| FineStp | 10, 25, 50, 100 Hz (SIDE key fine tune step) |
| BFO | Current BFO offset |
| Boot HF | OFF / ON (same setting as HFBoot in the normal menu) |
| Exit HF | MENU leaves HF mode |
