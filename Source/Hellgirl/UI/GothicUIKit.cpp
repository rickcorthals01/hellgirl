#include "UI/GothicUIKit.h"
#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Text/STextBlock.h"

namespace GothicUI
{
const FSlateBrush* Brush(const TCHAR* Name)
{
    static TMap<FString, TSharedPtr<FSlateBrush>> Brushes;
    if (const TSharedPtr<FSlateBrush>* Found = Brushes.Find(Name)) return Found->Get();
    TSharedPtr<FSlateBrush> New = MakeShared<FSlateBrush>();
    New->DrawAs = ESlateBrushDrawType::Image;
    if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/UI/Placeholder/T_%s.T_%s"), Name, Name)))
    {
        Texture->AddToRoot(); // the brush holds it for the rest of the game
        New->SetResourceObject(Texture);
        New->ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
    }
    else New->DrawAs = ESlateBrushDrawType::NoDrawType;
    Brushes.Add(Name, New);
    return New.Get();
}

FLinearColor Ink() { return FLinearColor(.94f, .88f, .77f); }
FLinearColor Faint() { return FLinearColor(.62f, .57f, .53f); }
FLinearColor Blood() { return FLinearColor(.9f, .2f, .15f); }

TSharedRef<STextBlock> Label(const FString& Value, int32 Size, FLinearColor Color, bool bCentred)
{
    return Label([Value]() { return Value; }, Size, Color, bCentred);
}

TSharedRef<STextBlock> Label(TFunction<FString()> Value, int32 Size, FLinearColor Color, bool bCentred)
{
    return SNew(STextBlock).Text_Lambda([Value]() { return FText::FromString(Value()); }).AutoWrapText(true)
        .Justification(bCentred ? ETextJustify::Center : ETextJustify::Left).Font(FCoreStyle::GetDefaultFontStyle("Regular", Size))
        .ColorAndOpacity(Color).ShadowOffset(FVector2D(1.f, 1.f)).ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, .8f));
}

bool IsLit(const TSharedPtr<SButton>& Button)
{
    return Button.IsValid() && Button->IsEnabled() && (Button->IsHovered() || Button->HasKeyboardFocus() || Button->HasAnyUserFocus().IsSet());
}

TSharedRef<SButton> ArtButton(TSharedPtr<SButton>& Out, const TCHAR* Dark, const TCHAR* Red, TSharedRef<SWidget> Content,
    TFunction<void()> OnClicked, TFunction<bool()> Selected, TFunction<bool()> Enabled)
{
    // The button draws nothing itself: its art is the image below the content.
    static const FButtonStyle Plain = FButtonStyle().SetNormal(FSlateNoResource()).SetHovered(FSlateNoResource())
        .SetPressed(FSlateNoResource()).SetDisabled(FSlateNoResource()).SetNormalPadding(FMargin(0.f)).SetPressedPadding(FMargin(0.f));
    const FSlateBrush* DarkBrush = Brush(Dark);
    const FSlateBrush* RedBrush = Red ? Brush(Red) : nullptr;
    TSharedPtr<TWeakPtr<SButton>> Self = MakeShared<TWeakPtr<SButton>>();
    auto Lit = [Self, Selected]() { return IsLit(Self->Pin()) || (Selected && Selected()); };
    SAssignNew(Out, SButton).ButtonStyle(&Plain).ContentPadding(FMargin(0.f)).HAlign(HAlign_Fill).VAlign(VAlign_Fill)
        .IsFocusable(true)
        .IsEnabled_Lambda([Enabled]() { return !Enabled || Enabled(); })
        .OnClicked_Lambda([OnClicked]() { if (OnClicked) OnClicked(); return FReply::Handled(); })
        [SNew(SOverlay)
            + SOverlay::Slot()[SNew(SImage)
                .Image_Lambda([Lit, DarkBrush, RedBrush]() { return Lit() && RedBrush ? RedBrush : DarkBrush; })
                .ColorAndOpacity_Lambda([Lit, RedBrush, Self]()
                {
                    const TSharedPtr<SButton> B = Self->Pin();
                    if (B.IsValid() && !B->IsEnabled()) return FSlateColor(FLinearColor(.55f, .52f, .52f));
                    return FSlateColor(Lit() && !RedBrush ? FLinearColor(1.6f, .75f, .72f) : FLinearColor::White);
                })]
            + SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Center)[Content]];
    *Self = Out;
    return Out.ToSharedRef();
}

TSharedRef<SWidget> Screen(const TCHAR* Background, FVector2D Size, const TArray<FPlace>& Places)
{
    TSharedRef<SConstraintCanvas> Canvas = SNew(SConstraintCanvas);
    Canvas->AddSlot().Anchors(FAnchors(0.f, 0.f)).Offset(FMargin(0.f, 0.f, Size.X, Size.Y))[SNew(SImage).Image(Brush(Background))];
    for (const FPlace& Place : Places)
        Canvas->AddSlot().Anchors(FAnchors(0.f, 0.f)).Offset(FMargin(Place.Position.X, Place.Position.Y, Place.Size.X, Place.Size.Y))[Place.Widget];
    return SNew(SOverlay)
        + SOverlay::Slot()[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, .7f))]
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
          [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
            [SNew(SBox).WidthOverride(Size.X).HeightOverride(Size.Y)[Canvas]]];
}
}
