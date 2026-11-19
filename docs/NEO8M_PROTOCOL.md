# Guia de Protocolo GPS — Módulo u-blox NEO-8M / NEO-M8 (10 Hz via UART)

Este documento é a **referência oficial de configuração binária UBX (`10 Hz`) e gramática NMEA 0183** para o módulo receptor GNSS u-blox **NEO-8M / NEO-M8** conectado ao Datalogger Central (`ESP32-S3`), utilizado para mapeamento de traçado, velocidade real em solo (*ground speed*) e correlação aerodinâmica sincronizados com a base temporal do `RtcService` ($\le 50\ \text{ms}$, resolução de $1\ \text{ms}$).

- **Pinos e Hardware (BSP):** [`components/bsp/include/bsp_pins.h`](../components/bsp/include/bsp_pins.h)
- **Interface HAL:** [`components/hal/include/i_gps_hal.hpp`](../components/hal/include/i_gps_hal.hpp)
- **Driver LLD (`UART1` + UBX):** [`components/lld/include/neo8m_uart_lld.hpp`](../components/lld/include/neo8m_uart_lld.hpp)
- **Serviço BSW (Parser NMEA Zero-Heap):** [`components/bsw/include/gps_service.hpp`](../components/bsw/include/gps_service.hpp)
- **Interface UART:** `UART1`, **`115200 baud`**, **`8N1`** *(boot inicial de fábrica em `9600 baud`)*
- **Taxa Alvo:** **`10 Hz`** (`measRate = 100 ms`), modelo dinâmico **`Automotive (4)`**, apenas sentenças **`GGA` + `RMC`**

---

## 1. Documentação Oficial u-blox (Referências Normativas)

Nos módulos comerciais e esquemáticos, a família **u-blox M8** (`NEO-M8N`, `NEO-M8M`, `NEO-M8Q`, `NEO-M8J`) e **u-blox 8** (`NEO-8Q`) é comumente referenciada como **"Neo-8M"**. Toda a sequência UBX e gramática NMEA implementadas neste firmware seguem os documentos normativos oficiais da u-blox:

| Documento | Título Oficial | Escopo | Link Oficial (`u-blox.com`) |
| :--- | :--- | :--- | :--- |
| **`UBX-15031086`** | *NEO-M8 series — u-blox M8 concurrent GNSS modules — Data Sheet* | Especificação elétrica, `9600 baud` padrão de fábrica, taxa máxima `10 Hz` (GNSS concorrente) e `18 Hz` (GNSS único), ROM vs. Flash | [u-blox.com/en/docs/UBX-15031086](https://www.u-blox.com/en/docs/UBX-15031086) |
| **`UBX-13003221`** | *u-blox 8 / u-blox M8 Receiver Description Including Protocol Specification* | Protocolo binário UBX (`UBX-CFG-PRT`, `UBX-CFG-MSG`, `UBX-CFG-RATE`, `UBX-CFG-NAV5`, `UBX-ACK-ACK/NAK`), algoritmo Fletcher-8 e gramática NMEA 0183 (`GGA`, `RMC`) | [u-blox.com/en/docs/UBX-13003221](https://www.u-blox.com/en/docs/UBX-13003221) · [PDF Oficial](https://content.u-blox.com/sites/default/files/products/documents/u-blox8-M8_ReceiverDescrProtSpec_UBX-13003221.pdf) |
| **`UBX-15031913`** | *NEO-8Q Data Sheet* | Variante u-blox 8 baseada em ROM (`NEO-8Q`) | [u-blox.com/en/docs/UBX-15031913](https://www.u-blox.com/en/docs/UBX-15031913) |
| **`UBX-15029985`** | *NEO-8Q / NEO-M8 Hardware Integration Manual* | Integração elétrica, UART, antena GNSS e alimentação de backup (`V_BCKP`) | [u-blox.com/en/docs/UBX-15029985](https://www.u-blox.com/en/docs/UBX-15029985) |

---

## 2. Conexão de Hardware (`components/bsp/include/bsp_pins.h`)

O conector **`J5`** da placa central interliga o módulo GPS à **`UART1`** do `ESP32-S3-DEVKITC-1-N8R2`:

| Macro BSP | GPIO ESP32-S3 | Tag BSP | Conector / Direção | Pino no NEO-8M |
| :--- | :---: | :--- | :--- | :---: |
| **`PIN_GPS_RX`** | `GPIO_NUM_13` | `TAG_GPS_RX` (`"GPS_RX"`) | `J1_19` · `ESP32-S3 RX` $\leftarrow$ Recebe dados do GPS | **`TXD`** |
| **`PIN_GPS_TX`** | `GPIO_NUM_47` | `TAG_GPS_TX` (`"GPS_TX"`) | `J3_17` · `ESP32-S3 TX` $\rightarrow$ Envia comandos UBX ao GPS | **`RXD`** |

> **Nota Crítica — Variantes ROM vs. Flash (`UBX-15031086`):** Os módulos `NEO-M8M` e `NEO-M8Q` utilizam **ROM** (sem Flash SQI interna; configurações em BBR dependem de bateria `V_BCKP`), enquanto apenas o `NEO-M8N` possui Flash interna para `UBX-CFG-CFG`. Para garantir comportamento idêntico e determinístico em qualquer variante instalada no conector `J5`, o driver **`Neo8mUartLld::init()` envia obrigatoriamente toda a sequência de configuração UBX a cada boot**.

---

## 3. Análise de Banda UART (`9600` $\rightarrow$ `115200 baud` @ `10 Hz`)

No estado de fábrica (`UBX-15031086`), o módulo inicia em **`9600 baud` (`8N1`)** transmitindo **6 sentenças NMEA** por época (`GGA`, `GLL`, `GSA`, `GSV`, `RMC`, `VTG`) a `1 Hz`. Elevar para `10 Hz` sem aumentar o baud rate e filtrar sentenças satura o buffer de transmissão do receptor (gerando avisos `$GNTXT,01,01,00,txbuf alloc` e perda de pacotes):

| Configuração UART | Taxa Útil (`8N1`, $\approx \text{baud}/10$) | Carga Padrão (6 NMEA, $\approx 500\text{--}750\ \text{B/época}$ @ $10\ \text{Hz}$) | Carga Filtrada (`GGA+RMC`, $\approx 160\text{--}200\ \text{B/época}$ @ $10\ \text{Hz}$) | Diagnóstico |
| :---: | :---: | :---: | :---: | :--- |
| **`9600 baud`** | $\approx 960\ \text{B/s}$ | $5000\text{--}7500\ \text{B/s}$ ($> 500\%$) | $1600\text{--}2000\ \text{B/s}$ ($> 166\%$) | **Inviável** — satura TX (`txbuf alloc`) |
| **`38400 baud`** | $\approx 3840\ \text{B/s}$ | $130\text{--}195\%$ (satura) | $\approx 42\text{--}52\%$ | Mínimo viável apenas com filtro `GGA+RMC` |
| **`115200 baud`** | $\approx 11520\ \text{B/s}$ | $\approx 43\text{--}65\%$ | **`< 18%`** ($\approx 14\ \text{ms}$ de serialização) | **Padrão Motorsport Recomendado** |

---

## 4. Protocolo Binário UBX e Sequência de Boot (`10 Hz`)

### 4.1 Estrutura do Frame e Checksum Fletcher-8 (`UBX-13003221`)

Todo pacote binário UBX possui cabeçalho fixo de sincronismo (`0xB5 0x62`), comprimento em **Little-Endian** (`2 bytes`) e dois bytes de checksum **Fletcher-8** (`CK_A`, `CK_B`) calculados exclusivamente sobre `[Class, ID, Len_LSB, Len_MSB, Payload...]`:

```text
+------+------+-------+----+---------+---------+------------------+------+------+
| 0xB5 | 0x62 | Class | ID | Len_LSB | Len_MSB | Payload (N B)    | CK_A | CK_B |
+------+------+-------+----+---------+---------+------------------+------+------+
|<---- Sync ->|<---------------- Coberto por CK_A / CK_B ---------------->|
```

$$\text{CK\_A} = \left(\sum_{i=0}^{M-1} b_i\right) \bmod 256, \qquad \text{CK\_B} = \left(\sum_{i=0}^{M-1} \text{CK\_A}_i\right) \bmod 256$$

```cpp
// Cálculo Fletcher-8 UBX sobre [Class, ID, Len_L, Len_H, Payload...]
inline void compute_ubx_checksum(const uint8_t* data, std::size_t len, uint8_t& ck_a, uint8_t& ck_b) {
    ck_a = 0u;
    ck_b = 0u;
    for (std::size_t i = 0u; i < len; ++i) {
        ck_a = static_cast<uint8_t>(ck_a + data[i]);
        ck_b = static_cast<uint8_t>(ck_b + ck_a);
    }
}
```

### 4.2 Campo `mode = 0x000008D0` do `UBX-CFG-PRT` (`8N1`)

No comando `UBX-CFG-PRT` (`Class = 0x06`, `ID = 0x00`, `portID = 1` UART1), o campo `mode` (`uint32_t` Little-Endian: `D0 08 00 00`) define o formato serial `8N1`:
- **Bits `7:6` (`charLen`):** `11` = 8 bits de dados (`0x00C0`)
- **Bits `11:9` (`parity`):** `100` = sem paridade (`0x0800`)
- **Bits `13:12` (`nStopBits`):** `00` = 1 stop bit (`0x0000`)
- **Bit `4`:** `1` (`0x0010`, compatibilidade legada padrão u-blox) $\Rightarrow$ `0x0800 | 0x00C0 | 0x0010 = 0x000008D0`
- **`inProtoMask` / `outProtoMask`:** `0x0003` (`03 00`) habilita **UBX + NMEA** (mantém `UBX-ACK-ACK`/`NAK` ativos; o `GpsService` descarta frames `0xB5 0x62` de forma determinística sem afetar o parser NMEA). Alternativa NMEA-only na saída: `outProtoMask = 0x0002`.

### 4.3 Tabela de Comandos UBX Verificados (Bytes Exatos no Fio)

| Passo | Comando | Parâmetros Alvo | Frame Hexadecimal Completo (`B5 62 ... CK_A CK_B`) | Tam. |
| :---: | :--- | :--- | :--- | ---: |
| **1** | `UBX-CFG-PRT` *(Padrão)* | `UART1`, **`115200`**, `8N1`, in/out `UBX+NMEA` (`0x0003`) | `B5 62 06 00 14 00 01 00 00 00 D0 08 00 00 00 C2 01 00 03 00 03 00 00 00 00 00 BC 5E` | `28 B` |
| *1b* | `UBX-CFG-PRT` *(Alt. NMEA)* | `UART1`, `115200`, `8N1`, out `NMEA-only` (`0x0002`) | `B5 62 06 00 14 00 01 00 00 00 D0 08 00 00 00 C2 01 00 03 00 02 00 00 00 00 00 BB 58` | `28 B` |
| *1c* | `UBX-CFG-PRT` *(Alt. 38400)* | `UART1`, `38400`, `8N1`, in/out `UBX+NMEA` (`0x0003`) | `B5 62 06 00 14 00 01 00 00 00 D0 08 00 00 00 96 00 00 03 00 03 00 00 00 00 00 8F 70` | `28 B` |
| **2** | `UBX-CFG-MSG` (`3B`) **`GGA`** | `0xF0 0x00`, `rate = 1` *(porta atual)* | `B5 62 06 01 03 00 F0 00 01 FB 10` | `11 B` |
| **3** | `UBX-CFG-MSG` (`3B`) **`RMC`** | `0xF0 0x04`, `rate = 1` *(porta atual)* | `B5 62 06 01 03 00 F0 04 01 FF 18` | `11 B` |
| **4** | `UBX-CFG-MSG` (`3B`) `GLL` OFF | `0xF0 0x01`, `rate = 0` | `B5 62 06 01 03 00 F0 01 00 FB 11` | `11 B` |
| **5** | `UBX-CFG-MSG` (`3B`) `GSA` OFF | `0xF0 0x02`, `rate = 0` | `B5 62 06 01 03 00 F0 02 00 FC 13` | `11 B` |
| **6** | `UBX-CFG-MSG` (`3B`) `GSV` OFF | `0xF0 0x03`, `rate = 0` | `B5 62 06 01 03 00 F0 03 00 FD 15` | `11 B` |
| **7** | `UBX-CFG-MSG` (`3B`) `VTG` OFF | `0xF0 0x05`, `rate = 0` | `B5 62 06 01 03 00 F0 05 00 FF 19` | `11 B` |
| **8** | `UBX-CFG-RATE` *(UTC)* | `measRate = 100 ms` (`10 Hz`), `navRate = 1`, `timeRef = 0` | `B5 62 06 08 06 00 64 00 01 00 00 00 79 10` | `14 B` |
| *8b* | `UBX-CFG-RATE` *(Alt. GPS)* | `measRate = 100 ms` (`10 Hz`), `navRate = 1`, `timeRef = 1` | `B5 62 06 08 06 00 64 00 01 00 01 00 7A 12` | `14 B` |
| **9** | `UBX-CFG-NAV5` *(Padrão)* | `mask = 0x0001` (`dyn`), `dynModel = 4` (Automotive) | `B5 62 06 24 24 00 01 00 04 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 53 70` | `44 B` |
| *9b* | `UBX-CFG-NAV5` *(+FixMode)* | `mask = 0x0005` (`dyn` + `posFixMode`), `dynModel = 4`, `fixMode = 3` | `B5 62 06 24 24 00 05 00 04 03 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 5A 63` | `44 B` |

> **Variante `UBX-CFG-MSG` 8 bytes (todas as portas explicitando `UART1` `[I2C, UART1, UART2, USB, SPI, Res]`):**
> - `GGA = 1` (`UART1`): `B5 62 06 01 08 00 F0 00 00 01 00 00 00 00 00 28`
> - `RMC = 1` (`UART1`): `B5 62 06 01 08 00 F0 04 00 01 00 00 00 00 04 44`
> - `GLL = 0`: `B5 62 06 01 08 00 F0 01 00 00 00 00 00 00 00 2A` · `GSA = 0`: `... F0 02 ... 01 31` · `GSV = 0`: `... F0 03 ... 02 38` · `VTG = 0`: `... F0 05 ... 04 46`
>
> **Respostas `UBX-ACK-ACK` (`0x05 0x01`) esperadas:** `CFG-PRT`: `B5 62 05 01 02 00 06 00 0E 37` · `CFG-MSG`: `... 06 01 0F 38` · `CFG-RATE`: `... 06 08 16 3F` · `CFG-NAV5`: `... 06 24 32 5B`.

### 4.4 Sequência Obrigatória de Inicialização (`Neo8mUartLld::init()`)

1. Instalar e configurar `UART1` no ESP32-S3 a **`9600 baud`**, `8N1` (`PIN_GPS_RX = GPIO 13`, `PIN_GPS_TX = GPIO 47`).
2. Aguardar estabilização inicial (`100 ms`) e limpar RX (`uart_flush_input`).
3. Enviar **`UBX-CFG-PRT`** (`115200`, `8N1`, `UBX+NMEA`), aguardar término físico de TX (`uart_wait_tx_done`), alterar o baud do ESP32-S3 (`uart_set_baudrate(115200)`) e **reenviar `UBX-CFG-PRT` uma segunda vez em `115200`** *(garante convergência tanto em cold-boot a `9600` quanto em warm-reset do ESP32 com o GPS já em `115200`)*.
4. Enviar **`UBX-CFG-MSG`** habilitando `GGA = 1` e `RMC = 1` e desabilitando `GLL`, `GSA`, `GSV` e `VTG` (`rate = 0`).
5. Enviar **`UBX-CFG-RATE`** (`measRate = 100 ms`, `navRate = 1`, `timeRef = 0` UTC) para **`10 Hz`**.
6. Enviar **`UBX-CFG-NAV5`** com `dynModel = 4` (*Automotive*).

---

## 5. Gramática NMEA 0183 (`$xxGGA` e `$xxRMC`) e Checksum XOR

### 5.1 Validação de Checksum XOR (Obrigatória)

Toda sentença NMEA segue o formato `$<payload>*<HH>\r\n`. O checksum de 8 bits é o **XOR byte a byte de todos os caracteres ASCII estritamente entre `$` e `*`** (excluindo `$` e `*`), codificado em 2 dígitos hexadecimais (`0-9`, `A-F` ou `a-f`).

- **Regras de Rejeição:** Rejeitar e incrementar `checksum_errors()` se faltar `$` inicial, faltar `*HH`, houver `$` interno antes de `*`, ou se o XOR calculado diferir de `HH`.
- **Varredura Zero-Heap:** Campos vazios consecutivos (`,,`) são válidos em NMEA (especialmente sem *fix* ou estacionário). **Proibido usar `strtok()`** (que colapsa vírgulas consecutivas) ou `atof()` (dependente de *locale*). O `GpsService` utiliza varredura manual de índices em `std::array<char, 128>` + acumuladores `double`.

### 5.2 Sentença `$xxGGA` (`$GPGGA` / `$GNGGA` — 15 campos)

Formato: `$xxGGA,time,lat,NS,lon,EW,quality,numSV,HDOP,alt,altUnit,sep,sepUnit,diffAge,diffStation*cs`

| Índice | Campo | Formato / Valores | Conversão / Destino em `hal::GpsData` |
| :---: | :--- | :--- | :--- |
| `0` | Talker + ID | `GPGGA`, `GNGGA` (qualquer talker 2-char + `GGA`) | Identificador da sentença |
| `1` | `time` | `hhmmss.ss` (UTC) | `utc_time_ms` (ms desde 00:00:00 UTC) |
| `2..3` | `lat`, `NS` | `ddmm.mmmmm`, `'N'` / `'S'` | `latitude_deg` (`double`: `dd + mm.mmmmm / 60.0`; negativo se `'S'`) |
| `4..5` | `lon`, `EW` | `dddmm.mmmmm`, `'E'` / `'W'` | `longitude_deg` (`double`: `ddd + mm.mmmmm / 60.0`; negativo se `'W'`) |
| `6` | `quality` | `0`=Sem fix, `1`=GPS/GNSS, `2`=DGPS, `4`=RTK, `5`=Float, `6`=DR | `fix_quality`; `gga_valid = (quality > 0)` |
| `7` | `numSV` | `00..99` | `satellites` (`uint8_t`) |
| `8` | `HDOP` | Decimal (ex.: `1.01`, `99.99` sem fix) | `hdop` (`float`) |
| `9..10` | `alt`, `altUnit` | Altitude MSL em metros, `'M'` | `altitude_m` (`float`, suporta valores negativos) |
| `11..14` | `sep`, `diff*` | Separação geoidal e idade DGPS | Validados na contagem (`15` campos) |

### 5.3 Sentença `$xxRMC` (`$GPRMC` / `$GNRMC` — 12, 13 ou 14 campos)

Formato (NMEA 4.10): `$xxRMC,time,status,lat,NS,lon,EW,spd,cog,date,mv,mvEW,posMode,navStatus*cs`

| Índice | Campo | Formato / Valores | Conversão / Destino em `hal::GpsData` |
| :---: | :--- | :--- | :--- |
| `0` | Talker + ID | `GPRMC`, `GNRMC` (qualquer talker 2-char + `RMC`) | Identificador da sentença |
| `1` | `time` | `hhmmss.ss` (UTC) | `utc_time_ms` |
| `2` | `status` | `'A'` (ativo/válido) ou `'V'` (inválido/aviso) | `rmc_valid = (status == 'A' && posMode != 'N')` |
| `3..6` | `lat, NS, lon, EW` | `ddmm.mmmmm`, `'N'/'S'`, `dddmm.mmmmm`, `'E'/'W'` | `latitude_deg`, `longitude_deg` (`double`) |
| `7` | `spd` | Velocidade em **nós** (*knots*) | `speed_knots`; `speed_kmh = spd * 1.852f`; `speed_mps = spd * (1852.0f / 3600.0f)` |
| `8` | `cog` | Curso verdadeiro em graus (`0.0..360.0`) | `course_deg` (`0.0f` quando vazio estacionário) |
| `9` | `date` | `ddmmyy` (UTC, século `2000 + yy`) | `utc_date_yyyymmdd` (validado inclusive anos bissextos) |
| `10..11` | `mv`, `mvEW` | Declinação magnética (frequentemente vazia) | Aceita tokens vazios (`,,`) |
| `12` | `posMode` *(NMEA $\ge$ 2.3)* | `'A'`, `'D'`, `'E'`, `'F'`, `'R'`, `'N'` (sem fix) | Opcional (ausente na NMEA 2.0 com 12 campos) |
| `13` | `navStatus` *(NMEA $\ge$ 4.10)* | `'V'` | Opcional (ausente em 12/13 campos; presente em 14 campos) |

### 5.4 Exemplos Verificados e Casos de Borda

| Cenário | Sentença Verificada | Comportamento do `GpsService` |
| :--- | :--- | :--- |
| **`$GPGGA` Fix Válido (N/E)** | `$GPGGA,092725.00,4717.11399,N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,*5B` | `lat=47.285233`, `lon=8.565265`, `alt=499.6 m`, `sats=8`, `has_fix=true` |
| **`$GNGGA` Fix DGPS (S/W)** | `$GNGGA,123519.00,2735.12345,S,04832.12345,W,1,10,0.95,12.4,M,-2.1,M,,*6A` | `lat=-27.585391`, `lon=-48.535391`, `fix_quality=1`, `sats=10` |
| **`$GNGGA` Sem Fix** | `$GNGGA,,,,,,0,00,99.99,,,,,,*56` | `gga_valid=false`, `fix_quality=0`, `hdop=99.99f` |
| **`$GPRMC` 14 campos (4.10)** | `$GPRMC,083559.00,A,4717.11437,N,00833.91522,E,0.004,77.52,091202,,,A,V*2D` | Aceita 14 campos (`rmc_valid=true`, `utc_date=20021209`) |
| **`$GPRMC` 13 campos (4.0)** | `$GPRMC,083559.00,A,4717.11437,N,00833.91522,E,0.004,77.52,091202,,,A*57` | Aceita 13 campos (`rmc_valid=true`) |
| **`$GPRMC` 12 campos (2.0)** | `$GPRMC,083559.00,A,4717.11437,N,00833.91522,E,0.004,77.52,091202,,*3A` | Aceita 12 campos (`rmc_valid=true`) |
| **`$GNRMC` Estacionário sem COG** | `$GNRMC,123519.00,A,2735.12345,S,04832.12345,W,0.012,,191126,,,A,V*17` | `rmc_valid=true`, `course_deg=0.0f` |
| **`$GNRMC` Sem Fix** | `$GNRMC,123519.00,V,,,,,,,191126,,,N,V*18` | `rmc_valid=false`, zera velocidade/curso; mantém data/hora UTC |
| **Texto / UBX Misto** | `$GNTXT,01,01,00,txbuf alloc*61` ou frames `0xB5 0x62 ...` | Ignorados de forma limpa sem erro de checksum (`checksum_errors() == 0`) |

> **Sincronização Temporal:** A cada sentença `GGA` ou `RMC` válida, `GpsService` invoca `RtcService::sample_timestamp()`, preenchendo `timestamp_ms`, `monotonic_ms` e `time_synchronized` (`true` quando o RTC `DS3231SN#` está sincronizado; `false` com base monotônica de $1\ \text{ms}$ em modo degradado).
