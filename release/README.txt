Mazduino CAN IO Extender - firmware @VERSION@

Nothing else to install: avrdude for Windows, macOS and Linux is included.

Contents
  flash.bat         Windows: double-click
  flash.command     macOS: double-click
  flash.sh          Linux (or macOS Terminal)
  hex/              Firmware .hex, one file per crystal and node
  bin/              The same firmware as .bin
  tunerstudio/      mazduino-iox.ini for TunerStudio
  tools/avrdude/    avrdude 8.3 (GPL-2.0, see COPYING)

Which file
  Use 8mhz. 16mhz is only for modified boards.
  Node 0 is the first module (factory default); nodes 1-3 are for a second,
  third and fourth module on the same CAN bus.

Steps
  1. Disconnect 12 V from the module and connect its USB.
  2. Windows: double-click flash.bat.
     macOS: double-click flash.command. If macOS blocks it, right-click >
     Open > Open.
     Linux: run ./flash.sh in a terminal.
  3. Enter the port shown in the list (e.g. COM5 or /dev/cu.usbmodem1101)
     and the node (Enter = 0).
  4. When it says Done, unplug USB and reconnect 12 V.

  From a command line: flash.bat 0 COM5   |   ./flash.sh 0 /dev/ttyACM0

Problems
  No sync: press RESET on the Mega just as avrdude starts, then try again.
  Apple Silicon Mac: install Rosetta once if asked
    (softwareupdate --install-rosetta --agree-to-license).
  Linux "permission denied": sudo usermod -aG dialout $USER, then log in again.

TunerStudio
  New project, ECU definition "Other / Browse", pick
  tunerstudio/mazduino-iox.ini from this same zip.
  Signature: @SIGNATURE@

Full guide: https://wiki.mazduino.com/can-io-extender/
