#pragma once
#include <cmath>
namespace HellgirlCoins
{
inline constexpr float AttractionRadius = 650.f;
inline constexpr float AttractionSpeed = 230.f;
inline constexpr float CoinRadius = 16.f;
inline bool TouchesCapsule(double DX, double DY, double DZ, double Radius, double HalfHeight)
{
    const double SegmentHalf = std::fmax(0.0, HalfHeight - Radius);
    const double OutsideZ = std::fmax(0.0, std::fabs(DZ) - SegmentHalf);
    return DX * DX + DY * DY + OutsideZ * OutsideZ <= (Radius + CoinRadius) * (Radius + CoinRadius);
}
}
