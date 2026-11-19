# Guia Rápido de Protocolo CAN — Nós Satélites (`ESP32-C3`) e ECU FuelTech

Este documento é a **referência oficial de IDs, formato de bytes e escalas** para programar o firmware de cada um dos **6 nós satélites `ESP32-C3`** (4x Rodas, 1x Freio/Direção, 1x IMU WIT) e configurar a **ECU FuelTech** conectados ao Datalogger Central (`ESP32-S3`).

- **Arquivo de cabeçalho no firmware central:** [`components/bsw/include/can_protocol.hpp`](../components/bsw/include/can_protocol.hpp)
- **Velocidade do Barramento CAN (Todos os nós):** **`500 kbps`**
- **Tamanho fixo do Payload (Todos os pacotes):** **`DLC = 8 bytes`**

---

## 1. Resumo Rápido de IDs por Nó (Tabela de Bolso)

| Nó Físico no Carro | Microcontrolador | CAN ID (Hex) | Tipo de Frame | Endianness (Ordem dos Bytes) | Taxa Recomendada |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **Nó Freio & Direção** | `ESP32-C3` | **`0x100`** | Standard (`11-bit`) | **Little-Endian** (`LSB` primeiro) | $100\ \text{Hz}$ ($10\ \text{ms}$) |
| **Nó Roda Dianteira Esquerda (`FL`)** | `ESP32-C3` | **`0x200`** | Standard (`11-bit`) | **Little-Endian** (`LSB` primeiro) | $100\ \text{Hz}$ ($10\ \text{ms}$) |
| **Nó Roda Dianteira Direita (`FR`)** | `ESP32-C3` | **`0x201`** | Standard (`11-bit`) | **Little-Endian** (`LSB` primeiro) | $100\ \text{Hz}$ ($10\ \text{ms}$) |
| **Nó Roda Traseira Esquerda (`RL`)** | `ESP32-C3` | **`0x202`** | Standard (`11-bit`) | **Little-Endian** (`LSB` primeiro) | $100\ \text{Hz}$ ($10\ \text{ms}$) |
| **Nó Roda Traseira Direita (`RR`)** | `ESP32-C3` | **`0x203`** | Standard (`11-bit`) | **Little-Endian** (`LSB` primeiro) | $100\ \text{Hz}$ ($10\ \text{ms}$) |
| **Nó IMU WIT — Aceleração** | `ESP32-C3` | **`0x300`** | Standard (`11-bit`) | **Little-Endian** (`LSB` primeiro) | $100\ \text{Hz}$ ($10\ \text{ms}$) |
| **Nó IMU WIT — Giroscópio** | `ESP32-C3` | **`0x301`** | Standard (`11-bit`) | **Little-Endian** (`LSB` primeiro) | $100\ \text{Hz}$ ($10\ \text{ms}$) |
| **ECU FuelTech — Bloco 1 (TPS/MAP/Temp)** | FuelTech | **`0x14080600`** *(ou `0x600`)* | Extended (`29-bit`) | **Big-Endian** (`MSB` primeiro) | Broadcast Nativo |
| **ECU FuelTech — Bloco 2 (Pressões/Marcha)** | FuelTech | **`0x14080601`** *(ou `0x601`)* | Extended (`29-bit`) | **Big-Endian** (`MSB` primeiro) | Broadcast Nativo |
| **ECU FuelTech — Bloco 3 (Lambda/RPM)** | FuelTech | **`0x14080602`** *(ou `0x602`)* | Extended (`29-bit`) | **Big-Endian** (`MSB` primeiro) | Broadcast Nativo |

---

## 2. Mapa Detalhado de Bytes para Programar os Nós `ESP32-C3`

> **Nota para o `ESP32-C3`:** Como a arquitetura RISC-V do `ESP32-C3` (assim como a Xtensa do `ESP32-S3`) já é nativamente **Little-Endian**, empacotar um `uint16_t` ou `int16_t` nos bytes `[0..1]`, `[2..3]`, `[4..5]`, `[6..7]` segue a ordem natural da memória (`byte_baixo = valor & 0xFF`, `byte_alto = (valor >> 8) & 0xFF`).

### 2.1 Nó de Freio e Direção — `ID: 0x100` (`Standard 11-bit`, `DLC = 8`)

