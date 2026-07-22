// Copyright GMTCK CX. CXMR shared types.

#pragma once

#include "CoreMinimal.h"
#include "CXMRTypes.generated.h"

class UMaterialInterface;

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

/**
 * Viewer commands relayed from input/UI to the vehicle actor through the subsystem.
 *
 * The pawn holds the input and the vehicle holds the turntable/loader, so they never see each other
 * directly — the GameInstance subsystem is the rendezvous, exactly as it is for placement requests.
 * One enum instead of one delegate per verb keeps that relay from ballooning as the Viewer grows.
 */
UENUM(BlueprintType)
enum class ECXMRViewerAction : uint8
{
	SpinLeft        UMETA(DisplayName = "Spin Left"),      // toggle continuous rotation
	SpinRight       UMETA(DisplayName = "Spin Right"),
	StopSpin        UMETA(DisplayName = "Stop Spin"),
	ResetRotation   UMETA(DisplayName = "Reset Rotation"),
	NextVehicle     UMETA(DisplayName = "Next Vehicle"),
	PreviousVehicle UMETA(DisplayName = "Previous Vehicle"),
	NextTrim        UMETA(DisplayName = "Next Trim"),
	PreviousTrim    UMETA(DisplayName = "Previous Trim"),
	NextCMF         UMETA(DisplayName = "Next CMF")
};

/** How the vehicle is placed. */
UENUM(BlueprintType)
enum class ECXMRPlacementMode : uint8
{
	MarkerAnchor UMETA(DisplayName = "Marker Anchor"), // align to a Calibration marker (interior freeze / exterior stand)
	PawnRelative UMETA(DisplayName = "Pawn Relative")  // place in front of the pawn, no marker (exterior turntable)
};

/**
 * One material swap inside a CMF option.
 *
 * Parts are addressed by COMPONENT TAG, not by name or index — a vehicle BP can be re-authored,
 * re-split or re-imported without invalidating the profile, as long as the tags survive.
 */
USTRUCT(BlueprintType)
struct FCXMRMaterialOverride
{
	GENERATED_BODY()

	/** Component tag to apply to. None = every primitive on the vehicle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|CMF") FName PartTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|CMF", meta = (ClampMin = "0")) int32 MaterialSlot = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|CMF") TSoftObjectPtr<UMaterialInterface> Material;
};

/**
 * Colour / material / finish option within a trim (paint, seat material, wheel finish).
 * PROVISIONAL — the shape will be settled against a real vehicle asset; no CAD vehicle exists yet.
 */
USTRUCT(BlueprintType)
struct FCXMRCMFOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|CMF") FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|CMF") TArray<FCXMRMaterialOverride> Materials;
};

/**
 * One trim level. Parts are shown/hidden by component tag: every tag mentioned by ANY trim in the
 * profile is "managed", and a trim shows the ones it lists and hides the rest. Untagged components
 * are always visible, so shared body geometry needs no marking — same rule as ECXMRSceneRole.
 *
 * PROVISIONAL — see FCXMRCMFOption.
 */
USTRUCT(BlueprintType)
struct FCXMRTrim
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Trim") FName Name;

	/** Component tags visible for this trim. Managed tags not listed here are hidden. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Trim") TArray<FName> VisibleParts;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Trim") TArray<FCXMRCMFOption> CMFOptions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Trim", meta = (ClampMin = "0")) int32 DefaultCMF = 0;
};

/**
 * One seating reference — a percentile manikin's eye (and optionally hip) point.
 *
 * Points are authored in the VEHICLE's own coordinate space, because they are package data: the
 * design eye point and H-point (SgRP) are defined relative to the vehicle origin, not the room.
 * Human Factors works in percentiles (5th female / 50th / 95th male), so a study holds a set of
 * these and snaps between them.
 */
USTRUCT(BlueprintType)
struct FCXMRManikinPosition
{
	GENERATED_BODY()

	/** e.g. "50th %ile", "95th male", "Driver eye". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Ergonomics") FName Name;

	/** Design eye point, vehicle-local. In VR the viewpoint moves here; in MR the vehicle moves so
	 *  this lands at the user's real eyes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Ergonomics") FTransform EyePoint = FTransform::Identity;

	/** H-point (SgRP), vehicle-local. Stored for manikin display / measurement; not moved to yet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Ergonomics") bool bHasHipPoint = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Ergonomics", meta = (EditCondition = "bHasHipPoint"))
	FTransform HipPoint = FTransform::Identity;
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
