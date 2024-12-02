/* 

Heartbeat breathing light based on Chrono Lumina Blinker example for MKR1010

*/

// library
#include <WiFiNINA.h>   
#include <PubSubClient.h>  // library to connect MQTT
#include <utility/wifi_drv.h>   // library to drive to RGB LED on the MKR1010
#include "arduino_secrets.h" 

#include <Wire.h>   // library to support I2C communication
#include "MAX30105.h"    // SparkFun MAX30102 library
#include "heartRate.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "FreeSansBold7pt7b.h"

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
const char* mqtt_server = "mqtt.cetools.org";  //MQTT server address
const int mqtt_port = 1884;
//const char*: store read-only strings, immutable;
//const int: store read-only integer values, immutable;
//int: store integer values, mutable;
//float: store decimals, mutable;

int status = WL_IDLE_STATUS;     // the Wifi radio's status
WiFiServer server(80);
WiFiClient wificlient;

//Manage MQTT message publishing and subscription
WiFiClient mkrClient;
PubSubClient client(mkrClient);

// MAX30102 setup
MAX30105 particleSensor;
const byte RATE_SIZE = 4; //Increase this for more averaging. 4 is good.
byte rates[RATE_SIZE]; //Array of heart rates
byte rateSpot = 0;
long lastBeat = 0; //Time at which the last beat occurred
//byte: a data type 0-255
//long: a time value

float currentBPM;
float temperature = 0.0;
int beatAvg;
int heartRate;

// control the printing of data once per second
unsigned long lastPrintTime = 0; 
const unsigned long printInterval = 500; // Serial monitor print interval
// Control the LED update frequency
unsigned long lastLedUpdateTime = 0;  
const unsigned long ledUpdateInterval = 1000; 
//unsigned long: store non-negative integer values, reinitialized each time they are used 每次使用重新初始化
//static: Used for variables that need to remember the state in the function, such as recording the last call time; Initialize once, retain value between function calls

// Click to switch control mode
bool clickDetected = false; // Detect if click occurs
unsigned long lastTapTime = 0; 
const unsigned long tapInterval = 500; // tap maximum interval time
const unsigned long debounceTime = 200; // prevent false triggering 避免误触
int mode = 0; // 0: heartbeat, 1: temperature
bool tapState = false; 
//bool: a data type, only true or false, Used in conditional judgment and control structures

// gradient control
int warmRed = 255;     
int warmGreen = 100;   
int warmBlue = 0;      
bool greenIncreasing = true; //Green channel increment sign

// OLED display setup
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// edit this for the light you are connecting to 
//char mqtt_topic_demo[] = "student/CASA0014/light/5/pixel/";
char mqtt_topic_demo[] = "student/CASA0014/light/16/pixel/";

void setup() {
  //Initialize the serial port
  Serial.begin(115200);
  delay(1000);

  // Initialize MAX30102
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 not detected. Check connections!");
    while (1);
  }
  Serial.println("Place your index finger on the sensor with steady pressure.");
  
  particleSensor.setup();  // Initialize the sensor with default values
  particleSensor.setPulseAmplitudeRed(0x3F); //Turn Red LED to low to indicate sensor is running
  particleSensor.setPulseAmplitudeIR(0x3F); 
  particleSensor.setPulseAmplitudeGreen(0); //Turn off Green LED
  particleSensor.enableDIETEMPRDY(); //Enable the temp ready interrupt.
  particleSensor.setSampleRate(100);         // increase sample rate
  particleSensor.setFIFOAverage(4);          // avg 4 samples
  particleSensor.enableFIFORollover();       // Avoid Data Loss
  Serial.println("Sensor Initialized");
  
  // Initialize OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (1); {
      delay(1000); 
    }
  }
  display.clearDisplay();  
  display.setTextSize(1);  
  display.setTextColor(SSD1306_WHITE);  
  display.setFont(&FreeSansBold7pt7b);
  display.setCursor(0,10);
  display.println(F("Hello ~"));
  display.setCursor(0,26);
  display.println(F("LumiHealth!"));
  display.display();  // display content

  //Initialize WIFI&MQTT
  WiFi.setHostname("Lumina ucbvjx4");
  startWifi();
  client.setServer(mqtt_server, mqtt_port);

  Serial.println("setup complete");
}

