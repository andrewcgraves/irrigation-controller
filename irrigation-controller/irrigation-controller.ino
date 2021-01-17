
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

#include "arduino_secrets.h"
char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)
int status = WL_IDLE_STATUS;     // the WiFi radio's status

Process date;                 // process used to get the date
int hours, minutes, seconds;  // for the results
int lastSecond = -1;          // need an impossible value for comparison

void setup() {

  // Initialize serial and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  //  run an initial date process. Should return:
  //  hh:mm:ss :
  if (!date.running()) {
    date.begin("date");
    date.addParameter("+%T");
    date.run();
  }

  connectToWifi();
//  getCurrentTime();
}

void loop() {

 
  getCurrentTime();
  


}

void getCurrentTime() {
  HttpClient client;
  client.get("http://www.arduino.cc/asciilogo.txt");

  while (client.available()) {
    char c = client.read();
    Serial.print(c);
    }

    Serial.flush();

    delay(5000);
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
 

//#include <Process.h>
//
//Process date;                 // process used to get the date
//int hours, minutes, seconds;  // for the results
//int lastSecond = -1;          // need an impossible value for comparison
//
//void setup() {
//
//  Bridge.begin();        // initialize Bridge
//  Serial.begin(9600);    // initialize serial
//  while (!Serial);              // wait for Serial Monitor to open
//  Serial.println("Time Check");  // Title of sketch
//
//  // run an initial date process. Should return:
//  // hh:mm:ss :
//  if (!date.running()) {
//    date.begin("date");
//    date.addParameter("+%T");
//    date.run();
//
//  }
//}
//
//void loop() {
//
//  if (lastSecond != seconds) { // if a second has passed
//    // print the time:
//    if (hours <= 9) {
//      Serial.print("0");  // adjust for 0-9
//    }
//
//    Serial.print(hours);
//    Serial.print(":");
//    
//    if (minutes <= 9) {
//      Serial.print("0");  // adjust for 0-9
//    }
//
//    Serial.print(minutes);
//    Serial.print(":");
//    
//    if (seconds <= 9) {
//      Serial.print("0");  // adjust for 0-9
//    }
//
//    Serial.println(seconds);
//
//    // restart the date process:
//    if (!date.running()) {
//      date.begin("date");
//      date.addParameter("+%T");
//      date.run();
//    }
//  }
//
//  //if there's a result from the date process, parse it:
//  while (date.available() > 0) {
//
//    // get the result of the date process (should be hh:mm:ss):
//    String timeString = date.readString();
//
//    // find the colons:
//    int firstColon = timeString.indexOf(":");
//    int secondColon = timeString.lastIndexOf(":");
//
//    // get the substrings for hour, minute second:
//    String hourString = timeString.substring(0, firstColon);
//    String minString = timeString.substring(firstColon + 1, secondColon);
//    String secString = timeString.substring(secondColon + 1);
//
//    // convert to ints,saving the previous second:
//    hours = hourString.toInt();
//    minutes = minString.toInt();
//    lastSecond = seconds;          // save to do a time comparison
//    seconds = secString.toInt();
//
//  }
//}
