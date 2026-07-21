// Copyright GMTCK CX.
//
// ACXMRGameMode — minimal base game mode that spawns ACXMRPawn.
// (Later: pawn-swap for hand-interaction pawn, per the interaction milestone.)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CXMRGameMode.generated.h"

UCLASS()
class CXMR_API ACXMRGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACXMRGameMode();
};
