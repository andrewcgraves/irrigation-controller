#include "MQTTManager.h"
#include <cstdlib>
#include <cstring>

// ============================================================
// Pure parsing function — no hardware dependency, fully testable
// ============================================================

// Helper: tokenize a topic string by '/' into an array
static int tokenizeTopic(const char* topic, char* buf, size_t bufLen,
                         const char* tokens[], int maxTokens) {
    if (!topic || !buf || bufLen == 0) return 0;
    size_t len = strlen(topic);
    if (len >= bufLen) len = bufLen - 1;
    memcpy(buf, topic, len);
    buf[len] = '\0';

    int count = 0;
    char* p = buf;
    while (*p && count < maxTokens) {
        tokens[count++] = p;
        char* slash = strchr(p, '/');
        if (slash) {
            *slash = '\0';
            p = slash + 1;
        } else {
            break;
        }
    }
    return count;
}

// Helper: copy payload to null-terminated string
static void payloadToString(const uint8_t* payload, unsigned int length,
                            char* buf, size_t bufLen) {
    size_t copyLen = length < bufLen - 1 ? length : bufLen - 1;
    memcpy(buf, payload, copyLen);
    buf[copyLen] = '\0';
}

MQTTCommand parseMQTTMessage(const char* topic, const uint8_t* payload, unsigned int length) {
    MQTTCommand cmd;
    cmd.type = MQTTCommandType::Unknown;
    cmd.queue.clear();
    cmd.scheduleSlot = -1;
    cmd.scheduleDays = 0;
    cmd.scheduleHour = 0;
    cmd.scheduleMinute = 0;

    if (!topic) return cmd;

    char topicBuf[128];
    const char* tokens[10];
    int tokenCount = tokenizeTopic(topic, topicBuf, sizeof(topicBuf), tokens, 10);

    if (tokenCount < 2) return cmd;

    // Payload as string
    char payloadStr[256];
    payloadToString(payload, length, payloadStr, sizeof(payloadStr));

    // ---- New JSON format: sprinkler/cmd/{action} ----
    if (tokenCount >= 3 && strcmp(tokens[0], "sprinkler") == 0
                        && strcmp(tokens[1], "cmd") == 0) {

        const char* action = tokens[2];

        if (strcmp(action, "on") == 0) {
            cmd.type = MQTTCommandType::TurnOn;
            // Parse JSON: {"zones":[{"zone":1,"duration":10},{"zone":2,"duration":15}]}
            // Minimal manual JSON parsing to avoid ArduinoJson dependency in native tests
            // Look for "zone": and "duration": pairs
            const char* p = payloadStr;
            while (*p) {
                // Find "zone":
                const char* zoneKey = strstr(p, "\"zone\"");
                if (!zoneKey) break;
                const char* colon = strchr(zoneKey + 6, ':');
                if (!colon) break;
                int zoneId = atoi(colon + 1);

                // Find "duration":
                const char* durKey = strstr(colon, "\"duration\"");
                if (!durKey) break;
                const char* durColon = strchr(durKey + 10, ':');
                if (!durColon) break;
                int duration = atoi(durColon + 1);

                if (zoneId >= 1 && zoneId <= ZONE_MAX && duration >= 1 && duration <= 30) {
                    cmd.queue.add(static_cast<uint8_t>(zoneId),
                                  static_cast<uint16_t>(duration));
                }

                p = durColon + 1;
            }

            if (cmd.queue.count == 0) {
                cmd.type = MQTTCommandType::Unknown;
            }

        } else if (strcmp(action, "off") == 0) {
            cmd.type = MQTTCommandType::TurnOff;

        } else if (strcmp(action, "status") == 0) {
            cmd.type = MQTTCommandType::GetStatus;

        } else if (tokenCount >= 4 && strcmp(action, "schedule") == 0) {
            if (strcmp(tokens[3], "set") == 0) {
                cmd.type = MQTTCommandType::SetSchedule;
            } else if (strcmp(tokens[3], "delete") == 0) {
                cmd.type = MQTTCommandType::DeleteSchedule;
            }
        }
        return cmd;
    }

    // ---- Legacy format: sprinkler/{action}/{zones} ----
    if (strcmp(tokens[0], "sprinkler") == 0) {
        if (strcmp(tokens[1], "on") == 0 && tokenCount >= 3) {
            cmd.type = MQTTCommandType::TurnOn;
            int duration = atoi(payloadStr);
            if (duration < 1) duration = 10; // default
            if (duration > 30) duration = 30;

            // Parse comma-separated zones from tokens[2]
            // Need a mutable copy since tokens[2] points into topicBuf
            char zonesBuf[32];
            size_t zLen = strlen(tokens[2]);
            if (zLen >= sizeof(zonesBuf)) zLen = sizeof(zonesBuf) - 1;
            memcpy(zonesBuf, tokens[2], zLen);
            zonesBuf[zLen] = '\0';

            char* zoneToken = strtok(zonesBuf, ",");
            while (zoneToken) {
                int zoneId = atoi(zoneToken);
                if (zoneId >= 1 && zoneId <= ZONE_MAX) {
                    cmd.queue.add(static_cast<uint8_t>(zoneId),
                                  static_cast<uint16_t>(duration));
                }
                zoneToken = strtok(nullptr, ",");
            }

            if (cmd.queue.count == 0) {
                cmd.type = MQTTCommandType::Unknown;
            }

        } else if (strcmp(tokens[1], "off") == 0) {
            cmd.type = MQTTCommandType::TurnOff;

        } else if (strcmp(tokens[1], "pong") == 0) {
            cmd.type = MQTTCommandType::Ping;

        } else if (strcmp(tokens[1], "reset") == 0) {
            // Legacy reset — treat as off
            cmd.type = MQTTCommandType::TurnOff;
        }
    }

    return cmd;
}

