#include "Levels/StageOneTerrain.h"
#include "Levels/CastleTerrainLayout.h"
#include "ProceduralMeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"

AStageOneTerrain::AStageOneTerrain()
{
    Ground = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("RollingGround"));
    RootComponent = Ground;
    Ground->bUseAsyncCooking = false;
    Ground->bUseComplexAsSimpleCollision = true;
    Ground->SetCollisionProfileName(TEXT("BlockAll"));
}

void AStageOneTerrain::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    constexpr int32 CellsX = 125;
    constexpr int32 CellsY = 95;
    constexpr float Spacing = CastleTerrain::GridSpacing;
    constexpr int32 VertexCount = (CellsX + 1) * (CellsY + 1);
    TArray<FVector> Vertices, Normals;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> Colors;
    TArray<FProcMeshTangent> Tangents;
    TArray<int32> Triangles;
    Vertices.Reserve(VertexCount); Normals.Reserve(VertexCount);
    UVs.Reserve(VertexCount); Colors.Reserve(VertexCount); Tangents.Reserve(VertexCount);
    Triangles.Reserve(CellsX * CellsY * 6);
    for (int32 Row = 0; Row <= CellsX; ++Row)
        for (int32 Column = 0; Column <= CellsY; ++Column)
        {
            const float X = -CastleTerrain::HalfExtentX + Row * Spacing;
            const float Y = -CastleTerrain::HalfExtentY + Column * Spacing;
            const float Z = CastleTerrain::Height(X, Y);
            Vertices.Add(FVector(X, Y, Z));
            const float DX = (CastleTerrain::Height(X + 10.f, Y) - CastleTerrain::Height(X - 10.f, Y)) / 20.f;
            const float DY = (CastleTerrain::Height(X, Y + 10.f) - CastleTerrain::Height(X, Y - 10.f)) / 20.f;
            Normals.Add(FVector(-DX, -DY, 1.f).GetSafeNormal());
            UVs.Add(FVector2D(X, Y));
            const float Path = CastleTerrain::PathBlend(X, Y);
            const float Variation = .96f + .045f * FMath::Sin(X / 190.f + Y / 330.f)
                + .025f * FMath::Cos(Y / 145.f - X / 270.f);
            const float Altitude = FMath::Clamp(Z / 500.f, 0.f, 1.f);
            const FLinearColor Moss(.25f + .025f * Altitude, .30f + .01f * Altitude, .19f + .025f * Altitude);
            const FLinearColor Earth(.29f, .20f, .12f);
            FLinearColor Color = (Moss * (1.f - Path) + Earth * Path) * Variation;
            Color.A = 1.f;
            Colors.Add(Color);
            Tangents.Add(FProcMeshTangent(FVector(1.f, 0.f, DX).GetSafeNormal(), false));
        }
    for (int32 Row = 0; Row < CellsX; ++Row)
        for (int32 Column = 0; Column < CellsY; ++Column)
        {
            const int32 A = Row * (CellsY + 1) + Column, B = A + CellsY + 1;
            // Unreal's clockwise front face; rows advance X and columns advance Y.
            Triangles.Append({A, A + 1, B, A + 1, B + 1, B});
        }
    Ground->ClearAllMeshSections();
    Ground->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, true);
    UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Environment/Materials/M_EnvironmentEarth.M_EnvironmentEarth"));
    if (!Base) Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Environment/Materials/M_EnvironmentSurface.M_EnvironmentSurface"));
    const bool bUsesVertexColor = Base != nullptr;
    if (!Base) Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (Base)
        if (UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Base, this))
        {
            Material->SetScalarParameterValue(TEXT("UseVertexColor"), 1.f);
            Material->SetVectorParameterValue(TEXT("Color"), bUsesVertexColor ? FLinearColor::White : FLinearColor(.25f, .30f, .19f));
            Material->SetScalarParameterValue(TEXT("Roughness"), .95f);
            Material->SetScalarParameterValue(TEXT("TextureScale"), .0025f);
            Ground->SetMaterial(0, Material);
        }
}
