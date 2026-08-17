#include "EnemyTypeIndicatorComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/Actor.h"

UEnemyTypeIndicatorComponent::UEnemyTypeIndicatorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEnemyTypeIndicatorComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeMarkerVisual();
}

FText UEnemyTypeIndicatorComponent::GetMarkerText() const
{
	return StaticEnum<EEnemyType>()->GetDisplayNameTextByValue(static_cast<int64>(EnemyType));
}

void UEnemyTypeIndicatorComponent::InitializeMarkerVisual()
{
	if (bHasInitializedMarkerVisual)
	{
		return;
	}
	bHasInitializedMarkerVisual = true;

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->GetRootComponent())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UEnemyTypeIndicatorComponent: InitializeMarkerVisual on '%s' found no Owner root component - marker not created."),
			*GetNameSafe(Owner));
		return;
	}

	MarkerTextComponent = NewObject<UTextRenderComponent>(Owner, TEXT("EnemyTypeMarkerText"));
	MarkerTextComponent->SetupAttachment(Owner->GetRootComponent());
	MarkerTextComponent->RegisterComponent();
	MarkerTextComponent->SetRelativeLocation(FVector(0.0f, 0.0f, MarkerHeightOffset));
	MarkerTextComponent->SetHorizontalAlignment(EHTA_Center);
	MarkerTextComponent->SetWorldSize(48.0f);
	// Neutral placeholder colour, deliberately not one of MISSION.md Hard Invariant 3's
	// five reserved gameplay-information colours (Purple/Teal/Orange/Blue/White) - same
	// literal UAbilityCooldownTrayWidget::BuildWidgetTree() uses for its own text chrome
	// (AbilityCooldownTrayWidget.cpp:74), already proven distinct from reserved White by
	// FKrowdKontrolReservedGameplayColoursTest.
	MarkerTextComponent->SetTextRenderColor(FColor(217, 217, 217));
	MarkerTextComponent->SetText(GetMarkerText());
}
