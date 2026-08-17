#pragma once

#include "CoreMinimal.h"
#include "EnemyBase.h"
#include "Components/SceneComponent.h"
#include "EnemyBaseNoTrimLightTestActor.generated.h"

// Test-only AEnemyBase subclass (issue #19) that deliberately does NOT override
// GetEliteTrimLightComponent(), unlike every other concrete subclass (AEnemyBaseTestActor
// included) - exists solely so KrowdKontrolEnemyBaseTest.cpp can exercise SetIsElite()'s
// nullptr-trim-light branch, which nothing else in the test suite reaches (every other
// tested subclass overrides the accessor to return a real component).
UCLASS()
class AEnemyBaseNoTrimLightTestActor : public AEnemyBase
{
	GENERATED_BODY()

public:
	AEnemyBaseNoTrimLightTestActor();
};
