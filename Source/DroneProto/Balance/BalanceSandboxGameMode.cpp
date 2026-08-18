#include "BalanceSandboxGameMode.h"

#include "Balance/BalanceSandboxPlayerController.h"
#include "Drone.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Raid/BossPatternComponent.h"
#include "Raid/DronePartInventory.h"
#include "Raid/DronePartReturnManager.h"
#include "Raid/RaidBoss.h"
#include "Raid/RaidGameState.h"
#include "Raid/RaidPlayerController.h"

namespace
{
bool TryResolvePatternAlias(const FString& Alias, EBossPatternKind& OutKind)
{
	const FString Normalized = Alias.TrimStartAndEnd().ToLower();
	if (Normalized == TEXT("corrupted") || Normalized == TEXT("corruptedactino") || Normalized == TEXT("c"))
	{
		OutKind = EBossPatternKind::CorruptedActino;
		return true;
	}

	if (Normalized == TEXT("stellar") || Normalized == TEXT("stellarremnant") || Normalized == TEXT("s"))
	{
		OutKind = EBossPatternKind::StellarRemnant;
		return true;
	}

	return false;
}
}

ABalanceSandboxGameMode::ABalanceSandboxGameMode()
{
	// 샌드박스 콘솔 명령을 쓰려면 전용 컨트롤러가 필요하다. 그 외 동작은 전부 RaidGameMode 그대로다.
	PlayerControllerClass = ABalanceSandboxPlayerController::StaticClass();
}

ARaidBoss* ABalanceSandboxGameMode::EnsureRaidBossForServer()
{
	// 스폰·등록은 전부 기존 경로다. 여기서는 확보된 보스에 프록시 크기만 입힌다.
	ARaidBoss* Boss = Super::EnsureRaidBossForServer();
	if (Boss)
	{
		Boss->ApplyVisualProxySize(
			BossProxyVisualWidthMeters,
			BossProxyVisualHeightMeters,
			FName(TEXT("BalanceSandbox")));
	}

	return Boss;
}

bool ABalanceSandboxGameMode::TryResolvePartAlias(const FString& Alias, FName& OutPartID)
{
	const FString Trimmed = Alias.TrimStartAndEnd();
	if (Trimmed.IsEmpty() || Trimmed.ToLower() == TEXT("none"))
	{
		OutPartID = NAME_None;
		return true;
	}

	const FString Normalized = Trimmed.ToLower();
	if (Normalized == TEXT("zenith"))
	{
		OutPartID = ADronePartInventory::GetCoreZenithPartID();
		return true;
	}
	if (Normalized == TEXT("booster"))
	{
		OutPartID = ADronePartInventory::GetCoreBoosterPartID();
		return true;
	}
	if (Normalized == TEXT("drain"))
	{
		OutPartID = ADronePartInventory::GetCoreDrainPartID();
		return true;
	}
	if (Normalized == TEXT("pulse"))
	{
		OutPartID = ADronePartInventory::GetPulseLaserPartID();
		return true;
	}
	if (Normalized == TEXT("fracture"))
	{
		OutPartID = ADronePartInventory::GetFractureBurstPartID();
		return true;
	}
	if (Normalized == TEXT("vector"))
	{
		OutPartID = ADronePartInventory::GetVectorCannonPartID();
		return true;
	}

	// 별칭이 아니면 실제 PartID로 본다. 존재 여부는 기존 선택 경로가 판정한다(STOCK-06).
	OutPartID = FName(*Trimmed);
	return true;
}

ARaidPlayerController* ABalanceSandboxGameMode::GetSandboxPlayerControllerForServer() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (ARaidPlayerController* RaidPC = Cast<ARaidPlayerController>(It->Get()))
		{
			return RaidPC;
		}
	}

	return nullptr;
}

