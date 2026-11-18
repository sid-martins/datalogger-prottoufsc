# Especificações e Requisitos de Engenharia — Datalogger Central ESP32-S3 (Protto UFSC)

> **Nota de Otimização de Contexto (IA):** A tabela descritiva de *Stakeholders* organizacionais e os requisitos puramente mecânicos/manufatura de PCB (bitola de cabo $0,30\ \text{mm}^2$, DRC Tecnel $0,50\ \text{mm}$, massa $< 1000\ \text{g}$) estão sintetizados na Seção 7 apenas para rastreabilidade do *FSAE Design Briefing*, mantendo este documento 100% focado nas especificações de arquitetura de rede CAN e código do firmware.

---

## 1. Arquitetura Distribuída de Aquisição (`RU 1.1`, `RU 2.1`, `RU 3.1`, `RU 3.2`)

O sistema de aquisição de dados (DAQ) da **Protto UFSC Motorsport** utiliza uma **Arquitetura Distribuída em Barramento CAN 2.0B (`500 kbps`)**, dividindo a carga computacional e reduzindo o comprimento de cabos analógicos sensíveis pelo chassi:

```text
+--------------------+   +--------------------+   +--------------------+   +--------------------+
| Nó Roda FL (C3)    |   | Nó Roda FR (C3)    |   | Nó Roda RL (C3)    |   | Nó Roda RR (C3)    |
| - Temp Disco Freio |   | - Temp Disco Freio |   | - Temp Disco Freio |   | - Temp Disco Freio |
| - Velocidade Roda  |   | - Velocidade Roda  |   | - Velocidade Roda  |   | - Velocidade Roda  |
| - Célula de Carga  |   | - Célula de Carga  |   | - Célula de Carga  |   | - Célula de Carga  |
| - Pot. Linear Susp |   | - Pot. Linear Susp |   | - Pot. Linear Susp |   | - Pot. Linear Susp |
+---------+----------+   +---------+----------+   +---------+----------+   +---------+----------+
          |                        |                        |                        |
==========+========================+========================+========================+===========
                                 BARRAMENTO CAN 2.0B (500 kbps)
==========+========================+========================+========================+===========
          |                        |                        |                        |
+---------+----------+   +---------+----------+   +---------+----------+   +---------v----------+
| Nó Freio/Dir (C3)  |   | Nó IMU WIT (C3)    |   | ECU do Motor       |   | DATALOGGER CENTRAL |
| - Ângulo Direção   |   | - Accel X, Y, Z    |   | - RPM, TPS         |   | (ESP32-S3-N8R2)    |
| - Pressão Freio D. |   | - Gyro X, Y, Z     |   | - Marcha Atual     |   | - Concentrador CAN |
| - Pressão Freio T. |   | - Ângulos/Calib.   |   | - Dados do Motor   |   | - 10x ADC1 Locais  |
| - Pos. Pedal Freio |   +--------------------+   +--------------------+   | - GPS Neo-8M 10 Hz |
+--------------------+                                                     | - RTC DS3231 (I2C) |
                                                                           | - MicroSD 64 GB    |
                                                                           | - UI (2 SW, 2 LED) |
                                                                           +--------------------+
```

### 1.1 Responsabilidades dos Nós Satélites (`ESP32-C3` via CAN) e ECU
1. **4x Nós de Roda (`ESP32-C3` — `FL`, `FR`, `RL`, `RR`):**
   - Leitura local em cada canto do carro de:
     - **Temperatura do Disco de Freio** (`RU 5.2`)
     - **Velocidade da Roda** (*Wheel Speed* — `RU 5.2`)
     - **Célula de Carga** (*Load Cell / Strain Gauge* $\ge 12\ \text{bits}$ — `RU 6.1`)
     - **Potenciômetro Linear de Suspensão** (Curso do amortecedor / LVDT — `RU 5.2`)
