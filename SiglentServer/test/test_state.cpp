#include <unity.h>
#include "state.h"

// Required by Unity — called before/after each test
void setUp() {}
void tearDown() {}

// ---- stateToJson ----

void test_stateToJson_on() {
    String result = stateToJson(true, "#ff0000", 128);
    TEST_ASSERT_EQUAL_STRING("{\"on\":true,\"color\":\"#ff0000\",\"brightness\":128}", result.c_str());
}

void test_stateToJson_off() {
    String result = stateToJson(false, "#ffffff", 0);
    TEST_ASSERT_EQUAL_STRING("{\"on\":false,\"color\":\"#ffffff\",\"brightness\":0}", result.c_str());
}

void test_stateToJson_full_brightness() {
    String result = stateToJson(true, "#00ff00", 255);
    TEST_ASSERT_EQUAL_STRING("{\"on\":true,\"color\":\"#00ff00\",\"brightness\":255}", result.c_str());
}

// ---- haStateToJson ----

void test_haStateToJson_on() {
    String result = haStateToJson(true, "#ff8000", 200);
    TEST_ASSERT_EQUAL_STRING(
        "{\"state\":\"ON\",\"color_mode\":\"rgb\",\"brightness\":200,"
        "\"color\":{\"r\":255,\"g\":128,\"b\":0}}",
        result.c_str());
}

void test_haStateToJson_off_keeps_color_and_brightness() {
    String result = haStateToJson(false, "#0000ff", 10);
    TEST_ASSERT_EQUAL_STRING(
        "{\"state\":\"OFF\",\"color_mode\":\"rgb\",\"brightness\":10,"
        "\"color\":{\"r\":0,\"g\":0,\"b\":255}}",
        result.c_str());
}

void test_haStateToJson_uppercase_color() {
    String result = haStateToJson(true, "#FFFFFF", 128);
    TEST_ASSERT_EQUAL_STRING(
        "{\"state\":\"ON\",\"color_mode\":\"rgb\",\"brightness\":128,"
        "\"color\":{\"r\":255,\"g\":255,\"b\":255}}",
        result.c_str());
}

void test_haStateToJson_parses_back_as_command() {
    // The published state must round-trip through the command parser unchanged
    bool    on         = false;
    String  color      = "#000000";
    uint8_t brightness = 0;
    TEST_ASSERT_TRUE(parseMqttCommand(haStateToJson(true, "#12ab34", 77), on, color, brightness));
    TEST_ASSERT_TRUE(on);
    TEST_ASSERT_EQUAL_STRING("#12ab34", color.c_str());
    TEST_ASSERT_EQUAL_UINT8(77, brightness);
}

// ---- rgbToHex ----

void test_rgbToHex_red() {
    TEST_ASSERT_EQUAL_STRING("#ff0000", rgbToHex(255, 0, 0).c_str());
}

void test_rgbToHex_green() {
    TEST_ASSERT_EQUAL_STRING("#00ff00", rgbToHex(0, 255, 0).c_str());
}

void test_rgbToHex_blue() {
    TEST_ASSERT_EQUAL_STRING("#0000ff", rgbToHex(0, 0, 255).c_str());
}

void test_rgbToHex_black() {
    TEST_ASSERT_EQUAL_STRING("#000000", rgbToHex(0, 0, 0).c_str());
}

void test_rgbToHex_white() {
    TEST_ASSERT_EQUAL_STRING("#ffffff", rgbToHex(255, 255, 255).c_str());
}

void test_rgbToHex_clamps() {
    TEST_ASSERT_EQUAL_STRING("#ff00ff", rgbToHex(300, -5, 1000).c_str());
}

// ---- isValidHexColor ----

void test_isValidHexColor_valid() {
    TEST_ASSERT_TRUE(isValidHexColor("#000000"));
    TEST_ASSERT_TRUE(isValidHexColor("#ffffff"));
    TEST_ASSERT_TRUE(isValidHexColor("#FFFFFF"));
    TEST_ASSERT_TRUE(isValidHexColor("#a1B2c3"));
}

