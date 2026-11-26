# Plano e Suíte Completa de Testes em Hardware — Datalogger Central ESP32-S3 (Protto UFSC)

Este documento define o **procedimento oficial de comissionamento elétrico, validação funcional módulo a módulo (bancada) e aprovação integrada (pista — Fase 9)** do Datalogger Central (`ESP32-S3-DEVKITC-1-N8R2`).

Execute os blocos **na ordem numérica (`TC-00` $\rightarrow$ `TC-09`)**: nunca energize ou conecte periféricos externos sem concluir o `TC-00` (*Smoke Test*).

- **Pinning Table Oficial:** [`components/bsp/include/bsp_pins.h`](../components/bsp/include/bsp_pins.h)
- **Protocolo CAN (`ESP32-C3` + FuelTech):** [`docs/CAN_PROTOCOL.md`](CAN_PROTOCOL.md)
- **Protocolo GPS (`Neo-8M` 10 Hz):** [`docs/NEO8M_PROTOCOL.md`](NEO8M_PROTOCOL.md)
- **Formato Binário de Log (`PDLG v1`):** [`docs/BINARY_LOG_FORMAT.md`](BINARY_LOG_FORMAT.md)

---

## 1. Equipamentos e Bancada Recomendada

| Item | Especificação / Uso nos Testes | Obrigatório? |
| :--- | :--- | :---: |
| **Fonte de Bancada DC** | Ajustável `0..15 V` com limitação de corrente (`CC`), para alimentar a placa (`12 V`) e injetar tensões de teste nos canais ADC (`0..5 V` e `0..12 V`). | Sim |
| **Multímetro Digital** | Continuidade, resistência de terminação CAN (`120 Ω` / `60 Ω`) e conferência das tensões nos pinos do `ESP32-S3` (`<= 3,30 V`). | Sim |
| **PC com ESP-IDF v5.4+ e Python 3** | Gravação do firmware (`idf.py flash monitor`) e execução do script auditor de logs `.bin` (Seção 11). | Sim |
| **Cartão MicroSD** | Formatado em **FAT32** (até `64 GB`, *Allocation Unit* `16 KB` ou `32 KB` recomendado). | Sim |
| **Bateria `CR2032`** | `3,0 V` para backup do RTC `DS3231SN#` (`B8`). | Sim |
| **Módulo GPS `u-blox NEO-8M` + Antena** | Conectado em `J5` com antena ativa posicionada próxima a janela ou área aberta para *3D Fix*. | Sim |
| **Gerador CAN (`USB-CAN` ou 1x `ESP32-C3`)** | Configurado a **`500 kbps`** para injetar os IDs dos nós (`0x100`, `0x200..0x203`, `0x300..0x301`) e da ECU FuelTech (`0x14080600..0x14080602`). | Sim |
| **Osciloscópio / Analisador Lógico** | Opcional para inspecionar formas de onda `I2C 400 kHz`, `UART 115200`, `CAN_H/CAN_L` e frequências dos LEDs (`1 Hz` / `2,5 Hz` / `5 Hz`). | Opcional |

---

## 2. Matriz de Rastreabilidade da Suíte de Testes

| Bloco | Escopo Validado | Fases | Requisitos Cobertos |
| :---: | :--- | :---: | :--- |
| **`TC-00`** | Inspeção Elétrica Pré-Energização (*Smoke Test*), Regulação e Proteção `<= 3,3 V` | Fase 0 / PCB | `bsp_pins.h`, `RU 1.4`, `RU 1.5` |
| **`TC-01`** | Gravação do Firmware (`Flash`), Boot Dual-Core e Logs de Inicialização | Fase 0 / ASW | `RNF-01`, `RNF-02` |
| **`TC-02`** | Interface de Pista: Botões (`Free_SW1` Standby, `Free_SW2` Beacon) e LEDs (`D2`/`D3`) | Fase 8 | `RF-06`, `RU 2.1` |
| **`TC-03`** | RTC `DS3231SN#` (`I2C 400 kHz`), Retenção `CR2032` e Fallback Degradado `1 ms` | Fase 5 | `RF-03`, `RU 5.3` ($\le 50\ \text{ms}$) |
| **`TC-04`** | Aquisição Analógica Local: `10` Canais `ADC1` (`Motor`, `5V->3V3`, `12V->3V3`) + Filtro EMA | Fase 4 | `RF-02`, `RU 1.2`, `RU 4.1`, `RU 4.2` |
| **`TC-05`** | GPS `Neo-8M`: Negociação UBX (`9600 -> 115200 baud`), `10 Hz`, *Fix* GNSS e Hot-Unplug | Fase 6 | `RF-04`, `RU 5.3`, `RU 6.2`, `RU 7.1` |
| **`TC-06`** | Rede CAN `500 kbps`: `6` Nós `ESP32-C3`, ECU FuelTech, *Node Timeout* e *Bus-Off Recovery* | Fases 1–3 | `RF-01`, `RU 1.1`, `RU 3.1`, `RU 5.1..6.1` |
| **`TC-07`** | Máquina de Estados por RPM (`> 600 RPM`, *Hold* `5 s`) e Logger MicroSD `PDLG v1` + Corte de Energia | Fases 7–8 | `RF-05`, `RF-06`, `RU 2.2` |
| **`TC-08`** | Auditoria Automatizada do Arquivo `.bin` (`CRC-32`, Taxa `100 Hz`, `0 Drops`) e Exportação CSV | Fases 7 / 9 | `RF-05`, Integridade de Dados |
| **`TC-09`** | *Burn-in* Integrado (`30 min` Contínuos) e *Shakedown* no Veículo | Fase 9 | Endurance `22 km` (`64 GB`) |