void loop() {
  // we need to make sure the arduino is still connected to the MQTT broker
  // otherwise we will not receive any messages
  if (!client.connected()) {
    reconnectMQTT();
  }
  if (WiFi.status() != WL_CONNECTED) {
    startWifi();
  }
  client.loop();
  
  static unsigned long lastSensorRead = 0;
  if (millis() - lastSensorRead >= 10) { // read data every 10ms
    lastSensorRead = millis();

  //different mode set
  long irValue = particleSensor.getIR();
  if (irValue < 50000) { 
        temperature = 0.0; 
        heartRate = 0;     
        updateOLED("No Finger", 0); 
        sendMQTTEffect(0, 0, 0, 0); 
    } else {
        if (mode == 0) {
          heartbeat();  
        } else if (mode == 1) {
          monitorTemperature();
        }
    }
  }
  static unsigned long lastOLEDUpdate = 0;
  if (millis() - lastOLEDUpdate >= 1000) { // update OLED every sec
    lastOLEDUpdate = millis();
    updateOLED(mode == 0 ? "Heart Rate" : "Temperature", 
              mode == 0 ? heartRate : temperature);
  }

  // Detect click to switch mode
  detectClick();   
}

void detectClick() {
  long irValue = particleSensor.getIR();  //get reflectance value of infrared light
  unsigned long currentTime = millis();  //millis(): Returns the time since the device was started

  // detect touch(ir && not detect touch before && the interval between double touch > debounceTime)
  if (irValue > 60000 && !tapState && currentTime - lastTapTime > debounceTime) {
    tapState = true; 
    lastTapTime = currentTime;  //Update lastTapTime
  }

  // detect release(ir && detect touch before)
  if (irValue < 50000 && tapState) {
    tapState = false; 

    // detect click(the interval between release and touch < tapInterval)
    if (currentTime - lastTapTime < tapInterval) {
      mode = (mode == 0) ? 1 : 0; // change mode
      Serial.print("Mode switched to: ");
      Serial.println(mode == 0 ? "Heart Rate Monitoring" : "Temperature Monitoring");
    }
    lastTapTime = currentTime;
  }
}

void updateOLED(const char* modeText, float value) {
    display.clearDisplay(); 
    display.setCursor(0, 10); 

    // display the name of mode
    display.print("Mode: ");
    display.println(modeText);

    if (value == 0) { 
        display.setCursor(0, 26); 
        display.println("Value: N/A");
    } else {
        if (strcmp(modeText, "Heart Rate") == 0) {
            display.setCursor(0, 26); 
            display.print("BPM: ");
            display.println(heartRate);
        } else if (strcmp(modeText, "Temperature") == 0) {
            display.setCursor(0, 26); 
            display.print("Temp: ");
            display.print(temperature);
            display.println(" C");
        }
    }

    display.display(); 
}


void monitorTemperature() {
  // get temperature and ir
  temperature = particleSensor.readTemperature();
  long irValue = particleSensor.getIR();

  // Update temperature data once per second
  static unsigned long lastPrintTime = 0;
  if (millis() - lastPrintTime >= printInterval) {
    lastPrintTime = millis();
    if (irValue < 50000) { 
            temperature = 0.0; 
    }
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println(" °C");
  }

  // Control LED
  if (irValue < 50000) {// Turn off all LED lights when no finger detected
    sendMQTTEffect(0, 0, 0, 0); 
  } else if (temperature > 33) {// coldcolor
    int R = random(0, 120);  
    int B = random(150, 255); 
    int G = random(50, 100);  
    int W = random(10, 80);
    sendMQTTEffect(R, G, B, W); 
    //sendMQTTEffect(0, 165, 255); // bluegreen
  } else {// warmcolor
    int R = random(200, 255); 
    int G = random(50, 150);  
    int B = random(0, 50);    
    int W = random(10, 80);  
    sendMQTTEffect(R, G, B, W);
    // if (greenIncreasing) {
    //   warmGreen += 5; 
    //   if (warmGreen >= 200) {
    //     greenIncreasing = false; 
    //   }
    // } else {
    //   warmGreen -= 5; 
    //   if (warmGreen <= 100) {
    //     greenIncreasing = true; 
    //   }
    // }

    // sendMQTTEffect(warmRed, warmGreen, warmBlue, 50); 
  }
}

