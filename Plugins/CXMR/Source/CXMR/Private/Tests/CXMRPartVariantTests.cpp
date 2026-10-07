#include "Tests/CXMRPartVariantTestTypes.h"
#include "CXMRUsbPortTarget.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ChildActorComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"

ACXMRPartVariantTestAssembly::ACXMRPartVariantTestAssembly()
{
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMeshComponent* First = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FirstMesh"));
	First->SetupAttachment(GetRootComponent());
	First->SetStaticMesh(Cube);
	UStaticMeshComponent* Second = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SecondMesh"));
	Second->SetupAttachment(GetRootComponent());
	Second->SetStaticMesh(Cube);
	Second->SetRelativeLocation(FVector(4, 7, 11));
	UChildActorComponent* Port = CreateDefaultSubobject<UChildActorComponent>(TEXT("UsbPort"));
	Port->SetupAttachment(GetRootComponent());
	Port->SetRelativeLocation(FVector(8, 2, 1));
	Port->SetChildActorClass(ACXMRUsbPortTarget::StaticClass());
}

void ACXMRPartForeignTestAssembly::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TEXT("CXMRPartForeignFixture")))
		{
			It->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
		}
	}
}

void ACXMRPartUnattachedTestAssembly::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	FActorSpawnParameters Params;
	Params.Owner = this;
	ACXMRUsbPortTarget* Port =
		GetWorld()->SpawnActor<ACXMRUsbPortTarget>(GetActorLocation(), GetActorRotation(), Params);
	Port->Tags.Add(TEXT("CXMRPartUnattachedFixture"));
}
ACXMRPartAuthoredActivityTestAssembly::ACXMRPartAuthoredActivityTestAssembly()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}
void ACXMRPartAuthoredActivityTestAssembly::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	SetActorTickEnabled(true);
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CXMRDesignOption.h"
#include "CXMRPartSlotComponent.h"
#include "CXMRPartVariantComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"
#include "UObject/Class.h"
#include <initializer_list>