---

## 3. Bloco `TC-00` — Inspeção Elétrica Pré-Energização (*Smoke Test*)

> **ATENÇÃO:** Execute `TC-00.1` a `TC-00.4` **antes** de aplicar sinais externos nos conectores `J1`/`J3` ou conectar a rede CAN do carro. Os GPIOs do `ESP32-S3` suportam no máximo **`3,6 V` absolutos (`3,3 V` nominais)**.

| ID | Teste | Procedimento Passo a Passo | Critério de Aprovação (*Pass*) |
| :---: | :--- | :--- | :--- |
| **`TC-00.1`** | Ausência de curto-circuito nas linhas de alimentação | Com a placa **desenergizada** e sem o módulo `ESP32-S3` (caso esteja em barra de pinos/soquete), meça resistência entre: `+12V` $\leftrightarrow$ `GND`, `+5V` $\leftrightarrow$ `GND` e `+3V3` $\leftrightarrow$ `GND`. | Nenhuma malha apresenta curto (`R > 1 kΩ`, subindo conforme carga dos capacitores). |
| **`TC-00.2`** | Terminação física do barramento CAN (`U2 SN65HVD230DR` + `SM712`) | Com a placa desligada e desconectada do chicote, meça resistência entre `CAN_H` e `CAN_L`. Repita com o chicote completo conectado (2 resistores de ponta). | Placa isolada (c/ jumper/resistor local): **`120 Ω ± 5%`** (ou aberto se nó intermediário). Chicote completo: **`60 Ω ± 5%`**. |
| **`TC-00.3`** | Regulação DC e consumo em vazio | Ajuste a fonte de bancada para `12,0 V` com limite de corrente em **`150 mA`** (sem `ESP32-S3`) ou **`300 mA`** (com `ESP32-S3`). Energize e meça as malhas `+5V`, `+3V3` e bateria `CR2032` (`B8`). | Sem aquecimento anormal; `+5V = 4,85..5,15 V`; `+3V3 = 3,20..3,40 V`; `VBAT (DS3231) >= 2,80 V`. |
| **`TC-00.4`** | Proteção de fundo de escala do condicionamento analógico (`MCP6022-E/SN` `U5..U9`) | *(Preferencialmente antes de encaixar o `ESP32-S3`)* Aplique `5,00 V` nas 4 entradas `5to3v3_1..4` e `12,00 V` (e `14,4 V` de alternador) nas 2 entradas `12to3v3_1..2`. Meça a tensão de saída nos pinos correspondentes a `GPIO 5..10`. | Tensão entregue aos pinos `GPIO 1..10` permanece estritamente no intervalo **`0,00 V .. 3,30 V`** (jamais ultrapassa `3,35 V`). |

---

## 4. Bloco `TC-01` — Gravação (Flash) e Boot Dual-Core (`Fase 0` / `ASW`)

### Preparação e Comando de Flash
Insira um cartão MicroSD (`FAT32`) em `J4`, conecte o `ESP32-S3` via USB ao PC e execute no terminal **ESP-IDF PowerShell**:
```powershell
idf.py set-target esp32s3
idf.py -p COMx flash monitor
```

| ID | Teste | Procedimento Passo a Passo | Critério de Aprovação (*Pass*) |
| :---: | :--- | :--- | :--- |
| **`TC-01.1`** | Gravação e boot limpo sem *Reset Loop* | Observe o boot completo no `idf.py monitor` por `30 segundos`. | Nenhum `Guru Meditation Error`, `Task watchdog got triggered` ou `Brownout detector was triggered`. |
| **`TC-01.2`** | Inicialização de todos os drivers `LLD` e serviços `BSW` | Verifique as mensagens `ESP_LOGI` emitidas por `ASW_MAIN`, `ADC_ONESHOT_LLD` e `SD_CARD_LLD` no console serial. | Aparecem: `ADC1 configurado com sucesso para 10 canais locais`, `RTC DS3231SN# inicializado e sincronizado`, `GPS Neo-8M inicializado (UART 115200 baud, 10 Hz)` e `MicroSD montado; sistema em Standby armado aguardando RPM > 600`. |
| **`TC-01.3`** | Estado visual inicial (*Standby Armado*) | Com tudo conectado e sem tráfego CAN de motor (`RPM = 0`), observe os LEDs `D2` (`LED_Acquiring` / `GPIO 37`) e `D3` (`LED_Working` / `GPIO 39`). | `LED_Acquiring` **apagado (`0 V`)**; `LED_Working` piscando regularmente a **`1 Hz`** (`500 ms` aceso / `500 ms` apagado). |

---

## 5. Bloco `TC-02` — Interface de Pista: Botões e Diagnóstico por LEDs (`Fase 8`)