void heartbeat(){
  // get ir value
  long irValue = particleSensor.getIR();

  static long smoothedIR = 0;
  static long previousIR = 0;
  smoothedIR = (smoothedIR * 0.95) + (irValue * 0.05); 
  long deltaIR = smoothedIR - previousIR;            
  previousIR = smoothedIR;

  // // ir
  // Serial.print("Raw IR Value: ");
  // Serial.print(irValue);
  // Serial.print(", Smoothed IR Value: ");
  // Serial.print(smoothedIR);
  // Serial.print(", IR Delta: ");
  // Serial.println(deltaIR);

  // Check for a heartbeat
  if (checkForBeat(irValue)== true) {
    Serial.println("Beat detected!");
    long delta = millis() - lastBeat; //Calculate interval between the current and previous heartbeat
    lastBeat = millis();

    currentBPM = 60 / (delta / 1000.0);

    // Filter abnormal BPM
    if (currentBPM > 20 && currentBPM < 255) {//Store BPM value in the array rates and loop over it
      rates[rateSpot++] = (byte)currentBPM; 
      rateSpot %= RATE_SIZE;              

      // Calculate average BPM
      beatAvg = 0;
      for (byte x = 0; x < RATE_SIZE; x++) {
        beatAvg += rates[x];
  
      }
      beatAvg /= RATE_SIZE;
      
      heartRate = beatAvg; // Updated value for LED control
    }
  }

  // // 添加 Avg Delta 计算
  // static long deltaSum = 0;
  // static int deltaCount = 0;

  // deltaSum += abs(deltaIR);
  // deltaCount++;

  // if (millis() % 1000 < 10) { // 每秒输出一次
  //   Serial.print("Avg Delta: ");
  //   Serial.println(deltaSum / deltaCount);
  //   deltaSum = 0;
  //   deltaCount = 0;
  // }

  if (millis() - lastPrintTime >= printInterval) {
    lastPrintTime = millis();
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

  //Update LED status every second
  static unsigned long lastLedUpdateTime = 0;
  if (millis() - lastLedUpdateTime >= ledUpdateInterval) {
    lastLedUpdateTime = millis();
    // Serial.print("heartRate=");
    // Serial.print(heartRate);

  if (irValue < 50000) {
      sendMQTTEffect(0, 0, 0, 0); 
  } else if (heartRate >= 95 && heartRate <= 120) {
      sendMQTTEffect(255, 0, 0, 0); // Red light
    } else if (heartRate >= 70 && heartRate < 95) {
      sendMQTTEffect(255, 255, 0, 0); // yellow light
    } else if (heartRate >= 40 && heartRate < 70) {
      sendMQTTEffect(0, 0, 255, 0);  // blue light
    } else {
      sendMQTTEffect(0, 0, 0, 0); // Turn off the light
    } 
  }
}

void sendMQTTEffect(int R, int G, int B, int W) {
  char mqtt_message[100];
  for (int i = 0; i < 12; i++) { // Control all 12 LEDs
    sprintf(mqtt_message, "{\"pixelid\": %d, \"R\": %d, \"G\": %d, \"B\": %d, \"W\": 0}", i, R, G, B, W);
    delay(10); 
    
    if (client.publish(mqtt_topic_demo, mqtt_message)) {
      //Serial.println("Message published");
    } else {
      Serial.println("Failed to publish message");
    }
  }
}

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