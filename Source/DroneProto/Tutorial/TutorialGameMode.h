#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TutorialGameMode.generated.h"

class ATutorialPlayerController;

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
};
