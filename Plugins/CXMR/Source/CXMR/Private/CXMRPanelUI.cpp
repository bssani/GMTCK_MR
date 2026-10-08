// Copyright GMTCK CX.

#include "CXMRPanelUI.h"

#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Brushes/SlateNoResource.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "CXMRPanelUI"

namespace
{
	// 행 높이와 오른쪽 조작 열 너비를 모든 행에서 맞춤.
	constexpr float RowHeight = 42.f;
	constexpr float ControlWidth = 268.f;
	constexpr float ResetWidth = 64.f;
	constexpr float ControlRadius = 6.f;

	FLinearColor Hex(uint8 R, uint8 G, uint8 B) { return FLinearColor::FromSRGBColor(FColor(R, G, B)); }

	FLinearColor RimColor() { return Hex(184, 193, 204); }
	FLinearColor RailRaisedColor() { return Hex(40, 51, 64); }

	FLinearColor ToneTint(CXMRPanelUI::ETone Tone)
	{
		switch (Tone)
		{
		case CXMRPanelUI::ETone::Positive:  return Hex(229, 243, 236);
		case CXMRPanelUI::ETone::Attention: return Hex(252, 241, 222);
		case CXMRPanelUI::ETone::Negative:  return Hex(251, 233, 231);
		default:                            return CXMRPanelUI::SelectionTint();
		}
	}

	const FSlateBrush* WhiteBrush() { return FCoreStyle::Get().GetBrush("WhiteBrush"); }

	const FSlateBrush* ChoiceBrush(bool bSelected)
	{
		static const FSlateRoundedBoxBrush Selected(CXMRPanelUI::SelectionTint(), ControlRadius, CXMRPanelUI::SelectionColor(), 1.5f);
		static const FSlateRoundedBoxBrush Idle(FLinearColor::Transparent, ControlRadius, CXMRPanelUI::HairlineColor(), 1.f);
		return bSelected ? &Selected : &Idle;
	}

	const FSlateBrush* PillBrush(bool bSelected)
	{
		static const FSlateRoundedBoxBrush Selected(CXMRPanelUI::SelectionColor(), ControlRadius);
		static const FSlateRoundedBoxBrush Idle(FLinearColor::Transparent, ControlRadius, RimColor(), 1.f);
		return bSelected ? &Selected : &Idle;
	}

	const FSlateBrush* RadioBrush(bool bSelected)
	{
		static const FSlateRoundedBoxBrush On(FLinearColor::White, 9.f, CXMRPanelUI::SelectionColor(), 5.f, FVector2f(18.f, 18.f));
		static const FSlateRoundedBoxBrush Off(FLinearColor::White, 9.f, RimColor(), 1.5f, FVector2f(18.f, 18.f));
		return bSelected ? &On : &Off;
	}

	const FSlateBrush* StepBrush(int32 State)
	{
		static const FSlateRoundedBoxBrush Done(CXMRPanelUI::SelectionColor(), 14.f, FVector2f(28.f, 28.f));
		static const FSlateRoundedBoxBrush Ready(FLinearColor::White, 14.f, CXMRPanelUI::SelectionColor(), 2.f, FVector2f(28.f, 28.f));
		static const FSlateRoundedBoxBrush Locked(FLinearColor::White, 14.f, RimColor(), 1.5f, FVector2f(28.f, 28.f));
		return State >= 2 ? &Done : State == 1 ? &Ready : &Locked;
	}

	const FButtonStyle& RailButtonStyle()
	{
		static const FButtonStyle Style = FButtonStyle()
			.SetNormal(FSlateNoResource())
			.SetHovered(FSlateRoundedBoxBrush(FLinearColor(1.f, 1.f, 1.f, 0.08f), ControlRadius))
			.SetPressed(FSlateRoundedBoxBrush(FLinearColor(1.f, 1.f, 1.f, 0.14f), ControlRadius))
			.SetDisabled(FSlateNoResource())
			.SetNormalForeground(CXMRPanelUI::RailInkColor()).SetHoveredForeground(CXMRPanelUI::RailInkColor())
			.SetPressedForeground(CXMRPanelUI::RailInkColor()).SetDisabledForeground(CXMRPanelUI::RailMutedColor())
			.SetNormalPadding(FMargin(0.f)).SetPressedPadding(FMargin(0.f));
		return Style;
	}

	FReply InvokeAction(const TFunction<void()>& Invoke)
	{
		if (Invoke) { Invoke(); }
		return FReply::Handled();
	}

	FText ChoiceText(const TFunction<float()>& Get, const TArray<FText>& Options)
	{
		const int32 Index = Get ? FMath::RoundToInt(Get()) : 0;
		return Options.IsValidIndex(Index) ? Options[Index] : FText::GetEmpty();
	}

	FReply ApplyChoice(const TFunction<void(float, bool)>& Apply, int32 Index)
	{
		if (Apply) { Apply(static_cast<float>(Index), true); }
		FSlateApplication::Get().DismissAllMenus();
		return FReply::Handled();
	}

