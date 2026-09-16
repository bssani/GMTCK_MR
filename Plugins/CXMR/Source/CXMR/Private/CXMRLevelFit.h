// Copyright GMTCK CX.
//
// CXMRLevelFit — the rigid fit every vehicle alignment shares: yaw about world up + translation, no roll or pitch.
//
// Markers and touched box corners are both "points known in vehicle space, measured in world space". The floor is
// level, so only yaw and position are solved — two points already give a stable heading from their baseline, which a
// single point's own orientation never does.

#pragma once

#include "CoreMinimal.h"

namespace CXMRLevelFit
{
	/**
	 * Least-squares yaw + translation mapping Local[i] onto Measured[i].
	 * OutRmsCm = RMS distance left between the mapped and measured points — how far the measurements disagree with the
	 * layout. False with fewer than two pairs, or when either set of points has no horizontal spread to take a heading from.
	 */
	inline bool Solve(const TArray<FVector>& Local, const TArray<FVector>& Measured, FTransform& Out, float& OutRmsCm,
		double MinHorizontalSpreadCm = 1.0)
	{
		const int32 N = FMath::Min(Local.Num(), Measured.Num());
		if (N < 2)
		{
			return false;
		}

		FVector PBar = FVector::ZeroVector;
		FVector QBar = FVector::ZeroVector;
		for (int32 i = 0; i < N; ++i) { PBar += Local[i]; QBar += Measured[i]; }
		PBar /= N;
		QBar /= N;

		// Optimal yaw about Z: atan2(sum cross_xy, sum dot_xy).
		double DotSum = 0.0;
		double CrossSum = 0.0;
		double LocalSpread = 0.0;
		double MeasuredSpread = 0.0;
		for (int32 i = 0; i < N; ++i)
		{
			const FVector dp = Local[i] - PBar;
			const FVector dq = Measured[i] - QBar;
			DotSum   += dp.X * dq.X + dp.Y * dq.Y;
			CrossSum += dp.X * dq.Y - dp.Y * dq.X;
			LocalSpread    = FMath::Max(LocalSpread,    FVector2D(dp.X, dp.Y).Size());
			MeasuredSpread = FMath::Max(MeasuredSpread, FVector2D(dq.X, dq.Y).Size());
		}
		if (LocalSpread < MinHorizontalSpreadCm || MeasuredSpread < MinHorizontalSpreadCm)
		{
			return false;   // points stacked vertically: any heading fits them equally well
		}

		const FRotator Rot(0.0f, FMath::RadiansToDegrees(static_cast<float>(FMath::Atan2(CrossSum, DotSum))), 0.0f);
		const FVector Trans = QBar - Rot.RotateVector(PBar);

		double SquaredError = 0.0;
		for (int32 i = 0; i < N; ++i)
		{
			SquaredError += FVector::DistSquared(Rot.RotateVector(Local[i]) + Trans, Measured[i]);
		}
		OutRmsCm = static_cast<float>(FMath::Sqrt(SquaredError / N));
		Out = FTransform(Rot, Trans, FVector::OneVector);
		return true;
	}
}
