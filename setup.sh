arduino-cli config init
arduino-cli core update-index
arduino-cli core update-index  --additional-urls https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/arduino/package_m5stack_index.json
arduino-cli core install m5stack:esp32
arduino-cli board attach -p /dev/cu.usbmodem53190055101 -b m5stack:esp32:m5stack-paper I2CGateHa.ino


