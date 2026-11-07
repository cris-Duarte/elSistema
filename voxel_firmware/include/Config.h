#pragma once
#include <cstddef>
#include <cstdint>

namespace voxel::cfg {
inline constexpr const char* Version="0.1.1";
inline constexpr int WorldX = 64, WorldY = 64, WorldZ = 32;
inline constexpr int ChunkSide = 16, ChunkCount = 16;
inline constexpr std::size_t WorldBytes = WorldX * WorldY * WorldZ;
#ifndef VOXEL_WIDTH
#define VOXEL_WIDTH 160
#define VOXEL_HEIGHT 90
#endif
inline constexpr int Width = VOXEL_WIDTH, Height = VOXEL_HEIGHT;
inline constexpr int ScreenW = 1280, ScreenH = 720;
inline constexpr std::size_t Pixels = Width * Height;
inline constexpr float Pi = 3.14159265358979323846f;
inline constexpr float Fov = 75.f * Pi / 180.f;
inline constexpr float PitchLimit = 85.f * Pi / 180.f;
inline constexpr float ViewDistance = 24.f, Reach = 5.f;
inline constexpr float PhysicsStep = 1.f / 60.f;
inline constexpr float HalfWidth = .3f, PlayerHeight = 1.8f, EyeHeight = 1.62f;
inline constexpr float Speed = 3.5f, Gravity = -18.f, JumpSpeed = 6.f;
inline constexpr int MaxRaySteps = 192;
inline constexpr uint32_t Seed = 0x5A17B10Cu;
static_assert(WorldBytes == 131072);
static_assert(ScreenW % Width == 0 && ScreenH % Height == 0);
static_assert(ScreenW / Width == ScreenH / Height);
}
