#include "neo8m_uart_lld.hpp"

#include <limits>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace datalogger::lld {

namespace {

constexpr const char* kTag = "NEO8M_UART_LLD";

bool is_valid_gpio_pin(gpio_num_t pin) {
    const int raw = static_cast<int>(pin);
    if (raw < 0 || raw >= static_cast<int>(GPIO_NUM_MAX)) {
        return false;
    }
    // Pinos reservados à Flash SPI interna no ESP32-S3 (GPIO 26..32)
    if (raw >= 26 && raw <= 32) {
        return false;
    }
    return true;
}

bool is_valid_dynamic_model(uint8_t dyn_model) {
    // UBX-13003221: dynModel válidos são 0 (Portable) e 2..10 (1 é reservado)
    return (dyn_model != 1u) && (dyn_model <= Neo8mUartLld::kMaxDynamicModel);
}

void write_le16(uint8_t* dst, uint16_t value) {
    dst[0] = static_cast<uint8_t>(value & 0xFFu);
    dst[1] = static_cast<uint8_t>((value >> 8u) & 0xFFu);
}

void write_le32(uint8_t* dst, uint32_t value) {
    dst[0] = static_cast<uint8_t>(value & 0xFFu);
    dst[1] = static_cast<uint8_t>((value >> 8u) & 0xFFu);
    dst[2] = static_cast<uint8_t>((value >> 16u) & 0xFFu);
    dst[3] = static_cast<uint8_t>((value >> 24u) & 0xFFu);
}

void finalize_ubx_frame(uint8_t* frame,
                        std::size_t frame_len,
                        uint8_t msg_class,
                        uint8_t msg_id,
                        uint16_t payload_len) {
    frame[0] = Neo8mUartLld::kUbxSync1;
    frame[1] = Neo8mUartLld::kUbxSync2;
    frame[2] = msg_class;
    frame[3] = msg_id;
    write_le16(&frame[4], payload_len);

    uint8_t ck_a = 0u;
    uint8_t ck_b = 0u;
    const std::size_t checksum_span = 4u + static_cast<std::size_t>(payload_len);
    Neo8mUartLld::compute_ubx_checksum(&frame[2], checksum_span, ck_a, ck_b);
    frame[frame_len - 2u] = ck_a;
    frame[frame_len - 1u] = ck_b;
}

} // namespace

Neo8mUartLld::~Neo8mUartLld() {
    deinit();
}

void Neo8mUartLld::deinit() {
    if (driver_installed_) {
        uart_driver_delete(uart_port_);
        driver_installed_ = false;
    }
    initialized_ = false;
}

