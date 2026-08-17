#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnemyType.h"
#include "RoomAbilityGate.h"
#include "RoomDifficultyTier.h"
#include "RoomMetadataComponent.generated.h"

// One locked-enemy-type tag paired with an int32 count. Reused for both
// EnemyTypeBudget and TargetZoneCounts on URoomMetadataComponent below, since both
// need exactly the same shape: a count per each of the 4 locked enemy types
// (MISSION.md Hard Invariant 5). TArray<FRoomEnemyTypeCount>, not
// TMap<EEnemyType, int32> - see URoomMetadataComponent's own comment for why.
// Mirrors ARoomActor::FRoomTargetZone's enum-tag-plus-payload shape (RoomActor.h).
USTRUCT(BlueprintType)
struct FRoomEnemyTypeCount
{
	GENERATED_BODY()

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Room Metadata")
	EEnemyType EnemyType = EEnemyType::RU_NNR;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Room Metadata")
	int32 Count = 0;
};

// Data-only tagging component for the future P1 room-pool shuffler (PRD 05 REQ-4;
// forward-referenced by EnemyType.h's and RoomActor.h's own comments). Attach to a
// placed ARoomActor instance and hand-author its enemy-type budget, target-zone
// summary, difficulty tier, and ability gate; a later shuffler issue reads these back
// to sequence rooms. No shuffling/sequencing logic lives here - metadata storage
// only, per the issue's explicit scope limit.
//
// EnemyTypeBudget/TargetZoneCounts use TArray<FRoomEnemyTypeCount>, not
// TMap<EEnemyType, int32>, despite a TMap looking like the obvious fit: a confirmed,
// still-open Unreal Editor bug (forums.unrealengine.com "TMap Struct value is not
// editable in components inherited from C++", tracked as UE-39260) makes
// struct-valued TMap UPROPERTYs on a UActorComponent unreliable to edit in the
// Details panel, and a second, separately-tracked bug (UE-219729) corrupts
// enum-keyed TMap entries on delete-then-add. TArray<FRoomEnemyTypeCount> avoids both
// and matches ARoomActor::FRoomTargetZone's already-established pattern.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class KROWDKONTROL_API URoomMetadataComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URoomMetadataComponent();

	// How many enemies of each locked type this room spawns.
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Room Metadata")
	TArray<FRoomEnemyTypeCount> EnemyTypeBudget;

	// How many target zones of each locked type this room has. Hand-authored to match
	// the room's actual ARoomActor::GetTargetZones() content, not derived from it -
	// see this issue's Notes on scope (metadata storage only, no cross-validation).
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Room Metadata")
	TArray<FRoomEnemyTypeCount> TargetZoneCounts;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Room Metadata")
	ERoomDifficultyTier DifficultyTier = ERoomDifficultyTier::Easy;

	// None (the default) means the room requires no ability to clear.
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Room Metadata")
	ERoomAbilityGate RequiredAbility = ERoomAbilityGate::None;
};
