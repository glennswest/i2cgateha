uint8_t read_relay(uint8_t port, uint8_t bus, uint8_t addr, uint8_t chan)
{
  uint8_t relayvalue;

  TCA9548A(bus);

  Wire1.beginTransmission(addr);
  Wire1.write(0x11);
  Wire1.endTransmission();
  Wire1.requestFrom(addr,(uint8_t)1);
  byte tvalue = Wire1.read();


  relayvalue = (tvalue >> chan) & 1;
  return (relayvalue);
}

void send_relay_status(uint8_t port, uint8_t bus, uint8_t addr, uint8_t chan)
{
  char buf[256];
  char message[32];
  char logb[256];
  char thetype[16];
  char thename[16];
  int csize;
  uint8_t value;

  value = read_relay(port, bus, addr, chan);
  strcpy(thetype, "switch");
  sprintf(thename, "r%1d%1d%2d%1d", port, bus, addr, chan);
  sprintf(buf, "homeassistant/%s/%s/state", thetype, thename);
  switch (value) {
    case 0:
      strcpy(message, "OFF");
      break;
    case 1:
      strcpy(message, "ON");
      break;
  }

  //sprintf(logb,"RelayStatus: %s - %s",buf,message);
  //log(logb);

  csize = strlen(message);
  mqttClient.publish(buf, 0, false, message, csize);
}

void ha_sw_register(char *thename, char *theuid, char *thetype) {
  StaticJsonDocument<2024> dev;
  char cconfig[1400];
  char buf[256];
  char logb[1400];

  //strcpy(logb,"");
  //sprintf(logb,"Device %s - %s",thetype,thename);
  //log(logb);
  dev["name"]            = thename;
  sprintf(buf, "%sId", thename);
  dev["object_id"]       = thename; // "switch.i2cgateha0101_identify";
  //dev["entity_category"] = thetype; // "switch";
  dev["device_class"]  = thetype; // "switch";
  dev["payload_on"]      = "ON";
  dev["payload_off"]     = "OFF";

  sprintf(buf, "homeassistant/%s/%s/availability", thetype, thename);
  dev["availability_topic"]  = buf;

  sprintf(buf, "homeassistant/%s/%s/set", thetype, thename);
  dev["command_topic"]   = buf;

  sprintf(buf, "homeassistant/%s/%s/state", thetype, thename);
  dev["state_topic"]     = buf;
  dev["payload"] = "OFF";

  String config = dev.as<String>();
  unsigned int csize = config.length() + 1;
  Serial.println("MQTT MSG: " + config);

  config.toCharArray(cconfig, csize);

  sprintf(buf, "homeassistant/%s/%s/config", thetype, thename); //char *subject = "homeassistant/switch/i2cgateha-01-01/config";
  //log(buf);
  //log(cconfig);

  mqttClient.subscribe(dev["state_topic"], 0);
  mqttClient.subscribe(dev["command_topic"], 0);
  mqttClient.publish(buf, 0, false, cconfig, csize);
  //delay(100);

}

void register_relay(uint8_t port, uint8_t bus, uint8_t addr, uint8_t chan)
{
  char thename[32];
  char theid[32];

  sprintf(thename, "r%1d%1d%2d%1d", port, bus, addr, chan);
  sprintf(theid,  "%s_id", thename);
  ha_sw_register(thename, theid, "switch");
  send_relay_status(port, bus, addr, chan);
}
