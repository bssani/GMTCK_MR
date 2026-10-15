// Copyright GMTCK CX.
#pragma once
#include "CXMRPanelUI.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace CXMRShowroomUI
{
	inline FLinearColor ShowroomColor(uint8 R, uint8 G, uint8 B) { return FLinearColor::FromSRGBColor(FColor(R,G,B)); }
	inline const FSlateBrush* Frame()
	{
		static const FSlateRoundedBoxBrush Brush(FLinearColor::White,6.f,CXMRPanelUI::HairlineColor(),1.f);
		return &Brush;
	}
	inline const FButtonStyle* Button(bool bPrimary = false)
	{
		static const FButtonStyle Plain = FButtonStyle()
			.SetNormal(FSlateRoundedBoxBrush(FLinearColor::Transparent,6.f,ShowroomColor(102,116,138),1.f))
			.SetHovered(FSlateRoundedBoxBrush(ShowroomColor(28,37,52),6.f,ShowroomColor(102,116,138),1.f))
			.SetPressed(FSlateRoundedBoxBrush(ShowroomColor(28,37,52),6.f,ShowroomColor(91,140,255),1.f))
			.SetDisabled(FSlateRoundedBoxBrush(FLinearColor::Transparent,6.f,ShowroomColor(46,58,78),1.f))
			.SetNormalForeground(CXMRPanelUI::InkColor()).SetHoveredForeground(CXMRPanelUI::InkColor()).SetPressedForeground(CXMRPanelUI::InkColor())
			.SetDisabledForeground(CXMRPanelUI::ReadoutColor()).SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
		static const FButtonStyle Primary = FButtonStyle(Plain)
			.SetNormal(FSlateRoundedBoxBrush(ShowroomColor(91,140,255),6.f)).SetHovered(FSlateRoundedBoxBrush(ShowroomColor(122,162,255),6.f))
			.SetPressed(FSlateRoundedBoxBrush(ShowroomColor(91,140,255),6.f)).SetNormalForeground(ShowroomColor(11,15,23)).SetHoveredForeground(ShowroomColor(11,15,23)).SetPressedForeground(ShowroomColor(11,15,23));
		return bPrimary ? &Primary : &Plain;
	}
	inline TSharedRef<STextBlock> Text(TAttribute<FText> Value, float Px = 15.f, int32 Weight = 400, FLinearColor Color = CXMRPanelUI::InkColor())
	{
		return SNew(STextBlock).Text(Value).Font(CXMRPanelUI::CSSFont(Px,Weight)).ColorAndOpacity(Color).AutoWrapText(true);
	}
	inline TSharedRef<SWidget> Dot(TAttribute<FLinearColor> Color,float Size=8.f)
	{
		static const FSlateRoundedBoxBrush Circle(FLinearColor::White,10.f);
		return SNew(SBox).WidthOverride(Size).HeightOverride(Size)[SNew(SBorder).Padding(0).BorderImage(&Circle).BorderBackgroundColor_Lambda([Color]{return FSlateColor(Color.Get());})];
	}

	inline TSharedRef<SWidget> Caption(TAttribute<FText> Value) { return Text(Value,13,400,CXMRPanelUI::ReadoutColor()); }
	inline TSharedRef<SWidget> QuietAction(TAttribute<FText> Value,TFunction<void()> Run)
	{
		return SNew(SButton).ButtonStyle(CXMRPanelUI::QuietButtonStyle()).ContentPadding(0)
			.OnClicked_Lambda([Run]{Run();return FReply::Handled();})
			[Text(Value,13,400,CXMRPanelUI::ReadoutColor())];
	}

	inline TSharedRef<SWidget> Action(TAttribute<FText> Value, TFunction<void()> Run, TAttribute<bool> Enabled = true, bool Primary = false)
	{
		return SNew(SBox).MinDesiredHeight(40)
		[ SNew(SButton).ButtonStyle(Button(Primary)).ContentPadding(FMargin(16,8)).IsEnabled(Enabled)
			.OnClicked_Lambda([Run] { if (Run) { Run(); } return FReply::Handled(); })
			[ Text(Value,13,500,Primary ? ShowroomColor(11,15,23) : CXMRPanelUI::InkColor()) ] ];
	}
	inline TSharedRef<SWidget> SelectAction(const FText& Value,TFunction<void()> Run,TAttribute<bool> Selected,TAttribute<bool> Enabled=true,bool Filled=true)
	{
		static const FSlateRoundedBoxBrush On(ShowroomColor(91,140,255),6.f),Outline(ShowroomColor(28,37,52),6.f,ShowroomColor(102,116,138),1.f),Off(ShowroomColor(28,37,52),6.f,ShowroomColor(102,116,138),1.f);
		return SNew(SButton).ButtonStyle(CXMRPanelUI::FlatButtonStyle()).ContentPadding(0).IsEnabled(Enabled)
			.OnClicked_Lambda([Run]{Run();return FReply::Handled();})
			[SNew(SBorder).Padding(FMargin(10,8)).BorderImage_Lambda([Selected,Filled]{return Selected.Get()?(Filled?&On:&Outline):&Off;})
				[SNew(STextBlock).Text(Value).Font(CXMRPanelUI::CSSFont(13,500))
					.ColorAndOpacity_Lambda([Selected,Filled]{return Selected.Get()?(Filled?ShowroomColor(11,15,23):CXMRPanelUI::SelectionColor()):CXMRPanelUI::ReadoutColor();})]];
	}

	inline TSharedRef<SWidget> VehicleChoice(const FText& Label,TAttribute<bool> Selected,TFunction<void()> Run)
	{
		static const FSlateRoundedBoxBrush On(ShowroomColor(28,37,52),6.f,ShowroomColor(91,140,255),1.f),Off(FLinearColor::Transparent,6.f,ShowroomColor(102,116,138),1.f);
		static const FSlateRoundedBoxBrush RadioOn(ShowroomColor(91,140,255),8.f,ShowroomColor(91,140,255),2.f),RadioOff(FLinearColor::Transparent,8.f,ShowroomColor(102,116,138),2.f);
		return SNew(SButton).ButtonStyle(CXMRPanelUI::FlatButtonStyle()).ContentPadding(0).OnClicked_Lambda([Run]{Run();return FReply::Handled();})
			[SNew(SBorder).Padding(FMargin(12,10)).BorderImage_Lambda([Selected]{return Selected.Get()?&On:&Off;})
				[SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,12,0)[SNew(SBox).WidthOverride(16).HeightOverride(16)[SNew(SBorder).Padding(0).BorderImage_Lambda([Selected]{return Selected.Get()?&RadioOn:&RadioOff;})]]
				+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Text(Label,15)]]];
	}

	inline TSharedRef<SWidget> Tabs(const TArray<FString>& Names, TFunction<int32()> Active, TFunction<void(int32)> Select, TFunction<bool(int32)> Changed = nullptr)
	{
		TSharedRef<SHorizontalBox> Bar = SNew(SHorizontalBox);
		for (int32 I=0; I<Names.Num(); ++I)
		{
			Bar->AddSlot().AutoWidth().Padding(0,0,16,0)
			[ SNew(SButton).ButtonStyle(CXMRPanelUI::FlatButtonStyle()).ContentPadding(FMargin(0,12))
				.OnClicked_Lambda([Select,I] { Select(I); return FReply::Handled(); })
				[ SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[ SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(FText::FromString(Names[I])).Font(CXMRPanelUI::CSSFont(13,500)).ColorAndOpacity_Lambda([Active,I]{return Active()==I?CXMRPanelUI::InkColor():CXMRPanelUI::ReadoutColor();})]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6,0,0,0)[SNew(SBox).Visibility_Lambda([Changed,I]{return Changed && Changed(I)?EVisibility::Visible:EVisibility::Collapsed;})[Dot(ShowroomColor(242,181,68),6.f)]] ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0,10,0,0)
				[ SNew(SBox).HeightOverride(2)[ SNew(SBorder).Padding(0).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor_Lambda([Active,I] { return Active()==I ? CXMRPanelUI::SelectionColor() : FLinearColor::Transparent; }) ] ] ] ];
		}
		return SNew(SBorder).Padding(FMargin(16,0)).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(CXMRPanelUI::SurfaceColor())[ Bar ];
	}
}
