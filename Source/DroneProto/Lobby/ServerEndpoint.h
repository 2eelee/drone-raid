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

UENUM(BlueprintType)
enum class ERaidServerState : uint8
{
	Unknown,
	Online,
	Offline,
	Full,
	Unavailable,
	Loading,
	Error,
};

inline const TCHAR* ToRaidServerStateText(ERaidServerState State)
{
	switch (State)
	{
	case ERaidServerState::Unknown:
		return TEXT("Unknown");
	case ERaidServerState::Online:
		return TEXT("Online");
	case ERaidServerState::Offline:
		return TEXT("Offline");
	case ERaidServerState::Full:
		return TEXT("Full");
	case ERaidServerState::Unavailable:
		return TEXT("Unavailable");
	case ERaidServerState::Loading:
		return TEXT("Loading");
	case ERaidServerState::Error:
		return TEXT("Error");
	default:
		return TEXT("Unknown");
	}
}

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
struct DRONEPROTO_API FRaidServerAvailability
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString SlotId;

	UPROPERTY(BlueprintReadOnly)
	int32 CurrentPlayers = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 MaxPlayers = 16;

	UPROPERTY(BlueprintReadOnly)
	ERaidServerState ServerState = ERaidServerState::Unknown;

	UPROPERTY(BlueprintReadOnly)
	bool bAcceptsPlayers = true;

	UPROPERTY(BlueprintReadOnly)
	FString DebugReason;
};

USTRUCT(BlueprintType)
struct DRONEPROTO_API FRaidServerCandidate
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FServerEndpoint Endpoint;

	UPROPERTY(BlueprintReadOnly)
	FRaidServerAvailability Availability;

	// Legacy compatibility mirrors. New assignment evaluation should read Availability.
	UPROPERTY(BlueprintReadOnly)
	int32 CurrentPlayers = 0;

	// Legacy compatibility mirror. New assignment evaluation should read Availability.
	UPROPERTY(BlueprintReadOnly)
	int32 MaxPlayers = 16;

	// Legacy compatibility mirror. New assignment evaluation should read Availability.ServerState.
	UPROPERTY(BlueprintReadOnly)
	bool bIsOnline = true;

	// Legacy compatibility mirror. New assignment evaluation should read Availability.bAcceptsPlayers.
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
	FRaidServerAvailability Availability;

	UPROPERTY(BlueprintReadOnly)
	ERaidEntryFailReason FailReason = ERaidEntryFailReason::None;

	UPROPERTY(BlueprintReadOnly)
	FName SelectedSlotId = NAME_None;

	UPROPERTY(BlueprintReadOnly)
	FString DebugReason;

	UPROPERTY(BlueprintReadOnly)
	FString ReservationToken;

	static FRaidAssignmentResult Success(
		const FRaidServerCandidate& Candidate,
		const FString& InDebugReason,
		const FString& InReservationToken = FString())
	{
		FRaidAssignmentResult Out;
		Out.Result = ERaidAssignmentResultType::Success;
		Out.Endpoint = Candidate.Endpoint;
		Out.Availability = Candidate.Availability;
		if (Out.Availability.SlotId.IsEmpty())
		{
			Out.Availability.SlotId = Candidate.Endpoint.SlotId;
		}
		Out.FailReason = ERaidEntryFailReason::None;
		Out.SelectedSlotId = Candidate.Endpoint.SlotId.IsEmpty()
			? NAME_None
			: FName(*Candidate.Endpoint.SlotId);
		Out.DebugReason = InDebugReason;
		Out.ReservationToken = InReservationToken;
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
		const FServerEndpoint& InEndpoint = FServerEndpoint{},
		const FRaidServerAvailability& InAvailability = FRaidServerAvailability())
	{
		FRaidAssignmentResult Out;
		Out.Result = ERaidAssignmentResultType::Failed;
		Out.Endpoint = InEndpoint;
		Out.Availability = InAvailability;
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