bool Neo8mUartLld::init(const hal::GpsConfig& config) {
    deinit();

    if (!is_valid_gpio_pin(config.rx_pin) ||
        !is_valid_gpio_pin(config.tx_pin) ||
        config.rx_pin == config.tx_pin) {
        ESP_LOGE(kTag, "Configuracao de pinos UART invalida para GPS Neo-8M");
        return false;
    }

    if (static_cast<int>(config.uart_port) >= static_cast<int>(UART_NUM_MAX)) {
        ESP_LOGE(kTag, "Porta UART invalida: %u", static_cast<unsigned>(config.uart_port));
        return false;
    }

    const uint32_t max_int_baud = static_cast<uint32_t>(std::numeric_limits<int>::max());
    if (config.initial_baud_rate == 0u ||
        config.operating_baud_rate == 0u ||
        config.initial_baud_rate > max_int_baud ||
        config.operating_baud_rate > max_int_baud) {
        ESP_LOGE(kTag, "Baud rate UART invalido");
        return false;
    }

    if (config.update_rate_hz == 0u || config.update_rate_hz > kMaxUpdateRateHz) {
        ESP_LOGE(kTag, "Taxa de atualizacao invalida para GNSS concorrente: %u Hz",
                 static_cast<unsigned>(config.update_rate_hz));
        return false;
    }

    if (!is_valid_dynamic_model(config.dynamic_model)) {
        ESP_LOGE(kTag, "Modelo dinamico UBX-CFG-NAV5 invalido: %u",
                 static_cast<unsigned>(config.dynamic_model));
        return false;
    }

    const uart_port_t port = static_cast<uart_port_t>(config.uart_port);
    if (uart_driver_install(port, kUartRxBufferSize, kUartTxBufferSize, 0, nullptr, 0) != ESP_OK) {
        ESP_LOGE(kTag, "Falha em uart_driver_install");
        return false;
    }
    uart_port_ = port;
    driver_installed_ = true;

    uart_config_t uart_cfg{};
    uart_cfg.baud_rate = static_cast<int>(config.initial_baud_rate);
    uart_cfg.data_bits = UART_DATA_8_BITS;
    uart_cfg.parity = UART_PARITY_DISABLE;
    uart_cfg.stop_bits = UART_STOP_BITS_1;
    uart_cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_cfg.rx_flow_ctrl_thresh = 0u;
    uart_cfg.source_clk = UART_SCLK_DEFAULT;

    if (uart_param_config(port, &uart_cfg) != ESP_OK) {
        ESP_LOGE(kTag, "Falha em uart_param_config");
        deinit();
        return false;
    }

    if (uart_set_pin(port,
                     static_cast<int>(config.tx_pin),
                     static_cast<int>(config.rx_pin),
                     UART_PIN_NO_CHANGE,
                     UART_PIN_NO_CHANGE) != ESP_OK) {
        ESP_LOGE(kTag, "Falha em uart_set_pin");
        deinit();
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(kWarmupSettleMs));
    uart_flush_input(port);

    // Passo 1: Configura UART1 do módulo para operating_baud_rate (115200 8N1, UBX+NMEA)
    const auto cfg_prt = build_cfg_prt(config.operating_baud_rate);
    if (!send_ubx_frame(cfg_prt.data(), cfg_prt.size())) {
        ESP_LOGE(kTag, "Falha ao enviar UBX-CFG-PRT inicial");
        deinit();
        return false;
    }

    if (config.operating_baud_rate != config.initial_baud_rate) {
        if (uart_set_baudrate(port, config.operating_baud_rate) != ESP_OK) {
            ESP_LOGE(kTag, "Falha ao alterar baud rate UART do host");
            deinit();
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(kBaudSwitchSettleMs));
        // Reenvia no baud operacional para convergir caso o módulo estivesse em warm-start
        if (!send_ubx_frame(cfg_prt.data(), cfg_prt.size())) {
            ESP_LOGE(kTag, "Falha ao reenviar UBX-CFG-PRT no baud operacional");
            deinit();
            return false;
        }
    }
    uart_flush_input(port);

    // Passo 2: Habilita apenas GGA e RMC, desabilitando GLL, GSA, GSV e VTG
    const auto msg_gga = build_cfg_msg(kNmeaClass, kNmeaIdGga, 1u);
    const auto msg_rmc = build_cfg_msg(kNmeaClass, kNmeaIdRmc, 1u);
    const auto msg_gll = build_cfg_msg(kNmeaClass, kNmeaIdGll, 0u);
    const auto msg_gsa = build_cfg_msg(kNmeaClass, kNmeaIdGsa, 0u);
    const auto msg_gsv = build_cfg_msg(kNmeaClass, kNmeaIdGsv, 0u);
    const auto msg_vtg = build_cfg_msg(kNmeaClass, kNmeaIdVtg, 0u);

    if (!send_ubx_frame(msg_gga.data(), msg_gga.size()) ||
        !send_ubx_frame(msg_rmc.data(), msg_rmc.size()) ||
        !send_ubx_frame(msg_gll.data(), msg_gll.size()) ||
        !send_ubx_frame(msg_gsa.data(), msg_gsa.size()) ||
        !send_ubx_frame(msg_gsv.data(), msg_gsv.size()) ||
        !send_ubx_frame(msg_vtg.data(), msg_vtg.size())) {
        ESP_LOGE(kTag, "Falha ao configurar filtro de sentencas UBX-CFG-MSG");
        deinit();
        return false;
    }

    // Passo 3: Configura taxa de navegação para 10 Hz (measRate = 100 ms)
    const uint16_t meas_rate_ms = static_cast<uint16_t>(1000u / config.update_rate_hz);
    const auto cfg_rate = build_cfg_rate(meas_rate_ms, 1u, 0u);
    if (!send_ubx_frame(cfg_rate.data(), cfg_rate.size())) {
        ESP_LOGE(kTag, "Falha ao enviar UBX-CFG-RATE");
        deinit();
        return false;
    }

    // Passo 4: Configura modelo dinâmico Automotive (dynModel = 4)
    const auto cfg_nav5 = build_cfg_nav5(config.dynamic_model);
    if (!send_ubx_frame(cfg_nav5.data(), cfg_nav5.size())) {
        ESP_LOGE(kTag, "Falha ao enviar UBX-CFG-NAV5");
        deinit();
        return false;
    }

    config_ = config;
    initialized_ = true;
    ESP_LOGI(kTag, "GPS Neo-8M inicializado na UART%u (%lu baud, %u Hz, dynModel=%u)",
             static_cast<unsigned>(config.uart_port),
             static_cast<unsigned long>(config.operating_baud_rate),
             static_cast<unsigned>(config.update_rate_hz),
             static_cast<unsigned>(config.dynamic_model));
    return true;
}

