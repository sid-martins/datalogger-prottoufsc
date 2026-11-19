# Especificação Oficial do Formato Binário de Log — `PDLG v1` (Protto UFSC Datalogger)

Este documento define o **contrato binário completo (`PDLG v1`)** gerado pelo firmware embarcado do Datalogger Central **ESP32-S3 (`LoggerService`)** no cartão MicroSD. Ele contém todas as especificações byte a byte necessárias para implementar um software decodificador/conversor offline (em Python, Go, Rust, C++ ou MATLAB) para análise de telemetria e exportação para **MoTeC i2**, **AiM Race Studio** ou **MegaLogViewer**.

> **Código-fonte de referência no firmware:** [`components/bsw/include/binary_log_format.hpp`](../components/bsw/include/binary_log_format.hpp) e [`components/bsw/src/logger_service.cpp`](../components/bsw/src/logger_service.cpp).

---

## 1. Visão Geral e Eficiência Embarcada

Para maximizar o determinismo no **ESP32-S3** a `100 Hz` sem consumir CPU formatando texto (`snprintf`), o firmware grava fluxos binários de tamanho fixo:

| Métrica | CSV Anterior (`56` colunas) | Binário `PDLG v1` | Ganho Obtido |
| :--- | :---: | :---: | :--- |
| **Tamanho por amostra** | $\approx 420 - 500\ \text{bytes}$ | **`256` bytes fixos** | **$\approx 45\%$ menor** |
| **Taxa de escrita @ `100 Hz`** | $\approx 45\ \text{KB/s}$ | **`25,6 KB/s`** | **Alinhado a bloco SD (`16 reg = 4096 B`)** |
| **Endurance (`25 min` @ `100 Hz`)** | $\approx 68\ \text{MB}$ | **`38,4 MB`** | $\approx 1600\times$ folga em MicroSD `64 GB` |
| **Custo CPU (Core 0)** | Formatação `float`/`double` via `snprintf` | Cópia direta de bytes + CRC-32 LUT | **$< 5\ \mu\text{s}$ por amostra** |
| **Resiliência a queda de energia** | Linha final truncada | *Sync Word* (`0xA55A`) + **CRC-32 IEEE** | Recuperação determinística quadro a quadro |

---

## 2. Convenção de Arquivos, Tipos e Endianness

### 2.1 Nomenclatura dos Arquivos no MicroSD (`/sdcard/`)
- **Com RTC `DS3231SN#` sincronizado:** `LOG_YYYYMMDD_HHMMSS.bin` (ex.: `LOG_20261119_143005.bin`).
- **Em modo degradado (falha I2C/OSF do RTC):** `LOG_BOOT_0000004321ms.bin` (`10` dígitos com zeros à esquerda indicando o tempo monotônico em ms na abertura da sessão).

### 2.2 Regras Gerais de Representação
- **Ordem de Bytes (*Endianness*):** Todos os inteiros e pontos flutuantes são gravados em **Little-Endian (LSB primeiro)**, sem *padding* dependente de compilador.
- **Ponto Flutuante:** Padrão **IEEE 754** (`float` = `binary32` 4 bytes; `double` = `binary64` 8 bytes).
- **Strings ASCII:** Tamanho fixo, preenchidas com `0x00` (*null-padded*). Para extrair em Python/Go, leia até o primeiro byte `0x00` e decodifique como ASCII (`UTF-8` compatível).
- **Estrutura Linear do Arquivo:**
  ```text
  [ Cabeçalho de Sessão: 256 bytes (offset 0x0000) ]
  [ Registro de Telemetria #1: 256 bytes ]
  [ Registro de Telemetria #2: 256 bytes ]
  ...
  [ Registro de Telemetria #N: 256 bytes ]
  ```

---

## 3. Especificação do Checksum `CRC-32`

Tanto o Cabeçalho (`bytes 0..251`) quanto cada Registro (`bytes 0..251`) são protegidos por **CRC-32 IEEE 802.3** nos últimos 4 bytes (`252..255`).

| Parâmetro | Valor Normativo |
| :--- | :--- |
| **Polinômio refletido** | `0xEDB88320` (representação normal: `0x04C11DB7`) |
| **Valor inicial (`Init`)** | `0xFFFFFFFF` |
| **RefIn / RefOut** | `true` / `true` |
| **XOR Final (`XorOut`)** | `0xFFFFFFFF` |
| **Vetor de Validação** | ASCII `"123456789"` (9 bytes) $\rightarrow$ **`0xCBF43926`** |
| **Equivalente Python / Go** | `zlib.crc32(data) & 0xFFFFFFFF` / `crc32.ChecksumIEEE(data)` |

