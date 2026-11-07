#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace el_sistema {

// Preserve existing identifiers; retired IDs 2, 3, 5, 7, 8 and 9 are not reassigned.
enum class ModuleId : uint8_t { Productivity = 0, Assistant = 1, Tetris = 4, Habits = 6, Camera = 10 };

struct ModuleDescriptor {
    ModuleId id;
    const char* name;
    const char* description;
    const char* action_label;
};

inline constexpr std::array<ModuleDescriptor, 5> kModules = {{
    {ModuleId::Productivity, "PRODUCTIVIDAD", "Organiza misiones, tiempo y progreso.", "ABRIR MÓDULO"},
    {ModuleId::Assistant, "ASISTENTE", "Habla con la entidad visual del Sistema.", "ABRIR MÓDULO"},
    {ModuleId::Tetris, "TETRIS", "Piezas, gestos y sonidos de 8 bits.", "ABRIR MÓDULO"},
    {ModuleId::Habits, "HÁBITOS", "Días cumplidos y periodos continuos, a tu ritmo.", "ABRIR MÓDULO"},
    {ModuleId::Camera, "CÁMARA", "Monitor USB Sony: prueba técnica 720p.", "PROBAR MONITOR"},
}};

inline constexpr const ModuleDescriptor* find_module(ModuleId id)
{
    for (const auto& module : kModules) {
        if (module.id == id) return &module;
    }
    return nullptr;
}

}  // namespace el_sistema
