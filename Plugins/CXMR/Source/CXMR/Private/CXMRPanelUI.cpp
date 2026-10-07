// Copyright GMTCK CX.

#include "CXMRPanelUI.h"

#include "Styling/CoreStyle.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Framework/Application/SlateApplication.h"
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

	TSharedRef<SWidget> ChoiceMenu(const TFunction<void(float, bool)>& Apply, const TArray<FText>& Options)
	{
		TSharedRef<SVerticalBox> Choices = SNew(SVerticalBox);
		for (int32 Index = 0; Index < Options.Num(); ++Index)
		{
			Choices->AddSlot().AutoHeight()
			[
				SNew(SButton).ButtonStyle(CXMRPanelUI::ButtonStyle()).TextStyle(CXMRPanelUI::BodyTextStyle())
				.ContentPadding(FMargin(12.f, 8.f)).Text(Options[Index])
				.OnClicked_Lambda([Apply, Index] { return ApplyChoice(Apply, Index); })
			];
		}
		return Choices;
	}

	FSlateColor TabBackground(const TFunction<int32()>& GetActive, int32 Index)
	{
		return GetActive && GetActive() == Index
			? FLinearColor::FromSRGBColor(FColor(224, 235, 251)) : FLinearColor::White;
	}

	FSlateColor TabInk(const TFunction<int32()>& GetActive, int32 Index)
	{
		return GetActive && GetActive() == Index ? CXMRPanelUI::SelectionColor() : CXMRPanelUI::InkColor();
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
}

FLinearColor CXMRPanelUI::BackgroundColor() { return FLinearColor::FromSRGBColor(FColor(238, 242, 247)); }
FLinearColor CXMRPanelUI::InkColor() { return FLinearColor::FromSRGBColor(FColor(23, 37, 56)); }
FLinearColor CXMRPanelUI::HeaderColor() { return InkColor(); }
FLinearColor CXMRPanelUI::ReadoutColor() { return FLinearColor::FromSRGBColor(FColor(64, 83, 108)); }
FLinearColor CXMRPanelUI::SelectionColor() { return FLinearColor::FromSRGBColor(FColor(36, 102, 206)); }
const FTextBlockStyle* CXMRPanelUI::BodyTextStyle()
{
	static const FTextBlockStyle Style = FTextBlockStyle().SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 13)).SetColorAndOpacity(FSlateColor::UseForeground());
	return &Style;
}
const FButtonStyle* CXMRPanelUI::ButtonStyle()
{
	static const FButtonStyle Style = FButtonStyle()
		.SetNormal(FSlateRoundedBoxBrush(FLinearColor::White, 5.f, FLinearColor::FromSRGBColor(FColor(196, 207, 222)), 1.f))
		.SetHovered(FSlateRoundedBoxBrush(FLinearColor::FromSRGBColor(FColor(224, 235, 251)), 5.f, SelectionColor(), 1.f))
		.SetPressed(FSlateRoundedBoxBrush(FLinearColor::FromSRGBColor(FColor(199, 220, 250)), 5.f, SelectionColor(), 1.f))
		.SetDisabled(FSlateRoundedBoxBrush(BackgroundColor(), 5.f))
		.SetNormalForeground(InkColor()).SetHoveredForeground(InkColor()).SetPressedForeground(InkColor()).SetDisabledForeground(ReadoutColor())
		.SetNormalPadding(FMargin(12.f, 8.f)).SetPressedPadding(FMargin(12.f, 8.f));
	return &Style;
}
const FSpinBoxStyle* CXMRPanelUI::SpinStyle()
{
	static const FSpinBoxStyle Style = FSpinBoxStyle(FCoreStyle::Get().GetWidgetStyle<FSpinBoxStyle>("SpinBox"))
		.SetBackgroundBrush(FSlateRoundedBoxBrush(BackgroundColor(), 4.f))
		.SetActiveBackgroundBrush(FSlateRoundedBoxBrush(FLinearColor::White, 4.f, SelectionColor(), 1.f))
		.SetInactiveFillBrush(FSlateRoundedBoxBrush(FLinearColor::Transparent, 4.f))
		.SetActiveFillBrush(FSlateRoundedBoxBrush(FLinearColor::Transparent, 4.f))
		.SetForegroundColor(InkColor());
	return &Style;
}
TSharedRef<SWidget> CXMRPanelUI::MakeSurface(const TSharedRef<SWidget>& Content)
{
	return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor::White).ForegroundColor(InkColor()).Padding(20.f)[Content];
}

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

