# Datalogger ESP32-S3 — Protto UFSC (Arquitetura Motorsport)

Firmware embarcado para **Datalogger Automotivo / Motorsport** baseado no **ESP32-S3 (`ESP32-S3-DEVKITC-1-N8R2`)**, desenvolvido em **C++17** sobre o **ESP-IDF nativo** e **FreeRTOS (Dual-Core)**, seguindo a arquitetura em camadas da indústria de **Motorsport / AUTOSAR** (`BSP`, `OSAL`, `HAL`, `LLD`, `BSW`, `ASW`) e validação **SIL (*Software-in-the-Loop*)** com **Google Test & Google Mock**.

- **Repositório Oficial:** `git@github.com:sid-martins/datalogger-prottoufsc.git`
- **Guia de Protocolo CAN para os Nós `ESP32-C3` e ECU FuelTech:** [`docs/CAN_PROTOCOL.md`](docs/CAN_PROTOCOL.md)
- **Guia de Protocolo GPS (`Neo-8M` 10 Hz — UBX + NMEA):** [`docs/NEO8M_PROTOCOL.md`](docs/NEO8M_PROTOCOL.md)
- **Especificação do Formato Binário de Log (`.bin` / `PDLG v1`):** [`docs/BINARY_LOG_FORMAT.md`](docs/BINARY_LOG_FORMAT.md)
- **Plano e Suíte de Testes em Hardware (`TC-00..TC-09`):** [`docs/HARDWARE_TEST_PLAN.md`](docs/HARDWARE_TEST_PLAN.md)
- **Especificações e Requisitos FSAE:** [`docs/REQUIREMENTS.md`](docs/REQUIREMENTS.md)
- **Roadmap de Implementação:** [`docs/ROADMAP.md`](docs/ROADMAP.md)

---

## Estado Atual do Projeto

- **Fases 0 a 8 Concluídas** (ver [`docs/ROADMAP.md`](docs/ROADMAP.md)): BSP (`26` GPIOs), Transporte CAN (`500 kbps`) + Decodificador (`ESP32-C3` + FuelTech), ADC1 (`10` canais com filtro EMA e multi-taxa), **RTC `DS3231SN#` (`I2C 0x68`) com sincronização monotônica de `1 ms` ($\le 50\ \text{ms}$)**, **GPS `Neo-8M` a `10 Hz` (`UART1 115200 baud`, UBX + NMEA zero-heap)**, **Orquestração Dual-Core + Logger Binário `.bin` (`PDLG v1`, `256 B/registro`, `CRC-32`, bloco `4 KB`) no MicroSD `J4`** e **Interface de Pista (`UiService` + `GpioLld`: auto-start `RPM > 600` com retenção de `5 s`, `Free_SW1` em standby, *Lap Beacon* em `Free_SW2` e diagnóstico nos LEDs `LED_Acquiring`/`LED_Working`)**.
- **Próximo Passo (Fase 9):** Validação integrada em bancada e pista (ESP32-S3 + Nós `ESP32-C3` + ECU FuelTech) e decodificação dos logs `.bin` (`PDLG v1`) no **MoTeC i2 / MegaLogViewer**.
- **Suíte SIL (Host) & Build Real:** **`106` testes automatizados passando** (`Google Test + Google Mock v1.14.0`, `0 warnings`) + firmware compilado no **ESP-IDF `v5.4.4`** (`idf.py build`).

---

## Mapa Rápido de IDs CAN (`500 kbps` — `DLC = 8 bytes`)

Para o guia completo de bytes, escalas e código C++ de exemplo para gravar nos nós **`ESP32-C3`**, consulte [`docs/CAN_PROTOCOL.md`](docs/CAN_PROTOCOL.md):