bool ABalanceSandboxGameMode::ApplySandboxLoadoutForServer(
	const FString& CoreAlias,
	const FString& LeftWeaponAlias,
	const FString& RightWeaponAlias)
{
	if (!HasAuthority())
	{
		return false;
	}

	ARaidPlayerController* RaidPC = GetSandboxPlayerControllerForServer();
	if (!RaidPC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DR_SUMMARY] SandboxLoadout Result=Fail Reason=NoPlayerController"));
		return false;
	}

	const TPair<EPartSlot, const FString*> Requests[] = {
		{EPartSlot::Core, &CoreAlias},
		{EPartSlot::LeftWeapon, &LeftWeaponAlias},
		{EPartSlot::RightWeapon, &RightWeaponAlias},
	};

	for (const TPair<EPartSlot, const FString*>& Request : Requests)
	{
		FName PartID = NAME_None;
		TryResolvePartAlias(*Request.Value, PartID);

		// 빈 슬롯 요청은 취소 경로로 보낸다. 선택 경로는 None을 거부하기 때문이다.
		if (PartID.IsNone())
		{
			RaidPC->Server_RequestCancelPart(Request.Key);
			continue;
		}

		// 기존 선택 RPC를 그대로 탄다 — 재고 확인·교체 원자 커밋·결과 통지가 전부 유지된다.
		RaidPC->Server_RequestSelectPart(Request.Key, PartID);
	}

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] SandboxLoadout Result=Requested Core=%s Left=%s Right=%s"),
		*RaidPC->GetSelectedPartIDBySlot(EPartSlot::Core).ToString(),
		*RaidPC->GetSelectedPartIDBySlot(EPartSlot::LeftWeapon).ToString(),
		*RaidPC->GetSelectedPartIDBySlot(EPartSlot::RightWeapon).ToString());
	return true;
}

bool ABalanceSandboxGameMode::StartSandboxBattleForServer()
{
	if (!HasAuthority())
	{
		return false;
	}

	ARaidPlayerController* RaidPC = GetSandboxPlayerControllerForServer();
	if (!RaidPC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DR_SUMMARY] SandboxStart Result=Fail Reason=NoPlayerController"));
		return false;
	}

	// 기존 Ready 경로다. 보스 확보·장착 적용·전투 진입이 전부 그 안에서 일어난다.
	RaidPC->Server_RequestReadyForRaid();

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] SandboxStart Result=Requested SelectionState=%s"),
		ARaidPlayerController::SelectionStateToLogString(RaidPC->GetPlayerSelectionState()));
	return true;
}

bool ABalanceSandboxGameMode::ResetSandboxRaidForServer()
{
	if (!HasAuthority())
	{
		return false;
	}

	UWorld* World = GetWorld();
	ARaidGameState* RaidGameState = World ? World->GetGameState<ARaidGameState>() : nullptr;
	const FName Reason(TEXT("BalanceSandboxReset"));

	StopBossPatternsForServer(Reason);
	ClearRaidTimeLimitTimerForServer(Reason);

	int32 RestartedControllerCount = 0;
	if (World)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			ARaidPlayerController* RaidPC = Cast<ARaidPlayerController>(It->Get());
			if (!RaidPC)
			{
				continue;
			}

			ClearDroneReportKeyForServer(RaidPC, Reason);
			// 부품 반환·드론 복구·선택 타이머 재시작을 한 곳에서 한다. 재고는 기존 반환 경로만 탄다.
			if (RaidPC->RestartSelectionPhaseForServer(Reason))
			{
				++RestartedControllerCount;
			}
		}
	}

	// 보스 HP 복구는 setter를 새로 만들지 않고 기존 스폰 경로를 재사용한다 —
	// 파괴하면 다음 Ready의 EnsureRaidBossForServer가 MaxHP 상태로 다시 만든다.
	if (RaidGameState)
	{
		if (ARaidBoss* Boss = RaidGameState->GetRaidBoss())
		{
			Boss->Destroy();
		}
		RaidGameState->SetRaidBossForServer(nullptr);
		RaidGameState->SetRaidStateForServer(ERaidState::Waiting);
	}

	ResetBossDamageContributionsForServer(Reason);
	ClearDroneReportDataListForServer(Reason);

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] SandboxReset Result=Success Controllers=%d RaidState=%s"),
		RestartedControllerCount,
		RaidGameState ? TEXT("Waiting") : TEXT("NoGameState"));
	return true;
}

