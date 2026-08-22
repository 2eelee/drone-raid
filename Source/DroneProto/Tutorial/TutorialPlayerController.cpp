#include "Tutorial/TutorialPlayerController.h"

#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Lobby/RaidSessionSubsystem.h"
#include "Tutorial/TutorialGameMode.h"
#include "Tutorial/TutorialHUDWidget.h"

namespace
{
const TArray<FText>& GetTutorialDialogueLines(ETutorialStep Step)
{
	static const TArray<FText> Empty;
	static const TArray<FText> WorldBriefing{
		FText::FromString(TEXT("어서오세요. 새로운 클리너님.")),
		FText::FromString(TEXT("클리너님은 우주 속 먼지를 청소하여, 우주의 환경을 보전하는 중대한 임무를 맡고 있습니다.")),
		FText::FromString(TEXT("클리너님의 인격 정보는 유틸리티 소프트웨어 형태로 지금 보이는 \"드론\" 속에 들어있습니다.")),
		FText::FromString(TEXT("클리너님이 생각하시는 방향대로 드론은 움직일 것입니다.")),
		FText::FromString(TEXT("지금부터 실전 임무에 투입되기 전 [가상 훈련]을 실시하겠습니다.")),
	};
	static const TArray<FText> Move{
		FText::FromString(TEXT("먼저 드론을 움직여봅시다.")),
		FText::FromString(TEXT("원하시는 방향으로 [방향키]를 입력해주세요.")),
	};
	static const TArray<FText> Attack{
		FText::FromString(TEXT("우주 속 먼지는 주로 뭉치면서 [악성먼지]가 되어 사람들을 위협합니다.")),
		FText::FromString(TEXT("클리너님은 악성먼지를 공격해서 제거해야 합니다.")),
		FText::FromString(TEXT("공격을 연습해보죠. Z를 입력해주세요.")),
	};
	static const TArray<FText> Dodge{
		FText::FromString(TEXT("잘하셨습니다.")),
		FText::FromString(TEXT("다음은 악성먼지의 공격으로부터 피할 수 있는 텔레포트를 연습해봅시다.")),
		FText::FromString(TEXT("C와 방향키를 같이 입력해주세요.")),
	};
	static const TArray<FText> CombatBriefing{
		FText::FromString(TEXT("지금부터 가상 훈련용으로 제작된 [잔해]를 클리닝해보겠습니다.")),
		FText::FromString(TEXT("앞서 익힌 Z 공격을 통해 클리닝을 진행해주세요.")),
	};
	static const TArray<FText> ClosingBriefing{
		FText::FromString(TEXT("수고 많으셨습니다. 가상 훈련을 종료하겠습니다.")),
		FText::FromString(TEXT("지금처럼 앞으로 클리너로서 우주를 위해 일해주시길 바랍니다.")),
	};

	switch (Step)
	{
	case ETutorialStep::WorldBriefing:
		return WorldBriefing;
	case ETutorialStep::Move:
		return Move;
	case ETutorialStep::Attack:
		return Attack;
	case ETutorialStep::Dodge:
		return Dodge;
	case ETutorialStep::CombatBriefing:
		return CombatBriefing;
	case ETutorialStep::ClosingBriefing:
		return ClosingBriefing;
	default:
		return Empty;
	}
}

bool IsDialogueOnlyStep(ETutorialStep Step)
{
	return Step == ETutorialStep::WorldBriefing
		|| Step == ETutorialStep::CombatBriefing
		|| Step == ETutorialStep::ClosingBriefing;
}
}

ATutorialPlayerController::ATutorialPlayerController()
{
	TutorialHUDWidgetClass = TSoftClassPtr<UTutorialHUDWidget>(
		FSoftObjectPath(TEXT("/Game/UI/WBP_TutorialHUD.WBP_TutorialHUD_C")));
}

void ATutorialPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;

	UClass* WidgetClass = TutorialHUDWidgetClass.LoadSynchronous();
	if (!WidgetClass)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] TutorialHUDMissing Class=%s Controller=%s"),
			*TutorialHUDWidgetClass.ToString(),
			*GetNameSafe(this));
		return;
	}

	ActiveTutorialHUD = CreateWidget<UTutorialHUDWidget>(this, WidgetClass);
	if (ActiveTutorialHUD)
	{
		ActiveTutorialHUD->AddToViewport();
		RefreshTutorialHUD();
	}
}

