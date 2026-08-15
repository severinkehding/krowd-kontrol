#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Herdable.generated.h"

// Data contract only (issue #79) - no CC-effect, enemy AI, or colour-rendering
// logic here. Future Target Zone work (a separate, not-yet-built issue) is
// expected to query any actor for "are you currently controlled, and what
// colour are you" without needing to know about concrete enemy types.
// C++-only (not Blueprintable) until a real Blueprint consumer exists to
// inform whether these accessors should become BlueprintNativeEvents instead
// - same rationale ThreatState.h documents for GetThreatState().
UINTERFACE(MinimalAPI)
class UHerdable : public UInterface
{
	GENERATED_BODY()
};

class KROWDKONTROL_API IHerdable
{
	GENERATED_BODY()

public:
	virtual bool IsControlled() const = 0;
	virtual FName GetHerdColourTag() const = 0;
};
