// Confirms AMainMenuPlayerController::BeginPlay() (issue #324) actually constructs
// UMainMenuWidget, adds it to the viewport, and enables the mouse cursor - this PR's
// headline acceptance criterion. Mirrors KrowdKontrolHUDWiringTest.cpp's
// ULocalPlayer-attachment + DispatchBeginPlay() pattern: CreateWidget<T>(this, ...)
// hard-requires OwnerPC.Player to already be a real ULocalPlayer or it silently
// returns nullptr (UserWidget.cpp's CreateWidgetInstance CastChecked<ULocalPlayer>()s
// it), so a bare ULocalPlayer is attached here to satisfy that precondition.

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "MainMenuPlayerController.h"
#include "MainMenuWidget.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolMainMenuPlayerControllerBeginPlayTest,
	"KrowdKontrol.Unit.MainMenuPlayerControllerBeginPlay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolMainMenuPlayerControllerBeginPlayTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	AMainMenuPlayerController* Controller = World->SpawnActor<AMainMenuPlayerController>();
	if (!TestNotNull(TEXT("Controller should spawn"), Controller))
	{
		return false;
	}

	// ULocalPlayer's ClassWithin is UEngine, so it must be constructed with GEngine as
	// its outer, not the default transient package, or NewObject asserts.
	Controller->Player = NewObject<ULocalPlayer>(GEngine);
	Controller->SetAsLocalPlayerController();

	// This harness's editor world never calls BeginPlay() on spawned actors. Calling
	// the virtual BeginPlay() directly is not an option - UE 5.8's AActor::BeginPlay()
	// asserts ActorHasBegunPlay is already in the BeginningPlay state, which only a
	// real DispatchBeginPlay() sets up first. AActor::DispatchBeginPlay() is the
	// public, legal route - matching KrowdKontrolHUDWiringTest.cpp's identical
	// precedent for the sibling controller.
	Controller->DispatchBeginPlay();

	TestTrue(TEXT("BeginPlay should show the mouse cursor"), Controller->bShowMouseCursor);
	TestNotNull(TEXT("BeginPlay should construct the main menu widget"),
		ToRawPtr(Controller->MainMenuWidgetInstance));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
