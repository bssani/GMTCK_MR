// Copyright GMTCK CX.

#include "CXMRPanelUI.h"

#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "CXMRPanelUI"

namespace
{
	bool HasRange(ECXMRTunableKind Kind)
	{
		return Kind == ECXMRTunableKind::Bool || Kind == ECXMRTunableKind::Float || Kind == ECXMRTunableKind::Choice;
	}

	float RangeMax(const FCXMRTunable& Tunable)
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

FLinearColor CXMRPanelUI::BackgroundColor() { return FLinearColor(0.035f, 0.037f, 0.042f); }
FLinearColor CXMRPanelUI::HeaderColor()     { return FLinearColor(0.55f, 0.75f, 1.0f); }
FLinearColor CXMRPanelUI::ReadoutColor()    { return FLinearColor(0.75f, 0.85f, 0.75f); }

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
		const bool bClamp = HasRange(Tunable.Kind);
		const float Min = bClamp ? (Tunable.Kind == ECXMRTunableKind::Float ? Tunable.Min : 0.0f) : 0.0f;
		const float Max = RangeMax(Tunable);
		Binding.Apply = [Set = Tunable.Set, bClamp, Min, Max](float Value, bool)
		{
			Set(bClamp ? FMath::Clamp(Value, Min, Max) : Value);
		};
	}
	Binding.Step = Tunable.Step;
	Binding.Invoke = Tunable.Invoke;
	Binding.Text = Tunable.Text;
	Binding.IsEnabled = Tunable.IsEnabled;
	return Binding;
}

TSharedRef<SWidget> CXMRPanelUI::MakeHeader(const FText& Category)
{
	return SNew(STextBlock)
		.Text(Category)
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
		.ColorAndOpacity(HeaderColor());
}

TSharedRef<SWidget> CXMRPanelUI::MakeRow(const FCXMRTunable& Tunable, const FRowBinding& Binding)
{
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
	if (Binding.IsEnabled)
	{
		Row->SetEnabled(TAttribute<bool>::CreateLambda([IsEnabled = Binding.IsEnabled] { return IsEnabled(); }));
	}

	if (Tunable.Kind == ECXMRTunableKind::Action)
	{
		Row->AddSlot().FillWidth(1.0f)
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
			.Text(Tunable.Label)
			.OnClicked_Lambda([Invoke = Binding.Invoke]
			{
				if (Invoke) { Invoke(); }
				return FReply::Handled();
			})
		];
		return Row;
	}

	Row->AddSlot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
	[
		SNew(STextBlock).Text(Tunable.Label)
	];

	const TFunction<float()> Get = Binding.Get;
	const TFunction<void(float, bool)> Apply = Binding.Apply;

	switch (Tunable.Kind)
	{
	case ECXMRTunableKind::Bool:
		Row->AddSlot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([Get]
			{
				return (Get && Get() > 0.5f) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([Apply](ECheckBoxState State)
			{
				if (Apply) { Apply(State == ECheckBoxState::Checked ? 1.0f : 0.0f, true); }
			})
		];
		break;

	case ECXMRTunableKind::Float:
		Row->AddSlot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(150.0f)
			[
				SNew(SSpinBox<float>)
				.MinValue(Tunable.Min).MaxValue(Tunable.Max)
				.MinSliderValue(Tunable.Min).MaxSliderValue(Tunable.Max)
				.Delta(Tunable.Delta)
				.Value_Lambda([Get] { return Get ? Get() : 0.0f; })
				// Applied live while dragging, committed once the drag or the typed edit ends.
				.OnValueChanged_Lambda([Apply](float Value) { if (Apply) { Apply(Value, false); } })
				.OnValueCommitted_Lambda([Apply](float Value, ETextCommit::Type) { if (Apply) { Apply(Value, true); } })
				.OnEndSliderMovement_Lambda([Apply](float Value) { if (Apply) { Apply(Value, true); } })
			]
		];
		Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SBox).WidthOverride(52.0f) [ SNew(STextBlock).Text(Tunable.Unit) ]
		];
		break;

	case ECXMRTunableKind::Choice:
	{
		const TArray<FText> Options = Tunable.Options;
		Row->AddSlot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(150.0f)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.Text_Lambda([Get, Options]
				{
					const int32 Index = Get ? FMath::RoundToInt(Get()) : 0;
					return Options.IsValidIndex(Index) ? Options[Index] : FText::GetEmpty();
				})
				.OnClicked_Lambda([Get, Apply, Count = Options.Num()]
				{
					if (Get && Apply && Count > 0)
					{
						Apply(static_cast<float>((FMath::RoundToInt(Get()) + 1) % Count), true);
					}
					return FReply::Handled();
				})
			]
		];
		break;
	}

	case ECXMRTunableKind::Stepper:
	{
		// A stepper that also says what it is stepping through ("Vehicle  Sedan 2 / 3  [-] [+]").
		if (Tunable.Text && Binding.Text)
		{
			Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(STextBlock)
				.ColorAndOpacity(ReadoutColor())
				.Text_Lambda([Text = Binding.Text] { return Text(); })
			];
		}
		const TFunction<void(float)> Step = Binding.Step;
		Row->AddSlot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(44.0f)
			[
				SNew(SButton).HAlign(HAlign_Center).Text(LOCTEXT("Minus", "-"))
				.OnClicked_Lambda([Step] { if (Step) { Step(-1.0f); } return FReply::Handled(); })
			]
		];
		Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SBox).WidthOverride(44.0f)
			[
				SNew(SButton).HAlign(HAlign_Center).Text(LOCTEXT("Plus", "+"))
				.OnClicked_Lambda([Step] { if (Step) { Step(1.0f); } return FReply::Handled(); })
			]
		];
		break;
	}

	case ECXMRTunableKind::Readout:
		Row->AddSlot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(STextBlock)
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
			SNew(SButton)
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

	// Features register as their actors begin play, so rows of one category can arrive between rows of another.
	TArray<FString> Categories;
	for (const FRowSpec& Row : Rows)
	{
		Categories.AddUnique(Row.Tunable.Category.ToString());
	}

	for (const FString& Category : Categories)
	{
		List->AddSlot().AutoHeight().Padding(10.0f, 14.0f, 10.0f, 4.0f)[ MakeHeader(FText::FromString(Category)) ];
		for (const FRowSpec& Row : Rows)
		{
			if (Row.Tunable.Category.ToString() == Category)
			{
				List->AddSlot().AutoHeight().Padding(10.0f, 2.0f)[ MakeRow(Row.Tunable, Row.Binding) ];
			}
		}
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
			SNew(SButton)
			.HAlign(HAlign_Center)
			.ContentPadding(FMargin(6.0f, 6.0f))
			.ButtonColorAndOpacity_Lambda([GetActive, Index]
			{
				return (GetActive && GetActive() == Index)
					? FSlateColor(HeaderColor() * 0.55f)
					: FSlateColor(FLinearColor(0.10f, 0.11f, 0.13f));
			})
			.OnClicked_Lambda([Select, Index] { if (Select) { Select(Index); } return FReply::Handled(); })
			[
				SNew(STextBlock)
				.Text(Labels[Index])
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
				.ColorAndOpacity_Lambda([GetActive, Index]
				{
					return (GetActive && GetActive() == Index)
						? FSlateColor(FLinearColor(0.95f, 0.97f, 1.0f))
						: FSlateColor(FLinearColor(0.50f, 0.53f, 0.58f));
				})
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
		.Padding(4.0f)
		[
			Content
		];
}

#undef LOCTEXT_NAMESPACE
