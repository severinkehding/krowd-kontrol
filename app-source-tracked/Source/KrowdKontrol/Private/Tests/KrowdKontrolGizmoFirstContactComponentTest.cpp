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
#include "FlatCamera3DPrototypePawn.h"
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

	// This harness never drives the World through World->BeginPlay() (see
	// KrowdKontrolWaveSpawnerComponentTest.cpp's DispatchBeginPlay note), so without an
	// explicit drive here, BeginPlay()'s own call to InitializeFirstContactBark() would
	// never be exercised by any test - every assertion below is otherwise reachable
	// purely through the idempotent second call inside HandleAbilityCastApplied().
	// AActor::DispatchBeginPlay() is the public, legal route (calling the component's
	// BeginPlay() directly on an owner that never itself began play is not an option -
	// see KrowdKontrolHUDWiringTest.cpp's note on the matching engine assert).
	Owner->DispatchBeginPlay();
	TestTrue(TEXT("BeginPlay() should register the bark ahead of any cast"),
		NarrativeSubsystem->IsBarkRegistered(TEXT("FirstContact.Stun")));
	TestFalse(TEXT("BeginPlay() should only register the bark, never trigger it"),
		NarrativeSubsystem->HasBarkFired(TEXT("FirstContact.Stun")));

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

	// (c) The respawn/relevel "no reset" safety guarantee: a second
	// UGizmoFirstContactComponent instance (as a fresh pawn respawn/relevel would
	// construct) sharing the SAME already-fired UGizmoNarrativeSubsystem must not
	// re-register FirstContact.Stun and reset it back to unfired -
	// UGizmoNarrativeSubsystem::RegisterBark's own comment documents that
	// re-registering an already-fired BarkID resets bHasBeenTriggered via a plain
	// TMap::Add overwrite, which is exactly the hazard InitializeFirstContactBark()'s
	// IsBarkRegistered() guard exists to prevent.
	UGizmoFirstContactComponent* RespawnedComponent = NewObject<UGizmoFirstContactComponent>(Owner);
	RespawnedComponent->RegisterComponent();
	RespawnedComponent->CachedNarrativeSubsystem = NarrativeSubsystem;

	RespawnedComponent->InitializeFirstContactBark();
	TestEqual(TEXT("A respawned component's own re-registration attempt must not replay the bark"),
		Listener->CallCount, 1);
	TestTrue(TEXT("FirstContact.Stun must still be marked fired after a respawned component's init"),
		NarrativeSubsystem->HasBarkFired(TEXT("FirstContact.Stun")));

	// (d) A non-Stun ability cast must never trigger the bark at all, even before any
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

	// (e) With no CachedNarrativeSubsystem injected, ResolveNarrativeSubsystem() falls
	// through to World->GetGameInstance() - which CreateNewMap()'s editor World does
	// not have (this file's own header comment; no other test in the codebase
	// exercises this call either). HandleAbilityCastApplied must degrade safely: no
	// crash, and reaching the assertion below is itself the proof this survived.
	UGizmoFirstContactComponent* UnresolvedComponent = NewObject<UGizmoFirstContactComponent>(Owner);
	UnresolvedComponent->RegisterComponent();
	UnresolvedComponent->HandleAbilityCastApplied(EAbilitySlot::Stun, Enemy);
	TestTrue(TEXT("HandleAbilityCastApplied must not crash when no UGizmoNarrativeSubsystem can be resolved"), true);

	// (f) Real pawn-constructor wiring: AFlatCamera3DPrototypePawn's constructor binds
	// AbilityCastComponent->OnAbilityCastApplied to its own GizmoFirstContactComponent
	// via AddDynamic, directly below an identically-shaped AbilityCastVFXComponent
	// bind - a copy-paste slip there (wrong instance/method/delegate) would compile
	// cleanly and every other case above would still pass, since none of them go
	// through the real pawn. This drives a real cast through the real pawn's own
	// components and confirms the bind actually reaches GizmoFirstContactComponent.
	AFlatCamera3DPrototypePawn* WiringPawn = World->SpawnActor<AFlatCamera3DPrototypePawn>();
	if (!TestNotNull(TEXT("AFlatCamera3DPrototypePawn should spawn into the test World"), WiringPawn))
	{
		return false;
	}
	if (!TestNotNull(TEXT("The real pawn's GizmoFirstContactComponent should be constructed"),
		ToRawPtr(WiringPawn->GizmoFirstContactComponent)))
	{
		return false;
	}

	UGameInstance* WiringGameInstanceOuter = NewObject<UGameInstance>();
	UGizmoNarrativeSubsystem* WiringNarrativeSubsystem = NewObject<UGizmoNarrativeSubsystem>(WiringGameInstanceOuter);
	WiringPawn->GizmoFirstContactComponent->CachedNarrativeSubsystem = WiringNarrativeSubsystem;

	UGizmoBarkTestListener* WiringListener = NewObject<UGizmoBarkTestListener>();
	WiringNarrativeSubsystem->OnBarkTriggered.AddDynamic(WiringListener, &UGizmoBarkTestListener::HandleBarkTriggered);

	AEnemyBaseTestActor* WiringEnemy = World->SpawnActor<AEnemyBaseTestActor>();
	if (!TestNotNull(TEXT("A second AEnemyBaseTestActor should spawn into the test World"), WiringEnemy))
	{
		return false;
	}
	WiringEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert

	const bool bWiringCastResult = WiringPawn->AbilityCastComponent->TryCastAbility(EAbilitySlot::Stun);
	TestTrue(TEXT("TryCastAbility(Stun) should succeed against an eligible in-range enemy via the real pawn"),
		bWiringCastResult);
	TestEqual(TEXT("The pawn's real constructor-time AddDynamic binding must reach GizmoFirstContactComponent"),
		WiringListener->CallCount, 1);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