Referência de pinos (`bsp_pins.h`):
- **`Free_SW1` (`GPIO 35` / `J3_13`):** Ativo-baixo (`GND`), pull-up `4,7 kΩ` + filtro RC (`100 Ω + 100 nF`). Função atual: **Standby / Reservado**.
- **`Free_SW2` (`GPIO 36` / `J3_12`):** Ativo-baixo (`GND`), pull-up `4,7 kΩ` + filtro RC (`100 Ω + 100 nF`). Função atual: **Lap Beacon** (*debounce* digital de `50 ms`).
- **`LED_Acquiring` (`GPIO 37` / `D2`):** Ativo-alto (`1 kΩ` p/ `GND`).
- **`LED_Working` (`GPIO 39` / `D3`):** Ativo-alto (`1 kΩ` p/ `GND`).

| ID | Teste | Procedimento Passo a Passo | Critério de Aprovação (*Pass*) |
| :---: | :--- | :--- | :--- |
| **`TC-02.1`** | Níveis elétricos dos botões (`SW1` e `SW2`) | Meça a tensão nos pinos `GPIO 35` e `GPIO 36` com os botões soltos e pressionados. | Solto: **`~3,3 V`**; Pressionado: **`< 0,2 V`** (`0 V`). |
| **`TC-02.2`** | Isolamento funcional de `Free_SW1` (Modo Standby) | 1. Em *Standby* (`RPM = 0`), pressione `Free_SW1` 5 vezes.<br>2. Durante uma gravação ativa (`RPM = 1500`), pressione e segure `Free_SW1` por `3 s`. | Em (1), a gravação **não inicia** (`LED_Acquiring` segue apagado). Em (2), a gravação **não para** e `lap_beacon` permanece inalterado no `.bin`. |
| **`TC-02.3`** | Marcação de voltas (*Lap Beacon*) e *Debounce* em `Free_SW2` | 1. Inicie uma sessão (`RPM = 1500`).<br>2. Aguarde `2 s` e pressione `Free_SW2` **3 vezes** pausadamente (intervalo `~2 s` entre cliques).<br>3. Encerre a sessão (`RPM = 0` por `6 s`) e inspecione o `.bin` com o script da Seção 11. | O campo `lap_beacon` inicia em **`0`**, sobe exatamente **`0` $\rightarrow$ `1` $\rightarrow$ `2` $\rightarrow$ `3`** (sem saltos duplos de *bounce*) e reinicia em **`0`** quando uma nova sessão é aberta. |
| **`TC-02.4`** | Hierarquia de diagnóstico dos LEDs (`1 Hz` < `2,5 Hz` < `5 Hz`) | Verifique visualmente (ou no osciloscópio em `GPIO 37` e `GPIO 39`) os 4 cenários:<br>A) Standby com SD OK e CAN OK;<br>B) Gravação ativa (`RPM > 600`) com SD OK;<br>C) Nó CAN desconectado (`> 200 ms`) com SD OK;<br>D) Boot sem cartão MicroSD inserido (mesmo com nó CAN desconectado). | A) `D2 = OFF`, `D3 = 1 Hz` (`500 ms` semi-período);<br>B) `D2 = ON contínuo`, `D3 = 1 Hz`;<br>C) `D3 = 2,5 Hz` (`200 ms` semi-período);<br>D) `D2 = OFF`, `D3 = 5 Hz` (`100 ms` semi-período — prioridade máxima de falha de SD). |

---

## 6. Bloco `TC-03` — RTC `DS3231SN#` (I2C) e Sincronização Temporal (`Fase 5`)

Referência (`bsp_pins.h`): `GPIO 11` (`SDA_I2C_1`, pull-up `4,7 kΩ`), `GPIO 12` (`SCL_I2C_1`, pull-up `4,7 kΩ`), clock `400 kHz`, endereço `0x68`.

| ID | Teste | Procedimento Passo a Passo | Critério de Aprovação (*Pass*) |
| :---: | :--- | :--- | :--- |
| **`TC-03.1`** | Integridade do barramento I2C (`400 kHz`) | Verifique no boot a mensagem `RTC DS3231SN# inicializado e sincronizado (base 1 ms)`. *(Se tiver osciloscópio, confirme `SCL = 400 kHz` com bordas rápidas em `GPIO 12`).* | Inicialização sem erro I2C (`status = kSynchronized`). |
| **`TC-03.2`** | Carimbo UTC de arquivo e flags de sincronismo | Dispare uma gravação curta (`RPM = 1500` por `5 s`, depois `0` por `6 s`) e leia o MicroSD no PC. | O arquivo chama-se **`LOG_YYYYMMDD_HHMMSS.bin`**; no cabeçalho `header_flags & 0x01 == 1` e nos registros `status_flags & 0x01 == 1`. |
| **`TC-03.3`** | Retenção de data/hora por bateria `CR2032` | Anote o horário do arquivo anterior. Desligue totalmente a alimentação `12 V` e o cabo USB por **`2 minutos`**. Religue e grave uma nova sessão. | O novo arquivo `LOG_YYYYMMDD_HHMMSS.bin` reflete exatamente `+2 min` em relação ao anterior (sem zerar para `2000-01-01`). |
| **`TC-03.4`** | *Fallback* Degradado Monotônico (Tolerância a Falha RTC) | Com a placa desligada, remova a bateria `CR2032` (provocando `OSF = 1` no `DS3231`) e religue sem bateria. Dispare uma gravação de `10 s`. | O firmware **não trava**: exibe `ESP_LOGW` de modo degradado, grava **`LOG_BOOT_XXXXXXXXXXms.bin`**, marca `status_flags & 0x01 == 0` e mantém `Δmonotonic_ms = 10 ms ± 1 ms` entre amostras (`<= 50 ms`). |

