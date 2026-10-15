// Copyright GMTCK CX.
#include "SCXMRAdvancedPanel.h"
#include "CXMRShowroomUI.h"
#include "CXMRTuningWindowComponent.h"
#include "CXMRTuningSubsystem.h"
#include "CXMRSubsystem.h"
#include "CXMRPlacementComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
using namespace CXMRShowroomUI;
namespace
{
	const TArray<FString> CXMRAdvancedTabs={TEXT("MR"),TEXT("VR"),TEXT("Calibration"),TEXT("Diagnostics")};
	const TCHAR* CXMRAdvancedIntros[]={TEXT("Applied while MR is on. Depth settings take effect only in MR."),TEXT("Applied while VR is on."),TEXT("How markers are read, averaged and reused, and how the fine-adjust keys move the car."),TEXT("Overlays and readings for checking tracking. Leave the overlays off during a review.")};
	FString CXMRAdvancedDefault(const FCXMRTunable& T)
	{
		if(T.Kind==ECXMRTunableKind::Bool){return T.Default>0.5f ? TEXT("on") : TEXT("off");}
		if(!T.Options.IsEmpty() && T.Options.IsValidIndex(FMath::RoundToInt(T.Default))){return T.Options[FMath::RoundToInt(T.Default)].ToString();}
		return FString::Printf(TEXT("%g"),T.Default)+(T.Unit.IsEmpty()?TEXT(""):TEXT(" ")+T.Unit.ToString());
	}
}
void SCXMRAdvancedPanel::Construct(const FArguments& A)
{
	Control=A._Control;Tuning=A._Tuning;
	TSharedRef<SWidgetSwitcher> Pages=SNew(SWidgetSwitcher).WidgetIndex_Lambda([this]{return Control.IsValid()?Control->AdvancedTab:0;});
	for(int I=0;I<4;++I){Pages->AddSlot()[CXMRPanelUI::MakeScroll(Page(I))];}
	ChildSlot[CXMRPanelUI::MakeBackground(SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Tabs(CXMRAdvancedTabs,[this]{return Control.IsValid()?Control->AdvancedTab:0;},[this](int32 I){if(Control.IsValid()){Control->AdvancedTab=I;}},[this](int32 I){return CXMRAdvancedSettings::ChangedCount(Tuning.Get(),CXMRAdvancedTabs[I])>0;})]
		+ SVerticalBox::Slot().FillHeight(1)[Pages])];
}
TSharedRef<SWidget> SCXMRAdvancedPanel::Page(int32 Index)
{
	TSharedRef<SVerticalBox> V=SNew(SVerticalBox);
	V->AddSlot().AutoHeight()[Caption(FText::FromString(CXMRAdvancedIntros[Index]))];
	if(Tuning.IsValid())
	{
		const auto Tunables=Tuning->GetTunables();FString LastGroup;TSharedPtr<SVerticalBox> Group;
		for(const FCXMRAdvancedRow& Spec:CXMRAdvancedSettings::Rows())
		{
			if(CXMRAdvancedTabs[Index]!=Spec.Tab){continue;}
			const FCXMRTunable* const* Found=Tunables.FindByPredicate([Spec](const FCXMRTunable* T){return T->Id==Spec.Id;});
			if(!Found){continue;}
			if(LastGroup!=Spec.Group)
			{
				LastGroup=Spec.Group;
				V->AddSlot().AutoHeight().Padding(0,24,0,12)[Text(FText::FromString(LastGroup.ToUpper()),12,500,CXMRPanelUI::ReadoutColor())];
				Group=SNew(SVerticalBox);
				V->AddSlot().AutoHeight()[SNew(SBorder).BorderImage(Frame()).BorderBackgroundColor(CXMRPanelUI::SurfaceColor()).Padding(1)[Group.ToSharedRef()]];
			}
			else{Group->AddSlot().AutoHeight()[CXMRPanelUI::MakeDivider()];}
			Group->AddSlot().AutoHeight()[Row(Spec,**Found)];
		}
	}
	V->AddSlot().AutoHeight().Padding(0,24,0,0)[SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(12,12))
		+ SWrapBox::Slot()[Caption(FText::FromString(TEXT("Saved on this PC. Applies to every session until reset.")))]
		+ SWrapBox::Slot()[Action(FText::FromString(TEXT("Reset ")+CXMRAdvancedTabs[Index]+TEXT(" to defaults")),[this,Index]{CXMRAdvancedSettings::ResetTab(Tuning.Get(),CXMRAdvancedTabs[Index]);},TAttribute<bool>::CreateLambda([this,Index]{return CXMRAdvancedSettings::ChangedCount(Tuning.Get(),CXMRAdvancedTabs[Index])>0;}))]];
	return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(CXMRPanelUI::SurfaceColor()).Padding(16)[V];
}
TSharedRef<SWidget> SCXMRAdvancedPanel::Row(const FCXMRAdvancedRow& Spec,const FCXMRTunable& T)
{
	const auto Weak=Tuning;const FName Id=T.Id;
	auto Get=[Weak,Id]{return Weak.IsValid()?Weak->GetTunableValue(Id):0.f;};
	auto Apply=[Weak,Id](float Value,bool Save){if(Weak.IsValid()){Weak->ApplyValue(Id,Value,Save);}};
	const bool Setting=T.Kind!=ECXMRTunableKind::Readout && T.Kind!=ECXMRTunableKind::Action;
	auto Enabled=[Weak,Spec]{return Spec.Requires.IsNone() || (Weak.IsValid() && Weak->GetTunableValue(Spec.Requires)>0.5f);};
	TSharedRef<SVerticalBox> V=SNew(SVerticalBox);
	TSharedRef<SHorizontalBox> Top=SNew(SHorizontalBox);
	Top->AddSlot().FillWidth(1).VAlign(VAlign_Center).Padding(0,0,16,0)
	[Text(TAttribute<FText>::CreateLambda([this,T,Id]{if(Id=="Vehicle.ResetGroupAlignment"){const auto* P=Control.IsValid()?Control->FindPlacement():nullptr;return FText::Format(T.Label,FText::FromName(P?P->GetAlignmentGroup():NAME_None));}return T.Label;}),15,500)];
	TSharedRef<SWidget> C=SNullWidget::NullWidget;
	if(T.Kind==ECXMRTunableKind::Readout)
	{C=Text(TAttribute<FText>::CreateLambda([Weak,Id]{return FText::FromString(Weak.IsValid()?Weak->GetTunableText(Id):FString());}),13,400,CXMRPanelUI::ReadoutColor());}
	else if(T.Kind==ECXMRTunableKind::Action)
	{
		C=SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,8,0)[Text(TAttribute<FText>::CreateLambda([this,Id]{return FText::FromString(Done.FindRef(Id));}),12,400,ShowroomColor(60,194,133))]
		+ SHorizontalBox::Slot().AutoWidth()[Action(TAttribute<FText>::CreateLambda([this,Id]{return FText::FromString(Id=="Vehicle.ResetGroupAlignment" ? (Control.IsValid() && Control->IsSharedAlignmentResetArmed()?TEXT("Click again to delete"):TEXT("Delete")):TEXT("Run"));}),[this,Weak,Id]
		{
			if(!Weak.IsValid()){return;}const auto* P=Control.IsValid()?Control->FindPlacement():nullptr;
			const auto* GI=Control.IsValid() && Control->GetWorld()?Control->GetWorld()->GetGameInstance():nullptr;const auto* X=GI?GI->GetSubsystem<UCXMRSubsystem>():nullptr;
			const bool Live=X && X->HasRunningSession();
			if(!Live && (Id=="Vehicle.PlaceInFront" || Id=="Hands.SnapToMarker" || Id=="Diagnostics.DumpMarkers")){Done.Add(Id,TEXT("No headset session"));return;}
			const bool Armed=Control.IsValid() && Control->IsSharedAlignmentResetArmed();
			if(!Weak->InvokeTunable(Id)){return;}
			FString Message;
			if(Id=="Vehicle.RetakeHeading"){Message=P && P->HasNudgeHeading()?TEXT("Updated"):TEXT("");}
			else if(Id=="Vehicle.LearnMarkers"){if(P && P->WasLastCalibrationSaveSuccessful()){Message=FString::Printf(TEXT("Learned %d markers"),P->GetRequiredAlignmentMarkerCount());}}
			else if(Id=="Vehicle.PlaceInFront"){Message=Live?TEXT("Placed 2.5 m ahead"):TEXT("No headset session");}
			else if(Id=="Hands.SnapToMarker"){Message=Live?Weak->GetTunableText(Id):TEXT("No headset session");}
			else if(Id=="Diagnostics.DumpMarkers"){Message=Live?Weak->GetTunableText(Id):TEXT("No headset session");}
			else if(Id=="Diagnostics.MRState"){Message=TEXT("Written to log");}
			else if(Id=="Vehicle.ResetGroupAlignment" && Armed && P && !P->HasSavedAlignment()){Message=TEXT("Deleted");}
			Done.Add(Id,Message);
		},TAttribute<bool>::CreateLambda([this,Weak,Id]{if(Id=="Vehicle.LearnMarkers"){const auto* P=Control.IsValid()?Control->FindPlacement():nullptr;return P && (P->GetRequiredAlignmentMarkerCount()>0 && P->GetMissingAlignmentMarkerIds().IsEmpty()) && P->GetControlStatus().Stage!=ECXMRControlStage::NoHeadset && P->GetControlStatus().Stage!=ECXMRControlStage::Starting;}return Weak.IsValid() && Weak->IsTunableEnabled(Id);}))];
	}
	else if(T.Kind==ECXMRTunableKind::Bool)
	{
		static const FSlateRoundedBoxBrush ThumbOn(ShowroomColor(11,15,23),8.f),ThumbOff(CXMRPanelUI::ReadoutColor(),8.f);
		C=SNew(SBox).WidthOverride(40).HeightOverride(22)[SNew(SButton).ButtonStyle(CXMRPanelUI::FlatButtonStyle()).ContentPadding(0).IsEnabled_Lambda(Enabled)
			.OnClicked_Lambda([Get,Apply]{Apply(Get()>0.5f?0.f:1.f,true);return FReply::Handled();})
			[SNew(SBorder).Padding(3).BorderImage(Frame()).BorderBackgroundColor(CXMRPanelUI::SurfaceColor()).BorderBackgroundColor_Lambda([Get]{return Get()>0.5f?CXMRPanelUI::SelectionColor():ShowroomColor(28,37,52);})
				.HAlign(HAlign_Left).Padding_Lambda([Get]{return Get()>0.5f ? FMargin(21,3,3,3) : FMargin(3,3,21,3);})
				[SNew(SBox).WidthOverride(16).HeightOverride(16)[SNew(SBorder).Padding(0).BorderImage_Lambda([Get]{return Get()>0.5f?&ThumbOn:&ThumbOff;})]]]];
	}
	else if(T.Kind==ECXMRTunableKind::Choice || Id=="MR.ViewOffset")
	{
		TSharedRef<SWrapBox> Options=SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8,8));
		for(int I=0;I<T.Options.Num();++I){Options->AddSlot()[SelectAction(T.Options[I],[Apply,I]{Apply(static_cast<float>(I),true);},TAttribute<bool>::CreateLambda([Get,I]{return FMath::RoundToInt(Get())==I;}),TAttribute<bool>::CreateLambda(Enabled))];}
		// 긴 선택지도 창의 폭 안에서 줄바꿈함.
		V->AddSlot().AutoHeight().Padding(0,0,0,8)[Top];Top=SNew(SHorizontalBox);C=Options;
	}
	else if(FString(Spec.Type)==TEXT("range"))
	{
		C=SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(120)[SNew(SSlider).Style(CXMRPanelUI::SliderStyle()).MinValue(T.Min).MaxValue(T.Max).StepSize(T.Delta).Value_Lambda(Get).OnValueChanged_Lambda([Apply](float Value){Apply(Value,false);}).OnMouseCaptureEnd_Lambda([Get,Apply]{Apply(Get(),true);})]]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8,0)[Text(TAttribute<FText>::CreateLambda([Get,T]{return FText::FromString(FString::Printf(TEXT("%+.1f %s"),Get(),*T.Unit.ToString()));}),12,400,CXMRPanelUI::ReadoutColor())];
	}
	else
	{
		C=SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(76)[SNew(SSpinBox<float>).Style(CXMRPanelUI::SpinStyle()).Font(CXMRPanelUI::CSSFont(15)).MinFractionalDigits(0).MinValue(T.Min).MaxValue(T.Max).MinSliderValue(T.Min).MaxSliderValue(T.Max).Delta(T.Delta).Value_Lambda(Get).IsEnabled_Lambda(Enabled)
			.OnValueChanged_Lambda([Apply](float Value){Apply(Value,false);}).OnValueCommitted_Lambda([Apply](float Value,ETextCommit::Type){Apply(Value,true);}).OnEndSliderMovement_Lambda([Apply](float Value){Apply(Value,true);})]]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8,0)[Text(T.Unit,12,400,CXMRPanelUI::ReadoutColor())];
	}
	if(T.Kind==ECXMRTunableKind::Choice || Id=="MR.ViewOffset"){Top->AddSlot().FillWidth(1)[C];}else{Top->AddSlot().AutoWidth().VAlign(VAlign_Center)[C];}
	V->AddSlot().AutoHeight()[Top];
	if(Setting)
	{
		const TSharedRef<STextBlock> Changed=Text(FText::FromString(TEXT("Changed · default ")+CXMRAdvancedDefault(T)),12,500,ShowroomColor(242,181,68));
		Changed->SetVisibility(TAttribute<EVisibility>::CreateLambda([Get,T]{return FMath::IsNearlyEqual(Get(),T.Default)?EVisibility::Collapsed:EVisibility::Visible;}));
		V->AddSlot().AutoHeight().Padding(0,4)[Changed];
	}
	if(!T.Help.IsEmpty() || !Spec.Requires.IsNone())
	{V->AddSlot().AutoHeight().Padding(0,4,0,0)[Text(TAttribute<FText>::CreateLambda([T,Enabled]{return FText::FromString(T.Help.ToString()+(!Enabled()?TEXT(" Turn on “Real objects in front of the car” first."):TEXT("")));}),12,400,CXMRPanelUI::ReadoutColor())];}
	return SNew(SBorder).ColorAndOpacity_Lambda([Enabled]{return FLinearColor(1,1,1,Enabled()?1.f:0.5f);}).Padding(12).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(CXMRPanelUI::SurfaceColor())[V];
}
