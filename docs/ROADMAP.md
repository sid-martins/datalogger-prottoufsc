# Roadmap de Implementação — Datalogger Central ESP32-S3 (Protto UFSC)

Este roadmap reflete a **Arquitetura Distribuída via CAN** (Datalogger Central no **ESP32-S3** + Nós Satélites **ESP32-C3** [4x Rodas, 1x Freio/Direção, 1x IMU WIT] + **ECU**) e prioriza primeiro toda a estratégia de aquisição e decodificação CAN antes dos periféricos locais.

---

## Fase 0: Fundação, Arquitetura Motorsport e Ambiente SIL ✅ (Concluída)
- [x] Estrutura ESP-IDF (`main/`, `components/`, `sdkconfig.defaults` c/ FreeRTOS `1000 Hz`, `partitions.csv`) e repositório remoto (`datalogger-prottoufsc`).
- [x] Ambiente SIL no Host (CMake + Ninja + LLVM-MinGW + Google Test/GMock v1.14.0 + `run_tests.ps1` com *Quality Gate*).
- [x] Arquitetura em 5 camadas fixas Motorsport (`bsp`, `osal`, `hal`, `lld`, `bsw`) + `asw` (`main/`) e `sil_mocks` (`tests/`).
- [x] **BSP (`bsp_pins.h`):** Pinning Table completa dos 26 sinais do esquemático da placa central validada via TDD (`test_bsp_pins.cpp`).
- [x] **OSAL (`IOsalQueue` / `FreeRtosQueueAdapter`):** Abstração de filas FreeRTOS + dublê `FakeOsalQueue`.
- [x] **BSW (`RingBuffer<T, Capacity>`):** Buffer circular estático sem heap validado via TDD.
- [x] Governança de IA: `.agents/rules/`, `.agents/skills/orchestrator/` e `GEMINI.md` ($< 200$ linhas).

---

## Fase 1: Transporte Base CAN Bus (`SN65HVD230DR` @ 500 kbps) ✅ (Concluída)
*Atende: `RU 3.1`, Requisito Final `500 kbps`.*
- [x] Contrato `ICanHal` (`CanFrame` 11/29-bit, `CanConfig` @ 500 kbps nos pinos `GPIO 15/16`).
- [x] Serviço `CanService` (validação ID/DLC, fila OSAL RX, recuperação completa *Bus-Off -> Stopped -> Start*) com 15 testes SIL no módulo CAN.
- [x] Driver `CanTwaiLld` (`driver/twai.h`) e integração inicial no `main.cpp`.

---

## Fase 2: Levantamento de Especificações e Topologia CAN Distribuída ✅ (Concluída)
*Atende: `RU 1.1` a `RU 8.1` e alinhamento com o FSAE Design Judging Score Sheet.*
- [x] Documentação da arquitetura distribuída (Central ESP32-S3 + 6 Nós ESP32-C3 + ECU) em `docs/REQUIREMENTS.md`.
- [x] Orçamento de banda CAN ($\approx 23 - 28\%$ @ `500 kbps`) e estratégia de empacotamento (`4 x 16-bit = 8 bytes` por nó).

---

## Fase 3: Estratégia de Aquisição da Rede CAN (Protocolo, Decodificador dos Nós C3 + ECU e Tabela de Estado) ✅ (Concluída)
*Atende: `RU 1.1`, `RU 3.1`, `RU 4.2` (Motor + Marcha), `RU 5.1` (Freio/Direção), `RU 5.2` (4 Rodas + IMU WIT), `RU 6.1` (4 Células de Carga).*
- [x] **3.1 Mapa de IDs e Protocolo CAN (`components/bsw/include/can_protocol.hpp` e `docs/CAN_PROTOCOL.md`):**
  - Definidos IDs CAN, endianness, fatores de escala e offsets para:
    - **4x Nós de Roda (`ESP32-C3` — `0x200` FL, `0x201` FR, `0x202` RL, `0x203` RR):** Potenciômetro linear de suspensão, velocidade da roda, célula de carga, temp. disco de freio.
    - **1x Nó de Freio/Direção (`ESP32-C3` — `0x100`):** Ângulo de direção, pressão de freio dianteiro, pressão de freio traseiro, posição do pedal de freio.
    - **1x Nó de IMU WIT (`ESP32-C3` — `0x300` Accel / `0x301` Gyro):** Aceleração 3 eixos (`Ax, Ay, Az`) e Giroscópio 3 eixos (`Gx, Gy, Gz`) calibrados.
    - **ECU FuelTech (`FTCAN 2.0` — `0x14080600..0x14080602` / `0x600..0x602`):** Dados do motor (`RPM`, `TPS`, `MAP`, `Lambda`, Temps, Pressões) e **Marcha Atual (`Gear`)**.