| Nó Físico | MCU / Origem | CAN ID (Hex) | Formato / Endianness | Variáveis no Frame (`4 x 16-bit = 8 bytes`) |
| :--- | :---: | :---: | :---: | :--- |
| **Freio & Direção** | `ESP32-C3` | **`0x100`** | 11-bit / Little-Endian | Ângulo Direção, Pressão Freio Diant., Pressão Freio Tras., Pos. Pedal Freio |
| **Roda Dianteira Esq. (`FL`)** | `ESP32-C3` | **`0x200`** | 11-bit / Little-Endian | Pot. Linear Suspensão, Velocidade Roda, Célula de Carga, Temp. Disco Freio |
| **Roda Dianteira Dir. (`FR`)** | `ESP32-C3` | **`0x201`** | 11-bit / Little-Endian | Pot. Linear Suspensão, Velocidade Roda, Célula de Carga, Temp. Disco Freio |
| **Roda Traseira Esq. (`RL`)** | `ESP32-C3` | **`0x202`** | 11-bit / Little-Endian | Pot. Linear Suspensão, Velocidade Roda, Célula de Carga, Temp. Disco Freio |
| **Roda Traseira Dir. (`RR`)** | `ESP32-C3` | **`0x203`** | 11-bit / Little-Endian | Pot. Linear Suspensão, Velocidade Roda, Célula de Carga, Temp. Disco Freio |
| **IMU WIT — Aceleração** | `ESP32-C3` | **`0x300`** | 11-bit / Little-Endian | Accel X, Accel Y, Accel Z ($0,01\ \text{m/s}^2$), Status Calibração |
| **IMU WIT — Giroscópio** | `ESP32-C3` | **`0x301`** | 11-bit / Little-Endian | Gyro X, Gyro Y, Gyro Z ($0,1^\circ/\text{s}$), Reservado |
| **ECU FuelTech (Bloco 1)** | FuelTech | **`0x14080600`** *(ou `0x600`)* | 29-bit / Big-Endian | TPS ($0,1\%$), MAP ($0,001\ \text{bar}$), Temp. Ar ($0,1^\circ\text{C}$), Temp. Motor ($0,1^\circ\text{C}$) |
| **ECU FuelTech (Bloco 2)** | FuelTech | **`0x14080601`** *(ou `0x601`)* | 29-bit / Big-Endian | Pressão Óleo, Pressão Comb., Pressão Água, **Marcha Atual (`Gear`)** |
| **ECU FuelTech (Bloco 3)** | FuelTech | **`0x14080602`** *(ou `0x602`)* | 29-bit / Big-Endian | Sonda Lambda ($0,001\ \lambda$), **RPM** ($1\ \text{rpm}$), Temp. Óleo ($0,1^\circ\text{C}$), Pit Limit |

---

## Arquitetura em Camadas (Padrão Motorsport)

