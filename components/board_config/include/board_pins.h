#pragma once

/**
 * @file board_pins.h
 * @brief Mapeamento oficial de Portas/GPIOs e Tags do Datalogger (ESP32-S3-DEVKITC-1-N8R2)
 *        gerado a partir do esquemático elétrico (Main.SchDoc / CPU.SchDoc).
 *
 * ============================================================================
 * RESUMO DO HARDWARE NO ESQUEMÁTICO:
 * ============================================================================
 * - MCU: U3 - ESP32-S3-DEVKITC-1-N8R2
 * - Condicionamento Analógico (MCP6022-E/SN - U5, U6, U7, U8, U9):
 *     * Motor (U8, U9): OilPressure, OilTemp, FuelPressure, Lambda -> GPIO 1 a 4 (ADC1_CH0 a CH3)
 *     * 5V para 3.3V (U5, U6): ADC5to3v3_1 a ADC5to3v3_4           -> GPIO 5 a 8 (ADC1_CH4 a CH7)
 *     * 12V para 3.3V (U7):    ADC12to3v3_1 e ADC12to3v3_2         -> GPIO 9 e 10 (ADC1_CH8 e CH9)
 * - RTC Externo: B8 - DS3231SN# via I2C (SDA = GPIO 11, SCL = GPIO 12)
 * - Transceptor CAN: U2 - SN65HVD230DR + Proteção TVS SM712 (CAN_TX = GPIO 15, CAN_RX = GPIO 16)
 * - GPS: J5 - Módulo GPS Neo-8M via UART (GPS_RX_ESP = GPIO 13, GPS_TX_ESP = GPIO 47)
 * - Cartão MicroSD: J4 - Suporte a SPI e SDMMC 4-bits (GPIO 14, 17, 18, 21, 40, 41)
 * - Interface (UI):
 *     * Botões com debounce RC (100R + 100nF) e pull-up 4.7k: Free_SW1 (GPIO 35), Free_SW2 (GPIO 36)
 *     * LEDs Verdes (1k para GND): LED_Acquiring (GPIO 37 / D2), LED_Working (GPIO 39 / D3)
 *
 * NOTA IMPORTANTE SOBRE GPIO 35, 36 e 37:
 * No módulo N8R2 (Quad SPI Flash / Quad PSRAM), evite habilitar Octal PSRAM no menuconfig,
 * e caso não utilize a PSRAM interna, mantenha CONFIG_SPIRAM=n para liberar totalmente os
 * pinos GPIO 35, 36 e 37 para os botões Free_SW1/SW2 e LED_Acquiring.
 * ============================================================================
 */

#include "driver/gpio.h"

// ============================================================================
// 1. ENTRADAS ANALÓGICAS DO MOTOR (Measurement_Engine.SchDoc - U8 e U9 MCP6022)
//    Mapeadas no ADC1 (Canais 0 a 3)
// ============================================================================
#define PIN_ADC_OIL_PRESSURE        GPIO_NUM_1    // J3_4  | ADC1_CH0 | Saída OP1_OUTA (OilPressure_IN)
#define TAG_ADC_OIL_PRESSURE        "OilPressure"

#define PIN_ADC_OIL_TEMP            GPIO_NUM_2    // J3_5  | ADC1_CH1 | Saída OP1_OUTB (OilTemp_IN+ / OilTemp_IN-)
#define TAG_ADC_OIL_TEMP            "OilTemp"

#define PIN_ADC_FUEL_PRESSURE       GPIO_NUM_3    // J1_13 | ADC1_CH2 | Saída OP2_OUTA (FuelPressure_IN)
#define TAG_ADC_FUEL_PRESSURE       "FuelPressure"

#define PIN_ADC_LAMBDA              GPIO_NUM_4    // J1_4  | ADC1_CH3 | Saída OP2_OUTB (Lambda_IN)
#define TAG_ADC_LAMBDA              "Lambda"

// ============================================================================
// 2. ENTRADAS ANALÓGICAS 5V -> 3.3V (Measurement_5to3v3.SchDoc - U5 e U6 MCP6022)
//    Mapeadas no ADC1 (Canais 4 a 7)
// ============================================================================
#define PIN_ADC_5TO3V3_1            GPIO_NUM_5    // J1_5  | ADC1_CH4 | Saída OP3_OUTA (5to3v3_1_IN)
#define TAG_ADC_5TO3V3_1            "ADC5to3v3_1"

#define PIN_ADC_5TO3V3_2            GPIO_NUM_6    // J1_6  | ADC1_CH5 | Saída OP3_OUTB (5to3v3_2_IN)
#define TAG_ADC_5TO3V3_2            "ADC5to3v3_2"

