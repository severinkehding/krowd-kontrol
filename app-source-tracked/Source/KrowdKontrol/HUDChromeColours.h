#pragma once

#include "CoreMinimal.h"

// Single source of truth for the HUD widgets' reserved-colour-safe chrome palette -
// the desaturated near-black background and light-gray text shared by every widget's
// non-informational chrome (borders, panels, static labels). This is the complement of
// ReservedGameplayColours, not a duplicate of it: ReservedGameplayColours locks the five
// gameplay-information colours (MISSION.md Hard Invariant 3 / PRD 13 REQ-4) that chrome
// must never use, while this namespace is the actual chrome value every widget should
// share so a future art-direction change is a one-file edit instead of N call-site edits
// (issue #93). Before this existed, AbilityCooldownTrayWidget.cpp, EnergyMeterWidget.cpp,
// and PostRunSummaryWidget.cpp each hardcoded their own copy of both literals with
// nothing enforcing they stayed identical.
//
// No UENUM/UCLASS/.generated.h - same rationale as ReservedGameplayColours.h: a plain
// namespace of free functions doesn't need UHT reflection or Blueprint visibility, and
// no consumer has asked for either yet (all three widgets build their UI tree in C++).
namespace HUDChromeColours
{
	// Desaturated near-black chrome background, shared by every HUD widget's root
	// border/panel.
	KROWDKONTROL_API FLinearColor GetBackground();

	// Light-gray (not pure white) chrome text colour, shared by every HUD widget's
	// non-informational text.
	KROWDKONTROL_API FLinearColor GetText();
}
