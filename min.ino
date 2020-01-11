//#include <TimeLib.h>
#include <WiFiClientSecure.h> 
//#include <SoftwareSerial.h>

#if (ESP8266)
  #include <ESP8266WiFi.h>
  #include <BlynkSimpleEsp8266.h>
  #include <ESP8266HTTPClient.h>
  #include <ESP8266WebServer.h>
  #include <brzo_i2c.h>
  #include "SSD1306Brzo.h"
  
  SSD1306Brzo display(0x3c, D1, D2);
#elif (ESP32)
  #include <WiFi.h>
  #include <Blynk.h>
  #include <HTTPClient.h>
  #include <WebServer.h>
  // #include <Esp32WifiManager.h>
//  #include "BluetoothSerial.h"
//  #include "esp_bt_main.h"
//  #include "esp_bt_device.h"
  
  #include <Wire.h>
  #include "SSD1306.h"
  SSD1306  display(0x3c, 4, 15);//4 | 15 SDA SCL
// SH1106Wire display(0x3c, D3, D5);
#endif
#include "OLEDDisplayUi.h"

OLEDDisplayUi ui     ( &display );
//--------------------
//constant
#define FRAME_RATE 200L
//#define D5 (14)
//#define D6 (12)
#define BAUD_RATE 57600

const char *ssid = "FreeWifi";
const char *password = "87654231";
//TODO: list wifi

const char *host = "lman-test.firebaseapp.com";
const String subdirectory = "/save";
const int httpsPort = 443;  //HTTPS= 443 and HTTP = 80
const uint8_t fingerprint[20] = {0x46, 0xf2, 0xe8, 0x99, 0x89, 0x6d, 0x93, 0xc2, 0x44, 0xe0, 0x44, 0x22, 0xd0, 0x86, 0x9b, 0xf2, 0x56, 0xa7, 0x7c, 0x95};

//I/O
#define TURBIDITY_SENSOR 12
#define PH_SENSOR 13
#if (ESP8266)
  #define BUTTON D3 //D3 D6! D7! ---- gạt sang bên trái để bật, sang phải để reset
#elif (ESP32)
  #define BUTTON 18
#endif
#ifdef LED_BUILTIN
  #define LED_NOTIFY LED_BUILTIN
#else
  #define LED_NOTIFY 5
#endif

//bluetooth config
//#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
//  bool isBluetoothEnabled = false;
//#else
//  bool isBluetoothEnabled = true;
//#endif

//constants
#define MEASURING_SCREEN 0
#define RESULT_SCREEN 1

#define SCANNING 0
#define DISCONNECTED 1
#define CONNECTED 2
#define CONNECTING 3
//font 10, 16, 24

//variables
int wifi_status = DISCONNECTED;
int bluetooth_status = DISCONNECTED;
int frameController = MEASURING_SCREEN;

//frame tracking variables
int measuringCount = 0;
int resultCount = 0;

//data sending
bool isDataSent = false;
int last_pH = 0;
int last_TDS = 0;
int last_Turbidity = 0;

//Singleton Objects
BlynkTimer buttonHandler;
BlynkTimer wifiScanner;
BlynkTimer bluetoothHandler;
BlynkTimer dataSender;

//#if (ESP32)
//BluetoothSerial SerialBT;
//#endif
//----------------------------------

