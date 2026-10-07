// Copyright GMTCK CX.

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CXMRVehicleRoot.h"
#include "CXMRVehicleProfile.h"
#include "CXMRVehicleLoaderComponent.h"
#include "CXMRPlacementComponent.h"
#include "CXMRMarkerProfile.h"
#include "CXMRUsbPortTarget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "UObject/UnrealType.h"
#include "CXMRTuningWindowComponent.h"
#include "TimerManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	struct FEyeFixture
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		ACXMRVehicleRoot* Root = nullptr;
		UCXMRVehicleProfile* Profile = nullptr;
		FEyeFixture()
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			Root = World->SpawnActor<ACXMRVehicleRoot>();
			Profile = NewObject<UCXMRVehicleProfile>();
			Profile->VehicleActor = ACXMRUsbPortTarget::StaticClass();
			Profile->VehicleRootOffset = FTransform(FRotator(0, 25, 0), FVector(900, -450, 50));
			UCXMRMarkerProfile* Markers = NewObject<UCXMRMarkerProfile>();
			Markers->CalibrationId = TEXT("DriverEyeTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
			for (int32 Id : {1, 2})
			{
				FCXMRMarkerEntry Entry;
				Entry.MarkerId = Id;
				Entry.LocalOffset.SetLocation(FVector(Id * 100, 0, 0));
				Markers->Markers.Add(Entry);
			}
			Profile->MarkerProfile = Markers;
			Root->Loader->LoadVehicle(Profile);
			Root->Placement->CalibrationSettleSeconds = 0.f;
		}
		~FEyeFixture()
		{
			const FString Path = Root->Placement->GetCalibrationFilePath();
			IFileManager::Get().Delete(*Path);
			IFileManager::Get().Delete(*FPaths::ChangeExtension(Path, TEXT("startup.json")));
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}
	};

	class FAdvanceEyeTimers : public IAutomationLatentCommand
	{
	public:
		FAdvanceEyeTimers(TSharedPtr<FEyeFixture> InFixture, float InDuration, TFunction<void()> InCheck)
			: Fixture(InFixture), Duration(InDuration), Check(MoveTemp(InCheck)) {}
		virtual bool Update() override
		{
			// 타이머 등록과 실행은 서로 다른 프레임에서 처리함.
			if (!bPrimed) { Fixture->World->GetTimerManager().Tick(0.f); bPrimed = true; return false; }
			Fixture->World->GetTimerManager().Tick(Duration);
			Check();
			return true;
		}
	private:
		TSharedPtr<FEyeFixture> Fixture;
		float Duration;
		TFunction<void()> Check;
		bool bPrimed = false;
	};

	bool ConfigureEye(FAutomationTestBase& Test, UCXMRVehicleProfile* Profile)
	{
		FStructProperty* Eye = FindFProperty<FStructProperty>(Profile->GetClass(), TEXT("DriverEyeReference"));
		FBoolProperty* Enabled = FindFProperty<FBoolProperty>(Profile->GetClass(), TEXT("bHasDriverEyeReference"));
		if (!Test.TestNotNull(TEXT("Vehicle profile exposes a driver eye reference"), Eye)
			|| !Test.TestNotNull(TEXT("Eye authoring is explicitly enabled"), Enabled)) { return false; }
		*Eye->ContainerPtrToValuePtr<FTransform>(Profile) = FTransform(FRotator(0, 12, 0), FVector(600, 250, 125));
		Enabled->SetPropertyValue_InContainer(Profile, true);
		return true;
	}

	bool AlignEye(FAutomationTestBase& Test, UCXMRPlacementComponent* Placement)
	{
		UFunction* Function = Placement->FindFunction(TEXT("AlignToDriverEyePose"));
		if (!Test.TestNotNull(TEXT("Placement supports eye-based alignment"), Function)) { return false; }
		struct FParams { FTransform HeadWorld; bool ReturnValue = false; } Params;
		Params.HeadWorld = FTransform(FRotator(-20, 70, 15), FVector(300, -100, 140));
		Placement->ProcessEvent(Function, &Params);
		return Test.TestTrue(TEXT("Configured eye alignment succeeds"), Params.ReturnValue);
	}
	void EyeMarker(UCXMRPlacementComponent* Placement, int32 Id, const FTransform& Pose)
	{
		struct FParams { int32 Id; FVector Location; FRotator Rotation; FVector2D Size; } Params{Id, Pose.GetLocation(), Pose.Rotator(), FVector2D(10, 10)};
		Placement->ProcessEvent(Placement->FindFunctionChecked(TEXT("HandleMarkerMoved")), &Params);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRDriverEyePivotTest, "CXMR.Alignment.DriverEyePivot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRDriverEyePivotTest::RunTest(const FString& Parameters)
{
	FEyeFixture Fixture;
	if (!ConfigureEye(*this, Fixture.Profile) || !AlignEye(*this, Fixture.Root->Placement)) { return false; }
	const FTransform EyeLocal(FRotator(0, 12, 0), FVector(600, 250, 125));
	AActor* Vehicle = Fixture.Root->Loader->GetSpawnedVehicle();
	FTransform Eye = EyeLocal * Vehicle->GetActorTransform();
	TestTrue(TEXT("Eye lands on the tracked head despite the imported offset"), Eye.GetLocation().Equals(FVector(300, -100, 140), 0.01));
	TestTrue(TEXT("Eye heading follows head yaw"), FMath::IsNearlyEqual(Eye.Rotator().Yaw, 70.0, 0.01));
	TestTrue(TEXT("Head pitch does not tilt the vehicle"), FMath::IsNearlyZero(Fixture.Root->GetActorRotation().Pitch, 0.01));
	TestTrue(TEXT("Head roll does not tilt the vehicle"), FMath::IsNearlyZero(Fixture.Root->GetActorRotation().Roll, 0.01));
	const FTransform ImportedOffset = Vehicle->GetActorTransform().GetRelativeTransform(Fixture.Root->Turntable->GetComponentTransform());
	Fixture.Root->Placement->NudgeVehicleInFrame(FVector::ZeroVector, 10, 70, FVector(5000, 5000, 0));
	Eye = EyeLocal * Vehicle->GetActorTransform();
	TestTrue(TEXT("Manual turn keeps the driver eye fixed, independent of current head position"), Eye.GetLocation().Equals(FVector(300, -100, 140), 0.01));
	TestTrue(TEXT("Manual turn changes the heading"), FMath::IsNearlyEqual(Eye.Rotator().Yaw, 80.0, 0.01));
	TestTrue(TEXT("Imported geometry offset is unchanged"), Vehicle->GetActorTransform().GetRelativeTransform(Fixture.Root->Turntable->GetComponentTransform()).Equals(ImportedOffset, 0.01));
	Fixture.Root->Placement->NudgeVehicleInFrame(FVector(10, 0, 0), 0, 150, FVector::ZeroVector);
	const FVector Expected = FVector(300, -100, 140) + FRotator(0, 70, 0).RotateVector(FVector(10, 0, 0));
	Eye = EyeLocal * Vehicle->GetActorTransform();
	TestTrue(TEXT("Translation moves the eye with the car along the initially held heading"), Eye.GetLocation().Equals(Expected, 0.01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRDriverEyeMissingTest, "CXMR.Alignment.MissingReference",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRDriverEyeMissingTest::RunTest(const FString& Parameters)
{
	FEyeFixture Fixture;
	const FTransform Before = Fixture.Root->GetActorTransform();
	TestFalse(TEXT("Unset reference cannot manufacture eye alignment"), Fixture.Root->Placement->AlignToDriverEyePose(FTransform::Identity));
	TestTrue(TEXT("Rejected alignment preserves the anchor"), Fixture.Root->GetActorTransform().Equals(Before));
	ConfigureEye(*this, Fixture.Profile);
	Fixture.Root->Loader->UnloadVehicle();
	TestFalse(TEXT("An unloaded vehicle cannot be aligned"), Fixture.Root->Placement->AlignToDriverEyePose(FTransform::Identity));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRDriverEyeSaveRestoreTest, "CXMR.Alignment.SequentialSaveRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRDriverEyeSaveRestoreTest::RunTest(const FString& Parameters)
{
	FEyeFixture Fixture;
	ConfigureEye(*this, Fixture.Profile);
	UCXMRPlacementComponent* Placement = Fixture.Root->Placement;
	AlignEye(*this, Placement);
	Placement->NudgeVehicleInFrame(FVector(2, -1, 3), 5, 70, FVector::ZeroVector);
	const FTransform Final = Fixture.Root->GetActorTransform();
	const FTransform First = FTransform(FRotator(0, 18, 0), FVector(20, 30, 70)) * Final;
	const FTransform Second = FTransform(FRotator(0, 40, 0), FVector(150, -30, 60)) * Final;
	EyeMarker(Placement, 1, First);
	EyeMarker(Placement, 2, Second);
	TestTrue(TEXT("New marker observations do not overwrite manual alignment"), Fixture.Root->GetActorTransform().Equals(Final, 0.001));
	TestTrue(TEXT("Final pose can be confirmed"), Placement->BeginAlignmentCapture());
	TestFalse(TEXT("Pre-confirm observations are not captured"), Placement->CanSaveAlignment());
	EyeMarker(Placement, 1, First);
	TestEqual(TEXT("First marker captured"), Placement->GetCapturedAlignmentMarkerCount(), 1);
	TestFalse(TEXT("Missing configured marker blocks saving"), Placement->SaveAlignment());
	Fixture.World->RealTimeSeconds += 2.0;
	int32 LostId = 1;
	Placement->ProcessEvent(Placement->FindFunctionChecked(TEXT("HandleMarkerLost")), &LostId);
	EyeMarker(Placement, 2, Second);
	TestTrue(TEXT("Markers can be captured separately at the fixed confirmed pose"), Placement->CanSaveAlignment());
	TestTrue(TEXT("Confirmed layout writes successfully"), Placement->SaveAlignment());
	TestTrue(TEXT("Saving does not move the vehicle"), Fixture.Root->GetActorTransform().Equals(Final, 0.001));
	TestTrue(TEXT("Saved pose is held"), Placement->bCalibrated);
	// 새 실행처럼 파일을 다시 읽고 마커 하나로 복원함.
	Fixture.Root->Loader->LoadVehicle(Fixture.Profile);
	Fixture.Root->SetActorTransform(FTransform::Identity);
	EyeMarker(Placement, 2, Second);
	TestTrue(TEXT("A saved secondary marker can restore without both markers visible"), Fixture.Root->GetActorTransform().Equals(Final, 0.01));
	TestTrue(TEXT("Single marker restoration requires confirmation"), Placement->NeedsRestoreConfirmation());
	TestFalse(TEXT("Single marker restoration is not silently accepted"), Placement->bCalibrated);
	const FTransform Candidate = Fixture.Root->GetActorTransform();
	EyeMarker(Placement, 1, FTransform(FVector(999, 888, 777)));
	TestTrue(TEXT("An additional marker cannot overwrite the held restoration"), Fixture.Root->GetActorTransform().Equals(Candidate, 0.001));
	TestTrue(TEXT("Restored alignment is explicitly accepted"), Placement->ConfirmRestoredAlignment());
	Placement->ProcessEvent(Placement->FindFunctionChecked(TEXT("HandleMarkerLost")), &LostId);
	EyeMarker(Placement, 1, First);
	TestTrue(TEXT("Loss and reacquisition keep accepted placement"), Fixture.Root->GetActorTransform().Equals(Candidate, 0.001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRDriverEyeCaptureInvalidationTest, "CXMR.Alignment.CaptureInvalidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRDriverEyeCaptureInvalidationTest::RunTest(const FString& Parameters)
{
	FEyeFixture Fixture;
	ConfigureEye(*this, Fixture.Profile);
	UCXMRPlacementComponent* Placement = Fixture.Root->Placement;
	AlignEye(*this, Placement);
	Placement->BeginAlignmentCapture();
	EyeMarker(Placement, 1, FTransform(FVector(100, 0, 0)));
	Placement->NudgeVehicleInFrame(FVector(1, 0, 0), 0, 70, FVector::ZeroVector);
	TestEqual(TEXT("Adjustment clears captures from the old pose"), Placement->GetCapturedAlignmentMarkerCount(), 0);
	TestFalse(TEXT("Old confirmation cannot save the changed pose"), Placement->SaveAlignment());
	Placement->BeginAlignmentCapture();
	Fixture.Profile->DriverEyeReference.AddToTranslation(FVector(1, 0, 0));
	TestFalse(TEXT("Editing the eye reference invalidates capture"), Placement->HasAlignmentCapture());
	Placement->BeginAlignmentCapture();
	Fixture.Root->Loader->LoadVehicle(Fixture.Profile);
	TestFalse(TEXT("Replacing a vehicle using the same profile invalidates capture"), Placement->HasAlignmentCapture());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRDriverEyeFailedSaveTest, "CXMR.Alignment.FailedSave",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRDriverEyeFailedSaveTest::RunTest(const FString& Parameters)
{
	FEyeFixture Fixture;
	ConfigureEye(*this, Fixture.Profile);
	UCXMRPlacementComponent* Placement = Fixture.Root->Placement;
	AlignEye(*this, Placement);
	UCXMRTuningWindowComponent* Control = NewObject<UCXMRTuningWindowComponent>(Fixture.Root);
	Control->RegisterComponent();
	TestTrue(TEXT("Panel confirmation starts capture"), Control->ConfirmCalibration());
	EyeMarker(Placement, 1, FTransform(FVector(100, 20, 10)));
	EyeMarker(Placement, 2, FTransform(FVector(200, 20, 10)));
	const FTransform OldOffset = Placement->MarkerProfile->Markers[0].LocalOffset;
	const FString Path = Placement->GetCalibrationFilePath();
	IFileManager::Get().MakeDirectory(*Path, true);
	TestFalse(TEXT("Write failure is reported"), Control->SaveCalibration());
	TestTrue(TEXT("Panel shows the save error instead of capture progress"), Control->GetCalibrationMessage().ToString().Contains(TEXT("Save failed")));
	TestTrue(TEXT("Write failure preserves profile offsets"), Placement->MarkerProfile->Markers[0].LocalOffset.Equals(OldOffset));
	TestTrue(TEXT("Write failure retains capture for retry"), Placement->CanSaveAlignment());
	IFileManager::Get().DeleteDirectory(*Path, false, false);
	TestTrue(TEXT("The same confirmed capture can be retried"), Control->SaveCalibration());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRDriverEyeCountdownTest, "CXMR.Alignment.CountdownTrackingGuard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRDriverEyeCountdownTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FEyeFixture> Fixture = MakeShared<FEyeFixture>();
	ConfigureEye(*this, Fixture->Profile);
	UCXMRTuningWindowComponent* Control = NewObject<UCXMRTuningWindowComponent>(Fixture->Root);
	Control->RegisterComponent();
	const FTransform Before = Fixture->Root->GetActorTransform();
	Control->StartInitialAlignment();
	TestTrue(TEXT("Initial alignment waits for a posture countdown"), Control->IsInitialAlignmentPending());
	Control->StartInitialAlignment();
	TestFalse(TEXT("Pressing again cancels the countdown"), Control->IsInitialAlignmentPending());
	Control->StartInitialAlignment();
	AddCommand(new FAdvanceEyeTimers(Fixture, 4.f, [this, Fixture, Control, Before]()
	{
		TestFalse(TEXT("Countdown actually expires"), Control->IsInitialAlignmentPending());
		TestTrue(TEXT("Missing tracked HMD cannot move the vehicle"), Fixture->Root->GetActorTransform().Equals(Before));
		TestFalse(TEXT("Missing HMD cannot create manual alignment"), Fixture->Root->Placement->IsManualAlignment());
		TestTrue(TEXT("Failed HMD tracking is reported"), Control->GetCalibrationMessage().ToString().Contains(TEXT("not tracked")));
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRDriverEyeMultiRestoreTest, "CXMR.Alignment.MultiMarkerRestoreQuality",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRDriverEyeMultiRestoreTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FEyeFixture> Fixture = MakeShared<FEyeFixture>();
	ConfigureEye(*this, Fixture->Profile);
	UCXMRPlacementComponent* Placement = Fixture->Root->Placement;
	AlignEye(*this, Placement);
	const FTransform Final = Fixture->Root->GetActorTransform();
	const FTransform First = FTransform(FVector(0, 0, 40)) * Final;
	const FTransform Second = FTransform(FVector(120, 30, 40)) * Final;
	Placement->BeginAlignmentCapture();
	EyeMarker(Placement, 1, First);
	EyeMarker(Placement, 2, Second);
	TestTrue(TEXT("Valid two-marker setup is saved"), Placement->SaveAlignment());
	Fixture->Root->Loader->LoadVehicle(Fixture->Profile);
	Fixture->Root->SetActorTransform(FTransform::Identity);
	Placement->CalibrationSettleSeconds = 0.2f;
	Placement->bAverageMarkerSamples = false;
	EyeMarker(Placement, 1, First);
	EyeMarker(Placement, 2, FTransform(FVector(200, 0, 0)) * Second);
	AddCommand(new FAdvanceEyeTimers(Fixture, 0.3f, [this, Fixture, Placement, Second]()
	{
		TestFalse(TEXT("Inconsistent saved marker poses cannot complete restoration"), Placement->bCalibrated);
		TestFalse(TEXT("Inconsistent pair cannot become an accepted single-marker fallback"), Placement->NeedsRestoreConfirmation());
		EyeMarker(Placement, 2, Second);
	}));
	AddCommand(new FAdvanceEyeTimers(Fixture, 0.3f, [this, Fixture, Placement, Final]()
	{
		TestTrue(TEXT("Consistent pair restores after settling"), Placement->bCalibrated);
		TestFalse(TEXT("Verified pair does not require single-marker confirmation"), Placement->NeedsRestoreConfirmation());
		TestTrue(TEXT("Verified pair reproduces the saved alignment"), Fixture->Root->GetActorTransform().Equals(Final, 0.01));
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRDriverEyePanelCaptureTest, "CXMR.Alignment.PanelCaptureWorkflow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRDriverEyePanelCaptureTest::RunTest(const FString& Parameters)
{
	FEyeFixture Fixture;
	ConfigureEye(*this, Fixture.Profile);
	UCXMRPlacementComponent* Placement = Fixture.Root->Placement;
	UCXMRTuningWindowComponent* Control = NewObject<UCXMRTuningWindowComponent>(Fixture.Root);
	Control->RegisterComponent();
	AlignEye(*this, Placement);
	TestTrue(TEXT("Initial alignment unlocks confirmation without fake marker calibration"), Control->CanConfirmCalibration());
	TestTrue(TEXT("Confirm starts final-pose capture"), Control->ConfirmCalibration());
	TestFalse(TEXT("Save stays disabled until new marker observations arrive"), Control->CanSaveCalibration());
	EyeMarker(Placement, 1, FTransform(FVector(100, 20, 30)));
	EyeMarker(Placement, 2, FTransform(FVector(220, 20, 30)));
	TestTrue(TEXT("Fresh full capture unlocks the panel save"), Control->CanSaveCalibration());
	Placement->NudgeVehicleInFrame(FVector(1, 0, 0), 0, 70, FVector::ZeroVector);
	TestFalse(TEXT("An adjustment invalidates the panel confirmation"), Control->CanSaveCalibration());
	TestTrue(TEXT("The new pose can be confirmed again"), Control->ConfirmCalibration());
	EyeMarker(Placement, 1, FTransform(FVector(100, 20, 30)));
	EyeMarker(Placement, 2, FTransform(FVector(220, 20, 30)));
	TestTrue(TEXT("Panel completes a real saved alignment"), Control->SaveCalibration());
	TestEqual(TEXT("Successful save completes the panel workflow"), Control->GetCalibrationPhase(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRDriverEyeReadOnlySaveTest, "CXMR.Alignment.PreservePreviousSave",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRDriverEyeReadOnlySaveTest::RunTest(const FString& Parameters)
{
	FEyeFixture Fixture;
	ConfigureEye(*this, Fixture.Profile);
	UCXMRPlacementComponent* Placement = Fixture.Root->Placement;
	AlignEye(*this, Placement);
	Placement->BeginAlignmentCapture();
	EyeMarker(Placement, 1, FTransform(FVector(100, 20, 30)));
	EyeMarker(Placement, 2, FTransform(FVector(220, 20, 30)));
	TestTrue(TEXT("Initial save succeeds"), Placement->SaveAlignment());
	const FString Path = Placement->GetCalibrationFilePath();
	FString Before, After;
	FFileHelper::LoadFileToString(Before, *Path);
	const FTransform OldOffset = Placement->MarkerProfile->Markers[0].LocalOffset;
	Placement->NudgeVehicleInFrame(FVector(4, 0, 0), 0, 70, FVector::ZeroVector);
	Placement->BeginAlignmentCapture();
	EyeMarker(Placement, 1, FTransform(FVector(100, 20, 30)));
	EyeMarker(Placement, 2, FTransform(FVector(220, 20, 30)));
	FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*Path, true);
	TestFalse(TEXT("Read-only destination rejects replacement"), Placement->SaveAlignment());
	FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*Path, false);
	FFileHelper::LoadFileToString(After, *Path);
	TestEqual(TEXT("Failed replacement preserves the prior file exactly"), After, Before);
	TestTrue(TEXT("Failed replacement preserves the profile"), Placement->MarkerProfile->Markers[0].LocalOffset.Equals(OldOffset));
	TestTrue(TEXT("Read-only failure can be retried after permission is restored"), Placement->SaveAlignment());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRVehicleCalibrationIdentityTest, "CXMR.Alignment.SharedMarkerProfile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRVehicleCalibrationIdentityTest::RunTest(const FString& Parameters)
{
	FEyeFixture Fixture;
	ConfigureEye(*this, Fixture.Profile);
	UCXMRPlacementComponent* Placement = Fixture.Root->Placement;
	AlignEye(*this, Placement);
	const FTransform FirstPose = Fixture.Root->GetActorTransform();
	const FTransform Marker = FTransform(FVector(100, 20, 30));
	Placement->BeginAlignmentCapture();
	EyeMarker(Placement, 1, Marker);
	EyeMarker(Placement, 2, FTransform(FVector(220, 20, 30)));
	TestTrue(TEXT("Vehicle A calibration saved"), Placement->SaveAlignment());
	const FString FirstPath = Placement->GetCalibrationFilePath();
	UCXMRVehicleProfile* Other = NewObject<UCXMRVehicleProfile>();
	Other->VehicleActor = Fixture.Profile->VehicleActor;
	Other->VehicleRootOffset = Fixture.Profile->VehicleRootOffset;
	Other->MarkerProfile = Fixture.Profile->MarkerProfile;
	ConfigureEye(*this, Other);
	Fixture.Root->Loader->LoadVehicle(Other);
	AlignEye(*this, Placement);
	Placement->NudgeVehicleInFrame(FVector(40, 0, 0), 10, 70, FVector::ZeroVector);
	Placement->BeginAlignmentCapture();
	EyeMarker(Placement, 1, Marker);
	EyeMarker(Placement, 2, FTransform(FVector(220, 20, 30)));
	TestTrue(TEXT("Vehicle B calibration saved"), Placement->SaveAlignment());
	const FString SecondPath = Placement->GetCalibrationFilePath();
	TestNotEqual(TEXT("Shared marker profiles have distinct vehicle files"), FirstPath, SecondPath);
	Fixture.Root->Loader->LoadVehicle(Fixture.Profile);
	Fixture.Root->SetActorTransform(FTransform::Identity);
	EyeMarker(Placement, 1, Marker);
	TestTrue(TEXT("Returning to A restores its own alignment"), Fixture.Root->GetActorTransform().Equals(FirstPose, 0.01));
	TestTrue(TEXT("Returning to A retains saved-layout confirmation"), Placement->NeedsRestoreConfirmation());
	IFileManager::Get().Delete(*SecondPath);
	IFileManager::Get().Delete(*FPaths::ChangeExtension(SecondPath, TEXT("startup.json")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRFinalFitHeadingTest, "CXMR.Alignment.FinalFitHeading",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRFinalFitHeadingTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FEyeFixture> Fixture = MakeShared<FEyeFixture>();
	ConfigureEye(*this, Fixture->Profile);
	UCXMRPlacementComponent* Placement = Fixture->Root->Placement;
	AlignEye(*this, Placement);
	const FTransform Final = Fixture->Root->GetActorTransform();
	Placement->BeginAlignmentCapture();
	EyeMarker(Placement, 1, FTransform(FVector(-5, 0, 0)) * Final);
	EyeMarker(Placement, 2, FTransform(FVector(5, 0, 0)) * Final);
	TestTrue(TEXT("Close but solvable pair saved"), Placement->SaveAlignment());
	Fixture->Root->Loader->LoadVehicle(Fixture->Profile);
	Placement->CalibrationSettleSeconds = 0.2f;
	Placement->bAverageMarkerSamples = false;
	const FQuat MovedYaw = FRotator(0, 10, 0).Quaternion();
	EyeMarker(Placement, 1, FTransform(MovedYaw.RotateVector(FVector(-5, 0, 0))) * Final);
	EyeMarker(Placement, 2, FTransform(MovedYaw.RotateVector(FVector(5, 0, 0))) * Final);
	AddCommand(new FAdvanceEyeTimers(Fixture, 0.3f, [this, Fixture, Placement, Final]()
	{
		TestFalse(TEXT("A positional fit cannot bypass marker heading disagreement"), Placement->bCalibrated);
		TestFalse(TEXT("Bad heading cannot become a single-marker confirmation"), Placement->NeedsRestoreConfirmation());
		EyeMarker(Placement, 1, FTransform(FVector(-5, 0, 0)) * Final);
		EyeMarker(Placement, 2, FTransform(FVector(5, 0, 0)) * Final);
	}));
	AddCommand(new FAdvanceEyeTimers(Fixture, 0.3f, [this, Fixture, Placement]()
	{
		TestTrue(TEXT("The unchanged nearby pair can complete restoration"), Placement->bCalibrated);
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRCaptureConfigurationTest, "CXMR.Alignment.CaptureConfiguration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRCaptureConfigurationTest::RunTest(const FString& Parameters)
{
	FEyeFixture Fixture;
	ConfigureEye(*this, Fixture.Profile);
	UCXMRPlacementComponent* Placement = Fixture.Root->Placement;
	AlignEye(*this, Placement);
	const FTransform Offset = Fixture.Profile->VehicleRootOffset;
	Placement->BeginAlignmentCapture();
	Fixture.Profile->VehicleRootOffset.AddToTranslation(FVector(1, 0, 0));
	TestFalse(TEXT("Changing the configured model offset invalidates confirmed capture"), Placement->HasAlignmentCapture());
	TestFalse(TEXT("A newly confirmed pose cannot use an unapplied model offset"), Placement->BeginAlignmentCapture());
	Fixture.Profile->VehicleRootOffset = Offset;
	TestTrue(TEXT("Applied model configuration can be confirmed"), Placement->BeginAlignmentCapture());
	Fixture.Profile->VehicleActor = ACXMRVehicleRoot::StaticClass();
	TestFalse(TEXT("Changing the configured model class invalidates capture"), Placement->HasAlignmentCapture());
	TestFalse(TEXT("A newly confirmed pose cannot use an unapplied model class"), Placement->BeginAlignmentCapture());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRLegacyAlignmentResetTest, "CXMR.Alignment.LegacyFileAndReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRLegacyAlignmentResetTest::RunTest(const FString& Parameters)
{
	FEyeFixture Fixture;
	ConfigureEye(*this, Fixture.Profile);
	UCXMRPlacementComponent* Placement = Fixture.Root->Placement;
	const FTransform Authored = Placement->MarkerProfile->Markers[0].LocalOffset;
	AlignEye(*this, Placement);
	Placement->BeginAlignmentCapture();
	EyeMarker(Placement, 1, FTransform(FVector(100, 20, 30)));
	EyeMarker(Placement, 2, FTransform(FVector(220, 20, 30)));
	TestTrue(TEXT("Calibration saved for compatibility fixture"), Placement->SaveAlignment());
	const FString Current = Placement->GetCalibrationFilePath();
	const FString Legacy = FPaths::ProjectSavedDir() / TEXT("CXMR") / FString::Printf(TEXT("MarkerCalib_%s.json"), *Placement->MarkerProfile->GetCalibrationId());
	if (Current == Legacy) { AddError(TEXT("Vehicle-specific storage is missing")); return false; }
	IFileManager::Get().Copy(*Legacy, *Current);
	IFileManager::Get().Delete(*Current);
	Fixture.Root->Loader->LoadVehicle(Fixture.Profile);
	EyeMarker(Placement, 1, FTransform(FVector(100, 20, 30)));
	TestTrue(TEXT("Existing marker-only file can restore the matching vehicle"), Placement->NeedsRestoreConfirmation());
	Placement->ResetCalibrationToAuthored();
	Fixture.Root->Loader->LoadVehicle(Fixture.Profile);
	TestTrue(TEXT("Reset remains authored after reload despite the legacy file"), Placement->MarkerProfile->Markers[0].LocalOffset.Equals(Authored));
	IFileManager::Get().Delete(*Legacy);
	IFileManager::Get().Delete(*FPaths::ChangeExtension(Legacy, TEXT("startup.json")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRCountdownReferenceChangeTest, "CXMR.Alignment.CountdownReferenceChange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRCountdownReferenceChangeTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FEyeFixture> Fixture = MakeShared<FEyeFixture>();
	ConfigureEye(*this, Fixture->Profile);
	UCXMRTuningWindowComponent* Control = NewObject<UCXMRTuningWindowComponent>(Fixture->Root);
	Control->RegisterComponent();
	const FTransform Before = Fixture->Root->GetActorTransform();
	Control->StartInitialAlignment();
	Fixture->Profile->DriverEyeReference.AddToTranslation(FVector(1, 0, 0));
	AddCommand(new FAdvanceEyeTimers(Fixture, 4.f, [this, Fixture, Control, Before]()
	{
		TestFalse(TEXT("Changed-reference countdown expires"), Control->IsInitialAlignmentPending());
		TestTrue(TEXT("Reference changes keep vehicle position"), Fixture->Root->GetActorTransform().Equals(Before));
		TestTrue(TEXT("Reference changes are rejected before querying HMD tracking"), Control->GetCalibrationMessage().ToString().Contains(TEXT("reference changed")));
		Control->StartInitialAlignment();
		Fixture->Root->Loader->LoadVehicle(Fixture->Profile);
	}));
	AddCommand(new FAdvanceEyeTimers(Fixture, 4.f, [this, Fixture, Control]()
	{
		TestFalse(TEXT("Replacement cannot create an alignment from the old session"), Fixture->Root->Placement->IsManualAlignment());
		TestTrue(TEXT("Vehicle replacement during countdown is reported"), Control->GetCalibrationMessage().ToString().Contains(TEXT("changed")));
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRUnicodeVehicleIdentityTest, "CXMR.Alignment.UnicodeVehicleIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRUnicodeVehicleIdentityTest::RunTest(const FString& Parameters)
{
	FEyeFixture Fixture;
	UCXMRVehicleProfile* First = NewObject<UCXMRVehicleProfile>(GetTransientPackage(), TEXT("Vehicle_\uac00"));
	UCXMRVehicleProfile* Second = NewObject<UCXMRVehicleProfile>(GetTransientPackage(), TEXT("Vehicle_\uac01"));
	for (UCXMRVehicleProfile* Profile : {First, Second})
	{
		Profile->VehicleActor = Fixture.Profile->VehicleActor;
		Profile->MarkerProfile = Fixture.Profile->MarkerProfile;
	}
	Fixture.Root->Loader->LoadVehicle(First);
	const FString FirstPath = Fixture.Root->Placement->GetCalibrationFilePath();
	Fixture.Root->Loader->LoadVehicle(Second);
	TestNotEqual(TEXT("Unicode vehicle names retain distinct calibration files"), FirstPath, Fixture.Root->Placement->GetCalibrationFilePath());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRKeyboardCaptureTest, "CXMR.Alignment.KeyboardConfirmCaptureSave",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRKeyboardCaptureTest::RunTest(const FString& Parameters)
{
	FEyeFixture Fixture;
	UCXMRPlacementComponent* Placement = Fixture.Root->Placement;
	UCXMRTuningWindowComponent* Control = NewObject<UCXMRTuningWindowComponent>(Fixture.Root);
	Control->RegisterComponent();
	EyeMarker(Placement, 1, FTransform(FVector(100, 20, 30)));
	EyeMarker(Placement, 2, FTransform(FVector(200, 20, 30)));
	TestTrue(TEXT("Original marker calibration completed"), Placement->bCalibrated);
	Placement->NudgeVehicleInFrame(FVector(2, 0, 0), 3, 0, FVector::ZeroVector);
	const FTransform Final = Fixture.Root->GetActorTransform();
	Placement->ProcessEvent(Placement->FindFunctionChecked(TEXT("HandleSaveOffsetRequest")), nullptr);
	TestTrue(TEXT("Enter confirms and starts fresh capture"), Placement->HasAlignmentCapture());
	TestEqual(TEXT("Keyboard confirmation enters the panel capture phase"), Control->GetCalibrationPhase(), 2);
	TestEqual(TEXT("Pre-Enter observations cannot count as captured"), Placement->GetCapturedAlignmentMarkerCount(), 0);
	TestFalse(TEXT("First Enter does not report a save before fresh capture"), Placement->WasLastCalibrationSaveSuccessful());
	EyeMarker(Placement, 1, FTransform(FVector(100, 20, 30)));
	Placement->SaveMarkerOffsetToProfile();
	TestTrue(TEXT("Incomplete save reports progress in the panel"), Control->GetCalibrationMessage().ToString().Contains(TEXT("1 / 2")));
	TestFalse(TEXT("Incomplete save does not create a file"), IFileManager::Get().FileExists(*Placement->GetCalibrationFilePath()));
	EyeMarker(Placement, 2, FTransform(FVector(220, 20, 30)));
	Placement->ProcessEvent(Placement->FindFunctionChecked(TEXT("HandleSaveOffsetRequest")), nullptr);
	TestTrue(TEXT("Enter saves the complete fresh capture"), Placement->WasLastCalibrationSaveSuccessful());
	TestEqual(TEXT("Keyboard save completes the panel phase"), Control->GetCalibrationPhase(), 3);
	Fixture.Root->Loader->LoadVehicle(Fixture.Profile);
	Fixture.Root->SetActorTransform(FTransform::Identity);
	EyeMarker(Placement, 2, FTransform(FVector(220, 20, 30)));
	TestTrue(TEXT("Keyboard adjustment survives reload"), Fixture.Root->GetActorTransform().Equals(Final, 0.01));
	TestTrue(TEXT("Keyboard save retains single-marker restoration"), Placement->NeedsRestoreConfirmation());
	Fixture.Root->Loader->UnloadVehicle();
	Placement->NudgeVehicleInFrame(FVector(1, 0, 0), 0, 0, FVector::ZeroVector);
	Placement->SaveMarkerOffsetToProfile();
	TestTrue(TEXT("Missing vehicle is explained"), Control->GetCalibrationMessage().ToString().Contains(TEXT("Load a vehicle")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRResaveMetadataTest, "CXMR.Alignment.ResaveMetadata",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRResaveMetadataTest::RunTest(const FString& Parameters)
{
	FEyeFixture Fixture;
	ConfigureEye(*this, Fixture.Profile);
	UCXMRPlacementComponent* Placement = Fixture.Root->Placement;
	AlignEye(*this, Placement);
	Placement->BeginAlignmentCapture();
	EyeMarker(Placement, 1, FTransform(FVector(100, 20, 30)));
	EyeMarker(Placement, 2, FTransform(FVector(220, 20, 30)));
	TestTrue(TEXT("Guided alignment saved"), Placement->SaveAlignment());
	// 파일에 지정한 기준 ID도 재저장 때 바뀌면 안 됨.
	FString Saved;
	FFileHelper::LoadFileToString(Saved, *Placement->GetCalibrationFilePath());
	TSharedPtr<FJsonObject> Data;
	if (!TestTrue(TEXT("Saved metadata parses"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Saved), Data))) { return false; }
	Data->SetNumberField(TEXT("primaryMarker"), 2);
	FJsonSerializer::Serialize(Data.ToSharedRef(), TJsonWriterFactory<>::Create(&Saved));
	FFileHelper::SaveStringToFile(Saved, *Placement->GetCalibrationFilePath());
	Fixture.Root->Loader->LoadVehicle(Fixture.Profile);
	TestTrue(TEXT("Normal re-save succeeds"), Placement->SaveCalibrationToDisk());
	TestTrue(TEXT("Normal re-save preserves runtime restoration metadata"), Placement->HasSavedAlignment());
	FString Text;
	FFileHelper::LoadFileToString(Text, *Placement->GetCalibrationFilePath());
	for (const TCHAR* Field : {TEXT("alignmentVersion"), TEXT("alignmentMarkers"), TEXT("primaryMarker")})
	{
		TestTrue(TEXT("Normal re-save preserves guided metadata in the file"), Text.Contains(Field));
	}
	TSharedPtr<FJsonObject> Resaved;
	if (!TestTrue(TEXT("Resaved metadata parses"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Resaved))) { return false; }
	TestEqual(TEXT("Normal re-save preserves the selected primary marker"), Resaved->GetIntegerField(TEXT("primaryMarker")), 2);
	TestEqual(TEXT("Normal re-save preserves both validated IDs"), Resaved->GetArrayField(TEXT("alignmentMarkers")).Num(), 2);
	Fixture.Root->Loader->LoadVehicle(Fixture.Profile);
	EyeMarker(Placement, 2, FTransform(FVector(220, 20, 30)));
	TestTrue(TEXT("Normal re-save preserves single-marker confirmation on reload"), Placement->NeedsRestoreConfirmation());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRPanelResaveTest, "CXMR.Alignment.PanelRestoredResave",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRPanelResaveTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FEyeFixture> Fixture = MakeShared<FEyeFixture>();
	ConfigureEye(*this, Fixture->Profile);
	UCXMRPlacementComponent* Placement = Fixture->Root->Placement;
	AlignEye(*this, Placement);
	const FTransform Final = Fixture->Root->GetActorTransform();
	const FTransform First = FTransform(FVector(0, 0, 40)) * Final;
	const FTransform Second = FTransform(FVector(120, 30, 40)) * Final;
	Placement->BeginAlignmentCapture();
	EyeMarker(Placement, 1, First);
	EyeMarker(Placement, 2, Second);
	Placement->SaveAlignment();
	UCXMRTuningWindowComponent* Control = NewObject<UCXMRTuningWindowComponent>(Fixture->Root);
	Control->RegisterComponent();
	Placement->CalibrationSettleSeconds = 0.2f;
	Control->StartCalibration();
	EyeMarker(Placement, 1, First);
	EyeMarker(Placement, 2, Second);
	AddCommand(new FAdvanceEyeTimers(Fixture, 0.3f, [this, Fixture, Placement, Control, Second, Final]()
	{
		TestTrue(TEXT("Saved pair restored"), Placement->bCalibrated);
		TestTrue(TEXT("Restored alignment can be confirmed"), Control->ConfirmCalibration());
		TestTrue(TEXT("Panel re-save succeeds"), Control->SaveCalibration());
		TestTrue(TEXT("Panel re-save retains runtime guided metadata"), Placement->HasSavedAlignment());
		Fixture->Root->Loader->LoadVehicle(Fixture->Profile);
		Placement->CalibrationSettleSeconds = 0.f;
		EyeMarker(Placement, 2, Second);
		TestTrue(TEXT("Panel re-save retains single-marker confirmation"), Placement->NeedsRestoreConfirmation());
		TestTrue(TEXT("Panel re-save restores the same physical alignment"), Fixture->Root->GetActorTransform().Equals(Final, 0.01));
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRStandaloneSaveRequestTest, "CXMR.Alignment.StandaloneSaveRequest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRStandaloneSaveRequestTest::RunTest(const FString& Parameters)
{
	FEyeFixture Fixture;
	ConfigureEye(*this, Fixture.Profile);
	UCXMRPlacementComponent* Placement = Fixture.Root->Placement;
	AlignEye(*this, Placement);
	Placement->SaveMarkerOffsetToProfile();
	TestTrue(TEXT("Standalone shortcut starts fresh capture"), Placement->HasAlignmentCapture());
	TestTrue(TEXT("Standalone confirmation explains the next step"), Placement->GetAlignmentSaveMessage().ToString().Contains(TEXT("press Enter again")));
	EyeMarker(Placement, 1, FTransform(FVector(100, 20, 30)));
	Placement->SaveMarkerOffsetToProfile();
	TestFalse(TEXT("Standalone request cannot save a partial capture"), Placement->WasLastCalibrationSaveSuccessful());
	TestTrue(TEXT("Standalone partial capture has a reason"), Placement->GetAlignmentSaveMessage().ToString().Contains(TEXT("1 / 2")));
	EyeMarker(Placement, 2, FTransform(FVector(220, 20, 30)));
	Placement->SaveMarkerOffsetToProfile();
	TestTrue(TEXT("Standalone shortcut saves without any window component"), Placement->WasLastCalibrationSaveSuccessful());
	Placement->NudgeVehicleInFrame(FVector(1, 0, 0), 0, 70, FVector::ZeroVector);
	Fixture.Profile->VehicleRootOffset.AddToTranslation(FVector(1, 0, 0));
	Placement->SaveMarkerOffsetToProfile();
	TestFalse(TEXT("Unapplied model settings cannot be captured"), Placement->HasAlignmentCapture());
	TestTrue(TEXT("Unapplied model settings have a specific save reason"), Placement->GetAlignmentSaveMessage().ToString().Contains(TEXT("reload the vehicle")));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRLearnAfterGuidedSaveTest, "CXMR.Alignment.LearnAfterGuidedSave",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRLearnAfterGuidedSaveTest::RunTest(const FString& Parameters)
{
	FEyeFixture Fixture;
	ConfigureEye(*this, Fixture.Profile);
	UCXMRPlacementComponent* Placement = Fixture.Root->Placement;
	AlignEye(*this, Placement);
	const FTransform Final = Fixture.Root->GetActorTransform();
	const FTransform First = FTransform(FVector(0, 0, 40)) * Final;
	const FTransform Second = FTransform(FVector(120, 30, 40)) * Final;
	Placement->BeginAlignmentCapture();
	EyeMarker(Placement, 1, First);
	EyeMarker(Placement, 2, Second);
	TestTrue(TEXT("Guided layout saved before learning"), Placement->SaveAlignment());
	Placement->Recalibrate();
	EyeMarker(Placement, 1, First);
	EyeMarker(Placement, 2, Second);
	EyeMarker(Placement, 3, FTransform(FVector(250, 0, 40)) * Final);
	Placement->LearnMarkerLayout();
	TestTrue(TEXT("New calibration marker is learned"), Fixture.Profile->MarkerProfile->GetEntryMutable(3) != nullptr);
	TestTrue(TEXT("Learning saves successfully"), Placement->WasLastCalibrationSaveSuccessful());
	TestFalse(TEXT("Changed marker set cannot keep old completion metadata"), Placement->HasSavedAlignment());
	TestTrue(TEXT("Learned file can be reloaded"), Placement->LoadCalibrationFromDisk());
	TestFalse(TEXT("Reload does not claim the new marker was validated"), Placement->HasSavedAlignment());
	TestTrue(TEXT("Reload keeps the learned marker"), Fixture.Profile->MarkerProfile->GetEntryMutable(3) != nullptr);
	return true;
}
#endif
