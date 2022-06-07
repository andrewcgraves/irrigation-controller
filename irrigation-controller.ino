#include <WiFiNINA.h>
#include <ezButton.h>
#include <Adafruit_NeoPixel.h>
#include <TimeLib.h>
#include <SPI.h>
#include <PubSubClient.h>
// #include <pthread.h>

#include "arduino_secrets.h"

// Led Config Stuff
#define LED_PIN 4
#define LED_COUNT 1
Adafruit_NeoPixel leds = Adafruit_NeoPixel(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// Network Config Information
char ssid[] = SECRET_SSID;               // your network SSID (name)
char pass[] = SECRET_PASS;               // your network password (use for WPA, or use as key for WEP)
char brokerIP[] = SECRET_BROKER_IP;
char brokerPort[] = SECRET_BROKER_PORT;
int WifiRadioStatus = WL_IDLE_STATUS;    // the WiFi radio's status
WiFiServer wifiServer(80);

// Vars
int DEFAULT_WATERING_LENGTH_MIN = 10;
int wateringLengthMin = DEFAULT_WATERING_LENGTH_MIN;
int NUMBER_OF_WATERING_ZONES = 3;
time_t controllerStartTime;
time_t uptime;

enum class TriggeredCause { Auto, Manual, Cascading };
TriggeredCause trigger;

long wateringStartTime;
long wateringEndTime;
long lastWateringTime;
long int secondsSinceProgramming = -1;
unsigned long lastMillis = 0;

bool isAutomatic;
bool isWatering;
bool timeReset = false;
bool isConnecting = false;

int lastZone;
int currentZone;
int triggerTime = 6;

// Zone pins
int zone1Pin = 10;
int zone2Pin = 11;
int zone3Pin = 12;
int zone4Pin = 13;

// Setup for Buttons
ezButton manualButton(7);
ezButton autoButton(8);

// MQTT things
WiFiClient net;
PubSubClient client(net);

void setup() {
    leds.begin();
    manualButton.setDebounceTime(100);
    autoButton.setDebounceTime(100);

    // Setting all the pins to be output pins
    pinMode(zone1Pin, OUTPUT);
    pinMode(zone2Pin, OUTPUT);
    pinMode(zone3Pin, OUTPUT);

    endWatering();
    setLedColor(0xE34C00, 10);
    Serial.begin(9600);

    while (!Serial) {
      ; // wait for serial port to connect. Needed for native USB port only
    }

    connectToWifi();
    client.setServer(SECRET_BROKER_IP, 1883);
    client.setCallback(callback); 
    getCurrentTime();
    Serial.println("Setup finished!");
    controllerStartTime = now();
    setLedColor(0xFFFFFF, 10);
}

void loop() {
    manualButton.loop();
    autoButton.loop();
    uptime = now() - controllerStartTime;

    // Connect to wifi if the connection drops
    if (!isConnecting && second() == 0 && (WiFi.status() != WL_CONNECTED)) {
        connectToWifi();
    }

    // check the mqtt connection
    if (!client.connected()) {
        reconnect();
    }

    client.loop();

    // Check to see if the minute hour and second are at the hour and reset the time accordingly
    // This aims to keep the time accurate because it will slowly fall behind
    if (second() == 0 && minute() == 0 && !timeReset) {
        getCurrentTime();
        timeReset = true;
    }

    if (second() == 1) {
        timeReset = false;
    }

    // TRIGGERING CHECKS
    if (manualButton.isPressed()) {
        endWatering();
        triggerAuto(false);
        isAutomatic = false;
        lastZone >= NUMBER_OF_WATERING_ZONES ? currentZone = 1 : currentZone = lastZone + 1;
        setLedColor(0xFFFF00, 10);
        secondsSinceProgramming = millis() / 1000;
        trigger = TriggeredCause::Manual;

    } else if (autoButton.isPressed()) {
        endWatering();

        if (isAutomatic) {
            triggerAuto(false);
            Serial.println("Automatic off");

        } else {
            triggerAuto(true);
            Serial.println("Automatic on");
        }

    } else if (isAutomatic && !isWatering && second() == 0 && minute() == 0 && hour() == triggerTime && ((day()%2) == 0) ) {
        Serial.println("Schedule triggered");
        endWatering();
        currentZone = 1;
        triggerWatering();
        trigger = TriggeredCause::Auto;
    }

    // LOOPING THROUGH TO CHECK THE TIME
    if (isWatering && wateringEndTime <= now()) {

        if (((trigger == TriggeredCause::Cascading) || (isAutomatic && trigger == TriggeredCause::Auto)) && currentZone < NUMBER_OF_WATERING_ZONES) {
            Serial.println("Watering Next Zone");
            endWatering();
            currentZone = lastZone + 1;
            triggerWatering();

        } else if (isAutomatic && currentZone >= NUMBER_OF_WATERING_ZONES) {
            endWatering();
            setLedColor(0x00FF00, 10);
            Serial.println("back to auto rest state");
        } else {
            endWatering();
            wateringLengthMin = DEFAULT_WATERING_LENGTH_MIN;
            Serial.println("done watering");
            setLedColor(0xFFFFFF, 10);
        }
    }

    if (isWatering == false && secondsSinceProgramming != -1 && (millis() / 1000 ) - secondsSinceProgramming > 5) {
        secondsSinceProgramming = -1;
        Serial.println("Watering!");
        triggerWatering();
    }
}

// -------------------------------------------------------------------------
// HELPER FUNCTIONS
// -------------------------------------------------------------------------

void setLedColor(long hex, int intensity) {
    leds.setPixelColor(0, hex); 
    leds.setBrightness(10);
    leds.show();
}

void callback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (int i=0;i<length;i++) {
        Serial.print((char)payload[i]);
    }
    Serial.println();

    // Todo - convert this to something that can be extended
    // if (topic == "sprinkler/zone1") {
        
    // } else if (topic == "sprinkler/zone2") {

    // } else if (topic == "sprinkler/zone3") {

    // }
    

    client.publish("ret", topic);
}

