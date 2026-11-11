#include <gtest/gtest.h>
#include <set>
#include <string_view>
#include "board_pins.h"

namespace datalogger::testing {

TEST(BoardPinsTest, HasTenAnalogChannelsOnAdc1InSchematicOrder) {
    ASSERT_EQ(std::size(board::kAnalogChannels), 10u);

    EXPECT_EQ(board::kAnalogChannels[0].pin, GPIO_NUM_1);
    EXPECT_STREQ(board::kAnalogChannels[0].tag, "OilPressure");

    EXPECT_EQ(board::kAnalogChannels[1].pin, GPIO_NUM_2);
    EXPECT_STREQ(board::kAnalogChannels[1].tag, "OilTemp");

    EXPECT_EQ(board::kAnalogChannels[2].pin, GPIO_NUM_3);
    EXPECT_STREQ(board::kAnalogChannels[2].tag, "FuelPressure");

    EXPECT_EQ(board::kAnalogChannels[3].pin, GPIO_NUM_4);
    EXPECT_STREQ(board::kAnalogChannels[3].tag, "Lambda");

    EXPECT_EQ(board::kAnalogChannels[4].pin, GPIO_NUM_5);
    EXPECT_STREQ(board::kAnalogChannels[4].tag, "ADC5to3v3_1");

    EXPECT_EQ(board::kAnalogChannels[5].pin, GPIO_NUM_6);
    EXPECT_STREQ(board::kAnalogChannels[5].tag, "ADC5to3v3_2");

    EXPECT_EQ(board::kAnalogChannels[6].pin, GPIO_NUM_7);
    EXPECT_STREQ(board::kAnalogChannels[6].tag, "ADC5to3v3_3");

    EXPECT_EQ(board::kAnalogChannels[7].pin, GPIO_NUM_8);
    EXPECT_STREQ(board::kAnalogChannels[7].tag, "ADC5to3v3_4");

    EXPECT_EQ(board::kAnalogChannels[8].pin, GPIO_NUM_9);
    EXPECT_STREQ(board::kAnalogChannels[8].tag, "ADC12to3v3_1");

    EXPECT_EQ(board::kAnalogChannels[9].pin, GPIO_NUM_10);
    EXPECT_STREQ(board::kAnalogChannels[9].tag, "ADC12to3v3_2");
}

TEST(BoardPinsTest, HasNoPinConflictsOrDuplicateTagsInBoardMap) {
    std::set<gpio_num_t> used_pins;
    std::set<std::string_view> used_tags;

    for (const auto& entry : board::kBoardPinMap) {
        EXPECT_NE(entry.pin, GPIO_NUM_NC);
        EXPECT_TRUE(used_pins.insert(entry.pin).second)
            << "Conflito de GPIO duplicado: GPIO " << static_cast<int>(entry.pin);
        EXPECT_TRUE(used_tags.insert(entry.tag).second)
            << "Tag duplicada: " << entry.tag;
    }

    EXPECT_EQ(used_pins.size(), 26u);
}

TEST(BoardPinsTest, PeripheralsMatchCpuSchematic) {
    // I2C RTC DS3231
    EXPECT_EQ(PIN_I2C_SDA, GPIO_NUM_11);
    EXPECT_EQ(PIN_I2C_SCL, GPIO_NUM_12);

    // GPS Neo-8M
    EXPECT_EQ(PIN_GPS_RX, GPIO_NUM_13);
    EXPECT_EQ(PIN_GPS_TX, GPIO_NUM_47);

    // CAN (SN65HVD230DR)
    EXPECT_EQ(PIN_CAN_TX, GPIO_NUM_15);
    EXPECT_EQ(PIN_CAN_RX, GPIO_NUM_16);

    // MicroSD
    EXPECT_EQ(PIN_SD_MISO, GPIO_NUM_14);
    EXPECT_EQ(PIN_SD_CS, GPIO_NUM_17);
    EXPECT_EQ(PIN_SD_MOSI, GPIO_NUM_18);
    EXPECT_EQ(PIN_SD_SCK, GPIO_NUM_21);
    EXPECT_EQ(PIN_SD_DAT1, GPIO_NUM_40);
    EXPECT_EQ(PIN_SD_DAT2, GPIO_NUM_41);

    // UI (Buttons & LEDs)
    EXPECT_EQ(PIN_BTN_FREE_SW1, GPIO_NUM_35);
    EXPECT_EQ(PIN_BTN_FREE_SW2, GPIO_NUM_36);
    EXPECT_EQ(PIN_LED_ACQUIRING, GPIO_NUM_37);
    EXPECT_EQ(PIN_LED_WORKING, GPIO_NUM_39);
}

} // namespace datalogger::testing
