#pragma once
#include "CoreMinimal.h"

struct FSecondaryMotionSpring
{
    FVector Offset = FVector::ZeroVector;
    FVector Velocity = FVector::ZeroVector;
    void Step(const FVector& Acceleration, float DeltaTime, float Limit, float Inertia)
    {
        if (DeltaTime <= 0.f) return;
        if (DeltaTime > .1f || Acceleration.ContainsNaN()) { Offset = Velocity = FVector::ZeroVector; return; }
        const int32 Steps = FMath::Max(1, FMath::CeilToInt(DeltaTime / (1.f / 240.f)));
        const float Dt = DeltaTime / Steps;
        const FVector Force = -Acceleration.GetClampedToMaxSize(2500.f) * Inertia;
        for (int32 I = 0; I < Steps; ++I)
        {
            Velocity += (Force - Offset * 180.f - Velocity * 22.f) * Dt;
            Offset += Velocity * Dt;
            if (Offset.SizeSquared() > Limit * Limit)
            {
                Offset = Offset.GetClampedToMaxSize(Limit);
                const FVector Normal = Offset.GetSafeNormal();
                Velocity -= Normal * FMath::Max(0.f, FVector::DotProduct(Velocity, Normal));
            }
        }
    }
};