void reconnect() {
    // Loop until we're reconnected
    while (!client.connected()) {
        Serial.print("Attempting MQTT connection...");
        // Attempt to connect
        if (client.connect("core-mosquitto")) {
            Serial.println("connected");
            // Once connected, publish an announcement...
            client.publish("outTopic","hello world");
            // ... and resubscribe
            client.subscribe("spinkler/#");
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

// WATERING FUNCTIONS

void triggerWatering() {
    isWatering = true;
    wateringStartTime = now();
    wateringEndTime = now() + (wateringLengthMin * 60);

    Serial.print("Watering Zone: ");
    Serial.println(currentZone);

    setLedColor(0x00FFFF, 10);

    switch (currentZone) {
        case 1: 
            digitalWrite(zone1Pin, LOW);
            break;
        case 2:
            digitalWrite(zone2Pin, LOW);
            break;
        case 3:
            digitalWrite(zone3Pin, LOW);
            break;
        default:
            Serial.println("INVALID ZONE");
            setLedColor(0xFFFFFF, 10);
    }
}

void triggerAuto(bool state) {

    if (state) {
        setLedColor(0x00FF00, 10);
    } else {
        setLedColor(0xFFFFFF, 10);
    }

    isAutomatic = state;
}

void endWatering() {
    lastZone = currentZone;
    lastWateringTime = now();

    // Reset the timer to default if triggered manually
    if (trigger == TriggeredCause::Manual) {
        wateringLengthMin = DEFAULT_WATERING_LENGTH_MIN;
    }

    currentZone = 0;
    isWatering = false;

    // Set all the zones to be off
    digitalWrite(zone1Pin, HIGH);
    digitalWrite(zone2Pin, HIGH);
    digitalWrite(zone3Pin, HIGH);
    digitalWrite(zone4Pin, HIGH);
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
    isConnecting = true;

    // check for the WiFi module:
    if (WiFi.status() == WL_NO_MODULE) {
        Serial.println("Communication with WiFi module failed!");
        // don't continue
        while (true);
    }

    setLedColor(0xd71414, 10);
        
    // Check firmware
    String fv = WiFi.firmwareVersion();
    if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
        Serial.println("Please upgrade the firmware");
    }
        
    // attempt to connect to WiFi network:
    while ( WifiRadioStatus != WL_CONNECTED ) {
        Serial.print("Attempting to connect to WPA SSID: ");
        Serial.println(ssid);
        
        // Connect to WPA/WPA2 network:
        WifiRadioStatus = WiFi.begin(ssid, pass);
        // wait 5 seconds for connection:
        delay(5000);
    }

    // you're connected now, so print out the data:
    Serial.println("You're connected to the network");
    isConnecting = false;
    printWiFiData();

    if (isWatering) {
        setLedColor(0x00FFFF, 10);
    } else if (isAutomatic) {
        setLedColor(0x00FF00, 10);
    } else {
        setLedColor(0xFFFFFF, 10);
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

    // print the MAC address of the router you're attached to:
    byte bssid[6];
    WiFi.BSSID(bssid);
    Serial.print("BSSID: ");

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