	TSharedRef<SWidget> ChoiceMenu(const TFunction<float()>& Get, const TFunction<void(float, bool)>& Apply, const TArray<FText>& Options)
	{
		TSharedRef<SVerticalBox> Choices = SNew(SVerticalBox);
		for (int32 Index = 0; Index < Options.Num(); ++Index)
		{
			Choices->AddSlot().AutoHeight().Padding(0.f, 1.f)
			[
				CXMRPanelUI::MakeChoice(Options[Index],
					TAttribute<bool>::CreateLambda([Get, Index] { return Get && FMath::RoundToInt(Get()) == Index; }),
					[Apply, Index] { return ApplyChoice(Apply, Index); })
			];
		}
		static const FSlateRoundedBoxBrush Menu(CXMRPanelUI::SurfaceColor(), 8.f, CXMRPanelUI::HairlineColor(), 1.f);
		return SNew(SBorder).BorderImage(&Menu).ForegroundColor(CXMRPanelUI::InkColor()).Padding(6.f)
			[ SNew(SBox).MinDesiredWidth(220.f)[ Choices ] ];
	}

	bool IsRangedKind(ECXMRTunableKind Kind)
	{
		return Kind == ECXMRTunableKind::Bool || Kind == ECXMRTunableKind::Float || Kind == ECXMRTunableKind::Choice;
	}

	float RangeTopFor(const FCXMRTunable& Tunable)
	{
		if (Tunable.Kind == ECXMRTunableKind::Bool)
		{
			return 1.0f;
		}
		if (Tunable.Kind == ECXMRTunableKind::Choice)
		{
			return static_cast<float>(FMath::Max(0, Tunable.Options.Num() - 1));
		}
		return Tunable.Max;
	}

	TSharedRef<SWidget> ResetSlot(const TFunction<void()>& Reset)
	{
		TSharedRef<SBox> Box = SNew(SBox).WidthOverride(ResetWidth).HAlign(HAlign_Right).VAlign(VAlign_Center);
		if (Reset)
		{
			Box->SetContent(SNew(SButton).ButtonStyle(CXMRPanelUI::QuietButtonStyle()).ContentPadding(FMargin(8.f, 4.f))
				.ToolTipText(LOCTEXT("DefaultTip", "Back to the default value, and forget the saved one"))
				.OnClicked_Lambda([Reset] { Reset(); return FReply::Handled(); })
				[ SNew(STextBlock).Font(CXMRPanelUI::BodyFont(11)).Text(LOCTEXT("Default", "Reset")) ]);
		}
		return Box;
	}
}

// 색

FLinearColor CXMRPanelUI::BackgroundColor() { return Hex(236, 239, 243); }
FLinearColor CXMRPanelUI::SurfaceColor()    { return FLinearColor::White; }
FLinearColor CXMRPanelUI::InkColor()        { return Hex(22, 32, 43); }
FLinearColor CXMRPanelUI::HeaderColor()     { return InkColor(); }
FLinearColor CXMRPanelUI::ReadoutColor()    { return Hex(88, 101, 116); }
FLinearColor CXMRPanelUI::FaintColor()      { return Hex(139, 149, 161); }
FLinearColor CXMRPanelUI::HairlineColor()   { return Hex(222, 227, 233); }
FLinearColor CXMRPanelUI::SelectionColor()  { return Hex(31, 91, 208); }
FLinearColor CXMRPanelUI::SelectionTint()   { return Hex(232, 239, 251); }
FLinearColor CXMRPanelUI::RailColor()       { return Hex(24, 33, 43); }
FLinearColor CXMRPanelUI::RailInkColor()    { return Hex(242, 245, 248); }
FLinearColor CXMRPanelUI::RailMutedColor()  { return Hex(160, 172, 186); }

FLinearColor CXMRPanelUI::ToneColor(ETone Tone)
{
	switch (Tone)
	{
	case ETone::Positive:  return Hex(31, 157, 97);
	case ETone::Attention: return Hex(214, 140, 18);
	case ETone::Negative:  return Hex(204, 64, 52);
	default:               return FaintColor();
	}
}

// 글꼴

FSlateFontInfo CXMRPanelUI::BodyFont(int32 Size)    { return FCoreStyle::GetDefaultFontStyle("Regular", Size); }
FSlateFontInfo CXMRPanelUI::StrongFont(int32 Size)  { return FCoreStyle::GetDefaultFontStyle("Medium", Size); }
FSlateFontInfo CXMRPanelUI::HeadingFont(int32 Size) { return FCoreStyle::GetDefaultFontStyle("BoldCondensed", Size); }

// 스타일

const FTextBlockStyle* CXMRPanelUI::BodyTextStyle()
{
	static const FTextBlockStyle Style = FTextBlockStyle().SetFont(BodyFont(12)).SetColorAndOpacity(FSlateColor::UseForeground());
	return &Style;
}