---

## 4. Layout Byte a Byte do Cabeçalho do Arquivo (`256 bytes`)

Gravado uma única vez no início do arquivo (`offset 0..255`).

| Offset (Dec) | Offset (Hex) | Tam (B) | Campo | Tipo C / Endian | `struct` (Python) | Tipo Go | Descrição / Valor Esperado |
| :---: | :---: | :---: | :--- | :--- | :---: | :---: | :--- |
| `0..3` | `0x00..0x03` | `4` | `magic` | `char[4]` | `4s` | `[4]byte` | Assinatura fixa ASCII `"PDLG"` (`0x50 0x44 0x4C 0x47`). |
| `4..5` | `0x04..0x05` | `2` | `format_version` | `uint16_t` LE | `H` | `uint16` | Versão do protocolo: `1`. Rejeitar se desconhecida. |
| `6..7` | `0x06..0x07` | `2` | `header_size` | `uint16_t` LE | `H` | `uint16` | Tamanho do cabeçalho em bytes (`256`). Use para saltar até o 1º registro. |
| `8..9` | `0x08..0x09` | `2` | `record_size` | `uint16_t` LE | `H` | `uint16` | Tamanho de cada registro em bytes (`256`). |
| `10..11` | `0x0A..0x0B` | `2` | `header_flags` | `uint16_t` LE | `H` | `uint16` | Bit `0` (`0x0001`): `1` se RTC estava sincronizado na abertura; bits `1..15` = `0`. |
| `12..13` | `0x0C..0x0D` | `2` | `nominal_rate_hz` | `uint16_t` LE | `H` | `uint16` | Taxa nominal de aquisição da task Core 1 (padrão `100` Hz). |
| `14` | `0x0E` | `1` | `adc_channel_count` | `uint8_t` | `B` | `uint8` | Número de canais ADC locais válidos no vetor `adc_physical` (`10`). |
| `15` | `0x0F` | `1` | `can_node_count` | `uint8_t` | `B` | `uint8` | Quantidade de nós lógicos CAN monitorados (`7`: 6 ESP32-C3 + ECU). |
| `16..23` | `0x10..0x17` | `8` | `start_monotonic_ms` | `uint64_t` LE | `Q` | `uint64` | Contador monotônico (`esp_timer`) em ms no momento em que o arquivo foi aberto. |
| `24..31` | `0x18..0x1F` | `8` | `start_epoch_ms` | `uint64_t` LE | `Q` | `uint64` | Timestamp Epoch UTC em ms (se `header_flags & 0x01`) ou fallback monotônico. |
| `32..191` | `0x20..0xBF` | `160` | `adc_channel_tags[10][16]` | `char[10][16]` | `160s` *(10×16s)* | `[10][16]byte` | `10` slots ASCII de `16 B` null-padded com os nomes `TAG_*` de `bsp::kAnalogChannels`. |
| `192..223` | `0xC0..0xDF` | `32` | `device_id` | `char[32]` | `32s` | `[32]byte` | Identificador ASCII null-padded do hardware: `"PROTTO-UFSC-ESP32S3-V1"`. |
| `224..251` | `0xE0..0xFB` | `28` | `reserved` | `uint8_t[28]` | `28s` | `[28]byte` | Bytes reservados para futuras revisões (sempre `0x00` na v1). |
| `252..255` | `0xFC..0xFF` | `4` | `header_crc32` | `uint32_t` LE | `I` | `uint32` | CRC-32 IEEE 802.3 calculado sobre os bytes `0..251` do cabeçalho. |

### 4.1 Nomes Padrão em `adc_channel_tags[0..9]` (Autodescritivos no Cabeçalho)
O decodificador **deve** ler as tags dinamicamente dos offsets `32 + 16 * i` (`i = 0..9`). Na configuração padrão atual do BSP (`bsp_pins.h`), os 10 canais são:

