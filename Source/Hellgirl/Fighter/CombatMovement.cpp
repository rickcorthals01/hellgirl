#include "Fighter/ArenaFighter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

namespace
{
constexpr float MaximumLeapSpeed = 1800.f;
constexpr float NormalDodgeSpeed = 1450.f;
}

float AArenaFighter::GetCombatMomentumSpeed() const
{
    // Remember the launch speed until this swing resolves, even if its lunge
    // reaches an enemy's capsule a little before the attack's contact frame.
    return FMath::Min(MaximumLeapSpeed, FMath::Max(static_cast<float>(GetVelocity().Size2D()),
        AttackClock > 0.f ? AttackMomentumSpeed : 0.f));
}

void AArenaFighter::StartDodgeMomentum()
{
    if (bEnemy || bCounterDodge) return;
    auto* Movement = GetCharacterMovement();
    const bool Airborne = Movement->IsFalling();
    const float Speed = Movement->Velocity.Size2D();
    // Keep aerial dodges controlled even during Caveman Strength. Its long
    // attack lunges are separate from evasive movement.
    const float DodgeScale = Airborne ? 1.f : FMath::Min(GetDashMultiplier(), 1.25f);
    const float LaunchSpeed = Airborne && bAirDodgeBoostUsed
        ? FMath::Min(Speed, MaximumLeapSpeed * DodgeScale)
        : FMath::Clamp(Speed + (Airborne ? 300.f : 180.f), NormalDodgeSpeed * DodgeScale, MaximumLeapSpeed * DodgeScale);
    Movement->Velocity = DodgeDirection * LaunchSpeed + FVector(0.f, 0.f, Movement->Velocity.Z);
    PlayerMomentumSpeedLimit = LaunchSpeed;
    PlayerMomentumClock = .43f;
    bMomentumAirborne = Airborne;
    if (Airborne) bAirDodgeBoostUsed = true;
    // Apply the movement limits before the very next physics tick. Otherwise
    // walking's normal braking could consume the impulse in its first frame.
    UpdatePlayerMomentum(0.f);
}

void AArenaFighter::ResetPlayerMomentum()
{
    GroundDashClock = GroundDashRemaining = GroundDashSpeed = 0.f;
    PlayerMomentumClock = PlayerMomentumSpeedLimit = AttackMomentumSpeed = 0.f;
    bMomentumAirborne = bAirDodgeBoostUsed = false;
    if (bEnemy) return;
    auto* Movement = GetCharacterMovement();
    const auto* Defaults = GetClass()->GetDefaultObject<AArenaFighter>()->GetCharacterMovement();
    Movement->GroundFriction = Defaults->GroundFriction;
    Movement->BrakingDecelerationWalking = Defaults->BrakingDecelerationWalking;
    Movement->FallingLateralFriction = Defaults->FallingLateralFriction;
    Movement->BrakingDecelerationFalling = Defaults->BrakingDecelerationFalling;
    Movement->AirControl = Defaults->AirControl;
    Movement->MaxAcceleration = Defaults->MaxAcceleration;
    // Incoming damage and recovery own the actual velocity. Do not erase an
    // incoming launch impulse while clearing the previous action's momentum.
}

void AArenaFighter::LandedPlayerMomentum()
{
    if (bEnemy) return;
    bAirDodgeBoostUsed = bMomentumAirborne = false;
    PlayerMomentumSpeedLimit = FMath::Min(static_cast<float>(GetVelocity().Size2D()), MaximumLeapSpeed * GetDashMultiplier());
    PlayerMomentumClock = PlayerMomentumSpeedLimit > WalkSpeed ? .18f : 0.f;
}

