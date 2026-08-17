#include "RoomMetadataComponent.h"

URoomMetadataComponent::URoomMetadataComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Pre-populate both arrays with one zeroed entry per locked enemy type, so a newly
	// added component starts structurally complete (all 4 types present, not an empty
	// array a designer has to build up from scratch) - matches the issue's "count per
	// each of the 4 locked enemy types" wording.
	for (EEnemyType Type : { EEnemyType::RU_NNR, EEnemyType::TR_UPR, EEnemyType::B0_0MR, EEnemyType::SN_1PR })
	{
		FRoomEnemyTypeCount BudgetEntry;
		BudgetEntry.EnemyType = Type;
		EnemyTypeBudget.Add(BudgetEntry);

		FRoomEnemyTypeCount ZoneEntry;
		ZoneEntry.EnemyType = Type;
		TargetZoneCounts.Add(ZoneEntry);
	}
}
