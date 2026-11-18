# Roadmap de Implementação — Datalogger ESP32-S3

Este roadmap organiza o desenvolvimento incremental em fases pequenas e executáveis via **TDD/SIL**, respeitando a arquitetura Motorsport (`BSP` → `HAL`/`OSAL` → `BSW` → `LLD` → `ASW`).

---

## Fase 0: Fundação, Arquitetura e Ambiente SIL ✅ (Concluída)
- [x] Estrutura ESP-IDF (`main/`, `components/`, `sdkconfig.defaults`, `partitions.csv`) e repositório remoto (`datalogger-prottoufsc`).
- [x] Ambiente SIL no Host (CMake + Ninja + LLVM-MinGW + Google Test/GMock v1.14.0 + `run_tests.ps1`).
- [x] Arquitetura em camadas Motorsport (`bsp`, `osal`, `hal`, `lld`, `bsw`, `asw`, `sil_mocks`).
- [x] **BSP (`bsp_pins.h`):** Pinning Table completa dos 26 sinais do esquemático validada via TDD (`test_bsp_pins.cpp`).
- [x] **OSAL (`IOsalQueue` / `FreeRtosQueueAdapter`):** Abstração de filas FreeRTOS + dublê `FakeOsalQueue`.
- [x] **BSW (`RingBuffer<T, Capacity>`):** Buffer circular estático sem heap validado via TDD.
- [x] Estrutura de regras (`.agents/rules/`), skill orquestradora (`.agents/skills/orchestrator/`) e documentação (`GEMINI.md`, `README.md`, `docs/`).

---

## Fase 1: Comunicação CAN Bus (`SN65HVD230DR`) ✅ (Base Concluída)
- [x] Contrato `ICanHal` (`CanFrame` 11/29-bit, `CanConfig` @ 500 kbps).
- [x] Serviço `CanService` (validação ID/DLC, fila OSAL RX, recuperação automática de *Bus-Off*) com 18 testes SIL passando.
- [x] Driver `CanTwaiLld` (`driver/twai.h`) e integração inicial no `main.cpp`.
- [ ] *(Opcional após definir `REQUIREMENTS.md`)*: Filtros de aceitação CAN ou decodificador DBC.

---

## Fase 2: Especificação e Requisitos ⏳ (Próximo Passo Imediato)
- [ ] Preencher `docs/REQUIREMENTS.md` com taxas de amostragem (Hz), calibrações dos sensores analógicos, formato do arquivo de log (CSV/binário), regras dos botões/LEDs e comportamento do GPS/RTC.
- [ ] Refinar as fases abaixo com base nos requisitos fechados.

---

## Fase 3: Aquisição Analógica — ADC1 (10 Canais) 🔜
- [ ] Definir `IAdcHal` (`components/hal/`) + `MockAdcHal` (`tests/sil_mocks/`).
- [ ] Implementar `AdcService` em `components/bsw/` (leitura dos 10 canais de `kAnalogChannels`, filtragem/oversampling e conversão mV → unidade física usando `TAG_*`).
- [ ] Implementar `AdcOneshotLld` em `components/lld/` (`esp_adc/adc_oneshot.h` + calibração) e stubs Host.
- [ ] Validar via testes SIL e integrar na task de tempo real (Core 1).

---

## Fase 4: RTC Externo `DS3231SN#` (I2C) 🔜
- [ ] Definir `II2cHal` / `IRtcHal` + Mocks SIL.
- [ ] Implementar `RtcService` em `components/bsw/` (leitura/escrita de data/hora, conversão BCD, timestamp para nomes de arquivo e linhas de log).
- [ ] Implementar `Ds3231I2cLld` em `components/lld/`.

---

## Fase 5: GPS `Neo-8M` (UART) 🔜
- [ ] Definir `IUartHal` / `IGpsHal` + Mocks SIL.
- [ ] Implementar `GpsService` + parser NMEA (`GGA`, `RMC`, `VTG` ou UBX) em `components/bsw/`.
- [ ] Implementar `Neo8mUartLld` em `components/lld/`.

---

## Fase 6: Armazenamento MicroSD (`J4`) e Logger Unificado 🔜
- [ ] Definir `IStorageHal` completo (criação de sessão/arquivo, header automático com `TAG_*`, rotação e `sync`).
- [ ] Implementar `LoggerService` em `components/bsw/` (agregando amostras CAN + ADC + GPS + RTC em CSV/binário sem alocação dinâmica no loop crítico).
- [ ] Implementar `SdCardLld` em `components/lld/` (SDMMC 4-bit ou SPI conforme definição).

---

## Fase 7: Interface de Usuário (Botões/LEDs) e Máquina de Estados 🔜
- [ ] Definir `IGpioHal` + Mocks SIL.
- [ ] Implementar `UiService` (debounce por software complementar ao RC, eventos de `Free_SW1`/`Free_SW2`, padrões de status de `LED_Acquiring` e `LED_Working`).
- [ ] Integrar máquina de estados principal (`IDLE`, `ACQUIRING`, `ERROR`) no `main.cpp` (ASW).

---

## Fase 8: Validação em Hardware (HIL / Bancada ESP32-S3) 🔜
- [ ] Compilação oficial com `idf.py set-target esp32s3 && idf.py build`.
- [ ] Testes em bancada por subsistema (ADC, RTC, CAN @ 500 kbps, GPS, MicroSD, UI).