- [x] **3.2 Decodificador e Tabela de Estado (`CanDecoderService` em `components/bsw/`):**
  - Decodifica cada `CanFrame` recebido sem uso de heap e atualiza a estrutura estática `VehicleCanState`.
  - Rastreia `last_rx_timestamp_ms` por nó para detectar falha/desconexão de qualquer `ESP32-C3` ou da ECU (*Node Stale / Timeout Detection*).
- [x] **3.3 Testes SIL (`tests/test_can_decoder_service.cpp`):**
  - Validada via TDD a decodificação de todos os nós (`FL/FR/RL/RR`, Freio/Direção, IMU WIT e ECU FuelTech c/ Marcha), frames corrompidos (`DLC` inválido / RTR) e detecção de *timeout* de nó desconectado (24 testes SIL totais passando).

---

## Fase 4: Aquisição Analógica Local na Central — ADC1 (10 Canais + Filtro Digital + Calibração) ✅ (Concluída)
*Atende: `RU 1.2`, `RU 1.3`, `RU 4.1`, `RU 4.2`, Requisito Final `10 canais ADC`.*
- [x] **HAL (`components/datalogger_hal/include/i_adc_hal.hpp`) + Mock SIL (`mock_adc_hal.hpp`):** Contrato para leitura dos 10 canais de `bsp::kAnalogChannels` (`OilPressure`, `OilTemp`, `FuelPressure`, `Lambda`, `ADC5to3v3_1..4`, `ADC12to3v3_1..2`).
- [x] **BSW (`AdcService`):** Filtro digital passa-baixa (EMA sem heap), conversão linear ($y = a \cdot x + b$) e amostragem multi-taxa por divisor de tick (`sample_tick`) com 7 testes SIL passando (`test_adc_service.cpp`).
- [x] **LLD (`AdcOneshotLld`):** Implementação para `ADC1` do ESP32-S3 (`esp_adc/adc_oneshot.h`) + stubs Host e inicialização em `main/main.cpp`.

---

## Fase 5: Sincronização Temporal (`<= 50 ms`) e RTC Externo `DS3231SN#` (I2C) ✅ (Concluída)
*Atende: `RU 5.3`, Requisito Final `Timestamp <= 50 ms` (Meta: `1 ms`).*
- [x] **HAL (`components/datalogger_hal/include/i_rtc_hal.hpp`) + Mock SIL (`tests/sil_mocks/mock_rtc_hal.hpp`):** Contrato `IRtcHal` (`RtcI2cConfig`, `RtcDateTime`, `RtcDeviceStatus`) usando `PIN_I2C_SDA` (`GPIO 11`), `PIN_I2C_SCL` (`GPIO 12`), `I2C_MASTER_FREQ_HZ` (`400 kHz`) e `RTC_DS3231_I2C_ADDR` (`0x68`).
- [x] **BSW (`RtcService` em `components/bsw/`):** Validação de calendário (`2000..2099`, anos bissextos), âncora monotônica de `1 ms` ($\le 50\ \text{ms}$), matemática epoch sem heap, `format_session_timestamp()` e fallback degradado monotônico (`RtcSyncStatus`) em falha I2C, perda de oscilador (`OSF`) ou calendário inválido.
- [x] **LLD (`Ds3231I2cLld` em `components/lld/`):** Driver I2C Master (`driver/i2c_master.h`, leitura em rajada `0x00..0x0F`, `/EOSC=0` e limpeza de `OSF` em `write_datetime()`) + stub Host (`tests/esp_idf_stubs/include/driver/i2c_master.h`), inicialização em `main/main.cpp` e `29` novos testes SIL (`tests/test_rtc_service.cpp`, `60` testes SIL totais passando).

---

## Fase 6: GPS Local `Neo-8M` a 10 Hz (UART) ✅ (Concluída)
*Atende: `RU 5.3`, `RU 6.2` (Sincronia Aero-Velocidade), `RU 7.1` (Traçado/Pilotagem), Requisito Final `GPS 10 Hz`.*
- [x] **Documentação Oficial (`docs/NEO8M_PROTOCOL.md`):** Referências normativas u-blox (`UBX-15031086`, `UBX-13003221`, `UBX-15031913`, `UBX-15029985`), análise de banda UART (`9600` $\rightarrow$ `115200 baud`), comandos binários UBX (`10 Hz`) e gramática NMEA `$xxGGA`/`$xxRMC`.
- [x] **HAL (`components/datalogger_hal/include/i_gps_hal.hpp`) + Mock SIL (`tests/sil_mocks/mock_gps_hal.hpp`):** Contrato `IGpsHal` (`GpsConfig` em `PIN_GPS_RX`/`PIN_GPS_TX` `GPIO 13/47`, `10 Hz`, `dynModel = 4` e `GpsData`).
- [x] **BSW (`GpsService` em `components/bsw/`):** Framing de linha fixo (`128 B`) sem heap, descarte limitado de frames UBX (`0xB5 0x62`), validação XOR NMEA, parser `$GPGGA`/`$GNGGA` e `$GPRMC`/`$GNRMC` (`12..14` campos: lat/lon, velocidade em nós/km/h/m/s, curso, satélites, *fix*) e carimbo temporal via `RtcService` (com fallback degradado).
- [x] **LLD (`Neo8mUartLld` em `components/lld/`):** Driver UART1 (`driver/uart.h`) com sequência de boot UBX (`UBX-CFG-PRT` `115200`, `UBX-CFG-MSG` GGA/RMC, `UBX-CFG-RATE` `100 ms`, `UBX-CFG-NAV5` Automotive) + stub Host (`tests/esp_idf_stubs/include/driver/uart.h`), inicialização em `main/main.cpp` e `19` novos testes SIL (`tests/test_gps_service.cpp`, **`79` testes SIL totais passando**).

