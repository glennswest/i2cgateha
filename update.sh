#arduino-cli upload -p /dev/cu.usbmodem53190055101 --fqbn m5stack:esp32:m5stack-paper i2CGateHA
/Users/gwest/Library/Arduino15/packages/m5stack/tools/esptool_py/3.1.0/esptool --chip esp32 --port /dev/cu.wchusbserial53190055101 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size 16MB 0xe000 /Users/gwest/Library/Arduino15/packages/m5stack/hardware/esp32/2.0.2/tools/partitions/boot_app0.bin 0x1000 ./build/m5stack.esp32.m5stack-paper/I2CGateHa.ino.bootloader.bin 0x10000 ./build/m5stack.esp32.m5stack-paper/I2CGateHa.ino.bin 0x8000 ./build/m5stack.esp32.m5stack-paper/I2CGateHa.ino.partitions.bin
./mkfilelist.sh
echo "ctl-a ctl-\ to exit"
sleep 11
screen -t "ctl-a ctl-\ to exit" /dev/cu.usbmodem53190055101 115200
