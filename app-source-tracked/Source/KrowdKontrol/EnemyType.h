#pragma once

#include "CoreMinimal.h"
#include "EnemyType.generated.h"

// Single source of truth for MISSION.md Hard Invariant 5: the 4 locked enemy
// codenames (RU-NNR, TR-UPR, B0-0MR, SN-1PR) as a real C++ type, for the first time
// anywhere in the codebase - previously only prose/ReservedGameplayColours.h's
// colour-accessor comments referenced this roster. Backs ARoomActor::AddTargetZone
// (issue #39) and is a prerequisite for a separate future P1 room-metadata-tagging
// issue (PRD 05 REQ-4).
//
// C++ identifiers can't contain '-', so each value is spelled with an underscore;
// UMETA(DisplayName) preserves the real hyphenated codename everywhere a human (Editor
// dropdown, Blueprint) sees it. No Count sentinel - the roster is locked at exactly 4
// by MISSION.md Hard Invariant 5, and nothing here needs to iterate/count it.
UENUM(BlueprintType)
enum class EEnemyType : uint8
{
	RU_NNR UMETA(DisplayName = "RU-NNR"),
	TR_UPR UMETA(DisplayName = "TR-UPR"),
	B0_0MR UMETA(DisplayName = "B0-0MR"),
	SN_1PR UMETA(DisplayName = "SN-1PR")
};
