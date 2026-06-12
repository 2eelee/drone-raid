#include "RaidGameMode.h"
#include "RaidGameState.h"
#include "RaidPlayerController.h"
#include "RaidPlayerState.h"

ARaidGameMode::ARaidGameMode()
{
	GameStateClass       = ARaidGameState::StaticClass();
	PlayerControllerClass = ARaidPlayerController::StaticClass();
	PlayerStateClass      = ARaidPlayerState::StaticClass();
}

void ARaidGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (HasAuthority())
	{
		if (ARaidGameState* GS = GetGameState<ARaidGameState>())
		{
			GS->CurrentPlayers++;
			UE_LOG(LogTemp, Log, TEXT("[Server] PostLogin: CurrentPlayers = %d"), GS->CurrentPlayers);
		}
	}
}

void ARaidGameMode::Logout(AController* Exiting)
{
	if (HasAuthority())
	{
		if (ARaidGameState* GS = GetGameState<ARaidGameState>())
		{
			GS->CurrentPlayers--;
			UE_LOG(LogTemp, Log, TEXT("[Server] Logout: CurrentPlayers = %d"), GS->CurrentPlayers);
		}
	}

	Super::Logout(Exiting);
}