void test_isValidHexColor_invalid() {
    TEST_ASSERT_FALSE(isValidHexColor(""));
    TEST_ASSERT_FALSE(isValidHexColor("#"));
    TEST_ASSERT_FALSE(isValidHexColor("ffffff"));     // no #
    TEST_ASSERT_FALSE(isValidHexColor("#fff"));       // short form
    TEST_ASSERT_FALSE(isValidHexColor("#fffffff"));   // too long
    TEST_ASSERT_FALSE(isValidHexColor("#gggggg"));    // not hex
    TEST_ASSERT_FALSE(isValidHexColor("#12345z"));    // last digit not hex
    TEST_ASSERT_FALSE(isValidHexColor(" #ffffff"));   // leading space
    TEST_ASSERT_FALSE(isValidHexColor("red"));
}

// ---- hexToRgb ----

void test_hexToRgb_valid() {
    uint8_t r = 0, g = 0, b = 0;
    TEST_ASSERT_TRUE(hexToRgb("#ff8001", r, g, b));
    TEST_ASSERT_EQUAL_UINT8(255, r);
    TEST_ASSERT_EQUAL_UINT8(128, g);
    TEST_ASSERT_EQUAL_UINT8(1, b);
}

void test_hexToRgb_uppercase() {
    uint8_t r = 0, g = 0, b = 0;
    TEST_ASSERT_TRUE(hexToRgb("#ABCDEF", r, g, b));
    TEST_ASSERT_EQUAL_UINT8(0xAB, r);
    TEST_ASSERT_EQUAL_UINT8(0xCD, g);
    TEST_ASSERT_EQUAL_UINT8(0xEF, b);
}

void test_hexToRgb_invalid_leaves_outputs() {
    uint8_t r = 1, g = 2, b = 3;
    TEST_ASSERT_FALSE(hexToRgb("#xyz123", r, g, b));
    TEST_ASSERT_FALSE(hexToRgb("123456", r, g, b));
    TEST_ASSERT_EQUAL_UINT8(1, r);
    TEST_ASSERT_EQUAL_UINT8(2, g);
    TEST_ASSERT_EQUAL_UINT8(3, b);
}

// ---- parseMqttCommand ----

void test_parseMqttCommand_on() {
    bool    on         = false;
    String  color      = "#000000";
    uint8_t brightness = 50;
    bool    ok         = parseMqttCommand("{\"state\":\"ON\"}", on, color, brightness);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_TRUE(on);
    TEST_ASSERT_EQUAL_STRING("#000000", color.c_str());  // unchanged
    TEST_ASSERT_EQUAL_UINT8(50, brightness);              // unchanged
}

void test_parseMqttCommand_off() {
    bool    on         = true;
    String  color      = "#ffffff";
    uint8_t brightness = 50;
    bool    ok         = parseMqttCommand("{\"state\":\"OFF\"}", on, color, brightness);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_FALSE(on);
}

void test_parseMqttCommand_unknown_state_ignored() {
    bool    on         = true;
    String  color      = "#ffffff";
    uint8_t brightness = 50;
    TEST_ASSERT_TRUE(parseMqttCommand("{\"state\":\"TOGGLE\"}", on, color, brightness));
    TEST_ASSERT_TRUE(on);
    TEST_ASSERT_TRUE(parseMqttCommand("{\"state\":1}", on, color, brightness));
    TEST_ASSERT_TRUE(on);
}

void test_parseMqttCommand_color_only() {
    bool    on         = true;
    String  color      = "#000000";
    uint8_t brightness = 50;
    bool    ok = parseMqttCommand("{\"color\":{\"r\":255,\"g\":128,\"b\":0}}", on, color, brightness);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_TRUE(on);  // unchanged
    TEST_ASSERT_EQUAL_STRING("#ff8000", color.c_str());
    TEST_ASSERT_EQUAL_UINT8(50, brightness);  // unchanged
}

