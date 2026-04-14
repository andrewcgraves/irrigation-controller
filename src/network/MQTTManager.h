#pragma once

#include "../hal/HAL.h"
#include "../zone/ZoneController.h"
#include <cstdint>
#include <cstring>

// --- Pure data types for MQTT command parsing (testable without network) ---

enum class MQTTCommandType : uint8_t {
    Unknown,
    TurnOn,
    TurnOff,
    SetSchedule,
    DeleteSchedule,
    GetStatus,
    Ping
};

struct MQTTCommand {
    MQTTCommandType type;
    ZoneRunQueue    queue;       // populated for TurnOn
    int             scheduleSlot; // populated for SetSchedule/DeleteSchedule
    uint8_t         scheduleDays;
    uint8_t         scheduleHour;
    uint8_t         scheduleMinute;
};

// Pure function — parses topic + payload into a command struct.
// Supports both legacy format (sprinkler/on/1,2,3 + plaintext duration)
// and new JSON format (sprinkler/cmd/on + JSON payload).
MQTTCommand parseMQTTMessage(const char* topic, const uint8_t* payload, unsigned int length);

// --- MQTTManager (hardware-dependent, only compiled for Arduino) ---

#ifdef ARDUINO

#include <WiFi.h>
#include <PubSubClient.h>

class MQTTManager {
public:
    MQTTManager(IHAL& hal);

    void begin(const char* ssid, const char* pass,
               const char* brokerIP, int brokerPort,
               const char* clientName, const char* mqttUser, const char* mqttPass);

    // Call every loop iteration
    void update();

    // Connection state
    bool isWifiConnected();
    bool isMQTTConnected();

    // Publishing
    void publishWateringStatus(bool on, int zoneId = 0, int remainingSec = 0);
    void publishZoneState(int zoneId, bool on);
    void publishOnline();

    // Home Assistant MQTT Discovery
    void publishDiscovery();

    // Get the last parsed command (consumed on read)
    bool hasCommand() const;
    MQTTCommand consumeCommand();

private:
    void connectWifi();
    void connectMQTT();
    void subscribe();

    static void staticCallback(char* topic, byte* payload, unsigned int length);
    void handleMessage(char* topic, byte* payload, unsigned int length);

    IHAL& _hal;

    // Connection config
    const char* _ssid;
    const char* _wifiPass;
    const char* _brokerIP;
    int         _brokerPort;
    const char* _clientName;
    const char* _mqttUser;
    const char* _mqttPass;

    // Network objects
    WiFiClient    _wifiClient;
    PubSubClient  _mqttClient;

    // Reconnect backoff
    unsigned long _lastReconnectAttempt;
    unsigned long _reconnectInterval;

    // WiFi state
    bool          _wifiConnecting;
    unsigned long _wifiConnectStart;

    // Command buffer (single command, consumed by main loop)
    MQTTCommand   _pendingCommand;
    bool          _hasCommand;

    // Singleton for static callback
    static MQTTManager* _instance;
};

#endif // ARDUINO