---

## 7. Bloco `TC-04` — Aquisição Analógica Local: 10 Canais `ADC1` (`Fase 4`)

### Mapa de Pinos e Canais para Bancada (`bsp_pins.h`)
> **Nota de Calibração Padrão:** Por padrão no `AdcService`, `slope = 0.001f` e `offset = 0.0f` (`physical_value` em **Volts no pino do ESP32-S3**, faixa `0,000 .. 3,300 V`, filtro EMA `alpha = 0.25`). Ao ajustar `set_channel_calibration()` para sensores específicos (bar, °C, λ), valide a grandeza correspondente.

| Índice `.bin` | Tag no Header (`adc_channel_tags`) | Pino ESP32-S3 (`ADC1`) | Pino Conector / Amp-Op | Faixa na Entrada da Placa |
| :---: | :--- | :---: | :---: | :---: |
| `0` | `OilPressure` | `GPIO 1` (`ADC1_CH0`) | `J3_4` / `U8A` (`OP1_OUTA`) | Condicionamento Motor $\rightarrow$ `0..3,3 V` no MCU |
| `1` | `OilTemp` | `GPIO 2` (`ADC1_CH1`) | `J3_5` / `U8B` (`OP1_OUTB`) | Condicionamento Motor $\rightarrow$ `0..3,3 V` no MCU |
| `2` | `FuelPressure` | `GPIO 3` (`ADC1_CH2`) | `J1_13` / `U9A` (`OP2_OUTA`) | Condicionamento Motor $\rightarrow$ `0..3,3 V` no MCU |
| `3` | `Lambda` | `GPIO 4` (`ADC1_CH3`) | `J1_4` / `U9B` (`OP2_OUTB`) | Condicionamento Motor $\rightarrow$ `0..3,3 V` no MCU |
| `4` | `ADC5to3v3_1` | `GPIO 5` (`ADC1_CH4`) | `J1_5` / `U5A` (`OP3_OUTA`) | **`0,00 .. 5,00 V`** $\rightarrow$ `0,00 .. 3,30 V` |
| `5` | `ADC5to3v3_2` | `GPIO 6` (`ADC1_CH5`) | `J1_6` / `U5B` (`OP3_OUTB`) | **`0,00 .. 5,00 V`** $\rightarrow$ `0,00 .. 3,30 V` |
| `6` | `ADC5to3v3_3` | `GPIO 7` (`ADC1_CH6`) | `J1_7` / `U6A` (`OP4_OUTA`) | **`0,00 .. 5,00 V`** $\rightarrow$ `0,00 .. 3,30 V` |
| `7` | `ADC5to3v3_4` | `GPIO 8` (`ADC1_CH7`) | `J1_12` / `U6B` (`OP4_OUTB`) | **`0,00 .. 5,00 V`** $\rightarrow$ `0,00 .. 3,30 V` |
| `8` | `ADC12to3v3_1` | `GPIO 9` (`ADC1_CH8`) | `J1_15` / `U7A` (`OP5_OUTA`) | **`0,00 .. 12,00 V`** $\rightarrow$ `0,00 .. 3,30 V` |
| `9` | `ADC12to3v3_2` | `GPIO 10` (`ADC1_CH9`) | `J1_16` / `U7B` (`OP5_OUTB`) | **`0,00 .. 12,00 V`** $\rightarrow$ `0,00 .. 3,30 V` |

| ID | Teste | Procedimento Passo a Passo | Critério de Aprovação (*Pass*) |
| :---: | :--- | :--- | :--- |
| **`TC-04.1`** | Validade dos 10 canais e linearidade em 3 pontos (`0%`, `50%`, `100%`) | Com gravação ativa (`RPM = 1500`), aplique em cada canal a tensão de **`0%`** (`GND`), **`50%`** (`2,50 V` nos canais 5V; `6,00 V` nos canais 12V) e **`100%`** (`5,00 V` / `12,00 V`), mantendo `2 s` em cada patamar. | No `.bin`: `adc_valid_mask == 0x03FF` (todos os 10 bits ativos) e os 3 patamares são proporcionais com erro **`<= ±3%`** em relação à leitura do multímetro no pino do MCU. |
| **`TC-04.2`** | Isolamento entre canais adjacentes (*Crosstalk*) e Filtro EMA | Aterre os canais pares (`CH0, 2, 4, 6, 8` em `0 V`) e aplique fundo de escala nos canais ímpares (`CH1, 3, 5, 7, 9`). Depois faça um degrau rápido `0 V -> 5 V` em `ADC5to3v3_1`. | Os canais aterrados permanecem `< 0,05 V` (sem *crosstalk*) e o degrau converge suavemente em `~10–15` amostras (`100–150 ms`) sem *overshoot*. |

---

## 8. Bloco `TC-05` — GPS `u-blox NEO-8M` a `10 Hz` (`Fase 6`)

