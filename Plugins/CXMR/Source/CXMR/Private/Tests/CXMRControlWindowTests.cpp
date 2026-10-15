// Copyright GMTCK CX.
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CXMRSubsystem.h"
#include "CXMRMarkerFollower.h"
#include "CXMRMarkerProfile.h"
#include "VarjoMarkersEvent.h"
#include "CXMRControlStatus.h"
#include "CXMRAdvancedSettings.h"
#include "CXMRTuningSubsystem.h"
#include "CXMRTuningWindowComponent.h"
#include "CXMRPlacementComponent.h"
#include "CXMRHandDebugComponent.h"
#include "CXMRSpectatorComponent.h"
#include "CXMRVarjoInputComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRControlStatusTest,"CXMR.Panels.ControlStatusText",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCXMRControlStatusTest::RunTest(const FString&)
{
	FCXMRControlStatus S;S.RequiredMarkers={3,7};S.MissingMarkers={7};S.SettleSeconds=1.5f;S.OriginMovement=0.12f;
	auto Check=[this,&S](ECXMRControlStage Stage,const TCHAR* Title,const TCHAR* Detail)
	{S.Stage=Stage;const auto Text=CXMRDescribeControlStatus(S);TestEqual(TEXT("Status title matches prototype"),Text.Title,FString(Title));TestEqual(TEXT("Status detail matches prototype"),Text.Detail,FString(Detail));};
	Check(ECXMRControlStage::Starting,TEXT("Starting the headset session"),TEXT("MR, the depth test and real-world depth switch on as soon as the headset is ready."));
	Check(ECXMRControlStage::NoHeadset,TEXT("Headset not running"),TEXT("MR and markers need a running headset session. Showing VR until it starts."));
	Check(ECXMRControlStage::Waiting,TEXT("Waiting for markers · 1 of 2 in view"),TEXT("Show marker 7 to the headset. The car stays where it is until every marker is in view."));
	Check(ECXMRControlStage::Settling,TEXT("Reading markers"),TEXT("Hold the view steady. Averaging every marker for 1.5 s."));
	Check(ECXMRControlStage::Saving,TEXT("Saving alignment"),TEXT("Averaging every marker. Hold the view steady."));
	Check(ECXMRControlStage::Adjusted,TEXT("Adjusted · not saved"),TEXT("Show marker 7 to save."));
	S.MissingMarkers.Reset();Check(ECXMRControlStage::Adjusted,TEXT("Adjusted · not saved"),TEXT("Save alignment to reuse this position next session."));
	S.Restore=ECXMRControlRestore::World;Check(ECXMRControlStage::Aligned,TEXT("Aligned · saved position reused"),TEXT("Tracking origin unchanged. Largest marker shift 0.12 cm."));
	S.Restore=ECXMRControlRestore::Markers;S.OriginMovement=1.4f;Check(ECXMRControlStage::Aligned,TEXT("Aligned · recalculated from markers"),TEXT("Tracking origin moved 1.40 cm since the save, so the position came from 2 markers."));
	S.Restore=ECXMRControlRestore::Saved;Check(ECXMRControlStage::Aligned,TEXT("Aligned · saved"),TEXT("Saved just now. The next session reuses this position."));
	S.Restore=ECXMRControlRestore::New;Check(ECXMRControlStage::Aligned,TEXT("Aligned from markers"),TEXT("Nothing saved for this vehicle yet. Fine-adjust if needed, then save."));
	Check(ECXMRControlStage::Idle,TEXT("Not calibrated"),TEXT(""));
	S.MissingMarkers={3,7};TestEqual(TEXT("Multiple missing IDs"),CXMRMissingMarkerNames(S),FString(TEXT("3 and 7")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRControlAdjustmentTest,"CXMR.Panels.AdjustmentReadout",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCXMRControlAdjustmentTest::RunTest(const FString&)
{
	UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	AActor* Owner=World->SpawnActor<AActor>();USceneComponent* Root=NewObject<USceneComponent>(Owner);Owner->SetRootComponent(Root);Root->RegisterComponent();
	UCXMRPlacementComponent* P=NewObject<UCXMRPlacementComponent>(Owner);P->RegisterComponent();P->bCalibrated=true;
	P->NudgeVehicleInFrame(FVector(1,-0.5,0),0.5f,0.f,FVector::ZeroVector);
	TestTrue(TEXT("Button and keyboard path accumulates in operator directions"),P->AdjustmentSinceSave.Equals(FVector(1,-0.5,0)));
	TestEqual(TEXT("Turn accumulation in degrees"),P->TurnSinceSave,0.5f);TestEqual(TEXT("Nudge shows unsaved status"),P->GetControlStatus().Stage,ECXMRControlStage::Adjusted);
	P->ResetMarkerOffset();TestTrue(TEXT("Discard zeroes the readout"),P->AdjustmentSinceSave.IsZero() && P->TurnSinceSave==0.f);
	TestTrue(TEXT("Discard retains the existing manual placement lock"),P->IsManualAlignment());TestEqual(TEXT("Discard returns to aligned status"),P->GetControlStatus().Stage,ECXMRControlStage::Aligned);
	P->NudgeVehicleInFrame(FVector(2,0,0),1.f,0.f,FVector::ZeroVector);TestEqual(TEXT("Next adjustment becomes dirty again"),P->GetControlStatus().Stage,ECXMRControlStage::Adjusted);
	P->Recalibrate();TestTrue(TEXT("Recalibration zeroes the readout"),P->AdjustmentSinceSave.IsZero() && P->TurnSinceSave==0.f);
	Owner->Destroy();World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRControlStartupTest,"CXMR.Panels.ControlStartup",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCXMRControlStartupTest::RunTest(const FString&)
{
	TStrongObjectPtr<UGameInstance> GI(NewObject<UGameInstance>());
	TStrongObjectPtr<UCXMRSubsystem> X(NewObject<UCXMRSubsystem>(GI.Get()));
	TArray<FName> Calls;X->SessionRunningForTest=false;X->RuntimeCallForTest=[&Calls](FName Call){Calls.Add(Call);};
	X->StartHeadsetSessionWait();X->SetMixedReality(true);X->SetDepthTest(true);X->SetEnvironmentDepthEstimation(true);X->SetMarkerTracking(true);X->SetViewOffset(1.f);
	TestTrue(TEXT("MR requested while waiting"),X->IsMRRequested());TestFalse(TEXT("MR has not been applied"),X->IsMixedRealityOn());TestEqual(TEXT("No runtime calls without session"),Calls.Num(),0);
	X->AdvanceSessionForTest(19.f);TestTrue(TEXT("Still starting before timeout"),X->IsHeadsetStarting());X->AdvanceSessionForTest(1.f);TestFalse(TEXT("No headset after 20 seconds"),X->IsHeadsetStarting());TestEqual(TEXT("Timeout makes no runtime call"),Calls.Num(),0);
	X->SessionRunningForTest=true;X->AdvanceSessionForTest(0.f);
	const TArray<FName> Expected={"MR","Depth.Test","Depth.Range","Depth.EnvEstimation","Diagnostics.MarkerTracking"};TestTrue(TEXT("Late session applies ordered MR startup"),Calls==Expected);TestTrue(TEXT("MR running"),X->IsMixedRealityOn());
	X->SetRequestedVRBackground(false);TestFalse(TEXT("Background stays hidden during MR"),X->IsVRBackgroundVisible());X->SetMixedReality(false);TestFalse(TEXT("VR applies background preference"),X->IsVRBackgroundVisible());TestTrue(TEXT("VR keeps depth preference"),X->bRequestedDepthTest);
	X->SetRequestedVRBackground(true);TestTrue(TEXT("VR background preference applies immediately"),X->IsVRBackgroundVisible());X->SetMixedReality(true);TestFalse(TEXT("MR hides actual background"),X->IsVRBackgroundVisible());TestTrue(TEXT("MR keeps background preference"),X->bRequestedVRBackground);X->ToggleVRBackground();TestTrue(TEXT("B in MR temporarily shows background"),X->IsVRBackgroundVisible());TestTrue(TEXT("B in MR keeps preference"),X->bRequestedVRBackground);X->SetMixedReality(false);X->ToggleVRBackground();TestFalse(TEXT("B in VR changes preference"),X->bRequestedVRBackground);
	X->SetMixedReality(true);Calls.Reset();X->SetDepthTest(false);
	TestTrue(TEXT("Disabling depth stops active environment depth"),Calls.Contains("Depth.EnvEstimation"));
	TestFalse(TEXT("Environment depth state is off"),X->IsEnvironmentDepthEstimationOn());
	TestTrue(TEXT("Environment depth preference is retained"),X->bRequestedEnvironmentDepth);
	Calls.Reset();X->SetMixedReality(true);TestFalse(TEXT("No environment-depth call when already off"),Calls.Contains("Depth.EnvEstimation"));
	Calls.Reset();X->SetDepthTest(true);
	TestTrue(TEXT("Re-enabling depth reapplies environment depth preference"),Calls.Contains("Depth.EnvEstimation") && X->IsEnvironmentDepthEstimationOn());
	X->SessionRunningForTest=false;Calls.Reset();X->SetDepthTest(false);
	TestEqual(TEXT("Disabling depth without a session makes no runtime calls"),Calls.Num(),0);
	X->SessionRunningForTest=false;X->bStartInMR=false;X->SetMarkerTracking(false);X->StartHeadsetSessionWait();Calls.Reset();X->SessionRunningForTest=true;X->AdvanceSessionForTest(0.f);TestFalse(TEXT("StartInMR off starts in VR"),X->IsMixedRealityOn());TestFalse(TEXT("Tracking off stays off at startup"),Calls.Contains("Diagnostics.MarkerTracking"));
	X->Deinitialize();return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRDynamicMarkerCallsTest,"CXMR.Panels.DynamicMarkerCalls",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCXMRDynamicMarkerCallsTest::RunTest(const FString&)
{
	TStrongObjectPtr<UGameInstance> GI(NewObject<UGameInstance>(GEngine));
	GI->InitializeStandalone();
	UCXMRSubsystem* X=GI->GetSubsystem<UCXMRSubsystem>();
	UWorld* World=GI->GetWorld();
	AActor* Owner=World->SpawnActor<AActor>();
	UCXMRPlacementComponent* Placement=NewObject<UCXMRPlacementComponent>(Owner);
	Owner->AddInstanceComponent(Placement);
	Placement->MarkerProfile=NewObject<UCXMRMarkerProfile>(Owner);
	FCXMRMarkerEntry Entry; Entry.MarkerId=3; Entry.TrackingMode=ECXMRMarkerTrackingMode::Stationary;
	Placement->MarkerProfile->Markers.Add(Entry);
	ACXMRMarkerFollower* Follower=World->SpawnActor<ACXMRMarkerFollower>();
	Follower->MarkerId=7; Follower->TrackingMode=ECXMRMarkerTrackingMode::Dynamic;
	TArray<FName> Calls;
	X->SessionRunningForTest=false;
	X->RuntimeCallForTest=[&Calls](FName Call) { Calls.Add(Call); };
	TestFalse(TEXT("Mode setter is guarded before the session"),X->SetMarkerTrackingMode(3,ECXMRMarkerTrackingMode::Dynamic));
	TestTrue(TEXT("No mode call before the session"),Calls.IsEmpty());
	X->SessionRunningForTest=true;
	auto Moved=[](int32 Id) { UVarjoMarkerDelegates::VarjoMarkerMoved.Broadcast(Id,FVector::ZeroVector,FRotator::ZeroRotator,FVector2D(10)); };
	auto Detected=[](int32 Id) { UVarjoMarkerDelegates::NewVarjoMarkerDetected.Broadcast(Id,FVector::ZeroVector,FRotator::ZeroRotator,FVector2D(10)); };
	Detected(3); X->SetDynamicMarkers(true);
	TestEqual(TEXT("Enabling applies once to an existing marker"),Calls.Num(),1);
	TestEqual(TEXT("Existing marker uses Dynamic override"),Calls.Last(),FName("Markers.Dynamic"));
	Calls.Reset();
	for(int32 I=0; I<5; ++I) { Moved(3); }
	TestTrue(TEXT("Existing marker movement does not reapply mode"),Calls.IsEmpty());
	Moved(9); Moved(9); Detected(9);
	TestEqual(TEXT("Moved-only new marker is configured once"),Calls.Num(),1);
	Detected(7); Detected(7); Moved(7);
	TestEqual(TEXT("New Detected marker is configured once"),Calls.Num(),2);
	TestTrue(TEXT("New markers both receive Dynamic"),Calls[0]=="Markers.Dynamic" && Calls[1]=="Markers.Dynamic");
	X->SetDynamicMarkers(true);
	TestEqual(TEXT("Repeated enable does not reapply modes"),Calls.Num(),2);
	Calls.Reset(); X->SetDynamicMarkers(false);
	TestEqual(TEXT("Disabling restores each known marker once"),Calls.Num(),3);
	TestEqual(TEXT("Profile and unconfigured marker return to Stationary"),Calls.FilterByPredicate([](FName Call) { return Call=="Markers.Stationary"; }).Num(),2);
	TestEqual(TEXT("Follower returns to its Dynamic mode"),Calls.FilterByPredicate([](FName Call) { return Call=="Markers.Dynamic"; }).Num(),1);
	Calls.Reset(); X->SetDynamicMarkers(false); Moved(3); Moved(7); Moved(9);
	TestTrue(TEXT("Disabled override does not reapply on movement"),Calls.IsEmpty());
	X->SetDynamicMarkers(true); X->SetMarkerTracking(false); X->SetMarkerTracking(true); Calls.Reset(); Moved(3); Moved(3);
	TestEqual(TEXT("Tracking restart configures rediscovered ID once"),Calls.Num(),1);
	X->RuntimeCallForTest=nullptr;
	Follower->Destroy(); Owner->Destroy(); GI->Shutdown(); World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRAdvancedChangedTest,"CXMR.Panels.AdvancedChangedAndReset",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCXMRAdvancedChangedTest::RunTest(const FString&)
{
	TStrongObjectPtr<UGameInstance> GI(NewObject<UGameInstance>());
	TStrongObjectPtr<UCXMRTuningSubsystem> R(NewObject<UCXMRTuningSubsystem>(GI.Get()));
	float Depth=1,Dynamic=0,Hidden=0,Choice=0,Float=0;int Actions=0;
	auto Add=[&R](const TCHAR* Id,float& Value,float Default,ECXMRTunableKind Kind,bool Persist)
	{FCXMRTunable T;T.Id=Id;T.Get=[&Value]{return Value;};T.Set=[&Value](float V){Value=V;};T.Default=Default;T.Kind=Kind;T.Min=0;T.Max=10;T.bPersist=Persist;if(Kind==ECXMRTunableKind::Choice){T.Options={FText::FromString(TEXT("A")),FText::FromString(TEXT("B"))};}R->Register(MoveTemp(T));};
	Add(TEXT("Depth.Test"),Depth,1,ECXMRTunableKind::Bool,true);Add(TEXT("Diagnostics.DynamicMarkers"),Dynamic,0,ECXMRTunableKind::Bool,false);Add(TEXT("Hands.OffsetUp"),Hidden,0,ECXMRTunableKind::Float,true);Add(TEXT("Vehicle.NudgeFrame"),Choice,0,ECXMRTunableKind::Choice,true);Add(TEXT("Display.MRExposure"),Float,0,ECXMRTunableKind::Float,true);
	FCXMRTunable Action;Action.Id="Diagnostics.MRState";Action.Kind=ECXMRTunableKind::Action;Action.Invoke=[&Actions]{++Actions;};R->Register(MoveTemp(Action));
	TestEqual(TEXT("At defaults"),CXMRAdvancedSettings::ChangedCount(R.Get()),0);R->ApplyValue("Depth.Test",0,false);R->ApplyValue("Diagnostics.DynamicMarkers",1,false);R->ApplyValue("Hands.OffsetUp",5,false);R->ApplyValue("Vehicle.NudgeFrame",1,false);R->ApplyValue("Display.MRExposure",1,false);R->InvokeTunable("Diagnostics.MRState");
	TestEqual(TEXT("Saved and session settings counted; hidden result excluded"),CXMRAdvancedSettings::ChangedCount(R.Get()),4);TestEqual(TEXT("Action ran without changing count"),Actions,1);TestEqual(TEXT("MR count"),CXMRAdvancedSettings::ChangedCount(R.Get(),TEXT("MR")),2);
	CXMRAdvancedSettings::ResetTab(R.Get(),TEXT("MR"));TestEqual(TEXT("MR reset restores boolean"),Depth,1.f);TestEqual(TEXT("MR reset restores number"),Float,0.f);TestEqual(TEXT("Other tab still changed"),CXMRAdvancedSettings::ChangedCount(R.Get()),2);TestEqual(TEXT("Hidden hand result survives reset"),Hidden,5.f);
	CXMRAdvancedSettings::ResetTab(R.Get(),TEXT("Diagnostics"));CXMRAdvancedSettings::ResetTab(R.Get(),TEXT("Calibration"));TestEqual(TEXT("Tab reset includes session settings and choices"),CXMRAdvancedSettings::ChangedCount(R.Get()),0);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRAdvancedWordingTest,"CXMR.Panels.AdvancedPrototypeWording",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCXMRAdvancedWordingTest::RunTest(const FString&)
{
	struct FExpected{const TCHAR* Id;const TCHAR* Label;const TCHAR* Help;};
	const FExpected Expected[]={
		{TEXT("MR.StartInMR"),TEXT("Start in MR"),TEXT("Switch to MR as soon as the headset session is running.")},
		{TEXT("Depth.Test"),TEXT("Real objects in front of the car"),TEXT("Hands and nearby real objects cover the virtual car instead of disappearing behind it.")},
		{TEXT("Depth.EnvEstimation"),TEXT("Estimate real-world depth"),TEXT("Cleaner hand edges. Works only with the setting above.")},
		{TEXT("Depth.FarZ"),TEXT("Depth range, far edge"),TEXT("Real objects closer than this cover the car; beyond it the car is always on top. A wider range can flicker where real and virtual surfaces meet, such as a physical steering wheel.")},
		{TEXT("Depth.NearZ"),TEXT("Depth range, near edge"),TEXT("Usually 0. Raise it only if something right in front of the headset should not cover the car.")},
		{TEXT("Display.MRExposure"),TEXT("Virtual exposure"),TEXT("Brightness of the virtual car over the camera image. Camera exposure itself is set in Varjo Base.")},
		{TEXT("MR.ViewOffset"),TEXT("Render from"),TEXT("Cameras matches the camera image best for objects within reach. Eyes matches your real eye position.")},
		{TEXT("MR.ViewOffsetGlide"),TEXT("Render-from change time"),TEXT("How long the switch between Eyes and Cameras takes. 0 switches at once.")},
		{TEXT("MR.Masking"),TEXT("Scene masks"),TEXT("Lets marked real objects show through the virtual scene.")},
		{TEXT("Display.VRExposure"),TEXT("Virtual exposure"),TEXT("Brightness of the virtual scene in VR. Kept separately from MR.")},
		{TEXT("MR.VRBackground"),TEXT("Virtual background"),TEXT("Show the virtual room around the car.")},
		{TEXT("Vehicle.Markers"),TEXT("Calibration markers"),TEXT("")},
		{TEXT("Vehicle.Steadiness"),TEXT("Marker steadiness"),TEXT("How much the marker readings shook while averaging. Large values mean poor light, distance or viewing angle.")},
		{TEXT("Vehicle.LayoutFit"),TEXT("Fit to saved layout"),TEXT("How far the markers in view are from the saved layout. This is the size of the session-to-session error.")},
		{TEXT("Vehicle.Pose"),TEXT("Vehicle position"),TEXT("")},
		{TEXT("Vehicle.KeepLevel"),TEXT("Keep the car level"),TEXT("Follow the markers' heading only and ignore their tilt.")},
		{TEXT("Vehicle.CalibrationSettleSeconds"),TEXT("Marker averaging time"),TEXT("How long marker readings are averaged before the car is placed or saved.")},
		{TEXT("Vehicle.OriginReuseTolerance"),TEXT("Reuse the saved position within"),TEXT("If every marker is at most this far from where it was when saved, the saved position is used as is. Otherwise it is recalculated from the markers.")},
		{TEXT("Vehicle.OriginReuseYawTolerance"),TEXT("Heading tolerance, one-marker profiles"),TEXT("With a single marker, its heading must also match within this angle before the saved position is reused.")},
		{TEXT("Vehicle.YawStep"),TEXT("Turn per press"),TEXT("Rotation for each Turn button or numpad press.")},
		{TEXT("Vehicle.NudgeFrame"),TEXT("Keys move along"),TEXT("Where I first faced keeps Away fixed even when you turn your head.")},
		{TEXT("Vehicle.NudgeHeading"),TEXT("Away from me is"),TEXT("")},
		{TEXT("Vehicle.RetakeHeading"),TEXT("Use the way I face now"),TEXT("")},
		{TEXT("Vehicle.Pivot"),TEXT("Turn around"),TEXT("The point the car rotates around when you press Turn.")},
		{TEXT("Input.OffsetAdjustSpeed"),TEXT("Numpad speed"),TEXT("Movement while a numpad key is held, per second (cm or °).")},
		{TEXT("Vehicle.LearnMarkers"),TEXT("Learn marker layout"),TEXT("Records where each marker sits on the car, from the car's current position. Needs every marker in view.")},
		{TEXT("Vehicle.PlaceInFront"),TEXT("Place car in front of me"),TEXT("For a walk-around without markers. The car is placed 2.5 m ahead, facing you.")},
		{TEXT("Vehicle.ResetGroupAlignment"),TEXT("Delete saved alignment for {0}"),TEXT("Affects every vehicle in the group. The second click must come within 5 seconds.")},
		{TEXT("Diagnostics.HandSkeleton"),TEXT("Hand skeleton"),TEXT("Draw tracked hand joints.")},
		{TEXT("Diagnostics.MarkerLabels"),TEXT("Marker axes and labels"),TEXT("Draw each marker's axes and ID where the headset sees it.")},
		{TEXT("Hands.RightTip"),TEXT("Right index tip"),TEXT("")},
		{TEXT("Hands.LeftTip"),TEXT("Left index tip"),TEXT("")},
		{TEXT("Hands.TipToMarker"),TEXT("Tip minus marker"),TEXT("")},
		{TEXT("Hands.SnapToMarker"),TEXT("Snap hand to marker"),TEXT("Touch a marker's centre with your index tip, then press. Corrects the hand offset.")},
		{TEXT("Spectator.Mode"),TEXT("Monitor shows"),TEXT("")},
		{TEXT("Spectator.FieldOfView"),TEXT("Monitor field of view"),TEXT("")},
		{TEXT("Spectator.FollowSpeed"),TEXT("Smoothing"),TEXT("Lower is calmer.")},
		{TEXT("Spectator.OrbitDistance"),TEXT("Orbit distance"),TEXT("")},
		{TEXT("Spectator.OrbitSpeed"),TEXT("Orbit speed"),TEXT("")},
		{TEXT("Spectator.Output"),TEXT("Monitor picture"),TEXT("")},
		{TEXT("Diagnostics.MarkerTracking"),TEXT("Marker tracking"),TEXT("Leave on. Turning it off stops calibration and restore.")},
		{TEXT("Diagnostics.DynamicMarkers"),TEXT("Dynamic tracking for all markers"),TEXT("For checking moving markers only. Markers follow movement faster but jitter more, so calibration is less accurate while this is on. Marker labels show each marker's mode. Not saved.")},
		{TEXT("Diagnostics.DumpMarkers"),TEXT("Write marker positions to log"),TEXT("")},
		{TEXT("Diagnostics.MRState"),TEXT("Write MR state to log"),TEXT("")},
	};
	TStrongObjectPtr<UGameInstance> GI(NewObject<UGameInstance>(GEngine));GI->InitializeStandalone();UWorld* World=GI->GetWorld();
	AActor* Owner=World->SpawnActor<AActor>();
	UCXMRPlacementComponent* P=NewObject<UCXMRPlacementComponent>(Owner);P->bStartMarkerTrackingOnBeginPlay=false;P->RegisterComponent();
	UCXMRHandDebugComponent* Hands=NewObject<UCXMRHandDebugComponent>(Owner);Hands->RegisterComponent();
	UCXMRSpectatorComponent* Spectator=NewObject<UCXMRSpectatorComponent>(Owner);Spectator->RegisterComponent();
	UCXMRVarjoInputComponent* Input=NewObject<UCXMRVarjoInputComponent>(Owner);Input->RegisterComponent();
	UCXMRTuningWindowComponent* Control=NewObject<UCXMRTuningWindowComponent>(Owner);Control->RegisterComponent();Owner->DispatchBeginPlay();
	const UCXMRTuningSubsystem* R=GI->GetSubsystem<UCXMRTuningSubsystem>();const auto Rows=R->GetTunables();
	for(const FExpected& E:Expected)
	{
		const FCXMRTunable* const* Found=Rows.FindByPredicate([&E](const FCXMRTunable* T){return T->Id==E.Id;});
		if(!TestTrue(E.Id,Found!=nullptr)){continue;}TestEqual(TEXT("Registry label equals prototype"),(*Found)->Label.ToString(),FString(E.Label));TestEqual(TEXT("Registry help equals prototype"),(*Found)->Help.ToString(),FString(E.Help));
	}
	for(FName Id:{FName("MR.VRBackground"),FName("Diagnostics.MarkerTracking"),FName("Depth.Test"),FName("Depth.EnvEstimation")})
	{const auto* const* Found=Rows.FindByPredicate([Id](const FCXMRTunable* T){return T->Id==Id;});if(TestTrue(TEXT("Default metadata exists"),Found!=nullptr)){TestEqual(TEXT("True default matches prototype"),(*Found)->Default,1.f);}}
	UCXMRSubsystem* X=GI->GetSubsystem<UCXMRSubsystem>();
	TestEqual(TEXT("No headset keeps default depth preference"),R->GetTunableValue("Depth.Test"),X->bRequestedDepthTest?1.f:0.f);
	TestEqual(TEXT("No headset keeps tracking preference"),R->GetTunableValue("Diagnostics.MarkerTracking"),1.f);
	X->SessionRunningForTest=false;
	UCXMRTuningSubsystem* Registry=GI->GetSubsystem<UCXMRTuningSubsystem>();
	for(const FCXMRTunable* T:Registry->GetTunables()){if(T->Set){Registry->ApplyValue(T->Id,T->Default,false);}}
	TestEqual(TEXT("No headset at defaults"),CXMRAdvancedSettings::ChangedCount(Registry),0);
	X->SessionRunningForTest=true;X->SetMixedReality(true);TestEqual(TEXT("Normal MR startup at defaults"),CXMRAdvancedSettings::ChangedCount(Registry),0);
	X->SetMixedReality(false);TestEqual(TEXT("VR immediately after MR at defaults"),CXMRAdvancedSettings::ChangedCount(Registry),0);
	Owner->Destroy();GI->Shutdown();World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return true;
}
#endif
