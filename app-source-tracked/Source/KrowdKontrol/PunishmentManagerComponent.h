#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PunishmentManagerComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPunishmentTriggered);

// Shared trigger surface for PRD "Punishment System (Punishments 1 & 2 +
// arbitration)" REQ-1 (issue #177). No punishment effect lives here - this is
// pure trigger plumbing; future issues (ability-lockout, speed-reduction,
// arbitration) bind their own listeners to OnPunishmentTriggered. Fires once
// per real ApplyContactDamage-caused energy change, by reusing
// UPlayerEnergyComponent::OnEnergyChanged rather than adding a new hook on
// ApplyContactDamage itself - see the class comment on UPlayerEnergyComponent
// for why that class is not touched.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class KROWDKONTROL_API UPunishmentManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPunishmentManagerComponent();

	// Fires once per real contact-damage event, no payload - future consumers
	// key off "a punishment should trigger", not the damage amount.
	UPROPERTY(BlueprintAssignable, Category = "Punishment")
	FOnPunishmentTriggered OnPunishmentTriggered;

	// Bound to a PlayerEnergyComponent's OnEnergyChanged in each pawn's
	// constructor. NewEnergy is accepted to match FOnEnergyChanged's signature
	// but is otherwise unused - a signature requirement, not a design need for
	// the value.
	UFUNCTION()
	void HandleEnergyChanged(float NewEnergy);
};
