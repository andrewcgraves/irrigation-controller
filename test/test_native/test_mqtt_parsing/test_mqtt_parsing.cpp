#include <unity.h>
#include "../../../src/network/MQTTManager.h"

// Only pull in the parsing function, not the Arduino-dependent MQTTManager class
// We include the .cpp directly but the #ifdef ARDUINO guards keep hardware code out
#include "../../../src/zone/ZoneController.h"
#include "../../../src/network/MQTTManager.cpp"

void setUp() {}
void tearDown() {}

// Helper to call parser with string payload
static MQTTCommand parse(const char* topic, const char* payload) {
    return parseMQTTMessage(topic,
        reinterpret_cast<const uint8_t*>(payload),
        payload ? strlen(payload) : 0);
}

// ============================================================
// New JSON format: sprinkler/cmd/*
// ============================================================

void test_json_on_single_zone() {
    auto cmd = parse("sprinkler/cmd/on",
        "{\"zones\":[{\"zone\":1,\"duration\":10}]}");
    TEST_ASSERT_EQUAL(MQTTCommandType::TurnOn, cmd.type);
    TEST_ASSERT_EQUAL(1, cmd.queue.count);
    TEST_ASSERT_EQUAL(1, cmd.queue.entries[0].zoneId);
    TEST_ASSERT_EQUAL(10, cmd.queue.entries[0].durationMin);
}

void test_json_on_multiple_zones() {
    auto cmd = parse("sprinkler/cmd/on",
        "{\"zones\":[{\"zone\":1,\"duration\":10},{\"zone\":2,\"duration\":15},{\"zone\":3,\"duration\":20}]}");
    TEST_ASSERT_EQUAL(MQTTCommandType::TurnOn, cmd.type);
    TEST_ASSERT_EQUAL(3, cmd.queue.count);
    TEST_ASSERT_EQUAL(1, cmd.queue.entries[0].zoneId);
    TEST_ASSERT_EQUAL(10, cmd.queue.entries[0].durationMin);
    TEST_ASSERT_EQUAL(2, cmd.queue.entries[1].zoneId);
    TEST_ASSERT_EQUAL(15, cmd.queue.entries[1].durationMin);
    TEST_ASSERT_EQUAL(3, cmd.queue.entries[2].zoneId);
    TEST_ASSERT_EQUAL(20, cmd.queue.entries[2].durationMin);
}

void test_json_on_per_zone_durations() {
    auto cmd = parse("sprinkler/cmd/on",
        "{\"zones\":[{\"zone\":1,\"duration\":5},{\"zone\":4,\"duration\":30}]}");
    TEST_ASSERT_EQUAL(MQTTCommandType::TurnOn, cmd.type);
    TEST_ASSERT_EQUAL(2, cmd.queue.count);
    TEST_ASSERT_EQUAL(5, cmd.queue.entries[0].durationMin);
    TEST_ASSERT_EQUAL(30, cmd.queue.entries[1].durationMin);
}

void test_json_off() {
    auto cmd = parse("sprinkler/cmd/off", "");
    TEST_ASSERT_EQUAL(MQTTCommandType::TurnOff, cmd.type);
}

void test_json_status() {
    auto cmd = parse("sprinkler/cmd/status", "");
    TEST_ASSERT_EQUAL(MQTTCommandType::GetStatus, cmd.type);
}

void test_json_schedule_set() {
    auto cmd = parse("sprinkler/cmd/schedule/set", "{}");
    TEST_ASSERT_EQUAL(MQTTCommandType::SetSchedule, cmd.type);
}

void test_json_schedule_delete() {
    auto cmd = parse("sprinkler/cmd/schedule/delete", "{}");
    TEST_ASSERT_EQUAL(MQTTCommandType::DeleteSchedule, cmd.type);
}

void test_json_invalid_empty_zones() {
    auto cmd = parse("sprinkler/cmd/on", "{}");
    TEST_ASSERT_EQUAL(MQTTCommandType::Unknown, cmd.type);
}

void test_json_out_of_range_zone_rejected() {
    auto cmd = parse("sprinkler/cmd/on",
        "{\"zones\":[{\"zone\":5,\"duration\":10}]}");
    TEST_ASSERT_EQUAL(MQTTCommandType::Unknown, cmd.type);
}

void test_json_zero_duration_rejected() {
    auto cmd = parse("sprinkler/cmd/on",
        "{\"zones\":[{\"zone\":1,\"duration\":0}]}");
    TEST_ASSERT_EQUAL(MQTTCommandType::Unknown, cmd.type);
}

void test_json_over_max_duration_rejected() {
    auto cmd = parse("sprinkler/cmd/on",
        "{\"zones\":[{\"zone\":1,\"duration\":31}]}");
    TEST_ASSERT_EQUAL(MQTTCommandType::Unknown, cmd.type);
}

// ============================================================
// Legacy format: sprinkler/{action}/{zones}
// ============================================================

void test_legacy_on_single_zone() {
    auto cmd = parse("sprinkler/on/1", "10");
    TEST_ASSERT_EQUAL(MQTTCommandType::TurnOn, cmd.type);
    TEST_ASSERT_EQUAL(1, cmd.queue.count);
    TEST_ASSERT_EQUAL(1, cmd.queue.entries[0].zoneId);
    TEST_ASSERT_EQUAL(10, cmd.queue.entries[0].durationMin);
}

