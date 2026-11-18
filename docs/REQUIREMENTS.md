# Especificações e Requisitos do Projeto — Datalogger ESP32-S3

> **Status:** Estrutura base pronta para preenchimento das especificações detalhadas.

---

## 1. Visão Geral e Objetivo do Sistema
- **Aplicação:** Datalogger Automotivo / Motorsport (Protto UFSC).
- **Plataforma:** ESP32-S3 (`ESP32-S3-DEVKITC-1-N8R2`) com FreeRTOS Dual-Core.
- **Objetivo:** *(A definir: contexto de uso em pista/dinamômetro, autonomia, análise pós-sessão, etc.)*

---

## 2. Requisitos de Hardware e Interfaces (Base: Esquemático `Main.SchDoc`)

| Subsistema | CI / Conector no Esquemático | Pinos (`bsp_pins.h`) | Especificação / Parâmetros (A detalhar) |
| :--- | :--- | :--- | :--- |
| **CAN Bus (ECU / Rede)** | `U2` (`SN65HVD230DR`) + `U1` (`SM712`) | `GPIO 15` (TX), `GPIO 16` (RX) | 500 kbps (Standard 11-bit / Extended 29-bit) — *Definir IDs/DBC ou modo sniffer* |
| **ADC Motor (4 canais)** | `U8`, `U9` (`MCP6022-E/SN`) | `GPIO 1..4` (`ADC1_CH0..3`) | `OilPressure`, `OilTemp`, `FuelPressure`, `Lambda` — *Definir curvas de calibração e taxa (Hz)* |
| **ADC 5V → 3.3V (4 canais)** | `U5`, `U6` (`MCP6022-E/SN`) | `GPIO 5..8` (`ADC1_CH4..7`) | `ADC5to3v3_1..4` — *Definir sensores/uso e taxa (Hz)* |
| **ADC 12V → 3.3V (2 canais)** | `U7` (`MCP6022-E/SN`) | `GPIO 9..10` (`ADC1_CH8..9`) | `ADC12to3v3_1..2` — *Definir sensores/uso e taxa (Hz)* |
| **RTC Externo** | `B8` (`DS3231SN#`) | `GPIO 11` (SDA), `GPIO 12` (SCL) | I2C `0x68` @ 400 kHz — *Definir sincronização e formato de timestamp* |
| **GPS** | `J5` (`Neo-8M`) | `GPIO 13` (RX), `GPIO 47` (TX) | UART — *Definir baudrate (9600/38400/115200), taxa de atualização (Hz) e sentenças (NMEA/UBX)* |
| **Armazenamento MicroSD** | `J4` (`SocketMicroSD`) | `GPIO 14, 17, 18, 21, 40, 41` | *Definir modo (SPI vs SDMMC 4-bit), formato de arquivo (CSV/Binário) e rotação de logs* |
| **Interface de Usuário (UI)** | `SW1`, `SW2`, `D2`, `D3` | `GPIO 35, 36` (Botões) / `37, 39` (LEDs) | *Definir comportamento dos botões (iniciar/parar log, marcar volta/evento) e padrões de piscada dos LEDs* |

---

## 3. Requisitos Funcionais (RF)
*(A preencher na próxima etapa)*
- **RF-01 (Aquisição):** ...
- **RF-02 (Sincronização Temporal / Timestamp):** ...
- **RF-03 (Armazenamento e Integridade dos Logs):** ...
- **RF-04 (Máquina de Estados / Controle por Botões e LEDs):** ...
- **RF-05 (Comunicação CAN / Decodificação ou Raw Log):** ...

---

## 4. Requisitos Não-Funcionais (RNF)
*(A preencher na próxima etapa)*
- **RNF-01 (Taxas de Amostragem e Latência):** ...
- **RNF-02 (Confiabilidade / Perda de Energia):** ...
- **RNF-03 (Memória e Desempenho Dual-Core):** ...
- **RNF-04 (Testabilidade SIL):** Cobertura unitária via Google Test / Google Mock no Host com 0 warnings.
