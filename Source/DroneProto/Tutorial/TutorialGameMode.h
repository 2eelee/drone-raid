#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Tutorial/TutorialTypes.h"
#include "TutorialGameMode.generated.h"

class ATutorialPlayerController;
class ATutorialDebris;

UCLASS()
class DRONEPROTO_API ATutorialGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATutorialGameMode();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Tutorial")
	bool StartTutorialForController(ATutorialPlayerController* TutorialPlayerController);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Tutorial")
	bool AdvanceTutorialForController(ATutorialPlayerController* TutorialPlayerController);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Tutorial")
	bool CompleteTutorialForController(ATutorialPlayerController* TutorialPlayerController);

	void HandleTutorialStepChangedForServer(
		ATutorialPlayerController* TutorialPlayerController,
		ETutorialStep NewStep);

	UFUNCTION(BlueprintPure, Category = "Tutorial")
	ATutorialDebris* GetTutorialDebris() const { return TutorialDebris; }

protected:
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Tutorial")
	TSubclassOf<ATutorialDebris> TutorialDebrisClass;

	UPROPERTY()
	TObjectPtr<ATutorialDebris> TutorialDebris;

	UPROPERTY(EditDefaultsOnly, Category = "Tutorial", meta = (Units = "cm"))
	FVector TutorialDebrisSpawnOffset = FVector(600.0f, 0.0f, 0.0f);

	ATutorialDebris* EnsureTutorialDebrisForController(ATutorialPlayerController* TutorialPlayerController);
};
