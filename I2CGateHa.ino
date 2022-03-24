//#define _ASYNC_TCP_SSL_LOGLEVEL_     4
//#define _ASYNC_HTTPS_LOGLEVEL_      4
//#define ASYNC_HTTPS_DEBUG_PORT      Serial

#include <AsyncDelay.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <AsyncTCP.h>
//#include <AsyncTCP_SSL.h>
#include "queue.h"

//#include <ESPmDNS.h>

extern "C" {
  #include "freertos/FreeRTOS.h"
  #include "freertos/timers.h"
}

#include <ArduinoOTA.h>
#include <M5EPD.h>
#include <arduino-timer.h>
#include <math.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"

#include <AsyncEventSource.h>
#include <AsyncJson.h>
#include <SPIFFSEditor.h>
#include <WebHandlerImpl.h>
#include <ESPAsyncWebServer.h>
#include <WebAuthentication.h>
#include <AsyncWebSynchronization.h>

#include <WebResponseImpl.h>
#include <StringArray.h>

#include <AsyncMqttClient.h>
#include <AsyncHTTPSRequest_Generic.h>           // https://github.com/khoih-prog/AsyncHTTPSRequest_Generic
AsyncHTTPSRequest request;

#include <cppQueue.h>

//char web_content[] = "https://raw.githubusercontent.com/glennswest/i2cgateha/main/contents/.version";
//char web_content[] = "https://api.github.com/";
//char web_content[] = "https://api.github.com/repos/glennswest/i2cgateha/contents/contents";
//char web_content[] = "https://worldtimeapi.org/api/timezone/Europe/London.txt";
// 192.168.1.248
#define MQTT_HOST IPAddress(192, 168, 1, 248)
#define MQTT_PORT 1883

auto timer = timer_create_default();

int scan_needed = 1;
int local_version;  // Version on SD card of content
int remote_version; // Version on github

struct tempPtrStruct {
  struct tempSensorStruct *ptr;
} tssptrrec;

typedef struct tempSensorStruct {
  char  name[32];
  uint8_t port;
  uint8_t bus;
  uint8_t addr;
  uint8_t chan;
  float value;
} tsrec;

cppQueue    sensorList(sizeof(tssptrrec), 100, FIFO);


struct content_entry {
       struct qentry_struct qe;
       char filename[256];
       int  todo;
       };

struct queue_struct content;

const int chipSelect = 4;
M5EPD_Canvas canvas(&M5.EPD);


const char* ssid = "gswlair";
const char* password = "0419196200";

const char* mqtt_server = "192.168.1.248";

AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;


AsyncWebServer  server(80);
String header;

void WiFiEvent(WiFiEvent_t event) {
  Serial.printf("[WiFi-event] event: %d\n", event);
  switch (event) {
    case SYSTEM_EVENT_STA_GOT_IP:
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP()); 
      Serial.print("RRSI: ");
      Serial.println(WiFi.RSSI()); 
      connectToMqtt();
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      Serial.println("WiFi lost connection");
      xTimerStop(mqttReconnectTimer, 0); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
      xTimerStart(wifiReconnectTimer, 0);
      break;
  }
}

void initSDCard() {
  if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
  log("Getting Local Version");
  local_version = getlocalversion();
}

void sendHttpRequest(char *theURL,void (*pFunc)(void* optParm, AsyncHTTPSRequest* request, int readyState))
//void sendHttpRequest(char *theURL)
{
  static bool requestOpenResult;
  if (request.readyState() == readyStateUnsent || request.readyState() == readyStateDone)
  {
    request.setTimeout(60);
    request.onReadyStateChange(pFunc);
    //request.setDebug(true);
    requestOpenResult = request.open("GET", theURL);
    
    if (requestOpenResult)
    {
      // Only send() if open() returns true, or crash
      Serial.println("Sending request");
      Serial.println(theURL);
      request.send();
    }
    else
    {
      Serial.println("Can't send bad request");
    }
  }
  else
  {
    Serial.println("Can't send request");
  }
}

void requestCB(void* optParm, AsyncHTTPSRequest* request, int readyState)
{
  (void) optParm;
  
  if (readyState == readyStateDone)
  {
    Serial.println("\n**************************************");
    Serial.println(request->responseLongText());
    Serial.println("**************************************");

    request->setDebug(false);
  }
}

void TCA9548A(uint8_t bus)
{
  Wire1.beginTransmission(0x70);  // TCA9548A address is 0x70
  Wire1.write(1 << bus);          // send byte to select bus
  Wire1.endTransmission();
}

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

