#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "EnemyTypes.generated.h"
class USkeletalMesh;
class UAnimInstance;
class UAnimSequence;

UENUM(BlueprintType)
enum class EHellgirlEnemyType : uint8
{
    Imps, FlyingImps, ImpCommander, Gulps, GulpBoss, Succubus, SuccubusBoss, Goblins, GoblinQueen,
    // World III's mass enemy: small and flying only (hovers and dives like the flying imps).
    MiniSuccubus
};

USTRUCT(BlueprintType)
struct FEnemyModelSlot
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, Category="Model") TSoftObjectPtr<USkeletalMesh> Mesh;
    UPROPERTY(EditAnywhere, Category="Model") TSoftClassPtr<UAnimInstance> AnimationBlueprint;
    UPROPERTY(EditAnywhere, Category="Animation") TSoftObjectPtr<UAnimSequence> Idle;
    UPROPERTY(EditAnywhere, Category="Animation") TSoftObjectPtr<UAnimSequence> Move;
    UPROPERTY(EditAnywhere, Category="Animation") TSoftObjectPtr<UAnimSequence> Attack;
    UPROPERTY(EditAnywhere, Category="Animation") TSoftObjectPtr<UAnimSequence> Hit;
    UPROPERTY(EditAnywhere, Category="Animation") TSoftObjectPtr<UAnimSequence> Death;
    UPROPERTY(EditAnywhere, Category="Model") FVector Offset = FVector(0,0,-88);
    UPROPERTY(EditAnywhere, Category="Model") FRotator Rotation = FRotator(0,-90,0);
    UPROPERTY(EditAnywhere, Category="Model") FVector Scale = FVector::OneVector;
    UPROPERTY(EditAnywhere, Category="Model") FName AttackFlashSocket;
};

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Hellgirl Enemy Models"))
class HELLGIRL_API UHellgirlEnemyModels : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    virtual FName GetCategoryName() const override { return TEXT("Game"); }
    UPROPERTY(Config, EditAnywhere, Category="Map 01") FEnemyModelSlot Imps;
    UPROPERTY(Config, EditAnywhere, Category="Map 01") FEnemyModelSlot FlyingImps;
    UPROPERTY(Config, EditAnywhere, Category="Map 01") FEnemyModelSlot ImpCommander;
    UPROPERTY(Config, EditAnywhere, Category="Map 02") FEnemyModelSlot Gulps;
    UPROPERTY(Config, EditAnywhere, Category="Map 02") FEnemyModelSlot GulpBoss;
    UPROPERTY(Config, EditAnywhere, Category="Map 03") FEnemyModelSlot Succubus;
    UPROPERTY(Config, EditAnywhere, Category="Map 03") FEnemyModelSlot SuccubusBoss;
    UPROPERTY(Config, EditAnywhere, Category="Map 03") FEnemyModelSlot MiniSuccubus;
    UPROPERTY(Config, EditAnywhere, Category="Level 01") FEnemyModelSlot Goblins;
    UPROPERTY(Config, EditAnywhere, Category="Level 01") FEnemyModelSlot GoblinQueen;
    const FEnemyModelSlot& ForType(EHellgirlEnemyType Type) const
    {
        switch (Type)
        {
        case EHellgirlEnemyType::Goblins: return Goblins;
        case EHellgirlEnemyType::GoblinQueen: return GoblinQueen;
        case EHellgirlEnemyType::FlyingImps: return FlyingImps;
        case EHellgirlEnemyType::ImpCommander: return ImpCommander;
        case EHellgirlEnemyType::Gulps: return Gulps;
        case EHellgirlEnemyType::GulpBoss: return GulpBoss;
        case EHellgirlEnemyType::Succubus: return Succubus;
        case EHellgirlEnemyType::SuccubusBoss: return SuccubusBoss;
        case EHellgirlEnemyType::MiniSuccubus: return MiniSuccubus;
        default: return Imps;
        }
    }
};
