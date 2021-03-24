# Key emulator

![Device](/pics/device.jpg)

This project aims at creating a device able to emulate most common key types such as Dallas(iButton), Cyfral and Metacom. A good idea would be to add RFID capabilities, as this type of key is also fairly common.

## Design

![Wiring](/pics/wiring.png)

At the heart of the emulator lies a 3.3V Arduino Pro Mini chosen in order to use battery voltage without a boost converter. As the whole project needs to be portable, it has a 150mah battery with TP4056 module for charging and protection. All the information gets displayed on an OLED display and all of this gets controlled by 3 pushbuttons.

Emulator's case gets 3D-printed in 3 parts: main body, part holding arduino and TP4056 module, and the lid. All stl files as well as Fusion 360 master file are available in the repository. More information about printing can be found on [Thingiverse page](https://www.thingiverse.com/thing:4578023).

The wiring is done with simple wires, so considering emulator's small size (66.5 * 16 * 33 mm) it is real messy.

Contact pads for reading/emulating keys are made of simple male headers, which is not accepted by some locks probably due to low contact area and contact pads being dirty, which is elliminated by wired key's shape. So a redesign with a key-style emulation pad is needed.

## Code

Main intention of the [code](https://github.com/s0ko1ex/Key-emulator/blob/master/main.cpp) was its modularity which was achieved to some extend: code for debouncing can be used separatly, screen manager and overall architecture can be adapted to other progects. Nevertheless it was not refactored, which leaves a lot to be desired (comments, multiple files file, etc).

Size of the code is relatively large - 27.1 Kb with available space being 30 Kb, - so a change to a new MCU with more Flash memory must be made, as there will be more key modules. One idea was to substitute Pro Mini for Pro Micro (one USB port for charging and programming, more SRAM, etc), but ditching it altogether and going for an STM32 also seems to be a good idea. Although it will require to adapt code to a whole other architecture, which requires a lot more work.

One of the missing features is low-power mode for using the battery mode efficiently. Moreover, currently there is no indication of battery charge and no safety measures when it is too low.

All of the keys are kept in EEPROM, which can accomodate 25 8-byte keys with 21-character names. So an upgrade to Micro-SD card is needed.

## TODO

- [ ] Do code refactoring, create a couple of libraries
- [ ] Add code dealing with the battery
- [ ] Redesign case
- [ ] Design and manufacture a PCB with a Micro-SD card
- [ ] Change MCU to a more powerful one
- [ ] Add RFID capabilities
