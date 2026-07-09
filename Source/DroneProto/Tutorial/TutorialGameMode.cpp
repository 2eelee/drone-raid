#include "Tutorial/TutorialGameMode.h"

#include "Tutorial/TutorialPlayerController.h"

ATutorialGameMode::ATutorialGameMode()
{
	PlayerControllerClass = ATutorialPlayerController::StaticClass();
}

bool ATutorialGameMode::StartTutorialForController(ATutorialPlayerController* TutorialPlayerController)
{
	if (!HasAuthority() || !TutorialPlayerController)
	{
		return false;
	}

	TutorialPlayerController->StartTutorial();
	return true;
}

bool ATutorialGameMode::AdvanceTutorialForController(ATutorialPlayerController* TutorialPlayerController)
{
	if (!HasAuthority() || !TutorialPlayerController)
	{
		return false;
	}

	TutorialPlayerController->AdvanceTutorialStep();
	return true;
}

bool ATutorialGameMode::CompleteTutorialForController(ATutorialPlayerController* TutorialPlayerController)
{
	if (!HasAuthority() || !TutorialPlayerController)
	{
		return false;
	}

	TutorialPlayerController->CompleteTutorial();
	return true;
}
