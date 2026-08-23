#include "QuestTrackerWidget.h"
#include "HUDChromeColours.h"
#include "TargetZone.h"
#include "EnemyBase.h"
#include "WaveSpawnerComponent.h"
#include "LevelLifecycleSubsystem.h"
#include "AbilityData.h"
#include "AbilityUnlockComponent.h"
#include "EnemyTypeIndicatorComponent.h"
#include "RoomActor.h"
#include "DoorConnectorActor.h"
#include "EngineUtils.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"

void UQuestTrackerWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EnsureWidgetTreeBuilt();
	BindToLevelLifecycle();
}

bool UQuestTrackerWidget::Initialize()
{
	const bool bNewlyInitialized = Super::Initialize();
	if (bNewlyInitialized)
	{
		EnsureWidgetTreeBuilt();
		BindToLevelLifecycle();
	}
	return bNewlyInitialized;
}

void UQuestTrackerWidget::EnsureWidgetTreeBuilt()
{
	if (!BankedCountText)
	{
		if (!WidgetTree)
		{
			WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
		}
		BuildWidgetTree();
		RefreshDisplay();
		RefreshSuggestedAbilityDisplay();
		RefreshRoomStateDisplay();
	}
}

void UQuestTrackerWidget::BuildWidgetTree()
{
	const FLinearColor ChromeBackgroundColor = HUDChromeColours::GetBackground();
	const FSlateColor TextColor(HUDChromeColours::GetText());

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("QuestTrackerRootCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	ChromeBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("QuestTrackerChromeBorder"));
	ChromeBorder->SetBrushColor(ChromeBackgroundColor);

	UCanvasPanelSlot* TrackerSlot = RootCanvas->AddChildToCanvas(ChromeBorder);
	checkf(TrackerSlot, TEXT("QuestTrackerWidget: AddChildToCanvas(ChromeBorder) returned null"));
	// Top-right corner anchoring - see this class's header comment for why this
	// corner. Diagonally opposite UOnScreenPromptWidget's top-center (not a corner
	// widget) and distinct from UEnergyMeterWidget (top-left)/UAbilityCooldownTrayWidget
	// (bottom-right).
	TrackerSlot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f));
	TrackerSlot->SetAlignment(FVector2D(1.0f, 0.0f));
	TrackerSlot->SetAutoSize(false);
	TrackerSlot->SetSize(FVector2D(TrackerWidthPx, TrackerHeightPx));
	TrackerSlot->SetPosition(FVector2D(-TrackerMarginPx, TrackerMarginPx));

	UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("QuestTrackerRows"));
	ChromeBorder->SetContent(Rows);

	BankedCountText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("QuestTrackerBankedCountText"));
	BankedCountText->SetColorAndOpacity(TextColor);
	Rows->AddChildToVerticalBox(BankedCountText);

	SuggestedAbilityText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("QuestTrackerSuggestedAbilityText"));
	Rows->AddChildToVerticalBox(SuggestedAbilityText);

	RoomStateText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("QuestTrackerRoomStateText"));
	RoomStateText->SetColorAndOpacity(TextColor);
	Rows->AddChildToVerticalBox(RoomStateText);
}

void UQuestTrackerWidget::BindToLevelLifecycle()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (ULevelLifecycleSubsystem* LifecycleSubsystem = World->GetSubsystem<ULevelLifecycleSubsystem>())
	{
		LifecycleSubsystem->OnLevelBegin.AddUniqueDynamic(this, &UQuestTrackerWidget::HandleLevelBegin);

		// Late-subscribe catch-up - see this function's header comment and
		// ULevelLifecycleSubsystem::HasLevelBegun()'s comment for why this widget's
		// creation isn't guaranteed to precede the broadcast it just subscribed to.
		if (LifecycleSubsystem->HasLevelBegun())
		{
			HandleLevelBegin(NAME_None);
		}
	}
}

