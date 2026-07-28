#include "Tutorial/TutorialPlayerController.h"

#include "Components/InputComponent.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Tutorial/TutorialGameMode.h"

void ATutorialPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!InputComponent)
	{
		return;
	}

	InputComponent->BindKey(EKeys::Left, IE_Pressed, this, &ATutorialPlayerController::HandleTutorialLeftPressed);
	InputComponent->BindKey(EKeys::Z, IE_Pressed, this, &ATutorialPlayerController::HandleTutorialAttackPressed);
	InputComponent->BindKey(EKeys::Up, IE_Pressed, this, &ATutorialPlayerController::HandleTutorialUpPressed);
	InputComponent->BindKey(EKeys::Up, IE_Released, this, &ATutorialPlayerController::HandleTutorialUpReleased);
	InputComponent->BindKey(EKeys::C, IE_Pressed, this, &ATutorialPlayerController::HandleTutorialDodgePressed);
}

void ATutorialPlayerController::StartTutorial()
{
	if (CurrentTutorialStep == ETutorialStep::Complete)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Tutorial Step=%s Result=Ignored Reason=AlreadyComplete Controller=%s"),
			ToTutorialStepLogString(CurrentTutorialStep),
			*GetNameSafe(this));
		return;
	}

	bReturnToLobbyRequested = false;
	bTutorialActive = true;
	SetTutorialStep(ETutorialStep::Start, FName(TEXT("Start")));
}

bool ATutorialPlayerController::AdvanceTutorialStep()
{
	switch (CurrentTutorialStep)
	{
	case ETutorialStep::None:
		StartTutorial();
		return true;
	case ETutorialStep::Start:
		SetTutorialStep(ETutorialStep::WorldBriefing, FName(TEXT("StartPresented")));
		return true;
	case ETutorialStep::WorldBriefing:
		SetTutorialStep(ETutorialStep::Move, FName(TEXT("WorldBriefingComplete")));
		return true;
	case ETutorialStep::CombatBriefing:
		SetTutorialStep(ETutorialStep::DebrisCombat, FName(TEXT("CombatBriefingComplete")));
		return true;
	case ETutorialStep::ClosingBriefing:
		return CompleteTutorial();
	case ETutorialStep::Complete:
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Tutorial Step=Complete Result=Ignored Reason=AlreadyComplete Controller=%s"),
			*GetNameSafe(this));
		return false;
	default:
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Tutorial Step=%s Result=Ignored Reason=RequiresGameplaySuccess Controller=%s"),
			ToTutorialStepLogString(CurrentTutorialStep),
			*GetNameSafe(this));
		return false;
	}
}

bool ATutorialPlayerController::CompleteTutorial()
{
	if (!bTutorialActive || CurrentTutorialStep != ETutorialStep::ClosingBriefing)
	{
		return false;
	}

	bTutorialActive = false;
	SetTutorialStep(ETutorialStep::Complete, FName(TEXT("Complete")));
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Tutorial Complete Controller=%s"),
		*GetNameSafe(this));
	BP_OnTutorialComplete();
	return true;
}

bool ATutorialPlayerController::NotifyTutorialMoveInput(FVector2D RawAxis)
{
	if (!bTutorialActive || CurrentTutorialStep != ETutorialStep::Move || RawAxis.IsNearlyZero())
	{
		return false;
	}

	SetTutorialStep(ETutorialStep::Attack, FName(TEXT("MoveSucceeded")));
	return true;
}

bool ATutorialPlayerController::NotifyTutorialAttackInput()
{
	if (!bTutorialActive || CurrentTutorialStep != ETutorialStep::Attack)
	{
		return false;
	}

	SetTutorialStep(ETutorialStep::Dodge, FName(TEXT("AttackSucceeded")));
	return true;
}

bool ATutorialPlayerController::NotifyTutorialDodgeInput(FVector2D RawDirection)
{
	if (!bTutorialActive || CurrentTutorialStep != ETutorialStep::Dodge || RawDirection.IsNearlyZero())
	{
		return false;
	}

	SetTutorialStep(ETutorialStep::CombatBriefing, FName(TEXT("DodgeSucceeded")));
	return true;
}

bool ATutorialPlayerController::NotifyTutorialDebrisDestroyed()
{
	if (!bTutorialActive || CurrentTutorialStep != ETutorialStep::DebrisCombat)
	{
		return false;
	}

	SetTutorialStep(ETutorialStep::ClosingBriefing, FName(TEXT("DebrisDestroyed")));
	return true;
}

