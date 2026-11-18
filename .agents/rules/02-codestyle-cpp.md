---
trigger: always_on
---

# Regras de Codestyle C++17 Embarcado (Motorsport)

## 1. Padrão de Nomenclatura
- **Namespaces:** `lowercase` por camada (`datalogger::bsp`, `datalogger::osal`, `datalogger::hal`, `datalogger::lld`, `datalogger::bsw`, `datalogger::sil`).
- **Classes, Structs e Enums:** `PascalCase` (ex: `CanService`, `CanFrame`, `CanBusState`).
- **Valores de `enum class` e Constantes:** prefixo `k` + `PascalCase` (ex: `CanBaudRate::k500Kbps`, `kMaxDlc`).
- **Métodos, Funções e Variáveis Locais:** `snake_case` (ex: `receive_to_queue()`, `rx_timeout_ms`).
- **Atributos Privados de Classe:** `snake_case` com sufixo `_` (ex: `can_hal_`, `tx_count_`).
- **Macros de Hardware (BSP):** `UPPER_SNAKE_CASE` com prefixo `PIN_` e `TAG_` (ex: `PIN_CAN_TX`, `TAG_CAN_TX`).
- **Nomes de Arquivos:** `snake_case` (`.hpp` para headers C++, `.cpp` para implementação, `.h` apenas para headers compatíveis com C como `bsp_pins.h`).

## 2. Segurança e Determinismo em Sistemas Embarcados
- **Zero Alocação Dinâmica (Heap) em Tempo Real:**
  - Proibido usar `new`, `delete`, `malloc`, `free`, `std::vector` ou `std::string` dentro de loops de aquisição (Core 1) ou em estruturas críticas da camada `bsw`/`lld`.
  - Utilize `std::array`, `std::string_view`, `RingBuffer<T, Capacity>` e objetos `static` instanciados na inicialização (`app_main`).
- **Tipos Explícitos:** Prefira `<cstdint>` (`uint8_t`, `uint16_t`, `uint32_t`, `int64_t`) em estruturas de dados, protocolos e fronteiras HAL.
- **Segurança de Tipos:**
  - Use `enum class` em vez de `enum` simples.
  - Use `nullptr` em vez de `NULL` ou `0`.
  - Use `static_cast` em vez de casts estilo C `(int)x`.
- **Modularidade:** Funções curtas, com responsabilidade única e retornos antecipados (*early returns*) para evitar aninhamento profundo.

## 3. Qualidade e Compilação
- **Headers:** Use `#pragma once` no topo de todo header.
- **Documentação:** Toda classe pública e método de interface (`HAL`/`OSAL`/`BSW`) deve ter docstring concisa (`/** @brief ... */`).
- **Zero Warnings:** Todo código em `components/` e `main/` deve compilar sem nenhum aviso com `-Wall -Wextra -Wpedantic` (GCC/Clang).
- **Portabilidade Git:** Nunca grave caminhos absolutos locais da máquina (ex: `C:\Users\...`) em arquivos versionados.
