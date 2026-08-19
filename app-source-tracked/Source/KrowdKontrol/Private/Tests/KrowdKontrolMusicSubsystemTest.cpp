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
#include "SleepShieldBoss.h"
#include "DualZoneBoss.h"
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

	// (k) the real Config-driven soft-object-ptr path (CalmTrack/CombatTrack pointing
	// at the same shipping asset paths DefaultGame.ini configures, resolved via
	// LoadSynchronous() rather than (j)'s NewObject<USoundWave>() injection) must also
	// spawn a persistent, looping AudioComponent. This is the exact path a live PIE
	// session exercises and that a prior E2E pass caught a regression in - (j)'s
	// injected in-memory USoundWave happens to default to non-looping too, but only
	// resolving the real Content asset proves PlayTrackForState()'s forced-looping fix
	// actually reaches the SoundWave that DefaultGame.ini configures. These paths must
	// stay in sync with DefaultGame.ini's [/Script/KrowdKontrol.MusicSubsystem]
	// section - a prior review pass caught them pointing at stale, never-imported
	// placeholder assets while the game shipped different ones, which silently turned
	// this case into a test of nothing.
	MusicSubsystem->CalmTrack = TSoftObjectPtr<USoundBase>(
		FSoftObjectPath(TEXT("/Game/Audio/Music/CalmTrack.CalmTrack")));
	MusicSubsystem->CombatTrack = TSoftObjectPtr<USoundBase>(
		FSoftObjectPath(TEXT("/Game/Audio/Music/CombatTrack.CombatTrack")));

	AEnemyBaseTestActor* FourthEnemy = World->SpawnActor<AEnemyBaseTestActor>();
	if (!TestNotNull(TEXT("Fourth AEnemyBaseTestActor should spawn into the test World"), FourthEnemy))
	{
		return false;
	}
	FourthEnemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert (Hot)
	MusicSubsystem->RefreshMusicState();
	UAudioComponent* RealCombatComponent = MusicSubsystem->CurrentMusicComponent;
	if (!TestNotNull(TEXT("The real Config-driven CombatTrack path should spawn a UAudioComponent"), RealCombatComponent))
	{
		return false;
	}
	USoundWave* RealCombatWave = Cast<USoundWave>(RealCombatComponent->Sound);
	if (TestNotNull(TEXT("The real CombatTrack asset should resolve to a USoundWave"), RealCombatWave))
	{
		TestTrue(TEXT("Music must be forced to loop so it persists for as long as the state holds, not stop after one playthrough"),
			RealCombatWave->bLooping);
	}

	// (l) natural reversion: the Hot enemy simply ceasing to exist (destroyed/
	// despawned - death, room unload, wave cleanup) must revert music to Calm. This
	// is a genuinely different code path from (g)/(i)'s Banked-pacification
	// reversion: there the enemy still exists and reports Idle; here
	// IsAnyEnemyInCombat()'s TActorIterator must correctly see no enemy at all.
	// Note: AEnemyBase currently has NO Alert->Idle de-detection transition
	// (TickCheckDetection only escalates), so an enemy "losing sight of the player"
	// cannot yet revert music - destruction and banking are the only two reversion
	// paths that exist at runtime today. If de-detection is ever added to the enemy
	// state machine, add a case here driving it.
	const int32 CallCountBeforeDestroy = Listener->CallCount;
	World->DestroyActor(FourthEnemy);
	MusicSubsystem->RefreshMusicState();
	TestEqual(TEXT("Destroying the only Hot enemy should revert music state to Calm"),
		static_cast<uint8>(MusicSubsystem->GetMusicState()), static_cast<uint8>(EMusicState::Calm));
	TestEqual(TEXT("The destruction-driven reversion should broadcast exactly once"),
		Listener->CallCount, CallCountBeforeDestroy + 1);
	TestEqual(TEXT("The reversion broadcast should carry Calm"),
		static_cast<uint8>(Listener->LastState), static_cast<uint8>(EMusicState::Calm));

	// (m) a boss reaching Armed with no other enemies and no twist telegraphed yet
	// establishes the Combat baseline on its own (AC #2's "standard combat track"
	// precondition) - ABossBase is not an AEnemyBase, so without this,
	// IsAnyEnemyInCombat() alone would leave a boss-only fight silently at Calm.
	ASleepShieldBoss* ShieldBoss = World->SpawnActor<ASleepShieldBoss>();
	if (!TestNotNull(TEXT("ASleepShieldBoss should spawn into the test World"), ShieldBoss))
	{
		return false;
	}
	ShieldBoss->DispatchBeginPlay();
	MusicSubsystem->RefreshMusicState();
	TestEqual(TEXT("A boss reaching Armed (shield up, not yet telegraphing) should establish the Combat baseline"),
		static_cast<uint8>(MusicSubsystem->GetMusicState()), static_cast<uint8>(EMusicState::Combat));

	// (n) the shield dropping (this boss's twist telegraph) switches to BossIntensity.
	AEnemyBaseTestActor* SleepMinion = World->SpawnActor<AEnemyBaseTestActor>();
	if (!TestNotNull(TEXT("Sleep-controlled minion should spawn"), SleepMinion))
	{
		return false;
	}
	SleepMinion->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	SleepMinion->ReceiveControl(EAbilitySlot::Sleep); // Alert -> Controlled
	SleepMinion->SetActorLocation(ShieldBoss->GetActorLocation());
	ShieldBoss->CheckShieldState();
	const int32 CallCountBeforeTelegraph = Listener->CallCount;
	MusicSubsystem->RefreshMusicState();
	TestEqual(TEXT("Shield dropping should switch music state to BossIntensity"),
		static_cast<uint8>(MusicSubsystem->GetMusicState()), static_cast<uint8>(EMusicState::BossIntensity));
	TestEqual(TEXT("The telegraph transition should broadcast exactly once"),
		Listener->CallCount, CallCountBeforeTelegraph + 1);
	TestEqual(TEXT("Broadcast should carry BossIntensity"),
		static_cast<uint8>(Listener->LastState), static_cast<uint8>(EMusicState::BossIntensity));
	TestEqual(TEXT("GetMusicState() inside the broadcast should already reflect BossIntensity (flip-before-broadcast)"),
		static_cast<uint8>(Listener->ObservedStateDuringBroadcast), static_cast<uint8>(EMusicState::BossIntensity));

	// (n2) BossIntensityTrack resolves and spawns real, looping audio through the same
	// crossfade path as CalmTrack/CombatTrack - mirrors case (j) above (in-memory
	// injection, not (k)'s real-asset-path resolution: unlike CalmTrack/CombatTrack,
	// no BossIntensityTrack.uasset has been imported into Content/Audio/Music yet,
	// even though DefaultGame.ini already configures the path for when it lands - see
	// app-changelog/issue-41.md's "Known Follow-up" section. Asserting against that path today
	// would be exactly the "test of nothing" failure mode (k)'s own comment warns
	// about, just inverted: LoadSynchronous() would reliably return null and the test
	// would either be written to expect null (proving nothing about real resolution)
	// or fail until someone imports the asset). Re-trigger via a fresh telegraph edge
	// so SetMusicState() doesn't no-op.
	MusicSubsystem->BossIntensityTrack = NewObject<USoundWave>();
	SleepMinion->SetActorLocation(ShieldBoss->GetActorLocation() + FVector(ShieldBoss->ShieldDropRadiusUnits * 10.0f, 0.0f, 0.0f));
	ShieldBoss->CheckShieldState(); // shield re-raises -> Combat
	MusicSubsystem->RefreshMusicState();
	SleepMinion->SetActorLocation(ShieldBoss->GetActorLocation());
	ShieldBoss->CheckShieldState(); // shield drops again -> BossIntensity, now with a track configured
	MusicSubsystem->RefreshMusicState();
	UAudioComponent* BossIntensityComponent = MusicSubsystem->CurrentMusicComponent;
	if (TestNotNull(TEXT("A configured BossIntensityTrack should spawn a UAudioComponent on switch"), BossIntensityComponent))
	{
		USoundWave* BossIntensityWave = Cast<USoundWave>(BossIntensityComponent->Sound);
		if (TestNotNull(TEXT("The BossIntensityTrack asset should resolve to a USoundWave"), BossIntensityWave))
		{
			TestTrue(TEXT("BossIntensity music must also be forced to loop, same as Calm/Combat"),
				BossIntensityWave->bLooping);
		}
	}

	// (o) the shield re-raising once the minion leaves reverts to Combat (the
	// "standard track"), not all the way to Calm - the boss is still engaged
	// (Vulnerable), matching AC #3's "revert... once the twist-mechanic window ends".
	SleepMinion->SetActorLocation(ShieldBoss->GetActorLocation() + FVector(ShieldBoss->ShieldDropRadiusUnits * 10.0f, 0.0f, 0.0f));
	ShieldBoss->CheckShieldState();
	const int32 CallCountBeforeReraise = Listener->CallCount;
	MusicSubsystem->RefreshMusicState();
	TestEqual(TEXT("Shield re-raising should revert music state to Combat, not Calm, while the boss is still engaged"),
		static_cast<uint8>(MusicSubsystem->GetMusicState()), static_cast<uint8>(EMusicState::Combat));
	TestEqual(TEXT("The reversion-to-Combat transition should broadcast exactly once"),
		Listener->CallCount, CallCountBeforeReraise + 1);
	TestEqual(TEXT("Broadcast should carry Combat"),
		static_cast<uint8>(Listener->LastState), static_cast<uint8>(EMusicState::Combat));

	// (p) the fight ending (Banked) reverts fully to Calm, matching AC #3's "or the
	// fight ends" clause.
	World->DestroyActor(SleepMinion);
	ShieldBoss->TransitionToBanked();
	const int32 CallCountBeforeShieldBossBanked = Listener->CallCount;
	MusicSubsystem->RefreshMusicState();
	TestEqual(TEXT("A boss reaching Banked with no other enemies should revert music state to Calm"),
		static_cast<uint8>(MusicSubsystem->GetMusicState()), static_cast<uint8>(EMusicState::Calm));
	TestEqual(TEXT("The Banked reversion should broadcast exactly once"),
		Listener->CallCount, CallCountBeforeShieldBossBanked + 1);
	TestEqual(TEXT("Broadcast should carry Calm"),
		static_cast<uint8>(Listener->LastState), static_cast<uint8>(EMusicState::Calm));

	// (q) ADualZoneBoss's Enrage is a second, independent telegraph signal (not
	// shield-based) that must also drive BossIntensity - proves IsTwistTelegraphed()'s
	// per-boss-subclass override shape, not just ASleepShieldBoss's one path.
	// ADualZoneBoss::BeginPlay() requires the world to have begun play for its
	// component/tick registration; ZoneA/ZoneB are intentionally left unwired below
	// so the delegate-binding branch itself isn't exercised by this test.
	World->InitializeActorsForPlay(FURL());
	ADualZoneBoss* ZoneBoss = World->SpawnActor<ADualZoneBoss>();
	if (!TestNotNull(TEXT("ADualZoneBoss should spawn into the test World"), ZoneBoss))
	{
		return false;
	}
	ZoneBoss->DispatchBeginPlay(); // ZoneA/ZoneB left unwired - not needed to drive Enrage directly
	MusicSubsystem->RefreshMusicState();
	TestEqual(TEXT("ADualZoneBoss reaching Armed (not yet enraged) should hold the Combat baseline"),
		static_cast<uint8>(MusicSubsystem->GetMusicState()), static_cast<uint8>(EMusicState::Combat));

	ZoneBoss->SetIsEnraged(true);
	const int32 CallCountBeforeEnrage = Listener->CallCount;
	MusicSubsystem->RefreshMusicState();
	TestEqual(TEXT("ADualZoneBoss enraging should switch music state to BossIntensity"),
		static_cast<uint8>(MusicSubsystem->GetMusicState()), static_cast<uint8>(EMusicState::BossIntensity));
	TestEqual(TEXT("The enrage-driven telegraph transition should broadcast exactly once"),
		Listener->CallCount, CallCountBeforeEnrage + 1);
	TestEqual(TEXT("Broadcast should carry BossIntensity"),
		static_cast<uint8>(Listener->LastState), static_cast<uint8>(EMusicState::BossIntensity));

	// (r) ADualZoneBoss's fight ending also reverts to Calm - Enrage has no natural
	// un-trigger (see DualZoneBoss.cpp), so this proves reversion works generically
	// off GetBossState() == Banked, not off a boss-specific flag clearing.
	ZoneBoss->AdvanceToVulnerable();
	ZoneBoss->TransitionToBanked();
	const int32 CallCountBeforeZoneBossBanked = Listener->CallCount;
	MusicSubsystem->RefreshMusicState();
	TestEqual(TEXT("ADualZoneBoss reaching Banked should revert music state to Calm even though IsEnraged() stays true forever"),
		static_cast<uint8>(MusicSubsystem->GetMusicState()), static_cast<uint8>(EMusicState::Calm));
	TestEqual(TEXT("The Banked reversion should broadcast exactly once"),
		Listener->CallCount, CallCountBeforeZoneBossBanked + 1);
	TestEqual(TEXT("Broadcast should carry Calm"),
		static_cast<uint8>(Listener->LastState), static_cast<uint8>(EMusicState::Calm));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