void setup() {
  pinMode(16, OUTPUT);
  digitalWrite(16, LOW);    // set GPIO16 low to reset OLED
  delay(50); 
  digitalWrite(16, HIGH); // while OLED is running, must set GPIO16 in high

  Serial.begin(BAUD_RATE);
  Serial.println("Starting...");

  //sensors
  pinMode(LED_NOTIFY, OUTPUT);
  digitalWrite(LED_NOTIFY, LOW);
  pinMode(BUTTON, INPUT);

  Serial1.begin(57600);
  Serial1.printf("AT+MODE=1\r\n");


  buttonHandler.setInterval(200, handleButtonClick);
  wifiScanner.setInterval(1000, scanWifi);
  dataSender.setInterval(10000, sendDataToExternal);

  //****Bluetooth****
//  #if (ESP32)
//    SerialBT.begin("Water Testing Instant");//-----------------------------------------------------------------------here
//    if (!isBluetoothEnabled
//        || !btStart()
//        || esp_bluedroid_init() != ESP_OK
//        || esp_bluedroid_enable() != ESP_OK
//    ) //Bluetooth available?
//    {
//      //Your chip doesn't support bluetooth
//      bluetooth_status = DISCONNECTED;
//    } else {
//      printDeviceAddress();
//      bluetoothHandler.setInterval(200L, bluetoothHandle);
//      bluetooth_status = SCANNING;
//    }
//  #endif
//  //***
  
  ui.init();
  display.flipScreenVertically();
}

void loop() {
  wifiScanner.run();
  buttonHandler.run();
  frameUpdate();
  // delay(FRAME_RATE);
}
//-------
void frameUpdate () {
  display.clear();
  
  switch (frameController) {
    case MEASURING_SCREEN:
      measuringScreen();
      break;
    case RESULT_SCREEN:
      resultScreen();
      break;
  }
  
  display.display();
}
//

int buttonStatus = HIGH; //invert
void handleButtonClick () {
  buttonStatus = digitalRead(BUTTON);
  if (buttonStatus == LOW) {
    if (frameController == MEASURING_SCREEN) {
      frameController = RESULT_SCREEN;
      measuringCount = resultCount = 0;
    } else {
      frameController = MEASURING_SCREEN;
      measuringCount = resultCount = 0;
    }
  }
}


//--------------Wifi Manager-----------------
int status = WL_IDLE_STATUS;
void scanWifi() {
  if (status == WL_IDLE_STATUS && wifi_status == DISCONNECTED) {
    // int numAvailableNetworks = WiFi.scanNetworks();
    // if (numAvailableNetworks < 0) { 
    //   Serial.println("Couldn't get a wifi connection");
    //   wifi_status = SCANNING;
    // } else {
    //   for (int thisWifi = 0; thisWifi < numAvailableNetworks; thisWifi++) {
    //     Serial.println(WiFi.SSID(thisWifi));
    //   }
      Serial.println("-------------------\nAttempt connecting to network: ");
      Serial.println(WiFi.SSID());
      WiFi.mode(WIFI_OFF);
      WiFi.mode(WIFI_STA);
      WiFi.begin(ssid, password);
      wifi_status = CONNECTING;
    // }
  } else if (WiFi.status() == WL_CONNECTED && wifi_status != CONNECTED) {
    wifi_status = CONNECTED;
    status = WL_CONNECTED;
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
}
bool sendDataToCloudViaWifi() {
  if (WiFi.status() == WL_CONNECTED) { //double check
    Serial.println("Data sending...");
    WiFiClientSecure httpsClient;
    #if (ESP8266)
      httpsClient.setFingerprint(fingerprint);
    // #elif (ESP32)
    //   httpsClient.setCACert(certificate);
    #endif
    httpsClient.setTimeout(15000);
    int r=0; //retry counter
    while((!httpsClient.connect(host, httpsPort)) && (r < 30)){
        // delay(100);
        Serial.print(".");
        r++;
    }
    if(r==30) {
      Serial.println("Connection failed");
    } else {
      Serial.println("Connected to web");
    }
    // String data = "{\"ph\":" + String(last_pH) + ",\"tds\":" + String(last_TDS) + ",\"turbidity\":" + String(last_Turbidity) + "}";
    String data = String(last_pH) + "-" + String(last_TDS) + "-" + String(last_Turbidity);
    
    httpsClient.print("POST " + subdirectory + " HTTP/1.1\r\n" +
                "Host: " + host + "\r\n" +
                "Content-Type: text/plain"+ "\r\n" +
                "Content-Length: " + String(data.length()) + "\r\n\r\n" +
                data + "\r\n" +
                "Connection: close\r\n\r\n");
    
    if (httpsClient.connected()) {
      Serial.println(data);
      Serial.println("data sent via wifi");
      return true;
    } else {
      Serial.println("data can't be sent via wifi");
      return false;
    }
  } else {
    return false;
  }
}
//------------Bluetooth Manager------------------
//#if (ESP32)
//  void bluetoothHandle () {
//    if (Serial.available()) {
//          SerialBT.write(Serial.read());
//    }
//    if (SerialBT.available()) {
//      bluetooth_status = CONNECTED;
//      Serial.println(SerialBT.read());
//    }
//  }
//  void printDeviceAddress() {
//    const uint8_t* point = esp_bt_dev_get_address();
//    for (int i = 0; i < 6; i++) {
//      char str[3];
//      sprintf(str, "%02X", (int)point[i]);
//      Serial.print(str);
//      if (i < 5){
//        Serial.print(":");
//      }
//    }
//  }
//  bool sendDataViaBluetooth() {
//    String data = "{\"ph\":" + String(last_pH) + ",\"tds\":" + String(last_TDS) + ",\"turbidity\":" + String(last_Turbidity) + "}";
//    return false;
//  }
//#endif

//--------------------View--------------------
void measuringScreen() {
  UIOverlay();
  readSensor();
  displayData();
  measuringCount++;
  if (measuringCount > 4000) {
    measuringCount = resultCount = 0;
    frameController = RESULT_SCREEN;
  }
}
void resultScreen() {
  if (resultCount % 700 > 400) {
    UIOverlay();
    //pH
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_24);
    display.drawString(0 , 30, String(last_pH));
    //TDS
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_24);
    display.drawString(60 , 30, String(last_TDS));
    //Turbidity
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.setFont(ArialMT_Plain_24);
    display.drawString(125 , 30, String(last_Turbidity));
  }
  resultCount++;
  if (resultCount > 300) {
    measuringCount = resultCount = 0;
    // frameController = MEASURING_SCREEN;
  }
  if (!isDataSent) {
    dataSender.run();
    isDataSent = true;
  }
};

