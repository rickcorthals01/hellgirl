#include "Fighter/ArenaFighter.h"
#include "Bosses/BossBehavior.h"
#include "Progress/HellgirlWallet.h"
#include "Fighter/CombatImpactBudget.h"
#include "Levels/ArenaGameMode.h"
#include "Rules/DefenseRules.h"
#include "Rules/CombatEnergyRules.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Fighter/HellgirlAnimInstance.h"
#include "Misc/ConfigCacheIni.h"
#include "Fighter/HellgirlOutfits.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "DrawDebugHelpers.h"
#include "UObject/ConstructorHelpers.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Components/PointLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/PackageName.h"

namespace
{
// Clip names produced by Tools/Animations (clips.json). Idle is the standing loop; Block is kept for the retired block pose.
const TCHAR* const CombatClipNames[] = {
    TEXT("Idle"), TEXT("RightPunch"), TEXT("LeftPunch"), TEXT("DoubleJab"), TEXT("RightKick"), TEXT("LeftKick"), TEXT("LegSweep"),
    TEXT("Headbutt"), TEXT("DodgeSlam"), TEXT("Charge"), TEXT("ChargedStrike"),
    TEXT("AirPunch"), TEXT("AirLeftPunch"), TEXT("AirKick"), TEXT("AirCrashKick"), TEXT("AirSlam"),
    TEXT("Dodge"), TEXT("Hit"), TEXT("Knockdown"), TEXT("Death"), TEXT("Block"),
    TEXT("SwordSlash"), TEXT("SwordBackslash"), TEXT("SwordThrust"), TEXT("SwordSpin")};

const TCHAR* AttackClipName(FistCombat::Move Type)
{
    using FistCombat::Move;
    switch (Type)
    {
    case Move::RightPunch: case Move::HeavyPunch: return TEXT("RightPunch");
    case Move::LeftPunch: case Move::Elbow: return TEXT("LeftPunch");
    case Move::DoubleJab: return TEXT("DoubleJab");
    case Move::RightHeavyKick: case Move::TurningKick: return TEXT("RightKick");
    case Move::LeftHeavyKick: case Move::FollowKick: return TEXT("LeftKick");
    case Move::LegSweep: return TEXT("LegSweep");
    case Move::Headbutt: case Move::DodgeUppercut: return TEXT("Headbutt");
    case Move::DodgeSlam: return TEXT("DodgeSlam");
    case Move::ChargedStrike: case Move::Tackle: case Move::ShoulderThrow: return TEXT("ChargedStrike");
    case Move::AirPunch: return TEXT("AirPunch");
    case Move::AirLeftPunch: return TEXT("AirLeftPunch");
    case Move::AirKick: return TEXT("AirKick");
    case Move::AirCrashKick: return TEXT("AirCrashKick");
    case Move::AirSlam: return TEXT("AirSlam");
    case Move::SwordSlash: return TEXT("SwordSlash");
    case Move::SwordBackslash: return TEXT("SwordBackslash");
    case Move::SwordThrust: return TEXT("SwordThrust");
    case Move::SwordSpin: return TEXT("SwordSpin");
    default: return nullptr;
    }
}

// Fraction of each clip where its hit connects, written to DefaultGame.ini by Tools/Animations/build.ps1.
float ClipContactFraction(const UAnimSequence* Clip)
{
    static TMap<FName, float> Fractions = []()
    {
        TMap<FName, float> Result;
        TArray<FString> Lines;
        GConfig->GetSection(TEXT("HellgirlAnimationContact"), Lines, GGameIni);
        for (const FString& Line : Lines)
        {
            FString Key, Value;
            if (Line.Split(TEXT("="), &Key, &Value)) Result.Add(FName(*Key), FCString::Atof(*Value));
        }
        return Result;
    }();
    const float* Fraction = Clip ? Fractions.Find(Clip->GetFName()) : nullptr;
    return Fraction ? FMath::Clamp(*Fraction, .05f, .95f) : -1.f;
}
}

AArenaFighter::AArenaFighter()
{
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(38.f, 88.f);
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.f, 720.f, 0.f);
    GetCharacterMovement()->bRunPhysicsWithNoController = true;
    GetCharacterMovement()->BrakingDecelerationWalking = 2400.f;
    GetCharacterMovement()->JumpZVelocity = 620.f;
    GetCharacterMovement()->AirControl = 0.45f;
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    bUseControllerRotationYaw = false;
    static ConstructorHelpers::FObjectFinder<UAnimSequence> IdleAsset(TEXT("/Game/Hellgirl/Outfits/Rags/Animations/NeutralIdle.NeutralIdle"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> WalkAsset(TEXT("/Game/Hellgirl/Outfits/Rags/Animations/Walk.Walk"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> RunAsset(TEXT("/Game/Hellgirl/Outfits/Rags/Animations/Run.Run"));
    NeutralIdleAnimation = IdleAsset.Object;
    WalkAnimation = WalkAsset.Object;
    RunAnimation = RunAsset.Object;
    // Combat clips load per outfit in SetOutfit; until then every state uses the neutral pose.
    JumpAnimation = NeutralIdleAnimation;
    for (const TCHAR* Name : CombatClipNames) CombatAnimations.Add(FName(Name), NeutralIdleAnimation);
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> HellgirlMesh(TEXT("/Game/Hellgirl/Outfits/Rags/Rags.Rags"));
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> ArmorAsset(TEXT("/Game/Hellgirl/Outfits/SuccubusArmor/SuccubusArmor.SuccubusArmor"));
    RagsMesh = HellgirlMesh.Object;
    ArmorMesh = ArmorAsset.Object;
    if (HellgirlMesh.Succeeded())
    {
        GetMesh()->SetSkeletalMesh(HellgirlMesh.Object);
        // Imported mesh faces +Y. The character moves along +X.
        GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
        GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -88.f));
        GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    }
    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderBody"));
    Body->SetupAttachment(RootComponent);
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    Body->SetStaticMesh(Sphere.Object);
    Body->SetRelativeLocation(FVector(0.f, 0.f, 5.f));
    Body->SetRelativeScale3D(FVector(0.55f, 0.5f, 0.85f));
    Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderHead"));
    RightHand = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightFist"));
    LeftHand = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftFist"));
    RightFoot = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightFoot"));
    LeftFoot = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftFoot"));
    for (UStaticMeshComponent* Part : {Head.Get(), RightHand.Get(), LeftHand.Get(), RightFoot.Get(), LeftFoot.Get()})
    {
        Part->SetupAttachment(RootComponent);
        Part->SetStaticMesh(Sphere.Object);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetRelativeScale3D(FVector(0.23f));
    }
    Head->SetRelativeLocation(FVector(0.f, 0.f, 65.f));
    Head->SetRelativeScale3D(FVector(0.35f));
    RightHand->SetRelativeLocation(FVector(25.f, 35.f, 25.f));
    LeftHand->SetRelativeLocation(FVector(25.f, -35.f, 25.f));
    RightFoot->SetRelativeLocation(FVector(8.f, 20.f, -62.f));
    LeftFoot->SetRelativeLocation(FVector(8.f, -20.f, -62.f));
    RightFoot->SetRelativeScale3D(FVector(0.4f, 0.22f, 0.25f));
    LeftFoot->SetRelativeScale3D(FVector(0.4f, 0.22f, 0.25f));
    Sword = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderSword"));
    Sword->SetupAttachment(RootComponent);
    Sword->SetStaticMesh(Cube.Object);
    Sword->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Sword->SetRelativeLocation(FVector(65.f, 55.f, 0.f));
    Sword->SetRelativeScale3D(FVector(1.1f, 0.035f, 0.09f));
    Sword->SetVisibility(false);
    AttackFlash = CreateDefaultSubobject<UPointLightComponent>(TEXT("AttackFlash"));
    AttackFlash->SetupAttachment(RightHand);
    AttackFlash->SetLightColor(FLinearColor(1.f, .9f, .55f));
    AttackFlash->SetIntensity(18000.f);
    AttackFlash->SetAttenuationRadius(300.f);
    AttackFlash->SetCastShadows(false);
    AttackFlash->SetVisibility(false);
    EnemyNameLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("EnemyTypeLabel"));
    EnemyNameLabel->SetupAttachment(RootComponent);
    EnemyNameLabel->SetRelativeLocation(FVector(0,0,165));
    EnemyNameLabel->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
    EnemyNameLabel->SetWorldSize(25.f);
    EnemyNameLabel->SetTextRenderColor(FColor(255,210,150));
    EnemyNameLabel->SetVisibility(false);
    CameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraArm"));
    CameraArm->SetupAttachment(RootComponent);
    CameraArm->TargetArmLength = DesiredCameraDistance;
    CameraArm->SocketOffset = FVector(0.f, 65.f, 100.f);
    CameraArm->bUsePawnControlRotation = true;
    CameraArm->SetRelativeRotation(FRotator(-23.f, 0.f, 0.f));
    CameraArm->bEnableCameraLag = true;
    CameraArm->CameraLagSpeed = 12.f;
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(CameraArm);
    Camera->FieldOfView = 85.f;
}

void AArenaFighter::BeginPlay()
{
    Super::BeginPlay();
    Health = MaxHealth;
    // Only forward portal travel supplies energy. Fresh games and restarts
    // begin empty, independently of the persistent coin wallet.
    if (const auto* GM = Cast<AArenaGameMode>(UGameplayStatics::GetGameMode(this)))
        Energy = FMath::Clamp(FCString::Atof(*UGameplayStatics::ParseOption(GM->OptionsString, TEXT("CombatEnergy"))), 0.f, MaxEnergy);
    NormalGravity = GetCharacterMovement()->GravityScale;
    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
    if (GetMesh()->GetSkeletalMeshAsset() && !bEnemy)
    {
        int32 SavedOutfit = 0;
        GConfig->GetInt(TEXT("HellgirlAppearance"), TEXT("Outfit"), SavedOutfit, GGameUserSettingsIni);
        const auto* Wallet=Cast<UHellgirlWallet>(GetGameInstance());
        if (SavedOutfit==4 && (!Wallet || !Wallet->bGoblinQueenOwned)) SavedOutfit=0;
        SetOutfit(SavedOutfit >= 0 && SavedOutfit < HellgirlOutfits::Count ? SavedOutfit : 0);
        GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
        if (NeutralIdleAnimation)
        {
            GetMesh()->SetAnimation(NeutralIdleAnimation);
            ActiveAnimation = NeutralIdleAnimation;
            GetMesh()->Stop();
            GetMesh()->SetPosition(0.f, false);
        }
        for (UStaticMeshComponent* Part : {Body.Get(), Head.Get(), RightHand.Get(), LeftHand.Get(), RightFoot.Get(), LeftFoot.Get()})
            Part->SetVisibility(false);
    }
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        PC->SetControlRotation(FRotator(-23.f, 0.f, 0.f));
        PC->SetInputMode(FInputModeGameOnly());
        PC->bShowMouseCursor = false;
    }
}

