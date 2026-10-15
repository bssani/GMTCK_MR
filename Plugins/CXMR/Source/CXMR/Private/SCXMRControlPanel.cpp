// Copyright GMTCK CX.
#include "SCXMRControlPanel.h"
#include "CXMRShowroomUI.h"
#include "CXMRTuningWindowComponent.h"
#include "CXMRTuningSubsystem.h"
#include "CXMRSubsystem.h"
#include "CXMRPlacementComponent.h"
#include "CXMRVehicleLoaderComponent.h"
#include "CXMRVehicleProfile.h"
#include "CXMRAdvancedSettings.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include "InputCoreTypes.h"
using namespace CXMRShowroomUI;
namespace
{
	UCXMRSubsystem* CXMRControlSubsystem(UCXMRTuningWindowComponent* C)
	{ return C && C->GetWorld() && C->GetWorld()->GetGameInstance() ? C->GetWorld()->GetGameInstance()->GetSubsystem<UCXMRSubsystem>() : nullptr; }
	UCXMRVehicleLoaderComponent* CXMRControlLoader(UCXMRTuningWindowComponent* C)
	{ const UCXMRPlacementComponent* P=C ? C->FindPlacement() : nullptr; return P && P->GetOwner() ? P->GetOwner()->FindComponentByClass<UCXMRVehicleLoaderComponent>() : nullptr; }
	class SCXMRStatusIcon : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SCXMRStatusIcon) {} SLATE_ATTRIBUTE(ECXMRControlStage, Stage) SLATE_END_ARGS()
		void Construct(const FArguments& A) { Stage=A._Stage; }
		FVector2D ComputeDesiredSize(float) const override { return FVector2D(22); }
		int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& L,int32 Layer,const FWidgetStyle&,bool) const override
		{
			const auto S=Stage.Get(); const bool Busy=S==ECXMRControlStage::Starting || S==ECXMRControlStage::Settling || S==ECXMRControlStage::Saving;
			const FLinearColor Color=S==ECXMRControlStage::Aligned ? ShowroomColor(60,194,133) : S==ECXMRControlStage::NoHeadset ? ShowroomColor(255,122,114) : Busy ? ShowroomColor(91,140,255) : ShowroomColor(242,181,68);
			const FSlateRoundedBoxBrush Circle(Color,11.f);
			if (!Busy) { FSlateDrawElement::MakeBox(L,Layer,G.ToPaintGeometry(),&Circle,ESlateDrawEffect::None,Color); }
			TArray<FVector2f> Lines;
			if (S==ECXMRControlStage::Aligned) { Lines={FVector2f(6.5,11.5),FVector2f(9.5,14.5),FVector2f(15.5,8)}; }
			else if (S==ECXMRControlStage::NoHeadset) { Lines={FVector2f(7.5,7.5),FVector2f(14.5,14.5)}; }
			else if (Busy) { for (int I=0;I<=20;++I) { const float A=I*1.5f*PI/20; Lines.Add(FVector2f(11+8*FMath::Cos(A),11+8*FMath::Sin(A))); } }
			else { Lines={FVector2f(11,6),FVector2f(11,11.5),FVector2f(14.5,13.5)}; }
			FSlateDrawElement::MakeLines(L,Layer+1,G.ToPaintGeometry(),Lines,ESlateDrawEffect::None,Busy ? Color : ShowroomColor(11,15,23),true,2.f);
			if (S==ECXMRControlStage::NoHeadset) { Lines={FVector2f(14.5,7.5),FVector2f(7.5,14.5)}; FSlateDrawElement::MakeLines(L,Layer+1,G.ToPaintGeometry(),Lines,ESlateDrawEffect::None,ShowroomColor(11,15,23),true,2.f); }
			return Layer+1;
		}
	private: TAttribute<ECXMRControlStage> Stage;
	};
	class SCXMRRotateIcon : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SCXMRRotateIcon) {} SLATE_ARGUMENT(bool, Clockwise) SLATE_END_ARGS()
		void Construct(const FArguments& A){bClockwise=A._Clockwise;}
		FVector2D ComputeDesiredSize(float) const override{return FVector2D(16);}
		int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& L,int32 Layer,const FWidgetStyle& Style,bool Enabled) const override
		{
			TArray<FVector2f> Points;const float Sign=bClockwise?1.f:-1.f;
			for(int I=0;I<=24;++I){const float A=-PI*0.8f+Sign*I*PI*1.65f/24;Points.Add(FVector2f(8+6*FMath::Cos(A),8+6*FMath::Sin(A)));}
			FSlateDrawElement::MakeLines(L,Layer,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,CXMRPanelUI::InkColor(),true,1.4f);
			const FVector2f Tip=Points.Last(),Tangent=(Tip-Points[23]).GetSafeNormal(),Side(-Tangent.Y,Tangent.X);
			Points={Tip-Tangent*4+Side*3,Tip,Tip-Tangent*4-Side*3};
			FSlateDrawElement::MakeLines(L,Layer,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,CXMRPanelUI::InkColor(),true,1.4f);return Layer;
		}
	private:bool bClockwise=false;
	};

}
FCXMRControlStatus SCXMRControlPanel::State() const
{
	if (Control.IsValid()) { if (const auto* P=Control->FindPlacement()) { return P->GetControlStatus(); } }
	FCXMRControlStatus S; const auto* X=CXMRControlSubsystem(Control.Get());
	if (X && !X->HasRunningSession()) { S.Stage=X->IsHeadsetStarting() ? ECXMRControlStage::Starting : ECXMRControlStage::NoHeadset; }
	return S;
}
bool SCXMRControlPanel::ModeSelected(bool bMR) const
{
	const auto* X=CXMRControlSubsystem(Control.Get());
	if(State().Stage==ECXMRControlStage::Starting && X && X->IsMRRequested()){return false;}
	return X && X->IsMixedRealityOn()==bMR;
}

