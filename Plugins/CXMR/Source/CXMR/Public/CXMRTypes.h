// Copyright GMTCK CX. CXMR shared types.

#pragma once

#include "CoreMinimal.h"
#include "CXMRTypes.generated.h"

/**
 * Role of a Varjo marker in a project's marker profile.
 * Assigned per-entry (NOT derived from the ID number) — each project uses different physical IDs.
 */
UENUM(BlueprintType)
enum class ECXMRMarkerRole : uint8
{
	Calibration   UMETA(DisplayName = "Calibration"),    // anchors the vehicle (one-shot freeze / multi-marker)
	DynamicObject UMETA(DisplayName = "Dynamic Object"),  // tracks a moving real prop (door, box, cup...)
	Reserved      UMETA(DisplayName = "Reserved")
};

/**
 * Marker tracking responsiveness. CXMR-owned mirror of the Varjo enum so the public API
 * stays Varjo-free (single façade). Mapped to EMarkerTrackingMode inside the subsystem.
 */
UENUM(BlueprintType)
enum class ECXMRMarkerTrackingMode : uint8
{
	Stationary UMETA(DisplayName = "Stationary"), // heavily filtered — fixed elements (cockpit)
	Dynamic    UMETA(DisplayName = "Dynamic")     // prediction on — moving elements (lower accuracy)
};

/**
 * What a scene object is FOR in mixed reality.
 *
 * Declared on the object itself (UCXMRSceneObjectComponent) rather than enumerated in a central
 * array. Objects with no component are simply always visible, so the vehicle needs no marking at
 * all — only the exceptions are tagged, and a new level cannot silently miss one.
 */
UENUM(BlueprintType)
enum class ECXMRSceneRole : uint8
{
	/** Virtual stand-in for the room (sky, walls, floor). Hidden while passthrough provides the real room. */
	VROnly   UMETA(DisplayName = "VR Only"),

	/** Custom-Depth mask geometry — punches a hole so a real object shows through (steering wheel, seat). */
	MaskMesh UMETA(DisplayName = "Mask Mesh")
};

/** How the vehicle is placed. */
UENUM(BlueprintType)
enum class ECXMRPlacementMode : uint8
{
	MarkerAnchor UMETA(DisplayName = "Marker Anchor"), // align to a Calibration marker (interior freeze / exterior stand)
	PawnRelative UMETA(DisplayName = "Pawn Relative")  // place in front of the pawn, no marker (exterior turntable)
};

/**
 * One physical marker's configuration in a project's marker profile.
 * Marker IDs differ per program, so this is authored per-project in a UCXMRMarkerProfile Data Asset.
 */
USTRUCT(BlueprintType)
struct FCXMRMarkerEntry
{
	GENERATED_BODY()

	/** Physical printed marker ID (from Varjo Base). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Marker") int32 MarkerId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Marker") ECXMRMarkerRole Role = ECXMRMarkerRole::Calibration;

	/** Marker pose in the target's LOCAL space (calibration: vehicle-local; dynamic: bound-actor-local). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Marker") FTransform LocalOffset = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Marker") ECXMRMarkerTrackingMode TrackingMode = ECXMRMarkerTrackingMode::Stationary;

	/** Seconds before a lost marker fires MarkerLost. Set in the OnMarkerDetected handler. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Marker", meta = (ClampMin = "0.0")) float Timeout = 3.0f;

	/** Human-readable name (e.g. "Dash Left"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Marker") FName Label;

	/** DynamicObject only: the actor (by tag) this marker drives. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Marker") FName TargetTag;
};
