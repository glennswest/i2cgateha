# I2CGateHA - I2C Gateway to HomeAssistant

## Introduction
[HomeAssistant](https://www.home-assistant.io/) is a open source home automation software that puts local control of your home automation. Its supported and ddeveloped by a world wide community of tinkerers and DIY enthusiasts. Another rich ecosystem is [Grove](https://www.seeedstudio.com/category/Grove-c-1003.html). This is a set of hardware devices including sensors, compute, and infrastructure based around i2c and other technologies. It removes the need to breadboard, and makes uses the sensors plug and play. 

The Gateway software runs on a arduino controller, with initial support for a ESP32 base m5stack epaper device. It will in future support additonal hardware platforms based on ESP32 and RP2010.  

##Known Issues
1. Need to move to mqtt client to async version to fix connection lost
2. Need to impletement linked list for epd update
3. Need to add relay sets
4. Need to add mutli-port support (Only 1 at moment, but structure is partially there)
5. Need to add missing thermistor detection to therm code
6. Add web gui for config and status (In progress)
7. Need to remove hard coded mqtt and ap config and move to json (In process)