bool AArenaFighter::SetOutfit(int32 Outfit)
{
    if (bEnemy || Outfit < 0 || Outfit >= HellgirlOutfits::Count) return false;
    const FString Folder = HellgirlOutfits::Folder(Outfit);
    const FString MeshPath = FString::Printf(TEXT("/Game/Hellgirl/Outfits/%s/%s.%s"), *Folder, *Folder, *Folder);
    USkeletalMesh* OutfitMesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath);
    if (!OutfitMesh) return false;
    TMap<FName, UAnimSequence*> Clips;
    for (const TCHAR* Name : {TEXT("NeutralIdle"), TEXT("Walk"), TEXT("Run")})
    {
        const FString Path = FString::Printf(TEXT("/Game/Hellgirl/Outfits/%s/Animations/%s.%s"), *Folder, Name, Name);
        UAnimSequence* Clip = LoadObject<UAnimSequence>(nullptr, *Path);
        if (!Clip || Clip->GetSkeleton() != OutfitMesh->GetSkeleton()) return false;
        Clips.Add(FName(Name), Clip);
    }
    NeutralIdleAnimation = Clips[TEXT("NeutralIdle")];
    WalkAnimation = Clips[TEXT("Walk")];
    RunAnimation = Clips[TEXT("Run")];
    JumpAnimation = NeutralIdleAnimation;
    // Combat clips built by Tools/Animations/build.ps1. Outfits without a clip (e.g. Frog, or moves
    // not downloaded yet) keep the neutral pose for that state.
    for (const TCHAR* Name : CombatClipNames)
    {
        const FString Path = FString::Printf(TEXT("/Game/Hellgirl/Outfits/%s/Animations/%s.%s"), *Folder, Name, Name);
        UAnimSequence* Clip = FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(Path))
            ? LoadObject<UAnimSequence>(nullptr, *Path) : nullptr;
        CombatAnimations.Add(FName(Name), Clip && Clip->GetSkeleton() == OutfitMesh->GetSkeleton() ? Clip : NeutralIdleAnimation.Get());
    }
    SelectedOutfit = Outfit;
    GetMesh()->SetSkeletalMesh(OutfitMesh);
    // The placeholder blade follows the right hand so sword clips swing it.
    Sword->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("RightHand"));
    Sword->SetRelativeLocation(SwordGripOffset);
    Sword->SetRelativeRotation(SwordGripRotation);
    GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    ActiveAnimation = nullptr;
    UpdatePose(0.f);
    return true;
}

void AArenaFighter::MakeEnemy(int32 Wave, bool Flying)
{
    bEnemy = true;
    EnemyOrbitSign = GetUniqueID() % 2 ? -1.f : 1.f;
    Energy = 0.f;
    GetMesh()->SetVisibility(false);
    for (UStaticMeshComponent* Part : {Body.Get(), Head.Get(), RightHand.Get(), LeftHand.Get(), RightFoot.Get(), LeftFoot.Get()})
        Part->SetVisibility(true);
    bFlyingEnemy = Flying;
    MaxHealth = 55.f + Wave * 12.f;
    Health = MaxHealth;
    AttackDamage = 9.f + Wave * 2.f;
    AttackRange = 165.f;
    WalkSpeed = (210.f + Wave * 18.f) * 1.2f;
    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
    GetCharacterMovement()->MaxFlySpeed = WalkSpeed;
    if (Flying) GetCharacterMovement()->SetMovementMode(MOVE_Flying);
    Body->SetRelativeScale3D(FVector(0.85f, 0.75f, 0.85f));
    Camera->Deactivate();
    CameraArm->SetComponentTickEnabled(false);
}

void AArenaFighter::SetupPlayerInputComponent(UInputComponent* Input)
{
    Super::SetupPlayerInputComponent(Input);
    Input->BindAxis("MoveForward", this, &AArenaFighter::Forward);
    Input->BindAxis("MoveRight", this, &AArenaFighter::Right);
    Input->BindAxis("Turn", this, &AArenaFighter::Turn);
    Input->BindAxis("LookUp", this, &AArenaFighter::Look);
    Input->BindAxis("TurnStick", this, &AArenaFighter::TurnStick);
    Input->BindAxis("LookStick", this, &AArenaFighter::LookStick);
    Input->BindAxis("ZoomWheel", this, &AArenaFighter::ZoomWheel);
    Input->BindAxis("ZoomTrigger", this, &AArenaFighter::ZoomTrigger);
    Input->BindAction("Attack", IE_Pressed, this, &AArenaFighter::Attack);
    Input->BindAction("HeavyAttack", IE_Pressed, this, &AArenaFighter::HeavyAttack);
    Input->BindAction("HeavyAttack", IE_Released, this, &AArenaFighter::ReleaseHeavyAttack);
    Input->BindAction("Dodge", IE_Pressed, this, &AArenaFighter::Dodge);
    Input->BindAction("Jump", IE_Pressed, this, &AArenaFighter::JumpPressed);
    Input->BindAction("Jump", IE_Released, this, &AArenaFighter::JumpReleased);
    Input->BindAction("Walk", IE_Pressed, this, &AArenaFighter::WalkPressed);
    Input->BindAction("Walk", IE_Released, this, &AArenaFighter::WalkReleased);
    Input->BindAction("Special", IE_Pressed, this, &AArenaFighter::Special);
    Input->BindAction("WeaponFists", IE_Pressed, this, &AArenaFighter::EquipFists);
    Input->BindAction("WeaponSword", IE_Pressed, this, &AArenaFighter::EquipSword);
    Input->BindAction("Sprint", IE_Pressed, this, &AArenaFighter::SprintPressed);
    Input->BindAction("Sprint", IE_Released, this, &AArenaFighter::SprintReleased);
    Input->BindAction("Restart", IE_Pressed, this, &AArenaFighter::RestartArena);
}

void AArenaFighter::SetEnemyType(EHellgirlEnemyType Type)
{
    EnemyType = Type;
    const TSubclassOf<UBossBehavior> BossClass = UBossBehavior::ClassFor(Type);
    if (BossBehavior && BossBehavior->GetClass() != BossClass) { BossBehavior->DestroyComponent(); BossBehavior = nullptr; }
    if (BossClass && !BossBehavior)
    {
        BossBehavior = NewObject<UBossBehavior>(this, BossClass);
        BossBehavior->RegisterComponent();
    }
    if (Type == EHellgirlEnemyType::Goblins) { WalkSpeed=390.f; AttackDamage=10.f; }
    if (Type == EHellgirlEnemyType::GoblinQueen) { WalkSpeed=430.f; AttackDamage=20.f; }
    const FString Name = StaticEnum<EHellgirlEnemyType>()->GetNameStringByValue(static_cast<int64>(Type));
    for (int32 I = Tags.Num()-1; I >= 0; --I)
        if (Tags[I].ToString().StartsWith(TEXT("EnemyType."))) Tags.RemoveAt(I);
    Tags.Add(FName(*(TEXT("EnemyType.") + Name)));
#if WITH_EDITOR
    SetActorLabel(Name);
#endif
    EnemyNameLabel->SetText(FText::FromString(Name));
    EnemyNameLabel->SetVisibility(true);
    const FEnemyModelSlot& Model = GetDefault<UHellgirlEnemyModels>()->ForType(Type);
    if (USkeletalMesh* EnemyMesh = Model.Mesh.LoadSynchronous())
    {
        GetMesh()->SetSkeletalMesh(EnemyMesh);
        GetMesh()->SetRelativeLocation(Model.Offset);
        GetMesh()->SetRelativeRotation(Model.Rotation);
        GetMesh()->SetRelativeScale3D(Model.Scale);
        GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
        GetMesh()->SetAnimation(nullptr);
        if (UClass* AnimClass = Model.AnimationBlueprint.LoadSynchronous()) GetMesh()->SetAnimInstanceClass(AnimClass);
        else
        {
            EnemyIdleAnimation = Model.Idle.LoadSynchronous();
            EnemyMoveAnimation = Model.Move.LoadSynchronous();
            EnemyAttackAnimation = Model.Attack.LoadSynchronous();
            EnemyHitAnimation = Model.Hit.LoadSynchronous();
            EnemyDeathAnimation = Model.Death.LoadSynchronous();
            EnemyActiveAnimation = nullptr;
            UpdateEnemyAnimation(0.f);
        }
        GetMesh()->SetVisibility(true);
        for (auto* Part : {Body.Get(), Head.Get(), RightHand.Get(), LeftHand.Get(), RightFoot.Get(), LeftFoot.Get(), Sword.Get()})
            Part->SetVisibility(false);
        if (!Model.AttackFlashSocket.IsNone() && GetMesh()->DoesSocketExist(Model.AttackFlashSocket))
            AttackFlash->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, Model.AttackFlashSocket);
    }
}