| Camada | Diretório / Namespace | Papel na Engenharia |
| :--- | :--- | :--- |
| **BSP** (*Board Support Package*) | [`components/bsp/`](components/bsp) (`datalogger::bsp`) | *Pinning Table* da placa física ([`bsp_pins.h`](components/bsp/include/bsp_pins.h)): mapeia os 26 sinais do esquemático (`PIN_*` e `TAG_*`). |
| **OSAL** (*OS Abstraction Layer*) | [`components/osal/`](components/osal) (`datalogger::osal`) | Abstrai o sistema operacional de tempo real ([`i_osal_queue.hpp`](components/osal/include/i_osal_queue.hpp) e [`freertos_queue_adapter.hpp`](components/osal/include/freertos_queue_adapter.hpp)). |
| **HAL** (*Hardware Abstraction Layer*) | [`components/datalogger_hal/`](components/datalogger_hal) (`datalogger::hal`) | Contratos puramente virtuais e tipos de fronteira ([`i_can_hal.hpp`](components/datalogger_hal/include/i_can_hal.hpp), [`i_adc_hal.hpp`](components/datalogger_hal/include/i_adc_hal.hpp), [`i_rtc_hal.hpp`](components/datalogger_hal/include/i_rtc_hal.hpp), [`i_gps_hal.hpp`](components/datalogger_hal/include/i_gps_hal.hpp), [`i_storage_hal.hpp`](components/datalogger_hal/include/i_storage_hal.hpp), [`i_gpio_hal.hpp`](components/datalogger_hal/include/i_gpio_hal.hpp)). |
| **LLD** (*Low-Level Drivers / MCAL*) | [`components/lld/`](components/lld) (`datalogger::lld`) | Drivers específicos do silício ESP32-S3 e CIs da placa ([`can_twai_lld.cpp`](components/lld/src/can_twai_lld.cpp) para `SN65HVD230DR`; [`adc_oneshot_lld.cpp`](components/lld/src/adc_oneshot_lld.cpp) para `ADC1`; [`ds3231_i2c_lld.cpp`](components/lld/src/ds3231_i2c_lld.cpp) para `DS3231SN#`; [`neo8m_uart_lld.cpp`](components/lld/src/neo8m_uart_lld.cpp) para `Neo-8M`; [`sd_card_lld.cpp`](components/lld/src/sd_card_lld.cpp) para MicroSD `J4`; [`gpio_lld.cpp`](components/lld/src/gpio_lld.cpp) para botões/LEDs). |
| **BSW** (*Basic Software Services*) | [`components/bsw/`](components/bsw) (`datalogger::bsw`) | Serviços de comunicação, aquisição, tempo, GPS, agregação, gravação binária `PDLG v1` e interface/FSM por RPM sem heap ([`can_service.cpp`](components/bsw/src/can_service.cpp), [`can_decoder_service.cpp`](components/bsw/src/can_decoder_service.cpp), [`adc_service.cpp`](components/bsw/src/adc_service.cpp), [`rtc_service.cpp`](components/bsw/src/rtc_service.cpp), [`gps_service.cpp`](components/bsw/src/gps_service.cpp), [`telemetry_aggregator.cpp`](components/bsw/src/telemetry_aggregator.cpp), [`logger_service.cpp`](components/bsw/src/logger_service.cpp), [`ui_service.cpp`](components/bsw/src/ui_service.cpp), [`ring_buffer.hpp`](components/bsw/include/ring_buffer.hpp)). |
| **ASW** (*Application Software*) | [`main/`](main) ([`main.cpp`](main/main.cpp)) | Aplicação final e orquestração das Tasks Dual-Core no ESP32-S3 (Core 1: `can_rx_task` + `acquisition_task` 100 Hz; Core 0: `ui_task` 50 Hz + `storage_logger_task`). |
| **SIL** (*Software-in-the-Loop*) | [`tests/`](tests) (`datalogger::sil`) | Suíte com `106` testes automatizados e dublês de hardware ([`tests/sil_mocks/`](tests/sil_mocks)) executados no PC via Google Test. |

---

## Estrutura de Pastas do Projeto