void ATutorialPlayerController::StartTutorial()
{
	if (!HasAuthority())
	{
		return;
	}

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
	if (!HasAuthority())
	{
		return false;
	}

	switch (CurrentTutorialStep)
	{
	case ETutorialStep::None:
		StartTutorial();
		return true;
	case ETutorialStep::Start:
		SetTutorialStep(ETutorialStep::WorldBriefing, FName(TEXT("StartPresented")));
		return true;
	case ETutorialStep::WorldBriefing:
		if (!IsCurrentTutorialDialogueReady())
		{
			return false;
		}
		SetTutorialStep(ETutorialStep::Move, FName(TEXT("WorldBriefingComplete")));
		return true;
	case ETutorialStep::CombatBriefing:
		if (!IsCurrentTutorialDialogueReady())
		{
			return false;
		}
		SetTutorialStep(ETutorialStep::DebrisCombat, FName(TEXT("CombatBriefingComplete")));
		return true;
	case ETutorialStep::ClosingBriefing:
		if (!IsCurrentTutorialDialogueReady())
		{
			return false;
		}
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
	if (!HasAuthority() || !bTutorialActive || CurrentTutorialStep != ETutorialStep::ClosingBriefing)
	{
		return false;
	}

	bTutorialActive = false;
	SetTutorialStep(ETutorialStep::Complete, FName(TEXT("Complete")));
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Tutorial Complete Controller=%s"),
		*GetNameSafe(this));
	Client_PersistTutorialCompletion();
	return true;
}

void ATutorialPlayerController::Client_PersistTutorialCompletion_Implementation()
{
	bool bSaved = false;
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (URaidSessionSubsystem* Session = GameInstance->GetSubsystem<URaidSessionSubsystem>())
		{
			bSaved = Session->MarkTutorialCompleted();
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] TutorialProfileSaved Controller=%s Result=%s"),
		*GetNameSafe(this),
		bSaved ? TEXT("Success") : TEXT("UnavailableOrFailed"));
	if (!bSaved)
	{
		return;
	}

	bTutorialActive = false;
	BP_OnTutorialComplete();
	ReturnToLobbyAfterTutorial();
}

FText ATutorialPlayerController::GetCurrentTutorialDialogueText() const
{
	const TArray<FText>& Lines = GetTutorialDialogueLines(CurrentTutorialStep);
	return Lines.IsValidIndex(CurrentTutorialDialogueIndex)
		? Lines[CurrentTutorialDialogueIndex]
		: FText::GetEmpty();
}

bool ATutorialPlayerController::IsCurrentTutorialDialogueReady() const
{
	const TArray<FText>& Lines = GetTutorialDialogueLines(CurrentTutorialStep);
	return !Lines.IsEmpty() && CurrentTutorialDialogueIndex >= Lines.Num() - 1;
}

bool ATutorialPlayerController::TryAdvanceTutorialDialogue()
{
	const TArray<FText>& Lines = GetTutorialDialogueLines(CurrentTutorialStep);
	if (!bTutorialActive || Lines.IsEmpty())
	{
		return false;
	}

	const bool bShouldConsumeInput =
		CurrentTutorialDialogueIndex < Lines.Num() - 1
		|| IsDialogueOnlyStep(CurrentTutorialStep);
	if (!bShouldConsumeInput)
	{
		return false;
	}

	if (!HasAuthority())
	{
		Server_RequestAdvanceTutorialDialogue();
		return true;
	}

	return AdvanceTutorialDialogueForServer();
}

void ATutorialPlayerController::Server_RequestAdvanceTutorialDialogue_Implementation()
{
	AdvanceTutorialDialogueForServer();
}

bool ATutorialPlayerController::AdvanceTutorialDialogueForServer()
{
	if (!HasAuthority() || !bTutorialActive)
	{
		return false;
	}

	const TArray<FText>& Lines = GetTutorialDialogueLines(CurrentTutorialStep);
	if (Lines.IsEmpty())
	{
		return false;
	}

	if (CurrentTutorialDialogueIndex < Lines.Num() - 1)
	{
		++CurrentTutorialDialogueIndex;
		SyncTutorialPresentationForOwner(CurrentTutorialStep);
		return true;
	}

	return IsDialogueOnlyStep(CurrentTutorialStep) && AdvanceTutorialStep();
}

bool ATutorialPlayerController::NotifyTutorialMoveInput(
	FVector2D RawAxis,
	float AppliedDistanceMeters)
{
	if (!HasAuthority()
		|| !bTutorialActive
		|| CurrentTutorialStep != ETutorialStep::Move
		|| !IsCurrentTutorialDialogueReady())
	{
		return false;
	}

	if (!RawAxis.IsNearlyZero())
	{
		AccumulatedTutorialMoveDistanceMeters += FMath::Max(0.0f, AppliedDistanceMeters);
		return false;
	}

	if (AccumulatedTutorialMoveDistanceMeters + KINDA_SMALL_NUMBER
		< TutorialMoveDistanceRequiredMeters)
	{
		return false;
	}

	SetTutorialStep(ETutorialStep::Attack, FName(TEXT("MoveSucceeded")));
	return true;
}