const FButtonStyle* CXMRPanelUI::ButtonStyle()
{
	static const FButtonStyle Style = FButtonStyle()
		.SetNormal(FSlateRoundedBoxBrush(SurfaceColor(), ControlRadius, RimColor(), 1.f))
		.SetHovered(FSlateRoundedBoxBrush(SelectionTint(), ControlRadius, SelectionColor(), 1.f))
		.SetPressed(FSlateRoundedBoxBrush(Hex(214, 227, 249), ControlRadius, SelectionColor(), 1.f))
		.SetDisabled(FSlateRoundedBoxBrush(BackgroundColor(), ControlRadius, HairlineColor(), 1.f))
		.SetNormalForeground(InkColor()).SetHoveredForeground(SelectionColor()).SetPressedForeground(SelectionColor())
		.SetDisabledForeground(FaintColor())
		.SetNormalPadding(FMargin(14.f, 7.f)).SetPressedPadding(FMargin(14.f, 7.f));
	return &Style;
}

const FButtonStyle* CXMRPanelUI::PrimaryButtonStyle()
{
	static const FButtonStyle Style = FButtonStyle()
		.SetNormal(FSlateRoundedBoxBrush(SelectionColor(), ControlRadius))
		.SetHovered(FSlateRoundedBoxBrush(Hex(24, 77, 180), ControlRadius))
		.SetPressed(FSlateRoundedBoxBrush(Hex(19, 63, 150), ControlRadius))
		.SetDisabled(FSlateRoundedBoxBrush(HairlineColor(), ControlRadius))
		.SetNormalForeground(FLinearColor::White).SetHoveredForeground(FLinearColor::White).SetPressedForeground(FLinearColor::White)
		.SetDisabledForeground(FaintColor())
		.SetNormalPadding(FMargin(16.f, 8.f)).SetPressedPadding(FMargin(16.f, 8.f));
	return &Style;
}

const FButtonStyle* CXMRPanelUI::QuietButtonStyle()
{
	static const FButtonStyle Style = FButtonStyle()
		.SetNormal(FSlateNoResource())
		.SetHovered(FSlateRoundedBoxBrush(SelectionTint(), 4.f))
		.SetPressed(FSlateRoundedBoxBrush(Hex(214, 227, 249), 4.f))
		.SetDisabled(FSlateNoResource())
		.SetNormalForeground(ReadoutColor()).SetHoveredForeground(SelectionColor()).SetPressedForeground(SelectionColor())
		.SetDisabledForeground(FaintColor())
		.SetNormalPadding(FMargin(0.f)).SetPressedPadding(FMargin(0.f));
	return &Style;
}

const FButtonStyle* CXMRPanelUI::FlatButtonStyle()
{
	static const FButtonStyle Style = FButtonStyle()
		.SetNormal(FSlateNoResource())
		.SetHovered(FSlateRoundedBoxBrush(Hex(243, 246, 252), ControlRadius))
		.SetPressed(FSlateRoundedBoxBrush(SelectionTint(), ControlRadius))
		.SetDisabled(FSlateNoResource())
		.SetNormalForeground(InkColor()).SetHoveredForeground(InkColor()).SetPressedForeground(InkColor())
		.SetDisabledForeground(FaintColor())
		.SetNormalPadding(FMargin(0.f)).SetPressedPadding(FMargin(0.f));
	return &Style;
}

const FSpinBoxStyle* CXMRPanelUI::SpinStyle()
{
	static const FSpinBoxStyle Style = FSpinBoxStyle(FCoreStyle::Get().GetWidgetStyle<FSpinBoxStyle>("SpinBox"))
		.SetBackgroundBrush(FSlateRoundedBoxBrush(Hex(245, 247, 250), 5.f, HairlineColor(), 1.f))
		.SetHoveredBackgroundBrush(FSlateRoundedBoxBrush(SurfaceColor(), 5.f, RimColor(), 1.f))
		.SetActiveBackgroundBrush(FSlateRoundedBoxBrush(SurfaceColor(), 5.f, SelectionColor(), 1.f))
		.SetInactiveFillBrush(FSlateNoResource())
		.SetHoveredFillBrush(FSlateNoResource())
		.SetActiveFillBrush(FSlateNoResource())
		.SetForegroundColor(InkColor())
		.SetTextPadding(FMargin(8.f, 4.f));
	return &Style;
}

const FSliderStyle* CXMRPanelUI::SliderStyle()
{
	static const FSliderStyle Style = FSliderStyle(FCoreStyle::Get().GetWidgetStyle<FSliderStyle>("Slider"))
		.SetNormalBarImage(FSlateRoundedBoxBrush(HairlineColor(), 2.f))
		.SetHoveredBarImage(FSlateRoundedBoxBrush(RimColor(), 2.f))
		.SetDisabledBarImage(FSlateRoundedBoxBrush(HairlineColor(), 2.f))
		.SetNormalThumbImage(FSlateRoundedBoxBrush(SurfaceColor(), 8.f, SelectionColor(), 2.f, FVector2f(16.f, 16.f)))
		.SetHoveredThumbImage(FSlateRoundedBoxBrush(SelectionTint(), 8.f, SelectionColor(), 2.f, FVector2f(16.f, 16.f)))
		.SetDisabledThumbImage(FSlateRoundedBoxBrush(SurfaceColor(), 8.f, RimColor(), 1.5f, FVector2f(16.f, 16.f)))
		.SetBarThickness(4.f);
	return &Style;
}