```text
datalogger-prottoufsc/
├── CMakeLists.txt                  # CMake híbrido: Firmware ESP-IDF (via idf.py) ou Testes SIL no Host
├── GEMINI.md                       # Contexto e regras arquiteturais para o Agente IA (equivalente ao CLAUDE.md)
├── README.md                       # Documentação geral do projeto
├── sdkconfig.defaults              # Configurações padrão do ESP32-S3 (Flash 8MB, FreeRTOS 1000Hz)
├── partitions.csv                  # Tabela de partições customizada (NVS, Factory App, Storage FAT)
├── run_tests.ps1                   # Script PowerShell para compilar e rodar a suíte SIL no Windows
├── .clangd                         # Flags de compilação para o servidor Clangd
├── docs/                           # Especificações técnicas, protocolos e roadmap
│   ├── BINARY_LOG_FORMAT.md        # Especificação byte a byte do log binário .bin (PDLG v1)
│   ├── CAN_PROTOCOL.md             # Tabela oficial de IDs CAN (ESP32-C3 + FuelTech)
│   ├── HARDWARE_TEST_PLAN.md       # Suíte completa de testes de bancada e pista (TC-00..TC-09)
│   ├── NEO8M_PROTOCOL.md           # Referência oficial u-blox NEO-8M (UBX 10 Hz + NMEA)
│   ├── REQUIREMENTS.md             # Requisitos funcionais e não-funcionais FSAE
│   └── ROADMAP.md                  # Fases de implementação e progresso
├── main/                           # [ASW] Camada de Aplicação
│   ├── CMakeLists.txt
│   └── main.cpp                    # Entrypoint app_main() + Tasks Dual-Core (Core 1: CAN+Acq 100 Hz / Core 0: Logger SD)
├── components/
│   ├── bsp/                        # [BSP] Board Support Package
│   │   ├── CMakeLists.txt
│   │   └── include/
│   │       └── bsp_pins.h          # Pinning Table oficial dos 26 sinais do esquemático (CPU.SchDoc)
│   ├── osal/                       # [OSAL] Operating System Abstraction Layer
│   │   ├── CMakeLists.txt
│   │   └── include/
│   │       ├── i_osal_queue.hpp    # Interface IOsalQueue<T>
│   │       └── freertos_queue_adapter.hpp # Implementação real com xQueue do FreeRTOS
│   ├── datalogger_hal/             # [HAL] Hardware Abstraction Layer (prefixo evita colisão com hal do ESP-IDF)
│   │   ├── CMakeLists.txt
│   │   └── include/
│   │       ├── i_adc_hal.hpp       # Contrato IAdcHal e AdcChannelId (10 canais ADC1)
│   │       ├── i_can_hal.hpp       # Contrato ICanHal, CanFrame (11/29-bit), CanConfig (500 kbps)
│   │       ├── i_gpio_hal.hpp      # Contrato IGpioHal e UiGpioConfig (botões Free_SW1/2 + LEDs)
│   │       ├── i_gps_hal.hpp       # Contrato IGpsHal, GpsConfig e GpsData (Neo-8M 10 Hz)
│   │       ├── i_rtc_hal.hpp       # Contrato IRtcHal, RtcI2cConfig e RtcDateTime (DS3231SN#)
│   │       └── i_storage_hal.hpp   # Contrato IStorageHal e StorageConfig (SDMMC 4-bit / SPI)
│   ├── lld/                        # [LLD] Low-Level Drivers (ESP32-S3 Silício + Periféricos)
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   │   ├── adc_oneshot_lld.hpp # Driver LLD ADC1 (GPIO 1..10, 12-bit, DB_12)
│   │   │   ├── can_twai_lld.hpp    # Driver LLD TWAI para o transceptor SN65HVD230DR (GPIO 15/16)
│   │   │   ├── ds3231_i2c_lld.hpp  # Driver LLD I2C Master para o RTC DS3231SN# (GPIO 11/12, 0x68)
│   │   │   ├── gpio_lld.hpp        # Driver LLD GPIO para botões (GPIO 35/36) e LEDs (GPIO 37/39)
│   │   │   ├── neo8m_uart_lld.hpp  # Driver LLD UART1 + UBX 10 Hz para o GPS Neo-8M (GPIO 13/47)
│   │   │   └── sd_card_lld.hpp     # Driver LLD FATFS MicroSD J4 (SDMMC 4-bit / SPI + fsync)
│   │   └── src/
│   │       ├── adc_oneshot_lld.cpp
│   │       ├── can_twai_lld.cpp
│   │       ├── ds3231_i2c_lld.cpp
│   │       ├── gpio_lld.cpp
│   │       ├── neo8m_uart_lld.cpp
│   │       └── sd_card_lld.cpp
│   └── bsw/                        # [BSW] Basic Software Services & Static Containers
│       ├── CMakeLists.txt
│       ├── include/
│       │   ├── adc_service.hpp     # Serviço ADC (10 canais, filtro EMA, calibração linear, multi-taxa)
│       │   ├── binary_log_format.hpp # Constantes do protocolo binário PDLG v1 (256 B/registro + CRC-32)
│       │   ├── can_decoder_service.hpp # Decodificador CAN (nós ESP32-C3 + FuelTech) e timeout
│       │   ├── can_protocol.hpp    # IDs CAN oficiais e struct VehicleCanState
│       │   ├── can_service.hpp     # Serviço CAN (validação, fila OSAL RX, auto-recovery Bus-Off)
│       │   ├── gps_service.hpp     # Serviço GPS NMEA zero-heap ($xxGGA/$xxRMC, XOR, timestamp RTC)
│       │   ├── logger_service.hpp  # Serviço Logger binário PDLG v1, bloco 4 KB e fsync periódico
│       │   ├── ring_buffer.hpp     # RingBuffer<T, Capacity> estático (sem alocação dinâmica)
│       │   ├── rtc_service.hpp     # Serviço RTC + âncora monotônica 1 ms e fallback degradado
│       │   ├── telemetry_aggregator.hpp # Agregador Core 1 (RTC + CAN + ADC + GPS -> fila OSAL)
│       │   ├── telemetry_snapshot.hpp   # Snapshot unificado POD sem heap
│       │   └── ui_service.hpp      # Serviço UI e FSM por RPM (> 600 RPM, hold 5s, beacon, LEDs)
│       └── src/
│           ├── adc_service.cpp
│           ├── can_decoder_service.cpp
│           ├── can_service.cpp
│           ├── gps_service.cpp
│           ├── logger_service.cpp
│           ├── ring_buffer.cpp
│           ├── rtc_service.cpp
│           ├── telemetry_aggregator.cpp
│           └── ui_service.cpp
└── tests/                          # [SIL] Ambiente de Testes Software-in-the-Loop (Host PC)
    ├── CMakeLists.txt              # Build isolado SIL + GoogleTest v1.14.0 (FetchContent automático)
    ├── esp_idf_stubs/              # Headers stub do ESP-IDF/FreeRTOS para compilação Host e IntelliSense
    ├── sil_mocks/                  # Mocks e Fakes SIL (MockAdcHal, MockCanHal, MockGpioHal, MockGpsHal, MockRtcHal, MockStorageHal, FakeOsalQueue)
    ├── test_adc_service.cpp        # Valida o bsw::AdcService (10 canais, EMA, calibração, multi-taxa)
    ├── test_bsp_pins.cpp           # Valida os 26 pinos e tags da Pinning Table do esquemático
    ├── test_can_decoder_service.cpp # Valida decodificação CAN (nós C3 + FuelTech) e timeout
    ├── test_can_service.cpp        # Valida o bsw::CanService (11/29-bit, 500 kbps, fila OSAL, Bus-Off)
    ├── test_gps_service.cpp        # Valida o bsw::GpsService e Neo8mUartLld (UBX 10 Hz, NMEA, timestamp)
    ├── test_logger_service.cpp     # Valida TelemetryAggregator, LoggerService (binário PDLG v1) e SdCardLld
    ├── test_ring_buffer.cpp        # Valida o container estático bsw::RingBuffer
    ├── test_rtc_service.cpp        # Valida o bsw::RtcService e Ds3231I2cLld (BCD, OSF, sync 1 ms, fallback)
    └── test_ui_service.cpp         # Valida o bsw::UiService e GpioLld (RPM > 600, hold 5s, beacon, LEDs)
```

---

## Como Clonar e Executar os Testes SIL (TDD no PC)

```powershell
# 1. Clonar o repositório
git clone git@github.com:sid-martins/datalogger-prottoufsc.git
cd datalogger-prottoufsc

# 2. Compilar e rodar todos os 106 testes SIL (Google Test)
.\run_tests.ps1
```

Para executar apenas uma suíte específica (ex: apenas testes do serviço CAN):

```powershell
.\build_tests\datalogger_unit_tests.exe --gtest_filter=*CanService*
```

---

## Como Compilar e Gravar no ESP32-S3 (ESP-IDF)

Com o terminal do **ESP-IDF 5.x** aberto na pasta do projeto:

```powershell
# 1. Definir o target para ESP32-S3 (apenas na primeira vez)
idf.py set-target esp32s3

# 2. Compilar o binário (.bin)
idf.py build

# 3. Gravar na placa (substitua COM4 pela porta do seu kit) e abrir o monitor serial
idf.py -p COM4 flash monitor
```
*(Pressione `Ctrl + ]` para sair do monitor serial).*