void test_parseMqttCommand_color_out_of_range_clamped() {
    bool    on         = true;
    String  color      = "#000000";
    uint8_t brightness = 50;
    TEST_ASSERT_TRUE(
        parseMqttCommand("{\"color\":{\"r\":400,\"g\":-1,\"b\":16}}", on, color, brightness));
    TEST_ASSERT_EQUAL_STRING("#ff0010", color.c_str());
}

void test_parseMqttCommand_partial_color_ignored() {
    bool    on         = true;
    String  color      = "#123456";
    uint8_t brightness = 50;
    TEST_ASSERT_TRUE(parseMqttCommand("{\"color\":{\"r\":255,\"g\":0}}", on, color, brightness));
    TEST_ASSERT_EQUAL_STRING("#123456", color.c_str());
}

void test_parseMqttCommand_brightness_only() {
    bool    on         = false;
    String  color      = "#123456";
    uint8_t brightness = 50;
    TEST_ASSERT_TRUE(parseMqttCommand("{\"brightness\":200}", on, color, brightness));
    TEST_ASSERT_FALSE(on);                              // unchanged
    TEST_ASSERT_EQUAL_STRING("#123456", color.c_str());  // unchanged
    TEST_ASSERT_EQUAL_UINT8(200, brightness);
}

void test_parseMqttCommand_brightness_clamped_high() {
    bool    on         = true;
    String  color      = "#ffffff";
    uint8_t brightness = 50;
    TEST_ASSERT_TRUE(parseMqttCommand("{\"brightness\":300}", on, color, brightness));
    TEST_ASSERT_EQUAL_UINT8(255, brightness);
}

void test_parseMqttCommand_brightness_clamped_low() {
    bool    on         = true;
    String  color      = "#ffffff";
    uint8_t brightness = 50;
    TEST_ASSERT_TRUE(parseMqttCommand("{\"brightness\":-20}", on, color, brightness));
    TEST_ASSERT_EQUAL_UINT8(0, brightness);
}

void test_parseMqttCommand_brightness_edges() {
    bool    on         = true;
    String  color      = "#ffffff";
    uint8_t brightness = 50;
    TEST_ASSERT_TRUE(parseMqttCommand("{\"brightness\":0}", on, color, brightness));
    TEST_ASSERT_EQUAL_UINT8(0, brightness);
    TEST_ASSERT_TRUE(parseMqttCommand("{\"brightness\":255}", on, color, brightness));
    TEST_ASSERT_EQUAL_UINT8(255, brightness);
}

void test_parseMqttCommand_brightness_float_rounded() {
    bool    on         = true;
    String  color      = "#ffffff";
    uint8_t brightness = 50;
    TEST_ASSERT_TRUE(parseMqttCommand("{\"brightness\":127.6}", on, color, brightness));
    TEST_ASSERT_EQUAL_UINT8(128, brightness);
}

void test_parseMqttCommand_brightness_not_number_ignored() {
    bool    on         = true;
    String  color      = "#ffffff";
    uint8_t brightness = 50;
    TEST_ASSERT_TRUE(parseMqttCommand("{\"brightness\":\"high\"}", on, color, brightness));
    TEST_ASSERT_EQUAL_UINT8(50, brightness);
}

void test_parseMqttCommand_full() {
    bool    on         = false;
    String  color      = "#000000";
    uint8_t brightness = 0;
    bool    ok         = parseMqttCommand(
        "{\"state\":\"ON\",\"color\":{\"r\":1,\"g\":2,\"b\":3},\"brightness\":99}", on, color,
        brightness);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_TRUE(on);
    TEST_ASSERT_EQUAL_STRING("#010203", color.c_str());
    TEST_ASSERT_EQUAL_UINT8(99, brightness);
}

void test_parseMqttCommand_empty_object() {
    bool    on         = true;
    String  color      = "#abcdef";
    uint8_t brightness = 42;
    TEST_ASSERT_TRUE(parseMqttCommand("{}", on, color, brightness));
    TEST_ASSERT_TRUE(on);
    TEST_ASSERT_EQUAL_STRING("#abcdef", color.c_str());
    TEST_ASSERT_EQUAL_UINT8(42, brightness);
}

