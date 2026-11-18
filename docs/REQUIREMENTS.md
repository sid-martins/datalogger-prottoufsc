# Especificações e Requisitos de Engenharia — Datalogger ESP32-S3 (Protto UFSC)

> **Nota de Otimização de Contexto (IA):** A tabela descritiva de *Stakeholders* organizacionais e os requisitos puramente mecânicos/manufatura de PCB (bitola de cabo $0,30\ \text{mm}^2$, DRC Tecnel $0,50\ \text{mm}$, massa $< 1000\ \text{g}$) foram sintetizados na Seção 6 apenas para rastreabilidade do *FSAE Design Briefing*, mantendo este documento 100% focado nas especificações que impactam a arquitetura e o código do firmware.

---

## 1. Estratégia de Aquisição e Alinhamento com o FSAE Score Sheet (`RU 1.1`, `RU 2.1`, `RU 2.2`)

O sistema adota uma **Arquitetura Híbrida (Centralizada para Sensores Críticos + Expansível via CAN 500 kbps)**:
1. **Nó Central (PCB Principal ESP32-S3-N8R2):**
   - Lê diretamente os **10 canais analógicos condicionados (`ADC1_CH0..9`)** com filtros passa-baixa ativos/RC (`MCP6022-E/SN`), **GPS `Neo-8M` (10 Hz)**, **RTC `DS3231SN#` (I2C)**, **MicroSD (até 64 GB)** e **Interface de Pista (2 Botões + 2 LEDs)**.
2. **Barramento CAN 2.0B (`SN65HVD230DR` @ 500 kbps):**
   - Recebe em tempo real os parâmetros da **ECU** (`RPM`, `TPS`, avanço, temperatura do motor) e permite plugar módulos satélites via CAN sem alterar o hardware central (`RU 3.1`, `RU 3.2`) — tais como condicionadores digitais de células de carga (`HX711` 12+ bits para Aero `RU 6.1`), IMU 6 eixos (Acelerômetro/Giroscópio `RU 5.2`) e contadores de frequência das 4 rodas / encoder de direção (`RU 5.1`, `RU 5.2`).

### Como o Firmware Pontua no *FSAE Design Judging Score Sheet* (150 pts):
- **Systems Management & Integration (20 pts):** Demonstra integração limpa entre ECU, sensores analógicos e GPS, além de metodologia formal de verificação com **Testes SIL (Software-in-the-Loop) via Google Test/GMock** e arquitetura em camadas Motorsport (`BSP`, `OSAL`, `HAL`, `LLD`, `BSW`, `ASW`).
- **Refinement / Validation (~25% da nota de Suspension, Aero, Powertrain e Brakes):** Gera arquivos `.csv` sincronizados no tempo prontos para correlação direta entre simulação (CFD, dinâmica veicular, malha fechada de injeção) e dados reais de pista.
- **Manufacturability / Field Serviceability (15 pts):** Diagnóstico visual imediato por LEDs (`LED_Working`, `LED_Acquiring`), auto-recuperação de falhas de barramento (*CAN Bus-Off recovery*) e operação autônoma por botões sem exigir notebook no *paddock*.

---

## 2. Parâmetros Quantitativos do Firmware (Requisitos Finais)

| Parâmetro de Engenharia | Valor Alvo | Unidade | Implementação no Firmware (`BSW` / `LLD` / `ASW`) |
| :--- | :---: | :---: | :--- |
| **Resolução de Timestamp (Alinhamento Multi-Sensor)** | $\le 50$ *(Meta: $1$)* | $\text{ms}$ | Tick do FreeRTOS em `1000 Hz` (`1 ms`) + `esp_timer_get_time()` sincronizado na inicialização com o RTC `DS3231SN#`. |
| **Canais Analógicos Integrados (0–5 V / 0–12 V → 3.3 V)** | $10$ | canais | `ADC1_CH0` a `ADC1_CH9` (`GPIO 1..10`) mapeados em `bsp::kAnalogChannels`. |
| **Taxa de Amostragem por Canal (Critério de Nyquist)** | Configurável ($10\times f_{\text{sinal}}$) | $\text{Hz}$ | Amostragem configurável por grupo de canal (1 década acima da dinâmica física) + filtro digital (média móvel / EMA) em `AdcService` (`RU 1.2`). |
| **Velocidade do Barramento CAN (ECU & Expansão)** | $500$ | $\text{kbps}$ | Implementado em `CanService` + `CanTwaiLld` (`GPIO 15/16`), Standard 11-bit e Extended 29-bit. |
| **Taxa de Atualização do GPS (`Neo-8M`)** | $10$ | $\text{Hz}$ | Configuração via comando UBX/NMEA na UART (`GPIO 13/47`) para frames a cada `100 ms`. |
| **Autonomia de Armazenamento (Endurance 22 km)** | $\ge 1$ stint (SD até $64\ \text{GB}$) | $\text{GB}$ | Sistema de arquivos FAT no MicroSD (`J4`), escrita em lote no Core 0 e `fsync` periódico para evitar corrupção. |
| **Resolução para Células de Carga (Aero CFD)** | $\ge 12$ | $\text{bits}$ | Suporte no payload de dados para 4 canais de *downforce* (via ADC interno 12-bit calibrado ou módulo CAN/digital `HX711` 24-bit). |
| **Canais Digitais / Frequência (Rodas e Encoder)** | Até $10$ (via CAN / Exp.) | canais | Estrutura de dados no Logger preparada para receber velocidades das 4 rodas e ângulo de direção via frames CAN ou expansão. |