TSharedRef<SWidget> CXMRPanelUI::MakeHeader(const FText& Category)
{
	return SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
		.Text(Category)
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 17))
		.ColorAndOpacity(HeaderColor());
}

TSharedRef<SWidget> CXMRPanelUI::MakeRow(const FCXMRTunable& Tunable, const FRowBinding& Binding)
{
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
	if (!Tunable.Help.IsEmpty()) { Row->SetToolTipText(Tunable.Help); }
	if (Binding.IsEnabled)
	{
		Row->SetEnabled(TAttribute<bool>::CreateLambda([IsEnabled = Binding.IsEnabled] { return IsEnabled(); }));
	}

	if (Tunable.Kind == ECXMRTunableKind::Action)
	{
		Row->AddSlot().FillWidth(1.0f)
		[
			SNew(SButton).ButtonStyle(ButtonStyle()).TextStyle(BodyTextStyle()).ContentPadding(FMargin(12.f, 9.f))
			.HAlign(HAlign_Center)
			.Text(Tunable.Label)
			.OnClicked_Lambda([Invoke = Binding.Invoke] { return InvokeAction(Invoke); })
		];
		return Row;
	}

	Row->AddSlot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
	[
		SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13)).Text(Tunable.Label).AutoWrapText(true)
	];

	const TFunction<float()> Get = Binding.Get;
	const TFunction<void(float, bool)> Apply = Binding.Apply;

	switch (Tunable.Kind)
	{
	case ECXMRTunableKind::Bool:
		Row->AddSlot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([Get] { return (Get && Get() > 0.5f) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([Apply](ECheckBoxState State) { if (Apply) { Apply(State == ECheckBoxState::Checked ? 1.0f : 0.0f, true); } })
		];
		break;

	case ECXMRTunableKind::Float:
	{
		const float Min = Tunable.Min;
		const float Span = FMath::Max(KINDA_SMALL_NUMBER, Tunable.Max - Min);
		Row->AddSlot().FillWidth(0.7f).VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)
		[
			SNew(SSlider).StepSize(Tunable.Delta / Span)
			.Value_Lambda([Get, Min, Span] { return Get ? FMath::Clamp((Get() - Min) / Span, 0.f, 1.f) : 0.f; })
			.OnValueChanged_Lambda([Apply, Min, Span](float Value) { if (Apply) { Apply(Min + Value * Span, false); } })
			.OnMouseCaptureEnd_Lambda([Get, Apply] { if (Get && Apply) { Apply(Get(), true); } })
			.OnControllerCaptureEnd_Lambda([Get, Apply] { if (Get && Apply) { Apply(Get(), true); } })
		];
		Row->AddSlot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(95.0f)
			[
				SNew(SSpinBox<float>).Style(SpinStyle()).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
				.MinValue(Tunable.Min).MaxValue(Tunable.Max)
				.MinSliderValue(Tunable.Min).MaxSliderValue(Tunable.Max)
				.Delta(Tunable.Delta)
				.Value_Lambda([Get] { return Get ? Get() : 0.0f; })
				// 조작 중에는 바로 적용하고 조작이 끝나면 저장함.
				.OnValueChanged_Lambda([Apply](float Value) { if (Apply) { Apply(Value, false); } })
				.OnValueCommitted_Lambda([Apply](float Value, ETextCommit::Type) { if (Apply) { Apply(Value, true); } })
				.OnEndSliderMovement_Lambda([Apply](float Value) { if (Apply) { Apply(Value, true); } })
			]
		];
		Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SBox).WidthOverride(52.0f) [ SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13)).Text(Tunable.Unit) ]
		];
		break;
	}

	case ECXMRTunableKind::Choice:
	{
		const TArray<FText> Options = Tunable.Options;
		Row->AddSlot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(150.0f)
			[
				SNew(SComboButton).ButtonStyle(ButtonStyle())
				.ButtonContent()
				[ SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13)).Text_Lambda([Get, Options] { return ChoiceText(Get, Options); }) ]
				.OnGetMenuContent_Lambda([Apply, Options] { return ChoiceMenu(Apply, Options); })
			]
		];
		break;
	}

	case ECXMRTunableKind::Stepper:
	{
		// 현재 항목과 순번을 함께 표시함.
		if (Tunable.Text && Binding.Text)
		{
			Row->AddSlot().FillWidth(1.f).VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
				.ColorAndOpacity(ReadoutColor())
					.Text_Lambda([Text = Binding.Text] { return Text(); })
					.AutoWrapText(true)
			];
		}
		const TFunction<void(float)> Step = Binding.Step;
		Row->AddSlot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(44.0f)
			[
				SNew(SButton).ButtonStyle(ButtonStyle()).TextStyle(BodyTextStyle()).ContentPadding(FMargin(12.f, 9.f)).HAlign(HAlign_Center).Text(LOCTEXT("Minus", "-"))
				.OnClicked_Lambda([Step] { if (Step) { Step(-1.0f); } return FReply::Handled(); })
			]
		];
		Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SBox).WidthOverride(44.0f)
			[
				SNew(SButton).ButtonStyle(ButtonStyle()).TextStyle(BodyTextStyle()).ContentPadding(FMargin(12.f, 9.f)).HAlign(HAlign_Center).Text(LOCTEXT("Plus", "+"))
				.OnClicked_Lambda([Step] { if (Step) { Step(1.0f); } return FReply::Handled(); })
			]
		];
		break;
	}

	case ECXMRTunableKind::Readout:
		Row->AddSlot().FillWidth(1.5f).VAlign(VAlign_Center)
		[
			SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
			.AutoWrapText(true)
			.ColorAndOpacity(ReadoutColor())
			.Text_Lambda([Text = Binding.Text] { return Text ? Text() : FText::GetEmpty(); })
		];
		break;

	default:
		break;
	}

	if (Binding.Reset)
	{
		Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SButton).ButtonStyle(ButtonStyle()).TextStyle(BodyTextStyle()).ContentPadding(FMargin(12.f, 9.f))
			.Text(LOCTEXT("Default", "Default"))
			.ToolTipText(LOCTEXT("DefaultTip", "Back to the default value, and forget the saved one"))
			.OnClicked_Lambda([Reset = Binding.Reset] { Reset(); return FReply::Handled(); })
		];
	}
	return Row;
}

