// Copyright GMTCK CX.

#include "CXMRAlignmentBoxComponent.h"
#include "CXMRHandTracking.h"
#include "CXMRLevelFit.h"
#include "CXMRPlacementComponent.h"
#include "CXMRTuningSubsystem.h"
#include "CXMRVehicleRoot.h"

#include "Components/TextRenderComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "HeadMountedDisplayTypes.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogCXMRAlignmentBox, Log, All);

namespace
{
	// Named per file: unity builds put anonymous namespaces of several files into one translation unit.
	const int32 BoxIndexTipKeypoint = static_cast<int32>(EHandKeypoint::IndexTip);
	const int32 BoxThumbTipKeypoint = static_cast<int32>(EHandKeypoint::ThumbTip);

	/** Thumb-to-index gap that starts a pinch, and the wider gap that ends it (cm). */
	const float BoxPinchClose = 1.5f;
	const float BoxPinchOpen = 2.5f;

	/** Pinch records only while this is on — an ordinary pinch (grabbing something) must never move the car. */
	bool GBoxTouchMode = false;

	void ForEachAlignmentBox(UWorld* World, TFunctionRef<void(UCXMRAlignmentBoxComponent&)> Action)
	{
		for (TObjectIterator<UCXMRAlignmentBoxComponent> It; It; ++It)
		{
			if (World && It->GetWorld() == World)
			{
				Action(**It);
			}
		}
	}
}

static FAutoConsoleCommandWithWorld GCXMRBoxTouch(
	TEXT("CXMR.BoxTouch"),
	TEXT("Record the next alignment box corner at the touch hand's index fingertip (turns touch mode on)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		GBoxTouchMode = true;
		ForEachAlignmentBox(World, [](UCXMRAlignmentBoxComponent& Box) { Box.RecordTouch(); });
	}));

static FAutoConsoleCommandWithWorld GCXMRBoxTouchClear(
	TEXT("CXMR.BoxTouchClear"),
	TEXT("Forget the touched alignment box corners."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		ForEachAlignmentBox(World, [](UCXMRAlignmentBoxComponent& Box) { Box.ClearTouches(); });
	}));

UCXMRAlignmentBoxComponent::UCXMRAlignmentBoxComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UCXMRAlignmentBoxComponent::BeginPlay()
{
	Super::BeginPlay();
	RegisterTunables();
}

void UCXMRAlignmentBoxComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	for (UTextRenderComponent* Label : Labels)
	{
		if (Label)
		{
			Label->DestroyComponent();
		}
	}
	Labels.Reset();

	const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (UCXMRTuningSubsystem* Tuning = GI ? GI->GetSubsystem<UCXMRTuningSubsystem>() : nullptr)
	{
		Tuning->UnregisterOwner(this);
	}
	GBoxTouchMode = false;
	Super::EndPlay(Reason);
}

FVector UCXMRAlignmentBoxComponent::CornerLocal(ECXMRBoxCorner Corner) const
{
	const double X = BoxSize.X * 0.5;
	const double Y = BoxSize.Y * 0.5;
	switch (Corner)
	{
	case ECXMRBoxCorner::TopFrontLeft:     return FVector( X, -Y, BoxSize.Z);
	case ECXMRBoxCorner::TopFrontRight:    return FVector( X,  Y, BoxSize.Z);
	case ECXMRBoxCorner::TopBackRight:     return FVector(-X,  Y, BoxSize.Z);
	case ECXMRBoxCorner::TopBackLeft:      return FVector(-X, -Y, BoxSize.Z);
	case ECXMRBoxCorner::BottomFrontLeft:  return FVector( X, -Y, 0.0);
	case ECXMRBoxCorner::BottomFrontRight: return FVector( X,  Y, 0.0);
	case ECXMRBoxCorner::BottomBackRight:  return FVector(-X,  Y, 0.0);
	default:                               return FVector(-X, -Y, 0.0);
	}
}