Referência (`bsp_pins.h` & `docs/NEO8M_PROTOCOL.md`): Conector `J5` — `GPIO 13` (`ESP RX` $\leftarrow$ `GPS TXD`) e `GPIO 47` (`ESP TX` $\rightarrow$ `GPS RXD`).

| ID | Teste | Procedimento Passo a Passo | Critério de Aprovação (*Pass*) |
| :---: | :--- | :--- | :--- |
| **`TC-05.1`** | Auto-negociação UBX (`9600 -> 115200 baud`, `10 Hz`, `Automotive`) | Conecte um `Neo-8M` em estado de fábrica (`9600 baud`, `1 Hz`) em `J5` e ligue a placa. Repita dando apenas *reset* no ESP32-S3 sem desligar o GPS (*warm reset* com GPS já em `115200`). | Em ambos os casos (frio e quente), o receptor responde em `115200 baud` enviando apenas `$xxGGA` + `$xxRMC` a `10 Hz` (verificável no pino `GPIO 13` ou nos dados do `.bin`). |
| **`TC-05.2`** | Operação segura sem *Fix* satelital (Ambiente Indoor) | Com antena desconectada ou em bancada fechada, grave uma sessão de `10 s`. | Gravação ocorre a `100 Hz` sem atrasos; `status_flags & 0x02 == 0` (`gps_has_fix = false`) e `gps_speed_kmh == 0.0`. |
| **`TC-05.3`** | Aquisição com *3D Fix* a `10 Hz` e teste de deslocamento | Posicione a antena a céu aberto até obter *fix* (LED PPS do GPS piscando). Grave `30 s` caminhando ou deslocando o conjunto. | `status_flags & 0x02 == 1`, `gps_satellites >= 4`, `gps_latitude_deg` e `gps_longitude_deg` batem com a posição real no mapa (precisão `< 5 m`) e atualizam a **`10 Hz`** (1 nova época a cada `~10` registros de `100 Hz`). |
| **`TC-05.4`** | Desconexão a quente (*Hot-Unplug* do conector `J5`) | Durante uma gravação ativa, desconecte o cabo `J5` do GPS por `5 s` e reconecte. | Core 1 e Core 0 continuam gravando CAN + ADC a `100 Hz` sem perder amostras (`dropped_samples == 0`). |

---

## 9. Bloco `TC-06` — Rede CAN `500 kbps`: Nós `ESP32-C3`, ECU FuelTech e *Bus-Off* (`Fases 1–3`)

### Vetores Hexadecimais Prontos para Injeção em Bancada (`500 kbps`, `DLC = 8`)
Configure seu adaptador `USB-CAN` (ou um `ESP32-C3` de teste) a **`500 kbps`** e transmita os frames abaixo a `50–100 Hz`:

| Nó Simulado | CAN ID | Formato / Ordem | Payload Hex (`8 Bytes`) | Valores Decodificados Esperados no `.bin` |
| :--- | :---: | :---: | :--- | :--- |
| **Freio & Direção** | `0x100` | `11-bit` / LE | `6A FF 5E 01 C8 00 EE 02` | `steering = -15.0°`, `brake_f = 35.0 bar`, `brake_r = 20.0 bar`, `pedal = 75.0%` |
| **Roda FL / FR / RL / RR** | `0x200..0x203` | `11-bit` / LE | `FA 00 58 02 52 03 F0 0A` | `damper = 25.0 mm`, `wheel_spd = 60.0 km/h`, `load = 850 N`, `disc_temp = 280.0 °C` |
| **IMU WIT (Accel)** | `0x300` | `11-bit` / LE | `F4 01 18 FC D5 03 01 00` | `Ax = +5.00 m/s²`, `Ay = -10.00 m/s²`, `Az = +9.81 m/s²`, `imu_status = 1` |
| **IMU WIT (Gyro)** | `0x301` | `11-bit` / LE | `7D 00 CE FF 2C 01 00 00` | `Gx = +12.5 °/s`, `Gy = -5.0 °/s`, `Gz = +30.0 °/s` |
| **ECU FuelTech (Msg 1)** | `0x14080600` | `29-bit` / **BE** | `01 F4 03 E8 01 40 03 75` | `TPS = 50.0%`, `MAP = 1.000 bar`, `AirT = 32.0 °C`, `EngT = 88.5 °C` |
| **ECU FuelTech (Msg 2)** | `0x14080601` | `29-bit` / **BE** | `10 68 0D AC 04 B0 00 03` | `OilP = 4.200 bar`, `FuelP = 3.500 bar`, `WatP = 1.200 bar`, `Gear = 3` |
| **ECU FuelTech (Msg 3)** | `0x14080602` | `29-bit` / **BE** | `03 70 0D AC 03 B6 00 00` | `Lambda = 0.880`, **`RPM = 3500`**, `OilT = 95.0 °C`, `PitLimit = 0` |