const FCheckBoxStyle* CXMRPanelUI::CheckStyle()
{
	static const FCheckBoxStyle Style = []
	{
		FCheckBoxStyle Base = FCoreStyle::Get().GetWidgetStyle<FCheckBoxStyle>("Checkbox");
		Base.SetBackgroundImage(FSlateRoundedBoxBrush(SurfaceColor(), 4.f, RimColor(), 1.5f, FVector2f(20.f, 20.f)));
		Base.SetBackgroundHoveredImage(FSlateRoundedBoxBrush(SurfaceColor(), 4.f, SelectionColor(), 1.5f, FVector2f(20.f, 20.f)));
		Base.SetBackgroundPressedImage(FSlateRoundedBoxBrush(SelectionTint(), 4.f, SelectionColor(), 1.5f, FVector2f(20.f, 20.f)));
		// 체크 표시만 선택색으로 바꿈.
		for (FSlateBrush* Check : { &Base.CheckedImage, &Base.CheckedHoveredImage, &Base.CheckedPressedImage })
		{
			Check->TintColor = FSlateColor(SelectionColor());
		}
		return Base;
	}();
	return &Style;
}

const FScrollBarStyle* CXMRPanelUI::ScrollBarStyle()
{
	static const FScrollBarStyle Style = FScrollBarStyle(FCoreStyle::Get().GetWidgetStyle<FScrollBarStyle>("ScrollBar"))
		.SetVerticalBackgroundImage(FSlateNoResource())
		.SetHorizontalBackgroundImage(FSlateNoResource())
		.SetVerticalTopSlotImage(FSlateNoResource())
		.SetVerticalBottomSlotImage(FSlateNoResource())
		.SetHorizontalTopSlotImage(FSlateNoResource())
		.SetHorizontalBottomSlotImage(FSlateNoResource())
		.SetNormalThumbImage(FSlateRoundedBoxBrush(Hex(196, 204, 213), 3.f))
		.SetHoveredThumbImage(FSlateRoundedBoxBrush(Hex(168, 178, 190), 3.f))
		.SetDraggedThumbImage(FSlateRoundedBoxBrush(Hex(142, 153, 166), 3.f))
		.SetThickness(6.f);
	return &Style;
}

// 바인딩

CXMRPanelUI::FRowBinding CXMRPanelUI::BindToRegistry(UCXMRTuningSubsystem* Tuning, const FCXMRTunable& Tunable)
{
	const TWeakObjectPtr<UCXMRTuningSubsystem> Weak(Tuning);
	const FName Id = Tunable.Id;

	FRowBinding Binding;
	Binding.Get       = [Weak, Id] { return Weak.IsValid() ? Weak->GetTunableValue(Id) : 0.0f; };
	Binding.Apply     = [Weak, Id](float Value, bool bCommit) { if (Weak.IsValid()) { Weak->ApplyValue(Id, Value, bCommit); } };
	Binding.Step      = [Weak, Id](float Direction) { if (Weak.IsValid()) { Weak->InvokeTunable(Id, Direction); } };
	Binding.Invoke    = [Weak, Id] { if (Weak.IsValid()) { Weak->InvokeTunable(Id); } };
	Binding.Text      = [Weak, Id] { return Weak.IsValid() ? FText::FromString(Weak->GetTunableText(Id)) : FText::GetEmpty(); };
	Binding.IsEnabled = [Weak, Id] { return !Weak.IsValid() || Weak->IsTunableEnabled(Id); };
	if (Tunable.bPersist)
	{
		Binding.Reset = [Weak, Id] { if (Weak.IsValid()) { Weak->ResetToDefault(Id); } };
	}
	return Binding;
}

CXMRPanelUI::FRowBinding CXMRPanelUI::BindDirect(const FCXMRTunable& Tunable)
{
	FRowBinding Binding;
	Binding.Get = Tunable.Get;
	if (Tunable.Set)
	{
		const bool bClamp = IsRangedKind(Tunable.Kind);
		const float Min = bClamp ? (Tunable.Kind == ECXMRTunableKind::Float ? Tunable.Min : 0.0f) : 0.0f;
		const float Max = RangeTopFor(Tunable);
		Binding.Apply = [Set = Tunable.Set, bClamp, Min, Max](float Value, bool) { Set(bClamp ? FMath::Clamp(Value, Min, Max) : Value); };
	}
	Binding.Step = Tunable.Step;
	Binding.Invoke = Tunable.Invoke;
	Binding.Text = Tunable.Text;
	Binding.IsEnabled = Tunable.IsEnabled;
	return Binding;
}

// 구성 요소

TSharedRef<SWidget> CXMRPanelUI::MakeSurface(const TSharedRef<SWidget>& Content)
{
	static const FSlateRoundedBoxBrush Sheet(SurfaceColor(), 10.f, HairlineColor(), 1.f);
	return SNew(SBorder).BorderImage(&Sheet).ForegroundColor(InkColor()).Padding(FMargin(28.f, 24.f))[ Content ];
}

