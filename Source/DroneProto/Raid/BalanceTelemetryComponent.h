#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BalanceTelemetryComponent.generated.h"

class APlayerController;

struct DRONEPROTO_API FBalanceTelemetryField
{
	FName Key = NAME_None;
	FString Value;
};

struct DRONEPROTO_API FBalanceTelemetryFormatter
{
	static constexpr int32 SchemaVersion = 1;

	static bool TryFormat(
		const FString& SessionId,
		int64 Sequence,
		double RelativeSeconds,
		FName Environment,
		const FString& BuildVersion,
		const FString& BalanceVersion,
		FName EventName,
		const TArray<FBalanceTelemetryField>& Fields,
		FString& OutLine);

	static FString SanitizeToken(const FString& Value);
	static bool IsForbiddenKey(FName Key);
};

UCLASS(ClassGroup=(Telemetry))
class DRONEPROTO_API UBalanceTelemetryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	void StartSessionForServer(FName ServerSlot, FName MapName, const FString& InBalanceVersion);
	FString GetOrAssignPlayerAliasForServer(APlayerController* PlayerController);
	void RecordPlayerJoinedForServer(APlayerController* PlayerController, bool bLateJoin, int32 PlayerCount);
	void EmitForServer(FName EventName, const TArray<FBalanceTelemetryField>& Fields);
	void EndSessionForServer(FName Outcome, int32 PlayerCount, float BossHPRemaining);
	static UBalanceTelemetryComponent* FindForServer(const UObject* WorldContext);
	static FString Number(double Value, int32 Precision = 3);

#if WITH_DEV_AUTOMATION_TESTS
	const TArray<FString>& GetEmittedLinesForTest() const { return EmittedLinesForTest; }
#endif

private:
	bool CanEmitForServer() const;
	FName ResolveEnvironment() const;

	FString SessionId;
	FString BuildVersion;
	FString BalanceVersion;
	double SessionStartWorldSeconds = 0.0;
	int64 NextSequence = 1;
	bool bSessionEnded = false;
	TMap<FString, FString> PlayerAliasesByStableKey;

#if WITH_DEV_AUTOMATION_TESTS
	TArray<FString> EmittedLinesForTest;
#endif
};
