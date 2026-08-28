#include "LevelBriefingSubsystem.h"
#include "LevelLifecycleSubsystem.h"
#include "KrowdKontrolPlayerController.h"
#include "GameFramework/PlayerController.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"

void ULevelBriefingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Force ULevelLifecycleSubsystem to construct/Initialize() before we look it up
	// below, mirroring UAbilityUnlockLevelSubsystem::Initialize()'s identical
	// InitializeDependency() call.
	Collection.InitializeDependency<ULevelLifecycleSubsystem>();
	if (UWorld* World = GetWorld())
	{
		if (ULevelLifecycleSubsystem* LifecycleSubsystem = World->GetSubsystem<ULevelLifecycleSubsystem>())
		{
			LifecycleSubsystem->OnLevelBegin.AddDynamic(this, &ULevelBriefingSubsystem::HandleLevelBegin);
		}

		// Real game worlds (PIE/packaged) load the shipped LevelBriefingTable content
		// asset from its fixed path here - mirrors
		// ULevelSequenceSubsystem::Initialize()'s identical auto-load, gated the same
		// way so Automation's CreateNewMap() Editor Worlds (World->IsGameWorld() ==
		// false there) never touch this and keep constructing the subsystem with
		// LevelBriefingTable unset, matching every existing
		// KrowdKontrolLevelBriefingSubsystemTest.cpp case that builds its own in-code
		// table. Subsystems have no per-instance Details panel to hand-author this
		// reference (issue #356's e2e gap - real menu-entry travel left players with no
		// briefing card).
		if (!LevelBriefingTable && World->IsGameWorld())
		{
			LevelBriefingTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Data/DT_LevelBriefingTable.DT_LevelBriefingTable"));
		}
	}
}

void ULevelBriefingSubsystem::HandleLevelBegin(FName MapName)
{
	if (!LevelBriefingTable)
	{
		if (!bHasWarnedMissingBriefingTable)
		{
			bHasWarnedMissingBriefingTable = true;
			UE_LOG(LogTemp, Warning,
				TEXT("ULevelBriefingSubsystem: LevelBriefingTable is unset - no pre-level briefing will show for map '%s'."),
				*MapName.ToString());
		}
		return;
	}

	const FString BareMapName = UWorld::RemovePIEPrefix(MapName.ToString());
	// bWarnIfRowMissing=false - a missing row is an expected, silent no-op (matches
	// UAbilityUnlockLevelSubsystem's prototype-maps-default-to-no-op precedent), and
	// this function already logs its own one-shot warning below.
	const FLevelBriefingRow* Row = LevelBriefingTable->FindRow<FLevelBriefingRow>(FName(*BareMapName), TEXT("ULevelBriefingSubsystem::HandleLevelBegin"), false);
	if (!Row)
	{
		if (!bHasWarnedMissingBriefingRow)
		{
			bHasWarnedMissingBriefingRow = true;
			UE_LOG(LogTemp, Warning,
				TEXT("ULevelBriefingSubsystem: no LevelBriefingTable row found for map '%s' - no pre-level briefing will show."),
				*BareMapName);
		}
		return;
	}

	UWorld* World = GetWorld();
	APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	if (AKrowdKontrolPlayerController* Controller = Cast<AKrowdKontrolPlayerController>(PlayerController))
	{
		Controller->ShowLevelBriefing(*Row);
		return;
	}

	// No AKrowdKontrolPlayerController resolvable yet - remember this row so
	// RetryPendingBriefingForController() can still deliver it once the controller
	// exists, since OnLevelBegin only fires once per world.
	bLevelBeginFiredWithNoController = true;
	PendingBriefingRow = *Row;
	if (!bHasWarnedMissingController)
	{
		bHasWarnedMissingController = true;
		UE_LOG(LogTemp, Warning,
			TEXT("ULevelBriefingSubsystem: no AKrowdKontrolPlayerController found for map '%s' - briefing will be shown once one is available."),
			*BareMapName);
	}
}

void ULevelBriefingSubsystem::RetryPendingBriefingForController(AKrowdKontrolPlayerController* Controller)
{
	if (!bLevelBeginFiredWithNoController || !Controller)
	{
		return;
	}
	bLevelBeginFiredWithNoController = false;
	Controller->ShowLevelBriefing(PendingBriefingRow);
}