FVector UCXMRAlignmentBoxComponent::GetCornerWorld(ECXMRBoxCorner Corner) const
{
	return GetComponentTransform().TransformPosition(CornerLocal(Corner));
}

UCXMRPlacementComponent* UCXMRAlignmentBoxComponent::FindPlacement() const
{
	// The box sits in the vehicle actor, which the vehicle root spawns under itself.
	for (AActor* Actor = GetOwner(); Actor; Actor = Actor->GetAttachParentActor())
	{
		if (UCXMRPlacementComponent* Placement = Actor->FindComponentByClass<UCXMRPlacementComponent>())
		{
			return Placement;
		}
	}
	for (TObjectIterator<UCXMRPlacementComponent> It; It; ++It)
	{
		if (It->GetWorld() == GetWorld())
		{
			return *It;
		}
	}
	return nullptr;
}

bool UCXMRAlignmentBoxComponent::GetTouchPoint(FVector& OutPoint) const
{
	TArray<FVector> Positions;
	TArray<FQuat> Rotations;
	TArray<float> Radii;
	if (!CXMRHands::GetJoints(GetWorld(), TouchHand, Positions, Rotations, Radii))
	{
		return false;
	}
	OutPoint = Positions[BoxIndexTipKeypoint];
	if (bTouchFromAbove)
	{
		// Some runtimes report no radius; an adult fingertip is about 0.8 cm.
		const float Radius = Radii.IsValidIndex(BoxIndexTipKeypoint) && Radii[BoxIndexTipKeypoint] > 0.1f ? Radii[BoxIndexTipKeypoint] : 0.8f;
		OutPoint.Z -= Radius;
	}
	return true;
}

void UCXMRAlignmentBoxComponent::RecordTouch()
{
	FVector Point;
	if (!GetTouchPoint(Point))
	{
		LastProblem = TEXT("touch hand not tracked - nothing recorded");
		UE_LOG(LogCXMRAlignmentBox, Warning, TEXT("Box touch: the %s hand is not tracked."),
			TouchHand == EControllerHand::Left ? TEXT("left") : TEXT("right"));
		return;
	}
	LastProblem.Reset();
	bSampling = true;
	SampleElapsed = 0.0f;
	Samples.Reset();
	Samples.Add(Point);
}

void UCXMRAlignmentBoxComponent::RecordTouchAt(FVector WorldPoint)
{
	AddTouch(WorldPoint, 0.0f);
}

void UCXMRAlignmentBoxComponent::AddTouch(const FVector& World, float ScatterCm)
{
	if (TouchCorners.Num() < 2)
	{
		LastProblem = TEXT("Touch Corners needs at least two corners");
		return;
	}
	if (Touches.Num() >= TouchCorners.Num())
	{
		Touches.Reset();   // a full set was already used: this touch starts a new round at corner 1
		TouchFitError = -1.0f;
	}
	Touches.Add({ World, ScatterCm });
	UE_LOG(LogCXMRAlignmentBox, Log, TEXT("Box corner %d/%d recorded at %s (scatter %.2f cm)."),
		Touches.Num(), TouchCorners.Num(), *World.ToCompactString(), ScatterCm);

	if (Touches.Num() == TouchCorners.Num())
	{
		ApplyTouches();
	}
}