void AArenaFighter::Forward(float Value)
{
    if (IsAlive() && DodgeClock <= 0.f && KnockdownClock <= 0.f && Controller)
        AddMovementInput(FRotationMatrix(FRotator(0.f, Controller->GetControlRotation().Yaw, 0.f)).GetUnitAxis(EAxis::X), Value);
}
void AArenaFighter::Right(float Value)
{
    if (IsAlive() && DodgeClock <= 0.f && KnockdownClock <= 0.f && Controller)
        AddMovementInput(FRotationMatrix(FRotator(0.f, Controller->GetControlRotation().Yaw, 0.f)).GetUnitAxis(EAxis::Y), Value);
}
void AArenaFighter::Turn(float Value) { if (FMath::Abs(Value) > .01f) ManualCameraClock = .8f; AddControllerYawInput(Value); }
void AArenaFighter::Look(float Value) { if (FMath::Abs(Value) > .01f) ManualCameraClock = .8f; AddControllerPitchInput(Value); }
void AArenaFighter::TurnStick(float Value) { if (FMath::Abs(Value) > .1f) ManualCameraClock = .8f; AddControllerYawInput(Value * 90.f * GetWorld()->GetDeltaSeconds()); }
void AArenaFighter::LookStick(float Value) { if (FMath::Abs(Value) > .1f) ManualCameraClock = .8f; AddControllerPitchInput(Value * 65.f * GetWorld()->GetDeltaSeconds()); }
void AArenaFighter::ZoomWheel(float Value)
{
    if (bEnemy || !Controller) return;
    if (const APlayerController* PC = Cast<APlayerController>(Controller); PC && PC->bShowMouseCursor) return;
    // Wheel notches are discrete: their distance must not depend on frame rate.
    DesiredCameraDistance = FMath::Clamp(DesiredCameraDistance - Value * 50.f, 200.f, 850.f);
}
void AArenaFighter::ZoomTrigger(float Value)
{
    if (bEnemy || !Controller || FMath::Abs(Value) < .05f) return;
    if (const APlayerController* PC = Cast<APlayerController>(Controller); PC && PC->bShowMouseCursor) return;
    DesiredCameraDistance = FMath::Clamp(DesiredCameraDistance - Value * 300.f * GetWorld()->GetDeltaSeconds(), 200.f, 850.f);
}
void AArenaFighter::JumpPressed()
{
    if (IsAlive() && !bHeavyHeld && AttackClock <= 0.f && DodgeClock <= 0.f && KnockdownClock <= 0.f) Jump();
}
void AArenaFighter::JumpReleased() { StopJumping(); }
bool AArenaFighter::IsBlocking() const
{
    return false; // Blocking was removed from the revised combat design.
}
void AArenaFighter::BlockPressed()
{
    bBlockHeld = true;
    CancelCharge();
    BufferClock = 0.f;
    if (IsBlocking() && Controller) SetActorRotation(FRotator(0.f, Controller->GetControlRotation().Yaw, 0.f));
}
void AArenaFighter::ReleaseMouse()
{
    CancelCharge();
    bBlockHeld = false;
    bSprintHeld = false;
    if (APlayerController* PC = Cast<APlayerController>(Controller))
    {
        PC->bShowMouseCursor = !PC->bShowMouseCursor;
        if (PC->bShowMouseCursor) PC->SetInputMode(FInputModeGameAndUI());
        else PC->SetInputMode(FInputModeGameOnly());
    }
}
void AArenaFighter::PrepareForPause()
{
    CancelCharge();
    BufferClock=0.f;
    bBlockHeld=bSprintHeld=bWalkHeld=false;
}
void AArenaFighter::RestartArena()
{
    if (auto* GM = Cast<AArenaGameMode>(UGameplayStatics::GetGameMode(this))) GM->RestartMap();
}

void AArenaFighter::Attack()
{
    QueueAttack(false);
}

void AArenaFighter::HeavyAttack()
{
    if (auto* GM=Cast<AArenaGameMode>(UGameplayStatics::GetGameMode(this)); GM && GM->bForestHub) return;
    if (!IsAlive() || KnockdownClock > 0.f || HitClock > 0.f || bHeavyHeld) return;
    if (GetCharacterMovement()->IsFalling()) { QueueAttack(true); return; }
    bHeavyHeld = true;
    ChargeClock = 0.f;
}

void AArenaFighter::ReleaseHeavyAttack()
{
    if (!bHeavyHeld) return;
    bHeavyHeld = false;
    PendingCharge = ChargeClock >= .2f ? FMath::Clamp(ChargeClock / FMath::Max(MaxChargeSeconds, .2f), 0.f, 1.f) : 0.f;
    ChargeClock = 0.f;
    QueueAttack(true);
    if (AttackClock <= 0.f && BufferClock <= 0.f) PendingCharge = 0.f;
}

void AArenaFighter::CancelCharge()
{
    bHeavyHeld = false;
    ChargeClock = 0.f;
    PendingCharge = 0.f;
}

void AArenaFighter::QueueAttack(bool Heavy)
{
    if (auto* GM=Cast<AArenaGameMode>(UGameplayStatics::GetGameMode(this)); GM && GM->bForestHub) return;
    if (!IsAlive() || KnockdownClock > 0.f || HitClock > 0.f || bGroundImpactPending || (!Heavy && bHeavyHeld)) return;
    // One queued input near the end of recovery; mashing does not queue a whole combo.
    if (AttackClock > 0.f || DodgeClock > 0.f)
    {
        if (AttackClock <= 0.22f && DodgeClock <= 0.18f)
        {
            BufferClock = 0.24f;
            bBufferedHeavy = Heavy;
        }
        return;
    }
    StartAttack(Heavy);
}

void AArenaFighter::StartAttack(bool Heavy)
{
    const bool Airborne = GetCharacterMovement()->IsFalling();
    if (Airborne && bAirFinisherUsed) return;
    FistCombat::AttackSpec RequestedAttack;
    if (bEnemy)
        RequestedAttack = {FistCombat::Move::EnemyClaw, 1.25f, .6f, AttackDamage, AttackRange, 200.f, 0.f, 0};
    else if (!Airborne && Heavy && PendingCharge > 0.f)
        RequestedAttack = FistCombat::Charged(PendingCharge);
    else
        RequestedAttack = FistCombat::Select(Heavy, Airborne ? AirCombo : (ComboClock > 0.f && bLastComboHeavy == Heavy ? Combo : 0), Airborne, PostDodgeClock > 0.f, SelectedWeapon == 1);
    PendingCharge = 0.f;
    bool bBasicAirFallback = false;
    if (!bEnemy)
    {
        // Normal input stays useful even when the aerial combo reaches its
        // area finisher without enough energy. Repeat a basic kick instead.
        if (RequestedAttack.Type == FistCombat::Move::AirCrashKick && Energy < HellgirlEnergy::Cost(RequestedAttack.Type))
        {
            RequestedAttack = FistCombat::Select(false, 2, true, false);
            RequestedAttack.NextCombo = 0;
            bBasicAirFallback = true;
        }
        if (Energy < HellgirlEnergy::Cost(RequestedAttack.Type)
            && (RequestedAttack.Type == FistCombat::Move::LegSweep || RequestedAttack.Type == FistCombat::Move::SwordSpin))
        {
            RequestedAttack = FistCombat::Select(Heavy, 0, false, false, SelectedWeapon == 1);
            RequestedAttack.NextCombo = 0;
        }
        const float Cost = HellgirlEnergy::Cost(RequestedAttack.Type);
        if (Energy < Cost)
        {
            BufferClock = 0.f;
            MoveLabel = FString::Printf(TEXT("NEED %.1f ENERGY / LAND BASIC HITS"), Cost);
            MoveLabelClock = 1.5f;
            return;
        }
        // Pay once on commitment, before animation, launch or target movement.
        // Holding/queuing is free; a whiff or dodge cancel does not refund it.
        Energy = FMath::Clamp(Energy - Cost, 0.f, MaxEnergy);
    }
    CurrentAttack = RequestedAttack;
    if (!bEnemy) CurrentAttack.Duration /= GetSpeedMultiplier();
    bLastComboHeavy = Heavy;
    bSecondHitResolved = CurrentAttack.Type != FistCombat::Move::DoubleJab;
    JabHitTargets.Reset();
    // The aerial finisher can launch a caught flyer before landing. Create
    // one ledger now, shared by that launch and every later area-hit body.
    ActiveAttackImpactBudget.Reset();
    if (!bEnemy && FistCombat::IsPlayerAreaMove(CurrentAttack.Type)) ActiveAttackImpactBudget = MakeShared<FCombatImpactBudget>();
    bAttackEnergyGranted = false;
    Combo = Airborne ? 0 : CurrentAttack.NextCombo;
    ComboClock = Combo > 0 ? CurrentAttack.Duration + .65f : 0.f;
    PostDodgeClock = 0.f;
    BufferClock = 0.f;
    AttackClock = CurrentAttack.Duration;
    if (!bEnemy)
    {
        UAnimSequence* Clip = FindAttackAnimation(CurrentAttack.Type);
        if (Clip)
        {
            GetMesh()->SetAnimation(Clip);
            ActiveAnimation = Clip;
            GetMesh()->Stop();
            GetMesh()->SetPosition(0.f, false);
        }
    }
    bHitResolved = false;
    if (!bEnemy && Controller)
        SetActorRotation(FRotator(0.f, Controller->GetControlRotation().Yaw, 0.f));
    if (!bEnemy) PrepareCombatMovement();
    using FistCombat::Move;
    switch (CurrentAttack.Type)
    {
    case Move::DoubleJab: MoveLabel = TEXT("4 / 4   DOUBLE LEFT JAB"); break;
    case Move::RightHeavyKick: MoveLabel = SelectedWeapon == 1 ? TEXT("HEAVY RIGHT SLASH") : TEXT("HEAVY RIGHT KICK"); break;
    case Move::LeftHeavyKick: MoveLabel = SelectedWeapon == 1 ? TEXT("HEAVY LEFT SLASH") : TEXT("HEAVY LEFT KICK"); break;
    case Move::DodgeSlam: MoveLabel = TEXT("DODGE FOLLOW-UP / SMALL SLAM"); break;
    case Move::SwordSlash: MoveLabel = TEXT("1 / 4   RIGHT SWORD SLASH"); break;
    case Move::SwordBackslash: MoveLabel = TEXT("2 / 4   LEFT SWORD SLASH"); break;
    case Move::SwordThrust: MoveLabel = TEXT("3 / 4   DASH THRUST"); break;
    case Move::SwordSpin: MoveLabel = TEXT("4 / 4   WHIRLWIND"); break;
    case Move::RightPunch: MoveLabel = Combo == 3 ? TEXT("3 / 4   RIGHT PUNCH") : TEXT("1 / 4   RIGHT PUNCH"); break;
    case Move::LeftPunch: MoveLabel = TEXT("2 / 4   LEFT PUNCH"); break;
    case Move::TurningKick: MoveLabel = TEXT("3 / 4   TURNING KICK"); break;
    case Move::FollowKick: MoveLabel = TEXT("4 / 4   FOLLOW-THROUGH KICK"); break;
    case Move::Headbutt: MoveLabel = TEXT("DODGE FOLLOW-UP   HEADBUTT"); break;
    case Move::Elbow: MoveLabel = TEXT("HEAVY BRANCH   CRITICAL ELBOW"); break;
    case Move::Tackle: MoveLabel = TEXT("HEAVY BRANCH   TACKLE"); break;
    case Move::ShoulderThrow: MoveLabel = TEXT("HEAVY BRANCH   SHOULDER THROW"); break;
    case Move::DodgeUppercut: MoveLabel = TEXT("DODGE FOLLOW-UP   UPPERCUT"); break;
    case Move::LegSweep: MoveLabel = TEXT("4 / 4   LEG SWEEP"); break;
    case Move::AirPunch: MoveLabel = TEXT("AIR 1 / 4   RIGHT PUNCH"); break;
    case Move::AirLeftPunch: MoveLabel = TEXT("AIR 2 / 4   LEFT PUNCH"); break;
    case Move::AirKick: MoveLabel = TEXT("AIR 3 / 4   DOWNWARD PUNCH"); break;
    case Move::AirCrashKick: MoveLabel = TEXT("AIR 4 / 4   CRASH KICK"); break;
    case Move::AirSlam: MoveLabel = TEXT("AIR   SLAM"); break;
    case Move::ChargedStrike: MoveLabel = TEXT("CHARGED AREA STRIKE"); break;
    default: MoveLabel = TEXT("HEAVY PUNCH"); break;
    }
    if (bBasicAirFallback) MoveLabel = TEXT("AIR KICK / BUILD ENERGY FOR CRASH");
    MoveLabelClock = CurrentAttack.Duration + 1.f;
    if (!bEnemy && Airborne) StartAirMove();
}

