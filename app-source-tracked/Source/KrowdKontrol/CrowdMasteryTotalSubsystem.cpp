#include "CrowdMasteryTotalSubsystem.h"
#include "LevelClearTimeSaveGame.h"
#include "LevelClearTimeSubsystem.h"
#include "MasteryTreeData.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/DataTable.h"

void UCrowdMasteryTotalSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadPersistedTotal();
	if (!MasteryTreeTable)
	{
		MasteryTreeTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Data/DT_MasteryTreeTable.DT_MasteryTreeTable"));
	}
}

void UCrowdMasteryTotalSubsystem::DepositRunMastery(int32 RunMasteryValue)
{
	AccumulatedTotal += FMath::Max(0, RunMasteryValue);
	PersistAccumulatedTotal();
}

void UCrowdMasteryTotalSubsystem::ResetAccumulatedTotal()
{
	AccumulatedTotal = 0;
	PersistAccumulatedTotal();
}

void UCrowdMasteryTotalSubsystem::LoadPersistedTotal()
{
	ULevelClearTimeSaveGame* SaveGameObject = LoadOrCreateSaveGame();
	AccumulatedTotal = SaveGameObject->AccumulatedCrowdMasteryTotal;
}

ULevelClearTimeSaveGame* UCrowdMasteryTotalSubsystem::LoadOrCreateSaveGame() const
{
	if (UGameplayStatics::DoesSaveGameExist(ULevelClearTimeSubsystem::SaveSlotName, 0))
	{
		if (USaveGame* Loaded = UGameplayStatics::LoadGameFromSlot(ULevelClearTimeSubsystem::SaveSlotName, 0))
		{
			if (ULevelClearTimeSaveGame* Typed = Cast<ULevelClearTimeSaveGame>(Loaded))
			{
				return Typed;
			}
			UE_LOG(LogTemp, Warning,
				TEXT("UCrowdMasteryTotalSubsystem::LoadOrCreateSaveGame: save slot '%s' loaded but was not a ULevelClearTimeSaveGame - starting from an empty record."),
				*ULevelClearTimeSubsystem::SaveSlotName);
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("UCrowdMasteryTotalSubsystem::LoadOrCreateSaveGame: save slot '%s' exists but failed to load - starting from an empty record."),
				*ULevelClearTimeSubsystem::SaveSlotName);
		}
	}
	return CastChecked<ULevelClearTimeSaveGame>(UGameplayStatics::CreateSaveGameObject(ULevelClearTimeSaveGame::StaticClass()));
}

void UCrowdMasteryTotalSubsystem::PersistAccumulatedTotal() const
{
	ULevelClearTimeSaveGame* SaveGameObject = LoadOrCreateSaveGame();
	SaveGameObject->AccumulatedCrowdMasteryTotal = AccumulatedTotal;
	if (!UGameplayStatics::SaveGameToSlot(SaveGameObject, ULevelClearTimeSubsystem::SaveSlotName, 0))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UCrowdMasteryTotalSubsystem::PersistAccumulatedTotal: SaveGameToSlot failed for slot '%s' - accumulated total was not persisted and will be lost on next launch."),
			*ULevelClearTimeSubsystem::SaveSlotName);
	}
}

bool UCrowdMasteryTotalSubsystem::HasMasteryTreeTable() const
{
	if (MasteryTreeTable)
	{
		return true;
	}
	if (!bHasWarnedMissingMasteryTreeTable)
	{
		bHasWarnedMissingMasteryTreeTable = true;
		UE_LOG(LogTemp, Warning,
			TEXT("UCrowdMasteryTotalSubsystem: MasteryTreeTable is unset - all skill-tree spend/prerequisite lookups will fail until it is assigned."));
	}
	return false;
}

bool UCrowdMasteryTotalSubsystem::FindBubbleAndOwningNode(FName BubbleId, const FMasteryTreeNode*& OutNode, const FMasterySkillBubble*& OutBubble) const
{
	OutNode = nullptr;
	OutBubble = nullptr;
	if (!HasMasteryTreeTable())
	{
		return false;
	}
	for (const FName& RowName : MasteryTreeTable->GetRowNames())
	{
		const FMasteryTreeNode* Node = MasteryTreeTable->FindRow<FMasteryTreeNode>(RowName, TEXT("UCrowdMasteryTotalSubsystem::FindBubbleAndOwningNode"));
		if (!Node)
		{
			continue;
		}
		for (const FMasterySkillBubble& Bubble : Node->Bubbles)
		{
			if (Bubble.BubbleId == BubbleId)
			{
				OutNode = Node;
				OutBubble = &Bubble;
				return true;
			}
		}
	}
	return false;
}

bool UCrowdMasteryTotalSubsystem::IsNodeReached(FName NodeRowName) const
{
	if (!HasMasteryTreeTable())
	{
		return false;
	}
	const FMasteryTreeNode* Node = MasteryTreeTable->FindRow<FMasteryTreeNode>(NodeRowName, TEXT("UCrowdMasteryTotalSubsystem::IsNodeReached"));
	if (!Node)
	{
		return false;
	}
	for (const FMasterySkillBubble& Bubble : Node->Bubbles)
	{
		if (UnlockedBubbleIds.Contains(Bubble.BubbleId))
		{
			return true;
		}
	}
	return false;
}

bool UCrowdMasteryTotalSubsystem::IsPrerequisiteMetForNode(const FMasteryTreeNode& Node) const
{
	if (Node.ParentNodeId == NAME_None)
	{
		return true;
	}
	return IsNodeReached(Node.ParentNodeId);
}

bool UCrowdMasteryTotalSubsystem::IsPrerequisiteMet(FName BubbleId) const
{
	const FMasteryTreeNode* Node = nullptr;
	const FMasterySkillBubble* Bubble = nullptr;
	if (!FindBubbleAndOwningNode(BubbleId, Node, Bubble))
	{
		return false;
	}
	return IsPrerequisiteMetForNode(*Node);
}

bool UCrowdMasteryTotalSubsystem::TrySpendOnBubble(FName BubbleId)
{
	if (UnlockedBubbleIds.Contains(BubbleId))
	{
		return false;
	}
	const FMasteryTreeNode* Node = nullptr;
	const FMasterySkillBubble* Bubble = nullptr;
	if (!FindBubbleAndOwningNode(BubbleId, Node, Bubble))
	{
		return false;
	}
	if (!IsPrerequisiteMetForNode(*Node))
	{
		return false;
	}
	const int32 AvailablePoints = AccumulatedTotal - SpentPoints;
	if (AvailablePoints < Bubble->PointCost)
	{
		return false;
	}
	SpentPoints += Bubble->PointCost;
	UnlockedBubbleIds.Add(BubbleId);
	return true;
}

TArray<FName> UCrowdMasteryTotalSubsystem::GetUnlockedBubbles() const
{
	return UnlockedBubbleIds.Array();
}

void UCrowdMasteryTotalSubsystem::RefundAllAndClearUnlocks()
{
	SpentPoints = 0;
	UnlockedBubbleIds.Empty();
}
