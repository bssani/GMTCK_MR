// Copyright GMTCK CX.

#include "CXMRPlacementComponent.h"
#include "CXMRSubsystem.h"
#include "CXMRMarkerProfile.h"

#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Pawn.h"

UCXMRPlacementComponent::UCXMRPlacementComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UCXMRSubsystem* UCXMRPlacementComponent::GetCXMR() const
{
	if (const UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			return GI->GetSubsystem<UCXMRSubsystem>();
		}
	}
	return nullptr;
}

AActor* UCXMRPlacementComponent::ResolveVehicleRoot()
{
	return VehicleRoot ? VehicleRoot.Get() : GetOwner();
}

void UCXMRPlacementComponent::BeginPlay()
{
	Super::BeginPlay();

	Subsystem = GetCXMR();
	if (Subsystem)
	{
		// Subscribe to the subsystem (marker events + placement requests), never the plugin directly.
		Subsystem->OnMarkerDetected.AddDynamic(this, &UCXMRPlacementComponent::HandleMarkerDetected);
		Subsystem->OnRecalibrateRequested.AddDynamic(this, &UCXMRPlacementComponent::HandleRecalibrateRequest);
		Subsystem->OnPlaceRequested.AddDynamic(this, &UCXMRPlacementComponent::HandlePlaceRequest);
	}
}

void UCXMRPlacementComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Subsystem)
	{
		Subsystem->OnMarkerDetected.RemoveDynamic(this, &UCXMRPlacementComponent::HandleMarkerDetected);
		Subsystem->OnRecalibrateRequested.RemoveDynamic(this, &UCXMRPlacementComponent::HandleRecalibrateRequest);
		Subsystem->OnPlaceRequested.RemoveDynamic(this, &UCXMRPlacementComponent::HandlePlaceRequest);
	}
	Super::EndPlay(Reason);
}

void UCXMRPlacementComponent::HandleMarkerDetected(int32 MarkerId, FVector Position, FRotator Rotation, FVector2D Size)
{
	if (Mode != ECXMRPlacementMode::MarkerAnchor || !MarkerProfile)
	{
		return;
	}

	FCXMRMarkerEntry Entry;
	if (!MarkerProfile->FindEntry(MarkerId, Entry) || Entry.Role != ECXMRMarkerRole::Calibration)
	{
		return; // not a calibration marker (DynamicObject handled by a future component)
	}

	// Per-marker config — only valid AFTER detection (plugin caveat).
	if (Subsystem)
	{
		Subsystem->SetMarkerTimeout(MarkerId, Entry.Timeout);
		Subsystem->SetMarkerTrackingMode(MarkerId, Entry.TrackingMode);
	}

	if (bFreezeAfterCalibration && bCalibrated)
	{
		return; // frozen
	}

	// Accumulate this calibration marker's world pose, then (re)compute the alignment.
	DetectedCalib.Add(MarkerId, FTransform(Rotation, Position, FVector::OneVector));
	RecomputeCalibration();
}

void UCXMRPlacementComponent::RecomputeCalibration()
{
	AActor* Root = ResolveVehicleRoot();
	if (!Root || DetectedCalib.Num() == 0)
	{
		return;
	}

	FTransform VehicleWorld;
	bool bHavePose = false;

	// 2+ markers: robust baseline yaw + floor. 1 marker: full-transform fallback (uses its orientation).
	if (DetectedCalib.Num() >= 2)
	{
		bHavePose = ComputeMultiMarkerTransform(VehicleWorld);
	}
	if (!bHavePose)
	{
		const TPair<int32, FTransform>& First = *DetectedCalib.CreateConstIterator();
		FCXMRMarkerEntry Entry;
		if (MarkerProfile->FindEntry(First.Key, Entry))
		{
			// VehicleWorld = Inverse(LocalOffset) * MarkerWorld (UE compose A*B = apply A then B).
			VehicleWorld = Entry.LocalOffset.Inverse() * First.Value;
			bHavePose = true;
		}
	}
	if (!bHavePose)
	{
		return;
	}

	Root->SetActorTransform(VehicleWorld);

	// Freeze once enough markers have contributed. The freeze itself is LOCAL — bCalibrated makes
	// HandleMarkerDetected stop re-placing. Killing the headset's marker tracking is global and would
	// take DynamicObject markers (doors, props) down with it, so it is opt-in and off by default.
	if (DetectedCalib.Num() >= MinMarkersToCalibrate)
	{
		bCalibrated = true;
		if (bStopMarkerTrackingWhenCalibrated && Subsystem)
		{
			Subsystem->SetMarkerTracking(false);
		}
	}
}

