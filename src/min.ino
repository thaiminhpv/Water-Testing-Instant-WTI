#include <ArduinoJson.h>
//#include <TimeLib.h>
#include <WiFiClientSecure.h> 
#include <SoftwareSerial.h>

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
#define BAUD_RATE 57600


//web settings
const char *penID = "pen1";
// const char *ssid = "FreeWifi";
// const char *password = "87654231";

const char *ssid = "Hackathon 2019";
const char *password = "Hackathon@2019";
//TODO: list wifi
//https://watertestinginstant.firebaseapp.com/new-noti
const char *host = "watertestinginstant.firebaseapp.com";
const String subdirectory = "/new-noti";
const int httpsPort = 443;  //HTTPS= 443 and HTTP = 80
const uint8_t fingerprint[20] = {0x46, 0xf2, 0xe8, 0x99, 0x89, 0x6d, 0x93, 0xc2, 0x44, 0xe0, 0x44, 0x22, 0xd0, 0x86, 0x9b, 0xf2, 0x56, 0xa7, 0x7c, 0x95};

//location
const char *hostGoogle = "www.googleapis.com";
const String API_KEY_GOOGLE = "/geolocation/v1/geolocate?key=AIzaSyDfg_X_OquHuYwe6iTsQlUC6y7DPNkVrXc";
//https://www.googleapis.com/geolocation/v1/geolocate?key=AIzaSyDfg_X_OquHuYwe6iTsQlUC6y7DPNkVrXc



//I/O
#define TURBIDITY_SENSOR 12
#define PH_SENSOR 13

#define SCANNING 0
#define DISCONNECTED 1
#define CONNECTED 2
#define CONNECTING 3
//font 10, 16, 24

//variables
int wifi_status = DISCONNECTED;
bool isMeasuring = true;

//frame tracking variables
int measuringCount = 0;
int resultCount = 0;

//data sending
bool isDataSent = false;
int last_pH = 0;
int last_TDS = 0;
int last_Turbidity = 0;

//Singleton Objects
// BlynkTimer buttonHandler;
BlynkTimer wifiScanner;
// BlynkTimer bluetoothHandler;
// BlynkTimer dataSender;

SoftwareSerial TdsSensor(14, 23);

void setup() {
  pinMode(16, OUTPUT);
  digitalWrite(16, LOW);    // set GPIO16 low to reset OLED
  delay(50); 
  digitalWrite(16, HIGH); // while OLED is running, must set GPIO16 in high

  Serial.begin(BAUD_RATE);
  Serial.println("Starting...");

  TdsSensor.begin(9600);
  TdsSensor.printf("AT+MODE=1\r\n");


  // buttonHandler.setInterval(200, handleButtonClick);
  wifiScanner.setInterval(1000, scanWifi);
  // dataSender.setInterval(10000, sendDataToExternal);

  ui.init();
  // display.flipScreenVertically();
}

void loop() {
  frameUpdate();
}
//-------
void frameUpdate () {
  display.clear();
  
  if (isMeasuring) {
    measuringScreen();
  } else {
    resultScreen();
  }
  display.display();
}