TSharedRef<SWidget> CXMRPanelUI::MakeDivider()
{
	return SNew(SBox).HeightOverride(1.f)[ SNew(SImage).Image(WhiteBrush()).ColorAndOpacity(HairlineColor()) ];
}

TSharedRef<SWidget> CXMRPanelUI::MakeHeader(const FText& Category)
{
	return SNew(STextBlock).Text(Category).Font(HeadingFont(16)).ColorAndOpacity(InkColor());
}

TSharedRef<SWidget> CXMRPanelUI::MakeSubheader(const FText& Text)
{
	return SNew(STextBlock).Text(Text).Font(StrongFont(12)).ColorAndOpacity(ReadoutColor());
}

TSharedRef<SWidget> CXMRPanelUI::MakeCaption(const TAttribute<FText>& Text)
{
	return SNew(STextBlock).Text(Text).Font(BodyFont(12)).ColorAndOpacity(ReadoutColor()).AutoWrapText(true);
}

TSharedRef<SWidget> CXMRPanelUI::MakeNotice(const TAttribute<FText>& Text, const TAttribute<ETone>& Tone)
{
	static const FSlateRoundedBoxBrush Plate(FLinearColor::White, ControlRadius);
	return SNew(SBorder)
		.Visibility_Lambda([Text] { return Text.Get().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
		.BorderImage(&Plate)
		.BorderBackgroundColor_Lambda([Tone] { return FSlateColor(ToneTint(Tone.Get())); })
		.Padding(0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[ SNew(SBox).WidthOverride(3.f)[ SNew(SImage).Image(WhiteBrush()).ColorAndOpacity_Lambda([Tone]
				{ return FSlateColor(Tone.Get() == ETone::Neutral ? SelectionColor() : ToneColor(Tone.Get())); }) ] ]
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(14.f, 10.f))
			[ SNew(STextBlock).Text(Text).Font(BodyFont(12)).ColorAndOpacity(InkColor()).AutoWrapText(true) ]
		];
}

TSharedRef<SWidget> CXMRPanelUI::MakeLamp(const TAttribute<FSlateColor>& Color)
{
	static const FSlateRoundedBoxBrush Dot(FLinearColor::White, 5.f, FVector2f(10.f, 10.f));
	return SNew(SBox).WidthOverride(10.f).HeightOverride(10.f)[ SNew(SImage).Image(&Dot).ColorAndOpacity(Color) ];
}

TSharedRef<SWidget> CXMRPanelUI::MakeChoice(const FText& Label, const TAttribute<bool>& IsSelected, TFunction<FReply()> OnClicked,
	const TAttribute<bool>& IsEnabled)
{
	return SNew(SButton).ButtonStyle(FlatButtonStyle()).IsEnabled(IsEnabled)
		.OnClicked_Lambda([OnClicked] { return OnClicked ? OnClicked() : FReply::Handled(); })
		[
			SNew(SBorder).BorderImage_Lambda([IsSelected] { return ChoiceBrush(IsSelected.Get()); }).Padding(FMargin(14.f, 11.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)
				[ SNew(SBox).WidthOverride(18.f).HeightOverride(18.f)[ SNew(SImage).Image_Lambda([IsSelected] { return RadioBrush(IsSelected.Get()); }) ] ]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(Label)
					.Font_Lambda([IsSelected] { return IsSelected.Get() ? StrongFont(13) : BodyFont(13); })
					.ColorAndOpacity_Lambda([IsSelected] { return FSlateColor(IsSelected.Get() ? SelectionColor() : InkColor()); })
				]
			]
		];
}

TSharedRef<SWidget> CXMRPanelUI::MakePill(const FText& Label, const TAttribute<bool>& IsSelected, TFunction<FReply()> OnClicked,
	const TAttribute<bool>& IsEnabled)
{
	return SNew(SButton).ButtonStyle(FlatButtonStyle()).IsEnabled(IsEnabled)
		.OnClicked_Lambda([OnClicked] { return OnClicked ? OnClicked() : FReply::Handled(); })
		[
			SNew(SBorder).BorderImage_Lambda([IsSelected] { return PillBrush(IsSelected.Get()); }).Padding(FMargin(18.f, 10.f))
			[
				SNew(STextBlock).Text(Label)
				.Font_Lambda([IsSelected] { return IsSelected.Get() ? StrongFont(13) : BodyFont(13); })
				.ColorAndOpacity_Lambda([IsSelected] { return FSlateColor(IsSelected.Get() ? FLinearColor::White : InkColor()); })
			]
		];
}

TSharedRef<SWidget> CXMRPanelUI::MakeNavItem(const FText& Label, const TAttribute<bool>& IsActive, TFunction<FReply()> OnClicked)
{
	static const FSlateRoundedBoxBrush Active(SurfaceColor(), ControlRadius);
	static const FSlateNoResource Idle;
	return SNew(SButton).ButtonStyle(FlatButtonStyle())
		.OnClicked_Lambda([OnClicked] { return OnClicked ? OnClicked() : FReply::Handled(); })
		[
			SNew(SBorder).Padding(0.f).BorderImage_Lambda([IsActive]() -> const FSlateBrush* { return IsActive.Get() ? static_cast<const FSlateBrush*>(&Active) : static_cast<const FSlateBrush*>(&Idle); })
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[ SNew(SBox).WidthOverride(3.f)[ SNew(SImage).Image(WhiteBrush())
					.ColorAndOpacity_Lambda([IsActive] { return FSlateColor(IsActive.Get() ? SelectionColor() : FLinearColor::Transparent); }) ] ]
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(14.f, 11.f))
				[
					SNew(STextBlock).Text(Label).Font(StrongFont(13))
					.ColorAndOpacity_Lambda([IsActive] { return FSlateColor(IsActive.Get() ? SelectionColor() : InkColor()); })
				]
			]
		];
}