bool UCXMRPlacementComponent::ComputeMultiMarkerTransform(FTransform& Out) const
{
	// Correspondences: p = marker position in vehicle-local space, q = measured world position.
	// Solve yaw (about Z) + translation, roll/pitch = 0 (vehicle sits level on the floor).
	// Uses marker POSITIONS only — the baseline between markers gives a far more stable yaw than
	// any single marker's own (noisy) orientation.
	TArray<FVector> P, Q;
	for (const TPair<int32, FTransform>& Pair : DetectedCalib)
	{
		FCXMRMarkerEntry Entry;
		if (MarkerProfile->FindEntry(Pair.Key, Entry))
		{
			P.Add(Entry.LocalOffset.GetLocation());
			Q.Add(Pair.Value.GetLocation());
		}
	}
	if (P.Num() < 2)
	{
		return false;
	}

	FVector PBar = FVector::ZeroVector;
	FVector QBar = FVector::ZeroVector;
	for (int32 i = 0; i < P.Num(); ++i) { PBar += P[i]; QBar += Q[i]; }
	PBar /= P.Num();
	QBar /= P.Num();

	// Optimal yaw about Z (least squares): yaw = atan2( sum cross_xy, sum dot_xy ).
	double DotSum = 0.0;
	double CrossSum = 0.0;
	for (int32 i = 0; i < P.Num(); ++i)
	{
		const FVector dp = P[i] - PBar;
		const FVector dq = Q[i] - QBar;
		DotSum   += dp.X * dq.X + dp.Y * dq.Y;
		CrossSum += dp.X * dq.Y - dp.Y * dq.X;
	}

	const float YawDeg = FMath::RadiansToDegrees(static_cast<float>(FMath::Atan2(CrossSum, DotSum)));
	const FRotator Rot(0.0f, YawDeg, 0.0f);
	const FVector Trans = QBar - Rot.RotateVector(PBar);

	Out = FTransform(Rot, Trans, FVector::OneVector);
	return true;
}

void UCXMRPlacementComponent::Recalibrate()
{
	bCalibrated = false;
	DetectedCalib.Reset();

	// Cycle tracking so the plugin re-fires Detected (its marker map is only reset on disable).
	if (Subsystem)
	{
		Subsystem->SetMarkerTracking(false);
		Subsystem->SetMarkerTracking(true);
	}
}

void UCXMRPlacementComponent::HandleRecalibrateRequest()
{
	Recalibrate();
}

void UCXMRPlacementComponent::HandlePlaceRequest()
{
	PlaceInFrontOfPawn();
}

void UCXMRPlacementComponent::PlaceInFrontOfPawn()
{
	AActor* Root = ResolveVehicleRoot();
	if (!Root)
	{
		return;
	}

	APlayerCameraManager* CamMgr = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0);
	APawn* Pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!CamMgr || !Pawn)
	{
		return;
	}

	// Horizontal look direction from the (HMD-driven) camera.
	FRotator LookRot = CamMgr->GetCameraRotation();
	LookRot.Pitch = 0.0f;
	LookRot.Roll = 0.0f;
	const FVector Forward = LookRot.Vector();

	// In front of the camera, on the floor (pawn origin = VR floor when tracking origin is floor/stage).
	FVector Target = CamMgr->GetCameraLocation() + Forward * PawnRelativeDistance;
	Target.Z = Pawn->GetActorLocation().Z;

	// Face the user (yaw + 180 so the vehicle front points back toward them).
	const FRotator FaceRot(0.0f, LookRot.Yaw + 180.0f, 0.0f);
	Root->SetActorTransform(FTransform(FaceRot, Target, FVector::OneVector));

	bCalibrated = true;
}
