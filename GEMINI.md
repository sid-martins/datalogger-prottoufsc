# Contexto do Projeto: Datalogger ESP32-S3 (Padrão Motorsport)

Este arquivo (`GEMINI.md`) é carregado automaticamente em todas as sessões. Ele mantém o resumo executivo do projeto (abaixo de 200 linhas) e aponta para as regras modulares e especificações detalhadas.

---

## 1. Visão Geral
- **Produto:** Firmware de Datalogger Automotivo / Motorsport no **ESP32-S3** (`ESP32-S3-DEVKITC-1-N8R2`).
- **Repositório:** `git@github.com:sid-martins/datalogger-prottoufsc.git`
- **Stack:** **ESP-IDF nativo**, **FreeRTOS (Dual-Core)**, **C++17**.
- **Arquitetura:** Camadas Motorsport/AUTOSAR (`BSP`, `OSAL`, `HAL`, `LLD`, `BSW`, `ASW`, `SIL`).
- **Metodologia:** **TDD / SIL** no Host Windows com **Google Test + Google Mock v1.14.0** (CMake + Ninja + LLVM-MinGW).

---

## 2. Mapa de Regras, Skills e Documentação
- **Regras do Workspace (`.agents/rules/`):**
  - `01-architecture.md` — Camadas Motorsport, direção de dependência, divisão Dual-Core e checklist de componentes.
  - `02-codestyle-cpp.md` — Padrão C++17 embarcado, nomenclatura, zero heap em tempo real e `-Wall -Wextra -Wpedantic`.
  - `03-tdd-sil-workflow.md` — Ciclo obrigatório Red → Green → Refactor, mocks SIL e stubs ESP-IDF no Host.
  - `04-hardware-esp32s3.md` — Restrições de GPIOs do ESP32-S3-N8R2 e uso obrigatório do `bsp_pins.h`.
- **Skill Orquestradora:** `.agents/skills/orchestrator/SKILL.md` — Fluxo padrão para planejar, delegar, implementar e validar novas fases.
- **Especificações, Protocolos e Roadmap:**
  - `docs/CAN_PROTOCOL.md` — **Tabela oficial de CAN IDs (`0x100`, `0x200..0x203`, `0x300..0x301`, FuelTech `0x14080600..0x14080602`), bytes e escalas para programar os nós `ESP32-C3`.**
  - `docs/NEO8M_PROTOCOL.md` — **Referência oficial u-blox NEO-8M / NEO-M8 (`UBX-15031086`, `UBX-13003221`), configuração UBX `10 Hz` (`115200 baud`) e gramática NMEA `$xxGGA`/`$xxRMC`.**
  - `docs/BINARY_LOG_FORMAT.md` — **Especificação oficial do log binário `.bin` (`PDLG v1`, `256 B/registro`, *Sync Word* + `CRC-32`) para decodificadores externos.**
  - `docs/HARDWARE_TEST_PLAN.md` — **Suíte completa de testes de bancada e pista (`TC-00..TC-09`) para validação do hardware montado.**
  - `docs/REQUIREMENTS.md` — Requisitos funcionais, não-funcionais, taxas e calibrações.
  - `docs/ROADMAP.md` — Fases de implementação e acompanhamento de progresso.

---

## 3. Arquitetura Resumida
```text
ASW (main/main.cpp) ──► BSW (components/bsw/) ──► HAL (components/datalogger_hal/) ◄── LLD (components/lld/)
                                              ├──► OSAL (components/osal/) ◄── FreeRtosQueueAdapter
                                              └──► BSP (components/bsp/include/bsp_pins.h)
```
- **Core 1 (`APP_CPU`):** Aquisição determinística em tempo real (CAN RX, ADC, GPS).
- **Core 0 (`PRO_CPU`):** Gravação em MicroSD, sync de arquivos e telemetria.
- **SIL (`tests/`):** Testes no PC usando `tests/sil_mocks/` e `tests/esp_idf_stubs/`.

---

## 4. Hardware Oficial (`components/bsp/include/bsp_pins.h`)
- **ADC1 (10 canais, `GPIO 1..10`):** `OilPressure`, `OilTemp`, `FuelPressure`, `Lambda`, `ADC5to3v3_1..4`, `ADC12to3v3_1..2`.
- **I2C RTC (`DS3231SN#`):** `GPIO 11` (`SDA_I2C_1`), `GPIO 12` (`SCL_I2C_1`).
- **UART GPS (`Neo-8M`):** `GPIO 13` (`GPS_RX`), `GPIO 47` (`GPS_TX`).
- **CAN / TWAI (`SN65HVD230DR` @ 500 kbps):** `GPIO 15` (`CAN_TX`), `GPIO 16` (`CAN_RX`).
- **MicroSD (`J4`):** `GPIO 14` (`MISO`/`DAT0`), `GPIO 17` (`CS`/`DAT3`), `GPIO 18` (`MOSI`/`CMD`), `GPIO 21` (`SCK`), `GPIO 40` (`DAT1`), `GPIO 41` (`DAT2`).
- **UI:** Botões `GPIO 35` (`Free_SW1`), `GPIO 36` (`Free_SW2`); LEDs `GPIO 37` (`LED_Acquiring`), `GPIO 39` (`LED_Working`).