---

## 3. Mapeamento de Canais e Taxas de Amostragem Recomendadas (`RU 1.2`, `RU 1.3`)

Para atender à regra de **frequência de amostragem $\ge 1$ década ($10\times$) acima da dinâmica do sinal** sem saturar a gravação no cartão MicroSD:

| Grupo / Canal (`TAG_*` em `bsp_pins.h`) | Pino ESP32-S3 | Dinâmica Típica | Taxa de Amostragem Alvo | Subsistema Atendido |
| :--- | :---: | :---: | :---: | :--- |
| **`OilPressure`**, **`FuelPressure`** | `GPIO 1`, `GPIO 3` | $5 - 10\ \text{Hz}$ | **$50 - 100\ \text{Hz}$** | Powertrain (`RU 4.1` — Saúde do Motor) |
| **`OilTemp`** | `GPIO 2` | $< 1\ \text{Hz}$ | **$10\ \text{Hz}$** | Powertrain (`RU 4.1` — Saúde Térmica) |
| **`Lambda`** | `GPIO 4` | $10\ \text{Hz}$ | **$100\ \text{Hz}$** | Powertrain (`RU 4.2` — Malha Fechada c/ RPM e TPS) |
| **`ADC5to3v3_1..4`** (Suspensão LVDT / Freio / Aero) | `GPIO 5..8` | $10 - 20\ \text{Hz}$ | **$100 - 200\ \text{Hz}$** | Dynamics & Aero (`RU 5.1`, `RU 5.2`, `RU 6.1`) |
| **`ADC12to3v3_1..2`** (Sensores Auxiliares 12 V / Vbat) | `GPIO 9..10` | $1 - 10\ \text{Hz}$ | **$20 - 100\ \text{Hz}$** | Systems Management (`RU 1.5`) |
| **CAN Bus RX** (ECU: RPM, TPS, Vel. Rodas, IMU) | `GPIO 15/16` | Evento / $50 - 100\ \text{Hz}$ | **Assíncrono ($500\ \text{kbps}$)** | Powertrain, Dynamics & Aero (`RU 3.1`, `RU 4.2`, `RU 5.2`) |
| **GPS `Neo-8M`** (Lat, Lon, Velocidade Solo, Satélites) | `GPIO 13/47` | $10\ \text{Hz}$ | **$10\ \text{Hz}$ ($100\ \text{ms}$)** | Dynamics, Aero & Drivers (`RU 5.3`, `RU 6.2`, `RU 7.1`) |

---

## 4. Requisitos Funcionais do Firmware (RF)

- **RF-01 (Aquisição Analógica e Filtragem Digital — `RU 1.2`, `RU 4.1`, `RU 4.2`):**
  - O serviço `AdcService` (`BSW`) deve amostrar os 10 canais do `ADC1`, aplicar filtro digital configurável (ex: média móvel / filtro exponencial IIR de 1ª ordem sem uso de heap) e converter a leitura bruta/mV para grandeza física usando coeficientes lineares ($y = a \cdot x + b$) ou tabela de calibração por canal.
- **RF-02 (Sincronização Temporal Unificada — `RU 5.3`, Requisito Final `50 ms`):**
  - Todo registro gravado deve conter um `timestamp_ms` monotônico (resolução de `1 ms`, muito superior ao requisito mínimo de `50 ms`), sincronizado com a data/hora real do RTC `DS3231SN#` na abertura do arquivo de log.
- **RF-03 (Comunicação CAN e Decodificação ECU — `RU 3.1`, `RU 4.2`, `RU 5.1`, `RU 5.2`):**
  - O `CanService` deve operar a `500 kbps` com auto-recuperação de *Bus-Off* e alimentar uma tabela de estado dos canais CAN (RPM, TPS, velocidades de roda, ângulo de volante, forças G / giroscópio e células de carga caso venham via barramento CAN).
