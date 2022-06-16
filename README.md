# ZX-ESPectrum-Wiimote

**LATEST UPDATES: Better CPU / Video tasks sync, allowing partial support of multicolor modes. Preliminary realtime .tap file loading support. Use new option in menu to select .tap file and F6,F7 to start/stop tape.**

An emulation of the ZX-Spectrum computer on an ESP32 chip with VGA output based on bitluni's driver, with PS/2 keyboard support, using a Wiimote as input device, based on bigw00d's driver.

There are per-game customizable (through simple text files) correspondences from Wiimote keys to Spectrum keys.

**This version is adjusted for using on a ESP32 Devkit with custom pin connection to VGA, PS/2 and audio**

Please watch the [project video on YouTube](https://youtu.be/ROthljwC5OA) (spanish audio, english subtitles).

If you have a LilyGo TTGo VGA32, please check the [lilygo-ttgo-vga32 branch](https://github.com/dcrespo3d/ZX-ESPectrum-Wiimote/tree/lilygo-ttgo-vga32).

This is a fork of the [ZX-ESPectrum](https://github.com/rampa069/ZX-ESPectrum) project, based on it, but with some enhancements.

## Features

- Spectrum 16/48 architecture emulation without PSRAM.
- Spectrum 128/+2/+3 architecture emulation with PSRAM.
- VGA output, 3 bit, 6 bit (default), 14 bit (untested).
- Accurate Z80 emulation, with enhanced timing and fast video generation.
- Dual Z80 emulators, selectable in compile time using #defines: the precise one (JLS), and the fast one (LKF)
- Contended memory algorithm for very precise timing on 48K, a little less precise on 128K.
- 48K sound: beeper digital output, good PWM sound using JLS CPU core.
- 128K sound: AY-3-8912 sound chip emulation (incomplete but working).
- PS/2 Keyboard used as input for Spectrum keys.
- Wiimote support with per-game key assignments.
- VGA OSD menu: Configuration, architecture, ROM and SNA/Z80 selection.
- Support for two aspect ratios: 16:9 or 4:3 monitors (using 360x200 or 320x240 modes)
- Tape saving and loading (untested).
- SNA snapshot loading.
- Z80 snapshot loading.
- Quick (to memory) and persistent snapshot saving and loading (both 48K and 128K supported).
- Internal SPIFFS support / external SD card support (only one of both, see hardconfig.h).
- Original Spectrum Keyboard support, using 8 + 5 contacts ribbon connectors connected to 13 ESP pins.

## Work in progress

- Better AY-3-8912 emulation (128K sound is still a little dirty).

## Compiling and installing

Windows, GNU/Linux and MacOS/X. This version has been developed using PlatformIO.

#### Install platformIO:

- There is an extension for Atom and VSCode, please check [this website](https://platformio.org/).
- Select your board, I have used a Espressif ESP32-WROVER.

#### Customize platformio.ini

Change upload_port to whatever you're using.
- Linux: `uploadport = /dev/ttyUSB0` or similar.
- Windows: `upload_port = COM1` or similar.
- MacOSX: `upload_port = /dev/cu.SLAB_USBtoUART` or similar.

#### Select your aspect ratio

Default aspect ratio is 16:9, so if your monitor has this, you don't need to change anything.

If your monitor is 4:3, you should edit hardconfig.h, comment the `#define AR_16_9 1` line, and uncomment the `#define AR_4_3 1` line.

#### Upload the data filesystem

If using internal flash storage (USE_INT_FLASH #defined in hardconfig.h), you must copy some files to internal storage using this procedure.

`PlatformIO > Project Tasks > Upload File System Image`

All files under the `/data` subdirectory will be copied to the SPIFFS filesystem partition. Run this task whenever you add any file to the data subdirectory (e.g, adding games in .SNA or .Z80 format, into the `/data/sna` subdirectory).

NEW: now including my own Spectrum 48K games: [Snake](https://github.com/dcrespo3d/zx-spectrum-snake) and [Tetris](https://github.com/dcrespo3d/zx-spectrum-tetris). NOTE: the games have NO sound.

#### Using a external micro SD Card and copying games into it

**(NOTE: this feature has been tested only on a Lilygo TTGo VGA32 board, which has build-in uSD card slot. Pins from hardware.h are from that board, if using a regular ESP32 you must connect the SD card CS, CLK, MISO and MOSI signals to the ESP32 and update pins in hardware.h accordingly)**

If using external micro sd card (USE_SD_CARD #defined in hardconfig.h), you must copy files from the `/data` subdirectory to the root of the sd card (copy the contents of the folder, NOT the folder itself, so boot.cfg is on the root folder).

The SD card should be formatted in FAT16 / FAT32.

For adding games to the emulator, just turn it off, extract the sd card, copy games in .SNA or .Z80 format to the `/sna` folder of the sd card, insert it again, and turn it on.

Important note: once you flash the firmware successfully with USE_SD_CARD defined, you won't be able to flash the firmware again, unless you remove the SD card. This is due to GPIO pin 2/12 used for SD card MISO/MOSI interfering with the boot/flashing process.

What I do: before flashing (`PlatformIO > Project Tasks > Upload`), I remove the SD card until flashing starts, then I insert it again. For convenience I use a micro sd card extension strap which features a push to insert / push to remove mechanism, which comes in handy.

#### Compile and flash it

`PlatformIO > Project Tasks > Build `, then

`PlatformIO > Project Tasks > Upload`.

Run these tasks (`Upload` also does a `Build`) whenever you make any change in the code.

## Hardware configuration and pinout

See ESP32 pin assignment in `hardpins.h` or change it to your own preference.

I have used VGA 3 bit driver (so BRIGHT attribute is lost), but it's simpler to connect and no R-2R DAC is needed. Also, I have assigned pins to bottom side of the ESP32, as it is too wide for usual breadboards.

![esp32-pinout](./docs/esp32-pinout.jpg)

## Connecting a Wiimote

To connect, press 1 and 2 buttons in the Wiimote.

All 4 leds will flash during connection phase, and only LED 1 will be ON when connected.

Important note: wiimote suport is NOT enabled by default on the TTGO. I have experienced slowness at least once when developing, but seems to have gone forever. It may happen when the option is enabled, but a Wiimote has never been paired. I'm not completely sure, your mileage may vary.

You can enable wiimote support uncommenting `#define WIIMOTE_PRESENT` in hardconfig.h

## OSD Menu

OSD menu can be opened using the Wiimote's Home key. Navigation is done using the D-Pad, and selection using buttons A, 1 or 2.

From OSD you can load snapshots (from `/data/sna`) or change ROMs.

## Assigning Wiimote keys to emulated Spectrum keys
For every `.sna / .z80` game in `/data/sna`, there should be a corresponding `.txt` file in the same dir, with a very simple format. Examples are provided.

The `ESPWiimote` library generates the following codes for Wiimote keys:

![wiimote-key-codes](./docs/wiimote-key-codes.jpg)

The `.txt` file must have at least 16 characters (only first 16 characters are considered). For example for Manic Miner, `ManicMiner.txt` would contain

`sZ-h------WQe---`

There is a character for each possible 16 bit code of wiimote keys, in order: `0001`, `0002`, `0004`, `0008`, `0010`, `0020`, `0040`, `0080`, `0100`, `0200`, `0400`, `0800`, `1000`, `2000`, `4000`, `8000`; and legal characters are:

```
1234567890
QWERTYUIOP
ASDFGHJKLe
hZXCVBNMys
````
With `e` for ENTER, `h` for CAPS SHIFT, `y` for SYMBOL SHIFT and `s` for SPACE. Any other character (ex: '``') means not used.

For the Manic Miner example, the correspondences would be:

```
(2)    (0001) -> SPACE (jump)
(1)    (0002) -> Z     (jump)
(A)    (0008) -> SHIFT (jump)
(down) (0400) -> W     (right)
(up)   (0800) -> Q     (left)
(+)    (1000) -> ENTER (start)

```

I have NOT included Manic Miner `.sna / .z80` snapshot, but you can download it from [worldofspectrum.org](https://worldofspectrum.org/archive/software/games/manic-miner-bug-byte-software-ltd) and convert it to `.sna / .z80` using [FUSE](http://fuse-emulator.sourceforge.net/), for example.

## My development history

I have write a detailed story, with photos, of the development process of this emulator and some of the devices I have tested it with.
- [Development story, in english](https://dcrespo3d.github.io/ZX-ESPectrum-Wiimote/my-esp32-history-en/)
- [Historia del desarrollo, en español](https://dcrespo3d.github.io/ZX-ESPectrum-Wiimote/my-esp32-history-es/)

## Thanks to

- Developers of the [original project](https://github.com/rampa069/ZX-ESPectrum), [Rampa](https://github.com/rampa069) and [Queru](https://github.com/jorgefuertes).
- Idea from the work of Charles Peter Debenham Todd: [PaseVGA](https://github.com/retrogubbins/paseVGA).
- VGA Driver from [ESP32Lib by BitLuni](https://github.com/bitluni/ESP32Lib).
- PS/2 keyboard support based on [ps2kbdlib](https://github.com/michalhol/ps2kbdlib).
- PS/2 boot for some keyboards from [PS2KeyAdvanced](https://github.com/techpaul/PS2KeyAdvanced).
- Wiimote library from [ESP32Wiimote by bigw00d](https://github.com/bigw00d/Arduino-ESP32Wiimote).
- Z80 Emulation (precise) derived from [z80cpp](https://github.com/jsanchezv/z80cpp), authored by José Luis Sánchez.
- Z80 Emulation (fast) derived from [z80emu](https://github.com/anotherlin/z80emu), authored by Lin Ke-Fong.
- AY sound hardware emulation from [AVR-AY](https://www.avray.ru/).
- [Amstrad PLC](http://www.amstrad.com) for the ZX-Spectrum ROM binaries [liberated for emulation purposes](http://www.worldofspectrum.org/permits/amstrad-roms.txt).
- [Nine Tiles Networs Ltd](http://www.worldofspectrum.org/sinclairbasic/index.html) for Sinclair BASIC.
- Gary Lancaster for the [+3e ROM](http://www.worldofspectrum.org/zxplus3e/).
- [Retroleum](http://blog.retroleum.co.uk/electronics-articles/a-diagnostic-rom-image-for-the-zx-spectrum/) for the diagnostics ROM.
- Emil Vikström for his [ArduinoSort](https://github.com/emilv/ArduinoSort) library.
- [StormBytes](https://www.youtube.com/channel/UCvvVcAC0n4dCuZ-SIIYOUCQ) for his code and help for supporting the original ZX Spectrum keyboard.
- Fabrizio di Vittorio for his [FabGL library](https://github.com/fdivitto/FabGL) which I use for sound only (but it's a great library).
- [Ackerman](https://github.com/rpsubc8/ESP32TinyZXSpectrum) for his code and ideas for the emulation of the AY-3-8912 sound chip, and for discussing details about this development.
- [EremusOne](https://github.com/EremusOne) for adding multiple snapshot slots, .TAP support, and other fixes.

## And all the involved people from the golden age

- [Sir Clive Sinclair](https://en.wikipedia.org/wiki/Clive_Sinclair).
- [Christopher Curry](https://en.wikipedia.org/wiki/Christopher_Curry).
- [The Sinclair Team](https://en.wikipedia.org/wiki/Sinclair_Research).
- [Lord Alan Michael Sugar](https://en.wikipedia.org/wiki/Alan_Sugar).
- [Investrónica team](https://es.wikipedia.org/wiki/Investr%C3%B3nica).
- [Matthew Smith](https://en.wikipedia.org/wiki/Matthew_Smith_(games_programmer)) for [Manic Miner](https://en.wikipedia.org/wiki/Manic_Miner).
- [Sovietic cloners](https://en.wikipedia.org/wiki/List_of_ZX_Spectrum_clones).
- DCrespo's uncle Pedro for giving him his first computer: a [Sinclair ZX81](https://en.wikipedia.org/wiki/ZX81).
- Queru's uncle Roberto for introducing him into the microcomputing world with a [Commodore VIC-20](https://en.wikipedia.org/wiki/Commodore_VIC-20).
- Queru's uncle Manolito for introducing him into the ZX-Spectrum world.
- Rampa's mother for the [Oric 1](https://en.wikipedia.org/wiki/Oric#Oric-1) and for inculcate her passion for electronics.

## And all the writters, hobbist and documenters

- [Microhobby magazine](https://es.wikipedia.org/wiki/MicroHobby).
- Dr. Ian Logan & Dr. Frank O'Hara for [The Complete Spectrum ROM Disassembly book](http://freestuff.grok.co.uk/rom-dis/).
- Chris Smith for the The [ZX-Spectrum ULA book](http://www.zxdesign.info/book/).
- Users from [Abadiaretro](https://abadiaretro.com/) and its Telegram group.
- Users from [El Mundo del Spectrum](http://www.elmundodelspectrum.com/) and its Telegram group.
- [The World of Spectrum](http://www.worldofspectrum.org/).

## A lot of programmers, especially

- [GreenWebSevilla](https://www.instagram.com/greenwebsevilla/) for its Fantasy Zone game and others.
- Julián Urbano Muñoz for [Speccy Pong](https://www.instagram.com/greenwebsevilla/).
- Others who have donated distribution rights for this project.
