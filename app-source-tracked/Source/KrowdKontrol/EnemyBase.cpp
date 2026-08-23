#include "EnemyBase.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "PlayerEnergyComponent.h"
#include "EngineUtils.h"
#include "Components/PointLightComponent.h"
#include "AbilityData.h"
#include "CrowdMasterySubsystem.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "RoomActor.h"
#include "CoreGlobals.h"
#include "ControlledDurationIndicatorComponent.h"

AEnemyBase::AEnemyBase()
{
	// Unlike ABossBase (never ticks; every transition is externally driven),
	// AEnemyBase must self-detect the player via a per-tick proximity check, so
	// ticking is required here. This divergence from the mirrored BossBase pattern is
	// deliberate, not an oversight - see issue #12.
	PrimaryActorTick.bCanEverTick = true;

	ControlledDurationIndicatorComponent = CreateDefaultSubobject<UControlledDurationIndicatorComponent>(TEXT("ControlledDurationIndicatorComponent"));
}

void AEnemyBase::BeginPlay()
{
	Super::BeginPlay();

	// Issue #211: every concrete subclass's own mesh root defaults to the engine's
	// BlockAllDynamic profile (Block response to the WorldDynamic channel). An
	// ATargetZone is also WorldDynamic ("OverlapAllDynamic" profile), and Unreal
	// always resolves a Block-vs-Overlap pair as a blocking collision - never an
	// overlap event - regardless of which side blocks, so no regular enemy could
	// ever physically trigger a zone's OnActorBanked without this. Narrowed to just
	// the WorldDynamic channel (leaves the root's Block response to WorldStatic - the
	// room floor - untouched) and safe to set generically here rather than in every
	// concrete subclass's own constructor: this module has zero
	// OnComponentHit/NotifyHit consumers, so no other system depends on an enemy's
	// root actually blocking (vs overlapping) a WorldDynamic object.
	// NOTE: this is channel-wide (ECC_WorldDynamic) and actor-wide (every AEnemyBase),
	// not scoped to ATargetZone specifically - safe today only because no other system
	// sweeps an enemy against another WorldDynamic actor (TickChaseMovement's
	// SetActorLocation is unswept, and the OnComponentHit/NotifyHit/OnActorHit grep
	// above is empty). Re-check this line before adding enemy-vs-enemy physics,
	// SimulatePhysics on an enemy, or any new WorldDynamic-channel blocking actor.
	if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(GetRootComponent()))
	{
		RootPrimitive->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	}

	// Issue #174 AC1: routes every real Controlled-state expiry into the Crowd
	// Mastery running-max sample, the same production wiring
	// UCrowdMasterySubsystem.h's class comment already flagged as the missing piece.
	if (UWorld* World = GetWorld())
	{
		if (UCrowdMasterySubsystem* CrowdMasterySubsystem = World->GetSubsystem<UCrowdMasterySubsystem>())
		{
			OnEnemyControlledExpired.AddDynamic(CrowdMasterySubsystem, &UCrowdMasterySubsystem::HandleEnemyControlledExpired);
		}
	}
}

void AEnemyBase::ReceiveControl(EAbilitySlot Ability)
{
	if (CurrentState == EEnemyState::Controlled)
	{
		// Sleep-flavour early wake (issue #257, PRD "Ability Targeting Shapes &
		// Effect Semantics" REQ-2): being hit by any OTHER ability's application
		// while still Controlled by an ability flagged
		// bWakesEarlyOnOtherAbilityHit ends that Controlled window immediately -
		// reuses the exact Controlled->Alert edge + OnEnemyControlledExpired
		// broadcast TickControlledDuration's own natural-expiry path already
		// uses below, rather than inventing a second "how Controlled ends" path.
		// Re-casting the SAME ability that's already controlling this enemy
		// still no-ops here (Ability == ControllingAbility), matching the base
		// "no-op unless Alert/Attack" contract - there is no real gameplay path
		// that re-casts an ability on its own already-Controlled target today.
		if (Ability != ControllingAbility && AbilityData::Get(ControllingAbility).bWakesEarlyOnOtherAbilityHit)
		{
			CurrentState = EEnemyState::Alert;
			OnControlledExpired();
			OnEnemyControlledExpired.Broadcast();
			ControlledDurationIndicatorComponent->Hide();
		}
		return;
	}

	if (CurrentState != EEnemyState::Alert && CurrentState != EEnemyState::Attack)
	{
		return;
	}
	CurrentState = EEnemyState::Controlled;
	ControllingAbility = Ability;
	const float OverrideSeconds = GetControlledDurationOverrideSeconds(Ability);
	RemainingControlledSeconds = OverrideSeconds >= 0.0f ? OverrideSeconds : AbilityData::Get(Ability).BaseDurationSeconds;
	TotalControlledSeconds = RemainingControlledSeconds;
	OnControlledEntry(Ability);
	ControlledDurationIndicatorComponent->Show(AbilityData::Get(Ability).Colour);
}

