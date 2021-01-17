
/*

  Time Check

 Gets the time from Linux via Bridge then parses out hours,

 minutes and seconds using a YunShield/Yún.

 created  27 May 2013

 modified 21 June 2013

 By Tom Igoe

 This example code is in the public domain.

 http://www.arduino.cc/en/Tutorial/TimeCheck

 */


#include <SPI.h>
#include <WiFiNINA.h>
#include <Bridge.h>
#include <HttpClient.h>
#include <Process.h>
#include <TimeLib.h>

#include "arduino_secrets.h"
char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)
int status = WL_IDLE_STATUS;     // the WiFi radio's status

void setup() {

  // Initialize serial and wait for port to open:
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);
  Bridge.begin();
  digitalWrite(13, HIGH);
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  connectToWifi();
  getCurrentTime();
}

void loop() {

// Initialize the client library
  HttpClient client;

  // Make a HTTP request:
  client.get("http://www.arduino.cc/asciilogo.txt");

  // if there are incoming bytes available
  // from the server, read them and print them:
  while (client.available()) {
    char c = client.read();
    Serial.print(c);
  }
  Serial.flush();

  delay(5000);

}

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
  

//  HttpClient client;
//  client.get("http://www.arduino.cc/asciilogo.txt");
//
//  while (client.available()) {
//    char c = client.read();
//    Serial.print(c);
//    }
//
//    Serial.flush();
//
//    delay(5000);
  }

// Connects to wifi
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