namespace
{
	struct FPartWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		AActor* Anchor = nullptr;
		AActor* Vehicle = nullptr;
		UCXMRPartVariantComponent* Controller = nullptr;
		UCXMRPartSlotComponent* Console = nullptr;
		UCXMRPartSlotComponent* Door = nullptr;
		FPartWorld()
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			Anchor = World->SpawnActor<AActor>();
			USceneComponent* AnchorRoot = NewObject<USceneComponent>(Anchor);
			Anchor->SetRootComponent(AnchorRoot);
			AnchorRoot->RegisterComponent();
			Anchor->SetActorTransform(FTransform(FRotator(5, 28, 2), FVector(220, -35, 64)));
			Vehicle = World->SpawnActor<AActor>();
			USceneComponent* Root = NewObject<USceneComponent>(Vehicle);
			Vehicle->SetRootComponent(Root);
			Root->RegisterComponent();
			Vehicle->AttachToComponent(AnchorRoot, FAttachmentTransformRules::KeepRelativeTransform);
			Vehicle->SetActorRelativeTransform(FTransform(FRotator(2, 13, 0), FVector(31, 12, -7)));
			Console = AddSlot(TEXT("Console"), FVector(21, 3, 9));
			Door = AddSlot(TEXT("Door"), FVector(7, 41, 13));
			Controller = NewObject<UCXMRPartVariantComponent>(Vehicle);
			Vehicle->AddInstanceComponent(Controller);
			Controller->RegisterComponent();
		}
		UCXMRPartSlotComponent* AddSlot(FName Id, FVector Location)
		{
			UCXMRPartSlotComponent* Slot = NewObject<UCXMRPartSlotComponent>(Vehicle);
			Slot->SlotId = Id;
			Vehicle->AddInstanceComponent(Slot);
			Slot->SetupAttachment(Vehicle->GetRootComponent());
			Slot->SetRelativeLocation(Location);
			Slot->RegisterComponent();
			return Slot;
		}
		UCXMRDesignOption* Option(FName Id, FName SlotId, FVector Location)
		{
			UCXMRDesignOption* Result = NewObject<UCXMRDesignOption>(Vehicle);
			Result->OptionId = Id;
			Result->SlotId = SlotId;
			Result->AssemblyActor = ACXMRPartVariantTestAssembly::StaticClass();
			Result->SlotLocalOffset = FTransform(FRotator(0, 12, 0), Location);
			return Result;
		}
		void Catalog(std::initializer_list<UCXMRDesignOption*> Entries)
		{
			TArray<TSoftObjectPtr<UCXMRDesignOption>> Options;
			for (UCXMRDesignOption* Entry : Entries)
			{
				Options.Add(Entry);
			}
			Controller->InitializeOptions(Options);
		}
		~FPartWorld()
		{
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}
	};
	ACXMRUsbPortTarget* FindPort(AActor* Assembly)
	{
		if (!IsValid(Assembly))
		{
			return nullptr;
		}
		TArray<AActor*> Children;
		Assembly->GetAttachedActors(Children, true, true);
		for (AActor* Child : Children)
		{
			if (ACXMRUsbPortTarget* Port = Cast<ACXMRUsbPortTarget>(Child))
			{
				return Port;
			}
		}
		return nullptr;
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRPartContractTest, "CXMR.Parts.RuntimeContract",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRPartContractTest::RunTest(const FString&)
{
	FPartWorld F;
	TestFalse(TEXT("Null option rejected"), F.Controller->SelectOption(nullptr));
	for (const TCHAR* Name : {TEXT("SelectOption"), TEXT("GetActiveAssembly"), TEXT("GetActiveOption"),
							  TEXT("ClearSlot"), TEXT("RemoveSlot")})
	{
		TestNotNull(Name, UCXMRPartVariantComponent::StaticClass()->FindFunctionByName(Name));
	}
	TestFalse(TEXT("No per-frame controller tick"), F.Controller->PrimaryComponentTick.bCanEverTick);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRPartStagingTest, "CXMR.Parts.InactiveStaging",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRPartStagingTest::RunTest(const FString&)
{
	FPartWorld F;
	ACXMRPartAssembly* Candidate = F.World->SpawnActor<ACXMRPartVariantTestAssembly>();
	FString Error;
	TestTrue(TEXT("Real model hierarchy validates"), Candidate->PrepareForReview(Error));
	TestFalse(TEXT("Staged candidate inactive"), Candidate->IsAssemblyActive());
	TestTrue(TEXT("Staged candidate invisible"), Candidate->IsHidden());
	TestFalse(TEXT("Staged candidate collision disabled"), Candidate->GetActorEnableCollision());
	ACXMRUsbPortTarget* Port = FindPort(Candidate);
	TestNotNull(TEXT("Staged USB child exists"), Port);
	if (Port)
	{
		TestTrue(TEXT("Staged USB invisible"), Port->IsHidden());
		TestFalse(TEXT("Staged USB collision disabled"), Port->GetActorEnableCollision());
		TestFalse(TEXT("Staged USB contact disabled"), Port->IsContactEnabled());
		TestFalse(TEXT("Staged USB tick disabled"), Port->IsActorTickEnabled());
	}
	Candidate->SetAssemblyActive(true);
	TestTrue(TEXT("Committed candidate active"), Candidate->IsAssemblyActive());
	TestFalse(TEXT("Committed candidate visible"), Candidate->IsHidden());
	if (Port)
	{
		TestTrue(TEXT("Committed USB contact enabled"), Port->IsContactEnabled());
	}
	Candidate->DestroyOwnedChildren();
	Candidate->Destroy();
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRPartSwapTest, "CXMR.Parts.OffsetAndIndependentSlots",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRPartSwapTest::RunTest(const FString&)
{
	FPartWorld F;
	UCXMRDesignOption* A = F.Option(TEXT("ConsoleA"), TEXT("Console"), FVector(2, 5, 1));
	UCXMRDesignOption* B = F.Option(TEXT("ConsoleB"), TEXT("Console"), FVector(11, -4, 3));
	UCXMRDesignOption* Door = F.Option(TEXT("DoorA"), TEXT("Door"), FVector(-2, 3, 8));
	F.Catalog({A, B, Door});
	const FTransform AnchorBefore = F.Anchor->GetActorTransform(), VehicleBefore = F.Vehicle->GetActorTransform(),
					 SlotBefore = F.Console->GetComponentTransform();
	TestTrue(TEXT("A selected"), F.Controller->SelectOption(A));
	AActor* Old = F.Controller->GetActiveAssembly(TEXT("Console"));
	if (!Old)
	{
		return false;
	}
	ACXMRUsbPortTarget* OldPort = FindPort(Old);
	TestNotNull(TEXT("USB child spawned"), OldPort);
	TestTrue(TEXT("A uses offset after CAD and slot transforms"),
			 Old->GetActorTransform().Equals(A->SlotLocalOffset * SlotBefore, 0.01f));
	TArray<UStaticMeshComponent*> Meshes;
	Old->GetComponents(Meshes);
	TestEqual(TEXT("Assembly contains both model meshes"), Meshes.Num(), 2);
	TestTrue(TEXT("Independent door selected"), F.Controller->SelectOption(Door));
	AActor* DoorActor = F.Controller->GetActiveAssembly(TEXT("Door"));
	TestTrue(TEXT("B replaces A"), F.Controller->SelectOption(B));
	AActor* New = F.Controller->GetActiveAssembly(TEXT("Console"));
	TestTrue(TEXT("B uses its own slot-local offset"),
			 New && New->GetActorTransform().Equals(B->SlotLocalOffset * SlotBefore, 0.01f));
	TestTrue(TEXT("Old assembly destroyed"), Old->IsActorBeingDestroyed());
	if (OldPort)
	{
		TestTrue(TEXT("Old USB hidden before destruction"), OldPort->IsHidden());
		TestFalse(TEXT("Old USB tick disabled"), OldPort->IsActorTickEnabled());
		TestFalse(TEXT("Old USB excluded from contact arbitration"), OldPort->IsContactEnabled());
		TestEqual(TEXT("Old USB feedback reset"), OldPort->GetPortState(), ECXMRPortState::Idle);
		TestTrue(TEXT("Old USB destroyed"), OldPort->IsActorBeingDestroyed());
	}
	TestEqual(TEXT("Door unaffected"), F.Controller->GetActiveAssembly(TEXT("Door")), DoorActor);
	TestEqual(TEXT("Active option B"), F.Controller->GetActiveOption(TEXT("Console")), B);
	TestTrue(TEXT("Anchor unchanged"), F.Anchor->GetActorTransform().Equals(AnchorBefore));
	TestTrue(TEXT("CAD offset unchanged"), F.Vehicle->GetActorTransform().Equals(VehicleBefore));
	TestTrue(TEXT("Fixed mount unchanged"), F.Console->GetComponentTransform().Equals(SlotBefore));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRPartFailureTest, "CXMR.Parts.FailurePreservesAccepted",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRPartFailureTest::RunTest(const FString&)
{
	FPartWorld F;
	UCXMRDesignOption* A = F.Option(TEXT("A"), TEXT("Console"), FVector(3, 2, 4));
	UCXMRDesignOption* B = F.Option(TEXT("B"), TEXT("Console"), FVector(6, 7, 8));
	F.Catalog({A, B});
	TestTrue(TEXT("Accepted option selected"), F.Controller->SelectOption(A));
	AActor* Accepted = F.Controller->GetActiveAssembly(TEXT("Console"));
	auto Preserved = [&]() {
		TestEqual(TEXT("Failed change retains assembly"), F.Controller->GetActiveAssembly(TEXT("Console")), Accepted);
		TestEqual(TEXT("Failed change retains option"), F.Controller->GetActiveOption(TEXT("Console")), A);
		TestFalse(TEXT("Explicit rejection reason"), F.Controller->LastError.IsEmpty());
	};
	B->OptionId = NAME_None;
	TestFalse(TEXT("Empty ID rejected"), F.Controller->SelectOption(B));
	Preserved();
	B->OptionId = A->OptionId;
	TestFalse(TEXT("Duplicate option IDs rejected"), F.Controller->SelectOption(B));
	Preserved();
	B->OptionId = TEXT("B");
	B->SlotId = TEXT("Absent");
	TestFalse(TEXT("Absent slot rejected"), F.Controller->SelectOption(B));
	Preserved();
	B->SlotId = TEXT("Console");
	UCXMRPartSlotComponent* Duplicate = F.AddSlot(TEXT("Console"), FVector::ZeroVector);
	TestFalse(TEXT("Ambiguous slot rejected"), F.Controller->SelectOption(B));
	Preserved();
	Duplicate->DestroyComponent();
	B->AssemblyActor = TSoftClassPtr<ACXMRPartAssembly>(FSoftObjectPath(TEXT("/Game/MissingPart.MissingPart_C")));
	AddExpectedError(TEXT("Failed to find object"), EAutomationExpectedErrorFlags::Contains, 0);
	TestFalse(TEXT("Unavailable assembly class rejected"), F.Controller->SelectOption(B));
	Preserved();
	B->AssemblyActor = ACXMRPartAssembly::StaticClass();
	TestFalse(TEXT("Empty model rejected after staging"), F.Controller->SelectOption(B));
	Preserved();
	B->AssemblyActor = ACXMRPartVariantTestAssembly::StaticClass();
	B->SlotLocalOffset.SetRotation(FQuat(0, 0, 0, 0));
	TestFalse(TEXT("Unnormalized rotation rejected"), F.Controller->SelectOption(B));
	Preserved();
	B->SlotLocalOffset = FTransform::Identity;
	B->SlotLocalOffset.SetScale3D(FVector(1, 0, 1));
	TestFalse(TEXT("Collapsed scale rejected"), F.Controller->SelectOption(B));
	Preserved();
	B->SlotLocalOffset = FTransform(FVector(3, 2, 1));
	UCXMRDesignOption* Foreign = F.Option(TEXT("Foreign"), TEXT("Console"), FVector::ZeroVector);
	TestFalse(TEXT("Foreign option rejected"), F.Controller->SelectOption(Foreign));
	Preserved();
	AActor* ForeignActor = F.World->SpawnActor<AActor>();
	USceneComponent* ForeignRoot = NewObject<USceneComponent>(ForeignActor);
	ForeignActor->SetRootComponent(ForeignRoot);
	ForeignRoot->RegisterComponent();
	ForeignActor->Tags.Add(TEXT("CXMRPartForeignFixture"));
	B->AssemblyActor = ACXMRPartForeignTestAssembly::StaticClass();
	TestFalse(TEXT("Foreign actor attachment rejected"), F.Controller->SelectOption(B));
	Preserved();
	TestFalse(TEXT("Foreign actor survives rejected candidate"), ForeignActor->IsActorBeingDestroyed());
	TestFalse(TEXT("Foreign actor visibility unchanged"), ForeignActor->IsHidden());
	TestTrue(TEXT("Foreign actor collision unchanged"), ForeignActor->GetActorEnableCollision());
	B->AssemblyActor = ACXMRPartVariantTestAssembly::StaticClass();
	TestTrue(TEXT("Valid option still usable after failures"), F.Controller->SelectOption(B));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRPartUnattachedTest, "CXMR.Parts.UnattachedOwnedContact",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRPartUnattachedTest::RunTest(const FString&)
{
	FPartWorld F;
	UCXMRDesignOption* Option = F.Option(TEXT("Unattached"), TEXT("Console"), FVector::ZeroVector);
	Option->AssemblyActor = ACXMRPartUnattachedTestAssembly::StaticClass();
	F.Catalog({Option});
	ACXMRPartAssembly* Candidate = F.World->SpawnActor<ACXMRPartUnattachedTestAssembly>();
	ACXMRUsbPortTarget* Unattached = nullptr;
	for (TActorIterator<ACXMRUsbPortTarget> It(F.World); It; ++It)
	{
		if (It->GetOwner() == Candidate && It->ActorHasTag(TEXT("CXMRPartUnattachedFixture")))
		{
			Unattached = *It;
		}
	}
	if (!TestNotNull(TEXT("Construction created unattached owned USB"), Unattached))
	{
		return false;
	}
	FString Error;
	TestTrue(TEXT("Unattached owned hierarchy validates"), Candidate->PrepareForReview(Error));
	TestNull(TEXT("USB remains unattached"), Unattached->GetAttachParentActor());
	TestTrue(TEXT("Unattached staged USB hidden"), Unattached->IsHidden());
	TestFalse(TEXT("Unattached staged USB contact disabled"), Unattached->IsContactEnabled());
	TestFalse(TEXT("Unattached staged USB collision disabled"), Unattached->GetActorEnableCollision());
	Candidate->DestroyOwnedChildren();
	Candidate->Destroy();
	TestTrue(TEXT("Unattached staged child destroyed"), Unattached->IsActorBeingDestroyed());
	TestTrue(TEXT("Option selected"), F.Controller->SelectOption(Option));
	AActor* Accepted = F.Controller->GetActiveAssembly(TEXT("Console"));
	Unattached = nullptr;
	for (TActorIterator<ACXMRUsbPortTarget> It(F.World); It; ++It)
	{
		if (It->GetOwner() == Accepted && It->ActorHasTag(TEXT("CXMRPartUnattachedFixture")))
		{
			Unattached = *It;
		}
	}
	if (!TestNotNull(TEXT("Accepted unattached USB exists"), Unattached))
	{
		return false;
	}
	TestTrue(TEXT("Slot removed"), F.Controller->RemoveSlot(TEXT("Console")));
	TestTrue(TEXT("Removal destroys unattached USB"), Unattached->IsActorBeingDestroyed());
	TestFalse(TEXT("Removal disables unattached USB contact"), Unattached->IsContactEnabled());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRPartAuthoredActivityTest, "CXMR.Parts.AuthoredActivityPreserved",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRPartAuthoredActivityTest::RunTest(const FString&)
{
	FPartWorld F;
	UCXMRDesignOption* Option = F.Option(TEXT("Authored"), TEXT("Console"), FVector::ZeroVector);
	Option->AssemblyActor = ACXMRPartAuthoredActivityTestAssembly::StaticClass();
	F.Catalog({Option});
	TestTrue(TEXT("Authored assembly selected"), F.Controller->SelectOption(Option));
	AActor* Accepted = F.Controller->GetActiveAssembly(TEXT("Console"));
	if (!TestNotNull(TEXT("Authored assembly exists"), Accepted))
	{
		return false;
	}
	TestTrue(TEXT("Construction-authored hidden flag preserved"), Accepted->IsHidden());
	TestFalse(TEXT("Construction-authored collision flag preserved"), Accepted->GetActorEnableCollision());
	TestTrue(TEXT("Construction-authored actor tick restored"), Accepted->IsActorTickEnabled());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRPartLifecycleTest, "CXMR.Parts.ContactAndOwnerCleanup",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRPartLifecycleTest::RunTest(const FString&)
{
	FPartWorld F;
	UCXMRDesignOption* A = F.Option(TEXT("A"), TEXT("Console"), FVector::ZeroVector);
	UCXMRDesignOption* B = F.Option(TEXT("B"), TEXT("Door"), FVector::ZeroVector);
	F.Catalog({A, B});
	TestTrue(TEXT("Console selected"), F.Controller->SelectOption(A));
	TestTrue(TEXT("Door selected"), F.Controller->SelectOption(B));
	AActor* ConsoleActor = F.Controller->GetActiveAssembly(TEXT("Console"));
	ACXMRUsbPortTarget* ConsolePort = FindPort(ConsoleActor);
	if (!TestNotNull(TEXT("Selected console assembly exists"), ConsoleActor))
	{
		return false;
	}
	TestTrue(TEXT("Remove slot succeeds"), F.Controller->RemoveSlot(TEXT("Console")));
	TestNull(TEXT("Removed assembly absent"), F.Controller->GetActiveAssembly(TEXT("Console")));
	TestNull(TEXT("Removed option absent"), F.Controller->GetActiveOption(TEXT("Console")));
	TestFalse(TEXT("Retired slot rejects selection"), F.Controller->SelectOption(A));
	TestTrue(TEXT("Authored mount retained"), IsValid(F.Console) && F.Console->IsRegistered());
	if (ConsolePort)
	{
		TestTrue(TEXT("Removed USB destroyed"), ConsolePort->IsActorBeingDestroyed());
		TestTrue(TEXT("Removed USB hidden"), ConsolePort->IsHidden());
		TestFalse(TEXT("Removed USB cannot tick"), ConsolePort->IsActorTickEnabled());
		TestFalse(TEXT("Removed USB cannot participate in contact"), ConsolePort->IsContactEnabled());
	}
	AActor* DoorActor = F.Controller->GetActiveAssembly(TEXT("Door"));
	ACXMRUsbPortTarget* DoorPort = FindPort(DoorActor);
	if (!TestNotNull(TEXT("Selected door assembly exists"), DoorActor))
	{
		return false;
	}
	F.Vehicle->Destroy();
	TestTrue(TEXT("Vehicle destruction destroys part"), DoorActor->IsActorBeingDestroyed());
	if (DoorPort)
	{
		TestTrue(TEXT("Vehicle destruction destroys USB"), DoorPort->IsActorBeingDestroyed());
	}
	return true;
}
#endif