//---
void UIOverlay() {
  //draw pH --- TDS --- Turbidity
  display.setTextAlignment(TEXT_ALIGN_LEFT);//pH
  display.setFont(ArialMT_Plain_10);
  display.drawString(5 , 5, "pH");
  display.setTextAlignment(TEXT_ALIGN_CENTER); //TDS
  display.setFont(ArialMT_Plain_10);
  display.drawString(60 , 5, "TDS");
  display.setTextAlignment(TEXT_ALIGN_RIGHT); //Turbidity
  display.setFont(ArialMT_Plain_10);
  display.drawString(125 , 5, "Turbidity");
}
void displayData() {
  //pH
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_24);
  display.drawString(0 , 30, String(last_pH));
  //TDS
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_24);
  display.drawString(60 , 30, String(last_TDS));
  //Turbidity
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.setFont(ArialMT_Plain_24);
  display.drawString(125 , 30, String(last_Turbidity));
}


//---------------Model------------------
int sampleCount = 0;
int total_pH = 0;
int total_TDS = 0;
int total_Turbidity = 0;
void readSensor() {
  //TDS
  Serial1.printf("AT+VALUE=?\r\n");
  String tds_measured = Serial.readStringUntil('\n');
  //nếu nhúng vào nước
  if (tds_measured.toInt() != 0) {
    total_TDS += tds_measured.toInt();
    //Turbidity
    total_Turbidity += analogRead(TURBIDITY_SENSOR);
    //pH
    total_pH += analogRead(PH_SENSOR);
    
    sampleCount++;
  }

  if (sampleCount == 40) {
    last_pH = total_pH / 40;
    last_TDS = total_TDS / 40;
    last_Turbidity = total_Turbidity / 40;
    sampleCount = 0;
  }
}

void sendDataToExternal() {
  if(!sendDataToCloudViaWifi()) {
//    #if (ESP32)
//      if(sendDataViaBluetooth) {
//        Serial.println("Data sent via bluetooth");
//      } else
//    #endif
      Serial.println("Data can't be sent");
  }
}