void AArenaFighter::Dodge()
{
    if (!IsAlive() || Stamina < 30.f || DodgeCooldown > 0.f || KnockdownClock > 0.f) return;
    bSecondHitResolved = true;
    const bool WasSlamming = bGroundImpactPending;
    GroundDashClock = 0.f;
    CancelCharge();
    AttackClock = 0.f;
    bHitResolved = true;
    bGroundImpactPending = false;
    ImpactTimeout = 0.f;
    FinishAirMove();
    GetCharacterMovement()->ClearAccumulatedForces();
    AirTarget.Reset();
    AirCombo = 0;
    // Cancel the dive as well as its landing damage, without granting another air finisher.
    if (WasSlamming)
        GetCharacterMovement()->Velocity.Z = FMath::Max(GetCharacterMovement()->Velocity.Z, -400.f);
    Stamina -= 30.f;
    DodgeClock = 0.25f;
    DodgeCooldown = 0.65f;
    PostDodgeClock = .65f;
    Combo = 0;
    ComboClock = 0.f;
    BufferClock = 0.f;
    MoveLabel = TEXT("DODGE   FOLLOW WITH NORMAL OR HEAVY");
    MoveLabelClock = 1.f;
    DodgeDirection = GetLastMovementInputVector().GetSafeNormal2D();
    if (DodgeDirection.IsNearlyZero()) DodgeDirection = GetActorForwardVector();
    SetActorRotation(DodgeDirection.Rotation());
    bCounterDodge = TryPerfectCounter();
    if (!bCounterDodge) StartDodgeMomentum();
}

bool AArenaFighter::CanCounter(const AArenaFighter* Enemy) const
{
    if (!Enemy || !Enemy->bEnemy || !Enemy->IsAlive() || Enemy->bHitResolved || Enemy->AttackClock <= 0.f) return false;
    // A timed dodge still evades bosses, but must not become an interrupting counter.
    if (Enemy->IsBossAttackArmored()) return false;
    const float UntilHit = Enemy->AttackClock - Enemy->CurrentAttack.Duration * (1.f - Enemy->CurrentAttack.ContactFraction);
    if (UntilHit <= 0.f || UntilHit > PerfectDodgeWindow) return false;
    const FVector Delta = GetActorLocation() - Enemy->GetActorLocation();
    const float Cone = Enemy->EnemyMove == EEnemyMove::CommanderCleave ? -.1f : .25f;
    if (Delta.Size2D() > Enemy->CurrentAttack.Range || FMath::Abs(Delta.Z) > (Enemy->bFlyingEnemy ? 200.f : 140.f)
        || (Enemy->EnemyMove != EEnemyMove::CommanderSlam && FVector::DotProduct(Enemy->GetActorForwardVector(), Delta.GetSafeNormal2D()) < Cone)) return false;
    FHitResult Wall;
    FCollisionQueryParams Query;
    Query.AddIgnoredActor(this); Query.AddIgnoredActor(Enemy);
    return !GetWorld()->LineTraceSingleByChannel(Wall, GetActorLocation(), Enemy->GetActorLocation(), ECC_Visibility, Query);
}

bool AArenaFighter::TryPerfectCounter()
{
    if (bEnemy) return false;
    TArray<AActor*> Fighters;
    UGameplayStatics::GetAllActorsOfClass(this, StaticClass(), Fighters);
    AArenaFighter* Target = nullptr;
    float Soonest = TNumericLimits<float>::Max();
    for (AActor* Actor : Fighters)
    {
        auto* Enemy = Cast<AArenaFighter>(Actor);
        if (!CanCounter(Enemy)) continue;
        const float UntilHit = Enemy->AttackClock - Enemy->CurrentAttack.Duration * (1.f - Enemy->CurrentAttack.ContactFraction);
        if (UntilHit < Soonest) { Target = Enemy; Soonest = UntilHit; }
    }
    if (!Target) return false;
    CombatTarget = Target;
    CombatFocusClock = 3.f;
    const FVector Away = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
    SetActorRotation(Away.Rotation());
    GetCharacterMovement()->Velocity.X = GetCharacterMovement()->Velocity.Y = 0.f;
    const float Before = Target->Health;
    Target->ReceiveHit(45.f * HellgirlDefense::BonusMultiplier(Riposte), Away, 1500.f, 1.2f);
    if (Target->Health >= Before) return false;
    Riposte = 0.f;
    Target->AttackFlash->SetVisibility(false);
    if (Target->IsAlive())
    {
        Target->ApplyCombatLaunch(Away * 1500.f + FVector(0,0,450.f));
    }
    CurrentAttack = FistCombat::Select(false, 0, false, true);
    AttackClock = CurrentAttack.Duration;
    bHitResolved = true; // Damage is immediate and must not repeat at the animation contact.
    MoveLabel = TEXT("PERFECT COUNTER!");
    MoveLabelClock = 1.5f;
    DrawDebugSphere(GetWorld(), Target->GetActorLocation(), 100.f, 16, FColor::Yellow, false, .2f, 0, 5.f);
    return true;
}

void AArenaFighter::ResolveAttack()
{
    if (bEnemy && BossBehavior && BossBehavior->ResolveMove(EnemyMove)) return;
    // One cone-shaped sweep per swing. Each opponent is damaged at most once.
    TArray<AActor*> Fighters;
    UGameplayStatics::GetAllActorsOfClass(this, StaticClass(), Fighters);
    bool bLandedHit = false;
    // Snapshot once: an area move receives the same bonus on each valid target.
    const float StoredRiposte = bEnemy ? 0.f : Riposte;
    const bool Critical = !bEnemy && CurrentAttack.Type == FistCombat::Move::Headbutt && FMath::FRand() < .25f;
    const float Damage = CurrentAttack.Damage * HellgirlDefense::BonusMultiplier(StoredRiposte)
        * ((!bEnemy && UltimateClock > 0.f && ActiveUltimate == 0) ? 1.5f : 1.f) * (Critical ? 1.5f : 1.f);
    if (Critical) MoveLabel += TEXT(" / CRITICAL");
    Fighters.Sort([this](const AActor& A, const AActor& B) { return FVector::DistSquared(A.GetActorLocation(), GetActorLocation()) < FVector::DistSquared(B.GetActorLocation(), GetActorLocation()); });
    const bool PlayerArea = !bEnemy && FistCombat::IsPlayerAreaMove(CurrentAttack.Type);
    if (PlayerArea && !ActiveAttackImpactBudget.IsValid()) ActiveAttackImpactBudget = MakeShared<FCombatImpactBudget>();
    for (AActor* Actor : Fighters)
    {
        AArenaFighter* Other = Cast<AArenaFighter>(Actor);
        if (!Other || Other == this || Other->bEnemy == bEnemy || !Other->IsAlive()) continue;
        FVector Delta = Other->GetActorLocation() - GetActorLocation();
        const bool bAirMove = FistCombat::IsAirStrike(CurrentAttack.Type);
        const bool bAreaAttack = (PlayerArea && !(SelectedWeapon == 1 && FistCombat::IsGroundImpact(CurrentAttack.Type)))
            || (bEnemy && (EnemyMove == EEnemyMove::CommanderSlam || EnemyMove == EEnemyMove::CommanderJumpSlam));
        const float Cone = bEnemy && EnemyMove == EEnemyMove::CommanderCleave ? -.1f : .25f;
        if (Delta.Size2D() > CurrentAttack.Range || FMath::Abs(Delta.Z) > (bAirMove ? 280.f : (bFlyingEnemy ? 200.f : 140.f))
            || (!bAreaAttack && FVector::DotProduct(GetActorForwardVector(), Delta.GetSafeNormal2D()) < Cone)) continue;
        FHitResult Obstacle;
        FCollisionQueryParams Query;
        Query.AddIgnoredActor(this);
        Query.AddIgnoredActor(Other);
        if (GetWorld()->LineTraceSingleByChannel(Obstacle, GetActorLocation(), Other->GetActorLocation(), ECC_Visibility, Query)) continue;
        const float PreviousHealth = Other->Health;
        float LaunchStrength = CurrentAttack.Knockback;
        if (!bEnemy && (CurrentAttack.Knockdown > 0.f || CurrentAttack.Knockback >= 200.f || CurrentAttack.Type == FistCombat::Move::ChargedStrike))
            LaunchStrength = FMath::Clamp(FMath::Max(650.f, LaunchStrength * 1.4f) + GetCombatMomentumSpeed() * .4f, 650.f, 1650.f);
        const float TargetDamage = Damage * (PlayerArea ? FistCombat::AreaDamageScale(Delta.Size2D(),CurrentAttack.Range) : 1.f);
        // Both contacts of the same jab must land even on a long frame or during a speed buff.
        if (CurrentAttack.Type == FistCombat::Move::DoubleJab && bSecondHitResolved && JabHitTargets.Contains(Other)) Other->HitClock = 0.f;
        Other->ReceiveHit(TargetDamage, Delta.GetSafeNormal2D(), LaunchStrength, CurrentAttack.Knockdown,
            PlayerArea ? ActiveAttackImpactBudget : nullptr);
        bLandedHit |= Other->Health < PreviousHealth;
        if (Other->Health < PreviousHealth && CurrentAttack.Type == FistCombat::Move::DoubleJab) JabHitTargets.Add(Other);
        if (CurrentAttack.Type == FistCombat::Move::SwordThrust) break;
    }
    if (!bEnemy)
    {
        if (bLandedHit && !bAttackEnergyGranted && UltimateClock <= 0.f)
        {
            Energy = FMath::Clamp(Energy + HellgirlEnergy::Gain(CurrentAttack.Type), 0.f, MaxEnergy);
            bAttackEnergyGranted = true;
        }
        Riposte = HellgirlDefense::AfterAttack(Riposte, bLandedHit);
        MoveLabel += bLandedHit ? (StoredRiposte > 0.f ? TEXT("  - RIPOSTE HIT") : TEXT("  - HIT")) : TEXT("  - MISS");
    }
    DrawDebugCircle(GetWorld(), GetActorLocation(), CurrentAttack.Range, 28,
        bEnemy ? FColor::Red : (bLandedHit ? FColor::Green : FColor::Cyan), false, 0.12f, 0, 3.f,
        GetActorForwardVector(), GetActorRightVector(), false);
}

