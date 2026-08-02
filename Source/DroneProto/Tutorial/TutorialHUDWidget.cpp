#include "Tutorial/TutorialHUDWidget.h"

#include "Components/TextBlock.h"
#include "Components/Widget.h"

void UTutorialHUDWidget::RefreshTutorial(
	ETutorialStep Step,
	int32 DialogueIndex,
	const FText& InDialogueText,
	bool bDialogueReady)
{
	(void)DialogueIndex;

	CachedDialogueText = InDialogueText;
	CachedStepText = BuildStepText(Step);
	CachedInputHintText = BuildInputHintText(Step, bDialogueReady);

	SetOptionalText(DialogueText, CachedDialogueText);
	SetOptionalText(StepText, CachedStepText);
	SetOptionalText(InputHintText, CachedInputHintText);

	if (DialoguePanel)
	{
		const bool bIsActiveStep = Step != ETutorialStep::None && Step != ETutorialStep::Complete;
		DialoguePanel->SetVisibility(
			bIsActiveStep ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (DialogueText)
	{
		DialogueText->SetVisibility(
			CachedDialogueText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}

	SetVisibility(Step == ETutorialStep::Complete
		? ESlateVisibility::Collapsed
		: ESlateVisibility::SelfHitTestInvisible);
}

FText UTutorialHUDWidget::BuildStepText(ETutorialStep Step)
{
	switch (Step)
	{
	case ETutorialStep::Start:
		return FText::FromString(TEXT("1. 시작"));
	case ETutorialStep::WorldBriefing:
		return FText::FromString(TEXT("2. 세계 안내"));
	case ETutorialStep::Move:
		return FText::FromString(TEXT("3. 이동"));
	case ETutorialStep::Attack:
		return FText::FromString(TEXT("4. 공격"));
	case ETutorialStep::Dodge:
		return FText::FromString(TEXT("5. 회피"));
	case ETutorialStep::CombatBriefing:
		return FText::FromString(TEXT("6. 전투 안내"));
	case ETutorialStep::DebrisCombat:
		return FText::FromString(TEXT("7. 잔해 전투"));
	case ETutorialStep::ClosingBriefing:
		return FText::FromString(TEXT("8. 마무리"));
	default:
		return FText::GetEmpty();
	}
}

FText UTutorialHUDWidget::BuildInputHintText(ETutorialStep Step, bool bDialogueReady)
{
	switch (Step)
	{
	case ETutorialStep::WorldBriefing:
	case ETutorialStep::CombatBriefing:
	case ETutorialStep::ClosingBriefing:
		return FText::FromString(TEXT("Z : 다음"));
	case ETutorialStep::Move:
		return FText::FromString(bDialogueReady ? TEXT("방향키 : 이동") : TEXT("Z : 다음"));
	case ETutorialStep::Attack:
		return FText::FromString(bDialogueReady ? TEXT("Z : 공격") : TEXT("Z : 다음"));
	case ETutorialStep::Dodge:
		return FText::FromString(bDialogueReady ? TEXT("방향키 + C : 회피") : TEXT("Z : 다음"));
	case ETutorialStep::DebrisCombat:
		return FText::FromString(TEXT("Z : 잔해 공격 (3회)"));
	default:
		return FText::GetEmpty();
	}
}

void UTutorialHUDWidget::SetOptionalText(UTextBlock* TextBlock, const FText& Text)
{
	if (TextBlock)
	{
		TextBlock->SetText(Text);
	}
}