- **RF-04 (Posicionamento e Velocidade GPS a 10 Hz — `RU 5.3`, `RU 6.2`, `RU 7.1`):**
  - O `GpsService` deve processar sentenças NMEA/UBX do módulo `Neo-8M` a `10 Hz`, extraindo latitude, longitude, velocidade no solo (*ground speed* em km/h ou m/s) e status de *fix*, mantendo a última posição válida disponível para o Logger sem bloquear o Core 1.
- **RF-05 (Exportação `.csv` Compatível com MoTeC i2 / AiM / MegaLogViewer — `RU 4.2`, `RU 5.3`):**
  - O `LoggerService` deve gerar arquivos `.csv` no cartão MicroSD cujo cabeçalho seja montado automaticamente a partir das `TAG_*` de `bsp_pins.h` e dos canais CAN/GPS, garantindo compatibilidade direta de importação no **MegaLogViewer**, **MoTeC i2** e **AiM Race Studio**.
- **RF-06 (Controle de Sessão e Diagnóstico via Botões e LEDs — `RU 2.1`):**
  - Os botões `Free_SW1` (`GPIO 35`) e `Free_SW2` (`GPIO 36`) devem permitir iniciar/pausar a gravação de um *stint* e inserir marcador de evento/volta (*Beacon/Lap Marker*).
  - Os LEDs `LED_Working` (`GPIO 39`) e `LED_Acquiring` (`GPIO 37`) devem indicar claramente os estados: `IDLE` (pronto), `ACQUIRING` (gravando no SD) e `FAULT` (erro de SD/CAN/RTC).

---

## 5. Requisitos Não-Funcionais de Firmware (RNF)

- **RNF-01 (Determinismo Dual-Core no ESP32-S3):**
  - **Core 1 (`APP_CPU`):** Exclusivo para aquisição determinística (CAN RX, amostragem ADC1 com `vTaskDelayUntil`, recepção UART GPS). Zero chamadas bloqueantes de sistema de arquivos.
  - **Core 0 (`PRO_CPU`):** Exclusivo para formatação CSV, escrita em lote no cartão MicroSD e `fsync` periódico.
- **RNF-02 (Zero Alocação Dinâmica em Tempo Real):**
  - Proibido usar `new`, `malloc`, `std::vector` ou concatenação de `std::string` no loop de amostragem; utilizar buffers estáticos e `bsw::RingBuffer<T, Capacity>`.
- **RNF-03 (Resiliência contra Queda de Alimentação em Pista):**
  - O `LoggerService` deve executar `sync()` (`fsync`) periódico a cada lote de amostras para garantir que, em caso de desligamento brusco da chave geral do carro, os dados do *stint* de Endurance (22 km) até o último segundo estejam salvos no MicroSD.
- **RNF-04 (Modularidade e Testabilidade SIL — `RU 3.2`):**
  - 100% dos serviços da camada `BSW` devem ser validados no Host via Google Test / Google Mock (`.\run_tests.ps1`) com `0 warnings` (`-Wall -Wextra -Wpedantic`).

---

## 6. Resumo de Rastreabilidade de Hardware / PCB (Referência FSAE Design Report)

*(Requisitos atendidos pelo projeto elétrico/mecânico da placa e chicote — mantidos aqui apenas para consulta rápida):*
- **`RU 1.2` (Filtro Anti-Aliasing Hardware):** Filtros passa-baixa nos estágios `MCP6022-E/SN` (`Measurement_Engine`, `Measurement_5to3v3`, `Measurement_12to3v3`) com frequência de corte 1 década acima da banda do canal.
- **`RU 1.4` (Chicote Elétrico):** Cabos blindados em par trançado de $0,30\ \text{mm}^2$ com conectores automotivos (`J1 DT13-4P`, `J2/J3 776280-2`).
- **`RU 1.5` (Alimentação e Proteção):** Conversor Buck 3A (`U4`) + LDO `LM3940IMP-3.3` (`B2`), fusível resetável `F1`, diodo Schottky `D1`, ferrite beads (`B1`, `B3..B7`) e TVS CAN `SM712` (`U1`) operando na rede LV ($5\ \text{V} / 3,3\ \text{V}$).
- **`RU 7.2` & `RU 8.1` (Packaging e Manufatura PCB):** Massa total do sistema $< 1000\ \text{g}$ e trilhas de potência $\ge 0,50\ \text{mm}$ conforme DRC da patrocinadora Tecnel.
