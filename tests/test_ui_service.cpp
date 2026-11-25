#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "bsp_pins.h"
#include "gpio_lld.hpp"
#include "mock_gpio_hal.hpp"
#include "ui_service.hpp"

using ::testing::_;
using ::testing::DoAll;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgReferee;

namespace datalogger::sil {

class UiServiceTest : public ::testing::Test {
protected:
    NiceMock<MockGpioHal> mock_gpio_hal_;
    bsw::UiService service_{mock_gpio_hal_};

    void init_default_ui(bool sw1_high = true, bool sw2_high = true) {
        ON_CALL(mock_gpio_hal_, init(_)).WillByDefault(Return(true));
        ON_CALL(mock_gpio_hal_, write_pin(_, _)).WillByDefault(Return(true));
        set_buttons_level(sw1_high, sw2_high);
        ASSERT_TRUE(service_.initialize());
    }

    void set_buttons_level(bool sw1_high, bool sw2_high) {
        ON_CALL(mock_gpio_hal_, read_pin(PIN_BTN_FREE_SW1, _))
            .WillByDefault(DoAll(SetArgReferee<1>(sw1_high), Return(true)));
        ON_CALL(mock_gpio_hal_, read_pin(PIN_BTN_FREE_SW2, _))
            .WillByDefault(DoAll(SetArgReferee<1>(sw2_high), Return(true)));
    }
};

TEST_F(UiServiceTest, DefaultConfigUsesBspPins600RpmThresholdAnd5sHold) {
    const bsw::UiConfig cfg{};
    EXPECT_EQ(cfg.gpio.sw1_pin, PIN_BTN_FREE_SW1);
    EXPECT_EQ(cfg.gpio.sw1_pin, GPIO_NUM_35);
    EXPECT_EQ(cfg.gpio.sw2_beacon_pin, PIN_BTN_FREE_SW2);
    EXPECT_EQ(cfg.gpio.sw2_beacon_pin, GPIO_NUM_36);
    EXPECT_EQ(cfg.gpio.led_acquiring_pin, PIN_LED_ACQUIRING);
    EXPECT_EQ(cfg.gpio.led_acquiring_pin, GPIO_NUM_37);
    EXPECT_EQ(cfg.gpio.led_working_pin, PIN_LED_WORKING);
    EXPECT_EQ(cfg.gpio.led_working_pin, GPIO_NUM_39);
    EXPECT_EQ(cfg.rpm_start_threshold, 600u);
    EXPECT_EQ(cfg.rpm_stop_hold_ms, 5000ULL);
    EXPECT_EQ(cfg.debounce_ms, 50ULL);
}

TEST_F(UiServiceTest, InitializeTurnsBothLedsOffAndHandlesHalFailure) {
    EXPECT_FALSE(service_.is_initialized());
    EXPECT_EQ(service_.state(), bsw::UiSystemState::kUninitialized);
    EXPECT_FALSE(service_.update(bsw::UiSystemInputs{}));

    EXPECT_CALL(mock_gpio_hal_, init(_)).WillOnce(Return(false));
    EXPECT_FALSE(service_.initialize());
    EXPECT_FALSE(service_.is_initialized());
    EXPECT_EQ(service_.state(), bsw::UiSystemState::kUninitialized);

    EXPECT_CALL(mock_gpio_hal_, init(_)).WillOnce(Return(true));
    EXPECT_CALL(mock_gpio_hal_, write_pin(PIN_LED_ACQUIRING, false)).WillOnce(Return(true));
    EXPECT_CALL(mock_gpio_hal_, write_pin(PIN_LED_WORKING, false)).WillOnce(Return(true));
    ASSERT_TRUE(service_.initialize());
    EXPECT_TRUE(service_.is_initialized());
    EXPECT_EQ(service_.state(), bsw::UiSystemState::kStandbyArmed);
    EXPECT_FALSE(service_.recording_requested());
    EXPECT_EQ(service_.lap_beacon(), 0u);
}

TEST_F(UiServiceTest, StaysInStandbyUntilEcuIsOnlineAndRpmStrictlyExceeds600) {
    init_default_ui();

    // ECU offline mesmo com RPM residual > 600 -> NÃO inicia gravação
    bsw::UiSystemInputs in{};
    in.now_ms = 100ULL;
    in.ecu_online = false;
    in.ecu_rpm = 1500u;
    ASSERT_TRUE(service_.update(in));
    EXPECT_FALSE(service_.recording_requested());
    EXPECT_EQ(service_.state(), bsw::UiSystemState::kStandbyArmed);

    // ECU online com RPM == 600 (limite exato) -> NÃO inicia gravação
    in.now_ms = 200ULL;
    in.ecu_online = true;
    in.ecu_rpm = 600u;
    ASSERT_TRUE(service_.update(in));
    EXPECT_FALSE(service_.recording_requested());
    EXPECT_EQ(service_.state(), bsw::UiSystemState::kStandbyArmed);

    // ECU online com RPM == 601 -> INICIA gravação automaticamente
    in.now_ms = 300ULL;
    in.ecu_online = true;
    in.ecu_rpm = 601u;
    ASSERT_TRUE(service_.update(in));
    EXPECT_TRUE(service_.recording_requested());
    EXPECT_EQ(service_.state(), bsw::UiSystemState::kRecording);
}

TEST_F(UiServiceTest, HoldsRecordingFor5SecondsWhenRpmDropsAndResumesWithoutStoppingIfRpmRecovers) {
    init_default_ui();

    bsw::UiSystemInputs in{};
    in.now_ms = 1000ULL;
    in.ecu_online = true;
    in.ecu_rpm = 2500u;
    in.session_active = true;
    ASSERT_TRUE(service_.update(in));
    ASSERT_TRUE(service_.recording_requested());
    EXPECT_EQ(service_.state(), bsw::UiSystemState::kRecording);

    // Queda de RPM para 450 em t = 2000 ms -> entra em kStopHold, mantendo gravação ativa
    in.now_ms = 2000ULL;
    in.ecu_rpm = 450u;
    ASSERT_TRUE(service_.update(in));
    EXPECT_TRUE(service_.recording_requested());
    EXPECT_EQ(service_.state(), bsw::UiSystemState::kStopHold);

    // Em t = 6999 ms (4999 ms de queda < 5000 ms) -> ainda mantém gravação ativa
    in.now_ms = 6999ULL;
    ASSERT_TRUE(service_.update(in));
    EXPECT_TRUE(service_.recording_requested());
    EXPECT_EQ(service_.state(), bsw::UiSystemState::kStopHold);

    // Motor recupera para 1200 RPM antes de completar 5 s -> cancela hold sem fechar arquivo
    in.now_ms = 7000ULL;
    in.ecu_rpm = 1200u;
    ASSERT_TRUE(service_.update(in));
    EXPECT_TRUE(service_.recording_requested());
    EXPECT_EQ(service_.state(), bsw::UiSystemState::kRecording);

    // Nova queda em t = 10000 ms que persiste até t = 15000 ms (5000 ms contínuos) -> encerra sessão!
    in.now_ms = 10000ULL;
    in.ecu_rpm = 0u;
    ASSERT_TRUE(service_.update(in));
    EXPECT_TRUE(service_.recording_requested());
    EXPECT_EQ(service_.state(), bsw::UiSystemState::kStopHold);

    in.now_ms = 14999ULL;
    ASSERT_TRUE(service_.update(in));
    EXPECT_TRUE(service_.recording_requested());

    in.now_ms = 15000ULL;
    ASSERT_TRUE(service_.update(in));
    EXPECT_FALSE(service_.recording_requested());
    EXPECT_EQ(service_.state(), bsw::UiSystemState::kStandbyArmed);
}

TEST_F(UiServiceTest, StopsSessionAfter5SecondsWhenEcuGoesOfflineEvenIfLastRpmStuckHigh) {
    init_default_ui();

    bsw::UiSystemInputs in{};
    in.now_ms = 1000ULL;
    in.ecu_online = true;
    in.ecu_rpm = 3200u;
    in.session_active = true;
    ASSERT_TRUE(service_.update(in));
    ASSERT_TRUE(service_.recording_requested());

    // Ignição desligada -> ECU para de enviar CAN (ecu_online = false), último RPM congelado em 3200
    in.now_ms = 2000ULL;
    in.ecu_online = false;
    in.ecu_rpm = 3200u;
    ASSERT_TRUE(service_.update(in));
    EXPECT_TRUE(service_.recording_requested());
    EXPECT_EQ(service_.state(), bsw::UiSystemState::kStopHold);

    in.now_ms = 6999ULL;
    ASSERT_TRUE(service_.update(in));
    EXPECT_TRUE(service_.recording_requested());

    in.now_ms = 7000ULL;
    ASSERT_TRUE(service_.update(in));
    EXPECT_FALSE(service_.recording_requested());
    EXPECT_EQ(service_.state(), bsw::UiSystemState::kStandbyArmed);
}

TEST_F(UiServiceTest, FreeSw1IsStandbyOnlyAndDoesNotStartOrStopRecordingOrChangeBeacon) {
    init_default_ui(true, true);

    bsw::UiSystemInputs in{};
    in.now_ms = 0ULL;
    in.ecu_online = true;
    in.ecu_rpm = 0u;
    ASSERT_TRUE(service_.update(in));

    // Pressiona Free_SW1 (ativo baixo -> false) por > 50 ms em standby: não inicia gravação
    set_buttons_level(false, true);
    in.now_ms = 10ULL;
    ASSERT_TRUE(service_.update(in));
    in.now_ms = 80ULL;
    ASSERT_TRUE(service_.update(in));
    EXPECT_TRUE(service_.is_sw1_pressed());
    EXPECT_FALSE(service_.recording_requested());
    EXPECT_EQ(service_.lap_beacon(), 0u);

    // Solta Free_SW1, inicia gravação via RPM > 600 e pressiona Free_SW1 novamente: não para gravação
    set_buttons_level(true, true);
    in.now_ms = 150ULL;
    in.ecu_rpm = 1800u;
    in.session_active = true;
    ASSERT_TRUE(service_.update(in));
    ASSERT_TRUE(service_.recording_requested());

    set_buttons_level(false, true);
    in.now_ms = 200ULL;
    ASSERT_TRUE(service_.update(in));
    in.now_ms = 270ULL;
    ASSERT_TRUE(service_.update(in));
    EXPECT_TRUE(service_.is_sw1_pressed());
    EXPECT_TRUE(service_.recording_requested());
    EXPECT_EQ(service_.state(), bsw::UiSystemState::kRecording);
    EXPECT_EQ(service_.lap_beacon(), 0u);
}

TEST_F(UiServiceTest, FreeSw2LapBeaconDebouncesActiveLowEdgesAndResetsOnNewSessionStart) {
    init_default_ui(true, true);

    bsw::UiSystemInputs in{};
    in.now_ms = 0ULL;
    in.ecu_online = true;
    in.ecu_rpm = 2000u;
    in.session_active = true;
    ASSERT_TRUE(service_.update(in));
    EXPECT_EQ(service_.lap_beacon(), 0u);

    // Ruído curto (< 50 ms) em Free_SW2 (LOW em t=100, volta HIGH em t=130) -> ignorado
    set_buttons_level(true, false);
    in.now_ms = 100ULL;
    ASSERT_TRUE(service_.update(in));
    EXPECT_EQ(service_.lap_beacon(), 0u);

    set_buttons_level(true, true);
    in.now_ms = 130ULL;
    ASSERT_TRUE(service_.update(in));
    EXPECT_EQ(service_.lap_beacon(), 0u);

    // Pressionamento válido (>= 50 ms) em t=200..250 -> incrementa lap_beacon para 1
    set_buttons_level(true, false);
    in.now_ms = 200ULL;
    ASSERT_TRUE(service_.update(in));
    in.now_ms = 249ULL;
    ASSERT_TRUE(service_.update(in));
    EXPECT_EQ(service_.lap_beacon(), 0u);

    in.now_ms = 250ULL;
    ASSERT_TRUE(service_.update(in));
    EXPECT_EQ(service_.lap_beacon(), 1u);

    // Mantendo pressionado em t=300..500 -> não repete incremento
    in.now_ms = 500ULL;
    ASSERT_TRUE(service_.update(in));
    EXPECT_EQ(service_.lap_beacon(), 1u);

    // Solta por >= 50 ms (t=510..560) e pressiona novamente (t=600..650) -> lap_beacon = 2
    set_buttons_level(true, true);
    in.now_ms = 510ULL;
    ASSERT_TRUE(service_.update(in));
    in.now_ms = 560ULL;
    ASSERT_TRUE(service_.update(in));

    set_buttons_level(true, false);
    in.now_ms = 600ULL;
    ASSERT_TRUE(service_.update(in));
    in.now_ms = 650ULL;
    ASSERT_TRUE(service_.update(in));
    EXPECT_EQ(service_.lap_beacon(), 2u);

    // Solta botão, para o motor por 5 s (encerra sessão) e liga novamente -> lap_beacon zera para nova sessão
    set_buttons_level(true, true);
    in.now_ms = 700ULL;
    in.ecu_rpm = 0u;
    ASSERT_TRUE(service_.update(in));
    in.now_ms = 5700ULL;
    in.session_active = false;
    ASSERT_TRUE(service_.update(in));
    ASSERT_FALSE(service_.recording_requested());

    in.now_ms = 6000ULL;
    in.ecu_rpm = 1500u;
    ASSERT_TRUE(service_.update(in));
    EXPECT_TRUE(service_.recording_requested());
    EXPECT_EQ(service_.lap_beacon(), 0u);
}

TEST_F(UiServiceTest, LedAcquiringReflectsRealActiveSessionAndLedWorkingFollowsDiagnosticPriority) {
    init_default_ui();

    bsw::UiSystemInputs in{};
    in.ecu_online = true;
    in.ecu_rpm = 2000u;

    // 1. Motor > 600 RPM mas sessão ainda não abriu (session_active = false): LED_Acquiring OFF, Heartbeat 1 Hz (500 ms)
    in.now_ms = 0ULL;
    in.session_active = false;
    ASSERT_TRUE(service_.update(in));
    EXPECT_FALSE(service_.led_acquiring_on());
    EXPECT_TRUE(service_.led_working_on()); // 0..499 ms ON

    in.now_ms = 500ULL;
    ASSERT_TRUE(service_.update(in));
    EXPECT_FALSE(service_.led_working_on()); // 500..999 ms OFF

    // 2. Sessão ativa (session_active = true): LED_Acquiring SOLID ON
    in.now_ms = 1000ULL;
    in.session_active = true;
    ASSERT_TRUE(service_.update(in));
    EXPECT_TRUE(service_.led_acquiring_on());
    EXPECT_TRUE(service_.led_working_on());

    // 3. Alerta CAN (can_warning = true, sem falha SD): LED_Working pisca em 2.5 Hz (200 ms ON / 200 ms OFF)
    in.can_warning = true;
    in.now_ms = 1200ULL; // (1200 / 200) = 6 (par -> ON)
    ASSERT_TRUE(service_.update(in));
    EXPECT_TRUE(service_.led_working_on());

    in.now_ms = 1400ULL; // (1400 / 200) = 7 (ímpar -> OFF)
    ASSERT_TRUE(service_.update(in));
    EXPECT_FALSE(service_.led_working_on());

    // 4. Falha crítica de SD (storage_fault = true) tem prioridade máxima:
    //    estado kStorageFault, LED_Acquiring OFF e LED_Working pisca rápido em 5 Hz (100 ms ON / 100 ms OFF)
    in.storage_fault = true;
    in.now_ms = 1500ULL; // (1500 / 100) = 15 (ímpar -> OFF)
    ASSERT_TRUE(service_.update(in));
    EXPECT_EQ(service_.state(), bsw::UiSystemState::kStorageFault);
    EXPECT_FALSE(service_.led_acquiring_on());
    EXPECT_FALSE(service_.led_working_on());

    in.now_ms = 1600ULL; // (1600 / 100) = 16 (par -> ON)
    ASSERT_TRUE(service_.update(in));
    EXPECT_TRUE(service_.led_working_on());
}

TEST(GpioLldTest, ValidatesPinsRejectsFlashAndDuplicatePinsAndOperatesWithStub) {
    lld::GpioLld gpio_lld;

    bool level = false;
    EXPECT_FALSE(gpio_lld.is_initialized());
    EXPECT_FALSE(gpio_lld.read_pin(PIN_BTN_FREE_SW2, level));
    EXPECT_FALSE(gpio_lld.write_pin(PIN_LED_WORKING, true));

    // Pino reservado à Flash SPI (GPIO 26..32) deve ser rejeitado
    hal::UiGpioConfig bad_flash{};
    bad_flash.sw1_pin = GPIO_NUM_26;
    EXPECT_FALSE(lld::GpioLld::is_valid_config(bad_flash));
    EXPECT_FALSE(gpio_lld.init(bad_flash));

    // Pino duplicado deve ser rejeitado
    hal::UiGpioConfig dup_cfg{};
    dup_cfg.led_acquiring_pin = dup_cfg.led_working_pin;
    EXPECT_FALSE(lld::GpioLld::is_valid_config(dup_cfg));
    EXPECT_FALSE(gpio_lld.init(dup_cfg));

    // Configuração padrão BSP (GPIO 35, 36, 37, 39) deve inicializar e operar corretamente
    const hal::UiGpioConfig valid_cfg{};
    EXPECT_TRUE(lld::GpioLld::is_valid_config(valid_cfg));
    ASSERT_TRUE(gpio_lld.init(valid_cfg));
    EXPECT_TRUE(gpio_lld.is_initialized());

    EXPECT_TRUE(gpio_lld.read_pin(PIN_BTN_FREE_SW1, level));
    EXPECT_TRUE(gpio_lld.read_pin(PIN_BTN_FREE_SW2, level));
    EXPECT_TRUE(gpio_lld.write_pin(PIN_LED_ACQUIRING, true));
    EXPECT_TRUE(gpio_lld.write_pin(PIN_LED_WORKING, false));

    // Escrita em pino de entrada (botão) deve ser rejeitada por segurança
    EXPECT_FALSE(gpio_lld.write_pin(PIN_BTN_FREE_SW2, true));
}

} // namespace datalogger::sil