// ============================================================
// MQTTManager implementation — Arduino only
// ============================================================

#ifdef ARDUINO

#include "../../include/config.h"

MQTTManager* MQTTManager::_instance = nullptr;

MQTTManager::MQTTManager(IHAL& hal)
    : _hal(hal)
    , _ssid(nullptr)
    , _wifiPass(nullptr)
    , _brokerIP(nullptr)
    , _brokerPort(1883)
    , _clientName(nullptr)
    , _mqttUser(nullptr)
    , _mqttPass(nullptr)
    , _mqttClient(_wifiClient)
    , _lastReconnectAttempt(0)
    , _reconnectInterval(MQTT_RECONNECT_MIN_MS)
    , _wifiConnecting(false)
    , _wifiConnectStart(0)
    , _hasCommand(false)
{
    _pendingCommand.type = MQTTCommandType::Unknown;
    _instance = this;
}

void MQTTManager::begin(const char* ssid, const char* pass,
                         const char* brokerIP, int brokerPort,
                         const char* clientName, const char* mqttUser, const char* mqttPass) {
    _ssid = ssid;
    _wifiPass = pass;
    _brokerIP = brokerIP;
    _brokerPort = brokerPort;
    _clientName = clientName;
    _mqttUser = mqttUser;
    _mqttPass = mqttPass;

    _mqttClient.setServer(_brokerIP, _brokerPort);
    _mqttClient.setCallback(staticCallback);

    connectWifi();
}

void MQTTManager::update() {
    // WiFi reconnect (non-blocking)
    if (WiFi.status() != WL_CONNECTED) {
        if (!_wifiConnecting) {
            _hal.serialPrintln("WiFi lost, reconnecting...");
            connectWifi();
        } else if (_hal.millis() - _wifiConnectStart > WIFI_CONNECT_TIMEOUT_MS) {
            _hal.serialPrintln("WiFi connect timeout, retrying...");
            WiFi.disconnect();
            _wifiConnecting = false;
        }
        return;
    }

    if (_wifiConnecting) {
        _wifiConnecting = false;
        _hal.serialPrintln("WiFi connected");
    }

    // MQTT reconnect with exponential backoff
    if (!_mqttClient.connected()) {
        unsigned long now = _hal.millis();
        if (now - _lastReconnectAttempt >= _reconnectInterval) {
            _lastReconnectAttempt = now;
            connectMQTT();
        }
    }

    _mqttClient.loop();
}

bool MQTTManager::isWifiConnected() {
    return WiFi.status() == WL_CONNECTED;
}

bool MQTTManager::isMQTTConnected() {
    return _mqttClient.connected();
}

void MQTTManager::publishWateringStatus(bool on, int zoneId, int remainingSec) {
    if (!_mqttClient.connected()) return;

    if (on) {
        char buf[64];
        snprintf(buf, sizeof(buf), "{\"zone\":%d,\"remaining\":%d}", zoneId, remainingSec);
        _mqttClient.publish("sprinkler/status/watering", buf);
    } else {
        _mqttClient.publish("sprinkler/status/watering", "off");
    }
}

void MQTTManager::publishZoneState(int zoneId, bool on) {
    if (!_mqttClient.connected()) return;

    char topic[40];
    snprintf(topic, sizeof(topic), "sprinkler/status/zone/%d", zoneId);
    _mqttClient.publish(topic, on ? "on" : "off", true); // retained
}

void MQTTManager::publishOnline() {
    if (!_mqttClient.connected()) return;
    _mqttClient.publish("sprinkler/status/online", "online", true);
}