bool AEnemyBase::IsAttackBehaviorActive() const
{
	if (CurrentState == EEnemyState::Attack)
	{
		return true;
	}
	return CurrentState == EEnemyState::Controlled && AbilityData::Get(ControllingAbility).bAllowsAttackWhileControlled;
}

bool AEnemyBase::IsMovementBehaviorActive() const
{
	if (CurrentState == EEnemyState::Alert)
	{
		return true;
	}
	return CurrentState == EEnemyState::Controlled && AbilityData::Get(ControllingAbility).bAllowsMovementWhileControlled;
}

float AEnemyBase::GetControlledSpeedMultiplier() const
{
	return CurrentState == EEnemyState::Controlled ? AbilityData::Get(ControllingAbility).ControlledSpeedMultiplier : 1.0f;
}

void AEnemyBase::TransitionToBanked()
{
	if (CurrentState != EEnemyState::Controlled)
	{
		return;
	}
	// Flip before broadcasting (see ABossBase::TransitionToBanked) so a listener that
	// re-enters from within OnEnemyBanked sees the terminal state immediately, and
	// this method is already a no-op for it.
	CurrentState = EEnemyState::Banked;
	ControlledDurationIndicatorComponent->Hide();
	OnEnemyBanked.Broadcast();
}

void AEnemyBase::AdvanceToAlert()
{
	if (CurrentState != EEnemyState::Idle)
	{
		return;
	}
	CurrentState = EEnemyState::Alert;
}

void AEnemyBase::AdvanceToAttack()
{
	if (CurrentState != EEnemyState::Alert)
	{
		return;
	}
	CurrentState = EEnemyState::Attack;
	OnAttackEntry();
}

bool AEnemyBase::IsPlayerInOwningRoom(const FVector& PlayerLocation) const
{
	ARoomActor* Room = OwningRoom.Get();
	if (!Room)
	{
		return true;
	}
	// Issue #245: while Room's first-entry countdown is actively running, hold the
	// same Idle->Alert gate closed regardless of player position - the issue's own
	// Notes section says this must extend REQ-2's existing gate, not add a second
	// one. Deliberately narrower than "not yet activated": a room whose countdown
	// hasn't started yet (CheckFirstEntry not yet called this tick, or never - e.g.
	// every pre-#245 test that drives TickCheckDetection directly) keeps the
	// unmodified #244 gate. See RoomActor.h's IsActivationPending() comment for the
	// full rationale. This enemy's own Tick() and ARoomActor::Tick() both run every
	// frame (issue #290 pass-1: ARoomActor's first-entry poll used to be throttled
	// to 0.25s, which left a same-magnitude window where this check saw
	// IsActivationPending() == false before the countdown had actually started -
	// see RoomActor.cpp's constructor comment), so the two loops now stay in sync
	// within the same frame rather than up to a quarter-second apart.
	if (Room->IsActivationPending())
	{
		return false;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return true;
	}
	return ARoomActor::FindNearestRoom(PlayerLocation, ARoomActor::GetCachedRoomList(World)) == Room;
}

void AEnemyBase::TickCheckDetection(const FVector& PlayerLocation)
{
	const float Distance = FVector::Dist(GetActorLocation(), PlayerLocation);
	// At most one state transition per call, via else-if: a very-close spawn advances
	// Idle->Alert this tick and Alert->Attack the next, never skipping Alert within a
	// single frame.
	if (CurrentState == EEnemyState::Idle && Distance <= DetectionRangeUnits)
	{
		// Issue #244: proximity alone is necessary but not sufficient for Idle->Alert
		// - the player must also be resolved to this enemy's own room (or the enemy
		// has no owning room at all, e.g. a level with zero ARoomActors).
		if (IsPlayerInOwningRoom(PlayerLocation))
		{
			AdvanceToAlert();
		}
	}
	else if (CurrentState == EEnemyState::Alert && Distance <= GetAttackRangeUnits())
	{
		AdvanceToAttack();
	}
}