void register_relay(uint8_t port, uint8_t bus, uint8_t addr, uint8_t chan)
{
  char thename[32];
  char theid[32];

  sprintf(thename, "r%1d%1d%2d%1d", port, bus, addr, chan);
  sprintf(theid,  "%s_id", thename);
  ha_sw_register(thename, theid, "switch");
  send_relay_status(port, bus, addr, chan);
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

void ha_tc_register(char *thename, char *theuid, char *thetype) {
  StaticJsonDocument<2024> dev;
  char cconfig[1400];
  char buf[256];
  char logb[256];

  //strcpy(logb,"");
  //sprintf(logb,"Device %s - %s",thetype,thename);
  //log(logb);
  dev["name"]            = thename;
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
  mqttClient.publish(buf, 0, false, cconfig, csize);
  //delay(100);

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
  //dev["unique_id"]       = buf; // "switch.i2cgateha0101_identify";
  //dev["entity_category"] = thetype; // "switch";
  //dev["device_class"]  = thetype; // "switch";
  dev["payload_on"]      = "ON";
  dev["payload_off"]     = "OFF";

  sprintf(buf, "homeassistant/%s/%s/availability", thetype, thename);
  dev["availability_topic"]  = buf;

  sprintf(buf, "homeassistant/%s/%s/set", thetype, thename);
  dev["command_topic"]   = buf;

  sprintf(buf, "homeassistant/%s/%s/state", thetype, thename);
  dev["state_topic"]     = buf;

  String config = dev.as<String>();
  unsigned int csize = config.length() + 1;
  //Serial.println("MQTT MSG: " + config);

  config.toCharArray(cconfig, csize);

  sprintf(buf, "homeassistant/%s/%s/config", thetype, thename); //char *subject = "homeassistant/switch/i2cgateha-01-01/config";
  //log(buf);
  //log(cconfig);

  mqttClient.subscribe(dev["state_topic"], 0);
  mqttClient.subscribe(dev["command_topic"], 0);
  mqttClient.publish(buf, 0, false, cconfig, csize);
  //delay(100);

}

void register_device(uint8_t port, uint8_t bus, uint8_t addr)
{
  char logb[256];

  if (addr == 0x70) { // Ignore Switch
    return;
  }

  switch (addr) {
    case 0x26: //* Grove 4 channel relay
      register_relay(port, bus, addr, 0);
      register_relay(port, bus, addr, 1);
      register_relay(port, bus, addr, 2);
      register_relay(port, bus, addr, 3);
      break;
    case 0x60:
      register_thermocouple(port, bus, addr, 0);
      break;
    default:
      sprintf(logb, "Unknown Device %1d - Bus %1d - Addr %02x", port, bus, addr);
      log(logb);
      break;
  }

}

void scanswitch()
{
  char logb[128];

  TCA9548A(0);
  uint8_t port = 0;

  for (uint8_t bus = 0; bus < 8; bus++) {
    TCA9548A(bus);
    for (uint8_t addr = 0; addr <= 127; addr++) {
      Wire1.beginTransmission(addr); // Check for 4 port relay
      int result = Wire1.endTransmission();
      if (result == 0) {
        register_device(port, bus, addr);
      }
      //delay(1);
    }
  }
}

void onMqttConnect(bool sessionPresent) {
  

  log("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);

  if (scan_needed == 1) {
     log("Starting scan of i2c");
     scanswitch();
     log("Scan Complete");
     content_check();
     timer.every(5000, check_temp_sensors);
     scan_needed = 0;
     server.begin();
     }
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  log("Disconnected from MQTT.");

  if (WiFi.isConnected()) {
    xTimerStart(mqttReconnectTimer, 0);
  }
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
  //Serial.println("Subscribe acknowledged.");
  //Serial.print("  packetId: ");
  //Serial.println(packetId);
  //Serial.print("  qos: ");
  //Serial.println(qos);
}

void onMqttUnsubscribe(uint16_t packetId) {
  //Serial.println("Unsubscribe acknowledged.");
  //Serial.print("  packetId: ");
  //Serial.println(packetId);
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  //Serial.println("Publish received.");
  //Serial.print("  topic: ");
  //Serial.println(topic);
  //Serial.print("  qos: ");
  //Serial.println(properties.qos);
  //Serial.print("  dup: ");
  //Serial.println(properties.dup);
  //Serial.print("  retain: ");
  //Serial.println(properties.retain);
  //Serial.print("  len: ");
  //Serial.println(len);
  //Serial.print("  index: ");
  //Serial.println(index);
  //Serial.print("  total: ");
  //Serial.println(total);
}

void onMqttPublish(uint16_t packetId) {
  //Serial.println("Publish acknowledged.");
  //Serial.print("  packetId: ");
  //Serial.println(packetId);
}

void epdlog(char *message)
{
#define STARTING_POS 105
#define LINE_OFFSET  20
#define MAX_LINE     50 * LINE_OFFSET

  static int current_pos = STARTING_POS;

  canvas.drawString(message, 25, current_pos);
  canvas.pushCanvas(0, 0, UPDATE_MODE_DU4); //Update the screen.
  current_pos = current_pos + LINE_OFFSET;
  if (current_pos > MAX_LINE) {
    current_pos = STARTING_POS;
  }
}

void log(String themessage)
{
  char Buf[250];

  themessage.toCharArray(Buf, 250);
  log(Buf);
}

void log(char *message)
{
  Serial.println(message);
  epdlog(message);

}


void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
 // delay(100);

 
  WiFi.begin(ssid, password);
  //while (WiFi.status() != WL_CONNECTED) {
  //  Serial.print('.');
  //  delay(1000);
  //}
  //Serial.println(WiFi.localIP());
  //Serial.print("RRSI: ");
  //Serial.println(WiFi.RSSI()); 
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void setup() {
  M5.begin();   //Init M5Paper.
  M5.EPD.SetRotation(90);   //Set the rotation of the display.
  M5.EPD.Clear(true);  //Clear the screen.
  M5.RTC.begin();  //Init the RTC.  初始化 RTC
  Wire1.begin(25, 32);
  canvas.createCanvas(540, 960);  //Create a canvas.  创建画布
  canvas.setTextSize(4); //Set the text size.  设置文字大小
  canvas.drawString("I2CGateHa", 25, 20);
  canvas.drawString("I2C Home Assistant Gateway", 25, 45);  //Draw a string.
  canvas.pushCanvas(0, 0, UPDATE_MODE_DU4); //Update the screen.

  Serial.begin(115200);
  log("Booting");


  


  initSDCard();

 


  canvas.drawString("I2CGateHa", 25, 20);
  canvas.drawString("I2C Home Assistant Gateway", 25, 45);  //Draw a string.
  //canvas.drawString(WiFi.localIP().toString(), 25, 60);
  canvas.pushCanvas(0, 0, UPDATE_MODE_DU4); //Update the screen.

  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
  wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));

  WiFi.onEvent(WiFiEvent);
 
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

 
  connectToWifi();
  
  log((char *)"Ready");
  
  websetup();
  
}