2. **1x Nó de Freio e Direção (`ESP32-C3`):**
   - Leitura dos comandos do piloto (`RU 5.1`):
     - **Ângulo de Direção** (*Steering Angle*)
     - **Pressão de Freio Dianteiro** (*Front Brake Pressure*)
     - **Pressão de Freio Traseiro** (*Rear Brake Pressure*)
     - **Posição do Pedal de Freio** (*Brake Pedal Position*)
3. **1x Nó de IMU (`ESP32-C3` + Sensor WIT):**
   - Lê o sensor inercial WIT, interpreta o protocolo serial, aplica calibrações/offsets locais e transmite no barramento CAN as acelerações lineares 3 eixos (`Ax, Ay, Az`) e velocidades angulares 3 eixos (`Gx, Gy, Gz`) (`RU 5.2`).
4. **ECU (Injeção Eletrônica):**
   - Transmite pacotes CAN com parâmetros do motor (`RPM`, `TPS`, temperatura do motor, tempo de injeção/ignição) e **Marcha Atual (`Gear`)** (`RU 4.1`, `RU 4.2`).

### 1.2 Responsabilidades do Datalogger Central (Este Firmware — `ESP32-S3-DEVKITC-1-N8R2`)
- **Concentrador e Decodificador CAN (`500 kbps`, `SN65HVD230DR`, `GPIO 15/16`):** Recebe, valida, decodifica e monitora *heartbeat/timeout* de todos os nós `ESP32-C3` e da ECU.
- **Aquisição Analógica Local (10 Canais `ADC1_CH0..9`, `GPIO 1..10`):**
  - 4 canais dedicados de motor (`Measurement_Engine.SchDoc`): `OilPressure`, `OilTemp`, `FuelPressure`, `Lambda` (`RU 4.1`, `RU 4.2`).
  - 4 canais `5V -> 3.3V` (`ADC5to3v3_1..4`) e 2 canais `12V -> 3.3V` (`ADC12to3v3_1..2`) disponíveis como entradas analógicas locais/reserva na central.
- **Sincronismo Temporal e Posicionamento Local:**
  - **RTC `DS3231SN#` (`GPIO 11/12`, I2C `0x68`)** + timer monotônico de `1 ms` (`esp_timer`) para alinhamento temporal ($\le 50\ \text{ms}$) entre pacotes CAN, ADC local e GPS.
  - **GPS `Neo-8M` (`GPIO 13/47`, UART @ `10 Hz`)** para mapa de pista, velocidade no solo (*ground speed*) e correlação aerodinâmica (`RU 5.3`, `RU 6.2`, `RU 7.1`).
- **Armazenamento e Interface de Pista:**
  - **Cartão MicroSD `J4` (`GPIO 14, 17, 18, 21, 40, 41`, até `64 GB`)** para gravação de stints de Endurance (`22 km`) em `.csv` compatível com **MoTeC i2 / AiM Race Studio / MegaLogViewer**.
  - **Botões (`Free_SW1`, `Free_SW2`) e LEDs (`LED_Acquiring`, `LED_Working`)** para operação autônoma no *paddock*/pista.

---

## 2. Estratégia de Aquisição via CAN (Empacotamento, Arbitragem e Orçamento de Banda)

Para garantir latência zero e aproveitamento máximo do barramento CAN a **500 kbps**:

### 2.1 Empacotamento Eficiente (`DLC = 8 bytes` por Nó)
Cada frame CAN 2.0 comporta até `8 bytes` (`64 bits`) de payload. Representando cada variável física como inteiro escalonado de **16 bits (`uint16_t` ou `int16_t`, Little-Endian)**, **cada nó satélite transmite todas as suas 4 grandezas em um único frame CAN de 8 bytes**:

| Origem no Barramento CAN | Sinais Empacotados no Frame (`4 x 16-bit = 8 bytes`) | Taxa de Envio Típica | Carga no Barramento (@ 500 kbps) |
| :--- | :--- | :---: | :---: |
| **Nó Freio / Direção (`C3`)** | `[0..1]` Ângulo Direção, `[2..3]` Pressão Freio Diant., `[4..5]` Pressão Freio Tras., `[6..7]` Pos. Pedal Freio | $100\ \text{Hz}$ | $\approx 2,6\%$ |
| **Nó Roda FL (`C3`)** | `[0..1]` Pot. Suspensão FL, `[2..3]` Vel. Roda FL, `[4..5]` Célula Carga FL, `[6..7]` Temp. Disco FL | $100\ \text{Hz}$ | $\approx 2,6\%$ |
| **Nó Roda FR (`C3`)** | `[0..1]` Pot. Suspensão FR, `[2..3]` Vel. Roda FR, `[4..5]` Célula Carga FR, `[6..7]` Temp. Disco FR | $100\ \text{Hz}$ | $\approx 2,6\%$ |
| **Nó Roda RL (`C3`)** | `[0..1]` Pot. Suspensão RL, `[2..3]` Vel. Roda RL, `[4..5]` Célula Carga RL, `[6..7]` Temp. Disco RL | $100\ \text{Hz}$ | $\approx 2,6\%$ |
| **Nó Roda RR (`C3`)** | `[0..1]` Pot. Suspensão RR, `[2..3]` Vel. Roda RR, `[4..5]` Célula Carga RR, `[6..7]` Temp. Disco RR | $100\ \text{Hz}$ | $\approx 2,6\%$ |
| **Nó IMU WIT — Frame A (`C3`)** | `[0..1]` Accel X, `[2..3]` Accel Y, `[4..5]` Accel Z, `[6..7]` Status/Temp IMU | $100\ \text{Hz}$ | $\approx 2,6\%$ |
| **Nó IMU WIT — Frame B (`C3`)** | `[0..1]` Gyro X (Roll rate), `[2..3]` Gyro Y (Pitch rate), `[4..5]` Gyro Z (Yaw rate), `[6..7]` Reservado | $100\ \text{Hz}$ | $\approx 2,6\%$ |
| **ECU (Motor & Marcha)** | `RPM`, `TPS`, `Marcha Atual (Gear)`, Temp. Motor, Tensão Bateria, etc. | $50 - 100\ \text{Hz}$ | $\approx 5 - 10\%$ |
| **Total Estimado na Rede CAN** | **7 frames de nós C3 + frames da ECU** | **$800 - 1100\ \text{frames/s}$** | **$\approx 23 - 28\%$ (Excelente, $< 50\%$)** |

> **Justificativa para o FSAE Design Judging (*Systems Management — 20 pts*):** Uma ocupação de barramento CAN entre $23\%$ e $28\%$ a $500\ \text{kbps}$ garante determinismo, zero perda por colisão de arbitragem e folga para telemetria futura (`RU 1.1`, `RU 3.2`).

### 2.2 Mapa Oficial de Protocolo CAN (Nós `ESP32-C3` + FuelTech FTCAN 2.0)

Para evitar qualquer colisão de ID com o protocolo **FTCAN 2.0** da **FuelTech (`FT450`/`FT550`/`FT600`)** (que utiliza identificadores **Extended 29-bit** na faixa `0x1408xxxx` / `0x1400xxxx` com `MessageID` nos 11 bits inferiores `0x600..0x602`), todos os **6 nós `ESP32-C3` da Protto UFSC** utilizam identificadores **Standard 11-bit (`0x100` a `0x301`, Little-Endian)**, enquanto os pacotes de Broadcast da **FuelTech** são decodificados em **Big-Endian (Motorola)**:

| ID CAN | Formato / Endianness | Nó Origem | Bytes `0..1` | Bytes `2..3` | Bytes `4..5` | Bytes `6..7` |
| :--- | :---: | :--- | :--- | :--- | :--- | :--- |
| **`0x100`** | 11-bit / Little-Endian | **Nó Freio & Direção (`C3`)** | Ângulo Direção (`int16`, $0,1^\circ$) | Pressão Freio Diant. (`uint16`, $0,1\ \text{bar}$) | Pressão Freio Tras. (`uint16`, $0,1\ \text{bar}$) | Posição Pedal Freio (`uint16`, $0,1\%$) |
| **`0x200`** | 11-bit / Little-Endian | **Nó Roda FL (`C3`)** | Pot. Suspensão FL (`uint16`, $0,1\ \text{mm}$) | Vel. Roda FL (`uint16`, $0,1\ \text{km/h}$) | Célula Carga FL (`int16`, $1\ \text{N}$) | Temp. Disco FL (`int16`, $0,1^\circ\text{C}$) |
| **`0x201`** | 11-bit / Little-Endian | **Nó Roda FR (`C3`)** | Pot. Suspensão FR (`uint16`, $0,1\ \text{mm}$) | Vel. Roda FR (`uint16`, $0,1\ \text{km/h}$) | Célula Carga FR (`int16`, $1\ \text{N}$) | Temp. Disco FR (`int16`, $0,1^\circ\text{C}$) |
| **`0x202`** | 11-bit / Little-Endian | **Nó Roda RL (`C3`)** | Pot. Suspensão RL (`uint16`, $0,1\ \text{mm}$) | Vel. Roda RL (`uint16`, $0,1\ \text{km/h}$) | Célula Carga RL (`int16`, $1\ \text{N}$) | Temp. Disco RL (`int16`, $0,1^\circ\text{C}$) |
| **`0x203`** | 11-bit / Little-Endian | **Nó Roda RR (`C3`)** | Pot. Suspensão RR (`uint16`, $0,1\ \text{mm}$) | Vel. Roda RR (`uint16`, $0,1\ \text{km/h}$) | Célula Carga RR (`int16`, $1\ \text{N}$) | Temp. Disco RR (`int16`, $0,1^\circ\text{C}$) |
| **`0x300`** | 11-bit / Little-Endian | **Nó IMU WIT — Accel (`C3`)** | Accel X (`int16`, $0,01\ \text{m/s}^2$) | Accel Y (`int16`, $0,01\ \text{m/s}^2$) | Accel Z (`int16`, $0,01\ \text{m/s}^2$) | Status IMU (`uint16`) |
| **`0x301`** | 11-bit / Little-Endian | **Nó IMU WIT — Gyro (`C3`)** | Gyro X (`int16`, $0,1^\circ/\text{s}$) | Gyro Y (`int16`, $0,1^\circ/\text{s}$) | Gyro Z (`int16`, $0,1^\circ/\text{s}$) | Reservado (`0`) |
| **`0x14080600`** *(ou `0x600`)* | 29-bit (ou 11-bit) / **Big-Endian** | **ECU FuelTech (Pacote 1)** | **TPS** (`int16`, $0,1\%$) | **MAP** (`int16`, $0,001\ \text{bar}$) | **Temp. Ar** (`int16`, $0,1^\circ\text{C}$) | **Temp. Motor** (`int16`, $0,1^\circ\text{C}$) |
| **`0x14080601`** *(ou `0x601`)* | 29-bit (ou 11-bit) / **Big-Endian** | **ECU FuelTech (Pacote 2)** | Pressão Óleo ECU (`int16`, $0,001\ \text{bar}$) | Pressão Comb. ECU (`int16`, $0,001\ \text{bar}$) | Pressão Água (`int16`, $0,001\ \text{bar}$) | **Marcha Atual / Gear** (`int16`, $1$) |
| **`0x14080602`** *(ou `0x602`)* | 29-bit (ou 11-bit) / **Big-Endian** | **ECU FuelTech (Pacote 3)** | Lambda ECU (`int16`, $0,001\ \lambda$) | **RPM** (`uint16`, $1\ \text{rpm}$) | Temp. Óleo ECU (`int16`, $0,1^\circ\text{C}$) | Pit Limit (`uint16`) |