void UQuestTrackerWidget::HandleLevelBegin(FName MapName)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	RecountTotalEnemies();

	for (TActorIterator<ATargetZone> It(World); It; ++It)
	{
		It->OnActorBanked.AddUniqueDynamic(this, &UQuestTrackerWidget::HandleActorBanked);
	}

	for (TActorIterator<ARoomActor> It(World); It; ++It)
	{
		It->OnRoomClearedStateChanged.AddUniqueDynamic(this, &UQuestTrackerWidget::HandleRoomClearedStateChanged);
	}

	// TActorIterator<AActor> + GetComponents(), not a bare
	// TObjectIterator<UWaveSpawnerComponent>: same rationale as
	// ULevelLifecycleSubsystem::RefreshLevelClearState() - avoids picking up a spawner
	// from another concurrently-loaded Automation test world.
	for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
	{
		TArray<UWaveSpawnerComponent*> Spawners;
		ActorIt->GetComponents<UWaveSpawnerComponent>(Spawners);
		for (UWaveSpawnerComponent* Spawner : Spawners)
		{
			Spawner->OnWaveSpawned.AddUniqueDynamic(this, &UQuestTrackerWidget::HandleWaveSpawned);
		}
	}

	RefreshDisplay();
	RefreshSuggestedAbilityDisplay();
	RefreshRoomStateDisplay();
}

void UQuestTrackerWidget::HandleActorBanked(AActor* BankedActor)
{
	if (BankedActors.ContainsByPredicate(
		[BankedActor](const TWeakObjectPtr<AActor>& Existing) { return Existing.Get() == BankedActor; }))
	{
		return;
	}
	BankedActors.Add(BankedActor);
	++BankedCount;
	RefreshDisplay();
	RefreshSuggestedAbilityDisplay();
}

void UQuestTrackerWidget::HandleWaveSpawned(int32 WaveIndex)
{
	RecountTotalEnemies();
	RefreshDisplay();
	RefreshSuggestedAbilityDisplay();
}

void UQuestTrackerWidget::RecountTotalEnemies()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TotalEnemyCount = 0;
	for (TActorIterator<AEnemyBase> It(World); It; ++It)
	{
		++TotalEnemyCount;
	}
}

void UQuestTrackerWidget::RefreshDisplay()
{
	if (!BankedCountText)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UQuestTrackerWidget: BankedCountText is null on '%s' (tree not built?) - banked count will render blank."),
			*GetNameSafe(this));
		return;
	}
	FNumberFormattingOptions NoGrouping;
	NoGrouping.SetUseGrouping(false);
	BankedCountText->SetText(FText::Format(
		NSLOCTEXT("QuestTrackerWidget", "BankedCountFormat", "Robots penned: {0}/{1}"),
		FText::AsNumber(BankedCount, &NoGrouping),
		FText::AsNumber(TotalEnemyCount, &NoGrouping)));
}

FText UQuestTrackerWidget::GetQuestTrackerDisplayText() const
{
	return BankedCountText ? BankedCountText->GetText() : FText::GetEmpty();
}

void UQuestTrackerWidget::BindAbilityUnlockComponent(UAbilityUnlockComponent* UnlockComponent)
{
	if (!UnlockComponent)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UQuestTrackerWidget::BindAbilityUnlockComponent called with null component - suggested-ability line keeps its current fallback state."));
		return;
	}
	BoundUnlockComponent = UnlockComponent;
	// AddUniqueDynamic - same "safe to call more than once" rationale as
	// BindToLevelLifecycle()'s own identical idiom above.
	UnlockComponent->OnAbilityUnlocked.AddUniqueDynamic(this, &UQuestTrackerWidget::HandleAbilityUnlocked);
	RefreshSuggestedAbilityDisplay();
}

void UQuestTrackerWidget::HandleAbilityUnlocked(EAbilitySlot Ability)
{
	RefreshSuggestedAbilityDisplay();
}

EAbilitySlot UQuestTrackerWidget::ComputeSuggestedAbility() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return EAbilitySlot::Stun;
	}

	TSet<EEnemyType> RemainingTypes;
	for (TActorIterator<AEnemyBase> It(World); It; ++It)
	{
		if (It->GetEnemyState() == EEnemyState::Banked)
		{
			continue;
		}
		if (const UEnemyTypeIndicatorComponent* Indicator = It->FindComponentByClass<UEnemyTypeIndicatorComponent>())
		{
			RemainingTypes.Add(Indicator->EnemyType);
		}
	}

	const UAbilityUnlockComponent* UnlockComponent = BoundUnlockComponent.Get();
	if (UnlockComponent)
	{
		// AbilityData::GetAll() order (Stun, Sleep, Root, Fear, Snare) is this
		// function's deterministic tie-break when more than one remaining type has
		// an unlocked counter - the issue's AC doesn't specify a priority, and no
		// other existing code establishes one.
		for (const FAbilityData& Data : AbilityData::GetAll())
		{
			if (!Data.bIsColourNeutral && RemainingTypes.Contains(Data.CounteredEnemyType)
				&& UnlockComponent->IsAbilityUnlocked(Data.Ability))
			{
				return Data.Ability;
			}
		}
	}

	// Universal fallback - Stun is the only bIsColourNeutral ability (MISSION.md
	// Hard Invariant 4), so this return value doubles as the fallback sentinel
	// RefreshSuggestedAbilityDisplay() below branches on.
	return EAbilitySlot::Stun;
}

