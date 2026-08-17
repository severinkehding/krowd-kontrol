#pragma once

#include "CoreMinimal.h"

// Single source of truth for MISSION.md Hard Invariant 3 / PRD 13 REQ-4: the five
// gameplay-information colours are a locked, hard-reserved channel - Purple (RU-NNR /
// Snare), Teal (TR-UPR / Root), Orange (B0-0MR / Fear), Blue (SN-1PR / Sleep), White
// (player / Stun). No other gameplay-relevant object, UI chrome, or environmental prop
// may use these five colours for non-informational purposes (issue #70). Every
// consumer (HUD widgets, their Automation tests) should go through these accessors
// rather than a hardcoded literal, so a future art-direction ruling on the real RGB
// values is a one-file change with zero call-site updates.
//
// The concrete RGB values below are deliberately saturated, mutually well-separated
// PLACEHOLDERS - no real enemy/ability visual asset exists yet anywhere in the
// codebase to source final values from (see APlaceholderTargetZoneActor's beacon
// colour for the same caveat pattern elsewhere in this module).
//
// No UENUM/UCLASS/.generated.h - a plain namespace of free functions doesn't need
// UHT reflection or Blueprint visibility, and nothing here has asked for it yet.
// This is the module's first UHT-reflection-free header at the root level (compare
// Public/Herdable.h and AbilitySlot.h, which both use .generated.h for their
// UINTERFACE/UENUM needs) - revisit if a Blueprint consumer ever needs these values.
//
// Issue #11 added GetBackground() below for the same reason GetAll()/the 5 accessors
// exist: one file, zero call-site duplication, for the palette's Background value too.
namespace ReservedGameplayColours
{
	// RU-NNR enemy / Snare ability.
	KROWDKONTROL_API FLinearColor GetPurple();

	// TR-UPR enemy / Root ability.
	KROWDKONTROL_API FLinearColor GetTeal();

	// B0-0MR enemy / Fear ability.
	KROWDKONTROL_API FLinearColor GetOrange();

	// SN-1PR enemy / Sleep ability.
	KROWDKONTROL_API FLinearColor GetBlue();

	// Player / Stun ability.
	KROWDKONTROL_API FLinearColor GetWhite();

	// All 5 reserved colours, for code (widgets, tests) that wants to assert
	// none-match rather than name each individually.
	KROWDKONTROL_API TArray<FLinearColor> GetAll();

	// Desaturated near-black environment background colour (issue #11, PRD 11
	// REQ-2). Deliberately NOT included in GetAll() - this is the backdrop the 5
	// reserved colours are locked against, not a 6th reserved colour.
	KROWDKONTROL_API FLinearColor GetBackground();
}