void AEnemyBase::TickControlledDuration(float DeltaSeconds)
{
	if (CurrentState != EEnemyState::Controlled)
	{
		return;
	}
	RemainingControlledSeconds = FMath::Max(0.0f, RemainingControlledSeconds - DeltaSeconds);
	ControlledDurationIndicatorComponent->RefreshFillFraction();
	if (RemainingControlledSeconds <= 0.0f)
	{
		// Operator decision, issue #138, 2026-08-18: duration expiring before banking
		// breaks the enemy free - it reverts to Alert and re-engages, never staying
		// Controlled indefinitely and never treated as a kill (MISSION.md Hard
		// Invariant 2). Banking within the window remains the only path to Banked.
		CurrentState = EEnemyState::Alert;
		OnControlledExpired();
		OnEnemyControlledExpired.Broadcast();
		ControlledDurationIndicatorComponent->Hide();
	}
}

void AEnemyBase::TickChaseMovement(const FVector& PlayerLocation, float DeltaSeconds)
{
	if (!IsMovementBehaviorActive())
	{
		return;
	}
	const FVector ToPlayer = PlayerLocation - GetActorLocation();
	const float DistanceRemaining = ToPlayer.Size();
	if (DistanceRemaining <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	const float MoveDistance = FMath::Min(DistanceRemaining, GetEffectiveMovementSpeedUnitsPerSecond() * GetControlledSpeedMultiplier() * DeltaSeconds);
	SetActorLocation(GetActorLocation() + ToPlayer.GetSafeNormal() * MoveDistance);
}

void AEnemyBase::TickFleeMovement(const FVector& CasterLocation, float DeltaSeconds)
{
	if (CurrentState != EEnemyState::Controlled
		|| !AbilityData::Get(ControllingAbility).bFleesFromCasterWhileControlled)
	{
		return;
	}
	const FVector AwayFromCaster = GetActorLocation() - CasterLocation;
	if (AwayFromCaster.SizeSquared() <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	const float MoveDistance = GetEffectiveMovementSpeedUnitsPerSecond() * DeltaSeconds;
	SetActorLocation(GetActorLocation() + AwayFromCaster.GetSafeNormal() * MoveDistance);
}

void AEnemyBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Not gated on a live player pawn below - the Controlled-duration timer isn't
	// player-position-dependent, unlike detection/chase.
	TickControlledDuration(DeltaTime);

	// GetPlayerPawn returns nullptr in a headless Automation test with no
	// PlayerController spawned - this is fine and expected; TickCheckDetection and
	// TickChaseMovement are both friend-testable independently of a live Tick() call
	// for exactly this reason.
	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
	{
		const FVector PlayerLocation = PlayerPawn->GetActorLocation();
		TickCheckDetection(PlayerLocation);
		TickChaseMovement(PlayerLocation, DeltaTime);
		TickFleeMovement(PlayerLocation, DeltaTime);
	}
}

UPlayerEnergyComponent* AEnemyBase::FindPlayerEnergyComponent() const
{
	if (!GetWorld())
	{
		return nullptr;
	}
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		if (UPlayerEnergyComponent* Energy = It->FindComponentByClass<UPlayerEnergyComponent>())
		{
			return Energy;
		}
	}
	UE_LOG(LogTemp, Warning,
		TEXT("AEnemyBase: FindPlayerEnergyComponent on '%s' found no APawn with a UPlayerEnergyComponent."),
		*GetNameSafe(this));
	return nullptr;
}

EThreatState AEnemyBase::GetThreatState() const
{
	const bool bIsEngaged = CurrentState == EEnemyState::Alert
		|| CurrentState == EEnemyState::Attack
		|| CurrentState == EEnemyState::Controlled;
	return bIsEngaged ? EThreatState::Hot : EThreatState::Idle;
}

bool AEnemyBase::IsControlled() const
{
	return CurrentState == EEnemyState::Controlled;
}

FName AEnemyBase::GetHerdColourTag() const
{
	return AbilityData::Get(ControllingAbility).ColourTag;
}

void AEnemyBase::SetIsElite(bool bNewIsElite)
{
	bIsElite = bNewIsElite;
	if (UPointLightComponent* TrimLight = GetEliteTrimLightComponent())
	{
		TrimLight->SetIntensity(bIsElite ? EliteTrimIntensity : 0.0f);
	}
}

float AEnemyBase::GetEffectiveMovementSpeedUnitsPerSecond() const
{
	return GetMovementSpeedUnitsPerSecond() * (bIsElite ? EliteMovementSpeedMultiplier : 1.0f);
}
