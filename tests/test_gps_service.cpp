#include <array>
#include <cmath>
#include <cstring>
#include <string>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "gps_service.hpp"
#include "mock_gps_hal.hpp"
#include "mock_rtc_hal.hpp"
#include "neo8m_uart_lld.hpp"
#include "rtc_service.hpp"

using ::testing::_;
using ::testing::DoAll;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgReferee;

namespace datalogger::sil {

namespace {

int copy_chunk_to_buf(const uint8_t* src,
                      std::size_t src_len,
                      uint8_t* dst,
                      std::size_t max_len) {
    const std::size_t n = (src_len <= max_len) ? src_len : max_len;
    std::memcpy(dst, src, n);
    return static_cast<int>(n);
}

hal::RtcDateTime make_rtc_dt(uint16_t year = 2026,
                             uint8_t month = 11,
                             uint8_t day = 19,
                             uint8_t dow = 4,
                             uint8_t hour = 14,
                             uint8_t minute = 25,
                             uint8_t second = 36) {
    hal::RtcDateTime dt{};
    dt.year = year;
    dt.month = month;
    dt.day = day;
    dt.day_of_week = dow;
    dt.hour = hour;
    dt.minute = minute;
    dt.second = second;
    return dt;
}

} // namespace

class GpsServiceTest : public ::testing::Test {
protected:
    NiceMock<MockGpsHal> mock_gps_hal_;
    NiceMock<MockRtcHal> mock_rtc_hal_;
    bsw::RtcService rtc_service_{mock_rtc_hal_};
    bsw::GpsService gps_service_{mock_gps_hal_, rtc_service_};
};

TEST_F(GpsServiceTest, DefaultConfigUsesBspUartPinsAnd10Hz) {
    const hal::GpsConfig cfg{};
    EXPECT_EQ(cfg.rx_pin, PIN_GPS_RX);
    EXPECT_EQ(cfg.tx_pin, PIN_GPS_TX);
    EXPECT_EQ(cfg.rx_pin, GPIO_NUM_13);
    EXPECT_EQ(cfg.tx_pin, GPIO_NUM_47);
    EXPECT_EQ(cfg.initial_baud_rate, 9600u);
    EXPECT_EQ(cfg.operating_baud_rate, 115200u);
    EXPECT_EQ(cfg.update_rate_hz, 10u);
    EXPECT_EQ(cfg.dynamic_model, 4u);
    EXPECT_EQ(cfg.uart_port, 1u);
}

TEST_F(GpsServiceTest, InitialStateAndPollBeforeInitialize) {
    EXPECT_FALSE(gps_service_.is_initialized());
    EXPECT_EQ(gps_service_.sentences_parsed(), 0u);
    EXPECT_EQ(gps_service_.checksum_errors(), 0u);
    EXPECT_EQ(gps_service_.rx_bytes(), 0u);
    EXPECT_EQ(gps_service_.overflow_count(), 0u);
    EXPECT_FALSE(gps_service_.latest_data().has_fix);
    EXPECT_FLOAT_EQ(gps_service_.latest_data().hdop, 99.99f);

    EXPECT_CALL(mock_gps_hal_, read_bytes(_, _, _)).Times(0);
    EXPECT_FALSE(gps_service_.poll(10u));
}

TEST_F(GpsServiceTest, InitializePropagatesConfigAndResetsStateOnFailure) {
    EXPECT_CALL(mock_gps_hal_, init(_)).WillOnce(Return(true));
    ASSERT_TRUE(gps_service_.initialize());
    EXPECT_TRUE(gps_service_.is_initialized());

    ASSERT_TRUE(gps_service_.parse_nmea_sentence(
        "$GPGGA,092725.00,4717.11399,N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,*5B"));
    EXPECT_EQ(gps_service_.sentences_parsed(), 1u);
    EXPECT_TRUE(gps_service_.latest_data().has_fix);

    EXPECT_CALL(mock_gps_hal_, init(_)).WillOnce(Return(false));
    EXPECT_FALSE(gps_service_.initialize());
    EXPECT_FALSE(gps_service_.is_initialized());
    EXPECT_EQ(gps_service_.sentences_parsed(), 0u);
    EXPECT_EQ(gps_service_.checksum_errors(), 0u);
    EXPECT_FALSE(gps_service_.latest_data().has_fix);
}

TEST_F(GpsServiceTest, ChecksumValidationAcceptsUpperLowerAndRejectsCorruption) {
    EXPECT_TRUE(bsw::GpsService::validate_nmea_checksum(
        "$GPGGA,092725.00,4717.11399,N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,*5B\r\n"));
    EXPECT_TRUE(bsw::GpsService::validate_nmea_checksum(
        "$GPGGA,092725.00,4717.11399,N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,*5b"));

    EXPECT_FALSE(bsw::GpsService::validate_nmea_checksum(
        "GPGGA,092725.00,4717.11399,N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,*5B"));
    EXPECT_FALSE(bsw::GpsService::validate_nmea_checksum(
        "$GPGGA,092725.00,4717.11399,N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,5B"));
    EXPECT_FALSE(bsw::GpsService::validate_nmea_checksum(
        "$GPGGA,092725.00,4717.11399,N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,*5"));
    EXPECT_FALSE(bsw::GpsService::validate_nmea_checksum(
        "$GPGGA,092725.00,4717.11399,N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,*5BX"));
    EXPECT_FALSE(bsw::GpsService::validate_nmea_checksum(
        "$GPGGA,092725.00,4717.11399,N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,*ZZ"));

    EXPECT_FALSE(gps_service_.parse_nmea_sentence(
        "$GPGGA,092725.00,4717.11399,N,00833.91590,E,1,09,1.01,499.6,M,48.0,M,,*5B"));
    EXPECT_EQ(gps_service_.checksum_errors(), 1u);
    EXPECT_EQ(gps_service_.sentences_parsed(), 0u);
}

TEST_F(GpsServiceTest, ParsesGpggaAndGnggaCoordinatesAltitudeSatellitesHdop) {
    ASSERT_TRUE(gps_service_.parse_nmea_sentence(
        "$GPGGA,092725.00,4717.11399,N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,*5B"));
    EXPECT_NEAR(gps_service_.latest_data().latitude_deg, 47.2852331667, 1e-7);
    EXPECT_NEAR(gps_service_.latest_data().longitude_deg, 8.5652650, 1e-7);
    EXPECT_FLOAT_EQ(gps_service_.latest_data().altitude_m, 499.6f);
    EXPECT_EQ(gps_service_.latest_data().satellites, 8u);
    EXPECT_FLOAT_EQ(gps_service_.latest_data().hdop, 1.01f);
    EXPECT_EQ(gps_service_.latest_data().fix_quality, 1u);
    EXPECT_EQ(gps_service_.latest_data().utc_time_ms, 34045000u);
    EXPECT_TRUE(gps_service_.latest_data().gga_valid);
    EXPECT_TRUE(gps_service_.latest_data().has_fix);

    ASSERT_TRUE(gps_service_.parse_nmea_sentence(
        "$GNGGA,123519.00,2736.05100,S,04831.12500,W,2,11,0.85,12.4,M,-2.1,M,,*6B"));
    EXPECT_NEAR(gps_service_.latest_data().latitude_deg, -27.60085, 1e-7);
    EXPECT_NEAR(gps_service_.latest_data().longitude_deg, -48.51875, 1e-7);
    EXPECT_FLOAT_EQ(gps_service_.latest_data().altitude_m, 12.4f);
    EXPECT_EQ(gps_service_.latest_data().satellites, 11u);
    EXPECT_FLOAT_EQ(gps_service_.latest_data().hdop, 0.85f);
    EXPECT_EQ(gps_service_.latest_data().fix_quality, 2u);

    ASSERT_TRUE(gps_service_.parse_nmea_sentence(
        "$GPGGA,092725.00,4717.11399,N,00833.91590,E,1,08,1.01,-12.5,M,48.0,M,,*42"));
    EXPECT_FLOAT_EQ(gps_service_.latest_data().altitude_m, -12.5f);
}

TEST_F(GpsServiceTest, ParsesGprmcAndGnrmcSpeedCourseDateAcross12To14Fields) {
    ASSERT_TRUE(gps_service_.parse_nmea_sentence(
        "$GPRMC,083559.00,A,4717.11437,N,00833.91522,E,0.004,77.52,091202,,,A,V*2D"));
    EXPECT_TRUE(gps_service_.latest_data().rmc_valid);
    EXPECT_EQ(gps_service_.latest_data().utc_date_yyyymmdd, 20021209u);

    ASSERT_TRUE(gps_service_.parse_nmea_sentence(
        "$GPRMC,083559.00,A,4717.11437,N,00833.91522,E,0.004,77.52,091202,,,A*57"));
    EXPECT_TRUE(gps_service_.latest_data().rmc_valid);

    ASSERT_TRUE(gps_service_.parse_nmea_sentence(
        "$GPRMC,083559.00,A,4717.11437,N,00833.91522,E,0.004,77.52,091202,,*3A"));
    EXPECT_TRUE(gps_service_.latest_data().rmc_valid);

    ASSERT_TRUE(gps_service_.parse_nmea_sentence(
        "$GNRMC,142536.25,A,2736.05100,S,04831.12500,W,54.000,182.45,191126,,,A,V*3E"));
    EXPECT_NEAR(gps_service_.latest_data().latitude_deg, -27.60085, 1e-7);
    EXPECT_NEAR(gps_service_.latest_data().longitude_deg, -48.51875, 1e-7);
    EXPECT_FLOAT_EQ(gps_service_.latest_data().speed_knots, 54.0f);
    EXPECT_NEAR(gps_service_.latest_data().speed_kmh, 100.008f, 1e-3f);
    EXPECT_NEAR(gps_service_.latest_data().speed_mps, 27.78f, 1e-3f);
    EXPECT_FLOAT_EQ(gps_service_.latest_data().course_deg, 182.45f);
    EXPECT_EQ(gps_service_.latest_data().utc_date_yyyymmdd, 20261119u);
    EXPECT_EQ(gps_service_.latest_data().utc_time_ms, 51936250u);
}

TEST_F(GpsServiceTest, HandlesMissingCourseWhenStationaryAndConsecutiveCommas) {
    ASSERT_TRUE(gps_service_.parse_nmea_sentence(
        "$GNRMC,123519.00,A,2735.12345,S,04832.12345,W,0.012,,191126,,,A,V*17"));
    EXPECT_TRUE(gps_service_.latest_data().rmc_valid);
    EXPECT_TRUE(gps_service_.latest_data().has_fix);
    EXPECT_FLOAT_EQ(gps_service_.latest_data().course_deg, 0.0f);
    EXPECT_FLOAT_EQ(gps_service_.latest_data().speed_knots, 0.012f);
}

TEST_F(GpsServiceTest, NoFixGgaAndRmcClearFixAndHandleEmptyFields) {
    ASSERT_TRUE(gps_service_.parse_nmea_sentence(
        "$GPGGA,092725.00,4717.11399,N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,*5B"));
    ASSERT_TRUE(gps_service_.parse_nmea_sentence(
        "$GNRMC,142536.25,A,2736.05100,S,04831.12500,W,54.000,182.45,191126,,,A,V*3E"));
    ASSERT_TRUE(gps_service_.latest_data().has_fix);

    ASSERT_TRUE(gps_service_.parse_nmea_sentence("$GNGGA,,,,,,0,00,99.99,,,,,,*56"));
    EXPECT_FALSE(gps_service_.latest_data().gga_valid);
    EXPECT_EQ(gps_service_.latest_data().fix_quality, 0u);
    EXPECT_EQ(gps_service_.latest_data().satellites, 0u);
    EXPECT_FLOAT_EQ(gps_service_.latest_data().altitude_m, 0.0f);
    EXPECT_TRUE(gps_service_.latest_data().has_fix); // RMC ainda válido

    ASSERT_TRUE(gps_service_.parse_nmea_sentence("$GNRMC,123519.00,V,,,,,,,191126,,,N,V*18"));
    EXPECT_FALSE(gps_service_.latest_data().rmc_valid);
    EXPECT_FALSE(gps_service_.latest_data().has_fix);
    EXPECT_FLOAT_EQ(gps_service_.latest_data().speed_kmh, 0.0f);
    EXPECT_FLOAT_EQ(gps_service_.latest_data().speed_mps, 0.0f);
    EXPECT_FLOAT_EQ(gps_service_.latest_data().course_deg, 0.0f);
    EXPECT_DOUBLE_EQ(gps_service_.latest_data().latitude_deg, 0.0);
    EXPECT_DOUBLE_EQ(gps_service_.latest_data().longitude_deg, 0.0);
    EXPECT_EQ(gps_service_.latest_data().utc_date_yyyymmdd, 20261119u);
    EXPECT_EQ(gps_service_.latest_data().utc_time_ms, 45319000u);
}

TEST_F(GpsServiceTest, IgnoresTxtAndNonGgaRmcSentencesWithoutErrors) {
    EXPECT_FALSE(gps_service_.parse_nmea_sentence("$GNTXT,01,01,00,txbuf alloc*61"));
    EXPECT_FALSE(gps_service_.parse_nmea_sentence("$GNGSA,A,3,01,02,03,,,,,,,,,,2.1,1.2,1.7*2A"));
    EXPECT_FALSE(gps_service_.parse_nmea_sentence("$GNVTG,77.52,T,,M,0.004,N,0.008,K,A*18"));

    EXPECT_EQ(gps_service_.checksum_errors(), 0u);
    EXPECT_EQ(gps_service_.sentences_parsed(), 0u);
    EXPECT_FALSE(gps_service_.latest_data().has_fix);
}

TEST_F(GpsServiceTest, PollHandlesFragmentedReadsAndMultipleSentencesInOneChunk) {
    EXPECT_CALL(mock_gps_hal_, init(_)).WillOnce(Return(true));
    ASSERT_TRUE(gps_service_.initialize());

    const char* chunk1 = "$GPGGA,092725.00,4717.";
    const char* chunk2 =
        "11399,N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,*5B\r\n"
        "$GNRMC,142536.25,A,2736.05100,S,04831.12500,W,54.000,182.45,191126,,,A,V*3E\r\n";

    EXPECT_CALL(mock_gps_hal_, read_bytes(_, _, 10u))
        .WillOnce([chunk1](uint8_t* buf, std::size_t max_len, uint32_t) {
            return copy_chunk_to_buf(reinterpret_cast<const uint8_t*>(chunk1),
                                     std::strlen(chunk1),
                                     buf,
                                     max_len);
        })
        .WillOnce([chunk2](uint8_t* buf, std::size_t max_len, uint32_t) {
            return copy_chunk_to_buf(reinterpret_cast<const uint8_t*>(chunk2),
                                     std::strlen(chunk2),
                                     buf,
                                     max_len);
        });

    EXPECT_FALSE(gps_service_.poll(10u));
    EXPECT_EQ(gps_service_.sentences_parsed(), 0u);

    EXPECT_TRUE(gps_service_.poll(10u));
    EXPECT_EQ(gps_service_.sentences_parsed(), 2u);
    EXPECT_TRUE(gps_service_.latest_data().gga_valid);
    EXPECT_TRUE(gps_service_.latest_data().rmc_valid);
    EXPECT_EQ(gps_service_.rx_bytes(),
              static_cast<uint32_t>(std::strlen(chunk1) + std::strlen(chunk2)));
}

TEST_F(GpsServiceTest, PollResyncsOnDollarAndTracksBufferOverflow) {
    EXPECT_CALL(mock_gps_hal_, init(_)).WillOnce(Return(true));
    ASSERT_TRUE(gps_service_.initialize());

    const char* resync_chunk =
        "$GPGGA,0927"
        "$GNRMC,142536.25,A,2736.05100,S,04831.12500,W,54.000,182.45,191126,,,A,V*3E\r\n";

    std::array<uint8_t, 220> overflow_chunk{};
    overflow_chunk[0] = static_cast<uint8_t>('$');
    for (std::size_t i = 1u; i < 135u; ++i) {
        overflow_chunk[i] = static_cast<uint8_t>('A');
    }
    overflow_chunk[135] = static_cast<uint8_t>('\r');
    overflow_chunk[136] = static_cast<uint8_t>('\n');
    const char* valid_after =
        "$GPGGA,092725.00,4717.11399,N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,*5B\r\n";
    const std::size_t valid_len = std::strlen(valid_after);
    std::memcpy(&overflow_chunk[137], valid_after, valid_len);
    const std::size_t overflow_total = 137u + valid_len;

    EXPECT_CALL(mock_gps_hal_, read_bytes(_, _, _))
        .WillOnce([resync_chunk](uint8_t* buf, std::size_t max_len, uint32_t) {
            return copy_chunk_to_buf(reinterpret_cast<const uint8_t*>(resync_chunk),
                                     std::strlen(resync_chunk),
                                     buf,
                                     max_len);
        })
        .WillOnce([&overflow_chunk, overflow_total](uint8_t* buf, std::size_t max_len, uint32_t) {
            return copy_chunk_to_buf(overflow_chunk.data(), overflow_total, buf, max_len);
        });

    EXPECT_TRUE(gps_service_.poll(0u));
    EXPECT_TRUE(gps_service_.latest_data().rmc_valid);
    EXPECT_EQ(gps_service_.checksum_errors(), 0u);

    EXPECT_TRUE(gps_service_.poll(0u));
    EXPECT_EQ(gps_service_.overflow_count(), 1u);
    EXPECT_TRUE(gps_service_.latest_data().gga_valid);
    EXPECT_EQ(gps_service_.sentences_parsed(), 2u);
}

TEST_F(GpsServiceTest, PollSkipsBinaryUbxFramesContainingDollarAndCrLfCleanly) {
    EXPECT_CALL(mock_gps_hal_, init(_)).WillOnce(Return(true));
    ASSERT_TRUE(gps_service_.initialize());

    // UBX-ACK-ACK para CFG-RATE (10 bytes) + UBX sintético com '$', '\r', '\n' no payload (12 bytes)
    std::array<uint8_t, 128> stream{};
    std::size_t pos = 0u;

    const std::array<uint8_t, 10> ubx_ack{
        0xB5u, 0x62u, 0x05u, 0x01u, 0x02u, 0x00u, 0x06u, 0x08u, 0x16u, 0x3Fu};
    std::memcpy(&stream[pos], ubx_ack.data(), ubx_ack.size());
    pos += ubx_ack.size();

    std::array<uint8_t, 12> ubx_with_nmea_chars{
        0xB5u, 0x62u, 0x01u, 0x07u, 0x04u, 0x00u, 0x24u, 0x0Du, 0x0Au, 0x41u, 0x00u, 0x00u};
    lld::Neo8mUartLld::compute_ubx_checksum(&ubx_with_nmea_chars[2],
                                            8u,
                                            ubx_with_nmea_chars[10],
                                            ubx_with_nmea_chars[11]);
    std::memcpy(&stream[pos], ubx_with_nmea_chars.data(), ubx_with_nmea_chars.size());
    pos += ubx_with_nmea_chars.size();

    const char* rmc =
        "$GNRMC,142536.25,A,2736.05100,S,04831.12500,W,54.000,182.45,191126,,,A,V*3E\r\n";
    const std::size_t rmc_len = std::strlen(rmc);
    std::memcpy(&stream[pos], rmc, rmc_len);
    pos += rmc_len;

    EXPECT_CALL(mock_gps_hal_, read_bytes(_, _, _))
        .WillOnce([&stream, pos](uint8_t* buf, std::size_t max_len, uint32_t) {
            return copy_chunk_to_buf(stream.data(), pos, buf, max_len);
        });

    EXPECT_TRUE(gps_service_.poll(5u));
    EXPECT_EQ(gps_service_.checksum_errors(), 0u);
    EXPECT_EQ(gps_service_.sentences_parsed(), 1u);
    EXPECT_TRUE(gps_service_.latest_data().rmc_valid);
}

TEST_F(GpsServiceTest, TimestampsGpsSamplesWithSynchronizedRtcAndDegradedMonotonicFallback) {
    const hal::RtcDateTime dt = make_rtc_dt(2026, 11, 19, 4, 14, 25, 36);
    const uint64_t expected_base_epoch = bsw::RtcService::datetime_to_epoch_ms(dt);

    // Caso 1: RTC sincronizado
    EXPECT_CALL(mock_rtc_hal_, init(_)).WillOnce(Return(true));
    EXPECT_CALL(mock_rtc_hal_, get_monotonic_ms())
        .WillOnce(Return(1000ULL))
        .WillOnce(Return(1002ULL))
        .WillOnce(Return(1501ULL));
    EXPECT_CALL(mock_rtc_hal_, read_datetime(_, _))
        .WillOnce(DoAll(SetArgReferee<0>(dt),
                        SetArgReferee<1>(hal::RtcDeviceStatus{false}),
                        Return(true)));

    ASSERT_TRUE(rtc_service_.initialize());
    ASSERT_TRUE(gps_service_.parse_nmea_sentence(
        "$GPGGA,092725.00,4717.11399,N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,*5B"));
    EXPECT_TRUE(gps_service_.latest_data().time_synchronized);
    EXPECT_EQ(gps_service_.latest_data().monotonic_ms, 1501ULL);
    EXPECT_EQ(gps_service_.latest_data().timestamp_ms, expected_base_epoch + 500ULL);

    // Caso 2: RTC degradado (OSF ativo -> fallback monotônico de 1 ms)
    EXPECT_CALL(mock_rtc_hal_, init(_)).WillOnce(Return(true));
    EXPECT_CALL(mock_rtc_hal_, get_monotonic_ms())
        .WillOnce(Return(2000ULL))
        .WillOnce(Return(2000ULL))
        .WillOnce(Return(2345ULL));
    EXPECT_CALL(mock_rtc_hal_, read_datetime(_, _))
        .WillOnce(DoAll(SetArgReferee<0>(dt),
                        SetArgReferee<1>(hal::RtcDeviceStatus{true}),
                        Return(true)));

    EXPECT_FALSE(rtc_service_.initialize());
    ASSERT_TRUE(gps_service_.parse_nmea_sentence(
        "$GNRMC,142536.25,A,2736.05100,S,04831.12500,W,54.000,182.45,191126,,,A,V*3E"));
    EXPECT_FALSE(gps_service_.latest_data().time_synchronized);
    EXPECT_EQ(gps_service_.latest_data().monotonic_ms, 2345ULL);
    EXPECT_EQ(gps_service_.latest_data().timestamp_ms, 2345ULL);
}

TEST_F(GpsServiceTest, RejectsMalformedDateOrTimeOrOutOfRangeCoordinates) {
    auto make_nmea = [](const char* body) {
        uint8_t xor_val = 0u;
        for (const char* p = body; *p != '\0'; ++p) {
            xor_val ^= static_cast<uint8_t>(*p);
        }
        static constexpr char kHex[] = "0123456789ABCDEF";
        std::string s = "$";
        s += body;
        s += '*';
        s += kHex[(xor_val >> 4u) & 0x0Fu];
        s += kHex[xor_val & 0x0Fu];
        return s;
    };

    // Data inválida em RMC (30 de fevereiro)
    EXPECT_FALSE(gps_service_.parse_nmea_sentence(
        make_nmea("GNRMC,123519.00,A,2736.05100,S,04831.12500,W,10.0,90.0,300225,,,A,V")));
    // Mês 13 inválido
    EXPECT_FALSE(gps_service_.parse_nmea_sentence(
        make_nmea("GNRMC,123519.00,A,2736.05100,S,04831.12500,W,10.0,90.0,011326,,,A,V")));
    // Hora 25 inválida
    EXPECT_FALSE(gps_service_.parse_nmea_sentence(
        make_nmea("GPGGA,250000.00,4717.11399,N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,")));
    // Hemisfério inválido ('X')
    EXPECT_FALSE(gps_service_.parse_nmea_sentence(
        make_nmea("GPGGA,092725.00,4717.11399,X,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,")));
    // Latitude > 90 graus
    EXPECT_FALSE(gps_service_.parse_nmea_sentence(
        make_nmea("GPGGA,092725.00,9100.00000,N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,")));
    // Quantidade errada de campos (GGA com 14 campos e RMC com 11 campos)
    EXPECT_FALSE(gps_service_.parse_nmea_sentence(
        make_nmea("GPGGA,092725.00,4717.11399,N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,")));
    EXPECT_FALSE(gps_service_.parse_nmea_sentence(
        make_nmea("GPRMC,083559.00,A,4717.11437,N,00833.91522,E,0.004,77.52,091202,")));

    EXPECT_EQ(gps_service_.sentences_parsed(), 0u);
    EXPECT_EQ(gps_service_.checksum_errors(), 0u);
}

TEST_F(GpsServiceTest, HandlesLongFractionalFieldsWithoutNanAndRejectsInvalidTailOrRange) {
    auto make_nmea = [](const std::string& body) {
        uint8_t xor_val = 0u;
        for (char c : body) {
            xor_val ^= static_cast<uint8_t>(c);
        }
        static constexpr char kHex[] = "0123456789ABCDEF";
        std::string s = "$";
        s += body;
        s += '*';
        s += kHex[(xor_val >> 4u) & 0x0Fu];
        s += kHex[xor_val & 0x0Fu];
        return s;
    };

    const std::string frac_320(320u, '5');

    // 320-digit fractional altitude: ignored beyond 18 decimal digits, finite (no NaN)
    ASSERT_TRUE(gps_service_.parse_nmea_sentence(make_nmea(
        "GPGGA,092725.00,4717.11399,N,00833.91590,E,1,08,1.01,1." + frac_320 + ",M,48.0,M,,")));
    EXPECT_TRUE(std::isfinite(gps_service_.latest_data().altitude_m));
    EXPECT_NEAR(gps_service_.latest_data().altitude_m, 1.5555556f, 1e-5f);

    // 320-digit fractional latitude minutes: finite (no NaN)
    ASSERT_TRUE(gps_service_.parse_nmea_sentence(make_nmea(
        "GPGGA,092725.00,4717." + frac_320 + ",N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,")));
    EXPECT_TRUE(std::isfinite(gps_service_.latest_data().latitude_deg));
    EXPECT_NEAR(gps_service_.latest_data().latitude_deg, 47.0 + (17.555555555555556 / 60.0), 1e-7);

    // 320-digit fractional course: finite (no NaN)
    ASSERT_TRUE(gps_service_.parse_nmea_sentence(make_nmea(
        "GNRMC,142536.25,A,2736.05100,S,04831.12500,W,54.000,1." + frac_320 + ",191126,,,A,V")));
    EXPECT_TRUE(std::isfinite(gps_service_.latest_data().course_deg));
    EXPECT_NEAR(gps_service_.latest_data().course_deg, 1.5555556f, 1e-5f);

    const hal::GpsData baseline = gps_service_.latest_data();
    const uint32_t baseline_parsed = gps_service_.sentences_parsed();

    // Non-digit after 18th fractional digit must still be rejected
    const std::string bad_tail_frac = std::string(25u, '5') + "A" + std::string(295u, '5');
    EXPECT_FALSE(gps_service_.parse_nmea_sentence(make_nmea(
        "GPGGA,092725.00,4717.11399,N,00833.91590,E,1,08,1.01,1." + bad_tail_frac + ",M,48.0,M,,")));
    // Out-of-range 320-digit fractional latitude minutes (>= 60)
    EXPECT_FALSE(gps_service_.parse_nmea_sentence(make_nmea(
        "GPGGA,092725.00,4760." + frac_320 + ",N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,")));
    // Out-of-range 320-digit fractional course (> 360)
    EXPECT_FALSE(gps_service_.parse_nmea_sentence(make_nmea(
        "GNRMC,142536.25,A,2736.05100,S,04831.12500,W,54.000,360." + frac_320 + ",191126,,,A,V")));

    EXPECT_EQ(gps_service_.sentences_parsed(), baseline_parsed);
    EXPECT_EQ(std::memcmp(&gps_service_.latest_data(), &baseline, sizeof(hal::GpsData)), 0);
}

TEST_F(GpsServiceTest, RejectsDoubleAndFloatOverflowAndPreservesValidState) {
    auto make_nmea = [](const std::string& body) {
        uint8_t xor_val = 0u;
        for (char c : body) {
            xor_val ^= static_cast<uint8_t>(c);
        }
        static constexpr char kHex[] = "0123456789ABCDEF";
        std::string s = "$";
        s += body;
        s += '*';
        s += kHex[(xor_val >> 4u) & 0x0Fu];
        s += kHex[xor_val & 0x0Fu];
        return s;
    };

    ASSERT_TRUE(gps_service_.parse_nmea_sentence(
        "$GPGGA,092725.00,4717.11399,N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,*5B"));
    ASSERT_TRUE(gps_service_.parse_nmea_sentence(
        "$GNRMC,142536.25,A,2736.05100,S,04831.12500,W,54.000,182.45,191126,,,A,V*3E"));

    const hal::GpsData baseline = gps_service_.latest_data();
    const uint32_t baseline_parsed = gps_service_.sentences_parsed();

    // 320-digit integer HDOP overflows double (+inf) -> rejected
    const std::string int_320(320u, '9');
    EXPECT_FALSE(gps_service_.parse_nmea_sentence(make_nmea(
        "GPGGA,092725.00,4717.11399,N,00833.91590,E,1,08," + int_320 + ",499.6,M,48.0,M,,")));

    // 45-digit integer fields (~1e44 > FLT_MAX) overflow float -> rejected
    const std::string int_1e44 = "1" + std::string(44u, '0');
    EXPECT_FALSE(gps_service_.parse_nmea_sentence(make_nmea(
        "GPGGA,092725.00,4717.11399,N,00833.91590,E,1,08,1.01," + int_1e44 + ",M,48.0,M,,")));
    EXPECT_FALSE(gps_service_.parse_nmea_sentence(make_nmea(
        "GPGGA,092725.00,4717.11399,N,00833.91590,E,1,08,1.01,-" + int_1e44 + ",M,48.0,M,,")));
    EXPECT_FALSE(gps_service_.parse_nmea_sentence(make_nmea(
        "GPGGA,092725.00,4717.11399,N,00833.91590,E,1,08," + int_1e44 + ",499.6,M,48.0,M,,")));
    EXPECT_FALSE(gps_service_.parse_nmea_sentence(make_nmea(
        "GNRMC,142536.25,A,2736.05100,S,04831.12500,W," + int_1e44 + ",182.45,191126,,,A,V")));

    // 2.5e38 knots fits in float, but knots * kKnotsToKmh (> 4.6e38) overflows FLT_MAX -> rejected
    const std::string knots_2_5e38 = "25" + std::string(37u, '0');
    EXPECT_FALSE(gps_service_.parse_nmea_sentence(make_nmea(
        "GNRMC,142536.25,A,2736.05100,S,04831.12500,W," + knots_2_5e38 + ",182.45,191126,,,A,V")));

    EXPECT_EQ(gps_service_.sentences_parsed(), baseline_parsed);
    EXPECT_EQ(gps_service_.checksum_errors(), 0u);
    EXPECT_EQ(std::memcmp(&gps_service_.latest_data(), &baseline, sizeof(hal::GpsData)), 0);
}

TEST(Neo8mUartLldTest, UbxFramesMatchVerifiedProtocolBytes) {
    const auto prt_115200_ubx_nmea = lld::Neo8mUartLld::build_cfg_prt(115200u);
    const std::array<uint8_t, 28> expected_prt_115200{
        0xB5u, 0x62u, 0x06u, 0x00u, 0x14u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u,
        0xD0u, 0x08u, 0x00u, 0x00u, 0x00u, 0xC2u, 0x01u, 0x00u, 0x03u, 0x00u,
        0x03u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0xBCu, 0x5Eu};
    EXPECT_EQ(prt_115200_ubx_nmea, expected_prt_115200);

    const auto prt_115200_nmea_only = lld::Neo8mUartLld::build_cfg_prt(
        115200u,
        lld::Neo8mUartLld::kProtoMaskUbxNmea,
        lld::Neo8mUartLld::kProtoMaskNmeaOnly);
    EXPECT_EQ(prt_115200_nmea_only[26], 0xBBu);
    EXPECT_EQ(prt_115200_nmea_only[27], 0x58u);

    const auto prt_38400 = lld::Neo8mUartLld::build_cfg_prt(38400u);
    EXPECT_EQ(prt_38400[26], 0x8Fu);
    EXPECT_EQ(prt_38400[27], 0x70u);

    const auto rate_10hz_utc = lld::Neo8mUartLld::build_cfg_rate(100u, 1u, 0u);
    const std::array<uint8_t, 14> expected_rate_utc{
        0xB5u, 0x62u, 0x06u, 0x08u, 0x06u, 0x00u, 0x64u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x79u, 0x10u};
    EXPECT_EQ(rate_10hz_utc, expected_rate_utc);

    const auto rate_10hz_gps = lld::Neo8mUartLld::build_cfg_rate(100u, 1u, 1u);
    EXPECT_EQ(rate_10hz_gps[12], 0x7Au);
    EXPECT_EQ(rate_10hz_gps[13], 0x12u);

    const auto msg_gga_on = lld::Neo8mUartLld::build_cfg_msg(0xF0u, 0x00u, 1u);
    const std::array<uint8_t, 11> expected_gga{
        0xB5u, 0x62u, 0x06u, 0x01u, 0x03u, 0x00u, 0xF0u, 0x00u, 0x01u, 0xFBu, 0x10u};
    EXPECT_EQ(msg_gga_on, expected_gga);

    const auto msg_rmc_on = lld::Neo8mUartLld::build_cfg_msg(0xF0u, 0x04u, 1u);
    EXPECT_EQ(msg_rmc_on[9], 0xFFu);
    EXPECT_EQ(msg_rmc_on[10], 0x18u);

    const auto msg_gll_off = lld::Neo8mUartLld::build_cfg_msg(0xF0u, 0x01u, 0u);
    EXPECT_EQ(msg_gll_off[9], 0xFBu);
    EXPECT_EQ(msg_gll_off[10], 0x11u);

    const auto msg_gsa_off = lld::Neo8mUartLld::build_cfg_msg(0xF0u, 0x02u, 0u);
    EXPECT_EQ(msg_gsa_off[9], 0xFCu);
    EXPECT_EQ(msg_gsa_off[10], 0x13u);

    const auto msg_gsv_off = lld::Neo8mUartLld::build_cfg_msg(0xF0u, 0x03u, 0u);
    EXPECT_EQ(msg_gsv_off[9], 0xFDu);
    EXPECT_EQ(msg_gsv_off[10], 0x15u);

    const auto msg_vtg_off = lld::Neo8mUartLld::build_cfg_msg(0xF0u, 0x05u, 0u);
    EXPECT_EQ(msg_vtg_off[9], 0xFFu);
    EXPECT_EQ(msg_vtg_off[10], 0x19u);

    const auto nav5_auto = lld::Neo8mUartLld::build_cfg_nav5(4u);
    EXPECT_EQ(nav5_auto[6], 0x01u);
    EXPECT_EQ(nav5_auto[8], 0x04u);
    EXPECT_EQ(nav5_auto[42], 0x53u);
    EXPECT_EQ(nav5_auto[43], 0x70u);

    const auto nav5_auto_fix = lld::Neo8mUartLld::build_cfg_nav5(
        4u,
        lld::Neo8mUartLld::kNav5MaskDynAndFix,
        lld::Neo8mUartLld::kNav5FixModeAuto);
    EXPECT_EQ(nav5_auto_fix[6], 0x05u);
    EXPECT_EQ(nav5_auto_fix[9], 0x03u);
    EXPECT_EQ(nav5_auto_fix[42], 0x5Au);
    EXPECT_EQ(nav5_auto_fix[43], 0x63u);
}

TEST(Neo8mUartLldTest, InitRejectsInvalidPinsPortsBaudRatesAndDynModels) {
    lld::Neo8mUartLld lld;

    auto bad_rx = hal::GpsConfig{};
    bad_rx.rx_pin = GPIO_NUM_NC;
    EXPECT_FALSE(lld.init(bad_rx));

    auto flash_tx = hal::GpsConfig{};
    flash_tx.tx_pin = GPIO_NUM_26;
    EXPECT_FALSE(lld.init(flash_tx));

    auto same_pins = hal::GpsConfig{};
    same_pins.tx_pin = same_pins.rx_pin;
    EXPECT_FALSE(lld.init(same_pins));

    auto bad_port = hal::GpsConfig{};
    bad_port.uart_port = static_cast<uint8_t>(UART_NUM_MAX);
    EXPECT_FALSE(lld.init(bad_port));

    auto zero_baud = hal::GpsConfig{};
    zero_baud.operating_baud_rate = 0u;
    EXPECT_FALSE(lld.init(zero_baud));

    auto zero_rate = hal::GpsConfig{};
    zero_rate.update_rate_hz = 0u;
    EXPECT_FALSE(lld.init(zero_rate));

    auto high_rate = hal::GpsConfig{};
    high_rate.update_rate_hz = 11u;
    EXPECT_FALSE(lld.init(high_rate));

    auto reserved_dyn = hal::GpsConfig{};
    reserved_dyn.dynamic_model = 1u;
    EXPECT_FALSE(lld.init(reserved_dyn));

    auto out_of_range_dyn = hal::GpsConfig{};
    out_of_range_dyn.dynamic_model = 11u;
    EXPECT_FALSE(lld.init(out_of_range_dyn));

    EXPECT_FALSE(lld.is_initialized());
}

TEST(Neo8mUartLldTest, UninitializedIoIsSafeAndDefaultInitSucceeds) {
    lld::Neo8mUartLld lld;
    std::array<uint8_t, 16> rx_buf{};
    const std::array<uint8_t, 4> tx_buf{0x01u, 0x02u, 0x03u, 0x04u};

    EXPECT_FALSE(lld.is_initialized());
    EXPECT_EQ(lld.read_bytes(rx_buf.data(), rx_buf.size(), 10u), -1);
    EXPECT_FALSE(lld.write_bytes(tx_buf.data(), tx_buf.size()));

    ASSERT_TRUE(lld.init(hal::GpsConfig{}));
    EXPECT_TRUE(lld.is_initialized());
    EXPECT_EQ(lld.read_bytes(nullptr, rx_buf.size(), 10u), -1);
    EXPECT_EQ(lld.read_bytes(rx_buf.data(), 0u, 10u), 0);
    EXPECT_EQ(lld.read_bytes(rx_buf.data(), rx_buf.size(), 10u), 0);
    EXPECT_FALSE(lld.write_bytes(nullptr, 4u));
    EXPECT_FALSE(lld.write_bytes(tx_buf.data(), 0u));
    EXPECT_TRUE(lld.write_bytes(tx_buf.data(), tx_buf.size()));
}

} // namespace datalogger::sil