void SCXMRControlPanel::Construct(const FArguments& A)
{
	Control=A._Control; Tuning=A._Tuning;
	ChildSlot[ CXMRPanelUI::MakeBackground(SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ Header() ]
		+ SVerticalBox::Slot().AutoHeight()[ Status() ]
		+ SVerticalBox::Slot().AutoHeight()[CXMRPanelUI::MakeDivider()]
		+ SVerticalBox::Slot().AutoHeight()[ Tabs({TEXT("Calibration"),TEXT("Vehicle")},[this]{return Control.IsValid() && Control->GetActivePage()==ECXMRControlPage::Vehicle ? 1 : 0;},[this](int32 I){if(Control.IsValid()){Control->SelectPage(I==0 ? ECXMRControlPage::Calibration : ECXMRControlPage::Vehicle); Refresh();}}) ]
		+ SVerticalBox::Slot().AutoHeight()[CXMRPanelUI::MakeDivider()]
		+ SVerticalBox::Slot().FillHeight(1)[ CXMRPanelUI::MakeScroll(SAssignNew(Content,SBox)) ]
		+ SVerticalBox::Slot().AutoHeight()[ SNew(SBorder).Padding(FMargin(16,12)).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(ShowroomColor(11,15,23))
			[ SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(12,8))
			+ SWrapBox::Slot()[ QuietAction(TAttribute<FText>::CreateLambda([this]{const int N=CXMRAdvancedSettings::ChangedCount(Tuning.Get());return FText::FromString(N==0 ? TEXT("Advanced settings at defaults") : FString::Printf(TEXT("%d advanced setting%s changed from defaults"),N,N==1 ? TEXT("") : TEXT("s")));}),[this]{if(Control.IsValid()){Control->OpenAdvancedWindow();}}) ]
			+ SWrapBox::Slot()[ Action(TAttribute<FText>::CreateLambda([this]{return FText::FromString(Control.IsValid() && Control->IsAdvancedWindowOpen() ? TEXT("Advanced settings (open)") : TEXT("Advanced settings…"));}),[this]{if(Control.IsValid()){Control->OpenAdvancedWindow();}}) ] ] ] ) ];
	Refresh();
}
TSharedRef<SWidget> SCXMRControlPanel::Header()
{
	TSharedRef<SHorizontalBox> Modes=SNew(SHorizontalBox);
	for (bool MR : {true,false})
	{
		const TSharedRef<STextBlock> Description=Text(TAttribute<FText>::CreateLambda([this,MR]
		{
			const auto* X=CXMRControlSubsystem(Control.Get());
			return FText::FromString(!MR?TEXT("Virtual car and background only"):State().Stage==ECXMRControlStage::Starting && X && X->IsMRRequested()?TEXT("Switching on when the headset is ready…"):State().Stage==ECXMRControlStage::NoHeadset?TEXT("Needs a running headset"):TEXT("Real world with the virtual car"));
		}),12);
		Description->SetColorAndOpacity(TAttribute<FSlateColor>::CreateLambda([this,MR]{return FSlateColor(ModeSelected(MR)?ShowroomColor(11,15,23):ShowroomColor(185,198,228));}));
		Modes->AddSlot().FillWidth(1).Padding(MR ? FMargin(0,0,8,0) : FMargin(0))
		[ SNew(SButton).ButtonStyle(Button()).ContentPadding(0)
			.IsEnabled_Lambda([this,MR]{return !MR || State().Stage!=ECXMRControlStage::NoHeadset;})
			.OnClicked_Lambda([this,MR]{if(auto* X=CXMRControlSubsystem(Control.Get())){X->SetMixedReality(MR);}return FReply::Handled();})
			[ SNew(SBorder).Padding(FMargin(14,10)).BorderImage(Frame()).BorderBackgroundColor(CXMRPanelUI::SurfaceColor())
				.BorderBackgroundColor_Lambda([this,MR]{return ModeSelected(MR) ? CXMRPanelUI::SelectionColor() : ShowroomColor(11,15,23);})
				[ SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).Text(FText::FromString(MR ? TEXT("MR") : TEXT("VR"))).Font(CXMRPanelUI::CSSFont(18,600)).ColorAndOpacity_Lambda([this,MR]{return ModeSelected(MR)?ShowroomColor(11,15,23):CXMRPanelUI::InkColor();}) ]
				+ SVerticalBox::Slot().AutoHeight()[ Description ] ] ] ];
	}
	return SNew(SBorder).Padding(16).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(CXMRPanelUI::RailColor())
	[ SNew(SVerticalBox)
	+ SVerticalBox::Slot().AutoHeight()[ SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1)[ SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[ Text(FText::FromString(TEXT("Vehicle")),12,400,ShowroomColor(185,198,228)) ]
			+ SVerticalBox::Slot().AutoHeight()[ Text(TAttribute<FText>::CreateLambda([this]{const auto* L=CXMRControlLoader(Control.Get());return L && L->Profile ? L->Profile->DisplayName : FText::GetEmpty();}),18,600) ] ]
		+ SHorizontalBox::Slot().AutoWidth()[ SNew(SBorder).BorderImage(Frame()).BorderBackgroundColor(ShowroomColor(11,15,23)).Padding(FMargin(10,4))
			[SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)[Dot(TAttribute<FLinearColor>::CreateLambda([this]{return State().Stage==ECXMRControlStage::Starting?CXMRPanelUI::SelectionColor():State().Stage==ECXMRControlStage::NoHeadset?ShowroomColor(255,122,114):ShowroomColor(60,194,133);}))]
			+ SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Font(CXMRPanelUI::CSSFont(13,500)).ColorAndOpacity(CXMRPanelUI::InkColor()).Text_Lambda([this]{return FText::FromString(State().Stage==ECXMRControlStage::Starting ? TEXT("Headset starting") : State().Stage==ECXMRControlStage::NoHeadset ? TEXT("No headset") : TEXT("Headset running"));})] ] ] ]
	+ SVerticalBox::Slot().AutoHeight().Padding(0,12,0,0)[ Modes ]
	+ SVerticalBox::Slot().AutoHeight().Padding(0,12,0,0)[ Text(TAttribute<FText>::CreateLambda([this]{const auto* X=CXMRControlSubsystem(Control.Get());const auto S=State();FString N;if(S.Stage==ECXMRControlStage::NoHeadset){N=TEXT("VR only until the headset session starts.");}else if(S.Stage==ECXMRControlStage::Starting && X && X->IsMRRequested()){N=TEXT("MR is selected and will switch on by itself.");}else if(X && X->IsMixedRealityOn()){N=X->bRequestedDepthTest ? FString::Printf(TEXT("Real hands and objects within %.2f m show in front of the car."),X->GetDepthTestRangeFarZ()) : TEXT("Depth test is off: real hands appear behind the virtual car.");}else{N=X && !X->bRequestedVRBackground ? TEXT("Virtual car on an empty background.") : TEXT("Virtual car with the virtual room around it.");}return FText::FromString(N);}),13,400,ShowroomColor(212,221,240)) ] ];
}
TSharedRef<SWidget> SCXMRControlPanel::Status()
{
	return SNew(SBorder).Padding(FMargin(16,12)).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(CXMRPanelUI::SurfaceColor())
	[ SNew(SHorizontalBox)
	+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0,2,12,0)[ SNew(SCXMRStatusIcon).Stage_Lambda([this]{return State().Stage;}) ]
	+ SHorizontalBox::Slot().FillWidth(1)[ SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).Font(CXMRPanelUI::CSSFont(15,600)).AutoWrapText(true)
			.Text_Lambda([this]{return FText::FromString(CXMRDescribeControlStatus(State()).Title);})
			.ColorAndOpacity_Lambda([this]{const auto S=State().Stage;return S==ECXMRControlStage::Aligned?ShowroomColor(60,194,133):S==ECXMRControlStage::NoHeadset?ShowroomColor(255,122,114):S==ECXMRControlStage::Adjusted || S==ECXMRControlStage::Waiting?ShowroomColor(242,181,68):CXMRPanelUI::SelectionColor();}) ]
		+ SVerticalBox::Slot().AutoHeight()[ Caption(TAttribute<FText>::CreateLambda([this]{return FText::FromString(CXMRDescribeControlStatus(State()).Detail);})) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0,6,0,0)[ SNew(SBox).HeightOverride(4)
			.Visibility_Lambda([this]{const auto S=State().Stage;return S==ECXMRControlStage::Saving || S==ECXMRControlStage::Settling ? EVisibility::Visible : EVisibility::Collapsed;})
			[ SNew(SProgressBar).Percent_Lambda([this]{return TOptional<float>(State().Progress);}).FillColorAndOpacity(CXMRPanelUI::SelectionColor()) ] ] ] ];
}
TSharedRef<SWidget> SCXMRControlPanel::Calibration()
{
	const auto S=State(); const bool Live=S.Stage!=ECXMRControlStage::Starting && S.Stage!=ECXMRControlStage::NoHeadset;
	const bool CanAdjust=Live && (S.Stage==ECXMRControlStage::Aligned || S.Stage==ECXMRControlStage::Adjusted);
	const bool CanSave=S.Stage==ECXMRControlStage::Adjusted || (S.Stage==ECXMRControlStage::Aligned && S.Restore==ECXMRControlRestore::New);
	TSharedRef<SVerticalBox> V=SNew(SVerticalBox);
	V->AddSlot().AutoHeight()[ SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1)[ Text(FText::FromString(TEXT("Markers")),15,600) ]
		+ SHorizontalBox::Slot().AutoWidth()[ Caption(TAttribute<FText>::CreateLambda([this]{const auto St=State();return FText::FromString(St.Stage==ECXMRControlStage::Starting || St.Stage==ECXMRControlStage::NoHeadset ? TEXT("Waiting for headset") : FString::Printf(TEXT("%d of %d in view"),St.RequiredMarkers.Num()-St.MissingMarkers.Num(),St.RequiredMarkers.Num()));})) ] ];
	TSharedRef<SVerticalBox> Markers=SNew(SVerticalBox);
	for(int32 Id:S.RequiredMarkers)
	{ Markers->AddSlot().AutoHeight().Padding(12,8)[ SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,12,0)[MarkerRing(TAttribute<bool>::CreateLambda([this,Id] { const auto St=State(); return St.Stage!=ECXMRControlStage::Starting && St.Stage!=ECXMRControlStage::NoHeadset && !St.MissingMarkers.Contains(Id); }))]
		+ SHorizontalBox::Slot().FillWidth(1)[ Text(FText::FromString(FString::Printf(TEXT("Marker %d"),Id)),15,500) ]
		+ SHorizontalBox::Slot().AutoWidth()[ SNew(STextBlock).Font(CXMRPanelUI::CSSFont(13))
			.Text_Lambda([this,Id] { const auto St=State(); return FText::FromString(St.Stage==ECXMRControlStage::Starting || St.Stage==ECXMRControlStage::NoHeadset ? TEXT("—") : St.MissingMarkers.Contains(Id) ? TEXT("Not in view") : TEXT("In view")); })
			.ColorAndOpacity_Lambda([this,Id] { const auto St=State(); return St.Stage==ECXMRControlStage::Starting || St.Stage==ECXMRControlStage::NoHeadset ? CXMRPanelUI::ReadoutColor() : St.MissingMarkers.Contains(Id) ? ShowroomColor(242,181,68) : ShowroomColor(60,194,133); }) ] ]; }
	V->AddSlot().AutoHeight().Padding(0,12,0,0)[ SNew(SBorder).Padding(0).BorderImage(Frame()).BorderBackgroundColor(CXMRPanelUI::SurfaceColor())[Markers] ];
	V->AddSlot().AutoHeight().Padding(0,8,0,24)[ Caption(FText::FromString(TEXT("Every marker in the profile must be in view to calibrate, save or restore. Until then the car stays where it is."))) ];
	V->AddSlot().AutoHeight()[ Text(FText::FromString(TEXT("Alignment")),15,600) ];
	if(!Live || S.Stage==ECXMRControlStage::Waiting || S.Stage==ECXMRControlStage::Settling || S.Stage==ECXMRControlStage::Saving)
	{ V->AddSlot().AutoHeight().Padding(0,12,0,0)[ Caption(FText::FromString(!Live ? TEXT("Calibration starts once the headset session is running.") : S.Stage==ECXMRControlStage::Waiting ? TEXT("Nothing to do here until every marker is in view.") : TEXT("Hold the view steady for a moment."))) ]; }
	else
	{
		TSharedRef<SWrapBox> Actions=SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8,8));
		if(CanSave){ Actions->AddSlot()[Action(FText::FromString(TEXT("Save alignment")),[this]{if(Control.IsValid()){Control->RequestAlignmentSave();}},TAttribute<bool>::CreateLambda([this] { const auto St=State(); return St.MissingMarkers.IsEmpty() && !St.RequiredMarkers.IsEmpty(); }),true)]; }
		if(S.Stage==ECXMRControlStage::Adjusted){ Actions->AddSlot()[Action(FText::FromString(TEXT("Discard changes")),[this]{if(Tuning.IsValid()){Tuning->InvokeTunable("Vehicle.ResetOffset");}})]; }
		if(S.Stage==ECXMRControlStage::Aligned){ Actions->AddSlot()[Action(FText::FromString(TEXT("Recalibrate")),[this]{if(Tuning.IsValid()){Tuning->InvokeTunable("Vehicle.Recalibrate");}})]; }
		V->AddSlot().AutoHeight().Padding(0,12,0,0)[Actions];
		if(CanSave)
		{
			TSharedRef<SWidget> Reason=Caption(TAttribute<FText>::CreateLambda([this] { return FText::FromString(FString::Printf(TEXT("Show marker %s to save."),*CXMRMissingMarkerNames(State()))); }));
			Reason->SetVisibility(TAttribute<EVisibility>::CreateLambda([this] { return State().MissingMarkers.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; }));
			V->AddSlot().AutoHeight().Padding(0,8)[Reason];
		}
		else if(S.Stage==ECXMRControlStage::Aligned && S.Restore!=ECXMRControlRestore::New){V->AddSlot().AutoHeight().Padding(0,8)[Caption(FText::FromString(TEXT("Recalibrate reads the markers again and places the car from them.")))];}
	}

	TSharedRef<SHorizontalBox> Steps=SNew(SHorizontalBox);
	int32 StepIndex=0;
	for(float Step:{0.1f,1.f,5.f})
	{
		if(StepIndex>0) { Steps->AddSlot().AutoWidth()[SNew(SBox).WidthOverride(1)[SNew(SBorder).Padding(0).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(ShowroomColor(102,116,138))]]; }
		Steps->AddSlot().AutoWidth()[StepAction(FText::FromString(FString::Printf(TEXT("%g cm"),Step)),[this,Step]{if(Tuning.IsValid()){Tuning->SetTunableValue("Vehicle.MoveStep",Step);}},TAttribute<bool>::CreateLambda([this,Step]{return Tuning.IsValid() && FMath::IsNearlyEqual(Tuning->GetTunableValue("Vehicle.MoveStep"),Step);}),CanAdjust,StepIndex)];
		++StepIndex;
	}
	static const FSlateRoundedBoxBrush StepFrame(FLinearColor::Transparent,6.f,ShowroomColor(102,116,138),1.f);
	V->AddSlot().AutoHeight().Padding(0,24,0,12)[SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Text(FText::FromString(TEXT("Fine-adjust position")),15,600)]
		+ SHorizontalBox::Slot().AutoWidth()[SNew(SBorder).BorderImage(&StepFrame).Padding(1)[Steps]]];
	const TCHAR* Labels[]={TEXT("Away"),TEXT("Toward"),TEXT("Left"),TEXT("Right"),TEXT("Up"),TEXT("Down"),TEXT("Turn ⟲"),TEXT("Turn ⟳")};
	const FName Ids[]={"Vehicle.Away","Vehicle.Away","Vehicle.Right","Vehicle.Right","Vehicle.Up","Vehicle.Up","Vehicle.Turn","Vehicle.Turn"};
	const float Dirs[]={1,-1,-1,1,1,-1,-1,1};
	for(int Row=0;Row<2;++Row)
	{
		TSharedRef<SHorizontalBox> Pad=SNew(SHorizontalBox);
		for(int Col=0;Col<4;++Col)
		{
			const int I=Row*4+Col;TSharedRef<SWidget> B=Action(FText::FromString(Labels[I]),[this,Id=Ids[I],D=Dirs[I]]{if(Tuning.IsValid()){Tuning->InvokeTunable(Id,D);}},CanAdjust);
			if(I>=6)
			{
				B=SNew(SBox).MinDesiredHeight(40)[SNew(SButton).ButtonStyle(Button()).ContentPadding(FMargin(6,8)).HAlign(HAlign_Center).VAlign(VAlign_Center).IsEnabled(CanAdjust)
					.OnClicked_Lambda([this,D=Dirs[I]]{if(Tuning.IsValid()){Tuning->InvokeTunable("Vehicle.Turn",D);}return FReply::Handled();})
					[SNew(SHorizontalBox)+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Text(FText::FromString(TEXT("Turn")),13,500)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4,0,0,0)[SNew(SCXMRRotateIcon).Clockwise(I==7)]]];
			}
			Pad->AddSlot().FillWidth(1).Padding(0,0,Col==3?0:8,0)[B];
		}
		V->AddSlot().AutoHeight().Padding(0,8,0,0)[Pad];
	}
	V->AddSlot().AutoHeight().Padding(0,12,0,8)[ Caption(TAttribute<FText>::CreateLambda([this]{const auto St=State();if(St.Stage!=ECXMRControlStage::Aligned && St.Stage!=ECXMRControlStage::Adjusted){return FText::FromString(TEXT("Available once the car is aligned."));}const auto* P=Control.IsValid()?Control->FindPlacement():nullptr;const FVector A=P?P->AdjustmentSinceSave:FVector::ZeroVector;const float T=P?P->TurnSinceSave:0;return FText::FromString(FString::Printf(TEXT("Change since last save: %.1f cm %s · %.1f cm %s · %.1f cm %s · %.1f ° %s"),FMath::Abs(A.X),A.X>=0?TEXT("away"):TEXT("toward"),FMath::Abs(A.Y),A.Y>=0?TEXT("right"):TEXT("left"),FMath::Abs(A.Z),A.Z>=0?TEXT("up"):TEXT("down"),FMath::Abs(T),T>=0?TEXT("clockwise"):TEXT("counter-clockwise")));})) ];
	V->AddSlot().AutoHeight()[Caption(FText::FromString(TEXT("Away and toward follow the direction you faced at the first press. Numpad keys do the same.")))];
	V->AddSlot().AutoHeight().Padding(0,24,0,0)[SNew(SExpandableArea).InitiallyCollapsed(true).BorderImage(Frame()).BorderBackgroundColor(CXMRPanelUI::SurfaceColor()).BodyBorderImage(Frame()).BodyBorderBackgroundColor(CXMRPanelUI::SurfaceColor()).Padding(12)
		.HeaderContent()[Text(FText::FromString(TEXT("First-time setup for a new rig")),15,500)]
		.BodyContent()[SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0,12)[Caption(FText::FromString(TEXT("1. Sit in the driver's seat and face straight ahead.")))]
		+ SVerticalBox::Slot().AutoHeight()[Caption(FText::FromString(TEXT("2. Align to my eyes. Puts the car's driver eye point at your eyes.")))]
		+ SVerticalBox::Slot().AutoHeight().Padding(0,6)[Action(FText::FromString(TEXT("Align to my eyes")),[this]{if(Control.IsValid()){Control->StartInitialAlignment();}},TAttribute<bool>::CreateLambda([this]{return Control.IsValid() && Control->CanStartInitialAlignment();}))]
		+ SVerticalBox::Slot().AutoHeight().Padding(0,8)[Caption(FText::FromString(TEXT("3. Fine-adjust until the virtual car sits on the physical rig.")))]
		+ SVerticalBox::Slot().AutoHeight()[Caption(FText::FromString(TEXT("4. Save alignment with every marker in view. Later sessions restore it on their own.")))] ] ];
	return SNew(SBorder).Padding(16).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(CXMRPanelUI::SurfaceColor())[V];
}
TSharedRef<SWidget> SCXMRControlPanel::Vehicles()
{
	VehicleAssets.Reset();TSharedRef<SVerticalBox> V=SNew(SVerticalBox);const auto* L=CXMRControlLoader(Control.Get());
	V->AddSlot().AutoHeight()[Text(FText::FromString(TEXT("Vehicle")),15,600)];
	const auto* P=Control.IsValid()?Control->FindPlacement():nullptr;
	if(P && !P->GetAlignmentGroup().IsNone()){V->AddSlot().AutoHeight().Padding(0,8)[Caption(FText::FromString(TEXT("Alignment group ")+P->GetAlignmentGroup().ToString()))];}
	if(L && L->Catalog)
	{for(int I=0;I<L->Catalog->Vehicles.Num();++I){auto* Profile=L->Catalog->Vehicles[I].LoadSynchronous();if(!Profile){continue;}VehicleAssets.Add(TStrongObjectPtr<UObject>(Profile));V->AddSlot().AutoHeight().Padding(0,8)[VehicleChoice(Profile->DisplayName,TAttribute<bool>::CreateLambda([this,I]{const auto* Loader=CXMRControlLoader(Control.Get());return Loader && Loader->GetVehicleIndex()==I;}),[this,I]{if(auto* Loader=CXMRControlLoader(Control.Get())){Loader->SelectVehicle(I);}})];}}
	if(P && !P->GetAlignmentGroup().IsNone()){V->AddSlot().AutoHeight().Padding(0,12)[Caption(FText::FromString(TEXT("Both vehicles share one saved alignment, so switching keeps the car in place.")))];}
	return SNew(SBorder).Padding(16).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(CXMRPanelUI::SurfaceColor())[V];
}
void SCXMRControlPanel::Refresh()
{
	if(!Content.IsValid()){return;}const auto S=State();const auto* L=CXMRControlLoader(Control.Get());
	const int Page=Control.IsValid()?static_cast<int>(Control->GetActivePage()):0;
	const FString K=FString::Printf(TEXT("%d/%d/%d/%p/%p/%p/%d"),Page,static_cast<int>(S.Stage),static_cast<int>(S.Restore),L,L?L->Profile.Get():nullptr,L?L->Catalog.Get():nullptr,L?L->GetVehicleIndex():0)+FString::JoinBy(S.RequiredMarkers,TEXT(","),[](int32 Id){return FString::FromInt(Id);});
	if(K==LastContentKey){return;}LastContentKey=K;
	Content->SetContent(Control.IsValid() && Control->GetActivePage()==ECXMRControlPage::Vehicle ? Vehicles() : Calibration());
}
void SCXMRControlPanel::Tick(const FGeometry& G,double T,float D){SCompoundWidget::Tick(G,T,D);Refresh();}

FReply SCXMRControlPanel::OnPreviewKeyDown(const FGeometry& G,const FKeyEvent& E)
{
	if(E.GetKey()==EKeys::Enter && Control.IsValid()){Control->RequestAlignmentSave();return FReply::Handled();}
	return SCompoundWidget::OnPreviewKeyDown(G,E);
}