void test_legacy_on_multiple_zones() {
    auto cmd = parse("sprinkler/on/1,2,3", "15");
    TEST_ASSERT_EQUAL(MQTTCommandType::TurnOn, cmd.type);
    TEST_ASSERT_EQUAL(3, cmd.queue.count);
    TEST_ASSERT_EQUAL(1, cmd.queue.entries[0].zoneId);
    TEST_ASSERT_EQUAL(2, cmd.queue.entries[1].zoneId);
    TEST_ASSERT_EQUAL(3, cmd.queue.entries[2].zoneId);
    // Legacy: all zones get the same duration
    TEST_ASSERT_EQUAL(15, cmd.queue.entries[0].durationMin);
    TEST_ASSERT_EQUAL(15, cmd.queue.entries[1].durationMin);
    TEST_ASSERT_EQUAL(15, cmd.queue.entries[2].durationMin);
}

void test_legacy_on_default_duration() {
    auto cmd = parse("sprinkler/on/1", ""); // empty payload
    TEST_ASSERT_EQUAL(MQTTCommandType::TurnOn, cmd.type);
    TEST_ASSERT_EQUAL(10, cmd.queue.entries[0].durationMin); // default 10
}

void test_legacy_on_caps_duration_at_30() {
    auto cmd = parse("sprinkler/on/1", "60");
    TEST_ASSERT_EQUAL(30, cmd.queue.entries[0].durationMin);
}

void test_legacy_off() {
    auto cmd = parse("sprinkler/off", "");
    TEST_ASSERT_EQUAL(MQTTCommandType::TurnOff, cmd.type);
}

void test_legacy_pong() {
    auto cmd = parse("sprinkler/pong", "");
    TEST_ASSERT_EQUAL(MQTTCommandType::Ping, cmd.type);
}

void test_legacy_reset_maps_to_off() {
    auto cmd = parse("sprinkler/reset", "");
    TEST_ASSERT_EQUAL(MQTTCommandType::TurnOff, cmd.type);
}

void test_legacy_invalid_zone_rejected() {
    auto cmd = parse("sprinkler/on/0", "10"); // zone 0 invalid
    TEST_ASSERT_EQUAL(MQTTCommandType::Unknown, cmd.type);
}

// ============================================================
// Edge cases
// ============================================================

void test_null_topic_returns_unknown() {
    auto cmd = parseMQTTMessage(nullptr, nullptr, 0);
    TEST_ASSERT_EQUAL(MQTTCommandType::Unknown, cmd.type);
}

void test_empty_topic_returns_unknown() {
    auto cmd = parse("", "");
    TEST_ASSERT_EQUAL(MQTTCommandType::Unknown, cmd.type);
}

void test_unrelated_topic_returns_unknown() {
    auto cmd = parse("homeassistant/sensor/temperature", "22.5");
    TEST_ASSERT_EQUAL(MQTTCommandType::Unknown, cmd.type);
}

void test_queue_capped_at_max_zones() {
    // Try to add 5 zones but max is 4
    auto cmd = parse("sprinkler/cmd/on",
        "{\"zones\":["
        "{\"zone\":1,\"duration\":10},"
        "{\"zone\":2,\"duration\":10},"
        "{\"zone\":3,\"duration\":10},"
        "{\"zone\":4,\"duration\":10},"
        "{\"zone\":1,\"duration\":10}"
        "]}");
    TEST_ASSERT_EQUAL(MQTTCommandType::TurnOn, cmd.type);
    TEST_ASSERT_EQUAL(4, cmd.queue.count); // capped at ZONE_MAX
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // JSON format
    RUN_TEST(test_json_on_single_zone);
    RUN_TEST(test_json_on_multiple_zones);
    RUN_TEST(test_json_on_per_zone_durations);
    RUN_TEST(test_json_off);
    RUN_TEST(test_json_status);
    RUN_TEST(test_json_schedule_set);
    RUN_TEST(test_json_schedule_delete);
    RUN_TEST(test_json_invalid_empty_zones);
    RUN_TEST(test_json_out_of_range_zone_rejected);
    RUN_TEST(test_json_zero_duration_rejected);
    RUN_TEST(test_json_over_max_duration_rejected);

    // Legacy format
    RUN_TEST(test_legacy_on_single_zone);
    RUN_TEST(test_legacy_on_multiple_zones);
    RUN_TEST(test_legacy_on_default_duration);
    RUN_TEST(test_legacy_on_caps_duration_at_30);
    RUN_TEST(test_legacy_off);
    RUN_TEST(test_legacy_pong);
    RUN_TEST(test_legacy_reset_maps_to_off);
    RUN_TEST(test_legacy_invalid_zone_rejected);

    // Edge cases
    RUN_TEST(test_null_topic_returns_unknown);
    RUN_TEST(test_empty_topic_returns_unknown);
    RUN_TEST(test_unrelated_topic_returns_unknown);
    RUN_TEST(test_queue_capped_at_max_zones);

    return UNITY_END();
}
