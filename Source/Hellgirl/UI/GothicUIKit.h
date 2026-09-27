#pragma once
#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SWidget.h"

class SButton;
class STextBlock;

// The placeholder menu art (the user's mockups in "Hellgirl Game/UI", cut by Tools/UI/prepare_ui.ps1 and imported as
// /Game/UI/Placeholder/T_<Name>). A menu is laid out at its mockup's own pixel size, with the mockup as its
// background and buttons drawn as the mockup's pieces on top at the same spots, and the whole is scaled to fit the
// screen. Used by the death menu, the blue portal, the goblin's shop and the level select.
namespace GothicUI
{
// The brush for T_<Name>, at the texture's pixel size (loaded once and kept for the game's lifetime).
const FSlateBrush* Brush(const TCHAR* Name);

// Text in the menus' parchment colours.
FLinearColor Ink();
FLinearColor Faint();
FLinearColor Blood();
TSharedRef<STextBlock> Label(const FString& Value, int32 Size, FLinearColor Color, bool bCentred = true);
TSharedRef<STextBlock> Label(TFunction<FString()> Value, int32 Size, FLinearColor Color, bool bCentred = true);

// A button drawn as art: Dark normally, Red when hovered, focused or Selected() (with no Red piece, Dark tinted red),
// dimmed when disabled; Content on top.
TSharedRef<SButton> ArtButton(TSharedPtr<SButton>& Out, const TCHAR* Dark, const TCHAR* Red, TSharedRef<SWidget> Content,
    TFunction<void()> OnClicked, TFunction<bool()> Selected = nullptr, TFunction<bool()> Enabled = nullptr);

// Whether a button is hovered or has the focus (what ArtButton shows in red).
bool IsLit(const TSharedPtr<SButton>& Button);

// One widget at mockup coordinates.
struct FPlace
{
    FVector2D Position, Size;
    TSharedRef<SWidget> Widget;
};
// A whole menu: the mockup T_<Background> at Size (its pixel size) and the places over it, scaled to fit the screen
// and centred, over a dark veil.
TSharedRef<SWidget> Screen(const TCHAR* Background, FVector2D Size, const TArray<FPlace>& Places);
}
// Note for layouts: buttons must not overlap (not even their art's spikes), or gamepad navigation between them breaks:
// "down" only finds buttons that start below the current one's bottom edge.
