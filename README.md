# GPN17 Badge
GPN Badge (Gulasch Push Notifications)

HowTo SetUp arduino:
- Install the newest arduino Version (tested with 1.8.2) from the [Arduino website](http://www.arduino.cc/en/main/software).
- Start Arduino and open Preferences window.
- Enter ```http://public.electronics.fail/package_gpnbadge_index.json``` into *Additional Board Manager URLs* field. You can add multiple URLs, separating them with commas.
- Open Boards Manager from Tools > Board menu and install *gpnbadge* platform (and don't forget to select the GPN Badge board from Tools > Board menu after installation).

To use multirom ability, please add 
```#include "rboot.h"```
to each of your sketches. This replaces the cache function in the libmain.a
Please not: For the moment, rboot is configured as following:

- 4 ROMs / 1MB each
- default boot ROM: 1 
- GPIO boot ROM: 0 (usually the ROM select app)

The GPIO boot is initialised by pulling GPIO16 high at POR. This is the same as pressing the center joystick of the GPN Badge at poweron. Other ROMs can be selected by using the rboot config library.

The patched arduino-esp8266 contains all tools for creating the usual eboot arduino roms, as well as rboot roms.
For this, in addition to esptool also Richard Burtons esptool2 ist downloaded and installed.

Have fun!


## Beschreibung
Teilnehmerbadge mit vielen Funktionen für die GPN 17.

## Display-lib
https://github.com/antonxy/TFT_ILI9163C

## Infrared-lib
https://github.com/markszabo/IRremoteESP8266

## Neopixel-lib
https://github.com/adafruit/Adafruit_NeoPixel