| Bytes | Variável | Tipo no Frame | Resolução / Escala | Fórmula no `ESP32-C3` (Envio) | Exemplo |
| :---: | :--- | :---: | :---: | :--- | :--- |
| **`0..1`** | Ângulo da Direção | `int16_t` | $0,1^\circ/\text{bit}$ | `raw = (int16_t)(angulo_deg * 10.0f)` | $-45,5^\circ \rightarrow -455$ |
| **`2..3`** | Pressão de Freio Dianteiro | `uint16_t` | $0,1\ \text{bar}/\text{bit}$ | `raw = (uint16_t)(pressao_bar * 10.0f)` | $38,5\ \text{bar} \rightarrow 385$ |
| **`4..5`** | Pressão de Freio Traseiro | `uint16_t` | $0,1\ \text{bar}/\text{bit}$ | `raw = (uint16_t)(pressao_bar * 10.0f)` | $24,0\ \text{bar} \rightarrow 240$ |
| **`6..7`** | Posição do Pedal de Freio | `uint16_t` | $0,1\%/\text{bit}$ | `raw = (uint16_t)(pedal_pct * 10.0f)` | $82,5\% \rightarrow 825$ |

---

### 2.2 Nós das 4 Rodas (`ESP32-C3`) — `DLC = 8`
Todos os 4 nós de roda usam **exatamente o mesmo código e layout de 8 bytes**, mudando apenas a constante do `CAN_ID`:
- **Roda Dianteira Esquerda (`FL`):** `CAN_ID = 0x200`
- **Roda Dianteira Direita (`FR`):** `CAN_ID = 0x201`
- **Roda Traseira Esquerda (`RL`):** `CAN_ID = 0x202`
- **Roda Traseira Direita (`RR`):** `CAN_ID = 0x203`

| Bytes | Variável | Tipo no Frame | Resolução / Escala | Fórmula no `ESP32-C3` (Envio) | Exemplo |
| :---: | :--- | :---: | :---: | :--- | :--- |
| **`0..1`** | Potenciômetro Linear (Suspensão) | `uint16_t` | $0,1\ \text{mm}/\text{bit}$ | `raw = (uint16_t)(curso_mm * 10.0f)` | $42,5\ \text{mm} \rightarrow 425$ |
| **`2..3`** | Velocidade da Roda | `uint16_t` | $0,1\ \text{km/h}/\text{bit}$ | `raw = (uint16_t)(vel_kmh * 10.0f)` | $95,4\ \text{km/h} \rightarrow 954$ |
| **`4..5`** | Célula de Carga (*Load Cell*) | `int16_t` | $1,0\ \text{N}/\text{bit}$ | `raw = (int16_t)(forca_newtons)` | $1250\ \text{N} \rightarrow 1250$ |
| **`6..7`** | Temperatura do Disco de Freio | `int16_t` | $0,1^\circ\text{C}/\text{bit}$ | `raw = (int16_t)(temp_c * 10.0f)` | $345,0^\circ\text{C} \rightarrow 3450$ |

---

### 2.3 Nó da IMU WIT (`ESP32-C3`) — `IDs: 0x300 e 0x301` (`Standard 11-bit`, `DLC = 8`)

#### Frame 1: Acelerações Lineares Calibradas — `ID: 0x300`
| Bytes | Variável | Tipo no Frame | Resolução / Escala | Fórmula no `ESP32-C3` (Envio) | Exemplo |
| :---: | :--- | :---: | :---: | :--- | :--- |
| **`0..1`** | Aceleração Eixo X (Longitudinal) | `int16_t` | $0,01\ \text{m/s}^2/\text{bit}$ | `raw = (int16_t)(ax_mps2 * 100.0f)` | $-12,50\ \text{m/s}^2 \rightarrow -1250$ |
| **`2..3`** | Aceleração Eixo Y (Lateral) | `int16_t` | $0,01\ \text{m/s}^2/\text{bit}$ | `raw = (int16_t)(ay_mps2 * 100.0f)` | $+18,25\ \text{m/s}^2 \rightarrow 1825$ |
| **`4..5`** | Aceleração Eixo Z (Vertical) | `int16_t` | $0,01\ \text{m/s}^2/\text{bit}$ | `raw = (int16_t)(az_mps2 * 100.0f)` | $+9,81\ \text{m/s}^2 \rightarrow 981$ |
| **`6..7`** | Status / Flags de Calibração IMU | `uint16_t` | Bitmask (`1 = OK`) | `raw = status_imu` | `1` |

