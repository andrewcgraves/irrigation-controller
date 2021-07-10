#include <WiFiNINA.h>
#include <ezButton.h>
#include <Adafruit_NeoPixel.h>

#include "arduino_secrets.h"

// Led Config Stuff
#define LED_PIN 4
#define LED_COUNT 1
Adafruit_NeoPixel leds = Adafruit_NeoPixel(LED_COUNT, PIN, NEO_GRB + NEO_KHZ800);

// Network Config Information
char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)
int WifiRadioStatus = WL_IDLE_STATUS;     // the WiFi radio's status
WiFiServer wifiServer(80);
WiFiClient wifiClient;

// Vars
long WATERING_LENGTH_MIN = 10;
long controllerStartTime;

long wateringStartTime;
long wateringEndTime;
long lastWateringTime;

bool isAutomatic;
bool isWatering;

int lastZone;
int currentZone;

// Zone pins
int zone1Pin = 10;
int zone2Pin = 11;
int zone3Pin = 12;
int zone4Pin = 13;

// Setup for Buttons
ezButton manualButton(7);
ezButton autoButton(8);
SerLCD lcd;


void setup() {
    leds.begin();
    manualButton.setDebounceTime(100);
    autoButton.setDebounceTime(100);

    // Setting all the pins to be output pins
    pinMode(zone1Pin, OUTPUT);
    pinMode(zone2Pin, OUTPUT);
    pinMode(zone3Pin, OUTPUT);

    setLedColor(0xE34C00, 10);
    Serial.begin(96000);
    wifiClient = wifiServer.available();

    while (!Serial) {
        // wait for serial port to connect
    }

    connectToWifi()
    getCurrentTime()
    Serial.println("Setup finished!");
}

void loop() {
    manualButton.loop();
    autoButton.loop();

    // Check to see if the minute hour and second are at the hour and reset the time accordingly
    // This aims to keep the time accurate because it will slowly fall behind
    if (second() == 0 && minute() == 0 && !timeReset) {
        getCurrentTime();
        timeReset = true;
    }

    if (second() == 1) { timeReset = false }


    // TRIGGERING CHECKS
    if (manualButton.isPressed()) {

    } else if (autoButton.isPressed()) {

    } else if (second() == 0 && minute() == 0 && hour() == 21 && isAutomatic && !isWatering) {
        
    } else if (wifiClient) {
        Serial.println("new client");
        String req = wifiClient.readStringUntil('\r');

        wifiClient.flush()
        wifiClient.stop()
        Serial.println("client disconnected");

        if (req.indexOf("/on") != -1) {

        } else if (req.indexOf("/off") != -1) {

        } else if (req.indexOf("/auto") != -1) {

        }
    }
}

// -------------------------------------------------------------------------
// HELPER FUNCTIONS
// -------------------------------------------------------------------------

void setLedColor(hex, intensity) {
    leds.setPixelColor(0, hex); 
    leds.setBrightness(10);
    leds.show();
}

// WATERING FUNCTIONS

void triggerWatering() {

}

void endWatering() {

}

// TIME GATHERING 

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
    adjustTime(3600 * -7);
}

// Wifi Connecting and debugging functions VVVVVVVVVV

void connectToWifi() {
    // check for the WiFi module:
    if (WiFi.status() == WL_NO_MODULE) {
        Serial.println("Communication with WiFi module failed!");
        // don't continue
        while (true);
    }
        
    // Check firmware
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
    printWiFiData();
    }
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