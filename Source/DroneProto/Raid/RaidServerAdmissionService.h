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

#if WITH_DEV_AUTOMATION_TESTS
	void InitializeForTest(const FString& InSlotId);
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
