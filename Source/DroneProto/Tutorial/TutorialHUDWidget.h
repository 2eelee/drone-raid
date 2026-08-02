#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Tutorial/TutorialTypes.h"
#include "TutorialHUDWidget.generated.h"

class UTextBlock;
class UWidget;

UCLASS()
class DRONEPROTO_API UTutorialHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Tutorial|UI")
	void RefreshTutorial(
		ETutorialStep Step,
		int32 DialogueIndex,
		const FText& InDialogueText,
		bool bDialogueReady);

	UFUNCTION(BlueprintPure, Category = "Tutorial|UI")
	FText GetDialogueText() const { return CachedDialogueText; }

	UFUNCTION(BlueprintPure, Category = "Tutorial|UI")
	FText GetStepText() const { return CachedStepText; }

	UFUNCTION(BlueprintPure, Category = "Tutorial|UI")
	FText GetInputHintText() const { return CachedInputHintText; }

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Tutorial|UI")
	TObjectPtr<UWidget> DialoguePanel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Tutorial|UI")
	TObjectPtr<UTextBlock> DialogueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Tutorial|UI")
	TObjectPtr<UTextBlock> StepText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Tutorial|UI")
	TObjectPtr<UTextBlock> InputHintText;

private:
	FText CachedDialogueText;
	FText CachedStepText;
	FText CachedInputHintText;

	static FText BuildStepText(ETutorialStep Step);
	static FText BuildInputHintText(ETutorialStep Step, bool bDialogueReady);
	static void SetOptionalText(UTextBlock* TextBlock, const FText& Text);
};
