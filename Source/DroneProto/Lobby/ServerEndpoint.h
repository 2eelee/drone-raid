#pragma once

#include "CoreMinimal.h"
#include "ServerEndpoint.generated.h"

UENUM(BlueprintType)
enum class ERaidEntryFailReason : uint8
{
	None,
	ServerListFailed,
	NoServerAvailable,
	MapLoadFailed,
	SpawnFailed,
	Cancelled,
};

UENUM(BlueprintType)
enum class ERaidAssignmentResultType : uint8
{
	Success,
	Waiting,
	Failed,
	Canceled,
};

USTRUCT(BlueprintType)
struct DRONEPROTO_API FServerEndpoint
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString SlotId;

	// true → UGameplayStatics::OpenLevel, false → ClientTravel(IP:Port)
	UPROPERTY(BlueprintReadOnly)
	FString TravelTarget;

	UPROPERTY(BlueprintReadOnly)
	bool bIsLevelName = true;
};

USTRUCT(BlueprintType)
struct DRONEPROTO_API FRaidServerCandidate
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FServerEndpoint Endpoint;

	UPROPERTY(BlueprintReadOnly)
	int32 CurrentPlayers = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 MaxPlayers = 16;

	UPROPERTY(BlueprintReadOnly)
	bool bIsOnline = true;

	UPROPERTY(BlueprintReadOnly)
	bool bAcceptsPlayers = true;
};

USTRUCT(BlueprintType)
struct DRONEPROTO_API FRaidAssignmentResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	ERaidAssignmentResultType Result = ERaidAssignmentResultType::Failed;

	UPROPERTY(BlueprintReadOnly)
	FServerEndpoint Endpoint;

	UPROPERTY(BlueprintReadOnly)
	ERaidEntryFailReason FailReason = ERaidEntryFailReason::None;

	UPROPERTY(BlueprintReadOnly)
	FName SelectedSlotId = NAME_None;

	UPROPERTY(BlueprintReadOnly)
	FString DebugReason;

	static FRaidAssignmentResult Success(const FRaidServerCandidate& Candidate, const FString& InDebugReason)
	{
		FRaidAssignmentResult Out;
		Out.Result = ERaidAssignmentResultType::Success;
		Out.Endpoint = Candidate.Endpoint;
		Out.FailReason = ERaidEntryFailReason::None;
		Out.SelectedSlotId = Candidate.Endpoint.SlotId.IsEmpty()
			? NAME_None
			: FName(*Candidate.Endpoint.SlotId);
		Out.DebugReason = InDebugReason;
		return Out;
	}

	static FRaidAssignmentResult Waiting(ERaidEntryFailReason InFailReason, const FString& InDebugReason)
	{
		FRaidAssignmentResult Out;
		Out.Result = ERaidAssignmentResultType::Waiting;
		Out.FailReason = InFailReason;
		Out.DebugReason = InDebugReason;
		return Out;
	}

	static FRaidAssignmentResult Failed(
		ERaidEntryFailReason InFailReason,
		const FString& InDebugReason,
		const FServerEndpoint& InEndpoint = FServerEndpoint{})
	{
		FRaidAssignmentResult Out;
		Out.Result = ERaidAssignmentResultType::Failed;
		Out.Endpoint = InEndpoint;
		Out.FailReason = InFailReason;
		Out.SelectedSlotId = InEndpoint.SlotId.IsEmpty() ? NAME_None : FName(*InEndpoint.SlotId);
		Out.DebugReason = InDebugReason;
		return Out;
	}

	static FRaidAssignmentResult Canceled(const FString& InDebugReason)
	{
		FRaidAssignmentResult Out;
		Out.Result = ERaidAssignmentResultType::Canceled;
		Out.FailReason = ERaidEntryFailReason::Cancelled;
		Out.DebugReason = InDebugReason;
		return Out;
	}
};
