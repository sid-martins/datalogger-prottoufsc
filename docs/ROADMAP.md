# Roadmap de Implementação — Datalogger ESP32-S3 (Protto UFSC)

Este roadmap organiza o desenvolvimento incremental do firmware em fases pequenas e testáveis via **TDD/SIL**, alinhadas aos requisitos de projeto (`docs/REQUIREMENTS.md`) e aos critérios de pontuação do **FSAE Design Judging Score Sheet**.

---

## Fase 0: Fundação, Arquitetura Motorsport e Ambiente SIL ✅ (Concluída)
- [x] Estrutura ESP-IDF (`main/`, `components/`, `sdkconfig.defaults` c/ FreeRTOS `1000 Hz` / `1 ms`, `partitions.csv`) e repositório remoto (`datalogger-prottoufsc`).
- [x] Ambiente SIL no Host (CMake + Ninja + LLVM-MinGW + Google Test/GMock v1.14.0 + `run_tests.ps1` com *Quality Gate*).
- [x] Arquitetura em 5 camadas fixas Motorsport (`bsp`, `osal`, `hal`, `lld`, `bsw`) + `asw` (`main/`) e `sil_mocks` (`tests/`).
- [x] **BSP (`bsp_pins.h`):** Pinning Table completa dos 26 sinais do esquemático validada via TDD (`test_bsp_pins.cpp`).
- [x] **OSAL (`IOsalQueue` / `FreeRtosQueueAdapter`):** Abstração de filas FreeRTOS + dublê `FakeOsalQueue`.
- [x] **BSW (`RingBuffer<T, Capacity>`):** Buffer circular estático sem heap validado via TDD.
- [x] Governança de IA: `.agents/rules/` (com `trigger: always_on`), `.agents/skills/orchestrator/` (com perfis de subagentes e protocolo anti-colisão) e `GEMINI.md` enxuto ($< 200$ linhas).

---

## Fase 1: Comunicação CAN Bus (`SN65HVD230DR` @ 500 kbps) ✅ (Base Concluída)
*Atende: `RU 1.1`, `RU 3.1`, `RU 4.2`, Requisito Final `500 kbps`.*
- [x] Contrato `ICanHal` (`CanFrame` 11/29-bit, `CanConfig` @ 500 kbps nos pinos `GPIO 15/16`).
- [x] Serviço `CanService` (validação ID/DLC, fila OSAL RX, recuperação automática de *Bus-Off*) com 18 testes SIL passando.
- [x] Driver `CanTwaiLld` (`driver/twai.h`) e integração inicial Dual-Core no `main.cpp`.

---

## Fase 2: Levantamento de Especificações e Alinhamento FSAE ✅ (Concluída)
*Atende: Rastreabilidade completa de `RU 1.1` a `RU 8.1` e FSAE Design Score Sheet.*
- [x] Consolidação dos requisitos técnicos e metas quantitativas em `docs/REQUIREMENTS.md` (descartando texto organizacional redundante para economizar tokens).
- [x] Definição das taxas de amostragem por grupo de canal (critério de Nyquist $10\times$), resolução temporal ($\le 50\ \text{ms}$, meta $1\ \text{ms}$), GPS a $10\ \text{Hz}$ e formato de exportação `.csv` (MoTeC i2 / AiM / MegaLogViewer).

---

## Fase 3: Aquisição Analógica — ADC1 (10 Canais + Filtro Digital + Calibração) ⏳ (Próximo Passo)
*Atende: `RU 1.2`, `RU 1.3`, `RU 4.1`, `RU 4.2`, `RU 5.1`, `RU 5.2`, `RU 6.1`, Requisito Final `10 canais ADC`.*
- [ ] **HAL (`components/hal/include/i_adc_hal.hpp`):** Definir contrato `IAdcHal` para leitura em mV/raw dos 10 canais de `bsp::kAnalogChannels` (`OilPressure`, `OilTemp`, `FuelPressure`, `Lambda`, `ADC5to3v3_1..4`, `ADC12to3v3_1..2`).
- [ ] **SIL (`tests/sil_mocks/mock_adc_hal.hpp` & `tests/test_adc_service.cpp`):** Testar amostragem multi-taxa, filtro digital passa-baixa (EMA / média móvel sem heap), calibração linear ($y = a \cdot x + b$) e detecção de falha/saturação de sensor.
- [ ] **BSW (`components/bsw/`):** Implementar `AdcService` com suporte a taxas configuráveis por canal e estrutura `AdcSampleSnapshot`.
- [ ] **LLD (`components/lld/`):** Implementar `AdcOneshotLld` usando `esp_adc/adc_oneshot.h` + calibração do ESP32-S3 e criar stub em `tests/esp_idf_stubs/`.

---

## Fase 4: Sincronização Temporal e RTC Externo `DS3231SN#` (I2C) 🔜
*Atende: `RU 5.3`, Requisito Final `Timestamp <= 50 ms` (Meta: `1 ms`).*
- [ ] **HAL (`components/hal/include/i_rtc_hal.hpp`):** Contrato para leitura/escrita de data/hora (`RtcDateTime`) e status do oscilador no `DS3231SN#` (`0x68` @ `400 kHz`, `GPIO 11/12`).
- [ ] **BSW (`components/bsw/`):** Implementar `TimeService` / `RtcService` combinando data/hora real do RTC (para nomear arquivos de sessão e cabeçalhos) com contador monotônico de `1 ms` (`esp_timer`), garantindo alinhamento temporal entre CAN, ADC e GPS muito abaixo do limite de `50 ms`.
- [ ] **LLD (`components/lld/`):** Implementar `Ds3231I2cLld` e stubs I2C no Host.
- [ ] **SIL (`tests/test_rtc_service.cpp`):** Validar conversão BCD, validação de calendário e fallback seguro caso a bateria `CR2032` do RTC esteja descarregada.

