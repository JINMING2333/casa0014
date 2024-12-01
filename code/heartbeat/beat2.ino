/* 

Chrono Lumina Blinker example for MKR1010

*/


#include <WiFiNINA.h>   
#include <PubSubClient.h>
#include <utility/wifi_drv.h>   // library to drive to RGB LED on the MKR1010
#include "arduino_secrets.h" 

#include <Wire.h>
#include "MAX30105.h"         // SparkFun MAX30102 库
#include "heartRate.h"

/*
**** please enter your sensitive data in the Secret tab/arduino_secrets.h
**** using format below

#define SECRET_SSID "ssid name"
#define SECRET_PASS "ssid password"
#define SECRET_MQTTUSER "user name - eg student"
#define SECRET_MQTTPASS "password";
 */

const char* ssid          = SECRET_SSID;
const char* password      = SECRET_PASS;
const char* mqtt_username = SECRET_MQTTUSER;
const char* mqtt_password = SECRET_MQTTPASS;
const char* mqtt_server = "mqtt.cetools.org";
const int mqtt_port = 1884;
int status = WL_IDLE_STATUS;     // the Wifi radio's status
float currentBPM;
WiFiServer server(80);
WiFiClient wificlient;

WiFiClient mkrClient;
PubSubClient client(mkrClient);

// MAX30102 设置
MAX30105 particleSensor;
const byte RATE_SIZE = 4; //Increase this for more averaging. 4 is good.
byte rates[RATE_SIZE]; //Array of heart rates
byte rateSpot = 0;
long lastBeat = 0; //Time at which the last beat occurred

float beatsPerMinute;
int beatAvg;
int heartRate;

// 新增：用于控制每秒打印一次数据
unsigned long lastPrintTime = 0;       // 上次打印时间
const unsigned long printInterval = 1000; // 串口打印间隔
unsigned long lastLedUpdateTime = 0;   // 上次 LED 更新时间
const unsigned long ledUpdateInterval = 1000; // LED 更新间隔

// 定义更新间隔和时间变量
const unsigned long bpmUpdateInterval = 1000; // 更新心率数据间隔（毫秒）
static unsigned long lastBpmUpdateTime = 0;   // 上次更新心率的时间

// edit this for the light you are connecting to 
//char mqtt_topic_demo[] = "student/CASA0014/light/5/pixel/";
char mqtt_topic_demo[] = "student/CASA0014/light/15/pixel/";

void setup() {
  // Start the serial monitor to show output
  Serial.begin(115200);
  delay(1000);

  // 初始化MAX30102
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 not detected. Check connections!");
    while (1);
    }
    
  particleSensor.setup();  // Initialize the sensor with default values
  particleSensor.setPulseAmplitudeRed(0x0A); //Turn Red LED to low to indicate sensor is running
  particleSensor.setPulseAmplitudeGreen(0); //Turn off Green LED
  Serial.println("Sensor Initialized");
  
  //初始化WIFI&MQTT
  WiFi.setHostname("Lumina ucbvjx4");
  startWifi();
  client.setServer(mqtt_server, mqtt_port);

  Serial.println("setup complete");
}

void loop() {
  // we need to make sure the arduino is still connected to the MQTT broker
  // otherwise we will not receive any messages
 // 确保连接到 MQTT 和 WiFi
   // 确保连接到 MQTT 和 WiFi
  if (!client.connected()) {
    reconnectMQTT();
  }
  if (WiFi.status() != WL_CONNECTED) {
    startWifi();
  }
  client.loop();

  heartbeat();
  
}

