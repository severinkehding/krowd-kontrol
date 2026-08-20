// Confirms the acceptance criteria of issue #171 (PRD "Run Lifecycle & Progression
// Signals" REQ-3): when the possessed pawn's UPlayerEnergyComponent reaches 0 energy,
// ULevelFailComponent::OnLevelFailed fires exactly once, the pawn's input is disabled
// via AKrowdKontrolPlayerController::HandleLevelFailed, and the level's in-progress
// clear timer is discarded (never recorded as a best) via
// ULevelClearTimeSubsystem::DiscardLevelTimer. Also confirms a further hit at
// already-0 energy does not re-fire OnLevelFailed.
//
// GetGameInstance() is null in this project's CreateNewMap()-based Automation test
// worlds (see KrowdKontrolLevelClearTimeSubsystemTest.cpp's own rationale), so a
// directly-constructed ULevelClearTimeSubsystem is injected into the controller's
// private CachedLevelClearTimeSubsystem via the FKrowdKontrolLevelFailedTest
// friendship, mirroring FKrowdKontrolGizmoFirstContactComponentTest's identical
// precedent.
//
// ULevelFailComponent::OnLevelFailed is a dynamic multicast delegate broadcast
// straight to an AActor target (AKrowdKontrolPlayerController::HandleLevelFailed) -
// that silently no-ops unless World->AreActorsInitialized() is true, since
// AActor::ProcessEvent gates reflection-dispatched calls on it. Same underlying
// "actor not initialized for play" gotcha KrowdKontrolDualZoneBossTest.cpp and
// KrowdKontrolMusicSubsystemTest.cpp already document; fixed the same way:
// World->InitializeActorsForPlay(FURL()) up front, before spawning any actors.
// SetBegunPlay(true) is intentionally NOT called here (matching
// KrowdKontrolDualZoneBossTest.cpp's rationale) - this test drives BeginPlay
// explicitly via DispatchBeginPlay() after possession.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "KrowdKontrolPlayerController.h"
#include "LevelClearTimeSubsystem.h"
#include "LevelFailComponent.h"
#include "LevelFailedTestListener.h"
#include "FlatCamera3DPrototypePawn.h"
#include "Paper2DPrototypePawn.h"
#include "PlayerEnergyComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolLevelFailedTest,
	"KrowdKontrol.Unit.LevelFailed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolLevelFailedTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	// Required for ULevelFailComponent::OnLevelFailed's dynamic-delegate-bound
	// AActor handler (AKrowdKontrolPlayerController::HandleLevelFailed) to actually
	// fire on Broadcast() - see the file comment above.
	World->InitializeActorsForPlay(FURL());

	AFlatCamera3DPrototypePawn* Pawn = World->SpawnActor<AFlatCamera3DPrototypePawn>();
	if (!TestNotNull(TEXT("Pawn should spawn"), Pawn))
	{
		return false;
	}

	AKrowdKontrolPlayerController* Controller = World->SpawnActor<AKrowdKontrolPlayerController>();
	if (!TestNotNull(TEXT("Controller should spawn"), Controller))
	{
		return false;
	}

	Controller->Possess(Pawn);
	Controller->Player = NewObject<ULocalPlayer>(GEngine);
	Controller->SetAsLocalPlayerController();

	// GetGameInstance() is null in this CreateNewMap() World - inject a
	// directly-constructed subsystem via friendship, mirroring
	// FKrowdKontrolGizmoFirstContactComponentTest's identical precedent. Injected
	// before DispatchBeginPlay() (issue #170) so BeginPlay()'s own
	// ResolveLevelClearTimeSubsystem()/SubscribeToLevelLifecycle() call finds it
	// already cached instead of hitting the no-subsystem warning path - a real
	// GameInstance-resolved subsystem would likewise already exist before BeginPlay
	// runs.
	UGameInstance* GameInstanceOuter = NewObject<UGameInstance>();
	ULevelClearTimeSubsystem* Subsystem = NewObject<ULevelClearTimeSubsystem>(GameInstanceOuter);
	Controller->CachedLevelClearTimeSubsystem = Subsystem;

	Controller->DispatchBeginPlay();

	const FName LevelID = FName(*World->GetMapName());
	Subsystem->StartLevelTimer(LevelID);

	UPlayerEnergyComponent* Energy = Pawn->FindComponentByClass<UPlayerEnergyComponent>();
	if (!TestNotNull(TEXT("Pawn should have a PlayerEnergyComponent"), Energy))
	{
		return false;
	}

	ULevelFailComponent* LevelFailComp = Pawn->FindComponentByClass<ULevelFailComponent>();
	if (!TestNotNull(TEXT("Pawn should have a LevelFailComponent"), LevelFailComp))
	{
		return false;
	}

	ULevelFailedTestListener* Listener = NewObject<ULevelFailedTestListener>();
	LevelFailComp->OnLevelFailed.AddDynamic(Listener, &ULevelFailedTestListener::HandleLevelFailed);

	TestTrue(TEXT("Pawn input should start enabled"), Pawn->InputEnabled());

	// Deterministic single-call floor to exactly 0, mirroring
	// KrowdKontrolPlayerEnergyComponentTest.cpp's own case (c).
	Energy->CurrentEnergy = 5.0f;
	Energy->ApplyContactDamage(10.0f, nullptr);

	TestEqual(TEXT("OnLevelFailed should fire exactly once"), Listener->CallCount, 1);
	TestFalse(TEXT("Pawn input should be disabled after level failure"), Pawn->InputEnabled());

	float OutBest = 0.0f;
	TestFalse(TEXT("No best time should be recorded - the failed run's timer must be discarded, not recorded"),
		Subsystem->GetBestClearTimeSeconds(LevelID, OutBest));

	// A further hit at already-0 energy must not re-fire OnLevelFailed - OnEnergyChanged
	// itself does not re-fire at the floor (see PlayerEnergyComponentTest case (d)).
	Energy->ApplyContactDamage(10.0f, nullptr);
	TestEqual(TEXT("A further hit at 0 energy should not re-fire OnLevelFailed"), Listener->CallCount, 1);

	// (b) With no possessed pawn and no CachedLevelClearTimeSubsystem injected,
	// ResolveLevelClearTimeSubsystem() falls through to GetGameInstance() - null in this
	// CreateNewMap() World - and HandleLevelFailed must degrade safely: no crash, and the
	// warning logs exactly once (warn-once). Mirrors
	// KrowdKontrolGizmoFirstContactComponentTest.cpp's case (e) and this file's own
	// possessed-pawn happy path above, minus the pawn - deliberately not spawning a
	// second AutoPossessPlayer pawn/controller pair here to keep this case isolated from
	// the possession machinery the happy path above already exercises.
	AKrowdKontrolPlayerController* UnresolvedController = World->SpawnActor<AKrowdKontrolPlayerController>();
	if (!TestNotNull(TEXT("A second AKrowdKontrolPlayerController should spawn"), UnresolvedController))
	{
		return false;
	}

	// SetAsLocalPlayerController() alone only flips bIsLocalPlayerController - it does
	// NOT attach a UPlayer, which CreateHUDWidgets() (called from BeginPlay(), below) hard-requires,
	// mirroring this file's own happy-path Controller setup above. Unrelated to the
	// deliberate no-possession isolation noted above - this is only to satisfy
	// CreateHUDWidgets()'s precondition, not to exercise possession machinery.
	UnresolvedController->Player = NewObject<ULocalPlayer>(GEngine);
	UnresolvedController->SetAsLocalPlayerController();

	// BeginPlay() (issue #170) now also calls ResolveLevelClearTimeSubsystem() to wire
	// SubscribeToLevelLifecycle() - with no CachedLevelClearTimeSubsystem injected and no
	// GetGameInstance() in this CreateNewMap() World, BeginPlay() itself is what "claims"
	// the warn-once flag now, before HandleLevelFailed() below ever gets a chance to. The
	// single expected-error count of 1 below (unchanged) covers both calls together,
	// proving the flag isn't silently double-consumed or re-logged from the second caller.
	AddExpectedError(TEXT("no ULevelClearTimeSubsystem available"), EAutomationExpectedErrorFlags::Contains, 1);
	UnresolvedController->DispatchBeginPlay();
	TestTrue(TEXT("BeginPlay must not crash with no resolvable subsystem"), true);

	UnresolvedController->HandleLevelFailed();
	TestTrue(TEXT("HandleLevelFailed must not crash with no possessed pawn and no resolvable subsystem"), true);

	// A second call must not log the warning again (warn-once).
	UnresolvedController->HandleLevelFailed();

	// (d) Real pawn-constructor wiring for the second prototype pawn: APaper2DPrototypePawn's
	// constructor has a textually near-identical LevelFailComponent construct+bind to
	// AFlatCamera3DPrototypePawn's - a copy-paste slip there (wrong instance/method/delegate)
	// would compile cleanly and every case above would still pass, since none of them spawn
	// this pawn class. Drives the same core scenario through the real pawn.
	//
	// AlwaysSpawn collision override: this World had InitializeActorsForPlay(FURL()) called
	// on it up front (required for the dynamic-delegate dispatch above), which activates real
	// collision checks - the default origin spawn location already has the first Pawn's
	// StaticMeshComponent sitting there, and the default spawn-collision handling would refuse
	// to spawn a second actor on top of it.
	FActorSpawnParameters Paper2DSpawnParams;
	Paper2DSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APaper2DPrototypePawn* Paper2DPawn = World->SpawnActor<APaper2DPrototypePawn>(
		APaper2DPrototypePawn::StaticClass(), FTransform::Identity, Paper2DSpawnParams);
	if (!TestNotNull(TEXT("APaper2DPrototypePawn should spawn"), Paper2DPawn))
	{
		return false;
	}
	ULevelFailComponent* Paper2DLevelFailComp = Paper2DPawn->FindComponentByClass<ULevelFailComponent>();
	if (!TestNotNull(TEXT("Paper2DPrototypePawn should have a LevelFailComponent"), Paper2DLevelFailComp))
	{
		return false;
	}
	UPlayerEnergyComponent* Paper2DEnergy = Paper2DPawn->FindComponentByClass<UPlayerEnergyComponent>();
	if (!TestNotNull(TEXT("Paper2DPrototypePawn should have a PlayerEnergyComponent"), Paper2DEnergy))
	{
		return false;
	}
	ULevelFailedTestListener* Paper2DListener = NewObject<ULevelFailedTestListener>();
	Paper2DLevelFailComp->OnLevelFailed.AddDynamic(Paper2DListener, &ULevelFailedTestListener::HandleLevelFailed);
	Paper2DEnergy->CurrentEnergy = 5.0f;
	Paper2DEnergy->ApplyContactDamage(10.0f, nullptr);
	TestEqual(TEXT("Paper2DPrototypePawn's LevelFailComponent should also fire on zero energy"),
		Paper2DListener->CallCount, 1);

	// (e) AKrowdKontrolPlayerController::Cheat_ZeroPlayerEnergy() (issue #183 pass-1
	// feedback): a QA/E2E hook that must reach the same zero-energy precondition purely
	// through ApplyContactDamage() - never a direct setter - and drive the same
	// OnLevelFailed/DisableInput live path as real combat damage. Fresh pawn+controller
	// pair so this doesn't ride on the already-zeroed energy above.
	FActorSpawnParameters CheatSpawnParams;
	CheatSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AFlatCamera3DPrototypePawn* CheatPawn = World->SpawnActor<AFlatCamera3DPrototypePawn>(
		AFlatCamera3DPrototypePawn::StaticClass(), FTransform::Identity, CheatSpawnParams);
	if (!TestNotNull(TEXT("Cheat-command pawn should spawn"), CheatPawn))
	{
		return false;
	}
	AKrowdKontrolPlayerController* CheatController = World->SpawnActor<AKrowdKontrolPlayerController>();
	if (!TestNotNull(TEXT("Cheat-command controller should spawn"), CheatController))
	{
		return false;
	}
	CheatController->Possess(CheatPawn);
	CheatController->Player = NewObject<ULocalPlayer>(GEngine);
	CheatController->SetAsLocalPlayerController();

	// Same injection as the happy-path Controller above (GetGameInstance() is null in
	// this CreateNewMap() World), and same reason it's injected before
	// DispatchBeginPlay() (issue #170) - otherwise HandleLevelFailed's no-subsystem
	// warning would fire again here (now also reachable from BeginPlay() itself, not
	// just a direct HandleLevelFailed() call) and break the UnresolvedController
	// warn-once count check earlier in this test.
	UGameInstance* CheatGameInstanceOuter = NewObject<UGameInstance>();
	CheatController->CachedLevelClearTimeSubsystem = NewObject<ULevelClearTimeSubsystem>(CheatGameInstanceOuter);

	CheatController->DispatchBeginPlay();

	UPlayerEnergyComponent* CheatEnergy = CheatPawn->FindComponentByClass<UPlayerEnergyComponent>();
	if (!TestNotNull(TEXT("Cheat-command pawn should have a PlayerEnergyComponent"), CheatEnergy))
	{
		return false;
	}
	ULevelFailComponent* CheatLevelFailComp = CheatPawn->FindComponentByClass<ULevelFailComponent>();
	if (!TestNotNull(TEXT("Cheat-command pawn should have a LevelFailComponent"), CheatLevelFailComp))
	{
		return false;
	}
	ULevelFailedTestListener* CheatListener = NewObject<ULevelFailedTestListener>();
	CheatLevelFailComp->OnLevelFailed.AddDynamic(CheatListener, &ULevelFailedTestListener::HandleLevelFailed);

	TestTrue(TEXT("Cheat-command pawn input should start enabled"), CheatPawn->InputEnabled());
	CheatController->Cheat_ZeroPlayerEnergy();

	TestEqual(TEXT("Cheat_ZeroPlayerEnergy should drain energy to exactly 0"), CheatEnergy->GetCurrentEnergy(), 0.0f);
	TestEqual(TEXT("Cheat_ZeroPlayerEnergy should trigger OnLevelFailed exactly once"), CheatListener->CallCount, 1);
	TestFalse(TEXT("Cheat_ZeroPlayerEnergy should disable pawn input"), CheatPawn->InputEnabled());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
