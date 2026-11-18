---
name: orchestrator
description: >-
  Orquestra o desenvolvimento de novos módulos e funcionalidades do Datalogger ESP32-S3
  seguindo a arquitetura Motorsport, TDD/SIL e o roadmap do projeto. Use esta skill sempre
  que o usuário pedir para implementar uma nova feature, módulo ou fase do roadmap.
---

# Orquestrador de Desenvolvimento (Datalogger ESP32-S3 Motorsport)

Esta skill define o protocolo oficial para o **Agente Orquestrador** planejar, decidir entre execução direta ou delegação paralela (Subagentes/Swarm), evitar colisões de arquivos e validar entregas do Datalogger.

---

## 1. Matriz de Decisão: Execução Direta vs. Subagentes vs. Swarm

Antes de iniciar, escolha o modo de execução adequado para economizar quota e maximizar velocidade:

| Cenário | Modo Recomendado | Como Executar |
| :--- | :--- | :--- |
| **Tarefa única / 1 módulo pequeno** (ex: criar `RtcService` ou ajustar `CanService`) | **Execução Direta** | O próprio agente principal executa o fluxo TDD em sequência sem instanciar subagentes extras. |
| **Pesquisa ampla ou 1 módulo grande (BSW + LLD separados)** | **Subagentes (`invoke_subagent`)** | Use `research` para leitura/mapeamento ou defina os perfis em [references/subagent-profiles.md](./references/subagent-profiles.md) via `define_subagent`. |
| **Múltiplas fases do Roadmap simultâneas** (ex: ADC + GPS + RTC em paralelo) | **Swarm (`/teamwork`)** | Recomende `/teamwork` ou use a tool `swarm` com `workspace: "share"` para isolar cópias de trabalho. |

---

## 2. Protocolo Anti-Colisão em 4 Fases (Contrato → Workers → Integração → Gate)

> **REGRA CRÍTICA:** Dois agentes em paralelo **NUNCA** podem editar o mesmo arquivo (`tests/CMakeLists.txt`, `components/bsw/CMakeLists.txt`, `components/lld/CMakeLists.txt`, `main/main.cpp`, `GEMINI.md`, `docs/ROADMAP.md`).

Sempre que usar subagentes em paralelo, siga estritamente esta divisão:

### Fase A — Contrato e Alinhamento (Somente o Orquestrador)
1. Leia `docs/ROADMAP.md`, `docs/REQUIREMENTS.md` e `components/bsp/include/bsp_pins.h`.
2. Crie **primeiro** o contrato puro da camada HAL (`components/hal/include/i_<modulo>_hal.hpp`).
   - *Por quê?* Com o `.hpp` do contrato já salvo no disco, o agente de `BSW+SIL` e o agente de `LLD` podem trabalhar 100% em paralelo contra o mesmo header sem divergência de assinatura!

### Fase B — Construção Isolada (Direta ou Subagentes Paralelos)
Consulte [references/subagent-profiles.md](./references/subagent-profiles.md) caso delegue para subagentes:
- **Frente 1 (BSW + SIL TDD):**
  - Cria exclusivamente:
    - `tests/sil_mocks/mock_<modulo>_hal.hpp`
    - `tests/test_<modulo>_service.cpp`
    - `components/bsw/include/<modulo>_service.hpp`
    - `components/bsw/src/<modulo>_service.cpp`
  - **Não edita** nenhum `CMakeLists.txt` nem `main.cpp`.
- **Frente 2 (LLD ESP32-S3 + Stubs Host):**
  - Cria exclusivamente:
    - `tests/esp_idf_stubs/include/...` (apenas novos headers stub que ainda não existam)
    - `components/lld/include/<modulo>_lld.hpp`
    - `components/lld/src/<modulo>_lld.cpp`
  - **Não edita** nenhum `CMakeLists.txt` nem `main.cpp`.

### Fase C — Integração e Quality Gate (Somente o Orquestrador)
Após os workers concluírem:
1. Registre os novos `.cpp` nos arquivos compartilhados:
   - `components/bsw/CMakeLists.txt`
   - `components/lld/CMakeLists.txt`
   - `tests/CMakeLists.txt`
2. Integre na camada `ASW` (`main/main.cpp`) respeitando **Core 1 (tempo real)** vs **Core 0 (persistência/telemetria)**.
3. Execute o portão de qualidade único:
   ```powershell
   .\run_tests.ps1
   ```
   *(Este script valida 100% dos testes Google Test, 0 warnings em `bsw`/`lld`, a sintaxe de `main/main.cpp` e o limite de `<= 200` linhas do `GEMINI.md`).*

### Fase D — Sincronização de Documentação (Somente o Orquestrador)
- Atualize o checklist em `docs/ROADMAP.md` e na seção 6 do `GEMINI.md`.
- Atualize o `README.md` se a árvore de arquivos mudou.