---

## Fase 5: GPS `Neo-8M` a 10 Hz (UART) 🔜
*Atende: `RU 5.3`, `RU 6.2` (Sincronia Aero-Velocidade), `RU 7.1` (Traçado/Pilotagem), Requisito Final `GPS 10 Hz`.*
- [ ] **HAL (`components/hal/include/i_gps_hal.hpp`):** Contrato para recepção de bytes/linhas da UART (`GPIO 13` RX / `GPIO 47` TX) e envio de comando de configuração `10 Hz`.
- [ ] **BSW (`components/bsw/`):** Implementar `GpsService` com parser NMEA determinístico (sem `malloc`/`std::string` dinâmico) extraindo Latitude, Longitude, Velocidade no Solo (km/h e m/s), curso, número de satélites e *fix* válido a `10 Hz` (`100 ms`).
- [ ] **LLD (`components/lld/`):** Implementar `Neo8mUartLld` (`driver/uart.h`) e stub correspondente.
- [ ] **SIL (`tests/test_gps_service.cpp`):** Validar parsing de sentenças `$GPGGA`/`$GPRMC` (ou `$GNGGA`/`$GNRMC`), validação de checksum NMEA e tratamento de perda de sinal de satélite.

---

## Fase 6: Decodificador CAN (ECU & Expansão Digital/Aero/IMU) 🔜
*Atende: `RU 1.1`, `RU 4.2` (Lambda + RPM + TPS), `RU 5.1`, `RU 5.2`, `RU 6.1`.*
- [ ] **BSW (`components/bsw/`):** Implementar `CanDecoderService` sobre os frames recebidos pelo `CanService`, mantendo o estado atualizado de:
  - **ECU (Powertrain):** `RPM`, `TPS` (posição da borboleta), temperatura do motor, tensão de bateria.
  - **Expansão Dinâmica / Aero (quando presente no barramento):** Velocidade das 4 rodas, ângulo de volante, IMU (aceleração lateral/longitudinal e yaw rate) e células de carga (`HX711`).
- [ ] **SIL (`tests/test_can_decoder_service.cpp`):** Validar extração *big-endian* / *little-endian*, fatores de escala, offset e *timeout* de mensagem CAN stale.

---

## Fase 7: Armazenamento MicroSD (`J4` 64 GB) e Exportador CSV (MoTeC / AiM / MegaLogViewer) 🔜
*Atende: `RU 2.2`, `RU 4.2`, `RU 5.3`, Requisito Final `Endurance 22 km (64 GB)`.*
- [ ] **HAL (`components/hal/include/i_storage_hal.hpp`):** Evoluir contrato com abertura de arquivo de sessão, escrita em bloco, `sync()` (`fsync`) e verificação de espaço livre.
- [ ] **BSW (`components/bsw/`):** Implementar `LoggerService` (rodando no **Core 0**):
  - Geração automática da linha de cabeçalho `.csv` usando as `TAG_*` de `bsp_pins.h` + colunas de ECU CAN + colunas de GPS (`Timestamp_ms,OilPressure,OilTemp,FuelPressure,Lambda,...,RPM,TPS,GPS_Speed_kmh,GPS_Lat,GPS_Lon,LapBeacon`).
  - Formatação em buffer estático e `sync()` periódico a cada $N$ registros para proteção contra desligamento brusco no Enduro (22 km).
- [ ] **LLD (`components/lld/`):** Implementar `SdCardLld` para o conector `J4` (`GPIO 14, 17, 18, 21, 40, 41`).
- [ ] **SIL (`tests/test_logger_service.cpp`):** Validar cabeçalho CSV, alinhamento de colunas com timestamp de `< 50 ms` e recuperação em falha de escrita no SD.

---

## Fase 8: Interface de Pista (Botões/LEDs) e Máquina de Estados ASW 🔜
*Atende: `RU 2.1` (Confiabilidade/MVP), FSAE Field Serviceability (15 pts).*
- [ ] **HAL (`components/hal/include/i_gpio_hal.hpp`):** Contrato de leitura dos botões `Free_SW1` (`GPIO 35`) e `Free_SW2` (`GPIO 36`) e acionamento dos LEDs `LED_Acquiring` (`GPIO 37`) e `LED_Working` (`GPIO 39`).
- [ ] **BSW (`components/bsw/`):** Implementar `UiService` e máquina de estados do Datalogger:
  - `Free_SW1`: Iniciar / Parar gravação do *stint* no MicroSD.
  - `Free_SW2`: Inserir marcador de volta / evento (*Lap Beacon*) no log CSV.
  - Padrões visuais de `LED_Working` e `LED_Acquiring` para `IDLE`, `ACQUIRING` e `FAULT`.
- [ ] **ASW (`main/main.cpp`):** Orquestração final integrando todas as tasks Dual-Core (**Core 1:** Aquisição CAN + ADC + GPS; **Core 0:** Logger MicroSD + UI).

---

## Fase 9: Bring-up em Bancada e Validação em Pista (ESP32-S3 Hardware-in-the-Loop) 🔜
*Atende: FSAE Refinement/Validation (25% da nota de Design).*
- [ ] Build final com `idf.py set-target esp32s3 && idf.py build`.
- [ ] Teste integrado em bancada (ADC 10 canais, RTC I2C, CAN 500 kbps, GPS 10 Hz, MicroSD 64 GB, Botões/LEDs).
- [ ] Importação de um arquivo `.csv` real gravado pela placa no **MegaLogViewer** / **MoTeC i2** para comprovação no *Design Briefing*.
