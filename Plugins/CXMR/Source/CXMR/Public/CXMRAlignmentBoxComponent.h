// Copyright GMTCK CX.
//
// UCXMRAlignmentBoxComponent — a real, measured box that the vehicle is lined up against.
//
// A plain box standing in for part of the car (a console, a seat base) has nothing in common with the CAD it stands
// for, so lining the CAD up with it by eye has no edge to match. This component puts the same box into the vehicle:
// type the tape-measured size, place it in the vehicle Blueprint where the real box should stand relative to the CAD.
// Then either
//   * look — its outline is drawn, and nudging the car until the outline sits on the real box aligns everything, or
//   * touch — rest an index fingertip on the numbered corners one at a time and pinch with the other hand; once the
//     corners are in, the car is moved so the virtual corners land on the touched ones.
// Either way the result is a normal manual placement: Learn marker layout keeps it for the next session.
//
// Component origin = centre of the box's BOTTOM face, X = the box's front. It lives in the vehicle actor (or anything
// attached under the vehicle root) so it moves with the car.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "InputCoreTypes.h"
#include "CXMRAlignmentBoxComponent.generated.h"

class UCXMRPlacementComponent;
class UTextRenderComponent;

UENUM(BlueprintType)
enum class ECXMRBoxCorner : uint8
{
	TopFrontLeft,
	TopFrontRight,
	TopBackRight,
	TopBackLeft,
	BottomFrontLeft,
	BottomFrontRight,
	BottomBackRight,
	BottomBackLeft
};

UCLASS(ClassGroup = (CXMR), meta = (BlueprintSpawnableComponent), DisplayName = "CXMR Alignment Box")
class CXMR_API UCXMRAlignmentBoxComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UCXMRAlignmentBoxComponent();

	/** The real box, tape-measured: X front-back, Y left-right, Z height, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Alignment Box", meta = (ClampMin = "1.0"))
	FVector BoxSize = FVector(40.0, 30.0, 30.0);

	/** Corners touched for alignment, in order. Two are enough; a third reports how well the touches agree. Pick corners
	 *  far apart — the heading comes from the distance between them. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Alignment Box")
	TArray<ECXMRBoxCorner> TouchCorners = { ECXMRBoxCorner::TopFrontLeft, ECXMRBoxCorner::TopFrontRight, ECXMRBoxCorner::TopBackRight };

	/** Draw the box outline and the corner numbers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Alignment Box") bool bShowOutline = true;

	/** Hand whose index fingertip touches the corners. The other hand's pinch records. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Alignment Box") EControllerHand TouchHand = EControllerHand::Right;

	/** Corners are touched from above with the finger pad. The tracked tip joint sits inside the finger, one finger
	 *  radius above the surface, so the recorded point is lowered by that radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Alignment Box") bool bTouchFromAbove = true;

	/** How long a touch is averaged once recording starts, seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Alignment Box", meta = (ClampMin = "0.1")) float TouchSampleSeconds = 0.5f;

	/** Line thickness of the outline, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Alignment Box", meta = (ClampMin = "0.0")) float OutlineThickness = 0.2f;

	/** Start recording the next corner with the touch hand's current fingertip. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Alignment Box") void RecordTouch();

	/** Record the next corner at a known world point (scripts and tests; the fingertip path ends here too). */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Alignment Box") void RecordTouchAt(FVector WorldPoint);

	/** Move the vehicle so the touched corners fit. Needs two touches; runs by itself after the last one. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Alignment Box") bool ApplyTouches();

	/** Forget the touches (the vehicle stays where it is). */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Alignment Box") void ClearTouches();

	/** A corner in world space, as the virtual box stands now. */
	UFUNCTION(BlueprintPure, Category = "CXMR|Alignment Box") FVector GetCornerWorld(ECXMRBoxCorner Corner) const;

	/** RMS cm between the touched points and the corners after the last apply; negative before one. */
	UFUNCTION(BlueprintPure, Category = "CXMR|Alignment Box") float GetTouchFitError() const { return TouchFitError; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	struct FTouch
	{
		FVector World = FVector::ZeroVector;
		float ScatterCm = 0.0f;
	};
	TArray<FTouch> Touches;
	float TouchFitError = -1.0f;
	FString LastProblem;

	// Recording in progress: fingertip samples until TouchSampleSeconds have passed.
	bool bSampling = false;
	float SampleElapsed = 0.0f;
	TArray<FVector> Samples;

	bool bOtherHandPinching = false;

	UPROPERTY(Transient) TArray<TObjectPtr<UTextRenderComponent>> Labels;

	FVector CornerLocal(ECXMRBoxCorner Corner) const;
	UCXMRPlacementComponent* FindPlacement() const;
	/** The touch hand's fingertip in world space, surface correction applied. */
	bool GetTouchPoint(FVector& OutPoint) const;
	void AddTouch(const FVector& World, float ScatterCm);
	void UpdatePinchTrigger();
	void DrawOutline();
	void UpdateLabels();
	void RegisterTunables();
	FText StatusText() const;
	FText NearestCornerText() const;
};