---

## 5. Comandos Essenciais
- **Rodar suíte SIL / TDD no Host:**
  ```powershell
  .\run_tests.ps1
  ```
- **Checar sintaxe do `main/main.cpp` no Host:**
  ```powershell
  g++ -fsyntax-only -std=c++17 -Wall -Wextra -Imain -Icomponents/bsp/include -Icomponents/osal/include -Icomponents/datalogger_hal/include -Icomponents/lld/include -Icomponents/bsw/include -Itests/esp_idf_stubs/include main/main.cpp
  ```
- **Build & Flash no ESP32-S3 (terminal ESP-IDF):**
  ```powershell
  idf.py set-target esp32s3
  idf.py build
  idf.py -p COMx flash monitor
  ```

---

## 6. Estado Atual (Resumo)
- [x] **Fase 0:** Base ESP-IDF, SIL (CMake/Ninja/GTest), Arquitetura Motorsport, `.agents/rules/`, `.agents/skills/orchestrator/` e `docs/`.
- [x] **Fase 0/BSP:** `bsp_pins.h` (26 sinais do esquemático da placa central) validado com TDD.
- [x] **Fase 1 (Base CAN):** `CanService` + `CanTwaiLld` (`SN65HVD230DR` @ 500 kbps, 11/29-bit, auto-recovery completo `Bus-Off -> Stopped -> Start`) — 15 testes do módulo CAN.
- [x] **Fase 2:** Topologia CAN distribuída (Central ESP32-S3 + 6 Nós ESP32-C3 [4x Rodas, Freio/Dir, IMU WIT] + ECU) documentada em `docs/REQUIREMENTS.md` e `docs/ROADMAP.md`.
- [x] **Fase 3 (Protocolo & Decodificador CAN):** `can_protocol.hpp` + `CanDecoderService` (`0x100` Freio/Dir, `0x200..0x203` 4x Rodas, `0x300..0x301` IMU WIT, FuelTech `0x14080600..0x14080602` Motor/Marcha + detecção de timeout) e guia `docs/CAN_PROTOCOL.md` — 24 testes SIL passando.
- [x] **Fase 4 (Aquisição Analógica ADC1):** `IAdcHal` + `AdcOneshotLld` (`ADC1_CH0..9` / `GPIO 1..10`) + `AdcService` (filtro EMA sem heap, calibração linear, divisores multi-taxa `sample_tick`) — 31 testes SIL passando.
- [x] **Fase 5 (RTC `DS3231SN#` & Sincronização Temporal):** `IRtcHal` + `Ds3231I2cLld` (I2C `GPIO 11/12`, `0x68` @ `400 kHz`) + `RtcService` (âncora monotônica `1 ms`, alinhamento $\le 50\ \text{ms}$, fallback degradado em falha I2C/OSF/calendário) — 29 novos testes (`60` SIL totais).
- [x] **Fase 6 (GPS `Neo-8M` 10 Hz & Protocolo Oficial):** `IGpsHal` + `Neo8mUartLld` (UART `GPIO 13/47`, UBX `115200 baud` / `10 Hz` / `Automotive`) + `GpsService` (parser NMEA `$xxGGA`/`$xxRMC` zero-heap, checksum XOR, timestamp `RtcService`) + `docs/NEO8M_PROTOCOL.md` — 19 novos testes (`79` SIL totais).
- [x] **Fase 7 (Orquestração Dual-Core & MicroSD Logger):** `TelemetrySnapshot` + `TelemetryAggregator` + `LoggerService` (100% binário `.bin` `PDLG v1` `256 B/reg` + `CRC-32`, bloco 4 KB, `fsync`) + `IStorageHal` + `SdCardLld` (`J4` SDMMC 4-bit/SPI) + tasks Dual-Core (`Core 1` 100 Hz $\rightarrow$ OSAL $\rightarrow$ `Core 0`) + `docs/BINARY_LOG_FORMAT.md` — 14 testes (`97` SIL totais).
- [x] **Fase 8 (Interface de Pista, Auto-Start RPM & Horímetro NVS):** `IGpioHal`/`GpioLld`/`UiService` (auto-start `RPM > 600` c/ hold `5 s`, `Free_SW1` standby, *Lap Beacon* `Free_SW2`, LEDs `LED_Acquiring`/`LED_Working`) + `INvmHal`/`NvsLld`/`EngineHoursService` (`HH:MM` na NVS a cada `1 min` + parada + header `PDLG v1`) + FSM Dual-Core em `main.cpp` — 16 testes (`113` SIL totais).
- [ ] **Fase 9 (Atual):** Validação integrada em bancada e pista (ESP32-S3 + Nós `ESP32-C3` + ECU FuelTech) e decodificação offline `.bin` (`PDLG v1`).

> **Regra:** Sempre que concluir uma etapa, atualize `docs/ROADMAP.md` e a seção **6** deste arquivo (mantendo `GEMINI.md` <= 200 linhas e sem caminhos locais da máquina).
