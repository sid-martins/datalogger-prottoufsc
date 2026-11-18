---
trigger: always_on
---

# Fluxo Obrigatório de TDD e Testes SIL (Software-in-the-Loop)

## 1. Ciclo Red -> Green -> Refactor
Toda nova funcionalidade nas camadas `bsw`, `hal`, `osal` ou `bsp` deve seguir rigorosamente o TDD:
1. **Contrato (`HAL`/`OSAL`):** Defina ou atualize a interface pura em `components/hal/include/` ou `components/osal/include/`.
2. **Dublê SIL (`tests/sil_mocks/`):** Crie ou atualize o Mock (`GMock`) ou Fake correspondente (ex: `MockCanHal`, `FakeOsalQueue`).
3. **Teste Primeiro (`Red`):** Escreva os testes unitários em `tests/test_<modulo>.cpp` cobrindo casos nominais, limites de borda e cenários de falha.
4. **Implementação (`Green`):** Implemente a lógica em `components/bsw/` e o driver físico em `components/lld/`.
5. **Validação:** Execute `.\run_tests.ps1` e confirme **100% dos testes passando**, **0 warnings** e sintaxe de `main/main.cpp` íntegra.

## 2. Stubs do ESP-IDF para Compilação no Host (`tests/esp_idf_stubs/`)
- Como os testes SIL rodam no Host (Windows) sem o toolchain completo do ESP-IDF, os headers de drivers nativos (`driver/twai.h`, `driver/gpio.h`, `freertos/*.h`, `esp_log.h`, `esp_timer.h`) possuem stubs leves em `tests/esp_idf_stubs/include/`.
- Ao criar um novo driver `LLD` (ex: `driver/i2c_master.h`, `esp_adc/adc_oneshot.h`, `driver/uart.h`), adicione apenas as assinaturas mínimas necessárias no respectivo stub em `tests/esp_idf_stubs/include/` para garantir que o `LLD` e o `main/main.cpp` continuem compilando no Host e sem erros no IntelliSense.
- **Importante:** A pasta `tests/esp_idf_stubs/` **nunca** entra no build do `idf.py` para o ESP32-S3.

## 3. Comando Único de Verificação Obrigatória
Antes de concluir qualquer tarefa de código, execute:
```powershell
.\run_tests.ps1
```
*(O script `run_tests.ps1` compila e executa os testes SIL no Google Test, verifica a sintaxe de `main/main.cpp` com `-Wall -Wextra -Wpedantic` e valida o limite de 200 linhas do `GEMINI.md`).*