bool UCXMRAlignmentBoxComponent::ApplyTouches()
{
	UCXMRPlacementComponent* Placement = FindPlacement();
	const AActor* Root = Placement ? (Placement->VehicleRoot ? Placement->VehicleRoot.Get() : Placement->GetOwner()) : nullptr;
	if (!Root)
	{
		LastProblem = TEXT("no vehicle placement found");
		return false;
	}
	if (Touches.Num() < 2)
	{
		LastProblem = TEXT("touch at least two corners first");
		return false;
	}

	// Corners in the vehicle root's frame: what the fit maps onto the touched points is the root's world pose.
	const FTransform RootWorld = Root->GetActorTransform();
	TArray<FVector> Local, Measured;
	for (int32 i = 0; i < Touches.Num() && i < TouchCorners.Num(); ++i)
	{
		Local.Add(RootWorld.InverseTransformPosition(GetCornerWorld(TouchCorners[i])));
		Measured.Add(Touches[i].World);
	}

	FTransform NewRoot;
	float Rms = 0.0f;
	if (!CXMRLevelFit::Solve(Local, Measured, NewRoot, Rms, 5.0))
	{
		LastProblem = TEXT("the touched corners are too close together to give a heading - pick corners far apart");
		return false;
	}

	Placement->MoveVehicleTo(NewRoot);
	TouchFitError = Rms;
	LastProblem.Reset();
	GBoxTouchMode = false;   // done: an ordinary pinch must not start moving the car again
	UE_LOG(LogCXMRAlignmentBox, Log, TEXT("Vehicle aligned to %d touched box corners, %.2f cm RMS. Learn the marker layout to keep it."),
		Touches.Num(), Rms);
	return true;
}

void UCXMRAlignmentBoxComponent::ClearTouches()
{
	Touches.Reset();
	bSampling = false;
	Samples.Reset();
	TouchFitError = -1.0f;
	LastProblem.Reset();
}

void UCXMRAlignmentBoxComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdatePinchTrigger();

	if (bSampling)
	{
		FVector Point;
		if (!GetTouchPoint(Point))
		{
			bSampling = false;
			LastProblem = TEXT("hand lost while recording - touch again");
		}
		else
		{
			Samples.Add(Point);
			SampleElapsed += DeltaTime;
			if (SampleElapsed >= TouchSampleSeconds)
			{
				bSampling = false;
				FVector Mean = FVector::ZeroVector;
				for (const FVector& S : Samples) { Mean += S; }
				Mean /= Samples.Num();
				double Squared = 0.0;
				for (const FVector& S : Samples) { Squared += FVector::DistSquared(S, Mean); }
				AddTouch(Mean, static_cast<float>(FMath::Sqrt(Squared / Samples.Num())));
			}
		}
	}

	DrawOutline();
	UpdateLabels();
}

void UCXMRAlignmentBoxComponent::UpdatePinchTrigger()
{
	const EControllerHand Other = TouchHand == EControllerHand::Left ? EControllerHand::Right : EControllerHand::Left;
	TArray<FVector> Positions;
	TArray<FQuat> Rotations;
	TArray<float> Radii;
	bool bPinch = false;
	if (CXMRHands::GetJoints(GetWorld(), Other, Positions, Rotations, Radii))
	{
		const float Gap = static_cast<float>(FVector::Dist(Positions[BoxThumbTipKeypoint], Positions[BoxIndexTipKeypoint]));
		bPinch = bOtherHandPinching ? Gap < BoxPinchOpen : Gap <= BoxPinchClose;
	}
	const bool bStarted = bPinch && !bOtherHandPinching;
	bOtherHandPinching = bPinch;

	if (bStarted && GBoxTouchMode && !bSampling)
	{
		RecordTouch();
	}
}

void UCXMRAlignmentBoxComponent::DrawOutline()
{
	UWorld* World = GetWorld();
	if (!World || !bShowOutline)
	{
		return;
	}
	const FTransform& T = GetComponentTransform();
	const FVector Extent = BoxSize * 0.5 * T.GetScale3D();
	DrawDebugBox(World, T.TransformPosition(FVector(0.0, 0.0, BoxSize.Z * 0.5)), Extent, T.GetRotation(),
		FColor(90, 220, 255), false, -1.0f, SDPG_World, OutlineThickness);

	if (!GBoxTouchMode && Touches.Num() == 0)
	{
		return;
	}
	for (int32 i = 0; i < TouchCorners.Num(); ++i)
	{
		const FColor Color = i < Touches.Num() ? FColor::Green : (i == Touches.Num() ? FColor::Yellow : FColor::White);
		DrawDebugSphere(World, GetCornerWorld(TouchCorners[i]), i == Touches.Num() ? 1.2f : 0.7f, 10, Color, false, -1.0f, SDPG_World, 0.1f);
	}
	for (const FTouch& Touch : Touches)
	{
		DrawDebugPoint(World, Touch.World, 8.0f, FColor::Magenta, false, -1.0f, SDPG_World);
	}
}