| Índice | Offset no Header | Canal Físico | Tag Padrão (`TAG_*`) |
| :---: | :---: | :--- | :--- |
| `0` | `32..47` (`0x20`) | `ADC1_CH0` (`GPIO 1`) | `OilPressure` |
| `1` | `48..63` (`0x30`) | `ADC1_CH1` (`GPIO 2`) | `OilTemp` |
| `2` | `64..79` (`0x40`) | `ADC1_CH2` (`GPIO 3`) | `FuelPressure` |
| `3` | `80..95` (`0x50`) | `ADC1_CH3` (`GPIO 4`) | `Lambda` |
| `4` | `96..111` (`0x60`) | `ADC1_CH4` (`GPIO 5`) | `ADC5to3v3_1` |
| `5` | `112..127` (`0x70`) | `ADC1_CH5` (`GPIO 6`) | `ADC5to3v3_2` |
| `6` | `128..143` (`0x80`) | `ADC1_CH6` (`GPIO 7`) | `ADC5to3v3_3` |
| `7` | `144..159` (`0x90`) | `ADC1_CH7` (`GPIO 8`) | `ADC5to3v3_4` |
| `8` | `160..175` (`0xA0`) | `ADC1_CH8` (`GPIO 9`) | `ADC12to3v3_1` |
| `9` | `176..191` (`0xB0`) | `ADC1_CH9` (`GPIO 10`) | `ADC12to3v3_2` |

---

## 5. Layout Byte a Byte do Registro de Telemetria (`256 bytes`, `record_type = 0x01`)

Todos os offsets abaixo são **relativos ao início do registro** (`0..255`).