bool ATutorialPlayerController::NotifyTutorialAttackInput()
{
	if (!HasAuthority()
		|| !bTutorialActive
		|| CurrentTutorialStep != ETutorialStep::Attack
		|| !IsCurrentTutorialDialogueReady())
	{
		return false;
	}

	bTutorialAttackInputPending = true;
	return true;
}

bool ATutorialPlayerController::NotifyTutorialAttackInputReleased()
{
	if (!HasAuthority()
		|| !bTutorialActive
		|| CurrentTutorialStep != ETutorialStep::Attack
		|| !IsCurrentTutorialDialogueReady()
		|| !bTutorialAttackInputPending)
	{
		return false;
	}

	SetTutorialStep(ETutorialStep::Dodge, FName(TEXT("AttackSucceeded")));
	return true;
}

bool ATutorialPlayerController::NotifyTutorialDodgeInput(FVector2D RawDirection)
{
	if (!HasAuthority()
		|| !bTutorialActive
		|| CurrentTutorialStep != ETutorialStep::Dodge
		|| !IsCurrentTutorialDialogueReady()
		|| RawDirection.IsNearlyZero())
	{
		return false;
	}

	SetTutorialStep(ETutorialStep::CombatBriefing, FName(TEXT("DodgeSucceeded")));
	return true;
}

bool ATutorialPlayerController::NotifyTutorialDebrisDestroyed()
{
	if (!HasAuthority() || !bTutorialActive || CurrentTutorialStep != ETutorialStep::DebrisCombat)
	{
		return false;
	}

	SetTutorialStep(ETutorialStep::ClosingBriefing, FName(TEXT("DebrisDestroyed")));
	return true;
}

bool ATutorialPlayerController::IsTutorialMoveAllowed() const
{
	return bTutorialActive
		&& ((CurrentTutorialStep == ETutorialStep::Move && IsCurrentTutorialDialogueReady())
			|| CurrentTutorialStep == ETutorialStep::CombatBriefing
			|| CurrentTutorialStep == ETutorialStep::DebrisCombat);
}

bool ATutorialPlayerController::IsTutorialAttackAllowed() const
{
	return bTutorialActive
		&& ((CurrentTutorialStep == ETutorialStep::Attack && IsCurrentTutorialDialogueReady())
			|| CurrentTutorialStep == ETutorialStep::CombatBriefing
			|| CurrentTutorialStep == ETutorialStep::DebrisCombat);
}

bool ATutorialPlayerController::IsTutorialDodgeAllowed() const
{
	return bTutorialActive
		&& ((CurrentTutorialStep == ETutorialStep::Dodge && IsCurrentTutorialDialogueReady())
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
	CurrentTutorialDialogueIndex = 0;
	AccumulatedTutorialMoveDistanceMeters = 0.0f;
	bTutorialAttackInputPending = false;

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Tutorial Step=%s Previous=%s Reason=%s Controller=%s"),
		ToTutorialStepLogString(CurrentTutorialStep),
		ToTutorialStepLogString(PreviousStep),
		Reason.IsNone() ? TEXT("None") : *Reason.ToString(),
		*GetNameSafe(this));

	SyncTutorialPresentationForOwner(PreviousStep);

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

void ATutorialPlayerController::SyncTutorialPresentationForOwner(ETutorialStep PreviousStep)
{
	if (IsLocalController())
	{
		if (PreviousStep != CurrentTutorialStep)
		{
			BP_OnTutorialStepChanged(PreviousStep, CurrentTutorialStep);
		}
		BP_OnTutorialDialogueChanged(
			CurrentTutorialStep,
			CurrentTutorialDialogueIndex,
			GetCurrentTutorialDialogueText());
		RefreshTutorialHUD();
		return;
	}

	Client_SyncTutorialPresentation(CurrentTutorialStep, CurrentTutorialDialogueIndex);
}

void ATutorialPlayerController::Client_SyncTutorialPresentation_Implementation(
	ETutorialStep Step,
	int32 DialogueIndex)
{
	const ETutorialStep PreviousStep = CurrentTutorialStep;
	CurrentTutorialStep = Step;
	CurrentTutorialDialogueIndex = DialogueIndex;
	bTutorialActive = Step != ETutorialStep::None && Step != ETutorialStep::Complete;

	if (PreviousStep != CurrentTutorialStep)
	{
		BP_OnTutorialStepChanged(PreviousStep, CurrentTutorialStep);
	}
	BP_OnTutorialDialogueChanged(
		CurrentTutorialStep,
		CurrentTutorialDialogueIndex,
		GetCurrentTutorialDialogueText());
	RefreshTutorialHUD();
}

void ATutorialPlayerController::RefreshTutorialHUD()
{
	if (ActiveTutorialHUD)
	{
		ActiveTutorialHUD->RefreshTutorial(
			CurrentTutorialStep,
			CurrentTutorialDialogueIndex,
			GetCurrentTutorialDialogueText(),
			IsCurrentTutorialDialogueReady());
	}
}