bool ATutorialPlayerController::IsTutorialMoveAllowed() const
{
	return bTutorialActive
		&& (CurrentTutorialStep == ETutorialStep::Move
			|| CurrentTutorialStep == ETutorialStep::CombatBriefing
			|| CurrentTutorialStep == ETutorialStep::DebrisCombat);
}

bool ATutorialPlayerController::IsTutorialAttackAllowed() const
{
	return bTutorialActive
		&& (CurrentTutorialStep == ETutorialStep::Attack
			|| CurrentTutorialStep == ETutorialStep::CombatBriefing
			|| CurrentTutorialStep == ETutorialStep::DebrisCombat);
}

bool ATutorialPlayerController::IsTutorialDodgeAllowed() const
{
	return bTutorialActive
		&& (CurrentTutorialStep == ETutorialStep::Dodge
			|| CurrentTutorialStep == ETutorialStep::CombatBriefing
			|| CurrentTutorialStep == ETutorialStep::DebrisCombat);
}

bool ATutorialPlayerController::ReturnToLobbyAfterTutorial()
{
	if (!IsTutorialComplete())
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReturnToLobbyIgnored Reason=TutorialIncomplete Source=Tutorial Controller=%s Step=%s"),
			*GetNameSafe(this),
			ToTutorialStepLogString(CurrentTutorialStep));
		return false;
	}

	if (bReturnToLobbyRequested)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReturnToLobbyIgnored Reason=AlreadyRequested Source=Tutorial Controller=%s"),
			*GetNameSafe(this));
		return false;
	}

	bReturnToLobbyRequested = true;
	BP_OnReturnToLobbyAfterTutorial(LobbyMapName);

#if WITH_DEV_AUTOMATION_TESTS
	if (bSuppressTutorialLobbyTravelForTest)
	{
		++TutorialLobbyTravelRequestCountForTest;
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReturnToLobby Reason=TutorialComplete Target=%s Source=Tutorial Controller=%s Mode=SuppressedForAutomation"),
			*LobbyMapName.ToString(),
			*GetNameSafe(this));
		return true;
	}
#endif

	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReturnToLobbyIgnored Reason=InvalidWorldOrDedicatedServer Source=Tutorial Controller=%s"),
			*GetNameSafe(this));
		return false;
	}

	if (!IsLocalController())
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReturnToLobbyIgnored Reason=NotLocalController Source=Tutorial Controller=%s"),
			*GetNameSafe(this));
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReturnToLobby Reason=TutorialComplete Target=%s Source=Tutorial Controller=%s Method=OpenLevel"),
		*LobbyMapName.ToString(),
		*GetNameSafe(this));
	UGameplayStatics::OpenLevel(World, LobbyMapName);
	return true;
}

void ATutorialPlayerController::SetTutorialStep(ETutorialStep NewStep, FName Reason)
{
	if (CurrentTutorialStep == NewStep)
	{
		return;
	}

	const ETutorialStep PreviousStep = CurrentTutorialStep;
	CurrentTutorialStep = NewStep;

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Tutorial Step=%s Previous=%s Reason=%s Controller=%s"),
		ToTutorialStepLogString(CurrentTutorialStep),
		ToTutorialStepLogString(PreviousStep),
		Reason.IsNone() ? TEXT("None") : *Reason.ToString(),
		*GetNameSafe(this));

	BP_OnTutorialStepChanged(PreviousStep, CurrentTutorialStep);

	if (HasAuthority())
	{
		UWorld* World = GetWorld();
		ATutorialGameMode* TutorialGameMode = World ? World->GetAuthGameMode<ATutorialGameMode>() : nullptr;
		if (!TutorialGameMode && World)
		{
			for (TActorIterator<ATutorialGameMode> It(World); It; ++It)
			{
				TutorialGameMode = *It;
				break;
			}
		}
		if (TutorialGameMode)
		{
			TutorialGameMode->HandleTutorialStepChangedForServer(this, CurrentTutorialStep);
		}
	}
}

void ATutorialPlayerController::HandleTutorialLeftPressed()
{
	NotifyTutorialMoveInput(FVector2D(-1.0f, 0.0f));
}

void ATutorialPlayerController::HandleTutorialAttackPressed()
{
	NotifyTutorialAttackInput();
}

void ATutorialPlayerController::HandleTutorialDodgePressed()
{
	if (bUpPressedForTutorialDodge)
	{
		NotifyTutorialDodgeInput(FVector2D(0.0f, 1.0f));
	}
}

void ATutorialPlayerController::HandleTutorialUpPressed()
{
	bUpPressedForTutorialDodge = true;
}

void ATutorialPlayerController::HandleTutorialUpReleased()
{
	bUpPressedForTutorialDodge = false;
}