| Offset (Dec) | Offset (Hex) | Tam (B) | Campo | Tipo C / LE | `struct` | Tipo Go | Unidade | Origem |
| :---: | :---: | :---: | :--- | :--- | :---: | :---: | :---: | :--- |
| `0..1` | `0x00..0x01` | `2` | `sync_word` | `uint16_t` | `H` | `uint16` | — | Fixo `0xA55A` (`0x5A 0xA5` no arquivo) |
| `2` | `0x02` | `1` | `record_type` | `uint8_t` | `B` | `uint8` | — | Fixo `0x01` (Snapshot de telemetria) |
| `3` | `0x03` | `1` | `status_flags` | `uint8_t` | `B` | `uint8` | bitmask | Bit 0: `rtc_synchronized`; Bit 1: `gps_has_fix` |
| `4..7` | `0x04..0x07` | `4` | `sample_seq` | `uint32_t` | `I` | `uint32` | contagem | Sequencial monotonicamente crescente (`1, 2, ...`) |
| `8..15` | `0x08..0x0F` | `8` | `monotonic_ms` | `uint64_t` | `Q` | `uint64` | `ms` | `RtcService` / `esp_timer` (base `1 ms`) |
| `16..23` | `0x10..0x17` | `8` | `epoch_ms` | `uint64_t` | `Q` | `uint64` | `ms` | Epoch UTC (se Bit 0 de `status_flags` = 1) ou `monotonic_ms` |
| `24..25` | `0x18..0x19` | `2` | `lap_beacon` | `uint16_t` | `H` | `uint16` | contagem | Marcador de volta/evento (`Free_SW2`) |
| `26..27` | `0x1A..0x1B` | `2` | `adc_valid_mask` | `uint16_t` | `H` | `uint16` | bitmask | Bits `0..9` = validade dos 10 canais `ADC1` |
| `28` | `0x1C` | `1` | `can_node_online_mask` | `uint8_t` | `B` | `uint8` | bitmask | Bits `0..6` = status online dos nós CAN |
| `29` | `0x1D` | `1` | `gps_satellites` | `uint8_t` | `B` | `uint8` | sats | Quantidade de satélites em uso (`$xxGGA`) |
| `30..31` | `0x1E..0x1F` | `2` | `imu_status_flags` | `uint16_t` | `H` | `uint16` | bitmask | `CAN 0x300` bytes `6..7` (status calibração WIT) |
| `32..39` | `0x20..0x27` | `8` | `gps_latitude_deg` | `double` | `d` | `float64` | `deg` | `GPS Neo-8M` (Sul negativo, Norte positivo) |
| `40..47` | `0x28..0x2F` | `8` | `gps_longitude_deg` | `double` | `d` | `float64` | `deg` | `GPS Neo-8M` (Oeste negativo, Leste positivo) |
| `48..51` | `0x30..0x33` | `4` | `gps_altitude_m` | `float` | `f` | `float32` | `m` | `GPS Neo-8M` (`$xxGGA` altitude MSL) |
| `52..55` | `0x34..0x37` | `4` | `gps_speed_kmh` | `float` | `f` | `float32` | `km/h` | `GPS Neo-8M` (`$xxRMC` velocidade no solo) |
| `56..59` | `0x38..0x3B` | `4` | `gps_speed_mps` | `float` | `f` | `float32` | `m/s` | `GPS Neo-8M` (`gps_speed_kmh / 3.6`) |
| `60..63` | `0x3C..0x3F` | `4` | `gps_course_deg` | `float` | `f` | `float32` | `deg` | `GPS Neo-8M` (`$xxRMC` COG $0..360^\circ$) |
| `64..67` | `0x40..0x43` | `4` | `adc_physical[0]` | `float` | `f` | `float32` | eng. | `ADC1_CH0` (`adc_channel_tags[0]`, padrão `OilPressure`) |
| `68..71` | `0x44..0x47` | `4` | `adc_physical[1]` | `float` | `f` | `float32` | eng. | `ADC1_CH1` (`adc_channel_tags[1]`, padrão `OilTemp`) |
| `72..75` | `0x48..0x4B` | `4` | `adc_physical[2]` | `float` | `f` | `float32` | eng. | `ADC1_CH2` (`adc_channel_tags[2]`, padrão `FuelPressure`) |
| `76..79` | `0x4C..0x4F` | `4` | `adc_physical[3]` | `float` | `f` | `float32` | eng. | `ADC1_CH3` (`adc_channel_tags[3]`, padrão `Lambda`) |
| `80..83` | `0x50..0x53` | `4` | `adc_physical[4]` | `float` | `f` | `float32` | eng. | `ADC1_CH4` (`adc_channel_tags[4]`, padrão `ADC5to3v3_1`) |
| `84..87` | `0x54..0x57` | `4` | `adc_physical[5]` | `float` | `f` | `float32` | eng. | `ADC1_CH5` (`adc_channel_tags[5]`, padrão `ADC5to3v3_2`) |
| `88..91` | `0x58..0x5B` | `4` | `adc_physical[6]` | `float` | `f` | `float32` | eng. | `ADC1_CH6` (`adc_channel_tags[6]`, padrão `ADC5to3v3_3`) |
| `92..95` | `0x5C..0x5F` | `4` | `adc_physical[7]` | `float` | `f` | `float32` | eng. | `ADC1_CH7` (`adc_channel_tags[7]`, padrão `ADC5to3v3_4`) |
| `96..99` | `0x60..0x63` | `4` | `adc_physical[8]` | `float` | `f` | `float32` | eng. | `ADC1_CH8` (`adc_channel_tags[8]`, padrão `ADC12to3v3_1`) |
| `100..103` | `0x64..0x67` | `4` | `adc_physical[9]` | `float` | `f` | `float32` | eng. | `ADC1_CH9` (`adc_channel_tags[9]`, padrão `ADC12to3v3_2`) |
| `104..107` | `0x68..0x6B` | `4` | `steering_angle_deg` | `float` | `f` | `float32` | `deg` | `CAN 0x100` (Freio/Direção) |
| `108..111` | `0x6C..0x6F` | `4` | `brake_pressure_front_bar` | `float` | `f` | `float32` | `bar` | `CAN 0x100` (Freio Dianteiro) |
| `112..115` | `0x70..0x73` | `4` | `brake_pressure_rear_bar` | `float` | `f` | `float32` | `bar` | `CAN 0x100` (Freio Traseiro) |
| `116..119` | `0x74..0x77` | `4` | `brake_pedal_pos_pct` | `float` | `f` | `float32` | `%` | `CAN 0x100` (Curso Pedal de Freio) |
| `120..123` | `0x78..0x7B` | `4` | `damper_fl_mm` | `float` | `f` | `float32` | `mm` | `CAN 0x200` (Roda FL — Suspensão) |
| `124..127` | `0x7C..0x7F` | `4` | `wheel_spd_fl_kmh` | `float` | `f` | `float32` | `km/h` | `CAN 0x200` (Roda FL — Velocidade) |
| `128..131` | `0x80..0x83` | `4` | `load_fl_n` | `float` | `f` | `float32` | `N` | `CAN 0x200` (Roda FL — Célula de Carga) |
| `132..135` | `0x84..0x87` | `4` | `disc_temp_fl_c` | `float` | `f` | `float32` | `°C` | `CAN 0x200` (Roda FL — Temp. Disco) |
| `136..139` | `0x88..0x8B` | `4` | `damper_fr_mm` | `float` | `f` | `float32` | `mm` | `CAN 0x201` (Roda FR — Suspensão) |
| `140..143` | `0x8C..0x8F` | `4` | `wheel_spd_fr_kmh` | `float` | `f` | `float32` | `km/h` | `CAN 0x201` (Roda FR — Velocidade) |
| `144..147` | `0x90..0x93` | `4` | `load_fr_n` | `float` | `f` | `float32` | `N` | `CAN 0x201` (Roda FR — Célula de Carga) |
| `148..151` | `0x94..0x97` | `4` | `disc_temp_fr_c` | `float` | `f` | `float32` | `°C` | `CAN 0x201` (Roda FR — Temp. Disco) |
| `152..155` | `0x98..0x9B` | `4` | `damper_rl_mm` | `float` | `f` | `float32` | `mm` | `CAN 0x202` (Roda RL — Suspensão) |
| `156..159` | `0x9C..0x9F` | `4` | `wheel_spd_rl_kmh` | `float` | `f` | `float32` | `km/h` | `CAN 0x202` (Roda RL — Velocidade) |
| `160..163` | `0xA0..0xA3` | `4` | `load_rl_n` | `float` | `f` | `float32` | `N` | `CAN 0x202` (Roda RL — Célula de Carga) |
| `164..167` | `0xA4..0xA7` | `4` | `disc_temp_rl_c` | `float` | `f` | `float32` | `°C` | `CAN 0x202` (Roda RL — Temp. Disco) |
| `168..171` | `0xA8..0xAB` | `4` | `damper_rr_mm` | `float` | `f` | `float32` | `mm` | `CAN 0x203` (Roda RR — Suspensão) |
| `172..175` | `0xAC..0xAF` | `4` | `wheel_spd_rr_kmh` | `float` | `f` | `float32` | `km/h` | `CAN 0x203` (Roda RR — Velocidade) |
| `176..179` | `0xB0..0xB3` | `4` | `load_rr_n` | `float` | `f` | `float32` | `N` | `CAN 0x203` (Roda RR — Célula de Carga) |
| `180..183` | `0xB4..0xB7` | `4` | `disc_temp_rr_c` | `float` | `f` | `float32` | `°C` | `CAN 0x203` (Roda RR — Temp. Disco) |
| `184..187` | `0xB8..0xBB` | `4` | `imu_accel_x_mps2` | `float` | `f` | `float32` | `m/s²` | `CAN 0x300` (Aceleração Longitudinal) |
| `188..191` | `0xBC..0xBF` | `4` | `imu_accel_y_mps2` | `float` | `f` | `float32` | `m/s²` | `CAN 0x300` (Aceleração Lateral) |
| `192..195` | `0xC0..0xC3` | `4` | `imu_accel_z_mps2` | `float` | `f` | `float32` | `m/s²` | `CAN 0x300` (Aceleração Vertical) |
| `196..199` | `0xC4..0xC7` | `4` | `imu_gyro_x_dps` | `float` | `f` | `float32` | `deg/s` | `CAN 0x301` (*Roll Rate*) |
| `200..203` | `0xC8..0xCB` | `4` | `imu_gyro_y_dps` | `float` | `f` | `float32` | `deg/s` | `CAN 0x301` (*Pitch Rate*) |
| `204..207` | `0xCC..0xCF` | `4` | `imu_gyro_z_dps` | `float` | `f` | `float32` | `deg/s` | `CAN 0x301` (*Yaw Rate*) |
| `208..211` | `0xD0..0xD3` | `4` | `ecu_tps_pct` | `float` | `f` | `float32` | `%` | `FuelTech 0x600` (`0x14080600`) |
| `212..215` | `0xD4..0xD7` | `4` | `ecu_map_bar` | `float` | `f` | `float32` | `bar` | `FuelTech 0x600` (`0x14080600`) |
| `216..219` | `0xD8..0xDB` | `4` | `ecu_air_temp_c` | `float` | `f` | `float32` | `°C` | `FuelTech 0x600` (`0x14080600`) |
| `220..223` | `0xDC..0xDF` | `4` | `ecu_engine_temp_c` | `float` | `f` | `float32` | `°C` | `FuelTech 0x600` (`0x14080600`) |
| `224..227` | `0xE0..0xE3` | `4` | `ecu_oil_pressure_bar` | `float` | `f` | `float32` | `bar` | `FuelTech 0x601` (`0x14080601`) |
| `228..231` | `0xE4..0xE7` | `4` | `ecu_fuel_pressure_bar` | `float` | `f` | `float32` | `bar` | `FuelTech 0x601` (`0x14080601`) |
| `232..235` | `0xE8..0xEB` | `4` | `ecu_water_pressure_bar` | `float` | `f` | `float32` | `bar` | `FuelTech 0x601` (`0x14080601`) |
| `236..239` | `0xEC..0xEF` | `4` | `ecu_exhaust_o2_lambda` | `float` | `f` | `float32` | `λ` | `FuelTech 0x602` (`0x14080602`) |
| `240..243` | `0xF0..0xF3` | `4` | `ecu_oil_temp_c` | `float` | `f` | `float32` | `°C` | `FuelTech 0x602` (`0x14080602`) |
| `244..245` | `0xF4..0xF5` | `2` | `ecu_rpm` | `uint16_t` | `H` | `uint16` | `rpm` | `FuelTech 0x602` (`0x14080602`) |
| `246..247` | `0xF6..0xF7` | `2` | `ecu_gear` | `int16_t` | `h` | `int16` | marcha | `FuelTech 0x601`: `-1`=Ré, `0`=Neutro, `1..6`=Marcha |
| `248..249` | `0xF8..0xF9` | `2` | `ecu_pit_limit_status` | `uint16_t` | `H` | `uint16` | status | `FuelTech 0x602` (`0x14080602`) |
| `250..251` | `0xFA..0xFB` | `2` | `reserved` | `uint16_t` | `H` | `uint16` | — | Reservado (sempre `0x0000` na v1) |
| `252..255` | `0xFC..0xFF` | `4` | `record_crc32` | `uint32_t` | `I` | `uint32` | — | CRC-32 IEEE 802.3 sobre bytes `0..251` do registro |