bool ABalanceSandboxGameMode::SetSandboxNextPatternForServer(const FString& PatternAlias)
{
	if (!HasAuthority())
	{
		return false;
	}

	EBossPatternKind NextKind = EBossPatternKind::CorruptedActino;
	if (!TryResolvePatternAlias(PatternAlias, NextKind))
	{
		UE_LOG(LogTemp, Warning, TEXT("[DR_SUMMARY] SandboxPattern Result=Fail Reason=UnknownPattern Requested=%s"), *PatternAlias);
		return false;
	}

	UWorld* World = GetWorld();
	ARaidGameState* RaidGameState = World ? World->GetGameState<ARaidGameState>() : nullptr;
	ARaidBoss* Boss = RaidGameState ? RaidGameState->GetRaidBoss() : nullptr;
	UBossPatternComponent* PatternComponent = Boss ? Boss->FindComponentByClass<UBossPatternComponent>() : nullptr;
	if (!PatternComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DR_SUMMARY] SandboxPattern Result=Fail Reason=NoPatternComponent"));
		return false;
	}

	PatternComponent->SetNextPatternForServer(NextKind, FName(TEXT("BalanceSandbox")));
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] SandboxPattern Result=Success Requested=%s"), *PatternAlias);
	return true;
}

bool ABalanceSandboxGameMode::RunSandboxPatternForServer(const FString& PatternAlias)
{
	if (!HasAuthority())
	{
		return false;
	}

	if (!SetSandboxNextPatternForServer(PatternAlias))
	{
		return false;
	}

	// 루프가 멈춰 있으면(전투 전, 초기화 직후) 여기서 시작한다. 이미 돌고 있으면 기존 진행을
	// 끊지 않고 지정한 패턴이 다음 차례에 나온다 — 패턴 순서 계약(PATTERN-01)을 건드리지 않는다.
	StartBossPatternsForServer();

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] SandboxPatternRun Result=Success Requested=%s"), *PatternAlias);
	return true;
}

bool ABalanceSandboxGameMode::DamageSandboxBossForServer(float DamageAmount)
{
	if (!HasAuthority())
	{
		return false;
	}

	UWorld* World = GetWorld();
	ARaidGameState* RaidGameState = World ? World->GetGameState<ARaidGameState>() : nullptr;
	ARaidBoss* Boss = RaidGameState ? RaidGameState->GetRaidBoss() : nullptr;
	if (!Boss)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DR_SUMMARY] SandboxBossDamage Result=Fail Reason=NoBoss"));
		return false;
	}

	ARaidPlayerController* RaidPC = GetSandboxPlayerControllerForServer();
	// 기존 피해 경로다. HP clamp·사망 처리·기여도 집계가 그대로 동작한다.
	Boss->ApplyDamageForServer(DamageAmount, RaidPC, RaidPC ? RaidPC->GetPawn() : nullptr);

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] SandboxBossDamage Result=Success Requested=%.0f CurrentHP=%.0f MaxHP=%.0f"),
		DamageAmount,
		Boss->GetCurrentHP(),
		Boss->GetMaxHP());
	return true;
}

bool ABalanceSandboxGameMode::CreateSandboxReportForServer()
{
	if (!HasAuthority())
	{
		return false;
	}

	ARaidPlayerController* RaidPC = GetSandboxPlayerControllerForServer();
	if (!RaidPC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DR_SUMMARY] SandboxReport Result=Fail Reason=NoPlayerController"));
		return false;
	}

	// 기존 리포트 생성 경로다. 점수·등급·보너스 계산과 1회 제한이 그대로 적용된다.
	const bool bCreated = RaidPC->TryCreateDroneReportForServer(EDroneReportTrigger::RaidTimeLimit, false);
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] SandboxReport Result=%s"), bCreated ? TEXT("Success") : TEXT("Rejected"));
	return bCreated;
}
