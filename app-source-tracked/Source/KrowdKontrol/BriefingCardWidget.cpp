#include "BriefingCardWidget.h"
#include "HUDChromeColours.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/PanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

void UBriefingCardWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EnsureWidgetTreeBuilt();
}

bool UBriefingCardWidget::Initialize()
{
	const bool bNewlyInitialized = Super::Initialize();
	if (bNewlyInitialized)
	{
		EnsureWidgetTreeBuilt();
	}
	return bNewlyInitialized;
}

void UBriefingCardWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	AdvanceDismissTimer(InDeltaTime);
}

void UBriefingCardWidget::EnsureWidgetTreeBuilt()
{
	// Whichever of NativeOnInitialized()/Initialize() fires first builds the tree;
	// the other is then a no-op, regardless of engine call order between the two -
	// mirrors UOnScreenPromptWidget::EnsureWidgetTreeBuilt().
	if (!RootBorder)
	{
		if (!WidgetTree)
		{
			WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
		}
		BuildWidgetTree();
	}
}

void UBriefingCardWidget::BuildWidgetTree()
{
	// Chrome background/text come from HUDChromeColours (issue #93), shared across
	// all HUD widgets - stays inside PRD 11 REQ-2's desaturated white/gray/black
	// base palette and outside MISSION.md Hard Invariant 3's reserved gameplay
	// colours.
	RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BriefingRootBorder"));
	RootBorder->SetBrushColor(HUDChromeColours::GetBackground());
	// Idle by default - matches UOnScreenPromptWidget::BuildWidgetTree()'s
	// hidden-until-shown precedent; ShowBriefing()/DismissBriefing() toggle this.
	RootBorder->SetVisibility(ESlateVisibility::Collapsed);
	WidgetTree->RootWidget = RootBorder;

	UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BriefingLayout"));
	UPanelSlot* LayoutSlot = RootBorder->SetContent(Layout);
	checkf(LayoutSlot, TEXT("UBriefingCardWidget: SetContent(Layout) returned null"));

	const FSlateColor TextColor(HUDChromeColours::GetText());

	LevelNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LevelNameText"));
	LevelNameText->SetColorAndOpacity(TextColor);
	Layout->AddChildToVerticalBox(LevelNameText);

	ObjectiveText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ObjectiveText"));
	ObjectiveText->SetColorAndOpacity(TextColor);
	Layout->AddChildToVerticalBox(ObjectiveText);

	NewAbilityText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("NewAbilityText"));
	NewAbilityText->SetColorAndOpacity(TextColor);
	Layout->AddChildToVerticalBox(NewAbilityText);
}

void UBriefingCardWidget::ShowBriefing(const FLevelBriefingRow& Row)
{
	if (!RootBorder || !LevelNameText || !ObjectiveText || !NewAbilityText)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UBriefingCardWidget::ShowBriefing: widget tree not built on '%s' - briefing dropped."),
			*GetNameSafe(this));
		return;
	}

	LevelNameText->SetText(Row.LevelDisplayName);

	TArray<FString> ObjectiveLineStrings;
	ObjectiveLineStrings.Reserve(Row.ObjectiveLines.Num());
	for (const FText& Line : Row.ObjectiveLines)
	{
		ObjectiveLineStrings.Add(Line.ToString());
	}
	ObjectiveText->SetText(FText::FromString(FString::Join(ObjectiveLineStrings, TEXT("\n"))));

	if (Row.NewAbilityUnlockLine.IsEmpty())
	{
		NewAbilityText->SetText(FText::GetEmpty());
		NewAbilityText->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		NewAbilityText->SetText(Row.NewAbilityUnlockLine);
		NewAbilityText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	RemainingSeconds = BriefingAutoDismissSeconds;
	// Never ESlateVisibility::Visible - keeps this chrome from intercepting player
	// input, matching UOnScreenPromptWidget::ShowPrompt()'s identical choice; this
	// widget's actual safe state comes from the world pause below, not from
	// blocking clicks.
	RootBorder->SetVisibility(ESlateVisibility::HitTestInvisible);

	UE_LOG(LogTemp, Warning,
		TEXT("UBriefingCardWidget::ShowBriefing: pausing world for %.1fs (level '%s')"),
		BriefingAutoDismissSeconds, *Row.LevelDisplayName.ToString());
	UGameplayStatics::SetGamePaused(GetWorld(), true);
}

void UBriefingCardWidget::DismissBriefing()
{
	RemainingSeconds = 0.0f;
	if (RootBorder)
	{
		RootBorder->SetVisibility(ESlateVisibility::Collapsed);
	}
	// Clears all three fields, not just chrome visibility - mirrors
	// UOnScreenPromptWidget::ClearPromptDisplay()'s identical "blank the text too"
	// shape, so a stale display never lingers behind a collapsed border.
	if (LevelNameText)
	{
		LevelNameText->SetText(FText::GetEmpty());
	}
	if (ObjectiveText)
	{
		ObjectiveText->SetText(FText::GetEmpty());
	}
	if (NewAbilityText)
	{
		NewAbilityText->SetText(FText::GetEmpty());
		NewAbilityText->SetVisibility(ESlateVisibility::Collapsed);
	}
	UE_LOG(LogTemp, Warning, TEXT("UBriefingCardWidget::DismissBriefing: unpausing world"));
	UGameplayStatics::SetGamePaused(GetWorld(), false);
}

void UBriefingCardWidget::AdvanceDismissTimer(float DeltaSeconds)
{
	if (RemainingSeconds > 0.0f)
	{
		RemainingSeconds = FMath::Max(0.0f, RemainingSeconds - DeltaSeconds);
		if (RemainingSeconds <= 0.0f)
		{
			DismissBriefing();
		}
	}
}

FText UBriefingCardWidget::GetLevelNameDisplayText() const
{
	return LevelNameText ? LevelNameText->GetText() : FText::GetEmpty();
}

FText UBriefingCardWidget::GetObjectiveDisplayText() const
{
	return ObjectiveText ? ObjectiveText->GetText() : FText::GetEmpty();
}

FText UBriefingCardWidget::GetNewAbilityDisplayText() const
{
	return NewAbilityText ? NewAbilityText->GetText() : FText::GetEmpty();
}
