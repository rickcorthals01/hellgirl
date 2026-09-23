#pragma once
#include <cmath>

namespace StageOne
{
inline constexpr float HalfExtent = 9000.f;
struct Point { float X, Y; };
inline constexpr Point PlayerStart{-6900.f, 3700.f};
inline constexpr Point Sites[] = {{-2900.f, -4100.f}, {6100.f, -6600.f}, {4500.f, 4500.f}};
inline constexpr int EnemyCounts[] = {4, 5, 9};
inline constexpr Point Road[] = {
    {-6900.f, 3700.f}, {-5600.f, 3550.f}, {-4400.f, 2900.f}, {-3600.f, 1400.f},
    {-3000.f, -1300.f}, {-2200.f, -800.f}, {-1200.f, 1500.f}, {500.f, 2200.f},
    {2200.f, 1800.f}, {4000.f, 1300.f}, {5000.f, 1700.f}, {5600.f, 600.f},
    {5600.f, -1800.f}, {5500.f, -4300.f}, {6300.f, -7200.f}
};

inline float Height(float X, float Y)
{
    const float DX = (X - 3000.f) / 2200.f, DY = (Y - 4500.f) / 2200.f;
    const float Hill = 950.f * std::exp(-(DX * DX + DY * DY));
    const float EdgeX = std::fmax(0.f, std::fabs(X) - 7300.f) / 1700.f;
    const float EdgeY = std::fmax(0.f, std::fabs(Y) - 7300.f) / 1700.f;
    return 65.f * std::sin(X / 1900.f) * std::cos(Y / 1600.f) + Hill
        + 1100.f * (EdgeX * EdgeX + EdgeY * EdgeY);
}

inline float SegmentDistanceSquared(float X, float Y, Point A, Point B)
{
    const float DX = B.X - A.X, DY = B.Y - A.Y;
    const float T = std::fmax(0.f, std::fmin(1.f, ((X - A.X) * DX + (Y - A.Y) * DY) / (DX * DX + DY * DY)));
    const float EX = X - (A.X + T * DX), EY = Y - (A.Y + T * DY);
    return EX * EX + EY * EY;
}

inline bool IsRoad(float X, float Y)
{
    for (unsigned int I = 1; I < sizeof(Road) / sizeof(Road[0]); ++I)
        if (SegmentDistanceSquared(X, Y, Road[I - 1], Road[I]) < 390.f * 390.f) return true;
    return false;
}

inline bool ShouldActivate(bool AlreadyActivated, bool PlayerAlive, float DistanceSquared, float Radius)
{
    return !AlreadyActivated && PlayerAlive && DistanceSquared <= Radius * Radius;
}
}