void UQuestTrackerWidget::RefreshSuggestedAbilityDisplay()
{
	if (!SuggestedAbilityText)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UQuestTrackerWidget: SuggestedAbilityText is null on '%s' (tree not built?) - suggestion will render blank."),
			*GetNameSafe(this));
		return;
	}

	const EAbilitySlot Suggested = ComputeSuggestedAbility();
	const FAbilityData& Data = AbilityData::Get(Suggested);

	const FText Line = Data.bIsColourNeutral
		? FText::Format(
			  NSLOCTEXT("QuestTrackerWidget", "SuggestedAbilityFallbackFormat", "ANY ROBOT → {0} ({1})"),
			  FText::FromString(AbilityData::GetDisplayName(Suggested)), Data.KeyBindingLabel)
		: FText::Format(
			  NSLOCTEXT("QuestTrackerWidget", "SuggestedAbilityFormat", "{0} → {1} ({2})"),
			  FText::FromString(AbilityData::GetEnemyPluralDisplayName(Data.CounteredEnemyType)),
			  FText::FromString(AbilityData::GetDisplayName(Suggested)), Data.KeyBindingLabel);

	SuggestedAbilityText->SetText(Line);
	// Genuine information swatch (Hard Invariant 3's exception) - mirrors
	// AbilityCooldownTrayWidget.cpp:174's identical SetColorAndOpacity(FSlateColor(
	// AbilityData::Get(...).Colour)) idiom. Stun's Colour is White (still one of
	// the 5 reserved colours), so the fallback line is legitimately tinted too.
	SuggestedAbilityText->SetColorAndOpacity(FSlateColor(Data.Colour));
}

FText UQuestTrackerWidget::GetSuggestedAbilityDisplayText() const
{
	return SuggestedAbilityText ? SuggestedAbilityText->GetText() : FText::GetEmpty();
}

FLinearColor UQuestTrackerWidget::GetSuggestedAbilityTextColour() const
{
	return SuggestedAbilityText ? SuggestedAbilityText->GetColorAndOpacity().GetSpecifiedColor() : FLinearColor::Black;
}

void UQuestTrackerWidget::HandleRoomClearedStateChanged()
{
	RefreshRoomStateDisplay();
}

FText UQuestTrackerWidget::GetRoomStateDisplayText() const
{
	return RoomStateText ? RoomStateText->GetText() : FText::GetEmpty();
}