void test_parseMqttCommand_malformed() {
    bool    on         = false;
    String  color      = "#ffffff";
    uint8_t brightness = 42;
    bool    ok         = parseMqttCommand("not json {{", on, color, brightness);
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_FALSE(on);
    TEST_ASSERT_EQUAL_STRING("#ffffff", color.c_str());
    TEST_ASSERT_EQUAL_UINT8(42, brightness);
}

void test_parseMqttCommand_truncated() {
    bool    on         = false;
    String  color      = "#ffffff";
    uint8_t brightness = 42;
    TEST_ASSERT_FALSE(parseMqttCommand("{\"state\":\"ON\"", on, color, brightness));
    TEST_ASSERT_FALSE(on);
}

void test_parseMqttCommand_empty_payload() {
    bool    on         = false;
    String  color      = "#ffffff";
    uint8_t brightness = 42;
    TEST_ASSERT_FALSE(parseMqttCommand("", on, color, brightness));
}

void test_parseMqttCommand_not_object() {
    bool    on         = false;
    String  color      = "#ffffff";
    uint8_t brightness = 42;
    TEST_ASSERT_FALSE(parseMqttCommand("[\"ON\"]", on, color, brightness));
    TEST_ASSERT_FALSE(parseMqttCommand("42", on, color, brightness));
    TEST_ASSERT_FALSE(on);
    TEST_ASSERT_EQUAL_UINT8(42, brightness);
}

// ---- Entry point ----

int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_stateToJson_on);
    RUN_TEST(test_stateToJson_off);
    RUN_TEST(test_stateToJson_full_brightness);

    RUN_TEST(test_haStateToJson_on);
    RUN_TEST(test_haStateToJson_off_keeps_color_and_brightness);
    RUN_TEST(test_haStateToJson_uppercase_color);
    RUN_TEST(test_haStateToJson_parses_back_as_command);

    RUN_TEST(test_rgbToHex_red);
    RUN_TEST(test_rgbToHex_green);
    RUN_TEST(test_rgbToHex_blue);
    RUN_TEST(test_rgbToHex_black);
    RUN_TEST(test_rgbToHex_white);
    RUN_TEST(test_rgbToHex_clamps);

    RUN_TEST(test_isValidHexColor_valid);
    RUN_TEST(test_isValidHexColor_invalid);

    RUN_TEST(test_hexToRgb_valid);
    RUN_TEST(test_hexToRgb_uppercase);
    RUN_TEST(test_hexToRgb_invalid_leaves_outputs);

    RUN_TEST(test_parseMqttCommand_on);
    RUN_TEST(test_parseMqttCommand_off);
    RUN_TEST(test_parseMqttCommand_unknown_state_ignored);
    RUN_TEST(test_parseMqttCommand_color_only);
    RUN_TEST(test_parseMqttCommand_color_out_of_range_clamped);
    RUN_TEST(test_parseMqttCommand_partial_color_ignored);
    RUN_TEST(test_parseMqttCommand_brightness_only);
    RUN_TEST(test_parseMqttCommand_brightness_clamped_high);
    RUN_TEST(test_parseMqttCommand_brightness_clamped_low);
    RUN_TEST(test_parseMqttCommand_brightness_edges);
    RUN_TEST(test_parseMqttCommand_brightness_float_rounded);
    RUN_TEST(test_parseMqttCommand_brightness_not_number_ignored);
    RUN_TEST(test_parseMqttCommand_full);
    RUN_TEST(test_parseMqttCommand_empty_object);
    RUN_TEST(test_parseMqttCommand_malformed);
    RUN_TEST(test_parseMqttCommand_truncated);
    RUN_TEST(test_parseMqttCommand_empty_payload);
    RUN_TEST(test_parseMqttCommand_not_object);

    return UNITY_END();
}
