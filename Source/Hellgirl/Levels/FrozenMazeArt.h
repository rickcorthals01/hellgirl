#pragma once
#include "CoreMinimal.h"

class UWorld;
class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class APointLight;
class AActor;

// Building blocks for the Frozen Maze (defined in FrozenMazeScenery.cpp). Assets live in /Game/Environment/Maze
// (Tools/Environment/build_maze_kit.ps1).
namespace MazeArt
{
// A mesh from the generated maze kit, e.g. Kit(TEXT("SM_IceWallA")).
UStaticMesh* Kit(const TCHAR* Name);
// A material from the maze folder, e.g. Material(TEXT("M_MazeFall")).
UMaterialInterface* Material(const TCHAR* Name);
// A point light; the ones that cast shadows keep their glow from leaking through the ice walls.
APointLight* Light(UWorld* World, FVector Where, FLinearColor Color, float Intensity, float Radius, bool Shadows);
// A flat rectangle at height Z (facing up or down) with world-space UVs (world units / TileSize).
AActor* Sheet(UWorld* World, FVector2D Min, FVector2D Max, float Z, bool FaceUp, UMaterialInterface* Material, float TileSize);
// A curtain of falling water from Top down to the floor along From-To, bowing out toward Out by Bulge at the foot.
UMaterialInstanceDynamic* Waterfall(UWorld* World, FVector2D From, FVector2D To, FVector2D Out, float Top, float Bulge, float Glow, float Opacity, float Speed);
}
