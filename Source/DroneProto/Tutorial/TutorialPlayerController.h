#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TutorialTypes.h"
#include "TutorialPlayerController.generated.h"

UCLASS()
class DRONEPROTO_API ATutorialPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void StartTutorial();

	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	bool AdvanceTutorialStep();

	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	bool CompleteTutorial();

	UFUNCTION(BlueprintCallable, Category = "Tutorial|Input")
	bool NotifyTutorialMoveInput(FVector2D RawAxis);

	UFUNCTION(BlueprintCallable, Category = "Tutorial|Input")
	bool NotifyTutorialAttackInput();

	UFUNCTION(BlueprintCallable, Category = "Tutorial|Input")
	bool NotifyTutorialDodgeInput(FVector2D RawDirection);

	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	bool NotifyTutorialDebrisDestroyed();

	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	bool ReturnToLobbyAfterTutorial();

	UFUNCTION(BlueprintPure, Category = "Tutorial")
	ETutorialStep GetCurrentTutorialStep() const { return CurrentTutorialStep; }

	UFUNCTION(BlueprintPure, Category = "Tutorial")
	bool IsTutorialComplete() const { return CurrentTutorialStep == ETutorialStep::Complete; }

	UFUNCTION(BlueprintPure, Category = "Tutorial")
	bool IsTutorialActive() const { return bTutorialActive; }

	UFUNCTION(BlueprintPure, Category = "Tutorial|Dialogue")
	int32 GetCurrentTutorialDialogueIndex() const { return CurrentTutorialDialogueIndex; }

	UFUNCTION(BlueprintPure, Category = "Tutorial|Dialogue")
	FText GetCurrentTutorialDialogueText() const;

	UFUNCTION(BlueprintPure, Category = "Tutorial|Dialogue")
	bool IsCurrentTutorialDialogueReady() const;

	UFUNCTION(BlueprintCallable, Category = "Tutorial|Dialogue")
	bool TryAdvanceTutorialDialogue();

	UFUNCTION(BlueprintPure, Category = "Tutorial|Input")
	bool IsTutorialMoveAllowed() const;

	UFUNCTION(BlueprintPure, Category = "Tutorial|Input")
	bool IsTutorialAttackAllowed() const;

	UFUNCTION(BlueprintPure, Category = "Tutorial|Input")
	bool IsTutorialDodgeAllowed() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "Tutorial|UI")
	void BP_OnTutorialStepChanged(ETutorialStep PreviousStep, ETutorialStep NewStep);

	UFUNCTION(BlueprintImplementableEvent, Category = "Tutorial|UI")
	void BP_OnTutorialDialogueChanged(ETutorialStep Step, int32 DialogueIndex, const FText& DialogueText);

	UFUNCTION(BlueprintImplementableEvent, Category = "Tutorial|UI")
	void BP_OnTutorialComplete();

	UFUNCTION(BlueprintImplementableEvent, Category = "Tutorial|UI")
	void BP_OnReturnToLobbyAfterTutorial(FName TargetMap);

#if WITH_DEV_AUTOMATION_TESTS
	void SetSuppressTutorialLobbyTravelForTest(bool bInSuppressTravel) { bSuppressTutorialLobbyTravelForTest = bInSuppressTravel; }
	int32 GetTutorialLobbyTravelRequestCountForTest() const { return TutorialLobbyTravelRequestCountForTest; }
#endif

protected:
	virtual void SetupInputComponent() override;

private:
	UFUNCTION(Client, Reliable)
	void Client_PersistTutorialCompletion();

	UFUNCTION(Server, Reliable)
	void Server_RequestAdvanceTutorialDialogue();

	UFUNCTION(Client, Reliable)
	void Client_SyncTutorialPresentation(ETutorialStep Step, int32 DialogueIndex);

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Tutorial", meta = (AllowPrivateAccess = "true"))
	ETutorialStep CurrentTutorialStep = ETutorialStep::None;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Tutorial|Dialogue", meta = (AllowPrivateAccess = "true"))
	int32 CurrentTutorialDialogueIndex = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Tutorial", meta = (AllowPrivateAccess = "true"))
	bool bTutorialActive = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tutorial", meta = (AllowPrivateAccess = "true"))
	FName LobbyMapName = FName(TEXT("LobbyMap"));

	bool bReturnToLobbyRequested = false;
	bool bUpPressedForTutorialDodge = false;

#if WITH_DEV_AUTOMATION_TESTS
	bool bSuppressTutorialLobbyTravelForTest = false;
	int32 TutorialLobbyTravelRequestCountForTest = 0;
#endif

	void SetTutorialStep(ETutorialStep NewStep, FName Reason);
	bool AdvanceTutorialDialogueForServer();
	void SyncTutorialPresentationForOwner(ETutorialStep PreviousStep);
	void HandleTutorialLeftPressed();
	void HandleTutorialDodgePressed();
	void HandleTutorialUpPressed();
	void HandleTutorialUpReleased();
};
