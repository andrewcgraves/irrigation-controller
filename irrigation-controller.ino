#include <TimeLib.h>

unsigned long lastTrigger = 0;
unsigned long lastHourLog = 0;
const unsigned long hourInMillis = 3600000UL; // 1 hour in milliseconds
const unsigned long dayInMillis = 86400000UL; // 24 hours in milliseconds

const unsigned long sixHoursInMillis = 21600000UL;
const unsigned long threeDaysInMillis = dayInMillis * 3; // 3 days in milliseconds
// const unsigned long threeDaysInMillis = 10000 * 3; // 3 days in milliseconds


// Constants
int MAX_WATERING_LENGTH = 20;
int DEFAULT_WATERING_LENGTH_MIN = 10;
int wateringLengthMin = DEFAULT_WATERING_LENGTH_MIN;
int NUMBER_OF_WATERING_ZONES = 3;
int PING_INTERVAL = 30; // in seconds
int MAX_PONG_INTERVAL = 60 * 2; // in seconds

// Variables
time_t controllerStartTime;
time_t uptime;
long wateringStartTime;
long wateringEndTime = -1;
long lastWateringTime;
long int secondsSinceProgramming = -1;
unsigned long lastMillis = 0;
int status = 0;

// Zone pins
int zone1Pin = 2;
int zone2Pin = 3;
int zone3Pin = 4;
int zone4Pin = 5;

// Handeling the Queue of zones
struct WateringZoneLinkedList {
    int zone;
    WateringZoneLinkedList *next;
};

// int zonesToWater[6];
WateringZoneLinkedList *nextZone = NULL;

void setup() {
  Serial.begin(9600);
  // Initialize your hardware here
  // Setting all the pins to be output pins
  pinMode(zone1Pin, OUTPUT);
  pinMode(zone2Pin, OUTPUT);
  pinMode(zone3Pin, OUTPUT);

  endWatering();

  // while (!Serial) {
  //   ; // wait for serial port to connect. Needed for native USB port only
  // }

  triggerSprinklers();
}

void loop() {
  digitalWrite(LED_BUILTIN, HIGH);  // turn the LED on (HIGH is the voltage level)
  delay(500);    
  unsigned long currentTime = millis();
  
  // Check if 1 hour has passed since last hourly log
  if (currentTime - lastHourLog >= hourInMillis) {
    logHourlyUpdate();
    lastHourLog = currentTime;
  }
  
  // Check if 3 days have passed since last trigger
  if (currentTime - lastTrigger >= sixHoursInMillis) {
    triggerSprinklers();
  }

  // TODO: there may be some weird behavior here
  if (wateringEndTime != -1 && wateringEndTime <= now()) {
    endWatering();

    if (nextZone != NULL && nextZone->zone > 0) {
        // Previous node is done watering, and there is another to water
        startWatering(nextZone->zone);

        // Increment the zone
        struct WateringZoneLinkedList* prevNode = nextZone;
        nextZone = nextZone->next;
        free(prevNode);

    } else if (nextZone == NULL) {
        Serial.println("Watering is done.");
        wateringLengthMin = DEFAULT_WATERING_LENGTH_MIN;
        wateringEndTime = -1;

    }
  }

  if (wateringEndTime != -1) {
    Serial.print(now());
    Serial.print(" - ");
    Serial.println(wateringEndTime);

    digitalWrite(LED_BUILTIN, LOW);   // turn the LED off by making the voltage LOW
    delay(100);
    digitalWrite(LED_BUILTIN, HIGH);   // turn the LED off by making the voltage LOW
    delay(100);
  }

  digitalWrite(LED_BUILTIN, LOW);   // turn the LED off by making the voltage LOW
  delay(1000);                      // wait for a second
}

void triggerSprinklers() {
    freeLinkedList(nextZone);
    nextZone = NULL;

    struct WateringZoneLinkedList* currentHead = NULL;
    struct WateringZoneLinkedList* newNode = NULL;

    freeLinkedList(nextZone);
    nextZone = NULL;

    newNode = (struct WateringZoneLinkedList*) malloc(sizeof(struct WateringZoneLinkedList));
    if (nextZone == NULL) { nextZone = newNode; } else { currentHead->next = newNode; }
    newNode->next = NULL;
    newNode->zone = 1;
    currentHead = newNode;

    currentHead->next = NULL;

    struct WateringZoneLinkedList* newNode2 = NULL;

    newNode2 = (struct WateringZoneLinkedList*) malloc(sizeof(struct WateringZoneLinkedList));
    newNode2->next = NULL;
    newNode2->zone = 2;
    newNode->next = newNode2;
    
    char* wateringZones;

    // TODO: Redo the start watering trigger for MQTT to wrap it up into the main looping body of the arduino code
    if (nextZone->zone > 0 && nextZone->zone <= NUMBER_OF_WATERING_ZONES) {
        startWatering(nextZone->zone);
        nextZone = nextZone->next;

    } else {
        Serial.print("\nInvalid Input. Zone must be <= ");
        Serial.print(NUMBER_OF_WATERING_ZONES);
        Serial.print(" and Time must be <= ");
        Serial.print(MAX_WATERING_LENGTH);
        Serial.print("... Value: ");
        Serial.print(nextZone->zone);
        Serial.print("\n");
    }
    
    lastTrigger = millis();
}

void logHourlyUpdate() {
  unsigned long hours = millis() / hourInMillis;
  Serial.print("Hour ");
  Serial.print(hours);
  Serial.println(" - System running");
}

// HELPER FUNCTIONS

// Free all links of a linked list
void freeLinkedList(WateringZoneLinkedList* list) {
    struct WateringZoneLinkedList* currentNode = list;
    struct WateringZoneLinkedList* temp;

    while (currentNode != NULL) {
        temp = currentNode;
        currentNode = currentNode->next;
        temp->next = NULL;
        free(temp);
    }
}

// Adds a node to an already existing linked-list. The new head is returned
WateringZoneLinkedList* addNodeToLinkedList(WateringZoneLinkedList* node, int value) {
    Serial.print("Adding a node with zone of ");
    Serial.println(value);

    struct WateringZoneLinkedList* newNode = (struct WateringZoneLinkedList*) malloc(sizeof(struct WateringZoneLinkedList));
    newNode->next = NULL;

    newNode->zone = value;
    node->next = newNode;

    return newNode;
}

// Triggers the watering of a specified zone
void startWatering(int toWater) {
    wateringStartTime = now();
    wateringEndTime = now() + (wateringLengthMin * 60);
    switch (toWater) {
      case 1: 
          Serial.println("Watering 1");
          wateringEndTime = now() + (1 * 60);
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
    }
}

// Resets all the pins (ends the watering cycle)
void endWatering() {

    // clear all the valves 
    digitalWrite(zone1Pin, HIGH);
    digitalWrite(zone2Pin, HIGH);
    digitalWrite(zone3Pin, HIGH);
    digitalWrite(zone4Pin, HIGH);

    lastWateringTime = now();
}