#### Frame 2: Giroscópio Calibrado (Velocidades Angulares) — `ID: 0x301`
| Bytes | Variável | Tipo no Frame | Resolução / Escala | Fórmula no `ESP32-C3` (Envio) | Exemplo |
| :---: | :--- | :---: | :---: | :--- | :--- |
| **`0..1`** | Giroscópio Eixo X (*Roll Rate*) | `int16_t` | $0,1^\circ/\text{s}/\text{bit}$ | `raw = (int16_t)(gx_dps * 10.0f)` | $+15,5^\circ/\text{s} \rightarrow 155$ |
| **`2..3`** | Giroscópio Eixo Y (*Pitch Rate*) | `int16_t` | $0,1^\circ/\text{s}/\text{bit}$ | `raw = (int16_t)(gy_dps * 10.0f)` | $-8,2^\circ/\text{s} \rightarrow -82$ |
| **`4..5`** | Giroscópio Eixo Z (*Yaw Rate*) | `int16_t` | $0,1^\circ/\text{s}/\text{bit}$ | `raw = (int16_t)(gz_dps * 10.0f)` | $+45,0^\circ/\text{s} \rightarrow 450$ |
| **`6..7`** | Reservado | `uint16_t` | Fixo `0` | `raw = 0` | `0` |

---

## 3. Exemplo de Código C/C++ Pronto para o Firmware dos Nós `ESP32-C3`

Quando estiver desenvolvendo o firmware de qualquer nó `ESP32-C3`, basta usar esta função auxiliar para montar o `twai_message_t`:

```cpp
#include <cstdint>
#include "driver/twai.h"

// Empacota 4 valores de 16 bits em Little-Endian em um frame CAN Standard (11-bit)
inline twai_message_t pack_node_can_frame(uint32_t standard_id,
                                          uint16_t word0,
                                          uint16_t word1,
                                          uint16_t word2,
                                          uint16_t word3) {
    twai_message_t msg{};
    msg.identifier = standard_id; // Ex: 0x200 (FL), 0x201 (FR), 0x202 (RL), 0x203 (RR), 0x100 (Freio/Dir)
    msg.flags = TWAI_MSG_FLAG_NONE; // Standard 11-bit
    msg.data_length_code = 8;
    msg.data[0] = static_cast<uint8_t>(word0 & 0xFFu);
    msg.data[1] = static_cast<uint8_t>((word0 >> 8u) & 0xFFu);
    msg.data[2] = static_cast<uint8_t>(word1 & 0xFFu);
    msg.data[3] = static_cast<uint8_t>((word1 >> 8u) & 0xFFu);
    msg.data[4] = static_cast<uint8_t>(word2 & 0xFFu);
    msg.data[5] = static_cast<uint8_t>((word2 >> 8u) & 0xFFu);
    msg.data[6] = static_cast<uint8_t>(word3 & 0xFFu);
    msg.data[7] = static_cast<uint8_t>((word3 >> 8u) & 0xFFu);
    return msg;
}
```

---

## 4. Pacotes da ECU FuelTech (`FTCAN 2.0 Simplified Broadcast` — Big-Endian)

O `CanDecoderService` aceita tanto os IDs **Extended 29-bit** nativos da linha PowerFT (`0x14080600..0x14080602` / `0x14000600..0x14000602`, mascarando `id & 0x7FF`) quanto os IDs **Standard 11-bit** (`0x600..0x602`):

| MessageID (`id & 0x7FF`) | Bytes `0..1` (BE) | Bytes `2..3` (BE) | Bytes `4..5` (BE) | Bytes `6..7` (BE) |
| :--- | :--- | :--- | :--- | :--- |
| **`0x600`** (`0x14080600`) | **TPS** (`int16`, $0,1\%$) | **MAP** (`int16`, $0,001\ \text{bar}$) | **Temp. Ar** (`int16`, $0,1^\circ\text{C}$) | **Temp. Motor** (`int16`, $0,1^\circ\text{C}$) |
| **`0x601`** (`0x14080601`) | Pressão Óleo (`int16`, $0,001\ \text{bar}$) | Pressão Comb. (`int16`, $0,001\ \text{bar}$) | Pressão Água (`int16`, $0,001\ \text{bar}$) | **Marcha Atual / Gear** (`int16`, $1$) |
| **`0x602`** (`0x14080602`) | Lambda (`int16`, $0,001\ \lambda$) | **RPM** (`uint16`, $1\ \text{rpm}$) | Temp. Óleo (`int16`, $0,1^\circ\text{C}$) | Pit Limit (`uint16`) |
