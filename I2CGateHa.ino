#include <AsyncDelay.h>
#include <SoftWire.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <M5EPD.h>
#include <PubSubClient.h>
//#include "TCA9548A.h"
//#include <M5Stack.h>
#include <M5EPD.h>
//#include <Wire.h>
SoftWire portA(25, 32);
//TCA9548A<SoftwareWire> portAtca;

  
M5EPD_Canvas canvas(&M5.EPD);

const char* ssid = "gswlair";
const char* password = "0419196200";

const char* mqtt_server = "192.168.1.248";
WiFiClient espClient;
PubSubClient client(espClient);

int check_for_switch(SoftWire theport){
    theport.beginTransmission(0x70);
    int result = theport.endTransmission();
    Serial.print("Is Switch Presence? ");
    Serial.print(result);
    Serial.print("\n");
    return(result);
}
    

void clog_setup() {


}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    client.setKeepAlive(60);
    if (client.connect("i2cGateHa","device","device1122")) {
      Serial.println("connected");
      // Subscribe
      client.subscribe("i2c-output");
      registerdevice();
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // Feel free to add more if statements to control more GPIOs with MQTT

  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off". 
  // Changes the output state according to the message
  if (String(topic) == "esp32/output") {
    Serial.print("Changing output to ");
    if(messageTemp == "on"){
      Serial.println("on");
//    digitalWrite(ledPin, HIGH);
    }
    else if(messageTemp == "off"){
      Serial.println("off");
//    digitalWrite(ledPin, LOW);
    }
  }
}

void registerdevice(){
StaticJsonDocument<1024> dev;
char cconfig[1024];

     dev["name"]            = "i2cgateha0101";
     dev["unique_id"]       = "switch.i2cgateha0101_identify";
     dev["entity_category"] = "switch";
     //dev["device_class"]  = "switch";
     dev["payload_on"]      = 1;
     dev["payload_off"]     = 0;
     dev["command_topic"]   = "homeassistant/switch/i2cgateha0101/set";
     dev["state_topic"]     = "homeassistant/switch/i2cgateha0101/state";
     
     String config = dev.as<String>();
     unsigned int csize = config.length();
     Serial.println("MQTT MSG: " + config);
     
     config.toCharArray(cconfig,csize);
     //char *subject = "homeassistant/switch/i2cgateha-01-01/config";
     client.publish("homeassistant/switch/i2cgateha0101/config",cconfig,csize);
     client.subscribe(dev["state_topic"]);
     client.subscribe(dev["command_topic"]);
}

void setup() {
  M5.begin();   //Init M5Paper.  
  M5.EPD.SetRotation(90);   //Set the rotation of the display.  
  M5.EPD.Clear(true);  //Clear the screen. 
  M5.RTC.begin();  //Init the RTC.  初始化 RTC
  canvas.createCanvas(540, 960);  //Create a canvas.  创建画布
  canvas.setTextSize(3); //Set the text size.  设置文字大小
  canvas.drawString("I2CGateHa",25,20);
  canvas.drawString("I2C Home Assistant Gateway", 25, 45);  //Draw a string.
  canvas.pushCanvas(0,0,UPDATE_MODE_DU4);  //Update the screen. 
    
  Serial.begin(115200);
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
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
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
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
   
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
 

  Serial.print("Setting up MQTT");
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  int value = check_for_switch(portA);

  }

void loop() {
  ArduinoOTA.handle();
   if (!client.connected()) {
    reconnect();
  }
  client.loop();
}