void UCXMRAlignmentBoxComponent::UpdateLabels()
{
	const bool bShow = bShowOutline && (GBoxTouchMode || Touches.Num() > 0);
	AActor* Owner = GetOwner();
	while (bShow && Owner && Labels.Num() < TouchCorners.Num())
	{
		UTextRenderComponent* Label = NewObject<UTextRenderComponent>(Owner);
		Label->SetupAttachment(this);
		Label->SetUsingAbsoluteLocation(true);
		Label->SetUsingAbsoluteRotation(true);
		Label->SetUsingAbsoluteScale(true);
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetVerticalAlignment(EVRTA_TextBottom);
		Label->SetWorldSize(3.0f);
		Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Label->SetCastShadow(false);
		Label->RegisterComponent();
		Labels.Add(Label);
	}

	FTransform Head;
	const bool bHaveHead = CXMRHands::GetHeadTransform(GetWorld(), Head);
	for (int32 i = 0; i < Labels.Num(); ++i)
	{
		UTextRenderComponent* Label = Labels[i];
		if (!Label)
		{
			continue;
		}
		if (!bShow || i >= TouchCorners.Num())
		{
			Label->SetVisibility(false);
			continue;
		}
		const FVector Location = GetCornerWorld(TouchCorners[i]) + FVector(0.0, 0.0, 2.0);
		Label->SetText(FText::AsNumber(i + 1));
		Label->SetTextRenderColor(i < Touches.Num() ? FColor::Green : (i == Touches.Num() ? FColor::Yellow : FColor::White));
		Label->SetWorldLocation(Location);
		if (bHaveHead)
		{
			Label->SetWorldRotation((Head.GetLocation() - Location).Rotation());
		}
		Label->SetVisibility(true);
	}
}

FText UCXMRAlignmentBoxComponent::StatusText() const
{
	FString Text;
	const bool bRight = TouchHand != EControllerHand::Left;
	if (bSampling)
	{
		Text = TEXT("recording - hold the fingertip still");
	}
	else if (TouchFitError >= 0.0f && Touches.Num() >= 2)
	{
		Text = FString::Printf(TEXT("aligned to %d corners, %.2f cm apart - Learn to keep it"), Touches.Num(), TouchFitError);
		if (TouchFitError > 2.0f)
		{
			// Touches that far from a rigid box mean a wrong corner, a wrong box size, or a hand that moved.
			Text += TEXT(" | touches disagree with the box: check Box Size and which corner was touched");
		}
	}
	else if (!GBoxTouchMode)
	{
		Text = TEXT("touch mode off");
	}
	else
	{
		Text = FString::Printf(TEXT("touch corner %d of %d with the %s index, pinch the %s hand"),
			Touches.Num() + 1, TouchCorners.Num(), bRight ? TEXT("right") : TEXT("left"), bRight ? TEXT("left") : TEXT("right"));
	}
	if (!LastProblem.IsEmpty())
	{
		Text += TEXT(" | ") + LastProblem;
	}
	return FText::FromString(Text);
}

FText UCXMRAlignmentBoxComponent::NearestCornerText() const
{
	FVector Point;
	if (!GetTouchPoint(Point))
	{
		return NSLOCTEXT("CXMRAlignmentBox", "NoHand", "touch hand not tracked");
	}
	double Best = TNumericLimits<double>::Max();
	int32 BestCorner = 0;
	for (int32 c = 0; c <= static_cast<int32>(ECXMRBoxCorner::BottomBackLeft); ++c)
	{
		const double D = FVector::Dist(Point, GetCornerWorld(static_cast<ECXMRBoxCorner>(c)));
		if (D < Best)
		{
			Best = D;
			BestCorner = c;
		}
	}
	const UEnum* Enum = StaticEnum<ECXMRBoxCorner>();
	return FText::FromString(FString::Printf(TEXT("%.1f cm from %s"), Best,
		Enum ? *Enum->GetDisplayNameTextByValue(BestCorner).ToString() : TEXT("corner")));
}

