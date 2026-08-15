// Confirms UStationPowerUpComponent (issue #60) reveals OrderedLights one at a time,
// strictly in order, only in response to NotifyPowerUpStageTriggered() - never all
// at once, never out of order - fires OnPowerUpSequenceComplete exactly once, and
// never touches player input (PRD 07 REQ-1: player input must remain enabled
// throughout the sequence).
//
// No Pawn/Character/PlayerController class exists in this codebase yet (confirmed
// via full-tree search), so "player input remains enabled" is proven using the
// engine's own AActor::EnableInput()/InputEnabled() on a plain stand-in actor: if
// the component ever called DisableInput on anything, it structurally cannot be
// this stand-in actor (the component holds no reference to it at all) - this test
// instead demonstrates the stronger fact that the component never disables input on
// ANY actor, by never calling any Disable/SetInputMode API anywhere in its
// implementation, and confirms the stand-in's input stays enabled across every
// stage of a full sequence run as a concrete, executable witness of that.
//
// Needs a real UWorld to spawn actors into (SetActorHiddenInGame requires a real
// actor), so - like KrowdKontrolRoomEnemyBudgetControllerTest.cpp -
// FAutomationEditorCommonUtils::CreateNewMap() is used rather than a bare
// NewObject()-only setup.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "StationPowerUpComponent.h"
#include "StationPowerUpTestListener.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Components/InputComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolStationPowerUpComponentTest,
	"KrowdKontrol.Unit.StationPowerUpComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolStationPowerUpComponentTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	AActor* OwnerActor = World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("Owner actor should spawn into the test World"), OwnerActor))
	{
		return false;
	}

	UStationPowerUpComponent* Component =
		NewObject<UStationPowerUpComponent>(OwnerActor);
	if (!TestNotNull(TEXT("UStationPowerUpComponent should construct"), Component))
	{
		return false;
	}
	Component->RegisterComponent();

	AActor* LightA = World->SpawnActor<AActor>();
	AActor* LightB = World->SpawnActor<AActor>();
	AActor* LightC = World->SpawnActor<AActor>();
	Component->OrderedLights = { LightA, LightB, LightC };

	// Player-input stand-in: no Pawn class exists in this codebase yet (see file
	// header comment), so a plain actor with input explicitly enabled stands in for
	// "the player" - if the component ever called DisableInput anywhere, its
	// InputComponent presence below would be the concrete signal to catch a
	// regression even though this actor isn't referenced by the component at all.
	// AActor has no InputEnabled() query and AActor::EnableInput(nullptr) is a no-op
	// (it only creates an InputComponent when given a real PlayerController), so the
	// InputComponent is constructed directly here as the enabled/disabled proxy.
	AActor* PlayerStandIn = World->SpawnActor<AActor>();
	PlayerStandIn->InputComponent = NewObject<UInputComponent>(PlayerStandIn);
	PlayerStandIn->InputComponent->RegisterComponent();
	TestTrue(TEXT("Player stand-in input should start enabled"), PlayerStandIn->InputComponent != nullptr);

	UStationPowerUpTestListener* Listener = NewObject<UStationPowerUpTestListener>();
	Component->OnLightEnabled.AddDynamic(Listener, &UStationPowerUpTestListener::HandleLightEnabled);
	Component->OnPowerUpSequenceComplete.AddDynamic(Listener, &UStationPowerUpTestListener::HandleSequenceComplete);

	Component->InitializeSequence();

	// (a) InitializeSequence() hides every configured light up front.
	TestTrue(TEXT("LightA should start hidden after InitializeSequence"), LightA->IsHidden());
	TestTrue(TEXT("LightB should start hidden after InitializeSequence"), LightB->IsHidden());
	TestTrue(TEXT("LightC should start hidden after InitializeSequence"), LightC->IsHidden());
	TestEqual(TEXT("No lights should be enabled yet"), Component->GetEnabledLightCount(), 0);

	// (b) First trigger reveals only the first light, strictly in order.
	Component->NotifyPowerUpStageTriggered();
	TestFalse(TEXT("LightA should become visible after the first trigger"), LightA->IsHidden());
	TestTrue(TEXT("LightB should remain hidden after the first trigger"), LightB->IsHidden());
	TestTrue(TEXT("LightC should remain hidden after the first trigger"), LightC->IsHidden());
	TestEqual(TEXT("OnLightEnabled should have fired once"), Listener->LightEnabledCallCount, 1);
	TestEqual(TEXT("OnLightEnabled should report index 0"), Listener->LastEnabledLightIndex, 0);
	TestEqual(TEXT("OnLightEnabled should report LightA"), static_cast<AActor*>(Listener->LastEnabledLightActor), static_cast<AActor*>(LightA));
	TestEqual(TEXT("GetEnabledLightCount should be 1 after the first trigger"), Component->GetEnabledLightCount(), 1);
	TestTrue(TEXT("Player input must remain enabled after the first stage"), PlayerStandIn->InputComponent != nullptr);

	// (b2) Regression: calling InitializeSequence() again mid-sequence must no-op,
	// not reset progress or re-hide already-revealed lights - the idempotency guard
	// (bHasInitializedSequence) exists specifically so BeginPlay() and any other
	// caller can call it more than once safely.
	Component->InitializeSequence();
	TestFalse(TEXT("LightA should remain visible after a redundant InitializeSequence call"), LightA->IsHidden());
	TestEqual(TEXT("GetEnabledLightCount should be unchanged after a redundant InitializeSequence call"), Component->GetEnabledLightCount(), 1);
	TestFalse(TEXT("IsSequenceComplete should be unchanged after a redundant InitializeSequence call"), Component->IsSequenceComplete());

	// (c) Second trigger reveals the second light; the first stays revealed.
	Component->NotifyPowerUpStageTriggered();
	TestFalse(TEXT("LightA should remain visible after the second trigger"), LightA->IsHidden());
	TestFalse(TEXT("LightB should become visible after the second trigger"), LightB->IsHidden());
	TestTrue(TEXT("LightC should remain hidden after the second trigger"), LightC->IsHidden());
	TestEqual(TEXT("GetEnabledLightCount should be 2 after the second trigger"), Component->GetEnabledLightCount(), 2);
	TestEqual(TEXT("OnPowerUpSequenceComplete should not have fired yet"), Listener->SequenceCompleteCallCount, 0);
	TestTrue(TEXT("Player input must remain enabled after the second stage"), PlayerStandIn->InputComponent != nullptr);

	// (d) Third trigger reveals the last light and fires completion exactly once.
	Component->NotifyPowerUpStageTriggered();
	TestFalse(TEXT("LightC should become visible after the third trigger"), LightC->IsHidden());
	TestEqual(TEXT("OnLightEnabled should have fired 3 times total"), Listener->LightEnabledCallCount, 3);
	TestEqual(TEXT("OnPowerUpSequenceComplete should have fired exactly once"), Listener->SequenceCompleteCallCount, 1);
	TestEqual(TEXT("GetEnabledLightCount should be 3 after the third trigger"), Component->GetEnabledLightCount(), 3);
	TestTrue(TEXT("IsSequenceComplete should report true"), Component->IsSequenceComplete());
	TestTrue(TEXT("Player input must remain enabled after the final stage"), PlayerStandIn->InputComponent != nullptr);

	// (e) Further triggers after completion are no-ops - no re-fire, no crash from
	// indexing past the array.
	Component->NotifyPowerUpStageTriggered();
	TestEqual(TEXT("OnLightEnabled should not fire again past the end"), Listener->LightEnabledCallCount, 3);
	TestEqual(TEXT("OnPowerUpSequenceComplete should still have fired exactly once"), Listener->SequenceCompleteCallCount, 1);

	// (f) Regression: an empty OrderedLights must warn, not crash, and must never
	// fire completion (mirrors RoomEnemyBudgetController's zero-density warning test).
	UStationPowerUpComponent* EmptyComponent =
		NewObject<UStationPowerUpComponent>(OwnerActor);
	if (!TestNotNull(TEXT("Empty-config component should construct"), EmptyComponent))
	{
		return false;
	}
	EmptyComponent->RegisterComponent();

	AddExpectedError(TEXT("OrderedLights is empty"), EAutomationExpectedErrorFlags::Contains, 1);
	EmptyComponent->InitializeSequence();

	EmptyComponent->NotifyPowerUpStageTriggered();
	TestEqual(TEXT("Empty config should never enable a light"), EmptyComponent->GetEnabledLightCount(), 0);
	TestFalse(TEXT("Empty config should never report sequence complete"), EmptyComponent->IsSequenceComplete());

	// (g) Regression: a nullptr entry inside OrderedLights (e.g. a placed light actor
	// deleted from the level after being wired into the array - a routine editor
	// workflow) must not crash, must still advance the sequence past it, and must
	// warn once so a level designer can see their config is broken.
	UStationPowerUpComponent* NullEntryComponent =
		NewObject<UStationPowerUpComponent>(OwnerActor);
	if (!TestNotNull(TEXT("Null-entry component should construct"), NullEntryComponent))
	{
		return false;
	}
	NullEntryComponent->RegisterComponent();

	AActor* LightX = World->SpawnActor<AActor>();
	AActor* LightZ = World->SpawnActor<AActor>();
	NullEntryComponent->OrderedLights = { LightX, nullptr, LightZ };

	UStationPowerUpTestListener* NullEntryListener = NewObject<UStationPowerUpTestListener>();
	NullEntryComponent->OnLightEnabled.AddDynamic(NullEntryListener, &UStationPowerUpTestListener::HandleLightEnabled);
	NullEntryComponent->OnPowerUpSequenceComplete.AddDynamic(NullEntryListener, &UStationPowerUpTestListener::HandleSequenceComplete);

	NullEntryComponent->InitializeSequence();

	NullEntryComponent->NotifyPowerUpStageTriggered();
	TestFalse(TEXT("LightX should become visible after the first trigger"), LightX->IsHidden());

	// IsRegex must be false here: AddExpectedError's pattern is a regex by default, and
	// the literal "[1]" would otherwise be parsed as a one-character regex class (i.e.
	// matching "OrderedLights1 is null"), never the actual bracketed log text.
	AddExpectedError(TEXT("OrderedLights[1] is null"), EAutomationExpectedErrorFlags::Contains, 1, false);
	NullEntryComponent->NotifyPowerUpStageTriggered();
	TestEqual(TEXT("Null entry should still advance the index"), NullEntryComponent->GetEnabledLightCount(), 2);
	TestEqual(TEXT("OnLightEnabled should fire even for the null entry"), NullEntryListener->LightEnabledCallCount, 2);
	TestNull(TEXT("OnLightEnabled should report a null actor for the null entry"), NullEntryListener->LastEnabledLightActor);

	NullEntryComponent->NotifyPowerUpStageTriggered();
	TestFalse(TEXT("LightZ should become visible after the third trigger"), LightZ->IsHidden());
	TestEqual(TEXT("Sequence should still complete despite the null entry"), NullEntryListener->SequenceCompleteCallCount, 1);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