### 5.1 Decodificação das Máscaras de Bits (*Bitmasks*)

#### `status_flags` (Offset `3`, `uint8_t`)
| Bit | Máscara | Significado |
| :---: | :---: | :--- |
| `0` | `0x01` | `rtc_synchronized` (`1` = `epoch_ms` é UTC válido via `DS3231SN#`; `0` = modo degradado monotônico) |
| `1` | `0x02` | `gps_has_fix` (`1` = GPS `Neo-8M` com fix válido no instante da amostra) |
| `2..7` | `0xFC` | Reservados (`0`) |

#### `adc_valid_mask` (Offset `26..27`, `uint16_t`)
- Bit `i` (`i = 0..9`, máscara `1 << i`): quando `1`, indica que `adc_physical[i]` contém amostra válida dentro da taxa configurada; quando `0`, o canal ainda não teve leitura válida.

#### `can_node_online_mask` (Offset `28`, `uint8_t`)
| Bit | Máscara | Enum C++ (`CanNodeId`) | Nó / ID CAN Monitorado (`timeout <= 200 ms` padrão) |
| :---: | :---: | :--- | :--- |
| `0` | `0x01` | `kFrontControls` | Nó Freio e Direção (`0x100`) |
| `1` | `0x02` | `kWheelFl` | Nó Roda Dianteira Esquerda (`0x200`) |
| `2` | `0x04` | `kWheelFr` | Nó Roda Dianteira Direita (`0x201`) |
| `3` | `0x08` | `kWheelRl` | Nó Roda Traseira Esquerda (`0x202`) |
| `4` | `0x10` | `kWheelRr` | Nó Roda Traseira Direita (`0x203`) |
| `5` | `0x20` | `kImuWit` | Nó IMU WIT (`0x300` / `0x301`) |
| `6` | `0x40` | `kEcuFuelTech` | ECU FuelTech (`0x600..0x602` / `0x14080600..0x14080602`) |
| `7` | `0x80` | — | Reservado (`0`) |

