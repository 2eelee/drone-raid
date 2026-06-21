#include "RaidGameMode.h"
#include "Drone.h"
#include "DronePartInventory.h"
#include "DronePartReturnManager.h"
#include "RaidBoss.h"
#include "RaidGameState.h"
#include "RaidPlayerController.h"
#include "RaidPlayerState.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
FString BuildRaidGameModeControllerLogString(const AController* Controller)
{
	return ARaidPlayerController::BuildStableControllerLogString(Controller);
}

void LogControllerPawnState(const TCHAR* Source, const AController* Controller)
{
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	UE_LOG(LogTemp, Log, TEXT("[Server] %s PawnState: PlayerController=%s Pawn=%s PawnClass=%s IsADrone=%s"),
		Source,
		*BuildRaidGameModeControllerLogString(Controller),
		Pawn ? *Pawn->GetName() : TEXT("None"),
		Pawn ? *Pawn->GetClass()->GetName() : TEXT("None"),
		Pawn && Pawn->IsA<ADrone>() ? TEXT("true") : TEXT("false"));
}
}

ARaidGameMode::ARaidGameMode()
{
	GameStateClass       = ARaidGameState::StaticClass();
	PlayerControllerClass = ARaidPlayerController::StaticClass();
	PlayerStateClass      = ARaidPlayerState::StaticClass();
}

void ARaidGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	ARaidGameState* GS = GetGameState<ARaidGameState>();
	if (!GS)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Server] DronePartInventory spawn failed: RaidGameState is missing"));
		return;
	}

	if (!GS->GetDronePartInventory())
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = GS;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ADronePartInventory* SpawnedInventory = GetWorld()->SpawnActor<ADronePartInventory>(
			ADronePartInventory::StaticClass(),
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			SpawnParams);

		if (SpawnedInventory)
		{
			GS->SetDronePartInventory(SpawnedInventory);
			UE_LOG(LogTemp, Log, TEXT("[Server] DronePartInventory spawned: Name=%s Replicates=%s AlwaysRelevant=%s Dormancy=%d"),
				*SpawnedInventory->GetName(),
				SpawnedInventory->GetIsReplicated() ? TEXT("true") : TEXT("false"),
				SpawnedInventory->bAlwaysRelevant ? TEXT("true") : TEXT("false"),
				static_cast<int32>(SpawnedInventory->NetDormancy));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[Server] DronePartInventory spawn failed"));
		}
	}

	EnsureDronePartReturnManagerForServer();

	if (!GS->GetRaidBoss())
	{
		ARaidBoss* ExistingBoss = nullptr;
		for (TActorIterator<ARaidBoss> It(GetWorld()); It; ++It)
		{
			ExistingBoss = *It;
			break;
		}

		if (!ExistingBoss)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = GS;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			ExistingBoss = GetWorld()->SpawnActor<ARaidBoss>(
				ARaidBoss::StaticClass(),
				FVector(600.0f, 0.0f, 100.0f),
				FRotator::ZeroRotator,
				SpawnParams);
		}

		if (ExistingBoss)
		{
			GS->SetRaidBossForServer(ExistingBoss);
			UE_LOG(LogTemp, Log, TEXT("[Server] RaidBoss ready: Name=%s Replicates=%s"),
				*ExistingBoss->GetName(),
				ExistingBoss->GetIsReplicated() ? TEXT("true") : TEXT("false"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[Server] RaidBoss spawn failed"));
		}
	}
}

bool ARaidGameMode::EnsureDronePartReturnManagerForServer()
{
	if (!HasAuthority())
	{
		return false;
	}

	if (DronePartReturnManager)
	{
		return true;
	}

	UWorld* World = GetWorld();
	ARaidGameState* GS = World ? World->GetGameState<ARaidGameState>() : nullptr;
	ADronePartInventory* Inventory = GS ? GS->GetDronePartInventory() : nullptr;
	if (!Inventory)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Server] DronePartReturnManager init skipped: DronePartInventory is missing"));
		return false;
	}

	DronePartReturnManager = NewObject<UDronePartReturnManager>(this);
	DronePartReturnManager->Initialize(Inventory);
	UE_LOG(LogTemp, Log, TEXT("[Server] DronePartReturnManager initialized"));
	return true;
}

void ARaidGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	LogControllerPawnState(TEXT("PostLogin"), NewPlayer);

	if (HasAuthority())
	{
		if (ARaidGameState* GS = GetGameState<ARaidGameState>())
		{
			GS->CurrentPlayers++;
			UE_LOG(LogTemp, Log, TEXT("[Server] PostLogin: CurrentPlayers = %d"), GS->CurrentPlayers);
		}
	}
}

void ARaidGameMode::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);
	LogControllerPawnState(TEXT("RestartPlayer"), NewPlayer);

	const APawn* Pawn = NewPlayer ? NewPlayer->GetPawn() : nullptr;
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Spawn PC=%s Pawn=%s PawnClass=%s IsADrone=%s"),
		*BuildRaidGameModeControllerLogString(NewPlayer),
		Pawn ? *Pawn->GetName() : TEXT("None"),
		Pawn ? *Pawn->GetClass()->GetName() : TEXT("None"),
		Pawn && Pawn->IsA<ADrone>() ? TEXT("true") : TEXT("false"));
}

