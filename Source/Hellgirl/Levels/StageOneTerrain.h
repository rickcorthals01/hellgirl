#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StageOneTerrain.generated.h"
class UProceduralMeshComponent;

UCLASS()
class HELLGIRL_API AStageOneTerrain : public AActor
{
    GENERATED_BODY()
public:
    AStageOneTerrain();
    virtual void OnConstruction(const FTransform& Transform) override;
    UProceduralMeshComponent* GetGroundMesh() const { return Ground.Get(); }
private:
    UPROPERTY() TObjectPtr<UProceduralMeshComponent> Ground;
};
