// Issue #65: proves the colour-match Controlled-duration bonus for the two
// remaining duration-bearing matchups (Root->TR-UPR: 8s, Fear->B0-0MR: 7s), added as
// per-enemy-subclass GetControlledDurationOverrideSeconds() overrides on ATrooperEnemy
// and ABomberEnemy, mirroring ASniperEnemy's already-shipped Sleep override (issue
// #121, covered separately by KrowdKontrolSniperEnemyTest.cpp - not re-tested here).
//
// Covers: one matched application per new override, one mismatched application
// (proves a non-countering ability still applies at full base effectiveness and is
// never gated), and Stun-vs-both-enemy-types (proves Stun stays colour-neutral).

#include "Misc/AutomationTest.h"
#include "TrooperEnemy.h"
#include "BomberEnemy.h"
#include "AbilityData.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolAbilityColourMatchTest,
	"KrowdKontrol.Unit.AbilityColourMatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolAbilityColourMatchTest::RunTest(const FString& Parameters)
{
	const FVector ZeroDistanceLocation(0.0f, 0.0f, 0.0f);

	// (a) Root-vs-TR-UPR: colour-matched, gets the 8s bonus, not the 5s base duration.
	ATrooperEnemy* MatchedTrooper = NewObject<ATrooperEnemy>();
	if (!TestNotNull(TEXT("ATrooperEnemy should construct"), MatchedTrooper))
	{
		return false;
	}
	MatchedTrooper->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	MatchedTrooper->TickCheckDetection(ZeroDistanceLocation); // Alert -> Attack
	MatchedTrooper->ReceiveControl(EAbilitySlot::Root); // colour-matched: TR-UPR countered by Root
	TestEqual(TEXT("Root-vs-TR-UPR should apply the 8s colour-match bonus, not the 5s base duration"),
		MatchedTrooper->GetTotalControlledSeconds(), 8.0f);
	TestNotEqual(TEXT("precondition: Root's colour-match bonus (8.0f) differs from Root's own base duration"),
		8.0f, AbilityData::Get(EAbilitySlot::Root).BaseDurationSeconds);

	// (b) Fear-vs-B0-0MR: colour-matched, gets the 7s bonus, not the 5s base duration.
	ABomberEnemy* MatchedBomber = NewObject<ABomberEnemy>();
	if (!TestNotNull(TEXT("ABomberEnemy should construct"), MatchedBomber))
	{
		return false;
	}
	MatchedBomber->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	MatchedBomber->TickCheckDetection(ZeroDistanceLocation); // Alert -> Attack
	MatchedBomber->ReceiveControl(EAbilitySlot::Fear); // colour-matched: B0-0MR countered by Fear
	TestEqual(TEXT("Fear-vs-B0-0MR should apply the 7s colour-match bonus, not the 5s base duration"),
		MatchedBomber->GetTotalControlledSeconds(), 7.0f);
	TestNotEqual(TEXT("precondition: Fear's colour-match bonus (7.0f) differs from Fear's own base duration"),
		7.0f, AbilityData::Get(EAbilitySlot::Fear).BaseDurationSeconds);

	// (c) Fear-vs-TR-UPR: colour-mismatched, still applies at full base effectiveness,
	// never gated or reduced.
	ATrooperEnemy* MismatchedTrooper = NewObject<ATrooperEnemy>();
	if (!TestNotNull(TEXT("ATrooperEnemy should construct"), MismatchedTrooper))
	{
		return false;
	}
	MismatchedTrooper->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	MismatchedTrooper->TickCheckDetection(ZeroDistanceLocation); // Alert -> Attack
	MismatchedTrooper->ReceiveControl(EAbilitySlot::Fear); // colour-mismatched: TR-UPR is countered by Root, not Fear
	TestEqual(TEXT("A mismatched ability should still apply at full base effectiveness, no reduction"),
		MismatchedTrooper->GetTotalControlledSeconds(), AbilityData::Get(EAbilitySlot::Fear).BaseDurationSeconds);
	TestEqual(TEXT("A mismatched ability should still transition the enemy to Controlled, never blocked"),
		static_cast<uint8>(MismatchedTrooper->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));

	// (d) Stun-vs-any-enemy: never grants a colour-match bonus, checked against both
	// new enemy types.
	ATrooperEnemy* StunnedTrooper = NewObject<ATrooperEnemy>();
	if (!TestNotNull(TEXT("ATrooperEnemy should construct"), StunnedTrooper))
	{
		return false;
	}
	StunnedTrooper->TickCheckDetection(ZeroDistanceLocation);
	StunnedTrooper->TickCheckDetection(ZeroDistanceLocation);
	StunnedTrooper->ReceiveControl(EAbilitySlot::Stun);
	TestEqual(TEXT("Stun should never grant a colour-match bonus against TR-UPR"),
		StunnedTrooper->GetTotalControlledSeconds(), AbilityData::Get(EAbilitySlot::Stun).BaseDurationSeconds);

	ABomberEnemy* StunnedBomber = NewObject<ABomberEnemy>();
	if (!TestNotNull(TEXT("ABomberEnemy should construct"), StunnedBomber))
	{
		return false;
	}
	StunnedBomber->TickCheckDetection(ZeroDistanceLocation);
	StunnedBomber->TickCheckDetection(ZeroDistanceLocation);
	StunnedBomber->ReceiveControl(EAbilitySlot::Stun);
	TestEqual(TEXT("Stun should never grant a colour-match bonus against B0-0MR either"),
		StunnedBomber->GetTotalControlledSeconds(), AbilityData::Get(EAbilitySlot::Stun).BaseDurationSeconds);

	// (e) Root-vs-B0-0MR: colour-mismatched (B0-0MR is countered by Fear, not Root),
	// still applies at full base effectiveness, never gated. Symmetric to case (c)'s
	// Fear-vs-TR-UPR check, closing the other direction of cross-contamination risk
	// between the two new overrides.
	ABomberEnemy* MismatchedBomber = NewObject<ABomberEnemy>();
	if (!TestNotNull(TEXT("ABomberEnemy should construct"), MismatchedBomber))
	{
		return false;
	}
	MismatchedBomber->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	MismatchedBomber->TickCheckDetection(ZeroDistanceLocation); // Alert -> Attack
	MismatchedBomber->ReceiveControl(EAbilitySlot::Root); // colour-mismatched: B0-0MR is countered by Fear, not Root
	TestEqual(TEXT("Root-vs-B0-0MR should still apply at full base effectiveness, not TR-UPR's 8s bonus"),
		MismatchedBomber->GetTotalControlledSeconds(), AbilityData::Get(EAbilitySlot::Root).BaseDurationSeconds);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