int getlocalversion()
{
char versionstr[32];
int theversion;
File vfile;

	vfile = SD.open("version");
        if (vfile){
           vfile.read((uint8_t * )versionstr,31);
           vfile.close();
          } else {
           strcpy(versionstr,"-1");
          }
        theversion = atoi(versionstr);
        return(theversion);

}
// Start the content update process
void content_check()
{
    
    log("Getting Remote Version");
    sendHttpRequest("https://raw.githubusercontent.com/glennswest/i2cgateha/main/contents/.version",remote_version_check);
}

void remote_version_check(void* optParm, AsyncHTTPSRequest* request, int readyState)
{
  (void) optParm;

  if (readyState == readyStateDone)
  {
    remote_version = request->responseText().toInt();
    Serial.printf("Remote Version: %d\n",remote_version);
    Serial.printf("Local Version: %d\n",local_version);
    request->setDebug(false);
    if (local_version < remote_version){
       log("Content Update Needed - Starting");
       start_content_update();
       }
    }
}

void start_content_update()
{
    log("Getting List of Updated Content");
    sendHttpRequest("https://raw.githubusercontent.com/glennswest/i2cgateha/main/contents/.content",get_content_list);
}

void get_content_list(void* optParm, AsyncHTTPSRequest* request, int readyState)
{
  (void) optParm;
  int tsize;
  char *tptr;
  char *sptr;
  
  if (readyState == readyStateDone)
  {
    Serial.printf("Got content list\n");
    
    tsize = request->responseText().length();
    Serial.printf("Size = %d\n",tsize);
    tptr = (char *)malloc(tsize+2);
    strcpy(tptr,request->responseText().c_str());
    //strcpy(tptr,request->responseText());
    Serial.printf("Ptr = %p\n",tptr);
    
    //remote_version = request->responseText().toInt();
    //Serial.printf("Remote Version: %d\n",remote_version);
    //Serial.printf("Local Version: %d\n",local_version);
    //request->setDebug(false);
    //if (local_version < remote_version){
       //log("Content Update Needed - Starting");
       //start_content_update();
       //}
    }
}

void websetup()
{
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SD, "/index.html", "text/html");
  });
  server.serveStatic("/", SD, "/");
  server.begin();
}

void loop() {
  timer.tick();
  
}