TSharedRef<SWidget> CXMRPanelUI::MakeRowList(const TArray<FRowSpec>& Rows)
{
	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);

	// 등록 순서와 관계없이 같은 분류끼리 모음.
	TArray<FString> Categories;
	for (const FRowSpec& Row : Rows)
	{
		Categories.AddUnique(Row.Tunable.Category.ToString());
	}

	for (const FString& Category : Categories)
	{
		TSharedRef<SVerticalBox> Group = SNew(SVerticalBox);
		Group->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 16.f)[ MakeHeader(FText::FromString(Category)) ];
		for (const FRowSpec& Row : Rows)
		{
			if (Row.Tunable.Category.ToString() == Category)
			{
				Group->AddSlot().AutoHeight().Padding(0.f, 9.f)[ MakeRow(Row.Tunable, Row.Binding) ];
			}
		}
		List->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 16.f)[ MakeSurface(Group) ];
	}
	return List;
}

TSharedRef<SWidget> CXMRPanelUI::MakeTabBar(const TArray<FText>& Labels, TFunction<int32()> GetActive, TFunction<void(int32)> Select)
{
	TSharedRef<SHorizontalBox> Bar = SNew(SHorizontalBox);
	for (int32 Index = 0; Index < Labels.Num(); ++Index)
	{
		Bar->AddSlot().FillWidth(1.0f).Padding(2.0f, 0.0f)
		[
			SNew(SButton).ButtonStyle(ButtonStyle()).TextStyle(BodyTextStyle()).ContentPadding(FMargin(12.f, 9.f))
			.HAlign(HAlign_Center)
			.ContentPadding(FMargin(6.0f, 6.0f))
			.ButtonColorAndOpacity_Lambda([GetActive, Index] { return TabBackground(GetActive, Index); })
			.OnClicked_Lambda([Select, Index] { if (Select) { Select(Index); } return FReply::Handled(); })
			[
				SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
				.Text(Labels[Index])
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
				.ColorAndOpacity_Lambda([GetActive, Index] { return TabInk(GetActive, Index); })
			]
		];
	}
	return Bar;
}

TSharedRef<SWidget> CXMRPanelUI::MakeScroll(const TSharedRef<SWidget>& Content)
{
	return SNew(SScrollBox) + SScrollBox::Slot()[ Content ];
}

TSharedRef<SWidget> CXMRPanelUI::MakeBackground(const TSharedRef<SWidget>& Content)
{
	return SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(BackgroundColor())
		.ForegroundColor(InkColor())
		.Padding(0.0f)
		[
			Content
		];
}

#undef LOCTEXT_NAMESPACE