| ID | Teste | Procedimento Passo a Passo | Critério de Aprovação (*Pass*) |
| :---: | :--- | :--- | :--- |
| **`TC-06.1`** | Decodificação exata dos 6 Nós `ESP32-C3` (`Little-Endian`) e `ECU FuelTech` (`Big-Endian`) | Transmita simultaneamente todos os 10 IDs da tabela acima a `100 Hz` por `10 s` (o pacote `0x14080602` com `RPM = 3500` iniciará a gravação automaticamente). Pare o envio, aguarde `6 s` e audite o `.bin`. | `can_node_online_mask == 0x7F` (todos os 7 nós online: bits `0..6 = 1`) e **100% das grandezas** (incluindo valores negativos `-15.0°`, `-10.00 m/s²`, `-5.0 °/s` e `Gear = -1` com `FF FF`) batem com a tabela sem erro de escala ou endianness. |
| **`TC-06.2`** | Compatibilidade `11-bit` vs `29-bit` da FuelTech | Repita enviando os blocos da ECU com IDs Standard `11-bit` (`0x600`, `0x601`, `0x602`) em vez de `0x14080600..0x14080602`. | Decodificação idêntica (`can_node_online_mask` bit `6 = 1`). |
| **`TC-06.3`** | Detecção de Nó Desconectado (*Stale Timeout* `> 200 ms`) | Com todos os nós transmitindo, interrompa apenas o envio do nó `0x200` (`Roda FL`). Após `3 s`, volte a transmitir `0x200`. | Em `< 250 ms` após parar `0x200`, o `LED_Working` acelera de `1 Hz` para **`2,5 Hz`** e o bit `1` (`0x02`) de `can_node_online_mask` cai para `0` no `.bin`. Ao religar `0x200`, o `LED_Working` retorna a **`1 Hz`** e o bit volta a `1`. |
| **`TC-06.4`** | Auto-Recuperação Completa de *Bus-Off* (`Bus-Off -> Stopped -> Start`) | Com tráfego CAN ativo e gravação rodando, provoque um curto rápido (`~0,5 s`) entre `CAN_H` e `CAN_L` com um jumper (forçando o controlador TWAI ao estado *Bus-Off*) e remova o curto. | O ESP32-S3 **não sofre Watchdog Reset**, executa o ciclo de recuperação automática e volta a receber frames CAN normalmente em `< 1 s` após remover o curto. |

---

## 10. Bloco `TC-07` — Máquina de Estados por RPM (`> 600 RPM` + *Hold* `5 s`) e MicroSD (`Fases 7–8`)

Payloads de teste para `0x14080602` (`29-bit`, Big-Endian):
- **`RPM = 600` (abaixo/igual ao limiar):** `03 70 02 58 03 B6 00 00`
- **`RPM = 601` (acima do limiar):** `03 70 02 59 03 B6 00 00`
- **`RPM = 400` (queda de giro):** `03 70 01 90 03 B6 00 00`

| ID | Teste | Procedimento Passo a Passo | Critério de Aprovação (*Pass*) |
| :---: | :--- | :--- | :--- |
| **`TC-07.1`** | Limiar exato de disparo (`600 RPM` vs `601 RPM`) | 1. Transmita `0x14080602` com **`RPM = 600`** por `10 s`.<br>2. Altere para **`RPM = 601`**. | Em `600 RPM`: sistema permanece em Standby (`LED_Acquiring = OFF`, nenhum arquivo criado). Ao passar para `601 RPM`: inicia imediatamente (`LED_Acquiring = ON` e log `Sessao MicroSD iniciada`). |
| **`TC-07.2`** | Janela de retenção (*Stop Hold* de `5 s`) contra fragmentação | Com sessão ativa (`RPM = 1500`), reduza para **`RPM = 400`** durante **`3,0 s`** e suba novamente para **`RPM = 1500`** por mais `5 s`. | O `LED_Acquiring` **não apaga** e o firmware mantém **um único arquivo `.bin` contínuo** (sem fechar nem criar segundo arquivo). |
| **`TC-07.3`** | Fechamento automático por motor parado (`5 s`) **e** por chave geral da ECU desligada | 1. Com sessão ativa, reduza para `RPM = 0` por `6 s` $\rightarrow$ confirme fechamento.<br>2. Abra nova sessão com `RPM = 3500` e **desconecte o cabo CAN da ECU** abruptamente (sem enviar `RPM = 0`). Cronometre o tempo até `LED_Acquiring` apagar. | Em (1), fecha em **`5,0 s`**. Em (2), mesmo com último RPM `3500`, a perda de presença da ECU (`200 ms`) + *hold* (`5,0 s`) fecha e sincroniza o `.bin` em **`~5,2 s`**, exibindo `Sessao MicroSD encerrada`. |
| **`TC-07.4`** | Resiliência a Corte Brusco de Energia (*Master Switch Kill Test*) | Com sessão ativa (`RPM = 3500`), aguarde **`20 segundos`** de gravação (`~2000` amostras) e **desligue a fonte de `12 V` de forma abrupta**. Leve o MicroSD ao PC e rode o script da Seção 11. | Graças ao bloco de `4 KB` e `fsync` a cada `50` amostras (`0,5 s`), o arquivo existe na tabela FAT32, possui cabeçalho íntegro e recupera **`>= 1950` registros com `CRC-32` válido** (`< 0,5 s` perdidos no corte). |
| **`TC-07.5`** | Proteção e *Cooldown* em falha de MicroSD | Ligue a placa **sem cartão MicroSD** e injete `RPM = 1500`. | `LED_Working` pisca a **`5 Hz`**, `LED_Acquiring` permanece **`OFF`**, e o Core 0 respeita o *cooldown* de `2000 ms` entre tentativas sem travar o console ou o Core 1. |
| **`TC-07.6`** | Horímetro permanente de motor na NVS (`Engine Run Time Total` — `HH:MM`) | 1. Verifique no boot o log `Horimetro de motor (NVS): HH:MM (X s acumulados)`.<br>2. Injete `RPM = 1500` por **`75 segundos`** (`1 min 15 s`), corte o `RPM` para `0`, aguarde `6 s` e desligue totalmente a placa (`12 V` + USB).<br>3. Religue a placa, abra uma nova sessão curta e rode o auditor Python da Seção 11. | O horímetro avança `+75 s` (`+00:01` em `HH:MM`), sobrevive ao desligamento total na partição `nvs` (checkpoint de `1 min` + commit na parada) e aparece gravado no campo `engine_hours_total_s` do cabeçalho `.bin`. |

