#include "GizmoFirstContactComponent.h"
#include "GizmoNarrativeSubsystem.h"
#include "GizmoBark.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

const FName UGizmoFirstContactComponent::FirstContactBarkID = TEXT("FirstContact.Stun");

UGizmoFirstContactComponent::UGizmoFirstContactComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UGizmoFirstContactComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeFirstContactBark();
}

UGizmoNarrativeSubsystem* UGizmoFirstContactComponent::ResolveNarrativeSubsystem()
{
	if (CachedNarrativeSubsystem)
	{
		return CachedNarrativeSubsystem;
	}

	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			CachedNarrativeSubsystem = GameInstance->GetSubsystem<UGizmoNarrativeSubsystem>();
		}
	}

	if (!CachedNarrativeSubsystem && !bHasWarnedMissingNarrativeSubsystem)
	{
		bHasWarnedMissingNarrativeSubsystem = true;
		UE_LOG(LogTemp, Warning,
			TEXT("UGizmoFirstContactComponent: no UGizmoNarrativeSubsystem available - ")
			TEXT("the first-contact bark cannot be registered or triggered."));
	}

	return CachedNarrativeSubsystem;
}

void UGizmoFirstContactComponent::InitializeFirstContactBark()
{
	if (bHasInitializedFirstContactBark)
	{
		return;
	}
	bHasInitializedFirstContactBark = true;

	UGizmoNarrativeSubsystem* Subsystem = ResolveNarrativeSubsystem();
	if (!Subsystem)
	{
		return;
	}

	if (Subsystem->IsBarkRegistered(FirstContactBarkID))
	{
		return;
	}

	FGizmoBark Bark;
	Bark.BarkID = FirstContactBarkID;
	Bark.Lines = {
		TEXT("Gizmo: Wait - hold on. I know that chassis."),
		TEXT("Gizmo: That's not just some hostile. That's one of ours. Or... it was."),
		TEXT("Gizmo: However long you were under, that thing's been down here the whole time.")
	};
	Subsystem->RegisterBark(Bark);
}

void UGizmoFirstContactComponent::HandleAbilityCastApplied(EAbilitySlot Ability, AEnemyBase* TargetEnemy)
{
	if (Ability != EAbilitySlot::Stun)
	{
		return;
	}

	// Defensive: also called from BeginPlay(), but a test (or an editor World that
	// never runs BeginPlay - see AbilityCastVFXComponent's own precedent) may invoke
	// this handler without BeginPlay ever having run. InitializeFirstContactBark() is
	// idempotent, so calling it again here is always safe.
	InitializeFirstContactBark();

	UGizmoNarrativeSubsystem* Subsystem = ResolveNarrativeSubsystem();
	if (!Subsystem)
	{
		return;
	}

	Subsystem->TriggerBark(FirstContactBarkID);
}