void AArenaFighter::UpdateAttackTiming(float Dt)
{
    if (AttackClock <= 0.f) return;
    AttackClock = FMath::Max(0.f, AttackClock - Dt);
    const float Progress = 1.f - AttackClock / CurrentAttack.Duration;
    if (!bHitResolved && !bGroundImpactPending && Progress >= CurrentAttack.ContactFraction)
    {
        // This actor ticks before CharacterMovement's sweep. Let a targeted
        // punch finish its remaining approach before testing the hit cone;
        // otherwise the final movement frame can arrive just after a miss.
        // Blocked/cancelled dashes clear their clock in UpdateCombatMovement.
        if (!bEnemy && bTargetedGroundDash && GroundDashClock > 0.f)
        {
            AttackClock = FMath::Max(AttackClock, UE_SMALL_NUMBER);
            return;
        }
        bHitResolved = true;
        ResolveAttack();
    }
    if (!bSecondHitResolved && bHitResolved && Progress >= .8f)
    {
        bSecondHitResolved = true;
        ResolveAttack();
    }
}

bool AArenaFighter::IsBossAttackArmored() const
{
    return bEnemy && IsAlive() && AttackClock > 0.f
        && (EnemyType == EHellgirlEnemyType::ImpCommander || EnemyType == EHellgirlEnemyType::GulpBoss
            || EnemyType == EHellgirlEnemyType::SuccubusBoss || EnemyType == EHellgirlEnemyType::GoblinQueen);
}

void AArenaFighter::ReceiveHit(float Damage, const FVector& Direction, float Knockback, float Knockdown, TSharedPtr<FCombatImpactBudget> ImpactBudget)
{
    if (!IsAlive() || DodgeClock > 0.f || HitClock > 0.f) return;
    Damage = FilterEnemyDamage(Damage);
    if (Damage <= 0.f) return;
    const bool Armored = IsBossAttackArmored();
    // Direction points away from the attacker, so negate it to test the guarded front.
    const auto Defense = HellgirlDefense::Receive(Damage,
        static_cast<float>(FVector::DotProduct(GetActorForwardVector(), -Direction.GetSafeNormal2D())), IsBlocking(), Riposte);
    Damage = Defense.Damage;
    Riposte = Defense.Meter;
    Health = FMath::Max(0.f, Health - Damage);
    if (Armored || ParalysisClock > 0.f)
    {
        // Damage applies, but no hit timer, animation reset or impulse may
        // cancel the current boss action on this tick or the following one.
        if (!IsAlive()) HandleDeath(Direction.GetSafeNormal2D()*Knockback+FVector(0,0,140));
        return;
    }
    if (Defense.Blocked && IsAlive())
    {
        MoveLabel = TEXT("BLOCKED / RIPOSTE CHARGED");
        MoveLabelClock = 1.f;
        return;
    }
    HitClock = 0.12f;
    if (bEnemy) CancelEnemyMove();
    GroundDashClock = 0.f;
    ResetPlayerMomentum();
    PlayerHitAnimationTime = 0.f;
    EnemyHitAnimationTime = 0.f;
    CancelCharge();
    bGroundImpactPending = false;
    FinishAirMove();
    AttackClock = 0.f;
    BufferClock = 0.f;
    Combo = 0;
    ComboClock = 0.f;
    PostDodgeClock = 0.f;
    bHitResolved = true;
    KnockdownClock = FMath::Max(KnockdownClock, Knockdown);
    PlayerKnockdownDuration = FMath::Max(KnockdownClock, .01f);
    const FVector HitVelocity = Direction.GetSafeNormal2D() * Knockback + FVector(0.f, 0.f, Knockback >= 600.f ? 380.f : (Knockdown > 0.f ? 140.f : 35.f));
    if (!IsAlive())
    {
        HandleDeath(HitVelocity);
    }
    else if (bEnemy && Knockback >= 600.f) ApplyCombatLaunch(HitVelocity,0,nullptr,ImpactBudget);
    else if (Knockback > 0.f || Knockdown > 0.f) LaunchCharacter(HitVelocity, true, true);
}