void heartbeat(){
  // 持续采样传感器数据
  long irValue = particleSensor.getIR();

  // 检查是否有心跳
  if (checkForBeat(irValue)) {
    long delta = millis() - lastBeat; // 时间间隔（毫秒）
    lastBeat = millis();

    currentBPM = 60 / (delta / 1000.0);

    // 过滤异常值
    if (currentBPM > 20 && currentBPM < 255) {
      rates[rateSpot++] = (byte)currentBPM; // 存储有效心率
      rateSpot %= RATE_SIZE;               // 循环存储

      // 计算平均心率
      beatAvg = 0;
      for (byte x = 0; x < RATE_SIZE; x++) {
        beatAvg += rates[x];
      }
      beatAvg /= RATE_SIZE;
      heartRate = beatAvg; // 更新用于 LED 控制的心率值
    }
  }

  // 每秒更新一次心率数据
  // if (millis() - lastBpmUpdateTime >= bpmUpdateInterval) {
  //   lastBpmUpdateTime = millis(); // 更新上次更新心率的时间

  if (millis() - lastPrintTime >= printInterval) {
    lastPrintTime = millis();
    // 输出更新后的心率数据
    Serial.print("IR=");
    Serial.print(irValue);
    Serial.print(", BPM=");
    Serial.print(currentBPM);
    Serial.print(", Avg BPM=");
    Serial.print(beatAvg);

    if (irValue < 50000) {
      Serial.print(" No finger?");
    }

    Serial.println();
  }

  //每秒更新LED状态
  static unsigned long lastLedUpdateTime = 0;
  if (millis() - lastLedUpdateTime >= ledUpdateInterval) {
    lastLedUpdateTime = millis();

    Serial.print("heartRate=");
    Serial.print(heartRate);

  if (irValue < 50000) {
      sendMQTTEffect(0, 0, 0); // 没有手指时关闭所有LED灯
  } else if (heartRate >= 100 && heartRate <= 120) {
      sendMQTTEffect(0, 0, 255); // Red light, fast flashing
    } else if (heartRate >= 80 && heartRate < 100) {
      sendMQTTEffect(255, 0, 0); // Green light, medium flashing
    } else if (heartRate >= 40 && heartRate < 80) {
      sendMQTTEffect(255, 255, 0); // Yellow light, slow flashing
    } else {
      sendMQTTEffect(0, 0, 0); // Turn off the light
    } 
  }
}


void sendMQTTEffect(int R, int G, int B) {
  char mqtt_message[100];
  for (int i = 0; i < 12; i++) { // Control all 12 LEDs
    sprintf(mqtt_message, "{\"pixelid\": %d, \"R\": %d, \"G\": %d, \"B\": %d, \"W\": 0}", i, R, G, B);
    delay(10); // Control flashing speed
    if (client.publish(mqtt_topic_demo, mqtt_message)) {
      //Serial.println("Message published");
    } else {
      Serial.println("Failed to publish message");
    }
  }
}

//light control functions
void pulsewhite(){
  sprintf(mqtt_topic_demo, "student/CASA0014/light/%d/all/",16);
  char mqtt_message[100];
  sprintf(mqtt_message,"{\"method\":\"pulsewhite\"}");
  delay(10);
}

void clearAllLEDs(){
  sprintf(mqtt_topic_demo, "student/CASA0014/light/%d/all/",15);
  char mqtt_message[50];
  sprintf(mqtt_message,"{\"method\":\"clear\"}");
  delay(10);
}

// void alllighton(){
//   sprintf(mqtt_topic_demo, "student/CASA0014/light/%d/all/",15);
//   char mqtt_message[50];
//   sprintf(mqtt_message,"{\"method\":\"clear\"}");
//   delay(10);
// }

void startWifi(){
    
  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }
  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
  }

  // Function for connecting to a WiFi network
  // is looking for UCL_IoT and a back up network (usually a home one!)
  int n = WiFi.scanNetworks();
  Serial.println("Scan done");
  if (n == 0) {
    Serial.println("no networks found");
  } else {
    Serial.print(n);
    Serial.println(" networks found");
    // loop through all the networks and if you find UCL_IoT or the backup - ssid1
    // then connect to wifi
    Serial.print("Trying to connect to: ");
    Serial.println(ssid);
    for (int i = 0; i < n; ++i){
      String availablessid = WiFi.SSID(i);
      // Primary network
      if (availablessid.equals(ssid)) {
        Serial.print("Connecting to ");
        Serial.println(ssid);
        WiFi.begin(ssid, password);
        while (WiFi.status() != WL_CONNECTED) {
          delay(600);
          Serial.print(".");
        }
        if (WiFi.status() == WL_CONNECTED) {
          Serial.println("Connected to " + String(ssid));
          break; // Exit the loop if connected
        } else {
          Serial.println("Failed to connect to " + String(ssid));
        }
      } else {
        Serial.print(availablessid);
        Serial.println(" - this network is not in my list");
      }

    }
  }


  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

}

void reconnectMQTT() {
  if (WiFi.status() != WL_CONNECTED){
    startWifi();
  } else {
    //Serial.println(WiFi.localIP());
  }

  // Loop until we're reconnected
  while (!client.connected()) {    // while not (!) connected....
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "LuminaSelector";
    clientId += String(random(0xffff), HEX);
    
    // Attempt to connect
    if (client.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("connected");
      // ... and subscribe to messages on broker
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, int length) {
  // Handle incoming messages
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

}