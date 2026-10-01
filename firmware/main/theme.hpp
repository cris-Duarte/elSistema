#pragma once

#include <cstdint>

namespace el_sistema {
namespace theme {

// Identidad visual del Sistema: fuente unica de verdad para color y metricas.
// Ningun otro archivo define colores literales (ver docs/ui/lineamientos.md).
//
// Reglas que estos valores codifican:
//  - La elevacion se expresa con luminosidad de superficie, nunca con sombras.
//  - El cian es acento: no debe superar ~10% del area de pantalla.
//  - El exito es cian, no verde; el dorado se reserva a logros mayores.

// --- Superficies, de mas profunda a mas elevada ---
constexpr uint32_t Background = 0x050A14;
constexpr uint32_t Surface = 0x0F1A2C;
constexpr uint32_t SurfaceAlt = 0x16243D;
constexpr uint32_t Border = 0x223A5C;

// --- Acentos ---
constexpr uint32_t Accent = 0x00E5FF;      // cian del Sistema
constexpr uint32_t AccentSoft = 0x0C3346;  // relleno tenido de cian
constexpr uint32_t AccentAlt = 0x7C6CFF;   // violeta: estados raros
constexpr uint32_t Danger = 0xFF3B5C;
constexpr uint32_t DangerSoft = 0x3A1524;
constexpr uint32_t Gold = 0xFFC24B;  // solo logros mayores
constexpr uint32_t Scrim = 0x000000;  // velo bajo los dialogos

// --- Texto ---
constexpr uint32_t TextPrimary = 0xE8F2FF;
constexpr uint32_t TextSecondary = 0x7A8CA3;

// --- Escala de rango E-S, derivada de la paleta ---
constexpr uint32_t RankE = TextSecondary;
constexpr uint32_t RankD = 0x4FA3C7;
constexpr uint32_t RankC = Accent;
constexpr uint32_t RankB = 0x6C8CFF;
constexpr uint32_t RankA = AccentAlt;
constexpr uint32_t RankS = Gold;

// --- Metricas ---
constexpr int TouchTargetMin = 48;  // alto minimo de un elemento tactil
constexpr int BodyFontMin = 16;     // cuerpo minimo legible en 5"
constexpr int RadiusPanel = 10;
constexpr int RadiusControl = 8;
constexpr int BorderWidth = 1;

}  // namespace theme
}  // namespace el_sistema
