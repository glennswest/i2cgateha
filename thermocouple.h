
void send_thermocouple_status(uint8_t port, uint8_t bus, uint8_t addr, uint8_t chan, float value)
{
  char buf[256];
  char message[128];
  char thetype[16];
  char thename[16];
  int csize;

  sprintf(thename, "t%1d%1d%2d%1d", port, bus, addr, chan);

  sprintf(message, "%.1f", value);
  strcpy(thetype, "sensor");
  sprintf(thename, "t%1d%1d%2d%1d", port, bus, addr, chan);
  sprintf(buf, "homeassistant/%s/%s/state", thetype, thename);
  csize = strlen(message);
  mqttClient.publish(buf, 0, false, message, csize);
}

int tc_temp_stat(uint8_t port, uint8_t bus, uint8_t addr, uint8_t chan)
{
  int stat;

  TCA9548A(bus);
  Wire1.beginTransmission(addr);
  Wire1.write(0x04);
  Wire1.endTransmission();

  Wire1.requestFrom(addr, 1);
  if (Wire1.available() == 1)
  {
    stat = Wire1.read();
  }

  return stat;
}

float read_thermocouple(uint8_t port, uint8_t bus, uint8_t addr, uint8_t chan)
{
  uint8_t tvalue;
  uint8_t data1;
  uint8_t data2;
  float   value1;
  float   value2;
  float temp;
  float rtemp;
  int status;
  char buf[64];
  char thename[32];

  status = 0x0;
  while (status == 0x00) {
    status = tc_temp_stat(port, bus, addr, chan);
  }
  TCA9548A(bus);

  Wire1.beginTransmission(addr);
  Wire1.write(0x00);
  Wire1.endTransmission();
  Wire1.requestFrom(addr,(uint8_t) 2);
  if (Wire1.available() == 2) {
    data1 = Wire1.read();
    data2 = Wire1.read();
    //log(data1);
    //log(data2);
    if ((data1 & 0x80) == 0x80) {
      data1 = data1 & 0x7F;
      temp = 1024 - ( (float) data1 * 16 + (float)data2 / 16);
    } else {
      value1 = data1 * 16;
      value2 = data2 * 0.0625;
      //log(data1);
      //log(data2);
      temp = value1 + value2;
      //   temp = ( data1 * 16 + data2/16);
    }
  } else {
    temp = 0;
    sprintf(thename, "t%1d%1d%2d%1d", port, bus, addr, chan);
    log("No data from sensor");
    log(thename);
  }
  //sprintf(buf,"Temp: %0.1f",temp);
  //log(buf);
  //tc_stat_clr(port,bus,addr,chan);
  return (temp);
}


void tc_temrmo_set(uint8_t port, uint8_t bus, uint8_t addr, uint8_t chan)
{
  TCA9548A(bus);
  Wire1.beginTransmission(addr);
  Wire1.write(0x05);
  Wire1.write(0x00);
  Wire1.endTransmission();
}

void tc_device_set(uint8_t port, uint8_t bus, uint8_t addr, uint8_t chan)
{
  TCA9548A(bus);
  Wire1.beginTransmission(addr);
  Wire1.write(0x06);
  Wire1.write(0x00);
  Wire1.endTransmission();
}

void tc_stat_clr(uint8_t port, uint8_t bus, uint8_t addr, uint8_t chan)
{
  TCA9548A(bus);
  Wire1.beginTransmission(addr);
  Wire1.write(0x04);
  Wire1.write(0x0F);
  Wire1.endTransmission();
}

void setup_thermocouple(uint8_t port, uint8_t bus, uint8_t addr, uint8_t chan)
{
  int status;

  tc_temrmo_set(port, bus, addr, chan);
  tc_device_set(port, bus, addr, chan);
  status = tc_temp_stat(port, bus, addr, chan); // Kick off temp reading
}

struct tempPtrStruct *newSensorEntry()
{
  struct tempPtrStruct *ptrentry;
  tsrec     *entry;

  ptrentry = (struct tempPtrStruct *)malloc(sizeof(struct tempPtrStruct));
  entry = (tsrec *)malloc(sizeof(tsrec));
  ptrentry->ptr = entry;
  return (ptrentry);
}

void ha_tc_register(char *thename, char *theuid, char *thetype) {
  StaticJsonDocument<2024> dev;
  char cconfig[1400];
  char buf[256];
  char logb[256];

  //strcpy(logb,"");
  //sprintf(logb,"Device %s - %s",thetype,thename);
  //log(logb);
  dev["name"]            = thename;
  dev["object_id"]       = thename;
  sprintf(buf, "%sId", thename);
  dev["unit_of_measurement"] = "°C";
  dev["device_class"] = "temperature";
  sprintf(buf, "homeassistant/%s/%s/state", thetype, thename);
  dev["state_topic"]     = buf;

  String config = dev.as<String>();
  unsigned int csize = config.length() + 1;

  config.toCharArray(cconfig, csize);


  mqttClient.subscribe(dev["state_topic"], 0);

  sprintf(buf, "homeassistant/%s/%s/config", thetype, thename); //char *subject = "homeassistant/switch/i2cgateha-01-01/config";
  //log(buf);
  //log(cconfig);
  Serial.println("MQTT MSG: " + config);

  mqttClient.publish(buf, 0, false, cconfig, csize);
  //delay(100);

}

void register_thermocouple(uint8_t port, uint8_t bus, uint8_t addr, uint8_t chan)
{
  char *thename;
  char theid[32];
  float value;
  tsrec *sensor;
  struct tempPtrStruct *tsrecptr;

  thename = (char *)malloc(32);
  sprintf(thename, "t%1d%1d%2d%1d", port, bus, addr, chan);
  sprintf(theid,  "%s_id", thename);

  setup_thermocouple(port, bus, addr, chan);
  ha_tc_register(thename, theid, "sensor");
  value = read_thermocouple(port, bus, addr, chan);
  send_thermocouple_status(port, bus, addr, chan, value);


  tsrecptr = newSensorEntry();
  sensor = tsrecptr->ptr;
  strcpy(sensor->name, thename);
  sensor->port = port;
  sensor->bus  = bus;
  sensor->addr = addr;
  sensor->chan = chan;
  sensor->value = value;
  sensorList.push(tsrecptr);

}

bool check_temp_sensors(void *)
{
  float new_value;
  tsrec *sensor;
  struct tempPtrStruct tsrecptr;


  char buf[256];

  for (int i = 0 ; i < sensorList.getCount(); i++) {
    sensorList.peekIdx(&tsrecptr, i);
    sensor = tsrecptr.ptr;
    new_value = read_thermocouple(sensor->port, sensor->bus, sensor->addr, sensor->chan);
    if ((new_value > sensor->value - .50) && (new_value < sensor->value + .50)) {
      new_value = sensor->value;
    }
    if (new_value != sensor->value) {
      send_thermocouple_status(sensor->port, sensor->bus, sensor->addr, sensor->chan, new_value);
      sensor->value = new_value;
      sprintf(buf, "Sensor: %s Temp %.2f", sensor->name, sensor->value);
      log(buf);
    }
  }
  return true;
}
