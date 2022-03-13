#include <AsyncDelay.h>
//#include <SoftWire.h>
#include <Wire.h>
#include <ArduinoJson.h>
//#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <M5EPD.h>
#include <PubSubClient.h>
//#include <M5Stack.h>
#include <M5EPD.h>
#include <LinkedList.h>
#include <arduino-timer.h>

auto timer = timer_create_default();
class TempSensor {
      public:
          char *name;
          float value;
          };
          
LinkedList<TempSensor*> sensorList = LinkedList<TempSensor*>();   

  
M5EPD_Canvas canvas(&M5.EPD);

const char* ssid = "gswlair";
const char* password = "0419196200";

const char* mqtt_server = "192.168.1.248";
WiFiClient espClient;
PubSubClient client(espClient);


void TCA9548A(uint8_t bus)
{
  Wire1.beginTransmission(0x70);  // TCA9548A address is 0x70
  Wire1.write(1 << bus);          // send byte to select bus
  Wire1.endTransmission();
}
 
uint8_t read_relay(uint8_t port,uint8_t bus,uint8_t addr, uint8_t chan)
{
  uint8_t relayvalue;
  
  TCA9548A(bus);
  
  Wire1.beginTransmission(addr);
  Wire1.write(0x11);
  Wire1.endTransmission();
  Wire1.requestFrom(addr,1);
  byte tvalue = Wire1.read();
  
 
  relayvalue = (tvalue >> chan) & 1; 
  return(relayvalue);
}

float send_thermocouple_status(uint8_t port,uint8_t bus,uint8_t addr, uint8_t chan)
{
char buf[256];
char message[128];
char thetype[16];
char thename[16];
float value;
int csize;   

    sprintf(thename,"t%1d%1d%2d%1d",port,bus,addr,chan);
    value = read_thermocouple(port,bus,addr,chan);
    //sprintf(message,"state/%s/%s: {\"temperature\": %.1f}","sensor",thename,value);
    sprintf(message,"%.1f",value);
    strcpy(thetype,"sensor");
    sprintf(thename,"t%1d%1d%2d%1d",port,bus,addr,chan);
    sprintf(buf,"homeassistant/%s/%s/state",thetype,thename); 
    csize = strlen(message);
    client.publish(buf,message,csize);
    return value;
}

float read_thermocouple(uint8_t port,uint8_t bus,uint8_t addr, uint8_t chan)
{
  uint8_t tvalue;
  uint8_t data1;
  uint8_t data2;
  float temp;
  int status;
  char buf[64];

  log("Reading Thermocouple");
  status = 0x0;
  while (status == 0x00){
        status = tc_temp_stat(port,bus,addr,chan);
        log("Waiting on temp"); 
        log(status);
  }
  log("Got It");
  
  TCA9548A(bus);
  
  Wire1.beginTransmission(addr);
  Wire1.write(0x00);
  Wire1.endTransmission();
  Wire1.requestFrom(addr, 2);
    if (Wire1.available() == 2){
      data1 = Wire1.read();
      data2 = Wire1.read();
      log(data1);
      log(data2);
      if((data1 & 0x80) == 0x80){
        data1 = data1 & 0x7F;
        temp = 1024 - ( data1 * 16 + data2/16);
       } else { 
        data1 = data1 *16;
        data2 = data2 * 0.0625;
        log(data1);
        log(data2);
        temp = data1 + data2;
        //   temp = ( data1 * 16 + data2/16);
        } 
      } else {
        temp = 0;
        log("No data from sensor");
      }
  sprintf(buf,"Temp: %0.1f",temp);
  log(buf);
  //tc_stat_clr(port,bus,addr,chan);
  
  return(temp);
} 

void send_relay_status(uint8_t port,uint8_t bus,uint8_t addr, uint8_t chan)
{
char buf[256];
char message[32];
char logb[256];
char thetype[16];
char thename[16];
int csize;
uint8_t value;
  
    value = read_relay(port,bus,addr,chan);
    strcpy(thetype,"switch");
    sprintf(thename,"r%1d%1d%2d%1d",port,bus,addr,chan);
    sprintf(buf,"homeassistant/%s/%s/state",thetype,thename);    
    switch(value){
      case 0:
       strcpy(message,"OFF");
       break;
      case 1:
       strcpy(message,"ON");
       break;      
       }
       
 //sprintf(logb,"RelayStatus: %s - %s",buf,message);
 //log(logb);
 
 csize = strlen(message);
 client.publish(buf,message,csize);
}

void register_relay(uint8_t port,uint8_t bus,uint8_t addr, uint8_t chan)
{
char thename[32];
char theid[32];

         sprintf(thename,"r%1d%1d%2d%1d",port,bus,addr,chan);
         sprintf(theid,  "%s_id",thename);        
         ha_sw_register(thename,theid,"switch");
         send_relay_status(port,bus,addr,chan);
}