> **Recomendação para o Software Decodificador:** Se o bit correspondente de um nó estiver em `0` em `can_node_online_mask`, o último valor conhecido permanece no registro para evitar saltos numéricos abruptos, mas o decodificador pode sinalizar *Node Offline / Stale* ou mascarar com `NaN` caso o usuário selecione modo estrito.

---

## 6. Strings de Empacotamento Prontas (*Reference Formats*)

### 6.1 Python (`struct`)
- **Cabeçalho (`256 B`):**
  - String: **`<4sHHHHHBBQQ160s32s28sI`**
  - `struct.calcsize("<4sHHHHHBBQQ160s32s28sI") == 256`
  - Para os nomes ADC: fatiar os `160` bytes em `10` blocos de `16` bytes (`tags_blob[i*16 : (i+1)*16].split(b"\x00", 1)[0].decode("ascii")`).
- **Registro (`256 B`):**
  - String: **`<HBBIQQHHBBHdd4f10f4f16f6f9fHhHHI`**
  - `struct.calcsize("<HBBIQQHHBBHdd4f10f4f16f6f9fHhHHI") == 256`

### 6.2 Go (`encoding/binary`)
Structs com campos ordenados exatamente sem *padding* implícito (ou decodificados campo a campo via `binary.LittleEndian` + `math.Float32frombits` / `math.Float64frombits`):

