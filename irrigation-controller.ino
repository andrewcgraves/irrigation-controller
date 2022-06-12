#include <WiFiNINA.h>
#include <ezButton.h>
#include <Adafruit_NeoPixel.h>
#include <TimeLib.h>
#include <SPI.h>
#include <PubSubClient.h>
// #include <pthread.h>

// Import unique information and secrets
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

// Vars
int MAX_WATERING_LENGTH = 20;
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

// bool isAutomatic;
// bool isWatering;
bool timeReset = false;
bool isConnecting = false;

// int lastZone;
// int currentZone;
// int triggerTime = 6;

// int selectingLastZone;
int selectingCurrentZone;

int zonesToWater[6];

void setup() {
    leds.begin();
    manualButton.setDebounceTime(100);
    autoButton.setDebounceTime(100);

    // Setting all the pins to be output pins
    pinMode(zone1Pin, OUTPUT);
    pinMode(zone2Pin, OUTPUT);
    pinMode(zone3Pin, OUTPUT);

    endWatering();
    // setLedColor(0xE34C00, 10);
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
        // triggerAuto(false);
        // isAutomatic = false;
        selectingCurrentZone >= NUMBER_OF_WATERING_ZONES ? selectingCurrentZone = 1 : selectingCurrentZone++;
        // lastZone >= NUMBER_OF_WATERING_ZONES ? currentZone = 1 : currentZone = lastZone + 1;
        secondsSinceProgramming = millis() / 1000;
        // trigger = TriggeredCause::Manual;

    }

    // LOOPING THROUGH TO CHECK THE TIME
    if (zonesToWater[0] != 0 && wateringEndTime <= now()) {

        // TODO: This may not be needed. I think its hitting it every time
        // watering is done, so cycle through the zones and set the watering to be the next zone
        endWatering();
        cycleArrayItems(zonesToWater);

        if(zonesToWater[0] != 0) {
            Serial.println("Next Zone: " + zonesToWater[0]);
            startWatering(zonesToWater[0]);

        } else {
            // watering is done
            Serial.println("Watering is done.");
            wateringLengthMin = DEFAULT_WATERING_LENGTH_MIN;
        }
    }

    if (zonesToWater[0] != 0 && secondsSinceProgramming != -1 && (millis() / 1000 ) - secondsSinceProgramming > 5) {
        secondsSinceProgramming = -1;
        zonesToWater[0] = selectingCurrentZone;
        startWatering(zonesToWater[0]);
    }
}

// -------------------------------------------------------------------------
// HELPER FUNCTIONS
// -------------------------------------------------------------------------

// Sets the LED color of connected RGB LED
void setLedColor(long hex, int intensity) {
    leds.setPixelColor(0, hex); 
    leds.setBrightness(10);
    leds.show();
}

// Fills an int array with 0
void clearIntArray(int array[]) {
    int arraySize = sizeof(array)/sizeof(array[0]);
    if (arraySize > 1) {
        for (size_t i = 0; i < arraySize; i++) {
            array[i] = 0;
        }
    }
}

// TODO: redo the queuing here.

template <typename T>
// Move all array items up a position, get rid of the first one.
void cycleArrayItems(T array[]) {
    int arraySize = sizeof(array)/sizeof(array[0]);
    if (arraySize > 1) {
        for (size_t i = 0; i < arraySize; i++) {
            array[i] = array[i+1];
            // memcpy(array[i], array[i+1], sizeof(array[i+1]));
        }
        array[arraySize-1] = 0;
    } else {
        array[0] = 0;
        return;
    }
}

