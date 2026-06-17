#include "LocalAssignment.h"

FServerEndpoint ULocalAssignment::ResolveServer(const FString& RequestedSlot)
{
	FServerEndpoint Endpoint;
	Endpoint.SlotId       = TEXT("A");
	Endpoint.TravelTarget = TEXT("Lvl_ThirdPerson");  // TODO: 레이드 전용 레벨 생성 후 교체
	Endpoint.bIsLevelName = true;
	return Endpoint;
}
