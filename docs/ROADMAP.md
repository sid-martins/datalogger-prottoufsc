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
- [x] Serviço `CanService` (validação ID/DLC, fila OSAL RX, recuperação automática de *Bus-Off*) com 18 testes SIL passando.
- [x] Driver `CanTwaiLld` (`driver/twai.h`) e integração inicial no `main.cpp`.

---

## Fase 2: Levantamento de Especificações e Topologia CAN Distribuída ✅ (Concluída)
*Atende: `RU 1.1` a `RU 8.1` e alinhamento com o FSAE Design Judging Score Sheet.*
- [x] Documentação da arquitetura distribuída (Central ESP32-S3 + 6 Nós ESP32-C3 + ECU) em `docs/REQUIREMENTS.md`.
- [x] Orçamento de banda CAN ($\approx 23 - 28\%$ @ `500 kbps`) e estratégia de empacotamento (`4 x 16-bit = 8 bytes` por nó).

---

## Fase 3: Estratégia de Aquisição da Rede CAN (Protocolo, Decodificador dos Nós C3 + ECU e Tabela de Estado) ⏳ (FOCO ATUAL)
*Atende: `RU 1.1`, `RU 3.1`, `RU 4.2` (Motor + Marcha), `RU 5.1` (Freio/Direção), `RU 5.2` (4 Rodas + IMU WIT), `RU 6.1` (4 Células de Carga).*
- [ ] **3.1 Mapa de IDs e Protocolo CAN (`components/hal/` ou `components/bsw/`):**
  - Definir IDs CAN, endianness, fatores de escala e offsets para:
    - **4x Nós de Roda (`ESP32-C3` — `FL`, `FR`, `RL`, `RR`):** Temp. disco de freio, velocidade da roda, célula de carga, potenciômetro linear de suspensão.
    - **1x Nó de Freio/Direção (`ESP32-C3`):** Ângulo de direção, pressão de freio dianteiro, pressão de freio traseiro, posição do pedal de freio.
    - **1x Nó de IMU WIT (`ESP32-C3`):** Aceleração 3 eixos (`Ax, Ay, Az`) e Giroscópio 3 eixos (`Gx, Gy, Gz`) calibrados.
    - **ECU:** Dados do motor (`RPM`, `TPS`, Temp. Motor, etc.) e **Marcha Atual (`Gear`)**.
- [ ] **3.2 Decodificador e Tabela de Estado (`CanTelemetryService` / `CanDecoder` em `components/bsw/`):**
  - Decodificar cada `CanFrame` recebido sem uso de heap e atualizar a estrutura estática `VehicleCanState`.
  - Rastrear `last_rx_timestamp_ms` por nó para detectar falha/desconexão de qualquer `ESP32-C3` ou da ECU (*Node Stale / Timeout Detection*).
- [ ] **3.3 Testes SIL (`tests/test_can_decoder.cpp`):**
  - Validar via TDD a decodificação exata de todos os nós (`FL/FR/RL/RR`, Freio/Direção, IMU WIT e ECU c/ Marcha), frames corrompidos (`DLC` inválido) e detecção de *timeout* de nó desconectado.

---

## Fase 4: Aquisição Analógica Local na Central — ADC1 (10 Canais + Filtro Digital + Calibração) 🔜
*Atende: `RU 1.2`, `RU 1.3`, `RU 4.1`, `RU 4.2`, Requisito Final `10 canais ADC`.*
- [ ] **HAL (`components/hal/include/i_adc_hal.hpp`) + Mock SIL (`mock_adc_hal.hpp`):** Contrato para leitura dos 10 canais de `bsp::kAnalogChannels` (`OilPressure`, `OilTemp`, `FuelPressure`, `Lambda`, `ADC5to3v3_1..4`, `ADC12to3v3_1..2`).
- [ ] **BSW (`AdcService`):** Filtro digital passa-baixa (EMA / média móvel sem heap), conversão linear ($y = a \cdot x + b$) e taxas configuráveis por grupo de canal.
- [ ] **LLD (`AdcOneshotLld`):** Implementação para `ADC1` do ESP32-S3 (`esp_adc/adc_oneshot.h`) + stubs Host.

---

## Fase 5: Sincronização Temporal (`<= 50 ms`) e RTC Externo `DS3231SN#` (I2C) 🔜
*Atende: `RU 5.3`, Requisito Final `Timestamp <= 50 ms` (Meta: `1 ms`).*
- [ ] **HAL (`IRtcHal`) + BSW (`RtcService`) + LLD (`Ds3231I2cLld`):** Leitura de data/hora do `DS3231SN#` (`GPIO 11/12`, `0x68`) e sincronização com o relógio monotônico de `1 ms` para carimbar os snapshots (CAN + ADC + GPS).

---

## Fase 6: GPS Local `Neo-8M` a 10 Hz (UART) 🔜
*Atende: `RU 5.3`, `RU 6.2` (Sincronia Aero-Velocidade), `RU 7.1` (Traçado/Pilotagem), Requisito Final `GPS 10 Hz`.*
- [ ] **HAL (`IGpsHal`) + BSW (`GpsService`) + LLD (`Neo8mUartLld`):** Configuração `10 Hz` na UART (`GPIO 13/47`) e parser NMEA/UBX sem heap extraindo Latitude, Longitude, Velocidade no Solo e status de *fix*.

---

## Fase 7: Orquestração Dual-Core (Core 1 → `RingBuffer` → Core 0) e Armazenamento MicroSD (`J4` 64 GB) 🔜
*Atende: `RU 2.2`, `RU 4.2`, `RU 5.3`, Requisito Final `Endurance 22 km (64 GB)`.*
- [ ] **Aggregator & RingBuffer (Core 1 → Core 0):** Montar o *snapshot* periódico unificando `VehicleCanState` (6 nós C3 + ECU) + `AdcLocalState` (10 canais) + `GpsState` + `timestamp_ms` e transferir via `RingBuffer` / fila OSAL do **Core 1** para o **Core 0**.
- [ ] **HAL (`IStorageHal`) + BSW (`LoggerService`) + LLD (`SdCardLld`):** Geração automática do cabeçalho `.csv` (compatível com **MoTeC i2 / AiM / MegaLogViewer**) e escrita em bloco no MicroSD com `fsync` periódico.

---

## Fase 8: Interface de Pista (Botões/LEDs) e Máquina de Estados ASW 🔜
*Atende: `RU 2.1`, FSAE Field Serviceability (15 pts).*
- [ ] **HAL (`IGpioHal`) + BSW (`UiService`):** Controle de Start/Stop (`Free_SW1`), *Lap Beacon* (`Free_SW2`) e diagnóstico visual nos LEDs (`LED_Acquiring`, `LED_Working`).
- [ ] **ASW (`main/main.cpp`):** Integração final de todas as tasks Dual-Core.

---

## Fase 9: Validação Integrada em Bancada e Pista (ESP32-S3 + Nós ESP32-C3 + ECU) 🔜
- [ ] Build e flash no ESP32-S3 (`idf.py build`) + teste de recepção CAN com os nós `ESP32-C3` e ECU + validação do log `.csv` no **MoTeC i2** / **MegaLogViewer**.
