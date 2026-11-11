#pragma once

/**
 * @file board_pins.h
 * @brief Mapeamento centralizado de Portas/GPIOs e Tags do Datalogger (ESP32-S3 DevKit).
 *
 * ============================================================================
 * GUIA RÁPIDO DE PINOS DO ESP32-S3 (DevKitC-1):
 * ============================================================================
 * - Pinos SEGUROS e livres para uso geral:
 *     GPIO 1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 21,
 *     38, 39, 40, 41, 42, 47, 48
 *
 * - Pinos com ADC1 (Recomendados para leituras analógicas mesmo com Wi-Fi ativo):
 *     GPIO 1 a GPIO 10 (ADC1_CH0 a ADC1_CH9)
 *
 * - Pinos com ADC2 (Evitar usar como ADC se o Wi-Fi estiver ligado):
 *     GPIO 11 a GPIO 20 (ADC2_CH0 a ADC2_CH9)
 *
 * - Pinos de Strapping (Cuidado no boot - evitar pull-up/pull-down externo forçado):
 *     GPIO 0 (Boot), GPIO 3, GPIO 45 (VDD_SPI), GPIO 46
 *
 * - Pinos Reservados / Sistema:
 *     GPIO 19 e 20 -> USB Nativo (D- / D+ e USB Serial/JTAG)
 *     GPIO 43 e 44 -> UART0 Padrão (TX0 / RX0 - Console/Gravação)
 *     GPIO 26 a 32 -> Flash SPI interna (NÃO USAR)
 *     GPIO 33 a 37 -> Reservados em módulos com Octal PSRAM (ex: N16R8 / N8R8)
 * ============================================================================
 */

#include "driver/gpio.h"

// ============================================================================
// 1. INTERFACE DE USUÁRIO E DIAGNÓSTICO (LEDs e Botões)
// ============================================================================
#define PIN_STATUS_LED              GPIO_NUM_48   // No DevKitC-1, o GPIO 48 (ou 38) costuma ser o LED onboard
#define TAG_STATUS_LED              "SYS_STATUS_LED"

#define PIN_USER_BUTTON             GPIO_NUM_0    // Botão BOOT da placa
#define TAG_USER_BUTTON             "BTN_BOOT_USER"

// ============================================================================
// 2. CONTROLE DE ENERGIA DOS SENSORES (Power Gating para Economia de Bateria)
// ============================================================================
#define PIN_SENSOR_PWR_EN           GPIO_NUM_21   // Liga/desliga alimentação do barramento de sensores
#define TAG_SENSOR_PWR_EN           "PWR_EN_SENSORS"

// ============================================================================
// 3. BARRAMENTO I2C (Sensores Digitais, RTC DS3231, BME280, ADS1115, Display)
// ============================================================================
#define PIN_I2C_SDA                 GPIO_NUM_8
#define TAG_I2C_SDA                 "I2C0_SDA"

#define PIN_I2C_SCL                 GPIO_NUM_9
#define TAG_I2C_SCL                 "I2C0_SCL"

#define I2C_MASTER_FREQ_HZ          400000        // Clock I2C: 400 kHz (Fast Mode)

// ============================================================================
// 4. BARRAMENTO SPI - CARTÃO MICROSD / MEMÓRIA EXTERNA
// ============================================================================
#define PIN_SD_SPI_CS               GPIO_NUM_10
#define TAG_SD_SPI_CS               "SD_SPI_CS"

#define PIN_SD_SPI_MOSI             GPIO_NUM_11
#define TAG_SD_SPI_MOSI             "SD_SPI_MOSI"

#define PIN_SD_SPI_SCK              GPIO_NUM_12
#define TAG_SD_SPI_SCK              "SD_SPI_SCK"

#define PIN_SD_SPI_MISO             GPIO_NUM_13
#define TAG_SD_SPI_MISO             "SD_SPI_MISO"

#define PIN_SD_CARD_DETECT          GPIO_NUM_14   // Opcional: chave de presença do cartão SD (ou GPIO_NUM_NC)
#define TAG_SD_CARD_DETECT          "SD_CARD_DETECT"

// ============================================================================
// 5. ENTRADAS ANALÓGICAS - ADC1 (Sensores Analógicos e Tensão da Bateria)
// ============================================================================
#define PIN_ADC_BATTERY_VOLTAGE     GPIO_NUM_1    // ADC1_CH0 - Divisor resistivo da bateria
#define TAG_ADC_BATTERY_VOLTAGE     "VBAT_MV"

#define PIN_ADC_ANALOG_CH1          GPIO_NUM_2    // ADC1_CH1 - Canal analógico 1 (ex: Pressão / 4-20mA)
#define TAG_ADC_ANALOG_CH1          "AI_CH1"

#define PIN_ADC_ANALOG_CH2          GPIO_NUM_4    // ADC1_CH3 - Canal analógico 2 (ex: Temperatura NTC / Solo)
#define TAG_ADC_ANALOG_CH2          "AI_CH2"

#define PIN_ADC_ANALOG_CH3          GPIO_NUM_5    // ADC1_CH4 - Canal analógico 3 (Livre)
#define TAG_ADC_ANALOG_CH3          "AI_CH3"

// ============================================================================
// 6. ENTRADAS DIGITAIS / CONTADORES DE PULSO (PCNT / Interrupção Externa)
// ============================================================================
#define PIN_DI_PULSE_CH1            GPIO_NUM_6    // Ex: Pluviômetro (báscula) ou vazão
#define TAG_DI_PULSE_CH1            "DI_PULSE_1"