void UCXMRAlignmentBoxComponent::RegisterTunables()
{
	const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UCXMRTuningSubsystem* Tuning = GI ? GI->GetSubsystem<UCXMRTuningSubsystem>() : nullptr;
	if (!Tuning)
	{
		return;
	}

	const FText Category = NSLOCTEXT("CXMRAlignmentBox", "Cat", "Alignment box");
	auto Make = [this, &Category](FName Id, const FText& Label, ECXMRTunableKind Kind)
	{
		FCXMRTunable Tunable;
		Tunable.Id = Id;
		Tunable.Category = Category;
		Tunable.Label = Label;
		Tunable.Kind = Kind;
		Tunable.Owner = this;
		return Tunable;
	};

	{
		FCXMRTunable T = Make("Box.Status", NSLOCTEXT("CXMRAlignmentBox", "Status", "Status"), ECXMRTunableKind::Readout);
		T.Text = [this] { return StatusText(); };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Box.Nearest", NSLOCTEXT("CXMRAlignmentBox", "Nearest", "Fingertip to nearest corner"), ECXMRTunableKind::Readout);
		T.Text = [this] { return NearestCornerText(); };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Box.Outline", NSLOCTEXT("CXMRAlignmentBox", "Outline", "Show box outline"), ECXMRTunableKind::Bool);
		T.Default = bShowOutline ? 1.0f : 0.0f;
		T.bPersist = true;
		T.Get = [this] { return bShowOutline ? 1.0f : 0.0f; };
		T.Set = [this](float V) { bShowOutline = V > 0.5f; };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Box.TouchMode", NSLOCTEXT("CXMRAlignmentBox", "TouchMode", "Touch mode (pinch records a corner)"), ECXMRTunableKind::Bool);
		T.Get = [] { return GBoxTouchMode ? 1.0f : 0.0f; };
		T.Set = [this](float V)
		{
			GBoxTouchMode = V > 0.5f;
			if (GBoxTouchMode && TouchFitError >= 0.0f)
			{
				ClearTouches();   // a new round after a finished one
			}
		};
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Box.TouchHand", NSLOCTEXT("CXMRAlignmentBox", "TouchHand", "Touching hand"), ECXMRTunableKind::Choice);
		T.Options = { NSLOCTEXT("CXMRAlignmentBox", "Right", "Right"), NSLOCTEXT("CXMRAlignmentBox", "Left", "Left") };
		T.bPersist = true;
		T.Get = [this] { return TouchHand == EControllerHand::Left ? 1.0f : 0.0f; };
		T.Set = [this](float V) { TouchHand = V > 0.5f ? EControllerHand::Left : EControllerHand::Right; };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Box.Record", NSLOCTEXT("CXMRAlignmentBox", "Record", "Record corner now"), ECXMRTunableKind::Action);
		T.Invoke = [this] { GBoxTouchMode = true; RecordTouch(); };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Box.Apply", NSLOCTEXT("CXMRAlignmentBox", "Apply", "Align to the corners touched so far"), ECXMRTunableKind::Action);
		T.Invoke = [this] { ApplyTouches(); };
		T.IsEnabled = [this] { return Touches.Num() >= 2; };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Box.Clear", NSLOCTEXT("CXMRAlignmentBox", "Clear", "Clear touches"), ECXMRTunableKind::Action);
		T.Invoke = [this] { ClearTouches(); };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Box.Learn", NSLOCTEXT("CXMRAlignmentBox", "Learn", "Learn marker layout from this pose (and save)"), ECXMRTunableKind::Action);
		T.Invoke = [this] { if (UCXMRPlacementComponent* P = FindPlacement()) { P->LearnMarkerLayout(); } };
		Tuning->Register(MoveTemp(T));
	}
}