```go
type FileHeaderV1 struct {
    Magic            [4]byte
    FormatVersion    uint16
    HeaderSize       uint16
    RecordSize       uint16
    HeaderFlags      uint16
    NominalRateHz    uint16
    AdcChannelCount  uint8
    CanNodeCount     uint8
    StartMonotonicMs uint64
    StartEpochMs     uint64
    AdcChannelTags   [10][16]byte
    DeviceID         [32]byte
    Reserved         [28]byte
    HeaderCRC32      uint32
}

type TelemetryRecordV1 struct {
    SyncWord            uint16
    RecordType          uint8
    StatusFlags         uint8
    SampleSeq           uint32
    MonotonicMs         uint64
    EpochMs             uint64
    LapBeacon           uint16
    AdcValidMask        uint16
    CanNodeOnlineMask   uint8
    GpsSatellites       uint8
    ImuStatusFlags      uint16
    GpsLatitudeDeg      float64
    GpsLongitudeDeg     float64
    GpsAltitudeM        float32
    GpsSpeedKmh         float32
    GpsSpeedMps         float32
    GpsCourseDeg        float32
    AdcPhysical         [10]float32
    SteeringAngleDeg    float32
    BrakePressFrontBar  float32
    BrakePressRearBar   float32
    BrakePedalPosPct    float32
    Wheels              [4][4]float32 // FL, FR, RL, RR -> Damper, Spd, Load, DiscTemp
    ImuAxes             [6]float32    // Ax, Ay, Az, Gx, Gy, Gz
    EcuFloats           [9]float32    // TPS, MAP, AirT, EngT, OilP, FuelP, WatP, Lambda, OilT
    EcuRpm              uint16
    EcuGear             int16
    EcuPitLimitStatus   uint16
    Reserved            uint16
    RecordCRC32         uint32
}
```
*(Em C++, Python ou Go, confirme sempre em tempo de inicialização que `header_size == 256` e `record_size == 256`.)*

---

## 7. Algoritmo Normativo de Decodificação e Recuperação de Falhas

O software decodificador **deve** seguir o fluxo abaixo para garantir leitura confiável mesmo quando a bateria do veículo é desligada pelo *Master Switch* no meio de uma gravação:

1. **Validação do Cabeçalho (`0..255`):**
   - Ler no mínimo `256` bytes do arquivo. Se menor que `256`, abortar arquivo inválido.
   - Verificar `magic == b"PDLG"`.
   - Calcular `CRC-32 IEEE` sobre `file_bytes[0:252]` e comparar com `header_crc32` (`file_bytes[252:256]`). Rejeitar arquivo se falhar.
   - Verificar `format_version == 1`. Caso contrário, abortar informando versão não suportada.
   - Ler `header_size` (`256`) e `record_size` (`256`), e extrair `adc_channel_tags[0..adc_channel_count-1]` para nomear dinamicamente os 10 canais locais.
