#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace el_sistema {

enum class ModuleId : uint8_t { Productivity = 0, Assistant = 1, Hourglass = 2, Radar = 3, Tetris = 4, Maze = 5 };

struct ModuleDescriptor {
    ModuleId id;
    const char* name;
    const char* description;
    const char* action_label;
};

inline constexpr std::array<ModuleDescriptor, 6> kModules = {{
    {ModuleId::Productivity, "PRODUCTIVIDAD", "Organiza misiones, tiempo y progreso.", "ABRIR MÓDULO"},
    {ModuleId::Assistant, "ASISTENTE", "Habla con la entidad visual del Sistema.", "ABRIR MÓDULO"},
    {ModuleId::Hourglass, "RELOJ DE ARENA", "Arena de luz y tiempo físico.", "ABRIR MÓDULO"},
    {ModuleId::Radar, "RADAR", "Explora señales Wi-Fi y Bluetooth LE.", "ABRIR MÓDULO"},
    {ModuleId::Tetris, "TETRIS", "Piezas, gestos y sonidos de 8 bits.", "ABRIR MÓDULO"},
    {ModuleId::Maze, "LABERINTO", "Guía una bola inclinando la Tab5.", "ABRIR MÓDULO"},
}};

inline constexpr const ModuleDescriptor* find_module(ModuleId id)
{
    for (const auto& module : kModules) {
        if (module.id == id) return &module;
    }
    return nullptr;
}

}  // namespace el_sistema