void AArenaFighter::Tick(float Dt)
{
    Super::Tick(Dt);
    RunBossDesignCheck();
    RunRevisedCombatCheck();
    UpdateUltimate(Dt);
    if (IsAlive() && ParalysisClock > 0.f)
    {
        ParalysisClock = FMath::Max(0.f, ParalysisClock - Dt);
        GetCharacterMovement()->StopMovementImmediately();
        GetCharacterMovement()->ClearAccumulatedForces();
        if (ParalysisClock <= 0.f) GetCharacterMovement()->SetMovementMode(bFlyingEnemy ? MOVE_Flying : MOVE_Falling);
        return;
    }
    if (BossBehavior && bEnemy && IsAlive() && BossBehavior->TickPhases(Dt)) return;
    UpdateCombatPhysics(Dt);
    RunPhysicsCheck(Dt);
    RunEnergyCheck();
    RunAttackCancelCheck(Dt);
    RunPerfectCounterCheck();
    RunEnemyMovesetCheck(Dt);
    RunCombatBalanceCheck(Dt);
    RunAttackAnimationPreview(Dt);
    const float UntilHit = AttackClock - CurrentAttack.Duration * (1.f - CurrentAttack.ContactFraction);
    const bool FlashNow = bEnemy && IsAlive() && !bHitResolved && AttackClock > 0.f && UntilHit > 0.f && UntilHit <= PerfectDodgeWindow;
    AttackFlash->SetVisibility(FlashNow);
    if (FlashNow)
    {
        const FVector P = AttackFlash->GetComponentLocation();
        DrawDebugSphere(GetWorld(), P, 20.f, 10, FColor::White, false, -1.f, 0, 4.f);
        for (const FVector Axis : {FVector::ForwardVector, FVector::RightVector, FVector::UpVector})
            DrawDebugLine(GetWorld(), P - Axis * 55.f, P + Axis * 55.f, FColor(255,240,150), false, -1.f, 0, 3.f);
    }
    if (!bEnemy)
        CameraArm->TargetArmLength = FMath::FInterpTo(CameraArm->TargetArmLength, DesiredCameraDistance, Dt, 12.f);
    if (!IsAlive())
    {
        if (bDeathRagdollActive) return;
        if (bEnemy) UpdateEnemyAnimation(Dt);
        else UpdatePose(Dt);
        return;
    }
    Stamina = FMath::Min(100.f, Stamina + Dt * 23.f);
    UpdateCombatMovement(Dt);
    RunCombatMovementCheck();
    if (bHeavyHeld && AttackClock <= 0.f && DodgeClock <= 0.f)
    {
        if (GetCharacterMovement()->IsFalling()) CancelCharge();
        else
        {
            ChargeClock = FMath::Min(FMath::Max(MaxChargeSeconds, .2f), ChargeClock + Dt);
            MoveLabel = FString::Printf(TEXT("CHARGING %d%%   RELEASE HEAVY"), FMath::RoundToInt(100.f * ChargeClock / FMath::Max(MaxChargeSeconds, .2f)));
            if (Energy < HellgirlEnergy::ChargeCost)
                MoveLabel = FString::Printf(TEXT("CHARGING / NEED %d ENERGY TO RELEASE"), FMath::RoundToInt(HellgirlEnergy::ChargeCost));
            MoveLabelClock = 1.f;
        }
    }
    HitClock = FMath::Max(0.f, HitClock - Dt);
    KnockdownClock = FMath::Max(0.f, KnockdownClock - Dt);
    PostDodgeClock = FMath::Max(0.f, PostDodgeClock - Dt);
    MoveLabelClock = FMath::Max(0.f, MoveLabelClock - Dt);
    if (MoveLabelClock <= 0.f) MoveLabel = SelectedWeapon == 1 ? TEXT("SWORD READY") : TEXT("FISTS READY");
    if (!bHeavyHeld) ComboClock = FMath::Max(0.f, ComboClock - Dt);
    DodgeCooldown = FMath::Max(0.f, DodgeCooldown - Dt);
    if (DodgeClock > 0.f)
    {
        DodgeClock = FMath::Max(0.f, DodgeClock - Dt);
    }
    if (bEnemy) UpdateEnemyMoveMotion(Dt);
    UpdateAttackTiming(Dt);
    if (bGroundImpactPending)
    {
        AttackClock = FMath::Max(AttackClock, .1f);
        ImpactTimeout -= Dt;
        if (ImpactTimeout <= 0.f)
        {
            bGroundImpactPending = false;
            bHitResolved = true;
            AttackClock = 0.f;
            FinishAirMove();
        }
    }
    else if (AirHangClock > 0.f)
    {
        AirHangClock = FMath::Max(0.f, AirHangClock - Dt);
        AirHangBudget = FMath::Max(0.f, AirHangBudget - Dt);
        if (AirHangClock <= 0.f || AirHangBudget <= 0.f) FinishAirMove();
        else if (AirDashClock > 0.f) AirDashClock = FMath::Max(0.f, AirDashClock - Dt);
    }
    if (BufferClock > 0.f && !bGroundImpactPending)
    {
        if (AttackClock <= 0.f && DodgeClock <= 0.f && KnockdownClock <= 0.f && HitClock <= 0.f)
        {
            const bool Heavy = bBufferedHeavy;
            BufferClock = 0.f;
            StartAttack(Heavy);
        }
        else BufferClock = FMath::Max(0.f, BufferClock - Dt);
    }
    UpdatePose(Dt);
    const bool Guarding = IsBlocking();
    const bool Sprinting = !bEnemy && bSprintHeld && !bWalkHeld && !Guarding && AttackClock <= 0.f && !bHeavyHeld && GetCharacterMovement()->IsMovingOnGround();
    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed * GetSpeedMultiplier() * ((!bEnemy && bWalkHeld) ? .4f : (Sprinting ? 1.5f : 1.f)) * ((AttackClock > 0.f || bHeavyHeld || Guarding) ? .3f : 1.f);
    UpdatePlayerMomentum(Dt);
    GetCharacterMovement()->bOrientRotationToMovement = AttackClock <= 0.f && DodgeClock <= 0.f && !bHeavyHeld && !Guarding;
    if (Guarding && Controller)
    {
        SetActorRotation(FRotator(0.f, Controller->GetControlRotation().Yaw, 0.f));
        for (int32 I = -3; I < 3; ++I)
        {
            const FVector A = GetActorLocation() + GetActorForwardVector().RotateAngleAxis(I * 20.f, FVector::UpVector) * 75.f;
            const FVector B = GetActorLocation() + GetActorForwardVector().RotateAngleAxis((I + 1) * 20.f, FVector::UpVector) * 75.f;
            DrawDebugLine(GetWorld(), A, B, FColor::Cyan, false, -1.f, 0, 4.f);
        }
    }
    if (bEnemy)
    {
        if (auto* View = UGameplayStatics::GetPlayerCameraManager(this, 0))
            EnemyNameLabel->SetWorldRotation((View->GetCameraLocation() - EnemyNameLabel->GetComponentLocation()).Rotation());
        if (EnemyMove != EEnemyMove::None) DrawEnemyMoveTelegraph();
        else if (AttackClock > 0.f)
        {
            const bool WindingUp = !bHitResolved;
            const FColor Cue = WindingUp ? FColor::Orange : FColor::Red;
            DrawDebugDirectionalArrow(GetWorld(), GetActorLocation(), GetActorLocation() + GetActorForwardVector() * CurrentAttack.Range, 35.f, Cue, false, -1.f, 0, 4.f);
            if (WindingUp)
                DrawDebugString(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, 165.f), TEXT("ATTACK INCOMING"), nullptr, Cue, 0.f, true, 1.2f);
        }
        if (bFlyingEnemy && KnockdownClock <= 0.f && !bCombatLaunched) GetCharacterMovement()->SetMovementMode(MOVE_Flying);
        AArenaFighter* Player = Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this, 0));
        if (Player && Player->IsAlive() && KnockdownClock <= 0.f && HitClock <= 0.f && !bCombatLaunched
            && (!bGuardHome || FVector::DistSquared2D(Player->GetActorLocation(), HomePosition) < FMath::Square(1000.f)))
        {
            if (BossBehavior) BossBehavior->TickTactics(Dt,Player);
            else if (EnemyType == EHellgirlEnemyType::Goblins) UpdateGoblinTactics(Dt,Player);
            else if (EnemyType == EHellgirlEnemyType::Imps || EnemyType == EHellgirlEnemyType::FlyingImps) UpdateImpTactics(Dt,Player);
            else
            {
            const FVector Delta = Player->GetActorLocation() - GetActorLocation();
            if (AttackClock <= 0.f)
            {
                SetActorRotation(Delta.GetSafeNormal2D().Rotation());
                const FVector Travel = bFlyingEnemy ? Delta + FVector(0.f, 0.f, 150.f) : Delta;
                if (Delta.Size2D() > 135.f || (bFlyingEnemy && FMath::Abs(Travel.Z) > 40.f))
                {
                    FHitResult Floor;
                    FCollisionQueryParams Query; Query.AddIgnoredActor(this); Query.AddIgnoredActor(Player);
                    const FVector Ahead = GetActorLocation() + Travel.GetSafeNormal2D() * 130.f;
                    if (bFlyingEnemy || (GetWorld()->LineTraceSingleByChannel(Floor, Ahead, Ahead - FVector(0,0,250), ECC_Visibility, Query)
                        && Floor.ImpactPoint.Z > -50.f))
                        AddMovementInput(bFlyingEnemy ? Travel.GetSafeNormal() : Travel.GetSafeNormal2D(), 1.f);
                }
                else Attack();
            }
            }
        }
        const FVector Bar = GetActorLocation() + FVector(0.f, 0.f, 115.f);
        DrawDebugLine(GetWorld(), Bar - FVector(0.f, 40.f, 0.f), Bar + FVector(0.f, 40.f, 0.f), FColor(40, 40, 40), false, -1.f, 0, 5.f);
        DrawDebugLine(GetWorld(), Bar - FVector(0.f, 40.f, 0.f), Bar + FVector(0.f, -40.f + 80.f * Health / MaxHealth, 0.f), FColor::Red, false, -1.f, 0, 5.f);
    }
}

void AArenaFighter::UpdateEnemyAnimation(float Dt)
{
    if (bDeathRagdollActive) return;
    if (!EnemyIdleAnimation || GetMesh()->GetAnimationMode() != EAnimationMode::AnimationSingleNode) return;
    EnemyHitAnimationTime += Dt;
    UAnimSequence* Clip = EnemyIdleAnimation.Get();
    const bool Dead = !IsAlive();
    const bool Attacking = !Dead && AttackClock > 0.f && EnemyAttackAnimation;
    const bool Hurt = !Dead && !Attacking && EnemyHitAnimation && EnemyHitAnimationTime < EnemyHitAnimation->GetPlayLength();
    const float Speed = GetVelocity().Size2D();
    const bool Moving = !Dead && !Attacking && !Hurt && Speed > 10.f && KnockdownClock <= 0.f;
    if (Dead && EnemyDeathAnimation) Clip = EnemyDeathAnimation.Get();
    else if (Attacking) Clip = EnemyAttackAnimation.Get();
    else if (Hurt) Clip = EnemyHitAnimation.Get();
    else if (Moving && EnemyMoveAnimation) Clip = EnemyMoveAnimation.Get();
    if (EnemyActiveAnimation != Clip)
    {
        GetMesh()->SetAnimation(Clip);
        GetMesh()->Stop();
        EnemyActiveAnimation = Clip;
        EnemyAnimationTime = 0.f;
    }
    const float Length = FMath::Max(Clip->GetPlayLength(), .01f);
    if (Attacking)
    {
        // The authored claw contact is at 60%, matching EnemyClaw's damage event.
        EnemyAnimationTime = FMath::Clamp(1.f - AttackClock / CurrentAttack.Duration, 0.f, 1.f) * Length;
    }
    else if (Hurt) EnemyAnimationTime = EnemyHitAnimationTime;
    else
    {
        const float Rate = Moving && EnemyType == EHellgirlEnemyType::Imps ? FMath::Clamp(Speed / 224.f, .4f, 1.8f) : 1.f;
        EnemyAnimationTime += Dt * Rate;
        EnemyAnimationTime = Dead ? FMath::Min(EnemyAnimationTime, Length) : FMath::Fmod(EnemyAnimationTime, Length);
    }
    GetMesh()->SetPosition(EnemyAnimationTime, false);
}

UAnimSequence* AArenaFighter::FindAttackAnimation(FistCombat::Move Type) const
{
    const TCHAR* Name = AttackClipName(Type);
    return Name ? CombatAnimations.FindRef(FName(Name)).Get() : nullptr;
}

float AArenaFighter::AttackClipPosition(float Progress, const UAnimSequence* Clip) const
{
    if (!Clip) return 0.f;
    const float Hit = FMath::Clamp(CurrentAttack.ContactFraction, .05f, .95f);
    const float ClipHit = ClipContactFraction(Clip);
    const float P = FMath::Clamp(Progress, 0.f, 1.f);
    // Without a recorded contact point the clip simply spans the attack.
    const float Fraction = ClipHit < 0.f ? P
        : P < Hit ? P / Hit * ClipHit : ClipHit + (P - Hit) / (1.f - Hit) * (1.f - ClipHit);
    return Fraction * Clip->GetPlayLength();
}