int tc_temp_stat(uint8_t port,uint8_t bus,uint8_t addr, uint8_t chan)
{
int stat;

  TCA9548A(bus);
  Wire1.beginTransmission(addr);
  Wire1.write(0x04);
  Wire1.endTransmission();
  delay(50);
  Wire1.requestFrom(addr, 1);
    if (Wire1.available() == 1)
  {
   stat = Wire1.read();
  }

  return stat;
}

void tc_temrmo_set(uint8_t port,uint8_t bus,uint8_t addr, uint8_t chan)
{
  TCA9548A(bus);
  Wire1.beginTransmission(addr);
  Wire1.write(0x05);
  Wire1.write(0x00);
  Wire1.endTransmission();
}

void tc_device_set(uint8_t port,uint8_t bus,uint8_t addr, uint8_t chan)
{
  TCA9548A(bus);
  Wire1.beginTransmission(addr);
  Wire1.write(0x06);
  Wire1.write(0x00);
  Wire1.endTransmission();
}

int tc_stat_clr(uint8_t port,uint8_t bus,uint8_t addr, uint8_t chan)
{
  TCA9548A(bus);
  Wire1.beginTransmission(addr);
  Wire1.write(0x04);
  Wire1.write(0x0F);
  Wire1.endTransmission();
}

void setup_thermocouple(uint8_t port,uint8_t bus,uint8_t addr, uint8_t chan)
{
  int status;
  
  tc_temrmo_set(port,bus,addr,chan);
  tc_device_set(port,bus,addr,chan);
  status = tc_temp_stat(port,bus,addr,chan); // Kick off temp reading
}

void register_thermocouple(uint8_t port,uint8_t bus,uint8_t addr, uint8_t chan)
{
char *thename;
char theid[32];
float value;

         thename = (char *)malloc(32);   
         sprintf(thename,"t%1d%1d%2d%1d",port,bus,addr,chan);
         sprintf(theid,  "%s_id",thename); 
        
         setup_thermocouple(port,bus,addr,chan);       
         ha_tc_register(thename,theid,"sensor");
         value = send_thermocouple_status(port,bus,addr,chan);
         TempSensor *sensor = new TempSensor();
         sensor->name = thename;
         sensor->value = value;
         sensorList.add(sensor);

}

void ha_tc_register(char *thename,char *theuid,char *thetype){
StaticJsonDocument<2024> dev;
char cconfig[1400];
char buf[256];
char logb[256];

     strcpy(logb,"");
     sprintf(logb,"Device %s - %s",thetype,thename);
     log(logb);
     dev["name"]            = thename; 
     sprintf(buf,"%sId",thename);     
     dev["unit_of_measurement"] = "°C";
     dev["device_class"] = "temperature";
     //dev["value_template"] = "{{value_json.temperature}}";
     sprintf(buf,"homeassistant/%s/%s/state",thetype,thename);
     dev["state_topic"]     = buf; 
     
     String config = dev.as<String>();
     unsigned int csize = config.length() + 1;
     
     config.toCharArray(cconfig,csize);
                
    
    
     client.subscribe(dev["state_topic"]);
     
     sprintf(buf,"homeassistant/%s/%s/config", thetype,thename); //char *subject = "homeassistant/switch/i2cgateha-01-01/config";
     log(buf);
     log(cconfig);
     client.publish(buf,cconfig,csize);
     delay(100);
   
}

void ha_sw_register(char *thename,char *theuid,char *thetype){
StaticJsonDocument<2024> dev;
char cconfig[1400];
char buf[256];
char logb[1400];

     strcpy(logb,"");
     sprintf(logb,"Device %s - %s",thetype,thename);
     log(logb);
     dev["name"]            = thename; 
     sprintf(buf,"%sId",thename);     
     //dev["unique_id"]       = buf; // "switch.i2cgateha0101_identify";
     //dev["entity_category"] = thetype; // "switch";
     //dev["device_class"]  = thetype; // "switch";
     dev["payload_on"]      = "ON";
     dev["payload_off"]     = "OFF";

     sprintf(buf,"homeassistant/%s/%s/availability",thetype,thename);
     dev["availability_topic"]  = buf; 
     
     sprintf(buf,"homeassistant/%s/%s/set",thetype,thename);
     dev["command_topic"]   = buf;
  
     sprintf(buf,"homeassistant/%s/%s/state",thetype,thename);
     dev["state_topic"]     = buf; 
     
     String config = dev.as<String>();
     unsigned int csize = config.length() + 1;
     //Serial.println("MQTT MSG: " + config);
     
     config.toCharArray(cconfig,csize);
                
     sprintf(buf,"homeassistant/%s/%s/config", thetype,thename); //char *subject = "homeassistant/switch/i2cgateha-01-01/config";
     //log(buf);
     //log(cconfig);
     
    
     client.subscribe(dev["state_topic"]);
     client.subscribe(dev["command_topic"]);
     //client.subscribe(dev["availability_topic"]);
     client.publish(buf,cconfig,csize);
     delay(100);
   
}