//--------------Wifi Manager-----------------
int status = WL_IDLE_STATUS;
void scanWifi() {
  if (status == WL_IDLE_STATUS && wifi_status == DISCONNECTED) {
      Serial.println("-------------------\nAttempt connecting to network: ");
      Serial.println(WiFi.SSID());
      WiFi.mode(WIFI_OFF);
      WiFi.mode(WIFI_STA);
      WiFi.begin(ssid, password);
      wifi_status = CONNECTING;
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
    #if (ESP8266)
      httpsClient.setFingerprint(fingerprint);
    // #elif (ESP32)
    //   httpsClient.setCACert(certificate);
    #endif

    //get location
    Serial.println("Get location...");
    WiFiClientSecure httpsClientLocation;
    httpsClientLocation.setTimeout(15000);
    int r=0; //retry counter
    while((!httpsClientLocation.connect(hostGoogle, httpsPort)) && (r < 30)){
        // delay(100);
        Serial.print(".");
        r++;
    }
    if(r==30) {
      Serial.println("Get location failed");
    } else {
      Serial.println("Get location!");
    }
    httpsClientLocation.print("POST " + API_KEY_GOOGLE + " HTTP/1.1\r\n" +
                "Host: " + hostGoogle + "\r\n" +
                "Content-Type: application/json"+ "\r\n" +
                "Content-Length: " + "0" + "\r\n\r\n" +
                "\r\n" +
                "Connection: close\r\n\r\n");

    while (httpsClientLocation.connected()) {
      String line = httpsClientLocation.readStringUntil('\n');
      if (line == "\r") {
        Serial.println("Location received");
        break;
      }
    }

    Serial.println("location is: ");
    String location;
    while(httpsClientLocation.available()){        
      location += httpsClientLocation.readStringUntil('\n');
    }
    location = location.substring(0, location.lastIndexOf("HTTP") - 1);

    //submit data to server
    WiFiClientSecure httpsClient;
    httpsClient.setTimeout(15000);
    r=0; //retry counter
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
    String data = String(last_pH) + "_" + String(last_TDS) + "_" + String(last_Turbidity) + 
                  "_" + String(penID) + "_" + location;
    
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
//--------------------View--------------------
void measuringScreen() {
  UIOverlay();
  readSensor();
  displayData();
  measuringCount++;
  if (measuringCount > 750) {
    measuringCount = resultCount = 0;
    isMeasuring = false;
  }
}
void resultScreen() {
  if (resultCount % 1000 > 650) {
    UIOverlay();
    displayData();

    wifiScanner.run();
    if (!isDataSent) {
      sendDataToExternal();
    }
  }
  resultCount++;
  if (resultCount > 3500) {
    measuringCount = resultCount = 0;
    // frameController = MEASURING_SCREEN;
  }
  
};

//---
void UIOverlay() {
  //draw pH --- TDS --- Turbidity
  // display.setTextAlignment(TEXT_ALIGN_LEFT);//pH
  // display.setFont(ArialMT_Plain_10);
  // display.drawString(5 , 5, "pH");
  // display.setTextAlignment(TEXT_ALIGN_CENTER); //TDS
  // display.setFont(ArialMT_Plain_10);
  // display.drawString(60 , 5, "TDS");
  // display.setTextAlignment(TEXT_ALIGN_RIGHT); //Turbidity
  // display.setFont(ArialMT_Plain_10);
  // display.drawString(125 , 5, "Turbidity");
}
void displayData() {
  //pH
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_24);
  display.drawString(0 , 10, last_pH == 14 ? "N/A" : String(last_pH));
  //TDS
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_24);
  display.drawString(60 , 10, String(last_TDS));
  //Turbidity
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.setFont(ArialMT_Plain_24);
  display.drawString(125 , 10, String(last_Turbidity));
}


//---------------Model------------------
int sampleCount = 0;
int total_pH = 0;
int total_TDS = 0;
int total_Turbidity = 0;
void readSensor() {
  //TDS
  TdsSensor.printf("AT+VALUE=?\r\n");
  String tds_measured = TdsSensor.readStringUntil('\n');
  //nếu nhúng vào nước
  if (tds_measured.toInt() != 0) {/////---------------------------------------------------
    // last_TDS = tds_measured.toInt();
    // //Turbidity
    // last_Turbidity = 1000 - (analogRead(TURBIDITY_SENSOR) * 1000 / 4095);
    // // Serial.println(last_Turbidity);
    // //pH
    // last_pH = analogRead(PH_SENSOR)
    //               * 5 / 4096 //analog -> millivolt
    //               * 3.5      //millivolt -> true value
    //                 ;
    total_TDS += tds_measured.toInt();
    //Turbidity
    total_Turbidity += 1000 - (analogRead(TURBIDITY_SENSOR) * 1000 / 4095);
    // Serial.println(last_Turbidity);
    //pH
    total_pH += analogRead(PH_SENSOR)
                  * 5 / 4096 //analog -> millivolt
                  * 3.5      //millivolt -> true value
                    ;
    sampleCount++;

  }

  if (sampleCount == 30) {
    //average out
    last_pH = total_pH / 30 ;
    last_TDS = total_TDS / 30;
    last_Turbidity = total_Turbidity / 30;
    // Serial.println(last_Turbidity);

    total_pH = total_TDS = total_Turbidity = 0;
    sampleCount = 0;
  }
}

void sendDataToExternal() {
  if(!(isDataSent = sendDataToCloudViaWifi())) {
      Serial.println("Data can't be sent");
  }
}