---

## 11. Bloco `TC-08` — Auditoria Automatizada dos Arquivos `.bin` (`PDLG v1`) no PC

Salve ou execute o comando Python abaixo no PC apontando para qualquer arquivo `/sdcard/LOG_*.bin` gravado nos testes anteriores. Ele usa exclusivamente a biblioteca padrão do Python (`struct` e `zlib`) e audita cabeçalho, `CRC-32` de cada quadro, taxa de `100 Hz`, perdas de pacote e valores físicos:

```python
import struct, sys, zlib
from pathlib import Path

HDR_FMT = "<4sHHHHHBBQQ160s32sI24sI"
REC_FMT = "<HBBIQQHHBBHdd4f10f4f16f6f9fHhHHI"

def audit_pdlg(path_str: str) -> None:
    data = Path(path_str).read_bytes()
    assert len(data) >= 256, "FAIL: Arquivo menor que 256 bytes!"

    hdr = struct.unpack(HDR_FMT, data[:256])
    magic, ver, hdr_sz, rec_sz, hdr_flags, rate_hz, adc_n, can_n, mono0, epoch0, tags_raw, dev_raw, eng_sec, _, hdr_crc = hdr
    calc_hdr_crc = zlib.crc32(data[:252]) & 0xFFFFFFFF
    assert magic == b"PDLG" and ver == 1, f"FAIL: Magic/Versao invalida: {magic} v{ver}"
    assert calc_hdr_crc == hdr_crc, f"FAIL: Header CRC32 invalido ({calc_hdr_crc:#x} != {hdr_crc:#x})"

    tags = [tags_raw[i*16:(i+1)*16].split(b"\x00", 1)[0].decode("ascii") for i in range(adc_n)]
    dev_id = dev_raw.split(b"\x00", 1)[0].decode("ascii")
    eng_hhmm = f"{eng_sec // 3600:02d}:{(eng_sec % 3600) // 60:02d}"
    print(f"[OK] Header PDLG v{ver} | Device: {dev_id} | Rate: {rate_hz} Hz | RTC Sync: {bool(hdr_flags & 1)} | Motor: {eng_hhmm} ({eng_sec} s)")
    print(f"     Tags ADC ({adc_n}): {tags}")

    cursor, valid_recs, crc_errors, resync_bytes, dropped = hdr_sz, 0, 0, 0, 0
    prev_seq, first_mono, last_mono = None, None, None
    max_rpm, max_beacon, can_masks, adc_masks, max_sats = 0, 0, set(), set(), 0

    while cursor + rec_sz <= len(data):
        chunk = data[cursor : cursor + rec_sz]
        if chunk[0] != 0x5A or chunk[1] != 0xA5 or chunk[2] != 0x01:
            resync_bytes += 1
            cursor += 1
            continue
        rec_crc = struct.unpack_from("<I", chunk, 252)[0]
        if (zlib.crc32(chunk[:252]) & 0xFFFFFFFF) != rec_crc:
            crc_errors += 1
            cursor += 1
            continue

        rec = struct.unpack(REC_FMT, chunk)
        seq, mono_ms = rec[3], rec[4]
        beacon, adc_mask, can_mask, sats = rec[6], rec[7], rec[8], rec[9]
        rpm = rec[-5]

        if prev_seq is not None and seq > prev_seq + 1:
            dropped += (seq - prev_seq - 1)
        prev_seq = seq
        first_mono = mono_ms if first_mono is None else first_mono
        last_mono = mono_ms
        max_rpm = max(max_rpm, rpm)
        max_beacon = max(max_beacon, beacon)
        max_sats = max(max_sats, sats)
        can_masks.add(can_mask)
        adc_masks.add(adc_mask)
        valid_recs += 1
        cursor += rec_sz

    trailing = len(data) - cursor
    dur_s = ((last_mono - first_mono) / 1000.0) if (valid_recs > 1 and last_mono > first_mono) else 0.0
    eff_hz = ((valid_recs - 1) / dur_s) if dur_s > 0 else 0.0

    print(f"[RESUMO] Registros validos : {valid_recs} ({dur_s:.2f} s @ {eff_hz:.2f} Hz)")
    print(f"         Drops (sample_seq): {dropped} | Erros CRC: {crc_errors} | Resync: {resync_bytes} B | Tail truncado: {trailing} B")
    print(f"         RPM max: {max_rpm} | Lap Beacon max: {max_beacon} | GPS Sats max: {max_sats}")
    print(f"         ADC masks vistas: {[hex(m) for m in sorted(adc_masks)]} | CAN masks vistas: {[hex(m) for m in sorted(can_masks)]}")

if __name__ == "__main__":
    audit_pdlg(sys.argv[1])
```

