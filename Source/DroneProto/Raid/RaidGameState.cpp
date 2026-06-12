#include "RaidGameState.h"
#include "Net/UnrealNetwork.h"

ARaidGameState::ARaidGameState()
{
	RaidState = ERaidState::Waiting;
	CurrentPlayers = 0;
}

void ARaidGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARaidGameState, RaidState);
	DOREPLIFETIME(ARaidGameState, CurrentPlayers);
}

void ARaidGameState::OnRep_RaidState()
{
	UE_LOG(LogTemp, Log, TEXT("[Client] RaidState replicated -> %d"), static_cast<int32>(RaidState));
}
