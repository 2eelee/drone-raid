#pragma once

#include "CoreMinimal.h"
#include "HttpRouteHandle.h"
#include "HttpResultCallback.h"
#include "RaidReservationLedger.h"
#include "UObject/Object.h"
#include "RaidServerAdmissionService.generated.h"

class ARaidGameMode;
class IHttpRouter;
struct FHttpServerRequest;

UCLASS()
class DRONEPROTO_API URaidServerAdmissionService : public UObject
{
	GENERATED_BODY()

public:
	bool Initialize(ARaidGameMode* InGameMode, const FString& InSlotId, uint32 InPort, const FString& InGameEndpoint);
	void Shutdown();
	virtual void BeginDestroy() override;

	bool TryClaim(const FString& RequestedSlot, const FString& Token, double NowSeconds, FString& OutErrorMessage);
	bool BindClaimedToken(APlayerController* PlayerController, const FString& Token);
	bool CommitForPlayer(APlayerController* PlayerController);
	bool ReleasePlayer(AController* Controller);
	int32 ReleaseAbandonedClaims(const TSet<FString>& LiveReservationTokens);
	int32 GetActivePlayers() const;

	// 예약 발급의 판정 경로. 입장 게이트와 정원을 순서대로 보고 토큰을 낸다.
	// HTTP 핸들러가 응답 형식만 얹도록 분리했다 — `FHttpServerRequest`를 만들 수 없는
	// 자동화에서도 게이트가 실제로 도는지 확인해야 하기 때문이다(ENTRY-13 잔여).
	bool TryIssueReservationForServer(double NowSeconds, FString& OutToken, FName& OutRejectReason);

#if WITH_DEV_AUTOMATION_TESTS
	// InGameMode를 주면 실환경 Initialize처럼 양방향으로 연결된다. 게이트를 타는 경로
	// (TryIssueReservationForServer)를 검증하려면 필요하다. 생략하면 ledger만 준비한다.
	void InitializeForTest(const FString& InSlotId, ARaidGameMode* InGameMode = nullptr);
	// 게이트를 의도적으로 우회해 토큰만 만든다. 닫힌 레이드에 도착하는 클라이언트를
	// 재현하려면 이 경로가 필요하다. 게이트 자체는 TryIssueReservationForServer로 검증한다.
	bool IssueReservationForTest(double NowSeconds, FString& OutToken);
	bool TryCommitClaimedForTest(const FString& Token);
#endif

private:
	bool HandleReservationRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

	TWeakObjectPtr<ARaidGameMode> GameMode;
	TUniquePtr<FRaidReservationLedger> Ledger;
	FString SlotId;
	FString GameEndpoint;
	TMap<TWeakObjectPtr<APlayerController>, FString> PlayerTokens;
	TSet<TWeakObjectPtr<AController>> AdmittedPlayers;
	TSharedPtr<IHttpRouter> Router;
	FHttpRouteHandle ReservationRoute;
};
