// Copyright GMTCK CX.

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CXMRVehicleRoot.h"
#include "CXMRVehicleProfile.h"
#include "CXMRVehicleLoaderComponent.h"
#include "CXMRPlacementComponent.h"
#include "CXMRMarkerProfile.h"
#include "CXMRUsbPortTarget.h"
#include "CXMRDesignOption.h"
#include "CXMRMarkerFollower.h"
#include "CXMRMarkerDebugComponent.h"
#include "HAL/IConsoleManager.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
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
		TSet<FString> ExtraFiles;
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
			Root->Placement->AlignmentCaptureMinSamples = 1;
		}
		~FEyeFixture()
		{
			ExtraFiles.Add(Root->Placement->GetCalibrationFilePath());
			for (const FString& Path : ExtraFiles)
			{
				IFileManager::Get().Delete(*Path);
				IFileManager::Get().Delete(*FPaths::ChangeExtension(Path, TEXT("startup.json")));
				IFileManager::Get().Delete(*(Path + TEXT(".pending")));
			}
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

	void EyeMarker(UCXMRPlacementComponent* Placement, int32 Id, const FTransform& Pose);
	bool AlignEye(FAutomationTestBase& Test, UCXMRPlacementComponent* Placement)
	{
		UFunction* Function = Placement->FindFunction(TEXT("AlignToDriverEyePose"));
		if (!Test.TestNotNull(TEXT("Placement supports eye-based alignment"), Function)) { return false; }
		struct FParams { FTransform HeadWorld; bool ReturnValue = false; } Params;
		Params.HeadWorld = FTransform(FRotator(-20, 70, 15), FVector(300, -100, 140));
		Placement->ProcessEvent(Function, &Params);
		if (!Test.TestTrue(TEXT("Configured eye alignment succeeds"), Params.ReturnValue)) { return false; }
		for (const FCXMRMarkerEntry& Entry : Placement->MarkerProfile->Markers)
		{
			if (Entry.Role == ECXMRMarkerRole::Calibration) { EyeMarker(Placement, Entry.MarkerId, Entry.LocalOffset * Placement->GetOwner()->GetActorTransform()); }
		}
		return true;
	}
	void EyeMarker(UCXMRPlacementComponent* Placement, int32 Id, const FTransform& Pose)
	{
		struct FParams { int32 Id; FVector Location; FRotator Rotation; FVector2D Size; } Params{Id, Pose.GetLocation(), Pose.Rotator(), FVector2D(10, 10)};
		Placement->ProcessEvent(Placement->FindFunctionChecked(TEXT("HandleMarkerMoved")), &Params);
	}

	void SetAlignmentGroup(FAutomationTestBase& Test, UCXMRVehicleProfile* Profile, FName Group)
	{
		FNameProperty* Field = FindFProperty<FNameProperty>(Profile->GetClass(), TEXT("AlignmentGroup"));
		if (Test.TestNotNull(TEXT("Vehicle profile supports an optional alignment group"), Field)) { Field->SetPropertyValue_InContainer(Profile, Group); }
	}

	UCXMRVehicleProfile* OtherGroupVehicle(FEyeFixture& Fixture, int32 Index)
	{
		UCXMRVehicleProfile* Other = NewObject<UCXMRVehicleProfile>();
		Other->VehicleActor = ACXMRVehicleRoot::StaticClass();
		Other->VehicleRootOffset = FTransform(FRotator(0, Index * 15, 0), FVector(-800, Index * 250, 10));
		UCXMRMarkerProfile* Markers = NewObject<UCXMRMarkerProfile>();
		Markers->CalibrationId = TEXT("GroupTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
		Markers->Markers = Fixture.Profile->MarkerProfile->Markers;
		for (FCXMRMarkerEntry& Entry : Markers->Markers)
		{
			Entry.LocalOffset = FTransform(FVector(Index * 300, Entry.MarkerId * 10, 25));
			Entry.Timeout = Index;
		}
		Other->MarkerProfile = Markers;
		return Other;
	}

	bool SaveGroupPose(FAutomationTestBase& Test, FEyeFixture& Fixture, const FTransform& First, const FTransform& Second)
	{
		UCXMRPlacementComponent* Placement = Fixture.Root->Placement;
		Fixture.ExtraFiles.Add(Placement->GetCalibrationFilePath());
		if (!Test.TestTrue(TEXT("Group alignment can be confirmed"), Placement->BeginAlignmentCapture())) { return false; }
		EyeMarker(Placement, 1, First);
		EyeMarker(Placement, 2, Second);
		return Test.TestTrue(TEXT("Fresh complete group alignment saves"), Placement->SaveAlignment());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRAlignmentGroupSwitchTest, "CXMR.Alignment.Group.ThreeVehicles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRAlignmentGroupSwitchTest::RunTest(const FString& Parameters)
{
	FEyeFixture Fixture;
	const FName Group(*(TEXT("SharedTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	SetAlignmentGroup(*this, Fixture.Profile, Group);
	FCXMRMarkerEntry Dynamic;
	Dynamic.MarkerId = 99; Dynamic.Role = ECXMRMarkerRole::DynamicObject; Dynamic.LocalOffset = FTransform(FVector(50, 60, 70));
	Fixture.Profile->MarkerProfile = DuplicateObject<UCXMRMarkerProfile>(Fixture.Profile->MarkerProfile.Get(), Fixture.Profile);
	Fixture.Profile->MarkerProfile->Markers.Add(Dynamic);
	Fixture.Root->Loader->LoadVehicle(Fixture.Profile);
	ConfigureEye(*this, Fixture.Profile);
	AlignEye(*this, Fixture.Root->Placement);
	const FTransform Accepted = Fixture.Root->GetActorTransform();
	const FTransform First(FVector(100, 20, 30)), Second(FVector(220, 20, 30));
	if (!SaveGroupPose(*this, Fixture, First, Second)) { return false; }
	const FString SharedPath = Fixture.Root->Placement->GetCalibrationFilePath();
	FString Before; FFileHelper::LoadFileToString(Before, *SharedPath);
	for (int32 Index : {1, 2})
	{
		UCXMRVehicleProfile* Other = OtherGroupVehicle(Fixture, Index);
		SetAlignmentGroup(*this, Other, Group);
		const FCXMRMarkerEntry* DynamicEntry = Other->MarkerProfile->GetEntryMutable(99);
		if (!TestNotNull(TEXT("The second vehicle has its own dynamic-object marker"), DynamicEntry)) { return false; }
		const FTransform DynamicOffset = DynamicEntry->LocalOffset;
		Fixture.Root->Loader->LoadVehicle(Other);
		Fixture.ExtraFiles.Add(Fixture.Root->Placement->GetCalibrationFilePath());
		TestEqual(TEXT("Different vehicle/model/profile IDs share one file"), Fixture.Root->Placement->GetCalibrationFilePath(), SharedPath);
		TestTrue(TEXT("Switch keeps the accepted anchor without new marker observations"), Fixture.Root->GetActorTransform().Equals(Accepted, 0.01));
		TestTrue(TEXT("Switch keeps confirmed shared alignment"), Fixture.Root->Placement->bCalibrated);
		TestTrue(TEXT("Shared restoration metadata is available"), Fixture.Root->Placement->HasSavedAlignment());
		TestTrue(TEXT("Each model retains its authored offset"), Fixture.Root->Loader->GetSpawnedVehicle()->GetRootComponent()->GetRelativeTransform().Equals(Other->VehicleRootOffset, 0.001));
		TestTrue(TEXT("Dynamic object offset stays vehicle-specific"), Other->MarkerProfile->GetEntryMutable(99)->LocalOffset.Equals(DynamicOffset));
		TestEqual(TEXT("Marker tracking settings stay vehicle-specific"), Other->MarkerProfile->GetEntryMutable(1)->Timeout, static_cast<float>(Index));
	}
	FString After; FFileHelper::LoadFileToString(After, *SharedPath);
	TestEqual(TEXT("Switching vehicles does not write the shared file"), After, Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRAlignmentGroupRestoreTest, "CXMR.Alignment.Group.RestoreAndResave",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRAlignmentGroupRestoreTest::RunTest(const FString& Parameters)
{
	FEyeFixture Fixture;
	const FName Group(*(TEXT("RestoreTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	SetAlignmentGroup(*this, Fixture.Profile, Group);
	Fixture.Root->Loader->LoadVehicle(Fixture.Profile);
	ConfigureEye(*this, Fixture.Profile);
	UCXMRPlacementComponent* Placement = Fixture.Root->Placement;
	AlignEye(*this, Placement);
	const FTransform First(FVector(100, 20, 30)), Second(FVector(220, 20, 30));
	const FTransform Original = Fixture.Root->GetActorTransform();
	if (!SaveGroupPose(*this, Fixture, First, Second)) { return false; }
	UCXMRVehicleProfile* Other = OtherGroupVehicle(Fixture, 1);
	SetAlignmentGroup(*this, Other, Group);
	Placement->bCalibrated = false;
	Fixture.Root->Loader->LoadVehicle(Other);
	Fixture.Root->SetActorTransform(FTransform::Identity);
	EyeMarker(Placement, 2, Second);
	TestTrue(TEXT("One marker preserves the untouched anchor"), Fixture.Root->GetActorTransform().Equals(FTransform::Identity, 0.001));

	EyeMarker(Placement, 1, First);
	TestTrue(TEXT("Complete marker set restores shared alignment"), Placement->bCalibrated);
	Placement->NudgeVehicleInFrame(FVector(5, 0, 0), 0, 70, FVector::ZeroVector);
	const FTransform Updated = Fixture.Root->GetActorTransform();
	if (!SaveGroupPose(*this, Fixture, First, Second)) { return false; }
	Fixture.Root->Loader->LoadVehicle(Fixture.Profile);
	TestTrue(TEXT("Same-session switch keeps the newly saved pose"), Fixture.Root->GetActorTransform().Equals(Updated, 0.01));
	Placement->bCalibrated = false;
	Fixture.Root->Loader->LoadVehicle(Fixture.Profile);
	Fixture.Root->SetActorTransform(FTransform::Identity);
	EyeMarker(Placement, 1, First);
	EyeMarker(Placement, 2, Second);
	TestTrue(TEXT("Saving from B updates A's later restoration"), Fixture.Root->GetActorTransform().Equals(Updated, 0.01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRAlignmentGroupIsolationTest, "CXMR.Alignment.Group.Isolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRAlignmentGroupIsolationTest::RunTest(const FString& Parameters)
{
	FEyeFixture Fixture;
	UCXMRPlacementComponent* Placement = Fixture.Root->Placement;
	ConfigureEye(*this, Fixture.Profile); AlignEye(*this, Placement);
	const FTransform First(FVector(100, 20, 30)), Second(FVector(220, 20, 30));
	if (!SaveGroupPose(*this, Fixture, First, Second)) { return false; }
	const FString IndividualPath = Placement->GetCalibrationFilePath();
	FString Before; FFileHelper::LoadFileToString(Before, *IndividualPath);
	const FString Group = TEXT("IsolateTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	SetAlignmentGroup(*this, Fixture.Profile, FName(*Group));
	Fixture.Root->Loader->LoadVehicle(Fixture.Profile);
	TestNotEqual(TEXT("Group file is separate from an existing individual save"), Placement->GetCalibrationFilePath(), IndividualPath);
	TestFalse(TEXT("Enabling sharing does not import the individual file"), Placement->HasSavedAlignment());
	AlignEye(*this, Placement);
	if (!SaveGroupPose(*this, Fixture, First, Second)) { return false; }
	const FString SharedPath = Placement->GetCalibrationFilePath();
	SetAlignmentGroup(*this, Fixture.Profile, FName(*(Group.ToUpper())));
	TestEqual(TEXT("Group names follow FName case-insensitive identity"), Placement->GetCalibrationFilePath(), SharedPath);
	SetAlignmentGroup(*this, Fixture.Profile, FName(*(Group + TEXT("Other"))));
	Fixture.Root->Loader->LoadVehicle(Fixture.Profile);
	TestFalse(TEXT("Another group cannot inherit the saved group"), Placement->HasSavedAlignment());
	SetAlignmentGroup(*this, Fixture.Profile, NAME_None);
	Fixture.Root->Loader->LoadVehicle(Fixture.Profile);
	TestEqual(TEXT("None returns to the original individual file"), Placement->GetCalibrationFilePath(), IndividualPath);
	TestTrue(TEXT("Individual restoration metadata is preserved"), Placement->HasSavedAlignment());
	FString After; FFileHelper::LoadFileToString(After, *IndividualPath);
	TestEqual(TEXT("Sharing leaves the individual file unchanged"), After, Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRAlignmentGroupMarkerSetTest, "CXMR.Alignment.Group.MarkerMismatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRAlignmentGroupMarkerSetTest::RunTest(const FString& Parameters)
{
	FEyeFixture Fixture;
	const FName Group(*(TEXT("MismatchTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	SetAlignmentGroup(*this, Fixture.Profile, Group);
	Fixture.Root->Loader->LoadVehicle(Fixture.Profile);
	ConfigureEye(*this, Fixture.Profile); AlignEye(*this, Fixture.Root->Placement);
	if (!SaveGroupPose(*this, Fixture, FTransform(FVector(100, 20, 30)), FTransform(FVector(220, 20, 30)))) { return false; }
	const FString SharedPath = Fixture.Root->Placement->GetCalibrationFilePath();
	FString Before; FFileHelper::LoadFileToString(Before, *SharedPath);
	UCXMRVehicleProfile* Other = OtherGroupVehicle(Fixture, 1);
	SetAlignmentGroup(*this, Other, Group);
	Other->MarkerProfile->Markers[1].MarkerId = 3;
	const FTransform Authored = Other->MarkerProfile->Markers[0].LocalOffset;
	Fixture.Root->Loader->LoadVehicle(Other);
	Fixture.ExtraFiles.Add(Fixture.Root->Placement->GetCalibrationFilePath());
	TestFalse(TEXT("A different calibration marker set cannot load shared alignment"), Fixture.Root->Placement->HasSavedAlignment());
	TestTrue(TEXT("Rejected load preserves authored offsets"), Other->MarkerProfile->Markers[0].LocalOffset.Equals(Authored));
	ConfigureEye(*this, Other); AlignEye(*this, Fixture.Root->Placement);
	Fixture.Root->Placement->BeginAlignmentCapture();
	EyeMarker(Fixture.Root->Placement, 1, FTransform(FVector(100, 20, 30)));
	EyeMarker(Fixture.Root->Placement, 3, FTransform(FVector(220, 20, 30)));
	TestFalse(TEXT("An incompatible group member cannot overwrite the existing group"), Fixture.Root->Placement->SaveAlignment());
	FString After; FFileHelper::LoadFileToString(After, *SharedPath);
	TestEqual(TEXT("Incompatible save preserves the shared file"), After, Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRAlignmentGroupCaptureTest, "CXMR.Alignment.Group.CaptureScope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRAlignmentGroupCaptureTest::RunTest(const FString& Parameters)
{
	FEyeFixture Fixture;
	const FName Group(*(TEXT("CaptureTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	SetAlignmentGroup(*this, Fixture.Profile, Group);
	Fixture.Root->Loader->LoadVehicle(Fixture.Profile);
	ConfigureEye(*this, Fixture.Profile); AlignEye(*this, Fixture.Root->Placement);
	Fixture.Root->Placement->BeginAlignmentCapture();
	EyeMarker(Fixture.Root->Placement, 1, FTransform(FVector(100, 20, 30)));
	EyeMarker(Fixture.Root->Placement, 2, FTransform(FVector(220, 20, 30)));
	TestTrue(TEXT("Capture is complete before its storage scope changes"), Fixture.Root->Placement->CanSaveAlignment());
	SetAlignmentGroup(*this, Fixture.Profile, FName(*(Group.ToString() + TEXT("Changed"))));
	TestFalse(TEXT("Changing group invalidates an in-flight capture"), Fixture.Root->Placement->HasAlignmentCapture());
	Fixture.ExtraFiles.Add(Fixture.Root->Placement->GetCalibrationFilePath());
	TestFalse(TEXT("Old capture cannot write a newly selected group"), Fixture.Root->Placement->SaveAlignment());
	TestFalse(TEXT("No file is created from an invalid capture"), IFileManager::Get().FileExists(*Fixture.Root->Placement->GetCalibrationFilePath()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRAlignmentGroupIdentityTest, "CXMR.Alignment.Group.FileIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRAlignmentGroupIdentityTest::RunTest(const FString& Parameters)
{
	FEyeFixture Fixture;
	const FName Group(*(TEXT("IdentityTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	SetAlignmentGroup(*this, Fixture.Profile, Group);
	Fixture.Root->Loader->LoadVehicle(Fixture.Profile);
	ConfigureEye(*this, Fixture.Profile); AlignEye(*this, Fixture.Root->Placement);
	if (!SaveGroupPose(*this, Fixture, FTransform(FVector(100, 20, 30)), FTransform(FVector(220, 20, 30)))) { return false; }
	const FString Path = Fixture.Root->Placement->GetCalibrationFilePath();
	FString Original; FFileHelper::LoadFileToString(Original, *Path);
	TSharedPtr<FJsonObject> Data;
	if (!TestTrue(TEXT("Shared save is valid JSON"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Original), Data))) { return false; }
	Data->SetStringField(TEXT("alignmentGroup"), TEXT("AnotherGroup"));
	FString WrongGroup; FJsonSerializer::Serialize(Data.ToSharedRef(), TJsonWriterFactory<>::Create(&WrongGroup));
	FFileHelper::SaveStringToFile(WrongGroup, *Path);
	TestFalse(TEXT("A shared filename cannot bypass group identity validation"), Fixture.Root->Placement->LoadCalibrationFromDisk());
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Original), Data);
	Data->RemoveField(TEXT("alignmentVersion"));
	Data->RemoveField(TEXT("alignmentMarkers"));
	Data->RemoveField(TEXT("primaryMarker"));
	FString Unvalidated; FJsonSerializer::Serialize(Data.ToSharedRef(), TJsonWriterFactory<>::Create(&Unvalidated));
	FFileHelper::SaveStringToFile(Unvalidated, *Path);
	TestFalse(TEXT("A group cannot load an unvalidated raw layout"), Fixture.Root->Placement->LoadCalibrationFromDisk());
	TestTrue(TEXT("Rejected reads preserve the already accepted runtime layout"), Fixture.Root->Placement->HasSavedAlignment());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRAlignmentGroupResetTest, "CXMR.Alignment.Group.ResetScope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRAlignmentGroupResetTest::RunTest(const FString& Parameters)
{
	FEyeFixture Fixture;
	UCXMRPlacementComponent* Placement = Fixture.Root->Placement;
	ConfigureEye(*this, Fixture.Profile); AlignEye(*this, Placement);
	const FTransform First(FVector(100, 20, 30)), Second(FVector(220, 20, 30));
	if (!SaveGroupPose(*this, Fixture, First, Second)) { return false; }
	const FString Individual = Placement->GetCalibrationFilePath();
	FString Before; FFileHelper::LoadFileToString(Before, *Individual);
	const FName Group(*(TEXT("ResetTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	SetAlignmentGroup(*this, Fixture.Profile, Group);
	Fixture.Root->Loader->LoadVehicle(Fixture.Profile);
	AlignEye(*this, Placement);
	if (!SaveGroupPose(*this, Fixture, First, Second)) { return false; }
	const FString Shared = Placement->GetCalibrationFilePath();
	UCXMRTuningWindowComponent* Control = NewObject<UCXMRTuningWindowComponent>(Fixture.Root);
	Control->RegisterComponent();
	UFunction* Reset = Control->FindFunction(TEXT("ResetSharedAlignment"));
	if (!TestNotNull(TEXT("The control panel exposes a group alignment reset"), Reset)) { return false; }
	Control->ProcessEvent(Reset, nullptr);
	TestTrue(TEXT("First reset click preserves the group save"), IFileManager::Get().FileExists(*Shared));
	TestTrue(TEXT("First reset click preserves restoration metadata"), Placement->HasSavedAlignment());
	Control->ProcessEvent(Reset, nullptr);
	TestEqual(TEXT("Reset returns the panel to the beginning of alignment"), Control->GetCalibrationPhase(), 0);
	TestFalse(TEXT("Reset removes the selected group's saved alignment"), IFileManager::Get().FileExists(*Shared));
	TestFalse(TEXT("Reset clears shared restoration metadata"), Placement->HasSavedAlignment());
	FString After; FFileHelper::LoadFileToString(After, *Individual);
	TestEqual(TEXT("Group reset preserves an individual alignment file"), After, Before);
	UCXMRVehicleProfile* Other = OtherGroupVehicle(Fixture, 1);
	SetAlignmentGroup(*this, Other, Group);
	Fixture.Root->Loader->LoadVehicle(Other);
	TestFalse(TEXT("Other group members cannot reload the reset alignment"), Placement->HasSavedAlignment());
	Fixture.ExtraFiles.Add(Placement->GetCalibrationFilePath());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRResetFreshObservationsTest, "CXMR.ReviewState.ResetFreshObservations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRResetFreshObservationsTest::RunTest(const FString&)
{
	FEyeFixture Fixture;
	UCXMRPlacementComponent* Placement = Fixture.Root->Placement;
	const FName Group(*(TEXT("ResetState_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	SetAlignmentGroup(*this, Fixture.Profile, Group);
	Fixture.Root->Loader->LoadVehicle(Fixture.Profile);
	if (!ConfigureEye(*this, Fixture.Profile) || !AlignEye(*this, Placement)
		|| !SaveGroupPose(*this, Fixture, FTransform(FVector(100, 20, 30)), FTransform(FVector(220, 20, 30))))
	{
		return false;
	}
	TestTrue(TEXT("Saved alignment is calibrated and frozen"), Placement->bCalibrated && Placement->bFreezeAfterCalibration);
	const FTransform Before = Fixture.Root->GetActorTransform();
	UCXMRTuningWindowComponent* Control = NewObject<UCXMRTuningWindowComponent>(Fixture.Root);
	Control->RegisterComponent();
	Control->ResetSharedAlignment();
	Control->ResetSharedAlignment();
	TestFalse(TEXT("Reset clears calibrated state"), Placement->bCalibrated);
	TestFalse(TEXT("Reset clears manual alignment"), Placement->IsManualAlignment());

	TestFalse(TEXT("Reset clears restoration metadata"), Placement->HasSavedAlignment());
	TestTrue(TEXT("Reset header returns to pending"), Control->GetAlignmentStatus(Fixture.Root->Loader).ToString().Contains(TEXT("Alignment pending")));
	TestTrue(TEXT("Reset does not move the anchor from stale observations"), Fixture.Root->GetActorTransform().Equals(Before, 0.001f));
	TestFalse(TEXT("Previous observations cannot confirm a new alignment"), Placement->BeginAlignmentCapture());
	EyeMarker(Placement, 1, FTransform(FVector(600, 40, 60)));
	TestFalse(TEXT("One new observation cannot complete the two-marker alignment"), Placement->bCalibrated);
	TestTrue(TEXT("A partial marker set preserves the anchor"), Fixture.Root->GetActorTransform().Equals(Before, 0.001f));
	EyeMarker(Placement, 2, FTransform(FVector(700, 40, 60)));
	TestTrue(TEXT("Only a complete fresh pair calibrates again"), Placement->bCalibrated);
	TestTrue(TEXT("Fresh pair uses authored marker offsets"), Fixture.Root->GetActorLocation().Equals(FVector(500, 40, 60), 0.01f));
	return true;
}

namespace
{
	void CheckResetStillSettling(FAutomationTestBase& Test, FEyeFixture& Fixture)
	{
		Test.TestFalse(TEXT("Old settle timer cannot finish the new calibration early"), Fixture.Root->Placement->bCalibrated);
		Test.TestTrue(TEXT("Old samples are excluded from the new fit"), Fixture.Root->GetActorLocation().Equals(FVector(800, 10, 20), 0.01f));
	}
	void CheckResetFinishedSettling(FAutomationTestBase& Test, FEyeFixture& Fixture)
	{
		Test.TestTrue(TEXT("New observations finish after their own settle interval"), Fixture.Root->Placement->bCalibrated);
		Test.TestTrue(TEXT("Fresh sample mean keeps the new anchor"), Fixture.Root->GetActorLocation().Equals(FVector(800, 10, 20), 0.01f));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRResetPendingSettleTest, "CXMR.ReviewState.ResetPendingSettle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRResetPendingSettleTest::RunTest(const FString&)
{
	TSharedPtr<FEyeFixture> Fixture = MakeShared<FEyeFixture>();
	UCXMRPlacementComponent* Placement = Fixture->Root->Placement;
	const FName Group(*(TEXT("ResetSettle_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	SetAlignmentGroup(*this, Fixture->Profile, Group);
	Fixture->Root->Loader->LoadVehicle(Fixture->Profile);
	Fixture->Profile->MarkerProfile->Markers.SetNum(1);
	Placement->CalibrationSettleSeconds = 0.2f;
	EyeMarker(Placement, 1, FTransform(FVector(100, 0, 0)));
	Fixture->World->GetTimerManager().Tick(0.f);
	TestFalse(TEXT("Original calibration is still settling"), Placement->bCalibrated);
	TestTrue(TEXT("Reset without a saved file succeeds"), Placement->TryResetCalibrationToAuthored());
	Placement->CalibrationSettleSeconds = 0.5f;
	EyeMarker(Placement, 1, FTransform(FVector(900, 10, 20)));
	AddCommand(new FAdvanceEyeTimers(Fixture, 0.25f, [this, Fixture] { CheckResetStillSettling(*this, *Fixture); }));
	AddCommand(new FAdvanceEyeTimers(Fixture, 0.3f, [this, Fixture] { CheckResetFinishedSettling(*this, *Fixture); }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRAlignmentGroupChangedResaveTest, "CXMR.Alignment.Group.ChangedScopeResave",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRAlignmentGroupChangedResaveTest::RunTest(const FString& Parameters)
{
	FEyeFixture Fixture;
	const FString Group = TEXT("ResaveScopeTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	SetAlignmentGroup(*this, Fixture.Profile, FName(*Group));
	Fixture.Root->Loader->LoadVehicle(Fixture.Profile);
	ConfigureEye(*this, Fixture.Profile); AlignEye(*this, Fixture.Root->Placement);
	if (!SaveGroupPose(*this, Fixture, FTransform(FVector(100, 20, 30)), FTransform(FVector(220, 20, 30)))) { return false; }
	const FString OriginalPath = Fixture.Root->Placement->GetCalibrationFilePath();
	FString Before; FFileHelper::LoadFileToString(Before, *OriginalPath);
	SetAlignmentGroup(*this, Fixture.Profile, FName(*(Group + TEXT("Changed"))));
	const FString ChangedPath = Fixture.Root->Placement->GetCalibrationFilePath();
	Fixture.ExtraFiles.Add(ChangedPath);
	TestFalse(TEXT("A re-save cannot move the old group's accepted layout into another group"), Fixture.Root->Placement->SaveCalibrationToDisk());
	TestFalse(TEXT("Changed scope does not get an implicit copy"), IFileManager::Get().FileExists(*ChangedPath));
	SetAlignmentGroup(*this, Fixture.Profile, NAME_None);
	Fixture.ExtraFiles.Add(Fixture.Root->Placement->GetCalibrationFilePath());
	TestFalse(TEXT("Removing a group also requires reloading before a raw re-save"), Fixture.Root->Placement->SaveCalibrationToDisk());
	FString After; FFileHelper::LoadFileToString(After, *OriginalPath);
	TestEqual(TEXT("Scope changes preserve the original shared file"), After, Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRAlignmentGroupUnsavedSwitchTest, "CXMR.Alignment.Group.UnsavedAdjustment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRAlignmentGroupUnsavedSwitchTest::RunTest(const FString& Parameters)
{
	FEyeFixture Fixture;
	const FName Group(*(TEXT("UnsavedTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	SetAlignmentGroup(*this, Fixture.Profile, Group);
	Fixture.Root->Loader->LoadVehicle(Fixture.Profile);
	ConfigureEye(*this, Fixture.Profile); AlignEye(*this, Fixture.Root->Placement);
	const FTransform Accepted = Fixture.Root->GetActorTransform();
	const FTransform First(FVector(100, 20, 30)), Second(FVector(220, 20, 30));
	if (!SaveGroupPose(*this, Fixture, First, Second)) { return false; }
	Fixture.Root->Placement->AdjustMarkerOffset(FVector(10, 0, 0), FRotator::ZeroRotator);
	TestTrue(TEXT("Legacy adjustment enters the fresh confirmation workflow for a group"), Fixture.Root->Placement->IsManualAlignment());
	UCXMRVehicleProfile* Other = OtherGroupVehicle(Fixture, 1);
	SetAlignmentGroup(*this, Other, Group);
	Fixture.Root->Loader->LoadVehicle(Other);
	TestFalse(TEXT("Switch cannot accept the unsaved adjusted pose as shared alignment"), Fixture.Root->Placement->bCalibrated);
	EyeMarker(Fixture.Root->Placement, 1, First);
	EyeMarker(Fixture.Root->Placement, 2, Second);
	TestTrue(TEXT("All current markers restore the saved pose rather than the unsaved adjustment"), Fixture.Root->GetActorTransform().Equals(Accepted, 0.01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRAlignmentGroupLearnTest, "CXMR.Alignment.Group.LegacyLearn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRAlignmentGroupLearnTest::RunTest(const FString& Parameters)
{
	FEyeFixture Fixture;
	const FName Group(*(TEXT("LearnTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	SetAlignmentGroup(*this, Fixture.Profile, Group);
	Fixture.Root->Loader->LoadVehicle(Fixture.Profile);
	ConfigureEye(*this, Fixture.Profile); AlignEye(*this, Fixture.Root->Placement);
	if (!SaveGroupPose(*this, Fixture, FTransform(FVector(100, 20, 30)), FTransform(FVector(220, 20, 30)))) { return false; }
	const FString Path = Fixture.Root->Placement->GetCalibrationFilePath();
	FString Before; FFileHelper::LoadFileToString(Before, *Path);
	const FTransform OldOffset = Fixture.Profile->MarkerProfile->Markers[0].LocalOffset;
	EyeMarker(Fixture.Root->Placement, 1, FTransform(FVector(500, 20, 30)));
	EyeMarker(Fixture.Root->Placement, 3, FTransform(FVector(800, 20, 30)));
	Fixture.Root->Placement->LearnMarkerLayout();
	TestFalse(TEXT("Legacy Learn cannot save a partially observed or extended shared layout"), Fixture.Root->Placement->WasLastCalibrationSaveSuccessful());
	TestTrue(TEXT("Rejected learning preserves the shared runtime offset"), Fixture.Profile->MarkerProfile->Markers[0].LocalOffset.Equals(OldOffset));
	TestNull(TEXT("Rejected learning does not add a new group marker"), Fixture.Profile->MarkerProfile->GetEntryMutable(3));
	TestTrue(TEXT("Rejected learning preserves accepted calibration"), Fixture.Root->Placement->bCalibrated);
	TestFalse(TEXT("A group learning rejection explains the required workflow"), Fixture.Root->Placement->GetAlignmentStorageMessage().IsEmpty());
	FString After; FFileHelper::LoadFileToString(After, *Path);
	TestEqual(TEXT("Rejected learning preserves the shared file"), After, Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRAlignmentGroupAutomaticCaptureTest, "CXMR.Alignment.Group.AutomaticFirstSave",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRAlignmentGroupAutomaticCaptureTest::RunTest(const FString& Parameters)
{
	FEyeFixture Fixture;
	const FName Group(*(TEXT("AutoGroupTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	SetAlignmentGroup(*this, Fixture.Profile, Group);
	Fixture.Root->Loader->LoadVehicle(Fixture.Profile);
	UCXMRTuningWindowComponent* Control = NewObject<UCXMRTuningWindowComponent>(Fixture.Root);
	Control->RegisterComponent();
	Control->StartCalibration();
	const FTransform First(FVector(1000, 20, 30)), Second(FVector(1100, 20, 30));
	EyeMarker(Fixture.Root->Placement, 1, First);
	EyeMarker(Fixture.Root->Placement, 2, Second);
	TestTrue(TEXT("Authored marker layout can provide initial placement"), Fixture.Root->Placement->bCalibrated);
	TestTrue(TEXT("Automatic placement can be confirmed for the group's first save"), Control->ConfirmCalibration());
	TestTrue(TEXT("A group's first confirmation starts fresh capture even after automatic placement"), Fixture.Root->Placement->HasAlignmentCapture());
	TestFalse(TEXT("Automatic placement cannot skip the group's fresh observations"), Control->CanSaveCalibration());
	EyeMarker(Fixture.Root->Placement, 1, First);
	EyeMarker(Fixture.Root->Placement, 2, Second);
	TestTrue(TEXT("Fresh observations save automatically"), Fixture.Root->Placement->WasLastCalibrationSaveSuccessful());
	Fixture.ExtraFiles.Add(Fixture.Root->Placement->GetCalibrationFilePath());
	TestEqual(TEXT("Automatic group save completes the panel phase"), Control->GetCalibrationPhase(), 3);
	TestTrue(TEXT("First save includes restoration metadata"), Fixture.Root->Placement->HasSavedAlignment());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRGroupResetExpiryTest, "CXMR.ReviewFix.ResetExpiry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRGroupResetExpiryTest::RunTest(const FString& Parameters)
{
	FEyeFixture Fixture;
	const FName Group(*(TEXT("ResetExpiry_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	SetAlignmentGroup(*this, Fixture.Profile, Group);
	Fixture.Root->Loader->LoadVehicle(Fixture.Profile);
	ConfigureEye(*this, Fixture.Profile); AlignEye(*this, Fixture.Root->Placement);
	if (!SaveGroupPose(*this, Fixture, FTransform(FVector(100, 20, 30)), FTransform(FVector(220, 20, 30)))) { return false; }
	const FString Path = Fixture.Root->Placement->GetCalibrationFilePath();
	UCXMRTuningWindowComponent* Control = NewObject<UCXMRTuningWindowComponent>(Fixture.Root);
	Control->RegisterComponent();
	Control->ResetSharedAlignment();
	TestTrue(TEXT("First click arms component confirmation"), Control->IsSharedAlignmentResetArmed());
	TestTrue(TEXT("Confirmation label identifies the group"), Control->GetSharedAlignmentResetLabel().ToString().Contains(Group.ToString()));
	Fixture.World->Tick(LEVELTICK_All, 6.f);
	TestFalse(TEXT("Timeout disarms component confirmation"), Control->IsSharedAlignmentResetArmed());
	Control->ResetSharedAlignment();
	TestTrue(TEXT("An expired confirmation cannot delete the save"), IFileManager::Get().FileExists(*Path));
	Control->ResetSharedAlignment();
	TestFalse(TEXT("The rearmed second click deletes the save"), IFileManager::Get().FileExists(*Path));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRFailureAlignmentStatusTest, "CXMR.ReviewFix.FailureAlignmentStatus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRFailureAlignmentStatusTest::RunTest(const FString& Parameters)
{
	FEyeFixture Fixture;
	Fixture.Profile->DisplayName = FText::FromString(TEXT("Review vehicle"));
	Fixture.Root->Placement->bCalibrated = true;
	UCXMRTuningWindowComponent* Control = NewObject<UCXMRTuningWindowComponent>(Fixture.Root);
	Control->RegisterComponent();
	UCXMRDesignOption* Invalid = NewObject<UCXMRDesignOption>(Fixture.Root);
	TestFalse(TEXT("Invalid option selection fails"), Fixture.Root->Loader->SelectDesignOption(Invalid));
	TestFalse(TEXT("Failure remains available for Review"), Fixture.Root->Loader->GetLastFailureReason().IsEmpty());
	const FString Status = Control->GetAlignmentStatus(Fixture.Root->Loader).ToString();
	TestTrue(TEXT("Failed option keeps the vehicle in the header"), Status.Contains(TEXT("Review vehicle")));
	TestTrue(TEXT("Failed option keeps alignment in the header"), Status.Contains(TEXT("Aligned")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRAlignmentGroupResetFailureTest, "CXMR.Alignment.Group.ResetFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRAlignmentGroupResetFailureTest::RunTest(const FString& Parameters)
{
	FEyeFixture Fixture;
	const FName Group(*(TEXT("ResetFailureTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	SetAlignmentGroup(*this, Fixture.Profile, Group);
	Fixture.Root->Loader->LoadVehicle(Fixture.Profile);
	ConfigureEye(*this, Fixture.Profile); AlignEye(*this, Fixture.Root->Placement);
	UCXMRTuningWindowComponent* Control = NewObject<UCXMRTuningWindowComponent>(Fixture.Root);
	Control->RegisterComponent();
	TestTrue(TEXT("Panel confirms the shared alignment"), Control->ConfirmCalibration());
	EyeMarker(Fixture.Root->Placement, 1, FTransform(FVector(100, 20, 30)));
	EyeMarker(Fixture.Root->Placement, 2, FTransform(FVector(220, 20, 30)));
	Fixture.ExtraFiles.Add(Fixture.Root->Placement->GetCalibrationFilePath());
	if (!TestTrue(TEXT("Panel automatically saves the group before the reset failure"), Fixture.Root->Placement->WasLastCalibrationSaveSuccessful())) { return false; }
	const FString Path = Fixture.Root->Placement->GetCalibrationFilePath();
	FString Before; FFileHelper::LoadFileToString(Before, *Path);
	const FTransform Offset = Fixture.Profile->MarkerProfile->Markers[0].LocalOffset;
	const FTransform Pose = Fixture.Root->GetActorTransform();
	IPlatformFile& Platform = FPlatformFileManager::Get().GetPlatformFile();
	if (!TestTrue(TEXT("Fixture can make the group file read-only"), Platform.SetReadOnly(*Path, true))) { return false; }
	Control->ResetSharedAlignment();
	Control->ResetSharedAlignment();
	Platform.SetReadOnly(*Path, false);
	FString After; FFileHelper::LoadFileToString(After, *Path);
	TestEqual(TEXT("Failed deletion preserves the group file"), After, Before);
	TestTrue(TEXT("Failed reset preserves restoration metadata"), Fixture.Root->Placement->HasSavedAlignment());
	TestTrue(TEXT("Failed reset preserves calibrated state"), Fixture.Root->Placement->bCalibrated);
	TestTrue(TEXT("Failed reset preserves the accepted runtime offset"), Fixture.Profile->MarkerProfile->Markers[0].LocalOffset.Equals(Offset));
	TestTrue(TEXT("Failed reset preserves the accepted anchor"), Fixture.Root->GetActorTransform().Equals(Pose, 0.001));
	TestEqual(TEXT("Failed reset preserves the completed panel phase"), Control->GetCalibrationPhase(), 3);
	TestTrue(TEXT("Failed reset reports failure instead of success"), Control->GetCalibrationMessage().ToString().Contains(TEXT("Reset failed")));
	Control->ResetSharedAlignment();
	Control->ResetSharedAlignment();
	TestFalse(TEXT("Reset can be retried when file access is restored"), IFileManager::Get().FileExists(*Path));
	TestEqual(TEXT("Successful retry returns the panel to setup"), Control->GetCalibrationPhase(), 0);
	return true;
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
	const FTransform ImportedOffset = Vehicle->GetActorTransform().GetRelativeTransform(Fixture.Root->VehicleAnchor->GetComponentTransform());
	Fixture.Root->Placement->NudgeVehicleInFrame(FVector::ZeroVector, 10, 70, FVector(5000, 5000, 0));
	Eye = EyeLocal * Vehicle->GetActorTransform();
	TestTrue(TEXT("Manual turn keeps the driver eye fixed, independent of current head position"), Eye.GetLocation().Equals(FVector(300, -100, 140), 0.01));
	TestTrue(TEXT("Manual turn changes the heading"), FMath::IsNearlyEqual(Eye.Rotator().Yaw, 80.0, 0.01));
	TestTrue(TEXT("Imported geometry offset is unchanged"), Vehicle->GetActorTransform().GetRelativeTransform(Fixture.Root->VehicleAnchor->GetComponentTransform()).Equals(ImportedOffset, 0.01));
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
	const FTransform First = FTransform(FRotator(0,18,0), FVector(20,30,70)) * Final;
	const FTransform Second = FTransform(FRotator(0,40,0), FVector(150,-30,60)) * Final;
	EyeMarker(Placement, 1, First); EyeMarker(Placement, 2, Second);
	TestTrue(TEXT("Current full pair can start a fresh capture"), Placement->BeginAlignmentCapture());
	TestFalse(TEXT("Old observations do not count as fresh samples"), Placement->CanSaveAlignment());
	EyeMarker(Placement, 1, First);
	int32 Lost = 1; Placement->ProcessEvent(Placement->FindFunctionChecked(TEXT("HandleMarkerLost")), &Lost);
	TestFalse(TEXT("Loss cancels the entire capture"), Placement->HasAlignmentCapture());
	TestTrue(TEXT("Loss reason lists the missing ID"), Placement->GetAlignmentSaveMessage().ToString().Contains(TEXT("1")));
	EyeMarker(Placement, 1, First); EyeMarker(Placement, 2, Second);
	Placement->RequestAlignmentSave(); EyeMarker(Placement, 1, First); EyeMarker(Placement, 2, Second);
	TestTrue(TEXT("One request saves a fresh full pair"), Placement->WasLastCalibrationSaveSuccessful());
	Fixture.Root->Loader->LoadVehicle(Fixture.Profile);
	Fixture.Root->SetActorTransform(FTransform::Identity);
	EyeMarker(Placement, 2, Second);
	TestTrue(TEXT("One marker cannot move a two-marker restored vehicle"), Fixture.Root->GetActorTransform().Equals(FTransform::Identity, 0.001));
	TestFalse(TEXT("Partial restore is not calibrated"), Placement->bCalibrated);
	EyeMarker(Placement, 1, First);
	TestTrue(TEXT("The complete pair restores automatically"), Placement->bCalibrated);
	TestTrue(TEXT("Unchanged origin reuses the exact saved pose"), Fixture.Root->GetActorTransform().Equals(Final, 0.001));
	EyeMarker(Placement, 1, FTransform(FVector(999,888,777)));
	TestTrue(TEXT("Accepted alignment stays fixed"), Fixture.Root->GetActorTransform().Equals(Final, 0.001));
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
	FEyeFixture Fixture; ConfigureEye(*this,Fixture.Profile);
	UCXMRPlacementComponent* Placement=Fixture.Root->Placement; AlignEye(*this,Placement);
	UCXMRTuningWindowComponent* Control=NewObject<UCXMRTuningWindowComponent>(Fixture.Root); Control->RegisterComponent();
	const FTransform Offset=Placement->MarkerProfile->Markers[0].LocalOffset;
	const FString Path=Placement->GetCalibrationFilePath(); IFileManager::Get().MakeDirectory(*Path,true);
	Control->RequestAlignmentSave(); EyeMarker(Placement,1,FTransform(FVector(100,20,10))); EyeMarker(Placement,2,FTransform(FVector(200,20,10)));
	TestFalse(TEXT("Failed automatic save cancels the request"),Placement->HasAlignmentCapture());
	TestTrue(TEXT("Failure explains retry"),Control->GetCalibrationMessage().ToString().Contains(TEXT("Save failed")));
	TestTrue(TEXT("Failure preserves profile offsets"),Placement->MarkerProfile->Markers[0].LocalOffset.Equals(Offset));
	IFileManager::Get().DeleteDirectory(*Path,false,false);
	Control->RequestAlignmentSave(); EyeMarker(Placement,1,FTransform(FVector(100,20,10))); EyeMarker(Placement,2,FTransform(FVector(200,20,10)));
	TestTrue(TEXT("Another request retries with fresh samples"),Placement->WasLastCalibrationSaveSuccessful());
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

		EyeMarker(Placement, 2, Second);
	}));
	AddCommand(new FAdvanceEyeTimers(Fixture, 0.3f, [this, Fixture, Placement, Final]()
	{
		TestTrue(TEXT("Consistent pair restores after settling"), Placement->bCalibrated);

		TestTrue(TEXT("Verified pair reproduces the saved alignment"), Fixture->Root->GetActorTransform().Equals(Final, 0.01));
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRDriverEyePanelCaptureTest, "CXMR.Alignment.PanelCaptureWorkflow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRDriverEyePanelCaptureTest::RunTest(const FString& Parameters)
{
	FEyeFixture Fixture; ConfigureEye(*this, Fixture.Profile);
	UCXMRPlacementComponent* Placement = Fixture.Root->Placement; AlignEye(*this, Placement);
	UCXMRTuningWindowComponent* Control = NewObject<UCXMRTuningWindowComponent>(Fixture.Root); Control->RegisterComponent();
	TestTrue(TEXT("Initial alignment can request saving"), Control->CanConfirmCalibration());
	TestTrue(TEXT("Confirm button starts the single save request"), Control->ConfirmCalibration());
	EyeMarker(Placement, 1, FTransform(FVector(100,20,30)));
	TestFalse(TEXT("Incomplete fresh pair is not saved"), Placement->WasLastCalibrationSaveSuccessful());
	Placement->NudgeVehicleInFrame(FVector(1,0,0),0,70,FVector::ZeroVector);
	TestFalse(TEXT("Adjustment cancels capture"), Placement->HasAlignmentCapture());
	TestTrue(TEXT("New pose can start another request"), Control->ConfirmCalibration());
	EyeMarker(Placement,1,FTransform(FVector(100,20,30))); EyeMarker(Placement,2,FTransform(FVector(220,20,30)));
	TestTrue(TEXT("No second click is needed"), Placement->WasLastCalibrationSaveSuccessful());
	TestEqual(TEXT("Panel observes automatic save completion"),Control->GetCalibrationPhase(),3);
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
	EyeMarker(Placement, 2, FTransform(FVector(220, 20, 30)));
	TestTrue(TEXT("Returning to A restores its own alignment"), Fixture.Root->GetActorTransform().Equals(FirstPose, 0.01));

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
	FEyeFixture Fixture; UCXMRPlacementComponent* Placement=Fixture.Root->Placement;
	UCXMRTuningWindowComponent* Control=NewObject<UCXMRTuningWindowComponent>(Fixture.Root); Control->RegisterComponent();
	EyeMarker(Placement,1,FTransform(FVector(100,20,30))); EyeMarker(Placement,2,FTransform(FVector(200,20,30)));
	Placement->NudgeVehicleInFrame(FVector(2,0,0),3,0,FVector::ZeroVector);
	const FTransform Final=Fixture.Root->GetActorTransform();
	Placement->ProcessEvent(Placement->FindFunctionChecked(TEXT("HandleSaveOffsetRequest")),nullptr);
	TestEqual(TEXT("Enter starts capture phase"),Control->GetCalibrationPhase(),2);
	TestEqual(TEXT("Pre-Enter samples are excluded"),Placement->GetCapturedAlignmentMarkerCount(),0);
	EyeMarker(Placement,1,FTransform(FVector(100,20,30)));
	TestFalse(TEXT("One fresh marker cannot save"),Placement->WasLastCalibrationSaveSuccessful());
	EyeMarker(Placement,2,FTransform(FVector(220,20,30)));
	TestTrue(TEXT("Single Enter saves automatically"),Placement->WasLastCalibrationSaveSuccessful());
	TestEqual(TEXT("Keyboard workflow finishes"),Control->GetCalibrationPhase(),3);
	Fixture.Root->Loader->LoadVehicle(Fixture.Profile); Fixture.Root->SetActorTransform(FTransform::Identity);
	EyeMarker(Placement,2,FTransform(FVector(220,20,30))); EyeMarker(Placement,1,FTransform(FVector(100,20,30)));
	TestTrue(TEXT("Keyboard adjustment survives full-set restoration"),Fixture.Root->GetActorTransform().Equals(Final,0.001));
	Fixture.Root->Loader->UnloadVehicle(); Placement->NudgeVehicleInFrame(FVector(1,0,0),0,0,FVector::ZeroVector); Placement->SaveMarkerOffsetToProfile();
	TestTrue(TEXT("Missing vehicle is explained"),Control->GetCalibrationMessage().ToString().Contains(TEXT("Load a vehicle")));
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
	EyeMarker(Placement, 1, FTransform(FVector(100, 20, 30)));
	EyeMarker(Placement, 2, FTransform(FVector(220, 20, 30)));
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
	AddCommand(new FAdvanceEyeTimers(Fixture, 0.3f, [this, Fixture, Placement, Control, First, Second, Final]()
	{
		TestTrue(TEXT("Saved pair restored"), Placement->bCalibrated);
		TestTrue(TEXT("Restored alignment can be confirmed"), Control->ConfirmCalibration());
		EyeMarker(Placement, 1, First); EyeMarker(Placement, 2, Second);
		TestTrue(TEXT("Panel re-save succeeds automatically"), Placement->WasLastCalibrationSaveSuccessful());
		TestTrue(TEXT("Panel re-save retains runtime guided metadata"), Placement->HasSavedAlignment());
		Fixture->Root->Loader->LoadVehicle(Fixture->Profile);
		Placement->CalibrationSettleSeconds = 0.f;
		EyeMarker(Placement, 2, Second);

		TestTrue(TEXT("Panel re-save restores the same physical alignment"), Fixture->Root->GetActorTransform().Equals(Final, 0.01));
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRStandaloneSaveRequestTest, "CXMR.Alignment.StandaloneSaveRequest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRStandaloneSaveRequestTest::RunTest(const FString& Parameters)
{
	FEyeFixture Fixture; ConfigureEye(*this,Fixture.Profile);
	UCXMRPlacementComponent* Placement=Fixture.Root->Placement; AlignEye(*this,Placement);
	Placement->SaveMarkerOffsetToProfile();
	TestTrue(TEXT("Standalone shortcut starts fresh capture"),Placement->HasAlignmentCapture());
	EyeMarker(Placement,1,FTransform(FVector(100,20,30)));
	TestFalse(TEXT("Partial fresh capture does not save"),Placement->WasLastCalibrationSaveSuccessful());
	EyeMarker(Placement,2,FTransform(FVector(220,20,30)));
	TestTrue(TEXT("Shortcut auto-saves without a window"),Placement->WasLastCalibrationSaveSuccessful());
	Placement->NudgeVehicleInFrame(FVector(1,0,0),0,70,FVector::ZeroVector); Fixture.Profile->VehicleRootOffset.AddToTranslation(FVector(1,0,0));
	Placement->SaveMarkerOffsetToProfile();
	TestFalse(TEXT("Unapplied model settings cannot start capture"),Placement->HasAlignmentCapture());
	TestTrue(TEXT("Model mismatch explains reload"),Placement->GetAlignmentSaveMessage().ToString().Contains(TEXT("reload the vehicle")));
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRCaptureMeanOriginTest, "CXMR.Alignment.OctReview.CaptureMeanAndOrigin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRCaptureMeanOriginTest::RunTest(const FString&)
{
	struct FCXMROctAlignmentLogProbe : FOutputDevice
	{
		int32 Saves = 0, Restores = 0;
		FCXMROctAlignmentLogProbe() { GLog->FlushThreadedLogs(); GLog->AddOutputDevice(this); GLog->FlushThreadedLogs(); Saves = Restores = 0; }
		~FCXMROctAlignmentLogProbe() { GLog->RemoveOutputDevice(this); }
		virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override
		{
			if (Category != TEXT("LogCXMRAlignment") || Verbosity != ELogVerbosity::Log) { return; }
			Saves += FCString::Strstr(Message, TEXT("Save marker id=")) ? 1 : 0;
			Restores += FCString::Strstr(Message, TEXT("Restore marker id=")) ? 1 : 0;
		}
	} Logs;
	FEyeFixture Fixture;
	ConfigureEye(*this, Fixture.Profile);
	UCXMRPlacementComponent* Placement = Fixture.Root->Placement;
	AlignEye(*this, Placement);
	Placement->AlignmentCaptureMinSamples = 3;
	const FTransform Final = Fixture.Root->GetActorTransform();
	const FTransform First = FTransform(FVector(20, 30, 40)) * Final;
	const FTransform Second = FTransform(FVector(140, 30, 40)) * Final;
	TestTrue(TEXT("Current observations permit capture"), Placement->BeginAlignmentCapture());
	EyeMarker(Placement, 1, First);
	TestFalse(TEXT("Minimum sample count is enforced"), Placement->SaveAlignment());
	TestTrue(TEXT("Insufficient samples explain progress"), Placement->GetAlignmentSaveMessage().ToString().Contains(TEXT("enough samples")));
	EyeMarker(Placement, 1, FTransform(FVector(-0.2, 0, 0)) * First);
	EyeMarker(Placement, 1, FTransform(FVector(0.2, 0, 0)) * First);
	for (int32 Index = 0; Index < 3; ++Index) { EyeMarker(Placement, 2, Second); }
	TestTrue(TEXT("Complete fresh means save"), Placement->SaveAlignment());
	GLog->FlushThreadedLogs();
	TestEqual(TEXT("Save logs one Log-level line per marker"), Logs.Saves, 2);
	TestTrue(TEXT("Saved local pose uses the mean rather than last sample"), Placement->MarkerProfile->Markers[0].LocalOffset.Equals(First.GetRelativeTransform(Final), 0.001));
	const FString Path = Placement->GetCalibrationFilePath();
	Fixture.Root->Loader->LoadVehicle(Fixture.Profile);
	Fixture.Root->SetActorTransform(FTransform::Identity);
	EyeMarker(Placement, 1, FTransform(First.GetRotation(), First.GetLocation() + FVector(0.1, 0, 0)));
	EyeMarker(Placement, 2, FTransform(Second.GetRotation(), Second.GetLocation() + FVector(0.1, 0, 0)));
	TestTrue(TEXT("Origin within tolerance is identified"), Placement->WasWorldPoseReused());
	TestTrue(TEXT("Restored world pose is exact to 0.001 cm"), Fixture.Root->GetActorTransform().Equals(Final, 0.001));
	Fixture.Root->Loader->LoadVehicle(Fixture.Profile);
	EyeMarker(Placement, 1, FTransform(First.GetRotation(), First.GetLocation() + FVector(5, 0, 0)));
	EyeMarker(Placement, 2, FTransform(Second.GetRotation(), Second.GetLocation() + FVector(5, 0, 0)));
	TestFalse(TEXT("Changed origin is fitted from markers"), Placement->WasWorldPoseReused());
	TestTrue(TEXT("Moved origin shifts the restored root"), Fixture.Root->GetActorLocation().Equals(Final.GetLocation() + FVector(5, 0, 0), 0.001));
	FString Text; FFileHelper::LoadFileToString(Text, *Path);
	TSharedPtr<FJsonObject> Data; FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Data);
	Data->RemoveField(TEXT("worldPose")); Data->RemoveField(TEXT("markerWorld"));
	FJsonSerializer::Serialize(Data.ToSharedRef(), TJsonWriterFactory<>::Create(&Text)); FFileHelper::SaveStringToFile(Text, *Path);
	Fixture.Root->Loader->LoadVehicle(Fixture.Profile);
	EyeMarker(Placement, 1, First); EyeMarker(Placement, 2, Second);
	TestFalse(TEXT("Old files use marker fitting"), Placement->WasWorldPoseReused());
	TestTrue(TEXT("Old files remain compatible"), Placement->bCalibrated && Fixture.Root->GetActorTransform().Equals(Final, 0.001));
	GLog->FlushThreadedLogs();
	TestEqual(TEXT("Each restoration logs one Log-level line per marker"), Logs.Restores, 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRAutomaticAlignmentSaveTest, "CXMR.Alignment.OctReview.AutomaticAlignmentSave",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRAutomaticAlignmentSaveTest::RunTest(const FString&)
{
	for (int32 Mode = 0; Mode < 3; ++Mode)
	{
		FEyeFixture Fixture;
		UCXMRPlacementComponent* Placement = Fixture.Root->Placement;
		if (Mode == 2)
		{
			SetAlignmentGroup(*this, Fixture.Profile, FName(*(TEXT("AutoSave_") + FGuid::NewGuid().ToString(EGuidFormats::Digits))));
			Fixture.Root->Loader->LoadVehicle(Fixture.Profile);
		}
		const FTransform First(FVector(100, 0, 0)), Second(FVector(200, 0, 0));
		EyeMarker(Placement, 1, First); EyeMarker(Placement, 2, Second);
		TestTrue(TEXT("Markers automatically calibrate"), Placement->bCalibrated);
		if (Mode > 0)
		{
			TestTrue(TEXT("Initial capture starts"), Placement->BeginAlignmentCapture());
			EyeMarker(Placement, 1, First); EyeMarker(Placement, 2, Second);
			TestTrue(TEXT("Initial alignment saves"), Placement->SaveAlignment());
			Fixture.Root->Loader->LoadVehicle(Fixture.Profile);
			EyeMarker(Placement, 1, First); EyeMarker(Placement, 2, Second);
			TestTrue(TEXT("Saved alignment automatically restores"), Placement->bCalibrated && Placement->HasSavedAlignment());
		}
		TestFalse(TEXT("No manual movement before shortcut"), Placement->IsManualAlignment());
		const FString Path = Placement->GetCalibrationFilePath();
		FString Before; FFileHelper::LoadFileToString(Before, *Path);
		const FTransform Final = Fixture.Root->GetActorTransform();
		Placement->AlignmentCaptureMinSamples = 3;
		Placement->ProcessEvent(Placement->FindFunctionChecked(TEXT("HandleSaveOffsetRequest")), nullptr);
		TestTrue(TEXT("Enter starts a fresh capture in every automatic state"), Placement->HasAlignmentCapture());
		FString During; FFileHelper::LoadFileToString(During, *Path);
		TestEqual(TEXT("Starting capture leaves the file unchanged"), During, Before);
		for (double Noise : {0.0, -0.2, 0.2})
		{
			EyeMarker(Placement, 1, FTransform(First.GetLocation() + FVector(Noise, 0, 0)));
			EyeMarker(Placement, 2, FTransform(Second.GetLocation() + FVector(Noise, 0, 0)));
			if (Noise != 0.2) { TestFalse(TEXT("Partial samples do not save"), Placement->WasLastCalibrationSaveSuccessful()); }
		}
		TestTrue(TEXT("Complete fresh means automatically save"), Placement->WasLastCalibrationSaveSuccessful());
		TestTrue(TEXT("LocalOffset uses the mean"), Placement->MarkerProfile->Markers[0].LocalOffset.Equals(First.GetRelativeTransform(Final), 0.001));
		FString Saved; FFileHelper::LoadFileToString(Saved, *Path);
		TSharedPtr<FJsonObject> Data;
		if (!TestTrue(TEXT("Saved JSON reads"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Saved), Data))) { return false; }
		const TArray<TSharedPtr<FJsonValue>>* WorldMarkers = nullptr;
		if (!TestTrue(TEXT("Capture writes markerWorld"), Data->TryGetArrayField(TEXT("markerWorld"), WorldMarkers))) { return false; }
		for (const TSharedPtr<FJsonValue>& Item : *WorldMarkers)
		{
			const TSharedPtr<FJsonObject> Marker = Item->AsObject();
			TestEqual(TEXT("markerWorld uses the mean rather than final noise"), Marker->GetNumberField(TEXT("x")), Marker->GetNumberField(TEXT("id")) * 100.0);
		}
		TestTrue(TEXT("Direct profile save remains supported"), Placement->SaveCalibrationToDisk());
		FFileHelper::LoadFileToString(Saved, *Path);
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Saved), Data);
		TestFalse(TEXT("Direct save omits worldPose without a new mean"), Data->HasField(TEXT("worldPose")));
		TestFalse(TEXT("Direct save omits raw markerWorld"), Data->HasField(TEXT("markerWorld")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRSingleMarkerOriginTest, "CXMR.Alignment.OctReview.SingleMarkerOriginYaw",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRSingleMarkerOriginTest::RunTest(const FString&)
{
	FEyeFixture Fixture;
	Fixture.Profile->MarkerProfile->Markers.SetNum(1);
	Fixture.Root->Loader->LoadVehicle(Fixture.Profile);
	ConfigureEye(*this, Fixture.Profile); AlignEye(*this, Fixture.Root->Placement);
	UCXMRPlacementComponent* Placement = Fixture.Root->Placement;
	const FTransform Final = Fixture.Root->GetActorTransform();
	const FTransform Marker = FTransform(FVector(100, 20, 40)) * Final;
	Placement->BeginAlignmentCapture(); EyeMarker(Placement, 1, Marker); Placement->SaveAlignment();
	Fixture.Root->Loader->LoadVehicle(Fixture.Profile); EyeMarker(Placement, 1, Marker);
	TestTrue(TEXT("One unchanged required marker reuses the world pose"), Placement->WasWorldPoseReused() && Fixture.Root->GetActorTransform().Equals(Final, 0.001));
	Fixture.Root->Loader->LoadVehicle(Fixture.Profile);
	EyeMarker(Placement, 1, FTransform(FRotator(0, Marker.Rotator().Yaw + 1, 0), Marker.GetLocation()));
	TestFalse(TEXT("Same position with changed yaw cannot reuse the world pose"), Placement->WasWorldPoseReused());
	TestTrue(TEXT("Single marker still calibrates using its direction"), Placement->bCalibrated);
	FString Text; const FString Path = Placement->GetCalibrationFilePath(); FFileHelper::LoadFileToString(Text, *Path);
	TSharedPtr<FJsonObject> Data; FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Data); Data->RemoveField(TEXT("markerWorld"));
	FJsonSerializer::Serialize(Data.ToSharedRef(), TJsonWriterFactory<>::Create(&Text)); FFileHelper::SaveStringToFile(Text, *Path);
	Fixture.Root->Loader->LoadVehicle(Fixture.Profile); EyeMarker(Placement, 1, Marker);
	TestFalse(TEXT("Incomplete optional origin fields fall back to markers"), Placement->WasWorldPoseReused());
	TestTrue(TEXT("Incomplete optional fields do not reject valid legacy alignment"), Placement->bCalibrated);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRAllMarkersFollowerTest, "CXMR.Alignment.OctReview.RequiredMarkersAndFollower",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRAllMarkersFollowerTest::RunTest(const FString&)
{
	FEyeFixture Fixture;
	UCXMRPlacementComponent* Placement = Fixture.Root->Placement;
	Fixture.Root->SetActorLocation(FVector(50, 60, 70));
	const FTransform Before = Fixture.Root->GetActorTransform();
	EyeMarker(Placement, 1, FTransform(FVector(100, 0, 0)));
	TestTrue(TEXT("Partial profile preserves the anchor"), Fixture.Root->GetActorTransform().Equals(Before));
	TestFalse(TEXT("Partial profile cannot calibrate"), Placement->bCalibrated);
	TestTrue(TEXT("Waiting text names missing ID"), Placement->GetAlignmentProgress().ToString().Contains(TEXT("missing 2")));
	Placement->LearnMarkerLayout();
	TestFalse(TEXT("Partial learning cannot save"), Placement->WasLastCalibrationSaveSuccessful());
	TestFalse(TEXT("Partial profile cannot re-save"), Placement->SaveCalibrationToDisk());
	ACXMRMarkerFollower* Follower = Fixture.World->SpawnActor<ACXMRMarkerFollower>();
	Follower->MarkerId = 2;
	TestEqual(TEXT("Follower IDs are excluded from required calibration"), Placement->GetRequiredAlignmentMarkerCount(), 1);
	EyeMarker(Placement, 1, FTransform(FVector(100, 0, 0)));
	TestTrue(TEXT("One required marker can calibrate"), Placement->bCalibrated);
	Follower->MarkerId = 99;
	EyeMarker(Placement, 2, FTransform(FVector(200, 0, 0)));
	EyeMarker(Placement, 99, FTransform(FVector(500, 0, 0)));
	Placement->LearnMarkerLayout();
	TestNull(TEXT("Follower marker is not learned"), Placement->MarkerProfile->GetEntryMutable(99));
	TestTrue(TEXT("All required calibration markers permit learning"), Placement->WasLastCalibrationSaveSuccessful());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRCaptureCancelExpiryTest, "CXMR.Alignment.OctReview.CaptureCancelAndExpiry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRCaptureCancelExpiryTest::RunTest(const FString&)
{
	FEyeFixture Fixture;
	UCXMRPlacementComponent* Placement = Fixture.Root->Placement;
	Placement->bFreezeAfterCalibration = false;
	EyeMarker(Placement, 1, FTransform(FVector(100, 0, 0))); EyeMarker(Placement, 2, FTransform(FVector(200, 0, 0)));
	Placement->RequestAlignmentSave();
	Fixture.World->RealTimeSeconds += 0.6;
	EyeMarker(Placement, 1, FTransform(FVector(110, 0, 0)));
	TestFalse(TEXT("Observation expiry cancels the entire request before reacquisition"), Placement->HasAlignmentCapture());
	TestTrue(TEXT("Expiry reason names missing markers"), Placement->GetAlignmentSaveMessage().ToString().Contains(TEXT("Missing markers")));
	EyeMarker(Placement, 2, FTransform(FVector(210, 0, 0)));
	TestTrue(TEXT("Cancel restores the previously unlocked follower state"), Fixture.Root->GetActorLocation().Equals(FVector(10, 0, 0), 0.001));
	Placement->RequestAlignmentSave();
	Fixture.Root->SetActorLocation(FVector(900, 0, 0));
	EyeMarker(Placement, 1, FTransform(FVector(120, 0, 0)));
	TestFalse(TEXT("External movement cancels the old capture"), Placement->HasAlignmentCapture());
	EyeMarker(Placement, 2, FTransform(FVector(220, 0, 0)));
	TestTrue(TEXT("Tracking can resume after external movement cancellation"), Fixture.Root->GetActorLocation().X < 900);
	Placement->MoveVehicleTo(FTransform(FVector(800, 0, 0)));
	Placement->RequestAlignmentSave();
	int32 Lost = 1; Placement->ProcessEvent(Placement->FindFunctionChecked(TEXT("HandleMarkerLost")), &Lost);
	EyeMarker(Placement, 1, FTransform(FVector(120, 0, 0)));
	TestTrue(TEXT("A pre-existing manual lock survives cancellation"), Fixture.Root->GetActorLocation().Equals(FVector(800, 0, 0), 0.001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRCaptureTimeoutTest, "CXMR.Alignment.OctReview.CaptureTimeout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRCaptureTimeoutTest::RunTest(const FString&)
{
	TSharedPtr<FEyeFixture> Fixture = MakeShared<FEyeFixture>();
	ConfigureEye(*this, Fixture->Profile); AlignEye(*this, Fixture->Root->Placement);
	UCXMRPlacementComponent* Placement = Fixture->Root->Placement;
	Placement->AlignmentCaptureMinSamples = 3; Placement->AlignmentCaptureTimeout = 0.1f;
	Placement->RequestAlignmentSave(); EyeMarker(Placement, 1, FTransform(FVector(100, 0, 0)));
	AddCommand(new FAdvanceEyeTimers(Fixture, 0.2f, [this, Fixture, Placement]
	{
		TestFalse(TEXT("Timeout clears the request"), Placement->HasAlignmentCapture());
		TestFalse(TEXT("Timeout never writes a partial file"), Placement->WasLastCalibrationSaveSuccessful());
		const FString Message = Placement->GetAlignmentSaveMessage().ToString();
		TestTrue(TEXT("Timeout lists IDs and missing sample counts"), Message.Contains(TEXT("1 (2 more samples)")) && Message.Contains(TEXT("2 (3 more samples)")));
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRPlacementCacheTest, "CXMR.Alignment.OctReview.PlacementCache",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRPlacementCacheTest::RunTest(const FString&)
{
	FEyeFixture Fixture;
	AActor* Host = Fixture.World->SpawnActor<AActor>();
	UCXMRTuningWindowComponent* Control = NewObject<UCXMRTuningWindowComponent>(Host); Control->RegisterComponent();
	TestTrue(TEXT("Valid cache returns the same placement"), Control->FindPlacement() == Fixture.Root->Placement && Control->FindPlacement() == Fixture.Root->Placement);
	Fixture.Root->Loader->LoadVehicle(Fixture.Profile);
	TestTrue(TEXT("Reload preserves the root placement"), Control->FindPlacement() == Fixture.Root->Placement);
	Fixture.Root->Destroy();
	Fixture.Root = Fixture.World->SpawnActor<ACXMRVehicleRoot>(); Fixture.Root->Loader->LoadVehicle(Fixture.Profile);
	TestTrue(TEXT("Destroyed placement is replaced on the next lookup"), Control->FindPlacement() == Fixture.Root->Placement);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRLoadRejectionReasonTest, "CXMR.Alignment.OctReview.LoadRejectionReasons",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRLoadRejectionReasonTest::RunTest(const FString&)
{
	FEyeFixture Fixture; ConfigureEye(*this, Fixture.Profile); AlignEye(*this, Fixture.Root->Placement);
	UCXMRPlacementComponent* Placement = Fixture.Root->Placement;
	SaveGroupPose(*this, Fixture, FTransform(FVector(100, 0, 0)), FTransform(FVector(200, 0, 0)));
	const FString Path = Placement->GetCalibrationFilePath(); FString Original; FFileHelper::LoadFileToString(Original, *Path);
	for (const TCHAR* Key : {TEXT("vehicleProfile"), TEXT("vehicleOffset"), TEXT("vehicleClass")})
	{
		TSharedPtr<FJsonObject> Data; FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Original), Data);
		Data->SetStringField(Key, TEXT("changed")); FString Modified; FJsonSerializer::Serialize(Data.ToSharedRef(), TJsonWriterFactory<>::Create(&Modified)); FFileHelper::SaveStringToFile(Modified, *Path);
		TestFalse(TEXT("Changed model metadata is rejected"), Placement->LoadCalibrationFromDisk());
		TestTrue(TEXT("Model rejection explains the reason"), Placement->GetAlignmentStorageMessage().ToString().Contains(TEXT("Model settings changed")));
	}
	FFileHelper::SaveStringToFile(Original, *Path);
	FCXMRMarkerEntry Added; Added.MarkerId = 7; Placement->MarkerProfile->Markers.Add(Added);
	TestFalse(TEXT("Added required marker rejects the saved layout"), Placement->LoadCalibrationFromDisk());
	TestTrue(TEXT("Marker mismatch names the added ID"), Placement->GetAlignmentStorageMessage().ToString().Contains(TEXT("7")));
	Placement->MarkerProfile->Markers.Pop(); FFileHelper::SaveStringToFile(TEXT("broken JSON"), *Path);
	TestFalse(TEXT("Damaged JSON is rejected"), Placement->LoadCalibrationFromDisk());
	TestTrue(TEXT("Corruption explains the reason"), Placement->GetAlignmentStorageMessage().ToString().Contains(TEXT("damaged")));
	FFileHelper::SaveStringToFile(Original, *Path);
	TestTrue(TEXT("Normal file loads"), Placement->LoadCalibrationFromDisk());
	TestTrue(TEXT("Normal load clears rejection message"), Placement->GetAlignmentStorageMessage().IsEmpty());
	TestNotNull(TEXT("Marker dump command is registered"), IConsoleManager::Get().FindConsoleObject(TEXT("CXMR.DumpMarkers")));
	return true;
}
#endif