2. **Varredura de Registros com Ressincronização Automática:**
   - Inicializar `cursor = header_size`.
   - Enquanto `cursor + record_size <= len(file_bytes)`:
     1. Ler `chunk = file_bytes[cursor : cursor + record_size]`.
     2. Verificar assinatura de quadro: `chunk[0] == 0x5A`, `chunk[1] == 0xA5` (`sync_word == 0xA55A` LE) e `chunk[2] == 0x01` (`record_type`).
     3. Se falhar assinatura, incrementar `resync_bytes_skipped += 1`, fazer `cursor += 1` e voltar ao passo 2 (isso ressincroniza automaticamente após um bloco de lixo no meio do arquivo).
     4. Calcular `CRC-32 IEEE` sobre `chunk[0 : record_size - 4]` e comparar com `record_crc32` em `chunk[record_size - 4 : record_size]`.
     5. Se o CRC falhar (falso positivo de `0x5A 0xA5` em dados float ou bit corrompido), incrementar `crc_errors += 1`, fazer `cursor += 1` e voltar ao passo 2.
     6. Se o CRC bater: decodificar todos os campos, emitir o registro válido e avançar `cursor += record_size`.
   - Ao atingir `cursor + record_size > len(file_bytes)`: quaisquer bytes restantes (`< 256 B`) representam um registro truncado no corte de energia; contabilizar `trailing_truncated_bytes = len(file_bytes) - cursor` e descartar silenciosamente com aviso no log do conversor.
3. **Derivação de Tempo e Detecção de Perda de Amostras (*Drops*):**
   - **Tempo relativo da sessão (`Time_s`):** `(record.monotonic_ms - first_record.monotonic_ms) / 1000.0` (ou relativo a `start_monotonic_ms` do cabeçalho).
   - **Detecção de lacunas:** Para registros consecutivos `k-1` e `k`, calcular `gap = record[k].sample_seq - record[k-1].sample_seq`. Se `gap > 1`, registrar `dropped_samples += (gap - 1)` no relatório de diagnóstico da sessão.

---

## 8. Guia de Exportação para Ferramentas de Análise (CSV / LD / Telemetry)

Ao converter o `.bin` recuperado para `.csv` legível pelo **MoTeC i2**, **AiM Race Studio Analysis** ou **MegaLogViewer**:

- **Primeira coluna temporal recomendada:** `Time` em **segundos** (`(monotonic_ms - first_monotonic_ms) / 1000.0`, precisão `.3f` para `1 ms`), mantendo `Timestamp_ms`, `Epoch_ms`, `SampleSeq` e `CanOnlineMask` como canais auxiliares de diagnóstico.
- **Canais locais dinâmicos:** Usar os nomes extraídos de `adc_channel_tags[0..9]` diretamente como cabeçalhos de coluna (ex.: `OilPressure`, `OilTemp`, `FuelPressure`, `Lambda`, etc.).
- **Unidades recomendadas na segunda linha de cabeçalho (padrão MoTeC i2 CSV):**
  - Tempo: `s` / `ms`
  - Velocidades: `km/h` e `m/s`
  - Pressões (Freio, Óleo, Combustível, MAP, Água): `bar`
  - Temperaturas (Motor, Óleo, Ar, Discos de Freio): `C`
  - Ângulos (Direção, GPS Course): `deg`
  - Curso de suspensão: `mm`
  - Células de carga: `N`
  - Acelerações (`Ax/Ay/Az`): `m/s^2` (ou dividir por `9.80665` caso deseje exportar canais adicionais em `G`)
  - Giroscópio (`Gx/Gy/Gz`): `deg/s`
  - Rotação do motor: `rpm`; TPS/Pedal: `%`; Sonda: `lambda`.

---

## 9. Checklist de Validação de Software Decodificador

Antes de liberar uma versão do decodificador PC para a equipe:
1. Validar a função `CRC-32` com `"123456789"` $\rightarrow$ `0xCBF43926`.
2. Validar o tamanho total das structs (`Header == 256 bytes`, `Record == 256 bytes`) e `magic == "PDLG"`.
3. Testar recuperação após corrupção simulada (inserir 7 bytes aleatórios entre 2 registros e truncar os últimos 100 bytes do arquivo $\rightarrow$ deve recuperar `100%` dos registros íntegros antes/depois do bloco corrompido e descartar apenas o tail truncado).
4. Conferir valores negativos (`ecu_gear == -1`, `gps_latitude_deg < 0`, `steering_angle_deg < 0`, `imu_accel_y_mps2 < 0`).
