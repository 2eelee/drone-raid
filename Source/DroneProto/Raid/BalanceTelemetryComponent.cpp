#include "Raid/BalanceTelemetryComponent.h"

#include "CoreGlobals.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/App.h"
#include "Raid/RaidGameMode.h"

namespace
{
const TSet<FString>& GetForbiddenTelemetryKeys()
{
	static const TSet<FString> Keys{
		TEXT("callsign"),
		TEXT("uniquenetid"),
		TEXT("uid"),
		TEXT("pid"),
		TEXT("playerid"),
		TEXT("playername"),
		TEXT("pcname"),
		TEXT("account"),
		TEXT("ip"),
		TEXT("address"),
	};
	return Keys;
}
}

bool FBalanceTelemetryFormatter::TryFormat(
	const FString& SessionId,
	int64 Sequence,
	double RelativeSeconds,
	FName Environment,
	const FString& BuildVersion,
	const FString& BalanceVersion,
	FName EventName,
	const TArray<FBalanceTelemetryField>& Fields,
	FString& OutLine)
{
	OutLine.Reset();
	for (const FBalanceTelemetryField& Field : Fields)
	{
		if (IsForbiddenKey(Field.Key))
		{
			return false;
		}
	}

	OutLine = FString::Printf(
		TEXT("Telemetry Schema=%d Event=%s Session=%s Seq=%lld T=%.3f Environment=%s BuildVersion=%s BalanceVersion=%s"),
		SchemaVersion,
		*SanitizeToken(EventName.ToString()),
		*SanitizeToken(SessionId),
		Sequence,
		RelativeSeconds,
		*SanitizeToken(Environment.ToString()),
		*SanitizeToken(BuildVersion),
		*SanitizeToken(BalanceVersion));

	for (const FBalanceTelemetryField& Field : Fields)
	{
		OutLine += FString::Printf(
			TEXT(" %s=%s"),
			*SanitizeToken(Field.Key.ToString()),
			*SanitizeToken(Field.Value));
	}
	return true;
}

FString FBalanceTelemetryFormatter::SanitizeToken(const FString& Value)
{
	if (Value.IsEmpty())
	{
		return TEXT("None");
	}

	FString Sanitized;
	Sanitized.Reserve(Value.Len());
	for (const TCHAR Character : Value)
	{
		const bool bAsciiAlphaNumeric = (Character >= TEXT('A') && Character <= TEXT('Z'))
			|| (Character >= TEXT('a') && Character <= TEXT('z'))
			|| (Character >= TEXT('0') && Character <= TEXT('9'));
		const bool bAllowed = bAsciiAlphaNumeric
			|| Character == TEXT('_')
			|| Character == TEXT('.')
			|| Character == TEXT(':')
			|| Character == TEXT('-');
		Sanitized.AppendChar(bAllowed ? Character : TEXT('_'));
	}
	return Sanitized.IsEmpty() ? TEXT("None") : Sanitized;
}

bool FBalanceTelemetryFormatter::IsForbiddenKey(FName Key)
{
	return GetForbiddenTelemetryKeys().Contains(Key.ToString().ToLower());
}

void UBalanceTelemetryComponent::StartSessionForServer(
	FName ServerSlot,
	FName MapName,
	const FString& InBalanceVersion)
{
	if (!CanEmitForServer() || !SessionId.IsEmpty())
	{
		return;
	}

	SessionId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	SessionStartWorldSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	NextSequence = 1;
	BuildVersion = FApp::GetBuildVersion();
	if (BuildVersion.IsEmpty())
	{
		BuildVersion = TEXT("Dev");
	}
	BalanceVersion = InBalanceVersion.IsEmpty() ? TEXT("Unspecified") : InBalanceVersion;
	PlayerAliasesByStableKey.Reset();
	bSessionEnded = false;
#if WITH_DEV_AUTOMATION_TESTS
	EmittedLinesForTest.Reset();
#endif

	EmitForServer(TEXT("RaidSessionStarted"), {
		{TEXT("ServerSlot"), ServerSlot.ToString()},
		{TEXT("Map"), MapName.ToString()},
		{TEXT("StartedUtc"), FDateTime::UtcNow().ToIso8601()},
	});
}

