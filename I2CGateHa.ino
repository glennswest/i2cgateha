
#define SD_FAT_TYPE 3     // Get us Fat32/ExFat
#define SPI_SPEED SD_SCK_MHZ(4)
#include <SdFatConfig.h>
#include <sdios.h>
#include <SdFat.h>
#include <ESP32Time.h>
#include "time.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

SdFs sd;
FsFile file;

ESP32Time rtc;

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


#include <M5EPD.h>
#include <arduino-timer.h>
#include <math.h>


//#include <AsyncEventSource.h>
#include <AsyncJson.h>
//#include <SPIFFSEditor.h>
#include <WebHandlerImpl.h>
#include <ESPAsyncWebServer.h>
#include <WebAuthentication.h>
//#include <AsyncWebSynchronization.h>

#include <WebResponseImpl.h>
#include <StringArray.h>




#include <AsyncMqttClient.h>
#include <AsyncHTTPSRequest_Generic.h>           // https://github.com/khoih-prog/AsyncHTTPSRequest_Generic
AsyncHTTPSRequest request;

#include <cppQueue.h>

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


#define DL_STATE_DOWNLOAD_NEEDED  1
#define DL_STATE_DOWNLOAD_STARTED 2
#define DL_STATE_DOWNLOAD_DONE    3
#define DL_STATE_DOWNLOAD_FAILED  4
FsFile dfile;
struct content_entry {
       struct qentry_struct qe;
       char filename[128];
       int  state;
       };

struct queue_struct dl_q;
struct content_entry *cur_dl;

const int chipSelect = 4;
M5EPD_Canvas canvas(&M5.EPD);

AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;
TimerHandle_t displayTimer;

static unsigned long linuxtime;
static int rtc_set_needed = 0;

#include "logging.h"
#include "switch.h"
#include "thermocouple.h"
#include "relay.h"
#include "cupdate.h"
#include "ntpupdate.h"

const char* ssid = "gswlair";
const char* password = "0419196200";

const char* mqtt_server = "192.168.1.248";



AsyncWebServer  server(80);
String header;
char message[256];

void WiFiEvent(WiFiEvent_t event) {
  
  switch (event) {
    case SYSTEM_EVENT_STA_GOT_IP:
      update_rtc();
      log("WiFi connected");
      sprintf(message,"IP address: %s",WiFi.localIP().toString());
      log(message);
      sprintf(message,"Gateway: %s",WiFi.gatewayIP().toString());
      log(message);
      sprintf(message,"DNS: %s",WiFi.dnsIP(0).toString());
      log(message);
      sprintf(message,"RSSI: %d",WiFi.RSSI());
      log(message);
      connectToMqtt();
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      log("WiFi lost connection");
      xTimerStop(mqttReconnectTimer, 0); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
      xTimerStart(wifiReconnectTimer, 0);
      break;
  }
}

void initSDCard() {
char message[256];
 
  if (!sd.begin(chipSelect, SPI_SPEED)) {
       log("SD Card Failed to Initialise");
       log("Functionality will be lost");
       return;
       }
  if (sd.vol()->fatType() == 0) {
      log("SD Card not formated properly");
      log("Use SD Assoc Formatter");
      log("Functionality will be lost");
    }     
  uint32_t size = sd.card()->sectorCount();
  uint32_t sizeMB = 0.000512 * size + 0.5;
  log("SD Card Ready");
  sprintf(message,"Card Size: %dMB", sizeMB);
  log(message);

  local_version = getlocalversion();
  sprintf(message,"Current Content Version: %d",local_version);
  log(message);
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

  
  if (scan_needed == 1) {
     log("Starting scan of i2c");
     scanswitch();
     log("Scan Complete");
     //content_check();
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
  log("Connecting to Wi-Fi...");
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
  log("Connecting to MQTT...");
  mqttClient.connect();
}

int  readbigfatsd(String thepath,uint8_t *buffer, size_t index,int maxlen)
{
FsFile vfile;
char message[256];
int thesize = 0;

  sprintf(message,"%s: Index: %d MaxLen: %d\n",thepath,index,maxlen);
  Serial.print(message);
  if (maxlen > 8196){
     maxlen = 8196;
     }
  
  vfile.open((char *)thepath.c_str(),O_RDWR);
  if (vfile){
     vfile.seekSet(index);
     thesize = vfile.read(buffer,maxlen);
     vfile.close();
     }
  return(thesize);
}

bool handleStaticFile(AsyncWebServerRequest *request) {
  String path = request->url();
 
  if (path.endsWith("/")) path += F("index.html");
  
  String contentType = request->contentType();
  String pathWithGz = path + ".gz";

  if (sd.exists(pathWithGz) || sd.exists(path)){
    bool gzipped = false;
    if (sd.exists(pathWithGz)) {
        gzipped = true;
        path += ".gz";
        }    
    
    AsyncWebServerResponse *response = request->beginChunkedResponse(contentType, [path](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {      
        return readbigfatsd(path,buffer,index,maxLen);
    });
    response->addHeader("Server","ESP Async Web Server");
    request->send(response);
    return true;
    }
  
  return false;
}


void websetup()
{
   server.onNotFound([](AsyncWebServerRequest *request) {
      if (handleStaticFile(request)) return; 
      request->send(404);
      });
  server.begin();
}



void setup(){
char message[256];


  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
  log_setup();
  delay(2);
  
  M5.begin();   //Init M5Paper.
  setup_rtc();
  
  M5.EPD.SetRotation(90);   //Set the rotation of the display.
  M5.EPD.Clear(true);  //Clear the screen.
  canvas.createCanvas(540, 960);  //Create a canvas.  创建画布
  
  M5.RTC.begin();  //Init the RTC.  初始化 RTC
  Wire1.begin(25, 32);
  
  Serial.begin(115200);
  log("Booting");
  
  initSDCard();

  epdupdate(NULL);
  timer.every(500, epdupdate);
 

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






void loop() {
time_t now;
tm *localtm;
char message[256];

  timer.tick();
}
