#include "PostRunSummaryWidget.h"
#include "HUDChromeColours.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/PanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

void UPostRunSummaryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EnsureWidgetTreeBuilt();
}

bool UPostRunSummaryWidget::Initialize()
{
	const bool bNewlyInitialized = Super::Initialize();
	if (bNewlyInitialized)
	{
		EnsureWidgetTreeBuilt();
	}
	return bNewlyInitialized;
}

void UPostRunSummaryWidget::EnsureWidgetTreeBuilt()
{
	// Whichever of NativeOnInitialized()/Initialize() fires first builds the tree;
	// the other is then a no-op, regardless of engine call order between the two.
	if (!ClearTimeText)
	{
		// UUserWidget::WidgetTree is normally lazily created inside Initialize()
		// (before it conditionally calls NativeOnInitialized()) - but
		// NativeOnInitialized() can also be invoked directly, bypassing Initialize()
		// entirely (this class's own Automation test exercises that call order to
		// prove idempotency). WidgetTree would still be null in that case, and
		// WidgetTree->ConstructWidget<T>() on a null WidgetTree doesn't crash on the
		// call itself - it silently passes a null Outer into NewObject<T>(), which
		// the engine then treats as fatal. Mirror UUserWidget::Initialize()'s own
		// lazy-creation exactly so this is safe regardless of call order - same fix
		// as UAbilityCooldownTrayWidget::EnsureWidgetTreeBuilt() (issue #66).
		if (!WidgetTree)
		{
			WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
		}
		BuildWidgetTree();
		SetSummaryValues(PlaceholderClearTimeSeconds, PlaceholderCrowdMasteryCount);
	}
}

void UPostRunSummaryWidget::BuildWidgetTree()
{
	// Chrome background/text come from HUDChromeColours (issue #93), shared across all
	// HUD widgets - stays inside PRD 11 REQ-2's desaturated white/gray/black base
	// palette and outside MISSION.md Hard Invariant 3's reserved gameplay colours.
	RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SummaryRootBorder"));
	RootBorder->SetBrushColor(HUDChromeColours::GetBackground());
	WidgetTree->RootWidget = RootBorder;

	UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SummaryLayout"));
	UPanelSlot* LayoutSlot = RootBorder->SetContent(Layout);
	checkf(LayoutSlot, TEXT("UPostRunSummaryWidget: SetContent(Layout) returned null"));

	const FSlateColor TextColor(HUDChromeColours::GetText());

	ClearTimeText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ClearTimeText"));
	ClearTimeText->SetColorAndOpacity(TextColor);
	Layout->AddChildToVerticalBox(ClearTimeText);

	CrowdMasteryText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CrowdMasteryText"));
	CrowdMasteryText->SetColorAndOpacity(TextColor);
	Layout->AddChildToVerticalBox(CrowdMasteryText);
}

void UPostRunSummaryWidget::SetSummaryValues(float ClearTimeSeconds, int32 CrowdMasteryCount)
{
	const int32 ClampedSeconds = FMath::Max(0, FMath::RoundToInt(ClearTimeSeconds));
	const int32 Minutes = ClampedSeconds / 60;
	const int32 Seconds = ClampedSeconds % 60;
	const FText ClearTimeDisplay = FText::Format(
		NSLOCTEXT("PostRunSummaryWidget", "ClearTimeFormat", "Clear Time: {0}:{1}"),
		FText::AsNumber(Minutes),
		FText::FromString(FString::Printf(TEXT("%02d"), Seconds)));
	SetTextBlockSafe(ClearTimeText, ClearTimeDisplay, TEXT("ClearTimeText"));

	const FText CrowdMasteryDisplay = FText::Format(
		NSLOCTEXT("PostRunSummaryWidget", "CrowdMasteryFormat", "Crowd Mastery: {0}"),
		FText::AsNumber(FMath::Max(0, CrowdMasteryCount)));
	SetTextBlockSafe(CrowdMasteryText, CrowdMasteryDisplay, TEXT("CrowdMasteryText"));
}

void UPostRunSummaryWidget::SetTextBlockSafe(UTextBlock* TextBlock, const FText& Text, const TCHAR* FieldName) const
{
	if (TextBlock)
	{
		TextBlock->SetText(Text);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UPostRunSummaryWidget: %s is null on '%s' - field will render blank."),
			FieldName, *GetNameSafe(this));
	}
}

FText UPostRunSummaryWidget::GetClearTimeDisplayText() const
{
	return ClearTimeText ? ClearTimeText->GetText() : FText::GetEmpty();
}

FText UPostRunSummaryWidget::GetCrowdMasteryDisplayText() const
{
	return CrowdMasteryText ? CrowdMasteryText->GetText() : FText::GetEmpty();
}

namespace
{
	// Dev-only trigger (issue #74 E2E gap): the widget was previously only ever
	// constructed directly by test code (CreateWidget/NewObject in
	// KrowdKontrolPostRunSummaryWidgetTest.cpp), leaving no in-game path an E2E pass
	// could use to visually confirm the rendered clear-time/Crowd-Mastery text or
	// chrome colours. Adds the widget to the local player's viewport with its
	// existing placeholder values - this is an observation path only, not a real
	// gameplay trigger (a future real "run cleared" event replaces this call site).
	void ShowPostRunSummary(const TArray<FString>& Args, UWorld* World)
	{
		APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
		if (!PlayerController)
		{
			return;
		}
		if (UPostRunSummaryWidget* Widget = CreateWidget<UPostRunSummaryWidget>(PlayerController, UPostRunSummaryWidget::StaticClass()))
		{
			Widget->AddToViewport();
		}
	}

	FAutoConsoleCommandWithWorldAndArgs ShowPostRunSummaryCommand(
		TEXT("KrowdKontrol.ShowPostRunSummary"),
		TEXT("Dev-only: adds UPostRunSummaryWidget to the local player's viewport with its placeholder clear-time/Crowd-Mastery values, so it can be visually confirmed through a real player-observable path rather than only direct construction in test code."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ShowPostRunSummary));
}
