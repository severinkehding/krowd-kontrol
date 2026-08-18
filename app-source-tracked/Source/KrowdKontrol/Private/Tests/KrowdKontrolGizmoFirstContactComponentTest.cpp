// Confirms UGizmoFirstContactComponent (issue #59, PRD 07 REQ-2): the first time
// UAbilityCastComponent::OnAbilityCastApplied fires with EAbilitySlot::Stun, the
// "FirstContact.Stun" bark is registered and triggered exactly once through
// UGizmoNarrativeSubsystem's own no-replay guarantee - subsequent Stun casts (second
// and later) must not re-trigger it. Also confirms a non-Stun ability cast never
// triggers the bark at all.
//
// CreateNewMap()'s editor World has no live GameInstance to resolve
// UGizmoNarrativeSubsystem through automatically (no existing test in this codebase
// relies on World->GetGameInstance() - only World->GetSubsystem<T>() for
// WorldSubsystems is used elsewhere), so this test injects a directly-constructed
// UGizmoNarrativeSubsystem via friend access, the same NewObject<>(GameInstanceOuter)
// construction KrowdKontrolGizmoNarrativeSubsystemTest.cpp uses.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "GizmoFirstContactComponent.h"
#include "GizmoNarrativeSubsystem.h"
#include "GizmoBarkTestListener.h"
#include "AbilityCastComponent.h"
#include "AbilityUnlockComponent.h"
#include "AbilityCooldownComponent.h"
#include "EnemyBaseTestActor.h"
#include "Engine/GameInstance.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolGizmoFirstContactComponentTest,
	"KrowdKontrol.Unit.GizmoFirstContactComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolGizmoFirstContactComponentTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	APawn* Owner = World->SpawnActor<APawn>();
	if (!TestNotNull(TEXT("APawn should spawn into the test World"), Owner))
	{
		return false;
	}

	UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
	UnlockComponent->RegisterComponent();
	UAbilityCooldownComponent* CooldownComponent = NewObject<UAbilityCooldownComponent>(Owner);
	CooldownComponent->RegisterComponent();
	UAbilityCastComponent* CastComponent = NewObject<UAbilityCastComponent>(Owner);
	CastComponent->RegisterComponent();

	UGizmoFirstContactComponent* FirstContactComponent = NewObject<UGizmoFirstContactComponent>(Owner);
	FirstContactComponent->RegisterComponent();

	// Directly-constructed UGizmoNarrativeSubsystem, injected via friend access -
	// bypasses the engine's subsystem-collection Initialize() lifecycle entirely
	// (mirrors KrowdKontrolGizmoNarrativeSubsystemTest.cpp), so RegisterPlaceholder-
	// MilestoneBarks() never runs and only FirstContactComponent's own registration
	// populates the registry.
	UGameInstance* GameInstanceOuter = NewObject<UGameInstance>();
	UGizmoNarrativeSubsystem* NarrativeSubsystem = NewObject<UGizmoNarrativeSubsystem>(GameInstanceOuter);
	FirstContactComponent->CachedNarrativeSubsystem = NarrativeSubsystem;

	UGizmoBarkTestListener* Listener = NewObject<UGizmoBarkTestListener>();
	NarrativeSubsystem->OnBarkTriggered.AddDynamic(Listener, &UGizmoBarkTestListener::HandleBarkTriggered);

	CastComponent->OnAbilityCastApplied.AddDynamic(FirstContactComponent, &UGizmoFirstContactComponent::HandleAbilityCastApplied);

	AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
	if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), Enemy))
	{
		return false;
	}
	Enemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert

	// (a) A real, gated, successful Stun cast fires the bark exactly once.
	const bool bFirstCastResult = CastComponent->TryCastAbility(EAbilitySlot::Stun);
	TestTrue(TEXT("TryCastAbility(Stun) should succeed against an eligible in-range enemy"), bFirstCastResult);
	TestEqual(TEXT("The first successful Stun cast should trigger the first-contact bark exactly once"),
		Listener->CallCount, 1);
	TestEqual(TEXT("The triggered bark ID should be FirstContact.Stun"),
		Listener->LastBarkID, FName(TEXT("FirstContact.Stun")));
	TestTrue(TEXT("FirstContact.Stun should be registered"),
		NarrativeSubsystem->IsBarkRegistered(TEXT("FirstContact.Stun")));
	TestTrue(TEXT("FirstContact.Stun should be marked fired"),
		NarrativeSubsystem->HasBarkFired(TEXT("FirstContact.Stun")));

	// (b) Simulated further successful Stun casts (as TryCastAbility would broadcast
	// on any later cast, once off cooldown against a fresh target) must not re-trigger
	// the bark - UAbilityCooldownComponent exposes no public way to force-expire a
	// cooldown outside its own friend test class, so this drives the handler directly
	// with the same (Ability, TargetEnemy) shape a second and third real cast would
	// broadcast, pinning the "exactly once across multiple casts" acceptance criterion
	// without needing to fight the cooldown gate (already covered independently by
	// KrowdKontrolAbilityCastComponentTest.cpp).
	FirstContactComponent->HandleAbilityCastApplied(EAbilitySlot::Stun, Enemy);
	TestEqual(TEXT("A second simulated Stun cast must not re-trigger the bark"),
		Listener->CallCount, 1);

	FirstContactComponent->HandleAbilityCastApplied(EAbilitySlot::Stun, Enemy);
	TestEqual(TEXT("A third simulated Stun cast must not re-trigger the bark"),
		Listener->CallCount, 1);

	// (c) A non-Stun ability cast must never trigger the bark at all, even before any
	// Stun cast has happened.
	UGameInstance* SecondGameInstanceOuter = NewObject<UGameInstance>();
	UGizmoNarrativeSubsystem* SecondNarrativeSubsystem = NewObject<UGizmoNarrativeSubsystem>(SecondGameInstanceOuter);
	UGizmoFirstContactComponent* SecondFirstContactComponent = NewObject<UGizmoFirstContactComponent>(Owner);
	SecondFirstContactComponent->RegisterComponent();
	SecondFirstContactComponent->CachedNarrativeSubsystem = SecondNarrativeSubsystem;

	UGizmoBarkTestListener* SecondListener = NewObject<UGizmoBarkTestListener>();
	SecondNarrativeSubsystem->OnBarkTriggered.AddDynamic(SecondListener, &UGizmoBarkTestListener::HandleBarkTriggered);

	SecondFirstContactComponent->HandleAbilityCastApplied(EAbilitySlot::Sleep, Enemy);
	TestEqual(TEXT("A non-Stun ability cast must never trigger the first-contact bark"),
		SecondListener->CallCount, 0);
	TestFalse(TEXT("A non-Stun cast must not even register the bark"),
		SecondNarrativeSubsystem->IsBarkRegistered(TEXT("FirstContact.Stun")));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
