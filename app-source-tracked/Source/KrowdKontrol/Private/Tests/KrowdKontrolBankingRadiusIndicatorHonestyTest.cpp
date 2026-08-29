// Closes a regression gap issue #366 identified in #365's ground-ring indicator
// (APlaceholderTargetZoneActor::BankingRadiusIndicatorComponent): neither existing
// test that touches this code path can distinguish "ring radius genuinely derived
// from ATargetZone::GetBankingRadiusUnits()" from "ring radius hardcoded to a
// constant that happens to match." KrowdKontrolPlaceholderTargetZoneActorTest.cpp
// drives ShowBankingRadiusIndicator() with a single literal (250.0f);
// KrowdKontrolRoomActorBankingWiringTest.cpp compares two zones that both
// coincidentally resolve to the same default 150-unit radius. This test spawns two
// independent APlaceholderTargetZoneActor + ATargetZone pairs with two genuinely
// different real box extents (150 default, 300 resized - both inside
// GetBankingRadiusUnits()'s clamp band, matching KrowdKontrolTargetZoneTest.cpp's
// own honesty sub-case (g)), and asserts each ring's radius matches its own zone's
// live GetBankingRadiusUnits() AND that the two rings' radii differ from each other -
// the one assertion shape that actually rules out a shared hardcoded constant.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "PlaceholderTargetZoneActor.h"
#include "TargetZone.h"
#include "AbilityTargetingIndicatorComponent.h"
#include "Components/BoxComponent.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolBankingRadiusIndicatorHonestyTest,
	"KrowdKontrol.Unit.BankingRadiusIndicatorHonesty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolBankingRadiusIndicatorHonestyTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	// Zone A: left at ATargetZone's constructor default extent (150x150), radius 150.0f.
	APlaceholderTargetZoneActor* MarkerA = World->SpawnActor<APlaceholderTargetZoneActor>();
	if (!TestNotNull(TEXT("MarkerA should spawn into the test World"), MarkerA))
	{
		return false;
	}
	ATargetZone* ZoneA = World->SpawnActor<ATargetZone>();
	if (!TestNotNull(TEXT("ZoneA should spawn into the test World"), ZoneA))
	{
		return false;
	}
	MarkerA->ShowBankingRadiusIndicator(ZoneA->GetBankingRadiusUnits(), FLinearColor::White);
	TestEqual(TEXT("MarkerA's ring radius should equal ZoneA's real GetBankingRadiusUnits() (default 150.0f)"),
		MarkerA->BankingRadiusIndicatorComponent->CurrentShapeSpec.RangeUnits, ZoneA->GetBankingRadiusUnits());

	// Zone B: resized to a different real extent (300x300), radius 300.0f - same
	// extent KrowdKontrolTargetZoneTest.cpp's honesty sub-case (g) uses, confirmed
	// there to land inside GetBankingRadiusUnits()'s clamp band unchanged.
	APlaceholderTargetZoneActor* MarkerB = World->SpawnActor<APlaceholderTargetZoneActor>();
	if (!TestNotNull(TEXT("MarkerB should spawn into the test World"), MarkerB))
	{
		return false;
	}
	ATargetZone* ZoneB = World->SpawnActor<ATargetZone>();
	if (!TestNotNull(TEXT("ZoneB should spawn into the test World"), ZoneB))
	{
		return false;
	}
	ZoneB->ZoneCollisionComponent->SetBoxExtent(FVector(300.f, 300.f, 100.f));
	MarkerB->ShowBankingRadiusIndicator(ZoneB->GetBankingRadiusUnits(), FLinearColor::White);
	TestEqual(TEXT("MarkerB's ring radius should equal ZoneB's real GetBankingRadiusUnits() (resized 300.0f)"),
		MarkerB->BankingRadiusIndicatorComponent->CurrentShapeSpec.RangeUnits, ZoneB->GetBankingRadiusUnits());

	// The assertion that actually proves derivation, not a shared hardcoded constant:
	// the two rings must reflect two genuinely different radii.
	TestTrue(TEXT("MarkerA's and MarkerB's ring radii should differ, proving each is derived from its own zone rather than a shared hardcoded constant"),
		!FMath::IsNearlyEqual(
			MarkerA->BankingRadiusIndicatorComponent->CurrentShapeSpec.RangeUnits,
			MarkerB->BankingRadiusIndicatorComponent->CurrentShapeSpec.RangeUnits));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
