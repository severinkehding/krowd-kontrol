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

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->GetRootComponent())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UEnemyTypeIndicatorComponent: InitializeMarkerVisual on '%s' found no Owner root component - marker not created."),
			*GetNameSafe(Owner));
		// bHasInitializedMarkerVisual deliberately NOT set here - unlike the happy
		// path below, this failure can be transient (e.g. owner's root component not
		// yet set up), so a later call is allowed to retry rather than permanently
		// losing the marker.
		return;
	}
	bHasInitializedMarkerVisual = true;

	MarkerTextComponent = NewObject<UTextRenderComponent>(Owner, TEXT("EnemyTypeMarkerText"));
	MarkerTextComponent->SetupAttachment(Owner->GetRootComponent());
	MarkerTextComponent->RegisterComponent();
	MarkerTextComponent->SetRelativeLocation(FVector(0.0f, 0.0f, MarkerHeightOffset));
	MarkerTextComponent->SetHorizontalAlignment(EHTA_Center);
	MarkerTextComponent->SetWorldSize(48.0f);
	// Neutral placeholder colour, deliberately not one of MISSION.md Hard Invariant 3's
	// five reserved gameplay-information colours (Purple/Teal/Orange/Blue/White) -
	// visually matches the light-grey UAbilityCooldownTrayWidget::BuildWidgetTree()
	// uses for its own text chrome (AbilityCooldownTrayWidget.cpp:74,
	// FLinearColor(0.85, 0.85, 0.85, 1.0)). Derived via ToFColor(bSRGB=true) rather
	// than a manual *255 byte literal, so this FColor's decoded FLinearColor value is
	// genuinely that same 0.85 grey - UTextRenderComponent::SetTextRenderColor only
	// accepts FColor, and a naive byte approximation decodes (sRGB-to-linear) to a
	// materially different value. Non-collision with all 5 reserved colours for this
	// component's actual runtime value is asserted directly by this component's own
	// test (KrowdKontrolEnemyTypeIndicatorComponentTest.cpp, part (e)) - the
	// pre-existing FKrowdKontrolReservedGameplayColoursTest only covers the ability
	// tray's own FLinearColor literal, not this component.
	static const FColor MarkerTextColour = FLinearColor(0.85f, 0.85f, 0.85f, 1.0f).ToFColor(/*bSRGB=*/true);
	MarkerTextComponent->SetTextRenderColor(MarkerTextColour);
	MarkerTextComponent->SetText(GetMarkerText());
}
