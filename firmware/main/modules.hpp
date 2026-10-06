#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace el_sistema {

enum class ModuleId : uint8_t { Productivity = 0, Assistant = 1 };

struct ModuleDescriptor {
    ModuleId id;
    const char* name;
    const char* description;
    const char* action_label;
};

inline constexpr std::array<ModuleDescriptor, 2> kModules = {{
    {ModuleId::Productivity, "PRODUCTIVIDAD", "Organiza misiones, tiempo y progreso.", "ABRIR MÓDULO"},
    {ModuleId::Assistant, "ASISTENTE", "Habla con la entidad visual del Sistema.", "ABRIR MÓDULO"},
}};

inline constexpr const ModuleDescriptor* find_module(ModuleId id)
{
    for (const auto& module : kModules) {
        if (module.id == id) return &module;
    }
    return nullptr;
}

}  // namespace el_sistema