#define PIN_ADC_5TO3V3_3            GPIO_NUM_7    // J1_7  | ADC1_CH6 | Saída OP4_OUTA (5to3v3_3_IN)
#define TAG_ADC_5TO3V3_3            "ADC5to3v3_3"

#define PIN_ADC_5TO3V3_4            GPIO_NUM_8    // J1_12 | ADC1_CH7 | Saída OP4_OUTB (5to3v3_4_IN)
#define TAG_ADC_5TO3V3_4            "ADC5to3v3_4"

// ============================================================================
// 3. ENTRADAS ANALÓGICAS 12V -> 3.3V (Measurement_12to3v3.SchDoc - U7 MCP6022)
//    Mapeadas no ADC1 (Canais 8 e 9)
// ============================================================================
#define PIN_ADC_12TO3V3_1           GPIO_NUM_9    // J1_15 | ADC1_CH8 | Saída OP5_OUTA (12to3v3_1_IN)
#define TAG_ADC_12TO3V3_1           "ADC12to3v3_1"

#define PIN_ADC_12TO3V3_2           GPIO_NUM_10   // J1_16 | ADC1_CH9 | Saída OP5_OUTB (12to3v3_2_IN)
#define TAG_ADC_12TO3V3_2           "ADC12to3v3_2"

// ============================================================================
// 4. BARRAMENTO I2C - RTC DS3231SN# (RTC.SchDoc - B8)
// ============================================================================
#define PIN_I2C_SDA                 GPIO_NUM_11   // J1_17 | SDA_I2C_1 (Pull-up 4.7k R91 para 3V3)
#define TAG_I2C_SDA                 "SDA_I2C_1"

#define PIN_I2C_SCL                 GPIO_NUM_12   // J1_18 | SCL_I2C_1 (Pull-up 4.7k R90 para 3V3)
#define TAG_I2C_SCL                 "SCL_I2C_1"

#define I2C_MASTER_FREQ_HZ          400000        // Clock I2C: 400 kHz
#define RTC_DS3231_I2C_ADDR         0x68          // Endereço I2C padrão do DS3231SN#

// ============================================================================
// 5. GPS NEO-8M - UART (CPU.SchDoc - Conector J5)
// ============================================================================
#define PIN_GPS_RX                  GPIO_NUM_13   // J1_19 | GPS_RX(LadoESP) <- Recebe do pino Tx do GPS Neo-8M
#define TAG_GPS_RX                  "GPS_RX"

#define PIN_GPS_TX                  GPIO_NUM_47   // J3_17 | GPS_TX(LadoESP) -> Envia para o pino Rx do GPS Neo-8M
#define TAG_GPS_TX                  "GPS_TX"

// ============================================================================
// 6. COMUNICAÇÃO CAN / TWAI - SN65HVD230DR (Communication.SchDoc - U2)
// ============================================================================
#define PIN_CAN_TX                  GPIO_NUM_15   // J1_8  | CAN_TX -> Pino 1 (D) do SN65HVD230DR via R1 (33R)
#define TAG_CAN_TX                  "CAN_TX"

#define PIN_CAN_RX                  GPIO_NUM_16   // J1_9  | CAN_RX <- Pino 4 (R) do SN65HVD230DR via R2 (33R)
#define TAG_CAN_RX                  "CAN_RX"

// ============================================================================
// 7. CARTÃO MICROSD (CPU.SchDoc - J4 SocketMicroSD - Modo SPI ou SDMMC 4-bit)
// ============================================================================
#define PIN_SD_MISO                 GPIO_NUM_14   // J1_20 | SD_MISO / DAT0 (P7) - Pull-up 4.7k (R8)
#define PIN_SD_DAT0                 PIN_SD_MISO
#define TAG_SD_MISO                 "SD_MISO"

#define PIN_SD_CS                   GPIO_NUM_17   // J1_10 | SD_CS / CD/DAT3 (P2) - Pull-up 10k (R5)
#define PIN_SD_DAT3                 PIN_SD_CS
#define TAG_SD_CS                   "SD_CS"

#define PIN_SD_MOSI                 GPIO_NUM_18   // J1_11 | SD_MOSI / CMD (P3) - Pull-up 10k (R6)
#define PIN_SD_CMD                  PIN_SD_MOSI
#define TAG_SD_MOSI                 "SD_MOSI"

#define PIN_SD_SCK                  GPIO_NUM_21   // J3_18 | SD_SCK(CLK) (P5) via resistor série 33R (R9)
#define PIN_SD_CLK                  PIN_SD_SCK
#define TAG_SD_SCK                  "SD_SCK"

