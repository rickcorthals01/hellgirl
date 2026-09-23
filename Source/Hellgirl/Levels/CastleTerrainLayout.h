#pragma once
#include <cmath>

// Shared by the castle collision mesh, prop placement and terrain checks.
// World units are centimetres. Authored encounters remain on level ground.
namespace CastleTerrain
{
inline constexpr float HalfExtentX = 6250.f;
inline constexpr float HalfExtentY = 4750.f;
inline constexpr float GridSpacing = 100.f;
inline constexpr float Pi = 3.14159265358979323846f;

inline float SmoothStep(float Low, float High, float Value)
{
    const float T = std::fmax(0.f, std::fmin(1.f, (Value - Low) / (High - Low)));
    return T * T * (3.f - 2.f * T);
}

inline float PathCenterY(float X)
{
    if (X < -3400.f)
    {
        const float T = std::fmax(0.f, std::fmin(1.f, (X + 5000.f) / 1600.f));
        const float Envelope = std::sin(Pi * T) * std::sin(Pi * T);
        return Envelope * (-300.f + 40.f * std::sin(4.f * Pi * T));
    }
    if (X > 3300.f)
    {
        const float T = std::fmax(0.f, std::fmin(1.f, (X - 3300.f) / 1500.f));
        const float Envelope = std::sin(Pi * T) * std::sin(Pi * T);
        return Envelope * (280.f + 35.f * std::sin(4.f * Pi * T));
    }
    const float T = (X + 3400.f) / 6700.f;
    const float Envelope = std::sin(Pi * T) * std::sin(Pi * T);
    return 125.f * std::sin(3.f * Pi * T) * Envelope;
}

inline float PathBlend(float X, float Y)
{
    const float Width = 280.f + 38.f * std::sin(X / 470.f) + 22.f * std::sin(X / 1070.f);
    const float EdgeDetail = 18.f * std::sin(X / 165.f + std::sin(Y / 240.f));
    return 1.f - SmoothStep(Width - 95.f, Width + 125.f,
        std::fabs(Y - PathCenterY(X)) + EdgeDetail);
}

inline float FlatPadMask(float X, float Y, float CenterX, float CenterY, float Radius)
{
    const float DX = X - CenterX, DY = Y - CenterY;
    return SmoothStep(Radius, Radius + 750.f, std::sqrt(DX * DX + DY * DY));
}

inline float FlatRectangleMask(float X, float Y, float CenterX, float HalfX, float HalfY)
{
    const float OutsideX = std::fmax(0.f, std::fabs(X - CenterX) - HalfX);
    const float OutsideY = std::fmax(0.f, std::fabs(Y) - HalfY);
    return SmoothStep(0.f, 650.f, std::sqrt(OutsideX * OutsideX + OutsideY * OutsideY));
}

inline float Hill(float X, float Y, float CenterX, float CenterY, float Radius, float Amount)
{
    const float DX = (X - CenterX) / Radius, DY = (Y - CenterY) / Radius;
    return Amount * std::exp(-(DX * DX + DY * DY));
}

inline float Height(float X, float Y)
{
    const float EdgeY = 280.f * SmoothStep(1600.f, HalfExtentY, std::fabs(Y));
    const float EdgeX = 180.f * SmoothStep(3600.f, HalfExtentX, std::fabs(X));
    const float Rolls = 38.f * (.5f + .5f * std::sin(X / 870.f + .4f) * std::cos(Y / 960.f));
    const float Banks = Hill(X, Y, -3900.f, -2900.f, 1700.f, 130.f)
        + Hill(X, Y, 1200.f, 3450.f, 1800.f, 150.f)
        + Hill(X, Y, 5100.f, -2300.f, 1600.f, 115.f);
    const float RawHeight = EdgeX + EdgeY + Rolls + Banks;
    // Smooth compression keeps the outer banks below 800 without a clipped
    // plateau, and broad frequencies keep the slopes walkable.
    const float NaturalHeight = 900.f * (1.f - std::exp(-RawHeight / 430.f));

    // Most of the castle courtyard is intentionally roomy and flat. Pads
    // overlap this area and protect every existing spawn, orb and exit.
    float FlatMask = FlatRectangleMask(X, Y, 250.f, 2500.f, 1300.f);
    struct Pad { float X, Y, Radius; };
    constexpr Pad Pads[] = {
        {-5000.f, 0.f, 650.f}, {-1400.f, 0.f, 600.f},
        {0.f, -500.f, 600.f}, {700.f, 500.f, 600.f},
        {1000.f, 0.f, 600.f}, {2100.f, 0.f, 700.f},
        {900.f, 0.f, 500.f}, {4800.f, 0.f, 650.f}
    };
    for (const auto& Pad : Pads)
        FlatMask = std::fmin(FlatMask, FlatPadMask(X, Y, Pad.X, Pad.Y, Pad.Radius));
    // Gate openings remain flat across their full width, with extra margin
    // for interpolation between the 100 cm collision-mesh vertices.
    FlatMask = std::fmin(FlatMask, FlatRectangleMask(X, Y, -3400.f, 300.f, 800.f));
    FlatMask = std::fmin(FlatMask, FlatRectangleMask(X, Y, 3300.f, 300.f, 800.f));
    return std::fmax(0.f, NaturalHeight * FlatMask);
}
}