void AArenaFighter::UpdatePose(float Dt)
{
    if (bEnemy) UpdateEnemyAnimation(Dt);
    if (!bEnemy && NeutralIdleAnimation)
    {
        UAnimSequence* Strike = FindAttackAnimation(CurrentAttack.Type);
        const bool IsStrike = AttackClock > 0.f && Strike;
        PlayerHitAnimationTime += Dt;
        const float Speed = GetVelocity().Size2D();
        const bool Moving = Speed > 10.f && GetCharacterMovement()->IsMovingOnGround() && DodgeClock <= 0.f && KnockdownClock <= 0.f && IsAlive();
        // Standing uses the Mixamo idle when the outfit has one (it falls back to the neutral pose).
        UAnimSequence* Clip = CombatAnimations.FindRef(TEXT("Idle")).Get();
        if (!Clip) Clip = NeutralIdleAnimation.Get();
        float Position = 0.f;
        const bool Airborne = GetCharacterMovement()->IsFalling();
        if (Airborne)
        {
            AirAnimationTime = bAnimationWasAirborne ? AirAnimationTime + Dt : 0.f;
            LandingAnimationTime = .25f;
        }
        else
        {
            LandingAnimationTime = bAnimationWasAirborne ? 0.f : FMath::Min(.25f, LandingAnimationTime + Dt);
            AirAnimationTime = 0.f;
        }
        bAnimationWasAirborne = Airborne;
        if (!IsAlive())
        {
            Clip = CombatAnimations.FindRef(TEXT("Death"));
            PlayerDeathAnimationTime += Dt;
            if (Clip) Position = FMath::Min(PlayerDeathAnimationTime, Clip->GetPlayLength());
        }
        else if (KnockdownClock > 0.f)
        {
            Clip = CombatAnimations.FindRef(TEXT("Knockdown"));
            if (Clip) Position = FMath::Clamp(1.f - KnockdownClock / PlayerKnockdownDuration, 0.f, 1.f) * Clip->GetPlayLength();
        }
        else if (DodgeClock > 0.f && !bCounterDodge)
        {
            Clip = CombatAnimations.FindRef(TEXT("Dodge"));
            if (Clip) Position = (1.f - DodgeClock / .25f) * Clip->GetPlayLength();
        }
        else if (IsStrike)
        {
            Clip = Strike;
            const float Progress = 1.f - AttackClock / CurrentAttack.Duration;
            Position = AttackClipPosition(Progress, Clip);
            // A perfect counter's damage is immediate, so its clip starts at the contact pose.
            if (bCounterDodge && (CurrentAttack.Type == FistCombat::Move::DodgeUppercut || CurrentAttack.Type == FistCombat::Move::Headbutt))
                Position = AttackClipPosition(CurrentAttack.ContactFraction + (1.f-CurrentAttack.ContactFraction)*Progress, Clip);
            // Hold the descending pose until the real landing resolves damage.
            if (bGroundImpactPending) Position = FMath::Min(Position, AttackClipPosition(CurrentAttack.ContactFraction-.02f, Clip));
        }
        else if (PlayerHitAnimationTime < .3f)
        {
            Clip = CombatAnimations.FindRef(TEXT("Hit"));
            Position = PlayerHitAnimationTime;
        }
        else if (bHeavyHeld || IsBlocking())
        {
            Clip = CombatAnimations.FindRef(bHeavyHeld ? TEXT("Charge") : TEXT("Block"));
            if (Clip) Position = FMath::Fmod(GetWorld()->GetTimeSeconds(), Clip->GetPlayLength());
        }
        else if (JumpAnimation && IsAlive() && KnockdownClock <= 0.f && (Airborne || LandingAnimationTime < .25f))
        {
            Clip = JumpAnimation.Get();
            // Hold the aerial section for any jump height; land only on actual floor contact.
            Position = Airborne ? (GetVelocity().Z > 0.f ? FMath::Min(.30f, .10f + AirAnimationTime) : .65f) : .75f + LandingAnimationTime;
        }
        else
        {
            const bool Running = Moving && Speed > (ActiveAnimation == RunAnimation ? 260.f : 300.f);
            if (Moving) Clip = Running ? RunAnimation.Get() : WalkAnimation.Get();
            const float CycleRate = Moving ? FMath::Clamp(Speed / (Running ? 560.f : 224.f), .35f, 1.7f) : 1.f;
            if (Clip)
            {
                LocomotionPhase = FMath::Fmod(LocomotionPhase + Dt * CycleRate / FMath::Max(Clip->GetPlayLength(), .01f), 1.f);
                Position = LocomotionPhase * Clip->GetPlayLength();
            }
        }
        if (Clip)
        {
            if (ActiveAnimation != Clip)
            {
                GetMesh()->SetAnimation(Clip);
                GetMesh()->Stop();
                ActiveAnimation = Clip;
            }
            GetMesh()->SetPosition(Position, false);
        }
    }
    using FistCombat::Move;
    FVector RH(25.f, 35.f, 25.f), LH(25.f, -35.f, 25.f);
    FVector RF(8.f, 20.f, -62.f), LF(8.f, -20.f, -62.f), HP(0.f, 0.f, 65.f);
    FRotator Lean = FRotator::ZeroRotator;
    const float Stride = FMath::Sin(GetWorld()->GetTimeSeconds() * 13.f) * FMath::Clamp(static_cast<float>(GetVelocity().Size2D()) / FMath::Max(WalkSpeed, 1.f), 0.f, 1.f);
    RF.X += Stride * 22.f;
    LF.X -= Stride * 22.f;
    if (AttackClock > 0.f)
    {
        const float Progress = 1.f - AttackClock / CurrentAttack.Duration;
        const float Pulse = FMath::Sin(PI * Progress);
        switch (CurrentAttack.Type)
        {
        case Move::EnemyClaw:
            if (EnemyMove == EEnemyMove::CommanderSlam)
            {
                const float Height = bHitResolved ? -35.f : 100.f;
                RH = FVector(45.f,30.f,Height); LH = FVector(45.f,-30.f,Height);
                Lean.Pitch = bHitResolved ? -35.f : 18.f;
            }
            else if (EnemyMove == EEnemyMove::CommanderCleave)
            {
                RH = FVector(65.f,bHitResolved ? -90.f : 90.f,35.f);
                Lean.Yaw = (bHitResolved ? -65.f : 35.f) * Pulse;
            }
            else if (EnemyMove == EEnemyMove::CommanderRush)
            {
                RH = FVector(65.f,35.f,20.f); LH = FVector(65.f,-35.f,20.f);
                Lean.Pitch = -40.f * Pulse;
            }
            else if (!bHitResolved) { RH.X -= 45.f * Pulse; RH.Z += 30.f; Lean.Pitch = 15.f; }
            else RH += FVector(100.f, -15.f, 0.f) * Pulse;
            break;
        case Move::RightPunch: case Move::HeavyPunch: case Move::AirPunch:
            RH += FVector(85.f, -15.f, 0.f) * Pulse; break;
        case Move::LeftPunch: case Move::Elbow: case Move::AirLeftPunch:
            LH += FVector(85.f, 20.f, 0.f) * Pulse; Lean.Yaw = 25.f * Pulse; break;
        case Move::TurningKick: case Move::AirKick:
            RF += FVector(105.f, -20.f, 65.f) * Pulse; Lean.Yaw = 100.f * Pulse; break;
        case Move::FollowKick:
            LF += FVector(115.f, 20.f, 70.f) * Pulse; Lean.Yaw = -100.f * Pulse; break;
        case Move::Headbutt:
            HP.X += 45.f * Pulse; Lean.Pitch = -30.f * Pulse; break;
        case Move::Tackle:
            Lean.Pitch = -65.f * Pulse; RH.X += 70.f * Pulse; LH.X += 70.f * Pulse; break;
        case Move::ShoulderThrow:
            RH += FVector(45.f, -10.f, 75.f) * Pulse; LH += FVector(45.f, 10.f, 75.f) * Pulse; Lean.Pitch = -40.f * Pulse; break;
        case Move::DodgeUppercut:
            RH += FVector(70.f, -15.f, 75.f) * Pulse; break;
        case Move::LegSweep:
            RF += FVector(100.f, 0.f, 20.f) * Pulse; Lean.Pitch = -45.f * Pulse; break;
        case Move::AirSlam: case Move::AirCrashKick:
            RH += FVector(60.f, 0.f, -45.f) * Pulse; LH += FVector(60.f, 0.f, -45.f) * Pulse; break;
        case Move::ChargedStrike:
            RH += FVector(90.f, 20.f, 0.f) * Pulse; LH += FVector(90.f, -20.f, 0.f) * Pulse; break;
        }
    }
    if (DodgeClock > 0.f && !bCounterDodge) Lean.Pitch = -360.f * (1.f - DodgeClock / .25f);
    if (bHeavyHeld && ChargeClock > .2f)
    {
        Lean.Pitch = 25.f;
        RH = FVector(-15.f, 40.f, 25.f); LH = FVector(-15.f, -40.f, 25.f);
        DrawDebugCircle(GetWorld(), GetActorLocation() - FVector(0.f, 0.f, 80.f),
            FistCombat::Charged(ChargeClock / FMath::Max(MaxChargeSeconds, .2f)).Range,
            40, FColor::Orange, false, -1.f, 0, 2.f, FVector::ForwardVector, FVector::RightVector, false);
    }
    if (KnockdownClock > 0.f) Lean.Roll = 85.f;
    if (IsBlocking()) { RH = FVector(40.f, 18.f, 50.f); LH = FVector(40.f, -18.f, 50.f); }
    Body->SetRelativeRotation(Lean);
    const FQuat Rotation = Lean.Quaternion();
    auto Pose = [&](UStaticMeshComponent* Part, FVector Position)
    {
        Part->SetRelativeLocation(FMath::VInterpTo(Part->GetRelativeLocation(), Rotation.RotateVector(Position), Dt, 30.f));
    };
    Pose(RightHand, RH); Pose(LeftHand, LH); Pose(RightFoot, RF); Pose(LeftFoot, LF); Pose(Head, HP);
}

void AArenaFighter::FinishAirMove()
{
    AirHangClock = 0.f;
    AirDashClock = 0.f;
    GetCharacterMovement()->GravityScale = NormalGravity;
}

void AArenaFighter::Landed(const FHitResult& Hit)
{
    Super::Landed(Hit);
    LandedPlayerMomentum();
    if (bGroundImpactPending && IsAlive())
    {
        bGroundImpactPending = false;
        bHitResolved = true;
        ResolveAttack();
        AttackClock = .35f;
        MoveLabelClock = 1.2f;
    }
    else if (FistCombat::IsAirStrike(CurrentAttack.Type))
    {
        AttackClock = 0.f;
        bHitResolved = true;
    }
    AirCombo = 0;
    bAirFinisherUsed = false;
    AirHangBudget = 2.4f;
    AirTarget.Reset();
    FinishAirMove();
}

