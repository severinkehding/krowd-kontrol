#include "OnScreenPromptWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

void UOnScreenPromptWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EnsureWidgetTreeBuilt();
}

bool UOnScreenPromptWidget::Initialize()
{
	const bool bNewlyInitialized = Super::Initialize();
	if (bNewlyInitialized)
	{
		EnsureWidgetTreeBuilt();
	}
	return bNewlyInitialized;
}

void UOnScreenPromptWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	AdvanceDismissTimer(InDeltaTime);
}

void UOnScreenPromptWidget::EnsureWidgetTreeBuilt()
{
	// Whichever of NativeOnInitialized()/Initialize() fires first builds the tree; the
	// other is then a no-op, regardless of engine call order between the two.
	if (!PromptBorder)
	{
		// UUserWidget::WidgetTree is normally lazily created inside Initialize()
		// (before it conditionally calls NativeOnInitialized()) - but NativeOnInitialized()
		// can also be invoked directly, bypassing Initialize() entirely (e.g. this class's
		// own Automation test exercises that call order to prove idempotency). WidgetTree
		// would still be null in that case, and WidgetTree->ConstructWidget<T>() on a null
		// WidgetTree doesn't crash on the call itself - it silently passes a null Outer
		// into NewObject<T>(), which the engine then treats as fatal. Mirror
		// UUserWidget::Initialize()'s own lazy-creation exactly so this is safe regardless
		// of call order - same fix as UAbilityCooldownTrayWidget::EnsureWidgetTreeBuilt().
		if (!WidgetTree)
		{
			WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
		}
		BuildWidgetTree();
	}
}

void UOnScreenPromptWidget::BuildWidgetTree()
{
	// Desaturated near-black background + light-gray (not pure white) text -
	// MISSION.md Hard Invariant 3 / PRD 13 REQ-4 / PRD 11 REQ-1 reserve
	// Purple/Teal/Orange/Blue/White for gameplay information; this prompt's chrome must
	// not use any of them. Same already-reviewed palette as
	// UAbilityCooldownTrayWidget/UPostRunSummaryWidget.
	const FLinearColor ChromeBackgroundColor(0.05f, 0.05f, 0.05f, 0.92f);
	const FSlateColor TextColor(FLinearColor(0.85f, 0.85f, 0.85f, 1.0f));

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PromptRootCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	PromptBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PromptBorder"));
	PromptBorder->SetBrushColor(ChromeBackgroundColor);

	UCanvasPanelSlot* PromptSlot = RootCanvas->AddChildToCanvas(PromptBorder);
	checkf(PromptSlot, TEXT("OnScreenPromptWidget: AddChildToCanvas(PromptBorder) returned null"));
	// Top-center anchoring - visually distinct from UEnergyMeterWidget's top-left and
	// UAbilityCooldownTrayWidget's bottom-right corner HUD chrome, appropriate for a
	// transient, attention-grabbing cue rather than persistent status chrome.
	PromptSlot->SetAnchors(FAnchors(0.5f, 0.0f, 0.5f, 0.0f));
	PromptSlot->SetAlignment(FVector2D(0.5f, 0.0f));
	PromptSlot->SetAutoSize(true);
	PromptSlot->SetPosition(FVector2D(0.0f, 24.0f));

	PromptText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PromptText"));
	PromptText->SetColorAndOpacity(TextColor);
	PromptBorder->SetContent(PromptText);

	// Idle by default - this is what keeps the widget non-blocking and invisible
	// until ShowPrompt() is called.
	PromptBorder->SetVisibility(ESlateVisibility::Collapsed);
}

void UOnScreenPromptWidget::ShowPrompt(const FText& Message, float DurationSeconds)
{
	if (!PromptText || !PromptBorder)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UOnScreenPromptWidget::ShowPrompt: widget tree not built on '%s' - prompt dropped."),
			*GetNameSafe(this));
		return;
	}

	PromptText->SetText(Message);
	// This clamp is the hard cap enforcement - no caller can make a prompt persist
	// beyond MaxPromptDurationSeconds.
	RemainingSeconds = FMath::Clamp(DurationSeconds, 0.0f, MaxPromptDurationSeconds);

	if (RemainingSeconds > 0.0f)
	{
		// Never ESlateVisibility::Visible - this is what guarantees the prompt never
		// intercepts player input, matching AbilityCooldownTrayWidget.cpp's identical
		// choice for CooldownText.
		PromptBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		ClearPromptDisplay();
	}
}

void UOnScreenPromptWidget::AdvanceDismissTimer(float DeltaSeconds)
{
	if (RemainingSeconds > 0.0f)
	{
		RemainingSeconds = FMath::Max(0.0f, RemainingSeconds - DeltaSeconds);
		if (RemainingSeconds <= 0.0f)
		{
			ClearPromptDisplay();
		}
	}
}

void UOnScreenPromptWidget::ClearPromptDisplay()
{
	if (PromptText)
	{
		PromptText->SetText(FText::GetEmpty());
	}
	if (PromptBorder)
	{
		PromptBorder->SetVisibility(ESlateVisibility::Collapsed);
	}
}

bool UOnScreenPromptWidget::IsPromptVisible() const
{
	return RemainingSeconds > 0.0f;
}

float UOnScreenPromptWidget::GetRemainingSeconds() const
{
	return RemainingSeconds;
}

FText UOnScreenPromptWidget::GetPromptDisplayText() const
{
	return PromptText ? PromptText->GetText() : FText::GetEmpty();
}

namespace
{
	// Dev-only trigger, mirroring UPostRunSummaryWidget::ShowPostRunSummary (issue #74):
	// adds the widget to the local player's viewport and fires a placeholder prompt, so
	// it can be visually confirmed through a real player-observable path rather than
	// only direct construction in test code.
	void ShowOnScreenPrompt(const TArray<FString>& Args, UWorld* World)
	{
		APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
		if (!PlayerController)
		{
			return;
		}
		if (UOnScreenPromptWidget* Widget = CreateWidget<UOnScreenPromptWidget>(PlayerController, UOnScreenPromptWidget::StaticClass()))
		{
			Widget->AddToViewport();
			Widget->ShowPrompt(NSLOCTEXT("OnScreenPromptWidget", "DevPlaceholder", "Match the colour to the enemy."));
		}
	}

	FAutoConsoleCommandWithWorldAndArgs ShowOnScreenPromptCommand(
		TEXT("KrowdKontrol.ShowOnScreenPrompt"),
		TEXT("Dev-only: adds UOnScreenPromptWidget to the local player's viewport and fires a placeholder prompt, so it can be visually confirmed through a real player-observable path rather than only direct construction in test code."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ShowOnScreenPrompt));
}