APawn* ARaidGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	UWorld* World = GetWorld();
	UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer);
	if (!PawnClass)
	{
		PawnClass = ADrone::StaticClass();
	}

	if (!World || !PawnClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Server] SpawnDefaultPawnAtTransform Failed: Player=%s World=%s PawnClass=%s"),
			*BuildRaidGameModeControllerLogString(NewPlayer),
			World ? TEXT("Valid") : TEXT("None"),
			*GetNameSafe(PawnClass));
		return nullptr;
	}

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Instigator = GetInstigator();
	SpawnInfo.ObjectFlags |= RF_Transient;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	APawn* SpawnedPawn = World->SpawnActor<APawn>(PawnClass, SpawnTransform, SpawnInfo);
	bool bUsedFallbackAlwaysSpawn = false;
	if (!SpawnedPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Server] SpawnDefaultPawnAtTransform Retry: Player=%s PawnClass=%s Reason=AdjustIfPossibleButAlwaysSpawn returned null"),
			*BuildRaidGameModeControllerLogString(NewPlayer),
			*GetNameSafe(PawnClass));

		SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		bUsedFallbackAlwaysSpawn = true;
		const FVector SpawnLocation = SpawnTransform.GetLocation();
		const FQuat SpawnRotation = SpawnTransform.GetRotation();
		const FVector SpawnScale = SpawnTransform.GetScale3D();
		const FVector RetryOffsets[] = {
			FVector::ZeroVector,
			FVector(150.0f, 0.0f, 0.0f),
			FVector(-150.0f, 0.0f, 0.0f),
			FVector(0.0f, 150.0f, 0.0f),
			FVector(0.0f, -150.0f, 0.0f),
		};

		for (const FVector& RetryOffset : RetryOffsets)
		{
			const FTransform RetryTransform(SpawnRotation, SpawnLocation + RetryOffset, SpawnScale);
			SpawnedPawn = World->SpawnActor<APawn>(PawnClass, RetryTransform, SpawnInfo);
			if (SpawnedPawn)
			{
				break;
			}
		}
	}

	const FString ResultPawnClassName = SpawnedPawn ? SpawnedPawn->GetClass()->GetName() : GetNameSafe(PawnClass);
	UE_LOG(LogTemp, Log, TEXT("[Server] SpawnDefaultPawnAtTransform Result: Player=%s Pawn=%s PawnClass=%s IsADrone=%s SpawnHandling=AdjustIfPossibleButAlwaysSpawn FallbackAlwaysSpawn=%s Transform=%s"),
		*BuildRaidGameModeControllerLogString(NewPlayer),
		SpawnedPawn ? *SpawnedPawn->GetName() : TEXT("None"),
		*ResultPawnClassName,
		SpawnedPawn && SpawnedPawn->IsA<ADrone>() ? TEXT("true") : TEXT("false"),
		bUsedFallbackAlwaysSpawn ? TEXT("true") : TEXT("false"),
		*SpawnTransform.ToHumanReadableString());

	return SpawnedPawn;
}

void ARaidGameMode::Logout(AController* Exiting)
{
	if (HasAuthority())
	{
		if (ARaidPlayerController* RaidPC = Cast<ARaidPlayerController>(Exiting))
		{
			if (RaidPC->GetPlayerSelectionState() == EPlayerSelectionState::Selecting)
			{
				UE_LOG(LogTemp, Log, TEXT("[Server] Logout return path: Player=%s PlayerSelectionState=Selecting Source=SelectedParts"),
					*RaidPC->GetName());
				RaidPC->ReturnSelectedPartsForServer(EDronePartReturnReason::Disconnect);
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT("[Server] Logout return path: Player=%s PlayerSelectionState=%d Source=EquippedParts"),
					*RaidPC->GetName(),
					static_cast<int32>(RaidPC->GetPlayerSelectionState()));
				RaidPC->ReturnEquippedPartsForServer(EDronePartReturnReason::Disconnect);
			}
		}

		if (ARaidGameState* GS = GetGameState<ARaidGameState>())
		{
			GS->CurrentPlayers--;
			UE_LOG(LogTemp, Log, TEXT("[Server] Logout: CurrentPlayers = %d"), GS->CurrentPlayers);
		}
	}

	Super::Logout(Exiting);
}

void ARaidGameMode::ReturnAllEquippedPartsForRaidEnd(FName Reason)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!EnsureDronePartReturnManagerForServer())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Server] RaidEnd return skipped: DronePartReturnManager is missing"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FString ReasonText = Reason.IsNone() ? TEXT("Manual") : Reason.ToString();
	int32 EligiblePlayerCount = 0;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (ARaidPlayerController* RaidPC = Cast<ARaidPlayerController>(It->Get()))
		{
			const EPlayerSelectionState SelectionState = RaidPC->GetPlayerSelectionState();
			if (SelectionState == EPlayerSelectionState::InBattle || SelectionState == EPlayerSelectionState::Locked)
			{
				EligiblePlayerCount++;
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] RaidEnd Reason=%s PlayerCount=%d"),
		*ReasonText,
		EligiblePlayerCount);

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (ARaidPlayerController* RaidPC = Cast<ARaidPlayerController>(It->Get()))
		{
			const EPlayerSelectionState SelectionState = RaidPC->GetPlayerSelectionState();
			if (SelectionState != EPlayerSelectionState::InBattle && SelectionState != EPlayerSelectionState::Locked)
			{
				continue;
			}

			DronePartReturnManager->ReturnEquippedParts(RaidPC, EDronePartReturnReason::RaidEnd);
		}
	}

	if (ARaidGameState* GS = World->GetGameState<ARaidGameState>())
	{
		GS->SetRaidStateForServer(ERaidState::End);
	}

	UE_LOG(LogTemp, Log, TEXT("[Server] RaidEnd equipped part return completed Reason=%s PlayerCount=%d"),
		*ReasonText,
		EligiblePlayerCount);
}

UDronePartReturnManager* ARaidGameMode::GetDronePartReturnManager() const
{
	return DronePartReturnManager;
}
