#include "Tutorial/TutorialGameMode.h"

#include "Drone.h"
#include "UObject/ConstructorHelpers.h"
#include "Raid/DronePartInventory.h"
#include "Tutorial/TutorialDebris.h"
#include "Tutorial/TutorialPlayerController.h"

ATutorialGameMode::ATutorialGameMode()
{
	PlayerControllerClass = ATutorialPlayerController::StaticClass();
	static ConstructorHelpers::FClassFinder<ADrone> DronePawnClass(TEXT("/Game/Blueprints/BP_Drone"));
	DefaultPawnClass = ADrone::StaticClass();
	if (DronePawnClass.Succeeded())
	{
		DefaultPawnClass = DronePawnClass.Class;
	}
	TutorialDebrisClass = ATutorialDebris::StaticClass();
}

void ATutorialGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);

	if (ATutorialPlayerController* TutorialPC = Cast<ATutorialPlayerController>(NewPlayer))
	{
		if (StartTutorialForController(TutorialPC))
		{
			AdvanceTutorialForController(TutorialPC);
		}
	}
}

bool ATutorialGameMode::StartTutorialForController(ATutorialPlayerController* TutorialPlayerController)
{
	if (!HasAuthority() || !TutorialPlayerController)
	{
		return false;
	}

	if (ADrone* Drone = Cast<ADrone>(TutorialPlayerController->GetPawn()))
	{
		const FName PulseLaser = ADronePartInventory::GetPulseLaserPartID();
		Drone->ApplyLoadout(NAME_None, PulseLaser, PulseLaser);
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

	return TutorialPlayerController->AdvanceTutorialStep();
}

bool ATutorialGameMode::CompleteTutorialForController(ATutorialPlayerController* TutorialPlayerController)
{
	if (!HasAuthority() || !TutorialPlayerController)
	{
		return false;
	}

	return TutorialPlayerController->CompleteTutorial();
}

void ATutorialGameMode::HandleTutorialStepChangedForServer(
	ATutorialPlayerController* TutorialPlayerController,
	ETutorialStep NewStep)
{
	if (HasAuthority() && NewStep == ETutorialStep::DebrisCombat)
	{
		EnsureTutorialDebrisForController(TutorialPlayerController);
	}
}

ATutorialDebris* ATutorialGameMode::EnsureTutorialDebrisForController(
	ATutorialPlayerController* TutorialPlayerController)
{
	if (!HasAuthority() || !TutorialPlayerController)
	{
		return nullptr;
	}

	if (IsValid(TutorialDebris) && !TutorialDebris->IsTutorialDebrisDestroyed())
	{
		return TutorialDebris;
	}

	UWorld* World = GetWorld();
	UClass* SpawnClass = TutorialDebrisClass ? TutorialDebrisClass.Get() : ATutorialDebris::StaticClass();
	if (!World || !SpawnClass)
	{
		return nullptr;
	}

	APawn* TargetPawn = TutorialPlayerController->GetPawn();
	const FVector SpawnLocation = (TargetPawn ? TargetPawn->GetActorLocation() : FVector::ZeroVector)
		+ TutorialDebrisSpawnOffset;
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = TutorialPlayerController;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	TutorialDebris = World->SpawnActor<ATutorialDebris>(
		SpawnClass,
		SpawnLocation,
		FRotator::ZeroRotator,
		SpawnParameters);

	if (TutorialDebris)
	{
		TutorialDebris->SetTargetPawnForServer(TargetPawn);
	}

	return TutorialDebris;
}