void AArenaFighter::UpdatePlayerMomentum(float Dt)
{
    if (bEnemy) return;
    auto* Movement = GetCharacterMovement();
    const auto* Defaults = GetClass()->GetDefaultObject<AArenaFighter>()->GetCharacterMovement();
    Movement->GroundFriction = Defaults->GroundFriction;
    Movement->BrakingDecelerationWalking = Defaults->BrakingDecelerationWalking;
    Movement->FallingLateralFriction = Defaults->FallingLateralFriction;
    Movement->BrakingDecelerationFalling = Defaults->BrakingDecelerationFalling;
    Movement->AirControl = Defaults->AirControl;
    Movement->MaxAcceleration = Defaults->MaxAcceleration;
    PlayerMomentumClock = FMath::Max(0.f, PlayerMomentumClock - Dt);
    if (!IsAlive() || KnockdownClock > 0.f || HitClock > 0.f)
    {
        PlayerMomentumClock = 0.f;
        bMomentumAirborne = false;
        return;
    }
    if (AirHangClock > 0.f || bGroundImpactPending)
    {
        // Aerial combos keep their existing hang and dive behaviour.
        bMomentumAirborne = false;
        PlayerMomentumClock = 0.f;
        return;
    }
    const bool Bursting = GroundDashClock > 0.f || (DodgeClock > 0.f && !bCounterDodge);
    if (Movement->IsFalling() && (Bursting || bMomentumAirborne))
    {
        const FVector Horizontal = FVector(Movement->Velocity.X, Movement->Velocity.Y, 0.f).GetClampedToMaxSize(MaximumLeapSpeed * GetDashMultiplier());
        Movement->Velocity.X = Horizontal.X;
        Movement->Velocity.Y = Horizontal.Y;
    }
    const float Speed = Movement->Velocity.Size2D();
    if (Bursting)
    {
        // MovementComponent performs all displacement, sweeps, sliding and
        // floor transitions. Never reapply the impulse after it hits a wall.
        Movement->GroundFriction = 0.f;
        Movement->BrakingDecelerationWalking = 0.f;
        Movement->FallingLateralFriction = 0.f;
        Movement->BrakingDecelerationFalling = 0.f;
        Movement->MaxAcceleration = 0.f;
        Movement->MaxWalkSpeed = FMath::Max(Movement->MaxWalkSpeed, Speed);
        if (Movement->IsFalling())
        {
            bMomentumAirborne = true;
            PlayerMomentumSpeedLimit = FMath::Min(Speed, MaximumLeapSpeed * GetDashMultiplier());
        }
        return;
    }
    if (Movement->IsFalling() && bMomentumAirborne)
    {
        // A collision can only lower the retained speed. Air steering cannot
        // accelerate back to the pre-collision speed or pump extra energy in.
        PlayerMomentumSpeedLimit = FMath::Min(PlayerMomentumSpeedLimit, Speed);
        if (PlayerMomentumSpeedLimit > WalkSpeed)
        {
            Movement->MaxWalkSpeed = FMath::Max(Movement->MaxWalkSpeed, PlayerMomentumSpeedLimit);
            Movement->FallingLateralFriction = .12f;
            Movement->BrakingDecelerationFalling = 80.f;
            Movement->AirControl = .18f;
        }
        else bMomentumAirborne = false;
    }
    else if (Movement->IsMovingOnGround() && PlayerMomentumClock > 0.f
        && Speed > WalkSpeed && !IsBlocking() && !bHeavyHeld
        && (AttackClock <= 0.f || CurrentAttack.Type == FistCombat::Move::ChargedStrike))
    {
        Movement->GroundFriction = 2.f;
        Movement->BrakingDecelerationWalking = 1600.f;
        // Ordinary walking speed remains the acceleration limit, so this
        // short landing/recovery coast always gives way to responsive walking.
    }
}