void UQuestTrackerWidget::RefreshRoomStateDisplay()
{
	if (!RoomStateText)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UQuestTrackerWidget: RoomStateText is null on '%s' (tree not built?) - room-state line will render blank."),
			*GetNameSafe(this));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		RoomStateText->SetText(FText::GetEmpty());
		CurrentObjectiveDirection = EQuestDirection8::None;
		return;
	}

	TArray<ARoomActor*> Rooms;
	for (TActorIterator<ARoomActor> It(World); It; ++It)
	{
		Rooms.Add(*It);
	}

	if (Rooms.Num() == 0)
	{
		// No room actors in this world (e.g. widget-only unit tests that never spawn
		// one) - stay blank rather than claiming a false "DOOR OPEN".
		RoomStateText->SetText(FText::GetEmpty());
		CurrentObjectiveDirection = EQuestDirection8::None;
		return;
	}

	// Chain order by world X - same convention ADoorConnectorActor::BeginPlay()'s
	// GatingRoom heuristic and KrowdKontrolLevelTestUtils::SortRoomsByX both use.
	Rooms.Sort([](const ARoomActor& A, const ARoomActor& B) { return A.GetActorLocation().X < B.GetActorLocation().X; });

	int32 FocusIndex = INDEX_NONE;
	for (int32 Index = 0; Index < Rooms.Num(); ++Index)
	{
		if (!Rooms[Index]->IsRoomCleared())
		{
			FocusIndex = Index;
			break;
		}
	}

	// REQ-3 directional cue - see ResolveObjectiveDirectionTarget()'s own
	// comment and plan.md's Design Decisions for the full rationale. Computed
	// at this same event-driven refresh point, not per-frame (see EVENT CADENCE
	// in plan.md) - a stale-until-next-event cue is an accepted trade-off for an
	// 8-bucket "cheapest useful" hint.
	FVector TargetLocation;
	FText DirectionGlyph = FText::GetEmpty();
	CurrentObjectiveDirection = EQuestDirection8::None;
	if (ResolveObjectiveDirectionTarget(Rooms, FocusIndex, TargetLocation))
	{
		if (const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0))
		{
			CurrentObjectiveDirection = ComputeCompassDirection(PlayerPawn->GetActorLocation(), TargetLocation);
			DirectionGlyph = GetDirectionGlyph(CurrentObjectiveDirection);
		}
	}

	if (FocusIndex == INDEX_NONE)
	{
		// Every room in chain order is cleared - the last gate has opened.
		const FText BaseLine = NSLOCTEXT("QuestTrackerWidget", "RoomStateDoorOpen", "DOOR OPEN");
		RoomStateText->SetText(DirectionGlyph.IsEmpty()
			? BaseLine
			: FText::Format(NSLOCTEXT("QuestTrackerWidget", "RoomStateWithDirectionFormat", "{0} {1}"), BaseLine, DirectionGlyph));
		return;
	}

	const int32 RemainingCount = Rooms[FocusIndex]->GetRemainingEnemyCount();
	FNumberFormattingOptions NoGrouping;
	NoGrouping.SetUseGrouping(false);
	const FText BaseLine = (RemainingCount == 1)
		? FText::Format(NSLOCTEXT("QuestTrackerWidget", "RoomStateSingularFormat", "Room {0} — {1} robot left"),
			  FText::AsNumber(FocusIndex + 1, &NoGrouping), FText::AsNumber(RemainingCount, &NoGrouping))
		: FText::Format(NSLOCTEXT("QuestTrackerWidget", "RoomStatePluralFormat", "Room {0} — {1} robots left"),
			  FText::AsNumber(FocusIndex + 1, &NoGrouping), FText::AsNumber(RemainingCount, &NoGrouping));

	RoomStateText->SetText(DirectionGlyph.IsEmpty()
		? BaseLine
		: FText::Format(NSLOCTEXT("QuestTrackerWidget", "RoomStateWithDirectionFormat", "{0} {1}"), BaseLine, DirectionGlyph));
}

