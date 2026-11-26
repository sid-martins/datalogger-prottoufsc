#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <array>
#include <cstdint>
#include <string_view>

#include "engine_hours_service.hpp"
#include "mock_nvm_hal.hpp"
#include "nvs_lld.hpp"

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgReferee;
using ::testing::StrEq;

namespace datalogger::sil {

class EngineHoursServiceTest : public ::testing::Test {
protected:
    NiceMock<MockNvmHal> mock_nvm_;
    bsw::EngineHoursService service_{mock_nvm_};
};

TEST_F(EngineHoursServiceTest, FirstBootWithKeyNotFoundStartsAtZeroWithoutErrorOrSpuriousWrite) {
    EXPECT_CALL(mock_nvm_, init(_)).WillOnce(Return(true));
    EXPECT_CALL(mock_nvm_, read_u32(StrEq("eng_sec_tot"), _))
        .WillOnce(Return(hal::NvmReadResult::kNotFound));
    EXPECT_CALL(mock_nvm_, write_u32(_, _)).Times(0);
    EXPECT_CALL(mock_nvm_, commit()).Times(0);

    ASSERT_TRUE(service_.initialize());
    EXPECT_TRUE(service_.is_initialized());
    EXPECT_TRUE(service_.is_nvm_healthy());
    EXPECT_EQ(service_.total_seconds(), 0u);
    EXPECT_EQ(service_.total_minutes(), 0u);
    EXPECT_EQ(service_.hours_part(), 0u);
    EXPECT_EQ(service_.minutes_part(), 0u);
    EXPECT_EQ(service_.nvm_error_count(), 0u);

    std::array<char, bsw::EngineHoursService::kMinFormatBufSize> hhmm{};
    ASSERT_GT(service_.format_hhmm(hhmm.data(), hhmm.size()), 0u);
    EXPECT_EQ(std::string_view(hhmm.data()), "00:00");
}

TEST_F(EngineHoursServiceTest, RestoresPersistedSecondsOnBootAndFormatsHhMmIncludingAbove100Hours) {
    // 125h 08m 45s = 125 * 3600 + 8 * 60 + 45 = 450525 s
    constexpr uint32_t kSavedSeconds = 125u * 3600u + 8u * 60u + 45u;

    EXPECT_CALL(mock_nvm_, init(_)).WillOnce(Return(true));
    EXPECT_CALL(mock_nvm_, read_u32(StrEq("eng_sec_tot"), _))
        .WillOnce(DoAll(SetArgReferee<1>(kSavedSeconds), Return(hal::NvmReadResult::kOk)));

    ASSERT_TRUE(service_.initialize());
    EXPECT_EQ(service_.total_seconds(), kSavedSeconds);
    EXPECT_EQ(service_.hours_part(), 125u);
    EXPECT_EQ(service_.minutes_part(), 8u);
    EXPECT_EQ(service_.seconds_part(), 45u);
    EXPECT_EQ(service_.total_minutes(), 125u * 60u + 8u);

    std::array<char, bsw::EngineHoursService::kMinFormatBufSize> hhmm{};
    ASSERT_GT(service_.format_hhmm(hhmm.data(), hhmm.size()), 0u);
    EXPECT_EQ(std::string_view(hhmm.data()), "125:08");

    // Buffer insuficiente deve falhar com retorno 0 sem estouro
    std::array<char, 4> tiny{};
    EXPECT_EQ(service_.format_hhmm(tiny.data(), tiny.size()), 0u);
    EXPECT_EQ(service_.format_hhmm(nullptr, 16u), 0u);
}

TEST_F(EngineHoursServiceTest, CountsOnlyWhenEcuIsOnlineAndRpmStrictlyAbove600) {
    ON_CALL(mock_nvm_, init(_)).WillByDefault(Return(true));
    ON_CALL(mock_nvm_, read_u32(_, _)).WillByDefault(Return(hal::NvmReadResult::kNotFound));
    ASSERT_TRUE(service_.initialize());

    // Chave ligada com RPM = 0, arranque em 450 RPM, limiar exato 600 RPM e ECU offline com 3000 RPM -> 0 s
    for (uint64_t t = 1000ULL; t <= 10000ULL; t += 100ULL) {
        service_.update(t, 0u, true);
    }
    for (uint64_t t = 10100ULL; t <= 20000ULL; t += 100ULL) {
        service_.update(t, 600u, true);
    }
    for (uint64_t t = 20100ULL; t <= 30000ULL; t += 100ULL) {
        service_.update(t, 3000u, false);
    }

    EXPECT_FALSE(service_.is_engine_running());
    EXPECT_EQ(service_.total_seconds(), 0u);
}

TEST_F(EngineHoursServiceTest, CheckpointsEveryOneMinuteOfEngineRunningAndCommitsPartialSecondsOnStop) {
    // Começa com 01h 59m 00s = 7140 s salvos na NVS
    constexpr uint32_t kInitialSeconds = 1u * 3600u + 59u * 60u;
    ON_CALL(mock_nvm_, init(_)).WillByDefault(Return(true));
    ON_CALL(mock_nvm_, read_u32(_, _))
        .WillByDefault(DoAll(SetArgReferee<1>(kInitialSeconds), Return(hal::NvmReadResult::kOk)));
    ASSERT_TRUE(service_.initialize());

    {
        InSequence seq;
        // 1º checkpoint automático exatamente após 60 s de motor rodando (7140 + 60 = 7200 s = 02:00)
        EXPECT_CALL(mock_nvm_, write_u32(StrEq("eng_sec_tot"), 7200u)).WillOnce(Return(true));
        EXPECT_CALL(mock_nvm_, commit()).WillOnce(Return(true));

        // Mais 25 s de motor rodando e parada do motor -> commit imediato na parada (7225 s)
        EXPECT_CALL(mock_nvm_, write_u32(StrEq("eng_sec_tot"), 7225u)).WillOnce(Return(true));
        EXPECT_CALL(mock_nvm_, commit()).WillOnce(Return(true));
    }

    // Partida em t = 1000 ms (601 RPM) e 60 s contínuos em passos de 1000 ms
    service_.update(1000ULL, 1500u, true);
    for (uint64_t step = 1ULL; step <= 59ULL; ++step) {
        service_.update(1000ULL + step * 1000ULL, 2500u, true);
    }
    EXPECT_EQ(service_.save_count(), 0u); // 59 s ainda não atingiu 1 min

    // Atinge 60 s de funcionamento em t = 61000 ms -> grava 7200 s (02:00)
    service_.update(61000ULL, 2500u, true);
    EXPECT_EQ(service_.save_count(), 1u);
    EXPECT_EQ(service_.total_seconds(), 7200u);

    std::array<char, bsw::EngineHoursService::kMinFormatBufSize> hhmm{};
    service_.format_hhmm(hhmm.data(), hhmm.size());
    EXPECT_EQ(std::string_view(hhmm.data()), "02:00");

    // Roda mais 25 s (t = 62000..86000 ms) -> ainda sem novo checkpoint periódico
    for (uint64_t step = 1ULL; step <= 25ULL; ++step) {
        service_.update(61000ULL + step * 1000ULL, 2500u, true);
    }
    EXPECT_EQ(service_.save_count(), 1u);
    EXPECT_EQ(service_.total_seconds(), 7225u);

    // Motor apaga (ECU offline ou RPM <= 600) -> salva imediatamente os 25 s parciais!
    service_.update(86020ULL, 0u, false);
    EXPECT_FALSE(service_.is_engine_running());
    EXPECT_EQ(service_.save_count(), 2u);
    EXPECT_EQ(service_.persisted_seconds(), 7225u);

    // Permanecer parado não deve gerar escritas repetidas na Flash
    service_.update(90000ULL, 0u, false);
    EXPECT_EQ(service_.save_count(), 2u);
}

TEST_F(EngineHoursServiceTest, DegradesGracefullyWhenNvmFailsWhileStillTrackingInRam) {
    EXPECT_CALL(mock_nvm_, init(_)).WillOnce(Return(false));
    EXPECT_FALSE(service_.initialize());
    EXPECT_TRUE(service_.is_initialized());
    EXPECT_FALSE(service_.is_nvm_healthy());
    EXPECT_EQ(service_.nvm_error_count(), 1u);

    // Mesmo com NVS indisponível, contabiliza horas em RAM sem travar
    service_.update(0ULL, 2000u, true);
    for (uint64_t step = 1ULL; step <= 65ULL; ++step) {
        service_.update(step * 1000ULL, 2000u, true);
    }
    EXPECT_EQ(service_.total_seconds(), 65u);
    EXPECT_EQ(service_.total_minutes(), 1u);
}

TEST_F(EngineHoursServiceTest, SupportsManualSetAndResetForEngineRebuildMaintenance) {
    ON_CALL(mock_nvm_, init(_)).WillByDefault(Return(true));
    ON_CALL(mock_nvm_, read_u32(_, _)).WillByDefault(Return(hal::NvmReadResult::kNotFound));
    ASSERT_TRUE(service_.initialize());

    EXPECT_CALL(mock_nvm_, write_u32(StrEq("eng_sec_tot"), 36000u)).WillOnce(Return(true));
    EXPECT_CALL(mock_nvm_, commit()).WillOnce(Return(true));
    ASSERT_TRUE(service_.set_total_seconds(36000u)); // 10:00
    EXPECT_EQ(service_.hours_part(), 10u);
    EXPECT_EQ(service_.minutes_part(), 0u);

    EXPECT_CALL(mock_nvm_, write_u32(StrEq("eng_sec_tot"), 0u)).WillOnce(Return(true));
    EXPECT_CALL(mock_nvm_, commit()).WillOnce(Return(true));
    ASSERT_TRUE(service_.reset_hours());
    EXPECT_EQ(service_.total_seconds(), 0u);
    EXPECT_EQ(service_.persisted_seconds(), 0u);
}

TEST(NvsLldTest, ValidatesPartitionNamespaceAndKeyLengths) {
    lld::NvsLld nvs_lld;

    hal::NvmConfig bad_cfg{};
    bad_cfg.partition_name = "";
    EXPECT_FALSE(lld::NvsLld::is_valid_config(bad_cfg));
    EXPECT_FALSE(nvs_lld.init(bad_cfg));

    hal::NvmConfig too_long_ns{};
    too_long_ns.namespace_name = "namespace_muito_longo_16";
    EXPECT_FALSE(lld::NvsLld::is_valid_config(too_long_ns));

    hal::NvmConfig valid_cfg{};
    EXPECT_TRUE(lld::NvsLld::is_valid_config(valid_cfg));
    ASSERT_TRUE(nvs_lld.init(valid_cfg));

    uint32_t val = 0u;
    EXPECT_EQ(nvs_lld.read_u32(nullptr, val), hal::NvmReadResult::kError);
    EXPECT_EQ(nvs_lld.read_u32("chave_acima_de_15_chars", val), hal::NvmReadResult::kError);
    EXPECT_EQ(nvs_lld.read_u32("eng_sec_tot", val), hal::NvmReadResult::kNotFound);

    EXPECT_FALSE(nvs_lld.write_u32("", 10u));
    EXPECT_TRUE(nvs_lld.write_u32("eng_sec_tot", 120u));
    EXPECT_TRUE(nvs_lld.commit());
}

} // namespace datalogger::sil