TSharedRef<SWidget> CXMRPanelUI::MakeDangerButton(const TAttribute<FText>& Label, const TAttribute<bool>& IsArmed, TFunction<FReply()> OnClicked)
{
	static const FSlateRoundedBoxBrush Armed(ToneColor(ETone::Negative), ControlRadius);
	static const FSlateRoundedBoxBrush Calm(FLinearColor::Transparent, ControlRadius, Hex(226, 170, 164), 1.f);
	return SNew(SButton).ButtonStyle(FlatButtonStyle())
		.OnClicked_Lambda([OnClicked] { return OnClicked ? OnClicked() : FReply::Handled(); })
		[
			SNew(SBorder).Padding(FMargin(14.f, 7.f)).BorderImage_Lambda([IsArmed]() -> const FSlateBrush* { return IsArmed.Get() ? &Armed : &Calm; })
			[
				SNew(STextBlock).Text(Label).Font(StrongFont(12))
				.ColorAndOpacity_Lambda([IsArmed] { return FSlateColor(IsArmed.Get() ? FLinearColor::White : ToneColor(ETone::Negative)); })
			]
		];
}

TSharedRef<SWidget> CXMRPanelUI::MakeStepMarker(int32 Number, const TAttribute<int32>& State)
{
	return SNew(SBox).WidthOverride(28.f).HeightOverride(28.f)
		[
			SNew(SBorder).BorderImage_Lambda([State] { return StepBrush(State.Get()); }).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0.f)
			[
				SNew(STextBlock).Text(FText::AsNumber(Number)).Font(StrongFont(12))
				.ColorAndOpacity_Lambda([State] { const int32 Value = State.Get(); return FSlateColor(Value >= 2 ? FLinearColor::White : Value == 1 ? SelectionColor() : FaintColor()); })
			]
		];
}

TSharedRef<SWidget> CXMRPanelUI::MakeRailGroup(const TSharedRef<SWidget>& Segments)
{
	static const FSlateRoundedBoxBrush Track(RailRaisedColor(), 8.f);
	return SNew(SBorder).BorderImage(&Track).Padding(3.f)[ Segments ];
}

TSharedRef<SWidget> CXMRPanelUI::MakeRailSegment(const FText& Title, const FText& Detail, const TAttribute<bool>& IsActive,
	const TAttribute<bool>& IsEnabled, TFunction<FReply()> OnClicked)
{
	static const FSlateRoundedBoxBrush Active(SelectionColor(), ControlRadius);
	static const FSlateNoResource Idle;
	return SNew(SButton).ButtonStyle(&RailButtonStyle()).IsEnabled(IsEnabled)
		.OnClicked_Lambda([OnClicked] { return OnClicked ? OnClicked() : FReply::Handled(); })
		[
			SNew(SBorder).Padding(FMargin(18.f, 7.f)).BorderImage_Lambda([IsActive]() -> const FSlateBrush* { return IsActive.Get() ? static_cast<const FSlateBrush*>(&Active) : static_cast<const FSlateBrush*>(&Idle); })
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[ SNew(STextBlock).Text(Title).Font(HeadingFont(15))
					.ColorAndOpacity_Lambda([IsActive] { return FSlateColor(IsActive.Get() ? FLinearColor::White : RailInkColor()); }) ]
				+ SVerticalBox::Slot().AutoHeight()
				[ SNew(STextBlock).Text(Detail).Font(BodyFont(10))
					.ColorAndOpacity_Lambda([IsActive] { return FSlateColor(IsActive.Get() ? Hex(214, 227, 249) : RailMutedColor()); }) ]
			]
		];
}

// 행