---

## Fase 7: Orquestração Dual-Core (Core 1 → `RingBuffer` → Core 0) e Armazenamento MicroSD (`J4` 64 GB) ✅ (Concluída)
*Atende: `RU 2.2`, `RU 4.2`, `RU 5.3`, Requisito Final `Endurance 22 km (64 GB)`.*
- [x] **Aggregator & Snapshot (`TelemetrySnapshot` + `TelemetryAggregator` em `components/bsw/`):** Unifica `RtcService` (`1 ms`), `CanDecoderService` (6 nós C3 + ECU), `AdcService` (10 canais) e `GpsService`, publicando via fila OSAL do **Core 1 (`acquisition_task` @ 100 Hz)** para o **Core 0**, com instanciação em `RingBuffer<TelemetrySnapshot, 32>`.
- [x] **HAL (`IStorageHal`) + BSW (`LoggerService` / `binary_log_format.hpp`) + LLD (`SdCardLld`) + Spec (`docs/BINARY_LOG_FORMAT.md`):** Suporte a SDMMC 4-bit e SPI no conector `J4` (`GPIO 14, 17, 18, 21, 40, 41`), gravação **100% binária `.bin` (`PDLG v1`)** com cabeçalho autodescritivo de `256 bytes` (contendo as `10` `TAG_*` de `bsp::kAnalogChannels`), registros fixos de `256 bytes` Little-Endian com *Sync Word* `0xA55A` e CRC-32 IEEE 802.3, buffer de bloco `4 KB` (`16` registros) com `fsync`/`_commit` periódico, stubs Host e `14` testes SIL em `test_logger_service.cpp` (**`97` testes SIL totais passando**).

---

## Fase 8: Interface de Pista (Acionamento por RPM, Beacon/LEDs) e Máquina de Estados ASW ✅ (Concluída)
*Atende: `RU 2.1`, `RF-06`, FSAE Field Serviceability (15 pts).*
- [x] **HAL (`IGpioHal` em `components/datalogger_hal/`) + LLD (`GpioLld`) + BSW (`UiService`):** Gravação automática acionada por **`RPM > 600`** (com ECU online na CAN) e encerramento automático após **`5 s` contínuos** com `RPM <= 600` ou ECU offline; botão `Free_SW1` (`GPIO 35`) reservado em *standby* (sem ação ativa), *Lap Beacon* com debounce (`50 ms`) em `Free_SW2` (`GPIO 36`) e diagnóstico visual nos LEDs (`LED_Acquiring` contínuo gravando; `LED_Working` heartbeat `1 Hz`, alerta CAN `2,5 Hz` e falha SD `5 Hz`) — `9` novos testes em `test_ui_service.cpp` (**`106` testes SIL totais passando**).
- [x] **ASW (`main/main.cpp` + `sdkconfig.defaults`):** Integração final de todas as tasks Dual-Core (Core 1: `can_rx_task` + `acquisition_task` 100 Hz; Core 0: `ui_task` 50 Hz + `storage_logger_task` com cooldown de retry de SD e drenagem de fila), `CONFIG_SPIRAM=n` e build real validado no **ESP-IDF `v5.4.4`** (`idf.py build`, `0` warnings).

---

## Fase 9: Validação Integrada em Bancada e Pista (ESP32-S3 + Nós ESP32-C3 + ECU) ⏳ (Próximo Passo)
- [x] **Plano e Suíte de Testes em Hardware (`docs/HARDWARE_TEST_PLAN.md`):** Roteiro completo `TC-00..TC-09` (*Smoke Test*, Boot Dual-Core, UI/LEDs, RTC `DS3231SN#`, `10x ADC1`, GPS `Neo-8M` `10 Hz`, CAN `500 kbps` c/ vetores hexadecimais e *Bus-Off*, Auto-Start `RPM > 600` + corte de energia e script auditor Python `PDLG v1`).
- [ ] Execução da suíte `docs/HARDWARE_TEST_PLAN.md` na placa física montada + teste integrado com os nós `ESP32-C3` e ECU + validação no **MoTeC i2** / **MegaLogViewer**.