### 2.3 Modelo de Agregação no Datalogger Central (*Vehicle State Table* + *Stale Detection*)
Como os frames CAN chegam de forma assíncrona dos vários `ESP32-C3` e da ECU:
1. **Task CAN RX (Core 1):** Recebe cada `CanFrame` do `CanService` / `CanTwaiLld` e atualiza imediatamente uma estrutura estática em memória (**`VehicleTelemetryState`** / *Blackboard Pattern*) contendo o último valor decodificado de cada sensor e o `last_update_ms` de cada nó.
2. **Detecção de Nó Offline (*Heartbeat / Stale Timeout*):** Se qualquer nó `ESP32-C3` ou a ECU parar de transmitir por mais de um *timeout* configurável (ex: $> 200\ \text{ms}$), o Datalogger Central marca a flag de diagnóstico daquele nó como `STALE/FAULT` sem travar a gravação dos demais sensores (`RU 2.1`).
3. **Alinhamento Temporal ($\le 50\ \text{ms}$):** Em intervalos fixos no Core 1, um *snapshot* unificado (combinando o `VehicleTelemetryState` da CAN + os 10 canais do `ADC1` local + última posição do `GPS Neo-8M`) é carimbado com `timestamp_ms` e enviado pelo `RingBuffer` / fila OSAL para o **Core 0** gravar no cartão MicroSD.

---

## 3. Parâmetros Quantitativos Gerais (Requisitos Finais)

| Parâmetro de Engenharia | Valor Alvo | Unidade | Atendimento na Arquitetura |
| :--- | :---: | :---: | :--- |
| **Velocidade da Rede CAN (ECU + 6 Nós ESP32-C3)** | $500$ | $\text{kbps}$ | `CanService` + `CanTwaiLld` (`GPIO 15/16`) no Datalogger Central ESP32-S3. |
| **Resolução de Timestamp (Alinhamento CAN + ADC + GPS)** | $\le 50$ *(Meta: $1$)* | $\text{ms}$ | Tick FreeRTOS `1000 Hz` (`1 ms`) + `esp_timer` sincronizado com RTC `DS3231SN#`. |
| **Canais Analógicos Locais na Central (0–5 V / 12 V → 3.3 V)** | $10$ | canais | `ADC1_CH0..9` (`GPIO 1..10`): `OilPressure`, `OilTemp`, `FuelPressure`, `Lambda`, `ADC5to3v3_1..4`, `ADC12to3v3_1..2`. |
| **Canais Remotos via Nós CAN (`ESP32-C3`)** | $26+$ | variáveis | 16 variáveis nas 4 rodas (4x4) + 4 no nó Freio/Direção + 6 no nó IMU WIT + variáveis da ECU (RPM, TPS, Marcha). |
| **Taxa de Amostragem (Critério de Nyquist)** | $10\times f_{\text{sinal}}$ | $\text{Hz}$ | Configurável por nó/canal + filtro digital passa-baixa em software (`RU 1.2`). |
| **Taxa de Atualização do GPS (`Neo-8M` na Central)** | $10$ | $\text{Hz}$ | UART (`GPIO 13/47`) com atualização a cada `100 ms` (`RU 5.3`, `RU 6.2`, `RU 7.1`). |
| **Capacidade de Armazenamento Local (Endurance 22 km)** | Até $64$ ($\ge 1$ stint) | $\text{GB}$ | Cartão MicroSD (`J4`) gerenciado pelo **Core 0** com `fsync` periódico. |

---

## 4. Requisitos Funcionais do Firmware Central (RF)

- **RF-01 (Aquisição e Decodificação da Rede CAN Distribuída — `RU 1.1`, `RU 3.1`, `RU 4.2`, `RU 5.1`, `RU 5.2`, `RU 6.1`):**
  - O módulo `CanDatabaseService` / `CanDecoder` (`BSW`) deve decodificar os frames vindos:
    1. Dos **4 Nós de Roda (`ESP32-C3`)**: Temp. disco de freio, velocidade da roda, célula de carga e potenciômetro linear.
    2. Do **Nó de Freio/Direção (`ESP32-C3`)**: Ângulo de direção, pressão de freio dianteiro/traseiro e posição do pedal de freio.
    3. Do **Nó de IMU WIT (`ESP32-C3`)**: Acelerações e velocidades angulares já calibradas.
    4. Da **ECU**: RPM, TPS, **Marcha Atual (`Gear`)** e temperaturas/status do motor.
  - Deve rastrear o timestamp da última mensagem de cada nó para detectar desconexão (*timeout*).