#define PIN_SD_DAT1                 GPIO_NUM_40   // J3_8  | MTDO/GPIO40 | SD_DAT1 (P8) via 33R (R10) + Pull-up 4.7k (R7)
#define TAG_SD_DAT1                 "SD_DAT1"

#define PIN_SD_DAT2                 GPIO_NUM_41   // J3_7  | MTDI/GPIO41 | SD_DAT2 (P1) via 33R (R11) + Pull-up 4.7k (R4)
#define TAG_SD_DAT2                 "SD_DAT2"

// ============================================================================
// 8. INTERFACE DE USUÁRIO: BOTÕES E LEDS DE STATUS (CPU.SchDoc)
// ============================================================================
#define PIN_BTN_FREE_SW1            GPIO_NUM_35   // J3_13 | Rede Free_SW1 (Botão SW2, ativo em nível BAIXO / GND)
#define TAG_BTN_FREE_SW1            "Free_SW1"

#define PIN_BTN_FREE_SW2            GPIO_NUM_36   // J3_12 | Rede Free_SW2 (Botão SW1, ativo em nível BAIXO / GND)
#define TAG_BTN_FREE_SW2            "Free_SW2"

#define PIN_LED_ACQUIRING           GPIO_NUM_37   // J3_11 | LED Verde D2 (R16 1k para GND, ativo em nível ALTO)
#define TAG_LED_ACQUIRING           "LED_Acquiring"

#define PIN_LED_WORKING             GPIO_NUM_39   // J3_9  | MTCK/GPIO39 | LED Verde D3 (R17 1k para GND, ativo em nível ALTO)
#define TAG_LED_WORKING             "LED_Working"

// Alias de compatibilidade para o LED principal de status
#define PIN_STATUS_LED              PIN_LED_WORKING
#define TAG_STATUS_LED              TAG_LED_WORKING

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
 * @brief Tabela apenas com os 10 canais analógicos (ADC1) na ordem do esquemático.
 * Ideal para iterar nas leituras do ADC e gerar o cabeçalho do CSV automaticamente.
 */
inline constexpr PinDescriptor kAnalogChannels[] = {
    {PIN_ADC_OIL_PRESSURE,  TAG_ADC_OIL_PRESSURE,  PinDirection::kAnalogInput, "Pressao de Oleo do Motor (ADC1_CH0)"},
    {PIN_ADC_OIL_TEMP,      TAG_ADC_OIL_TEMP,      PinDirection::kAnalogInput, "Temperatura de Oleo do Motor (ADC1_CH1)"},
    {PIN_ADC_FUEL_PRESSURE, TAG_ADC_FUEL_PRESSURE, PinDirection::kAnalogInput, "Pressao de Combustivel (ADC1_CH2)"},
    {PIN_ADC_LAMBDA,        TAG_ADC_LAMBDA,        PinDirection::kAnalogInput, "Sonda Lambda (ADC1_CH3)"},
    {PIN_ADC_5TO3V3_1,      TAG_ADC_5TO3V3_1,      PinDirection::kAnalogInput, "Entrada Analogica 5V->3.3V Canal 1 (ADC1_CH4)"},
    {PIN_ADC_5TO3V3_2,      TAG_ADC_5TO3V3_2,      PinDirection::kAnalogInput, "Entrada Analogica 5V->3.3V Canal 2 (ADC1_CH5)"},
    {PIN_ADC_5TO3V3_3,      TAG_ADC_5TO3V3_3,      PinDirection::kAnalogInput, "Entrada Analogica 5V->3.3V Canal 3 (ADC1_CH6)"},
    {PIN_ADC_5TO3V3_4,      TAG_ADC_5TO3V3_4,      PinDirection::kAnalogInput, "Entrada Analogica 5V->3.3V Canal 4 (ADC1_CH7)"},
    {PIN_ADC_12TO3V3_1,     TAG_ADC_12TO3V3_1,     PinDirection::kAnalogInput, "Entrada Analogica 12V->3.3V Canal 1 (ADC1_CH8)"},
    {PIN_ADC_12TO3V3_2,     TAG_ADC_12TO3V3_2,     PinDirection::kAnalogInput, "Entrada Analogica 12V->3.3V Canal 2 (ADC1_CH9)"}
};

/**
 * @brief Tabela completa de todos os IOs utilizados no esquemático do Datalogger.
 */
