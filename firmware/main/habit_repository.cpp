#include "habit_repository.hpp"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#ifdef ESP_PLATFORM
#include "esp_spiffs.h"
#include "sdkconfig.h"
#endif

namespace el_sistema {
bool HabitRepository::read(const std::string &path, HabitDocument &doc, bool &missing, std::string &error) {
    missing = false;
    FILE *f = std::fopen(path.c_str(), "rb");
    if (!f) {
        missing = errno == ENOENT;
        error = missing ? "" : "No se pudo leer el almacenamiento de hábitos";
        return false;
    }
    if (std::fseek(f, 0, SEEK_END) != 0) {
        std::fclose(f);
        error = "No se pudo leer hábitos";
        return false;
    }
    long bytes = std::ftell(f);
    if (bytes <= 0 || bytes > 2 * 1024 * 1024) {
        std::fclose(f);
        error = "Tamaño de documento de hábitos inválido";
        return false;
    }
    std::rewind(f);
    std::string json(static_cast<size_t>(bytes), '\0');
    const bool ok = std::fread(json.data(), 1, json.size(), f) == json.size();
    std::fclose(f);
    if (!ok) {
        error = "Lectura de hábitos incompleta";
        return false;
    }
    return habit_json_decode(json, doc, error);
}
bool HabitRepository::load(HabitDocument &doc, std::string &message) {
#ifdef ESP_PLATFORM
    size_t total = 0, used = 0;
    if (esp_spiffs_info(CONFIG_BSP_SPIFFS_PARTITION_LABEL, &total, &used) != ESP_OK) {
        message = "SPIFFS no está disponible. No se modificaron los datos.";
        return false;
    }
#endif
    bool absent = false, backup_absent = false;
    if (read(directory_ + "/habits.json", doc, absent, message)) {
        primary_valid_ = true;
        return true;
    }
    if (read(directory_ + "/habits.bak", doc, backup_absent, message)) {
        primary_valid_ = false;
        message = "Hábitos recuperados desde el respaldo";
        return true;
    }
    if (absent && backup_absent) {
        doc = HabitDocument{};
        message.clear();
        return true;
    }
    message = "No se pudieron recuperar los hábitos. Se conservaron los archivos; escritura bloqueada.";
    return false;
}
bool HabitRepository::warning() const {
#ifdef ESP_PLATFORM
    size_t total = 0, used = 0;
    return esp_spiffs_info(CONFIG_BSP_SPIFFS_PARTITION_LABEL, &total, &used) == ESP_OK && total &&
           used >= total * 8 / 10;
#else
    return false;
#endif
}
bool HabitRepository::save(const HabitDocument &doc, std::string &error) {
    std::string json;
    if (!habit_json_encode(doc, json, error))
        return false;
    const std::string main = directory_ + "/habits.json", tmp = directory_ + "/habits.tmp",
                      bak = directory_ + "/habits.bak";
#ifdef ESP_PLATFORM
    size_t total = 0, used = 0;
    if (esp_spiffs_info(CONFIG_BSP_SPIFFS_PARTITION_LABEL, &total, &used) != ESP_OK || used > total ||
        json.size() + 4096 > total - used) {
        error = "No hay espacio para guardar hábitos de forma segura. Los datos anteriores siguen intactos.";
        return false;
    }
#endif
    FILE *f = std::fopen(tmp.c_str(), "wb");
    if (!f) {
        error = "No se pudo abrir el temporal de hábitos";
        return false;
    }
    bool ok = std::fwrite(json.data(), 1, json.size(), f) == json.size();
    ok = (std::fflush(f) == 0) && ok;
    ok = (::fsync(::fileno(f)) == 0) && ok;
    ok = (std::fclose(f) == 0) && ok;
    if (!ok) {
        error = "No se pudo guardar hábitos. No se confirmó el cambio.";
        return false;
    }
    // Verify before rotating files. On backup recovery never replace the valid
    // backup with the damaged primary. A power loss at either rename is recoverable.
    HabitDocument verified;
    bool missing = false;
    if (!read(tmp, verified, missing, error))
        return false;
    if (primary_valid_) {
        if (std::remove(bak.c_str()) != 0 && errno != ENOENT) {
            error = "No se pudo preparar el respaldo";
            return false;
        }
        if (std::rename(main.c_str(), bak.c_str()) != 0) {
            error = "No se pudo conservar el respaldo";
            return false;
        }
    }
    if (std::rename(tmp.c_str(), main.c_str()) != 0) {
        error = "No se pudo activar el documento. El respaldo sigue disponible.";
        primary_valid_ = false;
        return false;
    }
    primary_valid_ = true;
    error.clear();
    return true;
}
} // namespace el_sistema