// Called whenever an MQTT message is recieved to any topic we are listening to
void callback(char* topic, byte* payload, unsigned int length) {
    String charPayload;
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (int i=0;i<length;i++) {
        charPayload += (char)payload[i];
        Serial.print((char)payload[i]);
    }
    Serial.println("");

    // The only commands that should pass through here should be preceded with "sprinkler/..."
    // Calls can take the following formats
    // {on/off}/{zones}
    // {auto/set_schedule} (set_schedule=data would be a schedule based like the following) DATA={1(0,0,0,0,0,0,0), 2(0,0,0,0,0,0,0), etc...} where each zone has a weekly schedule

    char* tokens[10];
    char* result = strtok(topic, "/");
    int c = 0;

    while (result != NULL) {
        tokens[c] = result;
        // Serial.println("Parsing token: ");
        // Serial.print(tokens[c]);
        // Serial.print("\n");
        result = strtok(NULL, "/");
        c++;
    }

    Serial.print("\n");

    if (strcmp("on", tokens[1]) == 0) {
        Serial.println("Switching on...");
        client.publish("ret", payload, length);

        // char* tokens2[NUMBER_OF_WATERING_ZONES];
        char* result2 = strtok(tokens[2], ",");
        c = 0;

        while (result2 != NULL) {
            // tokens2[c] = result2;
            zonesToWater[c] = atol(result2);
            Serial.println("Parsing token: ");
            Serial.print(zonesToWater[c]);
            Serial.print("\n");
            result2 = strtok(NULL, ",");
            c++;
        }

        int wateringTime = charPayload.toInt();
        char* wateringZones;
        
        if (zonesToWater[0] <= NUMBER_OF_WATERING_ZONES && wateringTime <= MAX_WATERING_LENGTH) {
            // Now we trigger the sprinkler system

            // currentZone = wateringZone;
            wateringLengthMin = wateringTime;
            trigger = TriggeredCause::Manual;
            startWatering(zonesToWater[0]);
            // triggerWatering();

        } else {
            Serial.println("Invalid Input. Zone must be <= " + NUMBER_OF_WATERING_ZONES);
            Serial.print("and Time must be <= " + MAX_WATERING_LENGTH);
        }

    } else if (strcmp("off", tokens[1]) == 0) {
        Serial.println("Switching off...");
        // currentZone = 0;

        wateringLengthMin = DEFAULT_WATERING_LENGTH_MIN;
        endWatering();

    } else if (strcmp("auto", tokens[1]) == 0) {
        Serial.println("Switching to auto...");
        // currentZone = 1;
        // triggerAuto(true);

    } else if (strcmp("set_schedule", tokens[1]) == 0) {
        // TODO - to implement
        
    } else {
        // should never arrive here...
        Serial.println("FORBIDDEN");
    }
    
    // For testing...
    // client.publish("ret", topic);
}

void reconnect() {
    // Loop until we're reconnected
    while (!client.connected()) {
        Serial.print("Attempting MQTT connection...");
        // Attempt to connect
        if (client.connect("core-mosquitto")) {
            Serial.println("connected");
            // subscribe to a topic
            client.subscribe("sprinkler/#");
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}
// --------------------------------------------------------
// WATERING FUNCTIONS
// --------------------------------------------------------

// Triggers the watering of a specified zone
void startWatering(int toWater) {
    setLedColor(0x00FFFF, 10);
    wateringStartTime = now();
    wateringEndTime = now() + (wateringLengthMin * 60);

    switch (toWater) {
        case 1: 
            Serial.println("Watering 1");
            digitalWrite(zone1Pin, LOW);
            break;
        case 2:
            Serial.println("Watering 2");
            digitalWrite(zone2Pin, LOW);
            break;
        case 3:
            Serial.println("Watering 3");
            digitalWrite(zone3Pin, LOW);
            break;
        default:
            Serial.println("INVALID ZONE");
            setLedColor(0xFFFFFF, 10);
    }
}

void endWatering() {
    // set the color
    setLedColor(0xffffff, 10);

    // clear all the valves 
    digitalWrite(zone1Pin, HIGH);
    digitalWrite(zone2Pin, HIGH);
    digitalWrite(zone3Pin, HIGH);
    digitalWrite(zone4Pin, HIGH);
    // Serial.println("Ending Watering...");

    lastWateringTime = now();
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

    // Red loading color
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

    setLedColor(0xFFFFFF, 10);
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
