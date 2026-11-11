#pragma once

#include <cstddef>
#include <string_view>

namespace datalogger::hal {

/**
 * @brief Interface de Abstração de Hardware (HAL) para armazenamento não-volátil
 * (ex: Cartão SD via SPI/SDMMC ou Flash interna com FATFS/LittleFS).
 */
class IStorage {
public:
    virtual ~IStorage() = default;

    /**
     * @brief Monta o sistema de arquivos / inicializa a mídia de armazenamento.
     * @return true se pronto para escrita.
     */
    virtual bool mount() = 0;

    /**
     * @brief Adiciona uma linha de texto ao arquivo especificado.
     * @param file_path Caminho do arquivo (ex: "/sdcard/log.csv").
     * @param line Conteúdo da linha a ser gravada.
     * @return true se gravado com sucesso.
     */
    virtual bool append_line(std::string_view file_path, std::string_view line) = 0;

    /**
     * @brief Força o sincronismo dos buffers na mídia física (fsync).
     * @return true se o flush ocorreu sem erros.
     */
    virtual bool sync() = 0;
};

} // namespace datalogger::hal

