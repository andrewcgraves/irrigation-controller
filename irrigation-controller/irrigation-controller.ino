
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
#include <SerLCD.h>
#include <Wire.h>

#include "arduino_secrets.h"
#define PIN 4
#define LED_COUNT 1

char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)
int status = WL_IDLE_STATUS;     // the WiFi radio's status
unsigned long int secondsSinceProgramming;

int wateringZone = 0;
int lastWateringZone = -1;

bool isAutomatic = false;
bool isWatering = false;
int wateringTime = 1;
int wateringStartTime;
int wateringEndTime;

int lastHour;
int lastMinute;
int lastSecond;

int lastWatered;
bool timeReset = false;
bool autoTriggered = false;

int zone1Pin = 10;
int zone2Pin = 11;
int zone3Pin = 12;
int zone4Pin = 13;

ezButton button(7);
ezButton autoButton(8);
Adafruit_NeoPixel leds = Adafruit_NeoPixel(LED_COUNT, PIN, NEO_GRB + NEO_KHZ800);
SerLCD lcd;


// --------------------------------
// SETUP
// --------------------------------

void setup() {

  // General preperation
  Wire.begin();
  lcd.begin(Wire);
  leds.begin();
  button.setDebounceTime(100);
  autoButton.setDebounceTime(100);

  lcd.setBacklight(255, 255, 255); //Set backlight to bright white
  lcd.setContrast(5); //Set contrast. Lower to 0 for higher contrast.

  // Setting all the pins to default
//  pinMode(buttonPin, OUTPUT);
  pinMode(zone1Pin, OUTPUT);
  pinMode(zone2Pin, OUTPUT);
  pinMode(zone3Pin, OUTPUT);
  pinMode(zone4Pin, OUTPUT);
  endWatering();

  // Setting the LED color while booting up
  leds.setPixelColor(0, 0xE34C00); 
  leds.setBrightness(10);
  leds.show();

  // Starting serial
  Serial.begin(9600);
  
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  // Functions to start out
  connectToWifi();
  getCurrentTime();

  Serial.println("Setup finished!");
}


// --------------------------------
// LOOP
// --------------------------------

void loop() {

//  lcd.clear(); //Clear the display - this moves the cursor to home position as well
//  lcd.print("Setup finished!");

  button.loop();
  autoButton.loop();
  

  // Check to see if the minute hour and second are at the hour
  if (second() == 0 && minute() == 0 && !timeReset) {
    getCurrentTime();
    timeReset = true;
  }

  if (second() == 1) {
    timeReset = false;  
  }

  // Whenever the button is pressed. Exit the current watering mode and go into programming mode.
  if (button.isPressed()) {
     endWatering();
     wateringZone >= 4 ? wateringZone = 1 : wateringZone += 1;
     secondsSinceProgramming = millis() / 1000;
     isAutomatic = false;
  }

  if (autoButton.isPressed()) {
     isAutomatic = true;

     leds.setPixelColor(0, 0x43FC18); 
     leds.setBrightness(10);
     leds.show();

     Serial.println("Automatic!");
  }

  // Schedule programming
  if (second() == 0 && minute() == 21 && hour() == 21 && isAutomatic && !autoTriggered) {
    
    endWatering();
    wateringZone = 1;
    watering();
    autoTriggered = true;
  }

  if (second() == 1 && minute() == 21 && hour() == 21) {
    autoTriggered = false;
  }

  // badly programmed schedule
  

  if (wateringZone == 0) {
     // STANDBY

     if (!isAutomatic) {
      leds.setPixelColor(0, 0xFFFFFF); 
      leds.setBrightness(10);
      leds.show();
     }

     if (wateringZone != lastWateringZone) {
       lcd.clear(); //Clear the display - this moves the cursor to home position as well
       lcd.print("System Ready...");

       lastWateringZone = 0;
      }
     
    
  } else if (wateringZone > 0) {
      // WATERING

      if(isWatering) {
          // Set the LED color
          leds.setPixelColor(0, 0x0079BF); 
          leds.setBrightness(10);
          leds.show();

          // check the watering timer
          if (wateringEndTime <= minute()) {

              if (isAutomatic && wateringZone < 2) {
                lcd.clear(); //Clear the display - this moves the cursor to home position as well
                lcd.print("Watering next zone!");
                Serial.println("Watering Next Zone");
                
                lastWateringZone = wateringZone;
                wateringZone += 1;
                endWatering();
                watering();
                
              } else {
                lcd.clear(); //Clear the display - this moves the cursor to home position as well
                lcd.print("Watering ended!");
                Serial.println("done watering");
                
                isAutomatic = false;
                endWatering();
                lastWateringZone = wateringZone;
                wateringZone = 0;
              }
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
    digitalWrite(zone1Pin, HIGH);
    digitalWrite(zone2Pin, HIGH);
    digitalWrite(zone3Pin, HIGH);
    digitalWrite(zone4Pin, HIGH);

    isWatering = false;
    
}

void watering() {

  Serial.print("Watering zone: ");
  Serial.println(wateringZone);

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

   lcd.clear(); //Clear the display - this moves the cursor to home position as well
   lcd.print("Watering zone ");
   lcd.print(wateringZone);
   lcd.setCursor(0,1);
   lcd.print("Ends at ");

   if (wateringEndTime < wateringStartTime) {
    lcd.print(hour()+1); 
   } else {
    lcd.print(hour()); 
   }

   lcd.print(":");
   lcd.print(wateringEndTime);
          

  if (wateringZone == 1) {
    digitalWrite(zone1Pin, LOW);
    
  } else if (wateringZone == 2) {
    digitalWrite(zone2Pin, LOW);
    
  } else if (wateringZone == 3) {
    digitalWrite(zone3Pin, LOW);

  } else if (wateringZone == 4) {
    digitalWrite(zone4Pin, LOW);

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
  
 }


// --------------------------------
// WIFI CONNECTION
// --------------------------------


void connectToWifi() {
    lcd.clear(); //Clear the display - this moves the cursor to home position as well
    lcd.print("Connecting to WiFi...");
  
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

  lcd.clear(); //Clear the display - this moves the cursor to home position as well
  lcd.print("Connected!");

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