void AArenaFighter::StartAirMove()
{
    AirCombo = CurrentAttack.NextCombo;
    // Aerial punches and kicks close distance to either ground or flying enemies.
    const bool bDash = CurrentAttack.Type == FistCombat::Move::AirPunch || CurrentAttack.Type == FistCombat::Move::AirKick;
    if (bDash)
    {
        const float Radius = (CurrentAttack.Type == FistCombat::Move::AirPunch ? ShortAirDashRange : MediumAirDashRange) * GetDashMultiplier();
        float BestDistance = Radius;
        AirTarget.Reset();
        TArray<AActor*> Fighters;
        UGameplayStatics::GetAllActorsOfClass(this, StaticClass(), Fighters);
        for (AActor* Actor : Fighters)
        {
            AArenaFighter* Candidate = Cast<AArenaFighter>(Actor);
            if (!Candidate || !Candidate->bEnemy || !Candidate->IsAlive()) continue;
            const FVector Delta = Candidate->GetActorLocation() - GetActorLocation();
            if (Delta.Size() > BestDistance || FVector::DotProduct(GetActorForwardVector(), Delta.GetSafeNormal2D()) < .1f) continue;
            FHitResult Wall;
            FCollisionQueryParams Query;
            Query.AddIgnoredActor(this); Query.AddIgnoredActor(Candidate);
            if (GetWorld()->LineTraceSingleByChannel(Wall, GetActorLocation(), Candidate->GetActorLocation(), ECC_Visibility, Query)) continue;
            BestDistance = static_cast<float>(Delta.Size());
            AirTarget = Candidate;
        }
    }
    if (FistCombat::IsGroundImpact(CurrentAttack.Type))
    {
        bAirFinisherUsed = true;
        bGroundImpactPending = true;
        ImpactTimeout = 3.f;
        FinishAirMove();
        FVector Downward(0.f, 0.f, -1800.f);
        if (AArenaFighter* Target = AirTarget.Get())
        {
            const FVector Delta = Target->GetActorLocation() - GetActorLocation();
            FHitResult Wall;
            FCollisionQueryParams Query;
            Query.AddIgnoredActor(this); Query.AddIgnoredActor(Target);
            const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Wall, GetActorLocation(), Target->GetActorLocation(), ECC_Visibility, Query);
            if (Target->IsAlive() && !Target->IsBossAttackArmored() && Target->bFlyingEnemy && Delta.Size() <= 260.f && !bBlocked)
            {
                Target->AttackClock = 0.f;
                Target->bHitResolved = true;
                Target->KnockdownClock = 1.5f;
                Target->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
                Target->ApplyCombatLaunch(FVector(0.f, 0.f, -1800.f),0,nullptr,ActiveAttackImpactBudget);
                Downward += Delta.GetSafeNormal2D() * 250.f;
            }
        }
        LaunchCharacter(Downward, true, true);
        return;
    }
    if (AirHangBudget > 0.f)
    {
        AirHangClock = CurrentAttack.Duration + .35f;
        // Normal aerial attacks keep the jump arc and incoming momentum.
        GetCharacterMovement()->GravityScale = NormalGravity;
        if (bDash)
        {
            if (AArenaFighter* Target = AirTarget.Get())
            {
                const FVector Delta = Target->GetActorLocation() - GetActorLocation();
                const FVector Destination = Target->GetActorLocation() - Delta.GetSafeNormal2D() * 100.f;
                AirDashClock = .16f;
                const FVector DashVelocity = ((Destination - GetActorLocation()) / AirDashClock).GetClampedToMaxSize(3500.f * GetDashMultiplier());
                GetCharacterMovement()->Velocity.X = DashVelocity.X;
                GetCharacterMovement()->Velocity.Y = DashVelocity.Y;
                SetActorRotation(Delta.GetSafeNormal2D().Rotation());
            }
        }
    }
}

void AArenaFighter::RunAttackCancelCheck(float Dt)
{
#if WITH_DEV_AUTOMATION_TESTS
    if (bEnemy || !FParse::Param(FCommandLine::Get(), TEXT("HellgirlCombatCheck"))) return;
    static float Time = 0.f;
    static int32 Phase = 0;
    static TWeakObjectPtr<AArenaFighter> Target;
    Time += Dt;
    if (Time < 1.f || Phase == 2) return;
    auto Check = [](bool Condition, const TCHAR* Reason)
    {
        if (!Condition)
        {
            UE_LOG(LogTemp, Error, TEXT("COMBAT CANCEL CHECK FAILED: %s"), Reason);
            FPlatformMisc::RequestExitWithStatus(false, 1);
        }
        return Condition;
    };
    if (Phase == 0)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Target = GetWorld()->SpawnActor<AArenaFighter>(GetActorLocation()+GetActorForwardVector()*150.f, GetActorRotation(), Params);
        if (!Check(Target.IsValid(), TEXT("Test target spawn"))) return;
        Target->MakeEnemy(1, false);
        Target->SetActorTickEnabled(false);
        Target->Health = Target->MaxHealth = 1000.f;
        Riposte = 50.f;
        Attack();
        if (!Check(AttackClock > 0.f, TEXT("Ground attack started"))) return;
        BufferClock = .2f;
        Dodge();
        if (!Check(AttackClock == 0.f && bHitResolved && BufferClock == 0.f && DodgeClock > 0.f && Stamina == 70.f && Riposte == 50.f, TEXT("Dodge cancels attack and buffer, charges stamina, preserves riposte"))) return;
        Phase = 1; Time = 0.f;
        return;
    }
    if (!Check(Target.IsValid() && Target->Health == 1000.f, TEXT("Cancelled swing cannot deal delayed damage"))) return;
    DodgeCooldown = 0.f; PostDodgeClock = 0.f;
    Attack(); Stamina = 0.f;
    Dodge();
    if (!Check(AttackClock > 0.f && DodgeClock == 0.f, TEXT("Insufficient stamina must not cancel an attack"))) return;
    Stamina = 100.f;
    Dodge();
    AttackClock = DodgeClock = DodgeCooldown = 0.f;
    Stamina = 100.f;
    GetCharacterMovement()->SetMovementMode(MOVE_Falling);
    Energy = MaxEnergy; // This fixture tests cancelling an affordable slam.
    HeavyAttack();
    if (!Check(bGroundImpactPending, TEXT("Aerial slam armed"))) return;
    Dodge();
    if (!Check(!bGroundImpactPending && bHitResolved && AttackClock == 0.f && AirHangClock == 0.f && AirDashClock == 0.f && GetCharacterMovement()->GravityScale == NormalGravity, TEXT("Air dodge cancels landing impact and restores gravity"))) return;
    Target->SetActorLocation(GetActorLocation() + GetActorForwardVector() * 120.f);
    Landed(FHitResult());
    if (!Check(Target->Health == 1000.f, TEXT("Cancelled slam cannot deal landing damage"))) return;
    Phase = 2;
    UE_LOG(LogTemp, Display, TEXT("COMBAT CANCEL CHECK PASSED: ground swing, queued input, stamina gate, aerial slam, landing, riposte"));
    FPlatformMisc::RequestExitWithStatus(false, 0);
#endif
}

void AArenaFighter::RunPerfectCounterCheck()
{
#if WITH_DEV_AUTOMATION_TESTS
    if (bEnemy || !FParse::Param(FCommandLine::Get(), TEXT("HellgirlCounterCheck"))) return;
    static bool Done = false;
    if (Done || GetWorld()->GetTimeSeconds() < 1.f) return;
    Done = true;
    auto Check = [](bool Good, const TCHAR* Reason)
    {
        if (!Good) { UE_LOG(LogTemp, Error, TEXT("COUNTER CHECK FAILED: %s"), Reason); FPlatformMisc::RequestExitWithStatus(false,1); }
        return Good;
    };
    FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Enemy = GetWorld()->SpawnActor<AArenaFighter>(GetActorLocation()+FVector(150,0,0), FRotator(0,180,0), Params);
    if (!Check(Enemy != nullptr, TEXT("Target spawn"))) return;
    Enemy->MakeEnemy(1,false); Enemy->SetActorTickEnabled(false);
    Enemy->Health = Enemy->MaxHealth = 500.f;
    Enemy->CurrentAttack = {FistCombat::Move::EnemyClaw, 1.25f, .6f, 10.f, 165.f, 200.f, 0.f, 0};
    Enemy->bHitResolved = false;
    Enemy->AttackClock = 1.05f;
    if (!Check(!CanCounter(Enemy), TEXT("Early dodge is not a counter"))) return;
    Enemy->AttackClock = .9f;
    if (!Check(CanCounter(Enemy), TEXT("Extended 400ms window is counterable"))) return;
    Enemy->SetActorRotation(FRotator::ZeroRotator);
    if (!Check(!CanCounter(Enemy), TEXT("Attack facing away cannot be countered"))) return;
    Enemy->SetActorRotation(FRotator(0,180,0));
    Enemy->SetActorLocation(GetActorLocation()+FVector(500,0,0));
    if (!Check(!CanCounter(Enemy), TEXT("Distant flash is not a threat"))) return;
    Enemy->SetActorLocation(GetActorLocation()+FVector(150,0,0));
    Enemy->AttackClock = .49f;
    if (!Check(!CanCounter(Enemy), TEXT("Late dodge is not a counter"))) return;
    Enemy->AttackClock = .6f;
    Stamina = 0.f; Dodge();
    if (!Check(Enemy->Health == 500.f, TEXT("No free counter without stamina"))) return;
    Stamina = 100.f; Riposte = 0.f;
    Dodge();
    if (!Check(bCounterDodge && Stamina == 70.f && Enemy->Health == 455.f && Enemy->bHitResolved && Enemy->AttackClock == 0.f && Enemy->KnockdownClock > 0.f, TEXT("Dodge becomes one damaging counter and interrupts attacker"))) return;
    if (!Check(!CanCounter(Enemy), TEXT("Same attack cannot be countered twice"))) return;
    Dodge();
    if (!Check(Enemy->Health == 455.f, TEXT("Cooldown prevents repeat counter"))) return;
    UE_LOG(LogTemp, Display, TEXT("COUNTER CHECK PASSED: timing, facing, range, stamina, damage, interrupt, repeat prevention"));
    FPlatformMisc::RequestExitWithStatus(false,0);
#endif
}

