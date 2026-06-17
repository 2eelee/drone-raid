#include "RaidPlayerController.h"

void ARaidPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// [확인3] 맵 로드 타임아웃 / 폰 Spawn 실패 (후순위, D11)
	//   단일 정상 환경에선 미발동. 구현 시 아래 패턴 사용:
	//   UGameInstance* GI = GetGameInstance();
	//   URaidSessionSubsystem* SS = GI ? GI->GetSubsystem<URaidSessionSubsystem>() : nullptr;
	//   if (LoadTimedOut || !GetPawn()) { if (SS) SS->ShowLoadFailed(); ReturnToLobby(); }   // TODO
}
