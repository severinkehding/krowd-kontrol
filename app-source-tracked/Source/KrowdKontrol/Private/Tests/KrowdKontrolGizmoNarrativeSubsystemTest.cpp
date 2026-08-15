// Confirms UGizmoNarrativeSubsystem (issue #57, PRD 07) is the reusable trigger point
// for one-sided Gizmo remote-call barks: a known bark ID broadcasts its lines exactly
// once, a second trigger of the same ID is a silent no-op, and triggering an unknown
// ID logs a warning and no-ops rather than crashing.
//
// No UWorld/CreateNewMap() needed - RegisterBark/TriggerBark/HasBarkFired never call
// GetWorld() or GetGameInstance(), so the subsystem is constructed directly via
// NewObject<>(), matching KrowdKontrolAbilityDataTest.cpp's "no engine-object
// dependency" precedent.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "GizmoNarrativeSubsystem.h"
#include "GizmoBarkTestListener.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolGizmoNarrativeSubsystemTest,
	"KrowdKontrol.Unit.GizmoNarrativeSubsystem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolGizmoNarrativeSubsystemTest::RunTest(const FString& Parameters)
{
	UGizmoNarrativeSubsystem* Subsystem = NewObject<UGizmoNarrativeSubsystem>();
	if (!TestNotNull(TEXT("UGizmoNarrativeSubsystem should construct"), Subsystem))
	{
		return false;
	}

	UGizmoBarkTestListener* Listener = NewObject<UGizmoBarkTestListener>();
	Subsystem->OnBarkTriggered.AddDynamic(Listener, &UGizmoBarkTestListener::HandleBarkTriggered);

	const FName KnownID = TEXT("TestBark.FirstContact");
	FGizmoBark Bark;
	Bark.BarkID = KnownID;
	Bark.Lines = { TEXT("Line one."), TEXT("Line two.") };
	Subsystem->RegisterBark(Bark);

	// (1) Triggering a known, unfired bark ID broadcasts once with the registered text
	// and marks it as fired.
	Subsystem->TriggerBark(KnownID);
	TestEqual(TEXT("CallCount should be 1 after the first trigger"), Listener->CallCount, 1);
	TestEqual(TEXT("LastBarkID should be the triggered bark's ID"), Listener->LastBarkID, KnownID);
	TestEqual(TEXT("LastLines should match the registered lines"), Listener->LastLines, Bark.Lines);
	TestTrue(TEXT("HasBarkFired should be true after triggering"), Subsystem->HasBarkFired(KnownID));

	// (2) Triggering the same bark ID a second time must not re-broadcast.
	Subsystem->TriggerBark(KnownID);
	TestEqual(TEXT("CallCount should still be 1 after a second trigger of the same ID"), Listener->CallCount, 1);

	// (3) Triggering an unregistered bark ID logs a warning, does not crash, and does
	// not broadcast.
	const FName UnknownID = TEXT("TestBark.NeverRegistered");
	AddExpectedError(TEXT("unknown bark ID"), EAutomationExpectedErrorFlags::Contains, 1);
	Subsystem->TriggerBark(UnknownID);
	TestEqual(TEXT("CallCount should still be 1 after triggering an unknown ID"), Listener->CallCount, 1);
	TestFalse(TEXT("HasBarkFired should be false for an unknown ID"), Subsystem->HasBarkFired(UnknownID));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