void MQTTManager::publishDiscovery() {
    if (!_mqttClient.connected()) return;

    // Increase PubSubClient buffer for large discovery payloads
    _mqttClient.setBufferSize(512);

    // Shared device block for all entities
    // "dev":{"ids":["irrigation_ctrl"],"name":"Irrigation Controller","mf":"DIY","mdl":"Nano ESP32"}
    static const char* DEV_BLOCK =
        "\"dev\":{\"ids\":[\"irrigation_ctrl\"],"
        "\"name\":\"Irrigation Controller\","
        "\"mf\":\"DIY\",\"mdl\":\"Nano ESP32\"}";

    // Availability block — tied to LWT
    static const char* AVAIL_BLOCK =
        "\"avty_t\":\"sprinkler/status/online\","
        "\"pl_avail\":\"online\",\"pl_not_avail\":\"offline\"";

    // Per-zone switch entities (zone 1-4)
    for (int z = 1; z <= NUM_ZONES; z++) {
        char topic[96];
        snprintf(topic, sizeof(topic),
            "homeassistant/switch/irrigation_zone%d/config", z);

        char payload[450];
        snprintf(payload, sizeof(payload),
            "{"
            "\"name\":\"Zone %d\","
            "\"uniq_id\":\"irrigation_zone%d\","
            "\"cmd_t\":\"sprinkler/cmd/on\","
            "\"stat_t\":\"sprinkler/status/zone/%d\","
            "\"pl_on\":\"{\\\"zones\\\":[{\\\"zone\\\":%d,\\\"duration\\\":10}]}\","
            "\"pl_off\":\"off\","
            "\"stat_on\":\"on\",\"stat_off\":\"off\","
            "%s,%s"
            "}",
            z, z, z, z, AVAIL_BLOCK, DEV_BLOCK);

        _mqttClient.publish(topic, payload, true);
    }

    // Binary sensor: watering active
    {
        char payload[350];
        snprintf(payload, sizeof(payload),
            "{"
            "\"name\":\"Watering Active\","
            "\"uniq_id\":\"irrigation_watering\","
            "\"stat_t\":\"sprinkler/status/watering\","
            "\"val_tpl\":\"{{ 'ON' if value != 'off' else 'OFF' }}\","
            "\"dev_cla\":\"running\","
            "%s,%s"
            "}",
            AVAIL_BLOCK, DEV_BLOCK);
        _mqttClient.publish(
            "homeassistant/binary_sensor/irrigation_watering/config",
            payload, true);
    }

    // Sensor: active zone + remaining time
    {
        char payload[350];
        snprintf(payload, sizeof(payload),
            "{"
            "\"name\":\"Watering Status\","
            "\"uniq_id\":\"irrigation_status\","
            "\"stat_t\":\"sprinkler/status/watering\","
            "\"val_tpl\":\"{{ value_json.zone if value != 'off' else 'idle' }}\","
            "\"ic\":\"mdi:sprinkler-variant\","
            "%s,%s"
            "}",
            AVAIL_BLOCK, DEV_BLOCK);
        _mqttClient.publish(
            "homeassistant/sensor/irrigation_status/config",
            payload, true);
    }

    _hal.serialPrintln("HA MQTT Discovery published");
}

bool MQTTManager::hasCommand() const {
    return _hasCommand;
}

MQTTCommand MQTTManager::consumeCommand() {
    _hasCommand = false;
    MQTTCommand cmd = _pendingCommand;
    _pendingCommand.type = MQTTCommandType::Unknown;
    return cmd;
}

void MQTTManager::connectWifi() {
    WiFi.begin(_ssid, _wifiPass);
    _wifiConnecting = true;
    _wifiConnectStart = _hal.millis();
}

void MQTTManager::connectMQTT() {
    _hal.serialPrint("MQTT connecting...");

    // Cap the TCP connection attempt at 3 seconds so a missing broker doesn't
    // stall the main loop (which must keep polling the encoder).
    _wifiClient.setTimeout(3000);

    bool connected = _mqttClient.connect(
        _clientName, _mqttUser, _mqttPass,
        "sprinkler/status/online", 0, true, "offline" // LWT
    );

    if (connected) {
        _hal.serialPrintln("connected");
        _reconnectInterval = MQTT_RECONNECT_MIN_MS;
        subscribe();
        publishOnline();
        publishDiscovery();
    } else {
        _hal.serialPrintln("failed");
        // Exponential backoff
        _reconnectInterval *= 2;
        if (_reconnectInterval > MQTT_RECONNECT_MAX_MS) {
            _reconnectInterval = MQTT_RECONNECT_MAX_MS;
        }
    }
}

void MQTTManager::subscribe() {
    _mqttClient.subscribe("sprinkler/cmd/#");
    _mqttClient.subscribe("sprinkler/#"); // legacy
}

void MQTTManager::staticCallback(char* topic, byte* payload, unsigned int length) {
    if (_instance) {
        _instance->handleMessage(topic, payload, length);
    }
}

void MQTTManager::handleMessage(char* topic, byte* payload, unsigned int length) {
    MQTTCommand cmd = parseMQTTMessage(topic, payload, length);
    if (cmd.type != MQTTCommandType::Unknown) {
        _pendingCommand = cmd;
        _hasCommand = true;
    }
}

#endif // ARDUINO
