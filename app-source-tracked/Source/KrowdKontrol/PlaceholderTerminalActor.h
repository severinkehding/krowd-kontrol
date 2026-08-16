// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GizmoNarrativeSubsystem.h"
#include "PlaceholderTerminalActor.generated.h"

class UStaticMeshComponent;

// Minimal placeholder-first actor (MISSION.md Quality Standards): a single reusable
// "Terminal" that reveals a short piece of foreshadowing log text exactly once, and
// never gates any level-critical-path logic (PRD 07 REQ-4). See issue #62.
//
// Deliberately does not go through UGizmoNarrativeSubsystem - it carries its own
// FGizmoBark-shaped content (TerminalLog) and its own FOnBarkTriggered-shaped
// delegate (OnTerminalLogRevealed), reusing the narrative system's data shape and
// delegate signature without depending on the subsystem itself. See this issue's
// investigation artifact ("Approach Chosen") for why: GetGameInstance() is null in
// this project's CreateNewMap()-based Automation Framework test worlds, which would
// make the subsystem route silently un-testable.
UCLASS()
class KROWDKONTROL_API APlaceholderTerminalActor : public AActor
{
	GENERATED_BODY()

public:
	APlaceholderTerminalActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terminal")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	// Placeholder foreshadowing content; level authors fill in real BarkID/Lines per
	// instance later - see this issue's Notes for why real Drain-foreshadowing copy
	// is out of scope here.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terminal")
	FGizmoBark TerminalLog;

	// Fires exactly once, when Interact() first reveals TerminalLog. Reuses
	// FOnBarkTriggered's existing signature (FName, TArray<FString>) so a future
	// text-display widget can bind one handler shape to either a real Gizmo bark or
	// a Terminal reveal.
	UPROPERTY(BlueprintAssignable, Category = "Terminal")
	FOnBarkTriggered OnTerminalLogRevealed;

	// The sole entry point that reveals TerminalLog. Called by whatever
	// player-triggered event a level author wires up (a trigger volume overlap,
	// player-driven interaction, etc.) - this actor has no opinion on what that
	// event is, and never touches player input APIs itself. Broadcasts
	// OnTerminalLogRevealed exactly once; every call after the first is a no-op.
	// Never touches any progression/gating API - interacting with the terminal is
	// never required to progress.
	UFUNCTION(BlueprintCallable, Category = "Terminal")
	void Interact();

	bool HasBeenInteracted() const { return TerminalLog.bHasBeenTriggered; }
};
