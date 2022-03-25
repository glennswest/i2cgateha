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

AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

#include "logging.h"
#include "switch.h"
#include "thermocouple.h"
#include "relay.h"

const char* ssid = "gswlair";
const char* password = "0419196200";

const char* mqtt_server = "192.168.1.248";



AsyncWebServer  server(80);
String header;

void WiFiEvent(WiFiEvent_t event) {
  Serial.printf("[WiFi-event] event: %d\n\r", event);
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
  Serial.printf("SD Card Size: %lluMB\n\r", cardSize);
  log("Getting Local Version");
  local_version = getlocalversion();
}

void downloadCB(void *uptr, tsize thesize)
{
AsyncHTTPSRequests *req;

     req = (AsyncHTTPSRequests *)uptr;


}

void downloadweb(char *theURL,char *theFileName)
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

void sendHttpRequest(char *theURL,void (*pFunc)(void* optParm, AsyncHTTPSRequest* request, int readyState))
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
    //sendHttpRequest("https://raw.githubusercontent.com/glennswest/i2cgateha/main/contents/.content",requestCB);
}

void get_content_list(void* optParm, AsyncHTTPSRequest* request, int readyState)
{
  (void) optParm;
  int tsize;
  int tbeg;
  int toff;
  String filename;
  
  if (readyState == readyStateDone)
  {
    Serial.print("Got content list\n");
    //Serial.println(request->responseLongText());
    
    tsize = request->responseText().length();
    Serial.printf("Size = %d\n\r",tsize);
    tbeg = 0;
    toff = request->responseText().indexOf(char(10));
    Serial.printf("Off = %d\n",toff);
    if (toff == -1 ){
       log("No Data");
       return;
       }
    Serial.printf("Process toff\n");
    while(toff > 0){
      filename = request->responseText().substring(tbeg,toff);
      Serial.printf("Filename: %s\n\r",filename.c_str());
      tbeg = toff + 1;
      toff = request->responseText().indexOf(char(10));
      }
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
