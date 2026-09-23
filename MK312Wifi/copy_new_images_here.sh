#!/bin/sh
echo "Trying to copy firmware image here..."
cp -a /tmp/arduino_build_*/MK312Wifi.ino.bin . && echo "Done!"
echo "Trying to copy LittleFS image here..."
cp -a /tmp/arduino_build_*/MK312Wifi.mklittlefs.bin . && echo "Done!"
ls -l *.bin
sleep 2
