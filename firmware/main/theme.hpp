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

// Inicio: negro puro y partículas de luz. Conserva iconos compactos de teléfono,
// sin trasladar sus halos al HUD de Productividad o Asistente.
constexpr uint32_t HomeBackground = 0x000000;
constexpr uint32_t HomeSurface = 0x10161F;
constexpr uint32_t HomeInk = 0xF0F7FF;
constexpr uint32_t HomeMuted = 0xA7B4C4;
constexpr uint32_t HomeIconInk = 0x17202B; // pictogramas sobre iconos claros
constexpr uint32_t HomeBlue = 0x6FC2FF;
constexpr uint32_t HomeCanary = 0xFFDE00;
constexpr uint32_t HomeSand = 0x111A26;
constexpr uint32_t HomeProductivityPressed = 0xEAF6FF;
constexpr uint32_t HomeAssistantPressed = 0xFFF9D9;
constexpr uint32_t HomeLightCore = 0xFFFDF8;
constexpr uint32_t HomeParticleCyan = 0xB6ECFF;
constexpr uint32_t HomeParticleLime = 0xFFE6AD; // luz cálida, no verde neón
constexpr uint32_t HomeParticleRose = 0xFFD0E8;
constexpr uint32_t HomeParticleLavender = 0xD9D0FF;

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