bool Neo8mUartLld::send_ubx_frame(const uint8_t* frame, std::size_t len) {
    if (!driver_installed_ || frame == nullptr || len == 0u) {
        return false;
    }
    const int written = uart_write_bytes(uart_port_, frame, len);
    if (written != static_cast<int>(len)) {
        return false;
    }
    return uart_wait_tx_done(uart_port_, pdMS_TO_TICKS(kTxDoneTimeoutMs)) == ESP_OK;
}

int Neo8mUartLld::read_bytes(uint8_t* buffer, std::size_t max_len, uint32_t timeout_ms) {
    if (!initialized_ || !driver_installed_ || buffer == nullptr) {
        return -1;
    }
    if (max_len == 0u) {
        return 0;
    }
    const std::size_t max_int = static_cast<std::size_t>(std::numeric_limits<int>::max());
    const uint32_t clamped_len = static_cast<uint32_t>((max_len <= max_int) ? max_len : max_int);
    return uart_read_bytes(uart_port_, buffer, clamped_len, pdMS_TO_TICKS(timeout_ms));
}

bool Neo8mUartLld::write_bytes(const uint8_t* data, std::size_t len) {
    if (!initialized_ || !driver_installed_ || data == nullptr || len == 0u) {
        return false;
    }
    if (len > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return false;
    }
    return uart_write_bytes(uart_port_, data, len) == static_cast<int>(len);
}

void Neo8mUartLld::compute_ubx_checksum(const uint8_t* data,
                                        std::size_t len,
                                        uint8_t& out_ck_a,
                                        uint8_t& out_ck_b) {
    uint8_t ck_a = 0u;
    uint8_t ck_b = 0u;
    if (data != nullptr) {
        for (std::size_t i = 0u; i < len; ++i) {
            ck_a = static_cast<uint8_t>(ck_a + data[i]);
            ck_b = static_cast<uint8_t>(ck_b + ck_a);
        }
    }
    out_ck_a = ck_a;
    out_ck_b = ck_b;
}

std::array<uint8_t, Neo8mUartLld::kCfgPrtFrameLen> Neo8mUartLld::build_cfg_prt(
    uint32_t baud_rate,
    uint16_t in_proto_mask,
    uint16_t out_proto_mask) {
    std::array<uint8_t, kCfgPrtFrameLen> frame{};
    frame[kUbxHeaderLen + 0u] = kUartPortId;
    write_le32(&frame[kUbxHeaderLen + 4u], kUartMode8N1);
    write_le32(&frame[kUbxHeaderLen + 8u], baud_rate);
    write_le16(&frame[kUbxHeaderLen + 12u], in_proto_mask);
    write_le16(&frame[kUbxHeaderLen + 14u], out_proto_mask);
    finalize_ubx_frame(frame.data(), frame.size(), kUbxClassCfg, kUbxIdCfgPrt, kCfgPrtPayloadLen);
    return frame;
}

std::array<uint8_t, Neo8mUartLld::kCfgMsgFrameLen> Neo8mUartLld::build_cfg_msg(
    uint8_t msg_class,
    uint8_t msg_id,
    uint8_t rate) {
    std::array<uint8_t, kCfgMsgFrameLen> frame{};
    frame[kUbxHeaderLen + 0u] = msg_class;
    frame[kUbxHeaderLen + 1u] = msg_id;
    frame[kUbxHeaderLen + 2u] = rate;
    finalize_ubx_frame(frame.data(), frame.size(), kUbxClassCfg, kUbxIdCfgMsg, kCfgMsgPayloadLen);
    return frame;
}

std::array<uint8_t, Neo8mUartLld::kCfgRateFrameLen> Neo8mUartLld::build_cfg_rate(
    uint16_t meas_rate_ms,
    uint16_t nav_rate,
    uint16_t time_ref) {
    std::array<uint8_t, kCfgRateFrameLen> frame{};
    write_le16(&frame[kUbxHeaderLen + 0u], meas_rate_ms);
    write_le16(&frame[kUbxHeaderLen + 2u], nav_rate);
    write_le16(&frame[kUbxHeaderLen + 4u], time_ref);
    finalize_ubx_frame(frame.data(), frame.size(), kUbxClassCfg, kUbxIdCfgRate, kCfgRatePayloadLen);
    return frame;
}

std::array<uint8_t, Neo8mUartLld::kCfgNav5FrameLen> Neo8mUartLld::build_cfg_nav5(
    uint8_t dyn_model,
    uint16_t mask,
    uint8_t fix_mode) {
    std::array<uint8_t, kCfgNav5FrameLen> frame{};
    write_le16(&frame[kUbxHeaderLen + 0u], mask);
    frame[kUbxHeaderLen + 2u] = dyn_model;
    frame[kUbxHeaderLen + 3u] = fix_mode;
    finalize_ubx_frame(frame.data(), frame.size(), kUbxClassCfg, kUbxIdCfgNav5, kCfgNav5PayloadLen);
    return frame;
}

} // namespace datalogger::lld