AArenaFighter* AArenaFighter::FindCombatTarget() const
{
    TArray<AActor*> Fighters;
    UGameplayStatics::GetAllActorsOfClass(this, StaticClass(), Fighters);
    const FVector Facing = Controller ? FRotator(0, Controller->GetControlRotation().Yaw, 0).Vector() : GetActorForwardVector();
    AArenaFighter* Best = nullptr;
    float BestScore = 100000.f;
    for (auto* Actor : Fighters)
    {
        auto* Enemy = Cast<AArenaFighter>(Actor);
        if (!Enemy || !Enemy->bEnemy || !Enemy->IsAlive()) continue;
        const FVector Delta = Enemy->GetActorLocation()-GetActorLocation();
        const float Distance = Delta.Size2D();
        if (Distance > 650.f * GetDashMultiplier() * (CurrentAttack.Type == FistCombat::Move::SwordThrust ? 2.f : 1.f) || FMath::Abs(Delta.Z) > 260.f || FVector::DotProduct(Facing,Delta.GetSafeNormal2D()) < .2f) continue;
        FCollisionQueryParams Query; Query.AddIgnoredActor(this); Query.AddIgnoredActor(Enemy);
        FHitResult Wall;
        if (GetWorld()->LineTraceSingleByChannel(Wall,GetActorLocation(),Enemy->GetActorLocation(),ECC_Visibility,Query)) continue;
        const float Score = Distance + (Enemy == CombatTarget.Get() ? -200.f : 0.f);
        if (Score < BestScore) { Best = Enemy; BestScore = Score; }
    }
    return Best;
}
void AArenaFighter::PrepareCombatMovement()
{
    using FistCombat::Move;
    auto* Movement = GetCharacterMovement();
    AttackMomentumSpeed = FMath::Min(static_cast<float>(Movement->Velocity.Size2D()), MaximumLeapSpeed * GetDashMultiplier());
    GroundDashClock = 0.f;
    if (CurrentAttack.Type == Move::ChargedStrike || CurrentAttack.Type == Move::SwordThrust)
    {
        GroundDashDirection = Controller ? FRotator(0,Controller->GetControlRotation().Yaw,0).Vector() : GetActorForwardVector();
        SetActorRotation(GroundDashDirection.Rotation());
        const float Power = FMath::Clamp((CurrentAttack.Range - 190.f) / 270.f, 0.f, 1.f);
        GroundDashSpeed = FMath::Min(2000.f, 1300.f + 500.f * Power + AttackMomentumSpeed * .2f);
        GroundDashClock = .28f + .1f * Power;
        GroundDashSpeed *= GetDashMultiplier() * (CurrentAttack.Type == Move::SwordThrust ? 2.f : 1.f);
        GroundDashRemaining = GroundDashSpeed * GroundDashClock;
        GroundDashLastLocation = GetActorLocation();
        bTargetedGroundDash = false;
        PlayerMomentumClock = GroundDashClock + .22f;
        PlayerMomentumSpeedLimit = FMath::Min(GroundDashSpeed, MaximumLeapSpeed * GetDashMultiplier());
        AttackMomentumSpeed = PlayerMomentumSpeedLimit;
        Movement->Velocity = GroundDashDirection * GroundDashSpeed + FVector(0.f, 0.f, Movement->Velocity.Z);
        UpdatePlayerMomentum(0.f);
        return;
    }
    if (auto* Target = FindCombatTarget())
    {
        CombatTarget = Target; CombatFocusClock = 3.f;
        const FVector Delta = Target->GetActorLocation()-GetActorLocation();
        SetActorRotation(Delta.GetSafeNormal2D().Rotation());
        const bool Punch = CurrentAttack.Type == Move::RightPunch || CurrentAttack.Type == Move::LeftPunch || CurrentAttack.Type == Move::HeavyPunch || CurrentAttack.Type == Move::DoubleJab || CurrentAttack.Type == Move::SwordSlash || CurrentAttack.Type == Move::SwordBackslash;
        if (Punch && GetCharacterMovement()->IsMovingOnGround() && FMath::Abs(Delta.Z) < 140.f)
        {
            GroundDashDirection = Delta.GetSafeNormal2D();
            GroundDashRemaining = FMath::Clamp(static_cast<float>(Delta.Size2D())-130.f,0.f,320.f * GetDashMultiplier());
            if (GroundDashRemaining <= 1.f) return;
            GroundDashClock = .12f; bTargetedGroundDash = true;
            GroundDashSpeed = GroundDashRemaining / GroundDashClock;
            GroundDashLastLocation = GetActorLocation();
            Movement->Velocity = GroundDashDirection * GroundDashSpeed + FVector(0.f, 0.f, Movement->Velocity.Z);
            UpdatePlayerMomentum(0.f);
        }
    }
}
void AArenaFighter::UpdateCombatMovement(float Dt)
{
    if (bEnemy) return;
    ManualCameraClock = FMath::Max(0.f,ManualCameraClock-Dt);
    CombatFocusClock = FMath::Max(0.f,CombatFocusClock-Dt);
    auto* Target = CombatTarget.Get();
    if (Target && (!Target->IsAlive() || FVector::Dist2D(GetActorLocation(),Target->GetActorLocation()) > 1200.f || CombatFocusClock <= 0.f))
    { CombatTarget.Reset(); Target = nullptr; }
    const bool Charging = bHeavyHeld || (AttackClock > 0.f && CurrentAttack.Type == FistCombat::Move::ChargedStrike);
    auto* PC = Cast<APlayerController>(Controller);
    if (Target && PC && !PC->bShowMouseCursor && !Charging && ManualCameraClock <= 0.f)
    {
        FCollisionQueryParams Query; Query.AddIgnoredActor(this); Query.AddIgnoredActor(Target);
        FHitResult Wall;
        if (!GetWorld()->LineTraceSingleByChannel(Wall,GetActorLocation(),Target->GetActorLocation(),ECC_Visibility,Query))
        {
            FRotator Aim = PC->GetControlRotation();
            Aim.Yaw = (Target->GetActorLocation()-GetActorLocation()).Rotation().Yaw;
            PC->SetControlRotation(FMath::RInterpTo(PC->GetControlRotation(),Aim,Dt,5.f));
        }
    }
    if (GroundDashClock <= 0.f) return;
    auto* Movement = GetCharacterMovement();
    auto StopTargetedDash = [&]()
    {
        GroundDashClock = GroundDashRemaining = 0.f;
        Movement->Velocity.X = Movement->Velocity.Y = 0.f;
    };
    if (DodgeClock > 0.f || KnockdownClock > 0.f || HitClock > 0.f || AttackClock <= 0.f)
    {
        GroundDashClock = 0.f;
        // Damage and dodge can already have installed a replacement impulse.
        if (bTargetedGroundDash && DodgeClock <= 0.f && KnockdownClock <= 0.f && HitClock <= 0.f) StopTargetedDash();
        return;
    }
    if (bTargetedGroundDash && !Movement->IsMovingOnGround())
    {
        StopTargetedDash();
        return;
    }
    GroundDashRemaining = FMath::Max(0.f, GroundDashRemaining - static_cast<float>(FVector::Dist2D(GetActorLocation(), GroundDashLastLocation)));
    GroundDashLastLocation = GetActorLocation();
    if (bTargetedGroundDash)
    {
        if (!Target) { StopTargetedDash(); return; }
        GroundDashRemaining = FMath::Min(GroundDashRemaining,FMath::Max(0.f,static_cast<float>(FVector::Dist2D(GetActorLocation(),Target->GetActorLocation()))-130.f));
    }
    const float Speed = Movement->Velocity.Size2D();
    if (GroundDashRemaining <= 1.f || Speed < 40.f)
    {
        // Targeted jabs stop in striking range. Charges deliberately keep
        // their remaining momentum and may carry Hellgirl over a ledge.
        if (bTargetedGroundDash) StopTargetedDash();
        else GroundDashClock = 0.f;
        return;
    }
    // The movement component has not performed this frame's sweep yet. Count
    // distance already travelled, then cap only its final step; expiring a
    // timer here would discard the last frame (or the whole jab on a hitch).
    GroundDashClock = GroundDashRemaining / FMath::Max(GroundDashSpeed, 1.f);
    if (Dt > UE_SMALL_NUMBER)
    {
        const float AllowedSpeed = GroundDashRemaining / Dt;
        if (Speed > AllowedSpeed)
        {
            Movement->Velocity.X *= AllowedSpeed / Speed;
            Movement->Velocity.Y *= AllowedSpeed / Speed;
        }
    }
    if (bTargetedGroundDash)
    {
        const float NextStep = FMath::Min(GroundDashRemaining, static_cast<float>(Movement->Velocity.Size2D()) * Dt);
        const FVector Ahead = GetActorLocation() + GroundDashDirection * (NextStep + 10.f);
        FCollisionQueryParams Query; Query.AddIgnoredActor(this); Query.AddIgnoredActor(Target);
        FHitResult Floor;
        if (!GetWorld()->LineTraceSingleByChannel(Floor, Ahead, Ahead - FVector(0.f, 0.f, 180.f), ECC_Visibility, Query)
            || Floor.ImpactNormal.Z < Movement->GetWalkableFloorZ())
        {
            StopTargetedDash();
        }
    }
}

