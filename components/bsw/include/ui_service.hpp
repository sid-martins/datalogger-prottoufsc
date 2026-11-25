#pragma once

#include <cstdint>
#include "i_gpio_hal.hpp"

namespace datalogger::bsw {

/**
 * @brief Estados operacionais da máquina de aquisição e diagnóstico de pista.
 */
enum class UiSystemState : uint8_t {
    kUninitialized = 0,
    kStandbyArmed  = 1, // Aguardando ECU online com RPM > 600
    kRecording     = 2, // Gravação ativa com motor acima do limiar de RPM
    kStopHold      = 3, // Retenção pós-queda de RPM (5 s antes de fechar o arquivo)
    kStorageFault  = 4  // Falha crítica de armazenamento MicroSD
};

/**
 * @brief Parâmetros de configuração do serviço de UI e acionamento automático por RPM.
 */
struct UiConfig {
    hal::UiGpioConfig gpio{};
    uint16_t rpm_start_threshold{600u};           // Inicia gravação quando RPM > 600
    uint64_t rpm_stop_hold_ms{5000ULL};           // Mantém 5 s após RPM <= 600 ou ECU offline
    uint64_t debounce_ms{50ULL};                  // Debounce de software dos botões (50 ms)
    uint64_t heartbeat_half_period_ms{500ULL};    // LED_Working normal: 1 Hz (500 ms ON / 500 ms OFF)
    uint64_t can_warn_half_period_ms{200ULL};     // LED_Working alerta CAN: 2.5 Hz (200 ms ON / OFF)
    uint64_t fault_blink_half_period_ms{100ULL};  // LED_Working falha SD: 5 Hz (100 ms ON / OFF)
};

/**
 * @brief Entradas consolidadas de telemetria e diagnóstico avaliadas a cada passo do `UiService`.
 */
struct UiSystemInputs {
    uint64_t now_ms{0};
    uint16_t ecu_rpm{0};
    bool ecu_online{false};
    bool session_active{false};
    bool storage_fault{false};
    bool can_warning{false};
};

/**
 * @brief BSW (Basic Software Service) - Interface de Pista e Máquina de Estados por RPM.
 *
 * Gerencia o início automático de gravação quando `ecu_online && ecu_rpm > 600`, a janela
 * de retenção de `5 s` para parada limpa sem fragmentar sessões, o botão `Free_SW1` em
 * modo reservado/standby, o marcador de volta (*Lap Beacon*) com debounce em `Free_SW2` e
 * a sinalização visual nos LEDs `LED_Acquiring` e `LED_Working` sem alocação dinâmica.
 */
class UiService {
public:
    explicit UiService(hal::IGpioHal& gpio_hal);

    /**
     * @brief Inicializa os GPIOs da UI, apaga ambos os LEDs e coloca o sistema em `kStandbyArmed`.
     */
    bool initialize(const UiConfig& config = UiConfig{});

    /**
     * @brief Executa um passo determinístico da máquina de estados, botões e LEDs.
     */
    bool update(const UiSystemInputs& inputs);

    /**
     * @brief Zera manualmente o contador de Lap Beacon.
     */
    void reset_lap_beacon() { lap_beacon_ = 0u; }

    bool is_initialized() const { return initialized_; }
    UiSystemState state() const { return state_; }
    bool recording_requested() const { return recording_requested_; }
    uint16_t lap_beacon() const { return lap_beacon_; }
    bool is_sw1_pressed() const { return sw1_debounce_.stable_pressed; }
    bool is_sw2_pressed() const { return sw2_debounce_.stable_pressed; }
    bool led_acquiring_on() const { return led_acquiring_on_; }
    bool led_working_on() const { return led_working_on_; }
    uint32_t gpio_error_count() const { return gpio_error_count_; }

private:
    struct ButtonDebounceState {
        bool raw_prev{false};
        bool stable_pressed{false};
        uint64_t candidate_since_ms{0};
    };

    static bool step_debounce(ButtonDebounceState& state,
                              bool raw_pressed,
                              uint64_t now_ms,
                              uint64_t debounce_ms);

    void update_rpm_state_machine(const UiSystemInputs& inputs);
    void sample_buttons(uint64_t now_ms);
    void drive_leds(const UiSystemInputs& inputs);

    hal::IGpioHal& gpio_hal_;
    UiConfig config_{};
    bool initialized_{false};
    UiSystemState state_{UiSystemState::kUninitialized};
    bool recording_requested_{false};
    bool stop_hold_active_{false};
    uint64_t stop_hold_start_ms_{0};
    uint16_t lap_beacon_{0};
    ButtonDebounceState sw1_debounce_{};
    ButtonDebounceState sw2_debounce_{};
    bool led_acquiring_on_{false};
    bool led_working_on_{false};
    bool leds_synced_{false};
    uint32_t gpio_error_count_{0};
};

} // namespace datalogger::bsw
