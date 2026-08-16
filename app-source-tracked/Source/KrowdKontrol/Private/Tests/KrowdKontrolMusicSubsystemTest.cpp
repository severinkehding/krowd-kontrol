// Confirms UMusicSubsystem (issue #25, PRD 12) crossfades correctly off any
// AEnemyBase's IThreatState: calm by default, switches to Combat the instant any
// enemy in the world is Hot (Alert/Attack/Controlled), reverts to Calm only once
// every enemy is Idle/Banked again - "any enemy", not "the last enemy checked",
// proven with two simultaneous enemies. RefreshMusicState() is called directly
// (never via a real Tick() loop) for the same synchronous-determinism reasons
// KrowdKontrolEnemyBaseTest.cpp/KrowdKontrolGizmoNarrativeSubsystemTest.cpp document -
// this repo has no AutomationSpec/latent-command test anywhere, and a fade's real
// elapsed time is never asserted, only the discrete CurrentState/delegate transition.
//
// Needs a real UWorld (FAutomationEditorCommonUtils::CreateNewMap()), unlike
// KrowdKontrolGizmoNarrativeSubsystemTest.cpp's bare NewObject<>() subsystem
// construction: UMusicSubsystem is a UWorldSubsystem, auto-instantiated per-UWorld,
// and IsAnyEnemyInCombat() needs a real World for TActorIterator<AEnemyBase> to find
// spawned enemies. Mirrors KrowdKontrolEnemyBaseTest.cpp cases (k)/(m)'s
// CreateNewMap()+SpawnActor pattern.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "MusicSubsystem.h"
#include "MusicStateTestListener.h"
#include "EnemyBaseTestActor.h"
#include "AbilitySlot.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Sound/SoundWave.h"
#include "Components/AudioComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolMusicSubsystemTest,
	"KrowdKontrol.Unit.MusicSubsystem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolMusicSubsystemTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	UMusicSubsystem* MusicSubsystem = World->GetSubsystem<UMusicSubsystem>();
	if (!TestNotNull(TEXT("UWorld should auto-instantiate UMusicSubsystem"), MusicSubsystem))
	{
		return false;
	}

	UMusicStateTestListener* Listener = NewObject<UMusicStateTestListener>();
	MusicSubsystem->OnMusicStateChanged.AddDynamic(Listener, &UMusicStateTestListener::HandleMusicStateChanged);
	Listener->WatchedSubsystem = MusicSubsystem;

	// (a) default state, before any enemy exists or any refresh runs.
	TestEqual(TEXT("Default music state should be Calm"),
		static_cast<uint8>(MusicSubsystem->GetMusicState()), static_cast<uint8>(EMusicState::Calm));

	// (b) an Idle enemy in the world does not trigger Combat.
	AEnemyBaseTestActor* FirstEnemy = World->SpawnActor<AEnemyBaseTestActor>();
	if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), FirstEnemy))
	{
		return false;
	}
	MusicSubsystem->RefreshMusicState();
	TestEqual(TEXT("An Idle-only enemy should leave music state at Calm"),
		static_cast<uint8>(MusicSubsystem->GetMusicState()), static_cast<uint8>(EMusicState::Calm));
	TestEqual(TEXT("OnMusicStateChanged should not have fired yet"), Listener->CallCount, 0);

	// (c) Idle -> Alert (Hot) switches to Combat and broadcasts exactly once.
	const FVector ZeroDistanceLocation(0.0f, 0.0f, 0.0f);
	FirstEnemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	MusicSubsystem->RefreshMusicState();
	TestEqual(TEXT("An Alert (Hot) enemy should switch music state to Combat"),
		static_cast<uint8>(MusicSubsystem->GetMusicState()), static_cast<uint8>(EMusicState::Combat));
	TestEqual(TEXT("OnMusicStateChanged should have fired once"), Listener->CallCount, 1);
	TestEqual(TEXT("Broadcast should carry Combat"),
		static_cast<uint8>(Listener->LastState), static_cast<uint8>(EMusicState::Combat));
	TestEqual(TEXT("GetMusicState() read from inside the broadcast should already reflect the new state (flip-before-broadcast)"),
		static_cast<uint8>(Listener->ObservedStateDuringBroadcast), static_cast<uint8>(EMusicState::Combat));

	// (d) a refresh with no underlying state change must not re-broadcast.
	MusicSubsystem->RefreshMusicState();
	TestEqual(TEXT("A no-op refresh should not re-fire OnMusicStateChanged"), Listener->CallCount, 1);

	// (e) Alert -> Attack (still Hot) stays Combat, no extra broadcast.
	FirstEnemy->TickCheckDetection(ZeroDistanceLocation); // Alert -> Attack
	MusicSubsystem->RefreshMusicState();
	TestEqual(TEXT("Attack is still Hot - music state should remain Combat"),
		static_cast<uint8>(MusicSubsystem->GetMusicState()), static_cast<uint8>(EMusicState::Combat));
	TestEqual(TEXT("CallCount should still be 1"), Listener->CallCount, 1);

	// (f) Attack -> Controlled (via the public ReceiveControl API, still Hot) stays Combat.
	FirstEnemy->ReceiveControl(EAbilitySlot::Stun);
	MusicSubsystem->RefreshMusicState();
	TestEqual(TEXT("Controlled is still Hot - music state should remain Combat"),
		static_cast<uint8>(MusicSubsystem->GetMusicState()), static_cast<uint8>(EMusicState::Combat));
	TestEqual(TEXT("CallCount should still be 1"), Listener->CallCount, 1);

	// (g) Controlled -> Banked (Idle again, via the public TransitionToBanked API)
	// reverts to Calm.
	FirstEnemy->TransitionToBanked();
	MusicSubsystem->RefreshMusicState();
	TestEqual(TEXT("A Banked-only enemy should revert music state to Calm"),
		static_cast<uint8>(MusicSubsystem->GetMusicState()), static_cast<uint8>(EMusicState::Calm));
	TestEqual(TEXT("OnMusicStateChanged should have fired a second time"), Listener->CallCount, 2);
	TestEqual(TEXT("Second broadcast should carry Calm"),
		static_cast<uint8>(Listener->LastState), static_cast<uint8>(EMusicState::Calm));

	// (h) aggregation across multiple enemies: a second, freshly-Alert enemy switches
	// back to Combat even though the first enemy is Banked (Idle) - proves "any enemy
	// Hot", not "only the most-recently-checked enemy".
	AEnemyBaseTestActor* SecondEnemy = World->SpawnActor<AEnemyBaseTestActor>();
	if (!TestNotNull(TEXT("Second AEnemyBaseTestActor should spawn into the test World"), SecondEnemy))
	{
		return false;
	}
	SecondEnemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	MusicSubsystem->RefreshMusicState();
	TestEqual(TEXT("A second Hot enemy should switch music state back to Combat"),
		static_cast<uint8>(MusicSubsystem->GetMusicState()), static_cast<uint8>(EMusicState::Combat));
	TestEqual(TEXT("OnMusicStateChanged should have fired a third time"), Listener->CallCount, 3);

	// (i) banking the second (and now only remaining Hot) enemy reverts to Calm again.
	SecondEnemy->ReceiveControl(EAbilitySlot::Root);
	SecondEnemy->TransitionToBanked();
	MusicSubsystem->RefreshMusicState();
	TestEqual(TEXT("Both enemies Banked should revert music state to Calm"),
		static_cast<uint8>(MusicSubsystem->GetMusicState()), static_cast<uint8>(EMusicState::Calm));
	TestEqual(TEXT("OnMusicStateChanged should have fired a fourth time"), Listener->CallCount, 4);

	// (j) with real tracks configured, a Calm->Combat->Calm round trip actually spawns
	// audio and crossfades, rather than leaving CurrentMusicComponent null/unchanged -
	// cases (a)-(i) above all run with CalmTrack/CombatTrack unset, so none of them
	// exercise SpawnSound2D/FadeIn/FadeOut at all. NewObject<USoundWave>() (no
	// .uasset) is sufficient: USoundWave is the concrete, non-abstract USoundBase
	// subclass (USoundBase itself can't be NewObject<>()'d), and
	// TSoftObjectPtr::LoadSynchronous() resolves an already-in-memory UObject via
	// FindObject before it would ever try to load from disk.
	MusicSubsystem->CalmTrack = NewObject<USoundWave>();
	MusicSubsystem->CombatTrack = NewObject<USoundWave>();

	AEnemyBaseTestActor* ThirdEnemy = World->SpawnActor<AEnemyBaseTestActor>();
	if (!TestNotNull(TEXT("Third AEnemyBaseTestActor should spawn into the test World"), ThirdEnemy))
	{
		return false;
	}
	ThirdEnemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert (Hot)
	MusicSubsystem->RefreshMusicState();
	UAudioComponent* CombatComponent = MusicSubsystem->CurrentMusicComponent;
	TestNotNull(TEXT("A configured CombatTrack should spawn a UAudioComponent on switch"),
		CombatComponent);

	ThirdEnemy->ReceiveControl(EAbilitySlot::Stun);
	ThirdEnemy->TransitionToBanked();
	MusicSubsystem->RefreshMusicState();
	UAudioComponent* CalmComponent = MusicSubsystem->CurrentMusicComponent;
	TestNotNull(TEXT("A configured CalmTrack should spawn a new UAudioComponent on switch back"),
		CalmComponent);
	TestTrue(TEXT("Switching tracks should replace the AudioComponent instance, not reuse/mutate it"),
		CalmComponent != CombatComponent);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