#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
void AArenaFighter::RunCombatMovementCheck()
{
#if WITH_DEV_AUTOMATION_TESTS
    if (bEnemy || !FParse::Param(FCommandLine::Get(),TEXT("HellgirlMovementCheck")) || GetWorld()->GetTimeSeconds() < 1.f) return;
    static bool Done = false; if (Done) return; Done = true;
    auto Check = [](bool Good, const TCHAR* Reason) { if (!Good) { UE_LOG(LogTemp,Error,TEXT("MOVEMENT CHECK FAILED: %s"),Reason); FPlatformMisc::RequestExitWithStatus(false,1); } return Good; };
    FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    const FVector Origin = GetActorLocation() + FVector(0.f, 0.f, 3000.f);
    auto MakeBlock = [&](const FVector& Position, const FVector& Scale)
    {
        auto* Block = GetWorld()->SpawnActor<AStaticMeshActor>(Position, FRotator::ZeroRotator, Params);
        Block->SetMobility(EComponentMobility::Movable);
        Block->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
        Block->SetActorScale3D(Scale);
        Block->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
        return Block;
    };
    // Isolate movement from the generated terrain and nearby encounters.
    MakeBlock(Origin - FVector(0.f, 0.f, 140.f), FVector(100.f, 100.f, 1.f));
    auto* Movement = GetCharacterMovement();
    auto ResetAtOrigin = [&]()
    {
        ResetPlayerMomentum();
        DodgeClock = DodgeCooldown = AttackClock = PostDodgeClock = ComboClock = HitClock = KnockdownClock = 0.f;
        Combo = 0; Stamina = 100.f; bCounterDodge = false;
        Energy = MaxEnergy; // Physics fixtures exercise movement, independent of earning special-attack energy.
        FinishAirMove();
        Movement->StopMovementImmediately();
        SetActorLocation(Origin, false, nullptr, ETeleportType::TeleportPhysics);
        Movement->SetMovementMode(MOVE_Walking);
        Movement->bForceNextFloorCheck = true;
        Movement->TickComponent(.016f, LEVELTICK_All, nullptr);
    };
    ResetAtOrigin();
    auto* Enemy = GetWorld()->SpawnActor<AArenaFighter>(GetActorLocation()+FVector(350,150,0),FRotator::ZeroRotator,Params);
    Enemy->MakeEnemy(1,false); Enemy->SetActorTickEnabled(false);
    Controller->SetControlRotation(FRotator(-20,0,0));
    StartAttack(false);
    if (!Check(CombatTarget.Get()==Enemy && GroundDashClock>0.f,TEXT("Punch acquires enemy and starts dash"))) return;
    const FVector Before = GetActorLocation();
    UpdateCombatMovement(.06f);
    if (!Check(GetActorLocation().Equals(Before, .01f), TEXT("Movement setup does not teleport the player"))) return;
    UpdatePlayerMomentum(.06f);
    Movement->TickComponent(.06f, LEVELTICK_All, nullptr);
    if (!Check(FVector::Dist2D(Before,GetActorLocation())>80.f && Controller->GetControlRotation().Yaw>0.f,TEXT("Punch closes gap and camera pans"))) return;
    Dodge();
    if (!Check(GroundDashClock==0.f,TEXT("Dodge cancels punch dash"))) return;
    for (const float FrameTime : {1.f/120.f, 1.f/30.f, .2f})
    {
        ResetAtOrigin();
        Controller->SetControlRotation(FRotator(-20.f,0.f,0.f));
        StartAttack(false);
        const FVector DashStart = GetActorLocation();
        const float ExpectedDistance = GroundDashRemaining;
        for (int32 Frame = 0; Frame < 64 && GroundDashClock > 0.f; ++Frame)
        {
            UpdateCombatMovement(FrameTime);
            UpdatePlayerMomentum(FrameTime);
            Movement->TickComponent(FrameTime, LEVELTICK_All, nullptr);
        }
        const float Distance = FVector::Dist2D(DashStart, GetActorLocation());
        if (!Check(ExpectedDistance > 200.f && FMath::Abs(Distance - ExpectedDistance) < 2.f && GroundDashClock == 0.f,
            TEXT("Jab completes its distance at 120 FPS, 30 FPS, and during a 200ms hitch"))) return;
    }
    // Exercise actual contact timing before each movement sweep, in the same
    // order as gameplay. Travel alone does not prove a lunging punch connects.
    for (const float FrameTime : {1.f/30.f, 1.f/60.f, 1.f/120.f, .2f})
    {
        for (const bool Blocked : {false, true})
        {
            ResetAtOrigin();
            Enemy->ResetAfterRecovery();
            Enemy->Health = 1000.f;
            Enemy->HitClock = Enemy->DodgeClock = 0.f;
            Enemy->SetActorLocation(GetActorLocation() + FVector(450.f, 0.f, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
            Controller->SetControlRotation(FRotator(-20.f,0.f,0.f));
            StartAttack(false);
            AStaticMeshActor* PunchWall = Blocked ? MakeBlock(GetActorLocation() + FVector(225.f,0.f,0.f), FVector(.2f,3.f,3.f)) : nullptr;
            for (int32 Frame = 0; Frame < 120 && AttackClock > 0.f; ++Frame)
            {
                UpdateCombatMovement(FrameTime);
                UpdateAttackTiming(FrameTime);
                UpdatePlayerMomentum(FrameTime);
                Movement->TickComponent(FrameTime, LEVELTICK_All, nullptr);
            }
            const float ExpectedHealth = Blocked ? 1000.f : 1000.f - FistCombat::Select(false, 0, false, false).Damage;
            if (!FMath::IsNearlyEqual(Enemy->Health, ExpectedHealth))
                UE_LOG(LogTemp, Display, TEXT("Punch contact: dt=%f blocked=%d health=%f expected=%f gap=%f"), FrameTime, Blocked, Enemy->Health, ExpectedHealth, FVector::Dist2D(GetActorLocation(), Enemy->GetActorLocation()));
            if (!Check(bHitResolved && FMath::IsNearlyEqual(Enemy->Health, ExpectedHealth),
                TEXT("Lunging punch damages once at 30/60/120 FPS and during a hitch; walls prevent damage"))) return;
            if (PunchWall) PunchWall->Destroy();
        }
    }
    ResetAtOrigin();
    Controller->SetControlRotation(FRotator(-20.f,0.f,0.f));
    StartAttack(false);
    CombatTarget.Reset();
    UpdateCombatMovement(.016f);
    if (!Check(GroundDashClock == 0.f && Movement->Velocity.Size2D() < .1f, TEXT("Losing a jab target stops its impulse"))) return;
    ResetAtOrigin();
    StartAttack(false);
    Movement->SetMovementMode(MOVE_Falling);
    UpdateCombatMovement(.016f);
    if (!Check(GroundDashClock == 0.f && Movement->Velocity.Size2D() < .1f, TEXT("Leaving the ground stops a targeted jab without leaking overspeed"))) return;
    ResetAtOrigin();
    Controller->SetControlRotation(FRotator(-20,90,0));
    PendingCharge=1.f;
    StartAttack(true);
    if (!Check(!bTargetedGroundDash && GroundDashDirection.Equals(FVector::RightVector,.001f),TEXT("Charge follows view instead of enemy"))) return;
    const FVector ChargeStart=GetActorLocation();
    UpdateCombatMovement(.08f);
    UpdatePlayerMomentum(.08f);
    Movement->TickComponent(.08f, LEVELTICK_All, nullptr);
    if (!Check(FMath::Abs(GetActorLocation().X-ChargeStart.X)<1.f && GetActorLocation().Y>ChargeStart.Y+100.f && FMath::Abs(Controller->GetControlRotation().Yaw-90.f)<.1f,TEXT("Charge moves forward without camera retargeting"))) return;
    if (!Check(GroundDashRemaining > 600.f && GetCombatMomentumSpeed() >= 1750.f, TEXT("Full charge has long range and carries impact momentum"))) return;
    auto* Wall = MakeBlock(GetActorLocation() + FVector(0.f,150.f,0.f), FVector(3.f,.2f,3.f));
    const FVector WallStart=GetActorLocation();
    UpdateCombatMovement(.08f);
    UpdatePlayerMomentum(.08f);
    Movement->TickComponent(.08f, LEVELTICK_All, nullptr);
    if (!Check(GetActorLocation().Y<WallStart.Y+115.f,TEXT("Dash cannot pass through wall"))) return;
    const FVector BlockedPosition = GetActorLocation();
    UpdateCombatMovement(.08f);
    UpdatePlayerMomentum(.08f);
    Movement->TickComponent(.08f, LEVELTICK_All, nullptr);
    if (!Check(GetActorLocation().Equals(BlockedPosition, 1.f), TEXT("A blocked dash does not inject velocity again"))) return;
    Wall->Destroy();
    ResetAtOrigin();
    Movement->Velocity = FVector(1000.f,0.f,0.f);
    PendingCharge = 1.f;
    StartAttack(true);
    Movement->SetMovementMode(MOVE_Falling);
    UpdatePlayerMomentum(0.f);
    if (!Check(Movement->Velocity.Size2D() <= MaximumLeapSpeed + .1f && bMomentumAirborne,
        TEXT("Charge leaving a ledge physically clamps air speed"))) return;
    ResetAtOrigin();
    SetActorLocation(Origin + FVector(0.f,0.f,300.f), false, nullptr, ETeleportType::TeleportPhysics);
    SetActorRotation(FRotator::ZeroRotator);
    Controller->SetControlRotation(FRotator(-20.f,0.f,0.f));
    Movement->SetMovementMode(MOVE_Falling);
    Movement->Velocity = FVector(560.f,0.f,300.f);
    const FVector LeapStart = GetActorLocation();
    Dodge();
    if (!Check(bAirDodgeBoostUsed && Movement->Velocity.X >= 1450.f && FMath::IsNearlyEqual(Movement->Velocity.Z, 300.f), TEXT("Jump dodge boosts horizontal momentum without adding flight"))) return;
    for (int32 Frame = 0; Frame < 4; ++Frame)
    {
        DodgeClock = FMath::Max(0.f,DodgeClock-.1f);
        Movement->MaxWalkSpeed = WalkSpeed;
        UpdatePlayerMomentum(.1f);
        Movement->TickComponent(.1f, LEVELTICK_All, nullptr);
    }
    if (!Check(GetActorLocation().X > LeapStart.X + 500.f && Movement->Velocity.Size2D() > 1200.f, TEXT("Leap retains momentum after dodge invulnerability ends"))) return;
    const float AirSpeedBefore = Movement->Velocity.Size2D();
    const float VerticalBefore = Movement->Velocity.Z;
    Stamina = 100.f; DodgeCooldown = 0.f;
    Dodge();
    if (!Check(Movement->Velocity.Size2D() <= AirSpeedBefore + .1f && Movement->Velocity.Z <= VerticalBefore + .1f, TEXT("Repeated air dodges cannot pump horizontal or vertical speed"))) return;
    Landed(FHitResult());
    if (!Check(!bAirDodgeBoostUsed && !bMomentumAirborne, TEXT("Landing resets the next leap"))) return;
    ResetPlayerMomentum();
    if (!Check(Movement->GroundFriction == GetClass()->GetDefaultObject<AArenaFighter>()->GetCharacterMovement()->GroundFriction
        && GroundDashClock == 0.f && PlayerMomentumClock == 0.f, TEXT("Interruption restores normal movement and cancels momentum state"))) return;
    UE_LOG(LogTemp,Display,TEXT("MOVEMENT CHECK PASSED: swept punch/charge movement, camera, dodge cancel, frame-independent jab distance and contact damage, blocked punch, target loss, ground exit, charge range, wall stop, air speed cap, long leap, finite air boost, landing, reset"));
    FPlatformMisc::RequestExitWithStatus(false,0);
#endif
}