FString UBalanceTelemetryComponent::GetOrAssignPlayerAliasForServer(APlayerController* PlayerController)
{
	if (!CanEmitForServer() || SessionId.IsEmpty() || !PlayerController)
	{
		return FString();
	}

	const FString StableKey = ARaidGameMode::BuildStablePlayerKeyForServer(PlayerController);
	if (StableKey.IsEmpty())
	{
		return FString();
	}
	if (const FString* ExistingAlias = PlayerAliasesByStableKey.Find(StableKey))
	{
		return *ExistingAlias;
	}

	const FString Alias = FString::Printf(TEXT("P%d"), PlayerAliasesByStableKey.Num() + 1);
	PlayerAliasesByStableKey.Add(StableKey, Alias);
	return Alias;
}

void UBalanceTelemetryComponent::RecordPlayerJoinedForServer(
	APlayerController* PlayerController,
	bool bLateJoin,
	int32 PlayerCount)
{
	const FString PlayerAlias = GetOrAssignPlayerAliasForServer(PlayerController);
	if (PlayerAlias.IsEmpty())
	{
		return;
	}

	EmitForServer(TEXT("PlayerJoined"), {
		{TEXT("Player"), PlayerAlias},
		{TEXT("LateJoin"), bLateJoin ? TEXT("1") : TEXT("0")},
		{TEXT("PlayerCount"), FString::FromInt(PlayerCount)},
	});
}

void UBalanceTelemetryComponent::EmitForServer(
	FName EventName,
	const TArray<FBalanceTelemetryField>& Fields)
{
	if (!CanEmitForServer() || SessionId.IsEmpty() || bSessionEnded)
	{
		return;
	}

	const double RelativeSeconds = GetWorld()
		? FMath::Max(0.0, GetWorld()->GetTimeSeconds() - SessionStartWorldSeconds)
		: 0.0;
	FString Line;
	if (!FBalanceTelemetryFormatter::TryFormat(
		SessionId,
		NextSequence,
		RelativeSeconds,
		ResolveEnvironment(),
		BuildVersion,
		BalanceVersion,
		EventName,
		Fields,
		Line))
	{
		return;
	}

	++NextSequence;
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] %s"), *Line);
#if WITH_DEV_AUTOMATION_TESTS
	EmittedLinesForTest.Add(Line);
#endif
}

void UBalanceTelemetryComponent::EndSessionForServer(FName Outcome, int32 PlayerCount, float BossHPRemaining)
{
	if (bSessionEnded || SessionId.IsEmpty())
	{
		return;
	}

	const double Duration = GetWorld()
		? FMath::Max(0.0, GetWorld()->GetTimeSeconds() - SessionStartWorldSeconds)
		: 0.0;
	const FName NormalizedOutcome = Outcome == FName(TEXT("RaidTimeLimit"))
		? FName(TEXT("TimeOver"))
		: (Outcome.IsNone() ? FName(TEXT("Manual")) : Outcome);
	EmitForServer(TEXT("RaidEnded"), {
		{TEXT("Outcome"), NormalizedOutcome.ToString()},
		{TEXT("PlayerCount"), FString::FromInt(PlayerCount)},
		{TEXT("Duration"), Number(Duration)},
		{TEXT("BossHPRemaining"), Number(BossHPRemaining)},
		{TEXT("CompletionState"), TEXT("Completed")},
	});
	bSessionEnded = true;
}

UBalanceTelemetryComponent* UBalanceTelemetryComponent::FindForServer(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	if (ARaidGameMode* GameMode = World->GetAuthGameMode<ARaidGameMode>())
	{
		return GameMode->GetBalanceTelemetryForServer();
	}
	for (TActorIterator<ARaidGameMode> It(World); It; ++It)
	{
		return It->GetBalanceTelemetryForServer();
	}
	return nullptr;
}

FString UBalanceTelemetryComponent::Number(double Value, int32 Precision)
{
	return FString::Printf(TEXT("%.*f"), FMath::Clamp(Precision, 0, 6), Value);
}

bool UBalanceTelemetryComponent::CanEmitForServer() const
{
	return GetOwner() && GetOwner()->HasAuthority();
}

FName UBalanceTelemetryComponent::ResolveEnvironment() const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (GIsAutomationTesting)
	{
		return TEXT("Automation");
	}
#endif
	const UWorld* World = GetWorld();
	if (World && World->WorldType == EWorldType::PIE)
	{
		return TEXT("PIE");
	}
	return IsRunningDedicatedServer() ? FName(TEXT("DedicatedServer")) : FName(TEXT("Standalone"));
}