TSharedRef<SWidget> CXMRPanelUI::MakeRow(const FCXMRTunable& Tunable, const FRowBinding& Binding)
{
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
	if (!Tunable.Help.IsEmpty()) { Row->SetToolTipText(Tunable.Help); }
	if (Binding.IsEnabled)
	{
		Row->SetEnabled(TAttribute<bool>::CreateLambda([IsEnabled = Binding.IsEnabled] { return IsEnabled(); }));
	}
	TSharedRef<SWidget> Framed = SNew(SBox).MinDesiredHeight(RowHeight).VAlign(VAlign_Center)[ Row ];

	if (Tunable.Kind == ECXMRTunableKind::Action)
	{
		Row->AddSlot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SButton).ButtonStyle(ButtonStyle()).TextStyle(BodyTextStyle())
			.Text(Tunable.Label)
			.OnClicked_Lambda([Invoke = Binding.Invoke] { return InvokeAction(Invoke); })
		];
		return Framed;
	}

	Row->AddSlot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(0.0f, 0.0f, 16.0f, 0.0f)
	[
		SNew(STextBlock).Font(BodyFont(12)).ColorAndOpacity(InkColor()).Text(Tunable.Label).AutoWrapText(true)
	];

	const TFunction<float()> Get = Binding.Get;
	const TFunction<void(float, bool)> Apply = Binding.Apply;

	switch (Tunable.Kind)
	{
	case ECXMRTunableKind::Bool:
		Row->AddSlot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(ControlWidth).HAlign(HAlign_Right)
			[
				SNew(SCheckBox).Style(CheckStyle())
				.IsChecked_Lambda([Get] { return (Get && Get() > 0.5f) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([Apply](ECheckBoxState State) { if (Apply) { Apply(State == ECheckBoxState::Checked ? 1.0f : 0.0f, true); } })
			]
		];
		break;

	case ECXMRTunableKind::Float:
	{
		const float Min = Tunable.Min;
		const float Span = FMath::Max(KINDA_SMALL_NUMBER, Tunable.Max - Min);
		Row->AddSlot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(ControlWidth)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)
				[
					SNew(SSlider).Style(SliderStyle()).StepSize(Tunable.Delta / Span)
					.Value_Lambda([Get, Min, Span] { return Get ? FMath::Clamp((Get() - Min) / Span, 0.f, 1.f) : 0.f; })
					.OnValueChanged_Lambda([Apply, Min, Span](float Value) { if (Apply) { Apply(Min + Value * Span, false); } })
					.OnMouseCaptureEnd_Lambda([Get, Apply] { if (Get && Apply) { Apply(Get(), true); } })
					.OnControllerCaptureEnd_Lambda([Get, Apply] { if (Get && Apply) { Apply(Get(), true); } })
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(76.0f)
					[
						SNew(SSpinBox<float>).Style(SpinStyle()).Font(BodyFont(12))
						.MinValue(Tunable.Min).MaxValue(Tunable.Max)
						.MinSliderValue(Tunable.Min).MaxSliderValue(Tunable.Max)
						.Delta(Tunable.Delta)
						.Value_Lambda([Get] { return Get ? Get() : 0.0f; })
						// 조작 중에는 바로 적용하고 조작이 끝나면 저장함.
						.OnValueChanged_Lambda([Apply](float Value) { if (Apply) { Apply(Value, false); } })
						.OnValueCommitted_Lambda([Apply](float Value, ETextCommit::Type) { if (Apply) { Apply(Value, true); } })
						.OnEndSliderMovement_Lambda([Apply](float Value) { if (Apply) { Apply(Value, true); } })
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SBox).WidthOverride(30.0f)[ SNew(STextBlock).Font(BodyFont(11)).ColorAndOpacity(ReadoutColor()).Text(Tunable.Unit) ]
				]
			]
		];
		break;
	}

	case ECXMRTunableKind::Choice:
	{
		const TArray<FText> Options = Tunable.Options;
		Row->AddSlot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(ControlWidth).HAlign(HAlign_Right)
			[
				SNew(SBox).WidthOverride(220.0f)
				[
					SNew(SComboButton).ButtonStyle(ButtonStyle()).ForegroundColor(InkColor())
					.ButtonContent()
					[ SNew(STextBlock).Font(BodyFont(12)).Text_Lambda([Get, Options] { return ChoiceText(Get, Options); }) ]
					.OnGetMenuContent_Lambda([Get, Apply, Options] { return ChoiceMenu(Get, Apply, Options); })
				]
			]
		];
		break;
	}

	case ECXMRTunableKind::Stepper:
	{
		// 현재 항목과 순번을 함께 표시함.
		const TFunction<void(float)> Step = Binding.Step;
		TSharedRef<SHorizontalBox> Controls = SNew(SHorizontalBox);
		if (Tunable.Text && Binding.Text)
		{
			Controls->AddSlot().FillWidth(1.f).VAlign(VAlign_Center).HAlign(HAlign_Right).Padding(0.0f, 0.0f, 10.0f, 0.0f)
			[ SNew(STextBlock).Font(BodyFont(12)).ColorAndOpacity(ReadoutColor()).Text_Lambda([Text = Binding.Text] { return Text(); }) ];
		}
		for (const float Direction : { -1.f, 1.f })
		{
			// SButton은 스타일 여백과 ContentPadding을 더함. 고정 폭 버튼은 둘 다 0으로 둬야 기호가 보임.
			Controls->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(Direction > 0.f ? 6.f : 0.f, 0.f, 0.f, 0.f)
			[
				SNew(SBox).WidthOverride(38.0f).HeightOverride(32.0f)
				[
					SNew(SButton).ButtonStyle(ButtonStyle())
					.ContentPadding(FMargin(0.f)).NormalPaddingOverride(FMargin(0.f)).PressedPaddingOverride(FMargin(0.f))
					.HAlign(HAlign_Center).VAlign(VAlign_Center)
					.ToolTipText(Direction > 0.f ? LOCTEXT("PlusTip", "Increase (+)") : LOCTEXT("MinusTip", "Decrease (-)"))
					.OnClicked_Lambda([Step, Direction] { if (Step) { Step(Direction); } return FReply::Handled(); })
					[
						SNew(STextBlock).Font(StrongFont(15))
						.Text(Direction > 0.f ? LOCTEXT("Plus", "+") : LOCTEXT("Minus", "−"))
					]
				]
			];
		}
		Row->AddSlot().AutoWidth().VAlign(VAlign_Center)[ SNew(SBox).WidthOverride(ControlWidth).HAlign(HAlign_Right)[ Controls ] ];
		break;
	}

	case ECXMRTunableKind::Readout:
		Row->AddSlot().FillWidth(1.4f).VAlign(VAlign_Center)
		[
			SNew(STextBlock).Font(BodyFont(12))
			.AutoWrapText(true)
			.ColorAndOpacity(ReadoutColor())
			.Text_Lambda([Text = Binding.Text] { return Text ? Text() : FText::GetEmpty(); })
		];
		return Framed;

	default:
		break;
	}

	// 저장값이 없는 행도 같은 오른쪽 끝에 맞춤.
	Row->AddSlot().AutoWidth().VAlign(VAlign_Center)[ ResetSlot(Binding.Reset) ];
	return Framed;
}