#define PIN_DI_PULSE_CH2            GPIO_NUM_7    // Ex: Anemômetro / Tacômetro
#define TAG_DI_PULSE_CH2            "DI_PULSE_2"

#define PIN_DI_ALARM_INPUT          GPIO_NUM_15   // Entrada digital de status/alarme externo
#define TAG_DI_ALARM_INPUT          "DI_ALARM_IN"

// ============================================================================
// 7. SAÍDAS DIGITAIS / ATUADORES / RELÉS
// ============================================================================
#define PIN_DO_RELAY_1              GPIO_NUM_16
#define TAG_DO_RELAY_1              "DO_RELAY_1"

#define PIN_DO_AUX_OUTPUT           GPIO_NUM_47
#define TAG_DO_AUX_OUTPUT           "DO_AUX_OUT"

// ============================================================================
// 8. COMUNICAÇÃO SERIAL SECUNDÁRIA - UART1 (Ex: RS-485 Modbus / GPS / Modem)
// ============================================================================
#define PIN_UART1_TX                GPIO_NUM_17
#define TAG_UART1_TX                "UART1_TX"

#define PIN_UART1_RX                GPIO_NUM_18
#define TAG_UART1_RX                "UART1_RX"

#define PIN_RS485_DE_RE             GPIO_NUM_38   // Controle de direção TX/RX do transceptor RS-485
#define TAG_RS485_DE_RE             "RS485_DE_RE"

// ============================================================================
// 9. ESTRUTURA E TABELA DE MAPEAMENTO (Pino <-> Tag) PARA USO NO CÓDIGO
// ============================================================================
#ifdef __cplusplus
namespace datalogger::board {

enum class PinDirection {
    kInput,
    kOutput,
    kAnalogInput,
    kBusIo
};

struct PinDescriptor {
    gpio_num_t pin;
    const char* tag;
    PinDirection direction;
    const char* description;
};

/**
 * @brief Tabela descritiva de todos os IOs do Datalogger.
 * Útil para inicializar GPIOs em loop, gerar cabeçalhos dinâmicos no CSV ou logs de diagnóstico.
 */
inline constexpr PinDescriptor kBoardPinMap[] = {
    {PIN_STATUS_LED,          TAG_STATUS_LED,          PinDirection::kOutput,      "LED de status do sistema"},
    {PIN_USER_BUTTON,         TAG_USER_BUTTON,         PinDirection::kInput,       "Botao de usuario / Boot"},
    {PIN_SENSOR_PWR_EN,       TAG_SENSOR_PWR_EN,       PinDirection::kOutput,      "Habilita alimentacao dos sensores"},
    {PIN_I2C_SDA,             TAG_I2C_SDA,             PinDirection::kBusIo,       "Barramento I2C Dados (SDA)"},
    {PIN_I2C_SCL,             TAG_I2C_SCL,             PinDirection::kBusIo,       "Barramento I2C Clock (SCL)"},
    {PIN_SD_SPI_CS,           TAG_SD_SPI_CS,           PinDirection::kOutput,      "Cartao SD SPI Chip Select"},
    {PIN_SD_SPI_MOSI,         TAG_SD_SPI_MOSI,         PinDirection::kBusIo,       "Cartao SD SPI MOSI"},
    {PIN_SD_SPI_SCK,          TAG_SD_SPI_SCK,          PinDirection::kBusIo,       "Cartao SD SPI Clock"},
    {PIN_SD_SPI_MISO,         TAG_SD_SPI_MISO,         PinDirection::kBusIo,       "Cartao SD SPI MISO"},
    {PIN_SD_CARD_DETECT,      TAG_SD_CARD_DETECT,      PinDirection::kInput,       "Deteccao de presenca do Cartao SD"},
    {PIN_ADC_BATTERY_VOLTAGE, TAG_ADC_BATTERY_VOLTAGE, PinDirection::kAnalogInput, "Leitura de tensao da bateria"},
    {PIN_ADC_ANALOG_CH1,      TAG_ADC_ANALOG_CH1,      PinDirection::kAnalogInput, "Entrada analogica Canal 1"},
    {PIN_ADC_ANALOG_CH2,      TAG_ADC_ANALOG_CH2,      PinDirection::kAnalogInput, "Entrada analogica Canal 2"},
    {PIN_ADC_ANALOG_CH3,      TAG_ADC_ANALOG_CH3,      PinDirection::kAnalogInput, "Entrada analogica Canal 3"},
    {PIN_DI_PULSE_CH1,        TAG_DI_PULSE_CH1,        PinDirection::kInput,       "Entrada de pulso / contador 1"},
    {PIN_DI_PULSE_CH2,        TAG_DI_PULSE_CH2,        PinDirection::kInput,       "Entrada de pulso / contador 2"},
    {PIN_DI_ALARM_INPUT,      TAG_DI_ALARM_INPUT,      PinDirection::kInput,       "Entrada digital de alarme"},
    {PIN_DO_RELAY_1,          TAG_DO_RELAY_1,          PinDirection::kOutput,      "Saida digital Rele 1"},
    {PIN_DO_AUX_OUTPUT,       TAG_DO_AUX_OUTPUT,       PinDirection::kOutput,      "Saida digital auxiliar"},
    {PIN_UART1_TX,            TAG_UART1_TX,            PinDirection::kBusIo,       "UART1 TX (RS-485 / Aux)"},
    {PIN_UART1_RX,            TAG_UART1_RX,            PinDirection::kBusIo,       "UART1 RX (RS-485 / Aux)"},
    {PIN_RS485_DE_RE,         TAG_RS485_DE_RE,         PinDirection::kOutput,      "Controle de direcao RS-485"}
};

} // namespace datalogger::board
#endif