bool UQuestTrackerWidget::ResolveObjectiveDirectionTarget(const TArray<ARoomActor*>& Rooms, int32 FocusIndex, FVector& OutTargetLocation) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	if (FocusIndex == INDEX_NONE)
	{
		// DOOR OPEN: every room is cleared - point at the last room's forward
		// gating door marker (the door whose clearing just opened it), if one
		// exists. A level whose last room has no door beyond it (the common
		// case today - no dedicated level-exit actor exists in this codebase)
		// has no further beacon to point at; the cue stays blank rather than
		// guessing.
		if (Rooms.Num() == 0)
		{
			return false;
		}
		ARoomActor* LastRoom = Rooms.Last();
		for (TActorIterator<ADoorConnectorActor> It(World); It; ++It)
		{
			if (It->GatingRoom == LastRoom && It->DoorMarkerMeshComponent && It->DoorMarkerMeshComponent->IsVisible())
			{
				OutTargetLocation = It->DoorMarkerMeshComponent->GetComponentLocation();
				return true;
			}
		}
		return false;
	}

	ARoomActor* FocusRoom = Rooms[FocusIndex];

	// Same "remaining enemy types" derivation as ComputeSuggestedAbility()
	// above, scoped to this room's own OwnedEnemies rather than the whole
	// level - the pen the arrow should point at is the one matching what's
	// still alive *in this room*. Predicate mirrors
	// ARoomActor::GetRemainingEnemyCount()'s own IsValid/
	// IsActorBeingDestroyed/GetEnemyState check exactly (RoomActor.cpp:309-323).
	TSet<EEnemyType> RemainingTypes;
	for (const TObjectPtr<AEnemyBase>& Enemy : FocusRoom->GetOwnedEnemies())
	{
		if (!IsValid(Enemy) || Enemy->IsActorBeingDestroyed() || Enemy->GetEnemyState() == EEnemyState::Banked)
		{
			continue;
		}
		if (const UEnemyTypeIndicatorComponent* Indicator = Enemy->FindComponentByClass<UEnemyTypeIndicatorComponent>())
		{
			RemainingTypes.Add(Indicator->EnemyType);
		}
	}

	for (const FRoomTargetZone& Zone : FocusRoom->GetTargetZones())
	{
		if (Zone.MarkerActor && RemainingTypes.Contains(Zone.EnemyType))
		{
			OutTargetLocation = Zone.MarkerActor->GetActorLocation();
			return true;
		}
	}

	// Fallback: no target zone matches a still-remaining type (e.g. a room
	// authored without AddTargetZone() calls) - use the room's first target
	// zone anyway, or the room's own location as a last resort, rather than
	// showing no cue at all for a room the player is actively working through.
	if (FocusRoom->GetTargetZones().Num() > 0 && FocusRoom->GetTargetZones()[0].MarkerActor)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UQuestTrackerWidget::ResolveObjectiveDirectionTarget: Room '%s' has no target zone matching a remaining enemy type - falling back to its first target zone; directional cue may point at the wrong pen."),
			*GetNameSafe(FocusRoom));
		OutTargetLocation = FocusRoom->GetTargetZones()[0].MarkerActor->GetActorLocation();
		return true;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("UQuestTrackerWidget::ResolveObjectiveDirectionTarget: Room '%s' has no target zones at all - falling back to room location; directional cue may point at empty space."),
		*GetNameSafe(FocusRoom));
	OutTargetLocation = FocusRoom->GetActorLocation();
	return true;
}

EQuestDirection8 UQuestTrackerWidget::ComputeCompassDirection(const FVector& FromLocation, const FVector& ToLocation)
{
	// Same real-gameplay-units dead zone shape as
	// UAbilityCastComponent::ComputeConeDirection (AbilityCastComponent.cpp:202-212).
	constexpr float DirectionDeadZoneRadiusUnits = 10.0f;
	const FVector2D Delta(ToLocation.X - FromLocation.X, ToLocation.Y - FromLocation.Y);
	if (Delta.SizeSquared() <= FMath::Square(DirectionDeadZoneRadiusUnits))
	{
		return EQuestDirection8::None;
	}

	// This codebase's flat top-down X/Y plane (matches
	// UAbilityCastComponent::IsPointInCone's own SizeSquared2D usage) -
	// world +X is North, +Y is East (see EQuestDirection8's own header
	// comment for why this specific mapping).
	const float AngleDegrees = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
	const float NormalizedDegrees = FMath::Fmod(AngleDegrees + 360.0f, 360.0f);
	const int32 SectorIndex = FMath::RoundToInt(NormalizedDegrees / 45.0f) % 8;

	static const EQuestDirection8 Sectors[8] = {
		EQuestDirection8::North, EQuestDirection8::NorthEast, EQuestDirection8::East, EQuestDirection8::SouthEast,
		EQuestDirection8::South, EQuestDirection8::SouthWest, EQuestDirection8::West, EQuestDirection8::NorthWest
	};
	return Sectors[SectorIndex];
}

FText UQuestTrackerWidget::GetDirectionGlyph(EQuestDirection8 Direction)
{
	switch (Direction)
	{
	case EQuestDirection8::North:     return FText::FromString(TEXT("↑"));
	case EQuestDirection8::NorthEast: return FText::FromString(TEXT("↗"));
	case EQuestDirection8::East:      return FText::FromString(TEXT("→"));
	case EQuestDirection8::SouthEast: return FText::FromString(TEXT("↘"));
	case EQuestDirection8::South:     return FText::FromString(TEXT("↓"));
	case EQuestDirection8::SouthWest: return FText::FromString(TEXT("↙"));
	case EQuestDirection8::West:      return FText::FromString(TEXT("←"));
	case EQuestDirection8::NorthWest: return FText::FromString(TEXT("↖"));
	case EQuestDirection8::None:
	default:
		return FText::GetEmpty();
	}
}