- **RF-02 (Aquisição Analógica Local de 10 Canais — `RU 1.2`, `RU 4.1`, `RU 4.2`):**
  - O `AdcService` (`BSW`) deve ler os 10 canais do `ADC1` local (`OilPressure`, `OilTemp`, `FuelPressure`, `Lambda`, `ADC5to3v3_1..4`, `ADC12to3v3_1..2`), aplicar filtro digital passa-baixa e converter para unidade de engenharia.
- **RF-03 (Sincronização Temporal Unificada — `RU 5.3`, Requisito Final `50 ms`):**
  - O `RtcService` deve obter a data/hora real do `DS3231SN#` via I2C (`GPIO 11/12`) e alinhar todos os pacotes CAN, leituras de ADC local e coordenadas GPS sob uma base de tempo única com resolução de `1 ms` ($\le 50\ \text{ms}$).
- **RF-04 (Posicionamento e Velocidade GPS a 10 Hz — `RU 5.3`, `RU 6.2`, `RU 7.1`):**
  - O `GpsService` deve processar os pacotes do `Neo-8M` (`GPIO 13/47`) a `10 Hz` sem alocação dinâmica, fornecendo latitude, longitude e velocidade no solo para correlacionar com as células de carga (Aero) e inputs do piloto.
- **RF-05 (Agregação em `RingBuffer` e Exportação `.csv` — `RU 2.2`, `RU 4.2`, `RU 5.3`):**
  - O `LoggerService` (no **Core 0**) deve consumir os snapshots do `RingBuffer` / fila OSAL e gravar no MicroSD (`J4`) arquivos `.csv` compatíveis com **MoTeC i2**, **AiM Race Studio** e **MegaLogViewer**, com `fsync` periódico.
- **RF-06 (Interface de Pista — Botões e LEDs — `RU 2.1`):**
  - `Free_SW1` (`GPIO 35`): Iniciar/parar gravação; `Free_SW2` (`GPIO 36`): Marcador de volta/evento (*Beacon*).
  - `LED_Working` (`GPIO 39`) e `LED_Acquiring` (`GPIO 37`): Indicação visual de status do sistema, gravação ativa e falhas de nó CAN/SD.

---

## 5. Requisitos Não-Funcionais (RNF)
- **RNF-01 (Divisão Dual-Core no ESP32-S3):**
  - **Core 1 (`APP_CPU`):** Recepção e decodificação CAN em tempo real, leitura periódica dos 10 ADCs locais e recepção UART do GPS.
  - **Core 0 (`PRO_CPU`):** Consumo do buffer, formatação CSV, escrita e `fsync` no MicroSD e atualização da UI.
- **RNF-02 (Zero Heap em Tempo Real):** Uso exclusivo de buffers estáticos, `std::array` e `bsw::RingBuffer<T, Capacity>`.
- **RNF-03 (Testabilidade SIL 100%):** Toda a decodificação CAN dos nós `ESP32-C3` e da ECU, filtros de ADC, parsers de GPS/RTC e formatação de log devem possuir testes unitários em `tests/` via Google Test com `0 warnings`.

---

## 6. Resumo de Rastreabilidade de Hardware / PCB (Referência FSAE Design Report)
- **`RU 1.2` (Filtro Anti-Aliasing Hardware):** Filtros passa-baixa ativos nos `MCP6022-E/SN` da placa central (1 década acima da frequência do canal).
- **`RU 1.4`, `RU 1.5`, `RU 7.2`, `RU 8.1`:** Chicote blindado $0,30\ \text{mm}^2$, proteção Buck/LDO/TVS `SM712`, massa $< 1000\ \text{g}$ e trilhas de alimentação $\ge 0,50\ \text{mm}$ (DRC Tecnel).
