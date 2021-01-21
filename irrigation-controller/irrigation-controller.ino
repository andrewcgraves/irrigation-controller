
/*

 Irrigation Controller

 Controlls our irrigation system using time and weather data. Can be used manually
 
 By Andrew Graves

 */


#include <SPI.h>
#include <WiFiNINA.h>
#include <Bridge.h>
#include <HttpClient.h>
#include <Process.h>
#include <TimeLib.h>
#include <ezButton.h>
#include <Adafruit_NeoPixel.h>

#include "arduino_secrets.h"
#define PIN 4
#define LED_COUNT 1

char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)
int status = WL_IDLE_STATUS;     // the WiFi radio's status
//int wateringStatus = 0;
int wateringZone = 0;
//bool programming = false;
unsigned long int secondsSinceProgramming;
int buttonPin = 8;

int wateringStatus = 0;
bool isWatering = false;
int wateringTime = 2;
int wateringStartTime;
int wateringEndTime;

int zone1Pin = 10;
int zone2Pin = 11;
int zone3Pin = 12;
int zone4Pin = 13;


ezButton button(7);
Adafruit_NeoPixel leds = Adafruit_NeoPixel(LED_COUNT, PIN, NEO_GRB + NEO_KHZ800);

void setup() {

  leds.setPixelColor(0, 0xFFFFFF); 
  leds.setBrightness(10);
  leds.show();
  Serial.begin(9600);
  leds.begin();
  
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  pinMode(buttonPin, OUTPUT);
  pinMode(zone1Pin, OUTPUT);
  pinMode(zone2Pin, OUTPUT);
  pinMode(zone3Pin, OUTPUT);
  pinMode(zone4Pin, OUTPUT);

  button.setDebounceTime(100);
  connectToWifi();
  getCurrentTime();

  Serial.println("Setup finished!");
}

void loop() {

  button.loop();

  if (button.isPressed()) {
     endWatering();
     wateringStatus >= 4 ? wateringStatus = 1 : wateringStatus += 1;
     secondsSinceProgramming = millis() / 1000;
  }
  

  if (wateringStatus == 0) {
     // STANDBY

    leds.setPixelColor(0, 0xEF5858); 
    leds.setBrightness(10);
    leds.show();
     
    
  } else if (wateringStatus > 0) {
      // WATERING

      if(isWatering) {
          // Set the LED color
          leds.setPixelColor(0, 0x0079BF); 
          leds.setBrightness(10);
          leds.show();

          // check the watering timer
          if (wateringEndTime <= minute()) {
              Serial.println("done watering");
              endWatering();
              wateringStatus = 0;
          }
          
      } else {
          leds.setPixelColor(0, 0xFFF04C); 
          leds.setBrightness(10);
          leds.show();
      }

      if (isWatering == false && secondsSinceProgramming && (millis() / 1000 ) - secondsSinceProgramming > 5) {
          Serial.print(millis() / 1000);
          Serial.print(" - ");
          Serial.println(secondsSinceProgramming);
          secondsSinceProgramming = NULL;
          Serial.println("Watering!");
          watering();

      } 
   }
}



// --------------------------------
// WATERING
// --------------------------------

void endWatering() {

    // Set all the zones to be off
    digitalWrite(zone1Pin, LOW);
    digitalWrite(zone2Pin, LOW);
    digitalWrite(zone3Pin, LOW);
    digitalWrite(zone4Pin, LOW);

    isWatering = false;
//    wateringStatus = 0;
    
}

void watering() {

  Serial.print("Watering zone: ");
  Serial.println(wateringStatus);

    endWatering();
    isWatering = true;


    // Set the starting and ending time for the timer
    wateringStartTime = minute();
    wateringEndTime = minute() + wateringTime;

    // Make sure it does not go over the 60 min limit
    if (wateringEndTime > 60) {
      wateringEndTime -= 60;
    }

    Serial.print("Started watering: ");
    Serial.println(wateringStartTime);
    Serial.print("Watering end time: ");
    Serial.println(wateringEndTime);

    if (wateringStatus == 1) {
      digitalWrite(zone1Pin, HIGH);
      
    } else if (wateringStatus == 2) {
      digitalWrite(zone2Pin, HIGH);
      
    } else if (wateringStatus == 3) {
      digitalWrite(zone3Pin, HIGH);

    } else if (wateringStatus == 4) {
      digitalWrite(zone4Pin, HIGH);

    }
  }


// --------------------------------
// TIME GATHERING
// --------------------------------


void checkCurrentTime() {
  Serial.print("time result:");
  Serial.println(hour());
  Serial.println(minute());
  Serial.println(second());
 }

void getCurrentTime() {
  Serial.println("Getting the current time");
  
  unsigned long epoch;
  epoch = WiFi.getTime();
  time_t epochTime = epoch;

  setTime(epochTime);
  adjustTime(3600 * -8);

  Serial.print("time result:");
  Serial.println(hour());
  Serial.println(minute());
  Serial.println(second());
  
 }


// --------------------------------
// WIFI CONNECTION
// --------------------------------


void connectToWifi() {
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
    
    // attempt to connect to WiFi network:
    while ( status != WL_CONNECTED) {
      Serial.print("Attempting to connect to WPA SSID: ");
      Serial.println(ssid);
    
      // Connect to WPA/WPA2 network:
      status = WiFi.begin(ssid, pass);
      // wait 5 seconds for connection:
      delay(5000);
    }
    
    // you're connected now, so print out the data:
    Serial.println("You're connected to the network");
    printCurrentNet();
    printWiFiData();
  }


// --------------------------------
// PRINTING AND DEBUGGING FUNCTIONS
// --------------------------------


void printWiFiData() {

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP address : ");
  Serial.println(ip);
  Serial.print("Subnet mask: ");
  Serial.println((IPAddress)WiFi.subnetMask());
  Serial.print("Gateway IP : ");
  Serial.println((IPAddress)WiFi.gatewayIP());
  
  // print your MAC address:
  byte mac[6];
  WiFi.macAddress(mac);
  Serial.print("MAC address: ");
  printMacAddress(mac);
}

void printCurrentNet() {

  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print the MAC address of the router you're attached to:
  byte bssid[6];
  WiFi.BSSID(bssid);
  Serial.print("BSSID: ");
  printMacAddress(bssid);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI): ");
  Serial.println(rssi);

  // print the encryption type:
  byte encryption = WiFi.encryptionType();
  Serial.print("Encryption Type: ");
  Serial.println(encryption, HEX);
  Serial.println();
}

void printMacAddress(byte mac[]) {
  for (int i = 5; i >= 0; i--) {
    if (mac[i] < 16) {
      Serial.print("0");
    }

    Serial.print(mac[i], HEX);
    if (i > 0) {
      Serial.print(":");
    }
  }
  Serial.println();
}