TSharedRef<SWidget> CXMRPanelUI::MakeRowList(const TArray<FRowSpec>& Rows, bool bShowHeadings)
{
	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);

	// 등록 순서와 관계없이 같은 분류끼리 모음.
	TArray<FString> Categories;
	for (const FRowSpec& Row : Rows)
	{
		Categories.AddUnique(Row.Tunable.Category.ToString());
	}

	for (int32 CategoryIndex = 0; CategoryIndex < Categories.Num(); ++CategoryIndex)
	{
		const FString& Category = Categories[CategoryIndex];
		if (bShowHeadings)
		{
			List->AddSlot().AutoHeight().Padding(0.f, CategoryIndex == 0 ? 0.f : 30.f, 0.f, 6.f)[ MakeHeader(FText::FromString(Category)) ];
		}
		else if (CategoryIndex > 0)
		{
			List->AddSlot().AutoHeight()[ MakeDivider() ];
		}
		bool bFirst = true;
		for (const FRowSpec& Row : Rows)
		{
			if (Row.Tunable.Category.ToString() != Category) { continue; }
			if (!bFirst) { List->AddSlot().AutoHeight()[ MakeDivider() ]; }
			List->AddSlot().AutoHeight().Padding(0.f, 4.f)[ MakeRow(Row.Tunable, Row.Binding) ];
			bFirst = false;
		}
	}
	return List;
}

TSharedRef<SWidget> CXMRPanelUI::MakeTabBar(const TArray<FText>& Labels, TFunction<int32()> GetActive, TFunction<void(int32)> Select)
{
	static const FSlateRoundedBoxBrush Track(Hex(222, 227, 234), 8.f);
	static const FSlateRoundedBoxBrush Active(SurfaceColor(), ControlRadius);
	static const FSlateNoResource Idle;
	TSharedRef<SHorizontalBox> Bar = SNew(SHorizontalBox);
	for (int32 Index = 0; Index < Labels.Num(); ++Index)
	{
		Bar->AddSlot().FillWidth(1.0f)
		[
			SNew(SButton).ButtonStyle(FlatButtonStyle())
			.OnClicked_Lambda([Select, Index] { if (Select) { Select(Index); } return FReply::Handled(); })
			[
				SNew(SBorder).HAlign(HAlign_Center).Padding(FMargin(12.f, 8.f))
				.BorderImage_Lambda([GetActive, Index]() -> const FSlateBrush* { return GetActive && GetActive() == Index ? static_cast<const FSlateBrush*>(&Active) : static_cast<const FSlateBrush*>(&Idle); })
				[
					SNew(STextBlock).Text(Labels[Index]).Font(StrongFont(12))
					.ColorAndOpacity_Lambda([GetActive, Index] { return FSlateColor(GetActive && GetActive() == Index ? SelectionColor() : ReadoutColor()); })
				]
			]
		];
	}
	return SNew(SBorder).BorderImage(&Track).Padding(3.f)[ Bar ];
}

TSharedRef<SWidget> CXMRPanelUI::MakeScroll(const TSharedRef<SWidget>& Content)
{
	return SNew(SScrollBox).ScrollBarStyle(ScrollBarStyle()).ScrollBarThickness(FVector2D(6.f, 6.f)).ScrollBarPadding(FMargin(6.f, 2.f))
		+ SScrollBox::Slot().Padding(0.f, 0.f, 4.f, 0.f)[ Content ];
}

TSharedRef<SWidget> CXMRPanelUI::MakeBackground(const TSharedRef<SWidget>& Content)
{
	return SNew(SBorder)
		.BorderImage(WhiteBrush())
		.BorderBackgroundColor(BackgroundColor())
		.ForegroundColor(InkColor())
		.Padding(0.0f)
		[
			Content
		];
}

#undef LOCTEXT_NAMESPACE