**Como executar no PowerShell:**
```powershell
python audit_pdlg.py LOG_20261125_193000.bin
```
- **Critério de Aprovação (`TC-08`):** `Erros CRC: 0`, `Drops (sample_seq): 0` e taxa efetiva entre **`99,5 Hz` e `100,5 Hz`**.

---

## 12. Bloco `TC-09` — *Burn-in* Integrado (`30 min`) e *Shakedown* de Pista (`Fase 9`)

| ID | Teste | Procedimento Passo a Passo | Critério de Aprovação (*Pass*) |
| :---: | :--- | :--- | :--- |
| **`TC-09.1`** | *Burn-in* de Bancada (Simulação de *Endurance* FSAE — `30 min`) | Conecte todos os periféricos (MicroSD `64 GB`, RTC c/ bateria, GPS com *fix*, gerador CAN a `500 kbps` enviando os 7 nós a `100 Hz` com `RPM > 600` e sinais nos ADCs). Deixe gravando ininterruptamente por **`30 minutos`**, acionando `Free_SW2` a cada `2 minutos` (`15` voltas). | Arquivo `.bin` único de **`~46 MB`** (`~180.000` registros), **`Drops == 0`**, **`Erros CRC == 0`**, `lap_beacon` de `0` a `15`, sem reinicializações e temperatura normal nos reguladores e no `ESP32-S3`. |
| **`TC-09.2`** | *Shakedown* Estático no Carro (Ruído EMI de Ignição) | Com o Datalogger instalado no chassi da Protto UFSC, dê partida real no motor (FuelTech conectada via CAN), mantenha marcha lenta (`> 600 RPM`), dê leves aceleradas e desligue no *Kill Switch* / chave após `2 min`. | Partida dispara gravação automaticamente sem *reset* por queda de tensão da bateria (`cranking`); ruído eletromagnético de bobinas/bicos não gera *Bus-Off* nem corrompe `CRC-32` no MicroSD. |
| **`TC-09.3`** | Validação em Pista + Software de Telemetria (**MoTeC i2 / MegaLogViewer**) | Execute uma sessão real de pista com os 6 nós `ESP32-C3` + ECU + GPS. Converta o `.bin` para `.csv` conforme a Seção 8 de [`docs/BINARY_LOG_FORMAT.md`](BINARY_LOG_FORMAT.md) e abra no **MoTeC i2** / **MegaLogViewer**. | Traçado GPS fecha perfeitamente sobre a pista, velocidade GPS correlaciona com velocidades das 4 rodas (`0x200..0x203`), células de carga sobem com velocidade² (aerodinâmica) e divisões de volta (`lap_beacon`) separam cada volta no software. |

---

## 13. Ficha de Registro de Validação (Checklist Imprimível de Bancada)

| Bloco | Descrição Resumida | Data | Responsável | Status (`PASS` / `FAIL`) | Observações / Medições |
| :---: | :--- | :---: | :---: | :---: | :--- |
| `TC-00` | *Smoke Test*, `3.3V`/`5V`, Terminação CAN `60/120 Ω` e Limite `<= 3.3V` | `____/____/202_` | | `[ ] PASS  [ ] FAIL` | |
| `TC-01` | Flash ESP-IDF, Boot Dual-Core sem Watchdog e Standby `1 Hz` | `____/____/202_` | | `[ ] PASS  [ ] FAIL` | |
| `TC-02` | `Free_SW1` Standby, `Free_SW2` Beacon (`50 ms`) e LEDs (`1/2.5/5 Hz`) | `____/____/202_` | | `[ ] PASS  [ ] FAIL` | |
| `TC-03` | RTC `DS3231SN#` (`0x68`), Bateria `CR2032` e Fallback Monotônico | `____/____/202_` | | `[ ] PASS  [ ] FAIL` | |
| `TC-04` | `10` Canais `ADC1` (`GPIO 1..10`), Escalas `5V`/`12V` e Filtro EMA | `____/____/202_` | | `[ ] PASS  [ ] FAIL` | |
| `TC-05` | GPS `Neo-8M` (`115200 baud`, `10 Hz`, *3D Fix* e *Hot-Unplug*) | `____/____/202_` | | `[ ] PASS  [ ] FAIL` | |
| `TC-06` | CAN `500 kbps` (`6x C3` + FuelTech), *Node Stale* e *Bus-Off Recovery* | `____/____/202_` | | `[ ] PASS  [ ] FAIL` | |
| `TC-07` | Auto-Start `RPM > 600`, *Hold* `5 s`, MicroSD e Corte Brusco `12V` | `____/____/202_` | | `[ ] PASS  [ ] FAIL` | |
| `TC-08` | Auditoria Python `.bin` (`PDLG v1`: `CRC-32`, `100 Hz`, `0 Drops`) | `____/____/202_` | | `[ ] PASS  [ ] FAIL` | |
| `TC-09` | *Burn-in* `30 min` (`180k` amostras) e *Shakedown* em Pista | `____/____/202_` | | `[ ] PASS  [ ] FAIL` | |