void register_device(uint8_t port, uint8_t bus, uint8_t addr)
{
char logb[256];
  
         if (addr == 0x70){ // Ignore Switch
            return;
            }
         
         switch(addr){
               case 0x26: //* Grove 4 channel relay
                    register_relay(port,bus,addr,0);
                    register_relay(port,bus,addr,1);
                    register_relay(port,bus,addr,2);
                    register_relay(port,bus,addr,3);
                    break;
               case 0x60:      
                    register_thermocouple(port,bus,addr,0);
                    break;
               default:
                    sprintf(logb,"Unknown Device %1d - Bus %1d - Addr %02x",port,bus,addr);
                    log(logb);
                    break;
         }

}

void scanswitch()
{
char logb[128];

  TCA9548A(0);
  uint8_t port = 0;
  
  for(uint8_t bus = 0; bus < 8; bus++){
    TCA9548A(bus);
    for (uint8_t addr = 0;addr <= 127; addr++){
      Wire1.beginTransmission(addr); // Check for 4 port relay
      int result = Wire1.endTransmission();
      if (result == 0){
         register_device(port,bus,addr);
         }
        delay(1); 
        }
      }
}


void reconnect() {
  static int scan_needed = 1;
  // Loop until we're reconnected
  while (!client.connected()) {
    log("Attempting MQTT connection...");
    // Attempt to connect
    client.setKeepAlive(60);
    if (client.connect("i2cGateHa","device","device1122")) {
      log("connected");
      if (scan_needed == 1){
         scanswitch();
         scan_needed = 0;
         }
      // Subscribe
      client.subscribe("i2c-output");
    } else {
      log("failed, rc=");
      log(client.state());
      log(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message: ");
  Serial.print(topic);
  //Serial.print(". Message: ");
  //String messageTemp;
  
  //for (int i = 0; i < length; i++) {
  //  Serial.print((char)message[i]);
  //  messageTemp += (char)message[i];
  //}
  Serial.println();

  // Feel free to add more if statements to control more GPIOs with MQTT

  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off". 
  // Changes the output state according to the message
//  if (String(topic) == "esp32/output") {
//    Serial.print("Changing output to ");
//    if(messageTemp == "on"){
//      Serial.println("on");
//    digitalWrite(ledPin, HIGH);
//    }
//    else if(messageTemp == "off"){
//      Serial.println("off");
//    digitalWrite(ledPin, LOW);
//    }
//  }
}



void epdlog(char *message)
{
#define STARTING_POS 105
#define LINE_OFFSET  20
#define MAX_LINE     50 * LINE_OFFSET

static int current_pos = STARTING_POS;

    canvas.drawString(message,25,current_pos);
    canvas.pushCanvas(0,0,UPDATE_MODE_DU4);  //Update the screen.
    current_pos = current_pos + LINE_OFFSET; 
    if (current_pos > MAX_LINE){
        current_pos = STARTING_POS;
        } 
}

void log(String themessage)
{
char Buf[250];

    themessage.toCharArray(Buf,250);
    log(Buf);
}

void log(char *message)
{
  Serial.println(message);
  epdlog(message);

}

bool check_temp_sensors(void *)
{

  log("BEEP!");
  return true;
}

void setup() {
  M5.begin();   //Init M5Paper.  
  M5.EPD.SetRotation(90);   //Set the rotation of the display.  
  M5.EPD.Clear(true);  //Clear the screen. 
  M5.RTC.begin();  //Init the RTC.  初始化 RTC
  Wire1.begin(25, 32);
  canvas.createCanvas(540, 960);  //Create a canvas.  创建画布
  canvas.setTextSize(3); //Set the text size.  设置文字大小
  canvas.drawString("I2CGateHa",25,20);
  canvas.drawString("I2C Home Assistant Gateway", 25, 45);  //Draw a string.
  canvas.pushCanvas(0,0,UPDATE_MODE_DU4);  //Update the screen. 
    
  Serial.begin(115200);
  log("Booting");
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.setSleep(false); // To avoid disconnect
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    log("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  ArduinoOTA.setHostname("i2cgateha");

  // No authentication by default
  //ArduinoOTA.setPassword("0419196200");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      log("Start updating " + type);
    })
    .onEnd([]() {
      log("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();

  canvas.drawString("I2CGateHa",25,20);
  canvas.drawString("I2C Home Assistant Gateway", 25, 45);  //Draw a string.
  canvas.drawString(WiFi.localIP().toString(), 25, 60);
  canvas.pushCanvas(0,0,UPDATE_MODE_DU4);  //Update the screen.
   
  log((char *)"Ready");
  
  log((char *)"Setting up MQTT");
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  
  timer.every(30000, check_temp_sensors);
  }

void loop() {
  timer.tick();
  ArduinoOTA.handle();
   if (!client.connected()) {
    reconnect();
  }
  client.loop();
}