inline constexpr PinDescriptor kBoardPinMap[] = {
    // Entradas Analógicas do Motor
    {PIN_ADC_OIL_PRESSURE,  TAG_ADC_OIL_PRESSURE,  PinDirection::kAnalogInput, "Pressao de Oleo (OP1_OUTA)"},
    {PIN_ADC_OIL_TEMP,      TAG_ADC_OIL_TEMP,      PinDirection::kAnalogInput, "Temperatura de Oleo (OP1_OUTB)"},
    {PIN_ADC_FUEL_PRESSURE, TAG_ADC_FUEL_PRESSURE, PinDirection::kAnalogInput, "Pressao de Combustivel (OP2_OUTA)"},
    {PIN_ADC_LAMBDA,        TAG_ADC_LAMBDA,        PinDirection::kAnalogInput, "Sonda Lambda (OP2_OUTB)"},

    // Entradas Analógicas 5V -> 3.3V
    {PIN_ADC_5TO3V3_1,      TAG_ADC_5TO3V3_1,      PinDirection::kAnalogInput, "Sensor 5V Canal 1 (OP3_OUTA)"},
    {PIN_ADC_5TO3V3_2,      TAG_ADC_5TO3V3_2,      PinDirection::kAnalogInput, "Sensor 5V Canal 2 (OP3_OUTB)"},
    {PIN_ADC_5TO3V3_3,      TAG_ADC_5TO3V3_3,      PinDirection::kAnalogInput, "Sensor 5V Canal 3 (OP4_OUTA)"},
    {PIN_ADC_5TO3V3_4,      TAG_ADC_5TO3V3_4,      PinDirection::kAnalogInput, "Sensor 5V Canal 4 (OP4_OUTB)"},

    // Entradas Analógicas 12V -> 3.3V
    {PIN_ADC_12TO3V3_1,     TAG_ADC_12TO3V3_1,     PinDirection::kAnalogInput, "Sensor 12V Canal 1 (OP5_OUTA)"},
    {PIN_ADC_12TO3V3_2,     TAG_ADC_12TO3V3_2,     PinDirection::kAnalogInput, "Sensor 12V Canal 2 (OP5_OUTB)"},

    // I2C (RTC DS3231SN#)
    {PIN_I2C_SDA,           TAG_I2C_SDA,           PinDirection::kBusIo,       "I2C1 SDA - RTC DS3231"},
    {PIN_I2C_SCL,           TAG_I2C_SCL,           PinDirection::kBusIo,       "I2C1 SCL - RTC DS3231"},

    // GPS Neo-8M
    {PIN_GPS_RX,            TAG_GPS_RX,            PinDirection::kBusIo,       "UART GPS RX (conectado ao TX do Neo-8M)"},
    {PIN_GPS_TX,            TAG_GPS_TX,            PinDirection::kBusIo,       "UART GPS TX (conectado ao RX do Neo-8M)"},

    // Barramento CAN (SN65HVD230DR)
    {PIN_CAN_TX,            TAG_CAN_TX,            PinDirection::kBusIo,       "TWAI/CAN TX (SN65HVD230 D)"},
    {PIN_CAN_RX,            TAG_CAN_RX,            PinDirection::kBusIo,       "TWAI/CAN RX (SN65HVD230 R)"},

    // Cartão MicroSD (SPI / SDMMC)
    {PIN_SD_MISO,           TAG_SD_MISO,           PinDirection::kBusIo,       "MicroSD MISO / DAT0"},
    {PIN_SD_CS,             TAG_SD_CS,             PinDirection::kOutput,      "MicroSD CS / DAT3"},
    {PIN_SD_MOSI,           TAG_SD_MOSI,           PinDirection::kBusIo,       "MicroSD MOSI / CMD"},
    {PIN_SD_SCK,            TAG_SD_SCK,            PinDirection::kBusIo,       "MicroSD SCK / CLK"},
    {PIN_SD_DAT1,           TAG_SD_DAT1,           PinDirection::kBusIo,       "MicroSD DAT1 (Modo SDMMC 4-bit)"},
    {PIN_SD_DAT2,           TAG_SD_DAT2,           PinDirection::kBusIo,       "MicroSD DAT2 (Modo SDMMC 4-bit)"},

    // Botões e LEDs
    {PIN_BTN_FREE_SW1,      TAG_BTN_FREE_SW1,      PinDirection::kInput,       "Botao Free_SW1 (SW2 - Ativo Baixo)"},
    {PIN_BTN_FREE_SW2,      TAG_BTN_FREE_SW2,      PinDirection::kInput,       "Botao Free_SW2 (SW1 - Ativo Baixo)"},
    {PIN_LED_ACQUIRING,     TAG_LED_ACQUIRING,     PinDirection::kOutput,      "LED Verde Acquiring (D2)"},
    {PIN_LED_WORKING,       TAG_LED_WORKING,       PinDirection::kOutput,      "LED Verde Working (D3)"}
};

} // namespace datalogger::board
#endif
