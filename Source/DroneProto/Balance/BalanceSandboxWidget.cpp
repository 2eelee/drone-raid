#include "BalanceSandboxWidget.h"

#include "Balance/BalanceSandboxGameMode.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/TextBlock.h"
#include "Drone.h"
#include "Engine/World.h"
#include "Raid/BossPatternComponent.h"
#include "Raid/DronePartInventory.h"
#include "Raid/RaidBoss.h"
#include "Raid/RaidGameState.h"
#include "Raid/RaidPlayerController.h"
#include "TimerManager.h"

TArray<FString> UBalanceSandboxWidget::GetCoreOptions()
{
	return {TEXT("None"), TEXT("Zenith"), TEXT("Booster"), TEXT("Drain")};
}

TArray<FString> UBalanceSandboxWidget::GetWeaponOptions()
{
	return {TEXT("None"), TEXT("Pulse"), TEXT("Fracture"), TEXT("Vector")};
}

ABalanceSandboxGameMode* UBalanceSandboxWidget::GetSandboxGameMode() const
{
	UWorld* World = GetWorld();
	return World ? World->GetAuthGameMode<ABalanceSandboxGameMode>() : nullptr;
}

void UBalanceSandboxWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindSandboxWidgetEvents();
	RefreshSandboxUI();
	RefreshStatusPanel();
	StartStatusRefreshTimer();

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] SandboxUI State=Ready GameMode=%s Core=%s Left=%s Right=%s"),
		GetSandboxGameMode() ? TEXT("BalanceSandbox") : TEXT("Missing"),
		*CoreSelection,
		*LeftWeaponSelection,
		*RightWeaponSelection);
}

void UBalanceSandboxWidget::NativeDestruct()
{
	StopStatusRefreshTimer();
	UnbindSandboxWidgetEvents();
	Super::NativeDestruct();
}

void UBalanceSandboxWidget::BindSandboxWidgetEvents()
{
	if (Button_ApplyLoadout)
	{
		Button_ApplyLoadout->OnClicked.AddUniqueDynamic(this, &UBalanceSandboxWidget::HandleApplyLoadoutClicked);
	}
	if (Button_StartBattle)
	{
		Button_StartBattle->OnClicked.AddUniqueDynamic(this, &UBalanceSandboxWidget::HandleStartBattleClicked);
	}
	if (Button_Corrupted)
	{
		Button_Corrupted->OnClicked.AddUniqueDynamic(this, &UBalanceSandboxWidget::HandleCorruptedClicked);
	}
	if (Button_Stellar)
	{
		Button_Stellar->OnClicked.AddUniqueDynamic(this, &UBalanceSandboxWidget::HandleStellarClicked);
	}
	if (Button_Report)
	{
		Button_Report->OnClicked.AddUniqueDynamic(this, &UBalanceSandboxWidget::HandleReportClicked);
	}
	if (Button_Reset)
	{
		Button_Reset->OnClicked.AddUniqueDynamic(this, &UBalanceSandboxWidget::HandleResetClicked);
	}

	if (ComboBox_Core)
	{
		ComboBox_Core->OnSelectionChanged.AddUniqueDynamic(this, &UBalanceSandboxWidget::HandleCoreSelectionChanged);
	}
	if (ComboBox_LeftWeapon)
	{
		ComboBox_LeftWeapon->OnSelectionChanged.AddUniqueDynamic(this, &UBalanceSandboxWidget::HandleLeftWeaponSelectionChanged);
	}
	if (ComboBox_RightWeapon)
	{
		ComboBox_RightWeapon->OnSelectionChanged.AddUniqueDynamic(this, &UBalanceSandboxWidget::HandleRightWeaponSelectionChanged);
	}
}

void UBalanceSandboxWidget::UnbindSandboxWidgetEvents()
{
	if (Button_ApplyLoadout)
	{
		Button_ApplyLoadout->OnClicked.RemoveDynamic(this, &UBalanceSandboxWidget::HandleApplyLoadoutClicked);
	}
	if (Button_StartBattle)
	{
		Button_StartBattle->OnClicked.RemoveDynamic(this, &UBalanceSandboxWidget::HandleStartBattleClicked);
	}
	if (Button_Corrupted)
	{
		Button_Corrupted->OnClicked.RemoveDynamic(this, &UBalanceSandboxWidget::HandleCorruptedClicked);
	}
	if (Button_Stellar)
	{
		Button_Stellar->OnClicked.RemoveDynamic(this, &UBalanceSandboxWidget::HandleStellarClicked);
	}
	if (Button_Report)
	{
		Button_Report->OnClicked.RemoveDynamic(this, &UBalanceSandboxWidget::HandleReportClicked);
	}
	if (Button_Reset)
	{
		Button_Reset->OnClicked.RemoveDynamic(this, &UBalanceSandboxWidget::HandleResetClicked);
	}

	if (ComboBox_Core)
	{
		ComboBox_Core->OnSelectionChanged.RemoveDynamic(this, &UBalanceSandboxWidget::HandleCoreSelectionChanged);
	}
	if (ComboBox_LeftWeapon)
	{
		ComboBox_LeftWeapon->OnSelectionChanged.RemoveDynamic(this, &UBalanceSandboxWidget::HandleLeftWeaponSelectionChanged);
	}
	if (ComboBox_RightWeapon)
	{
		ComboBox_RightWeapon->OnSelectionChanged.RemoveDynamic(this, &UBalanceSandboxWidget::HandleRightWeaponSelectionChanged);
	}
}

void UBalanceSandboxWidget::PopulateComboBox(
	UComboBoxString* ComboBox,
	const TArray<FString>& Options,
	const FString& CurrentSelection)
{
	if (!ComboBox)
	{
		return;
	}

	ComboBox->ClearOptions();
	for (const FString& Option : Options)
	{
		ComboBox->AddOption(Option);
	}
	ComboBox->SetSelectedOption(CurrentSelection);
}

void UBalanceSandboxWidget::RefreshSandboxUI()
{
	PopulateComboBox(ComboBox_Core, GetCoreOptions(), CoreSelection);
	PopulateComboBox(ComboBox_LeftWeapon, GetWeaponOptions(), LeftWeaponSelection);
	PopulateComboBox(ComboBox_RightWeapon, GetWeaponOptions(), RightWeaponSelection);

	if (!GetSandboxGameMode())
	{
		SetStatusText(TEXT("BalanceSandboxGameMode 아님 — World Settings의 GameMode Override를 확인하세요."));
		return;
	}

	SetStatusText(FString::Printf(TEXT("Core=%s Left=%s Right=%s"), *CoreSelection, *LeftWeaponSelection, *RightWeaponSelection));
}

void UBalanceSandboxWidget::SetStatusText(const FString& Status)
{
	if (Text_SandboxStatus)
	{
		Text_SandboxStatus->SetText(FText::FromString(Status));
	}
}

void UBalanceSandboxWidget::SetCoreSelection(const FString& CoreAlias)
{
	CoreSelection = CoreAlias.IsEmpty() ? TEXT("None") : CoreAlias;
	if (ComboBox_Core)
	{
		ComboBox_Core->SetSelectedOption(CoreSelection);
	}
}

void UBalanceSandboxWidget::SetLeftWeaponSelection(const FString& WeaponAlias)
{
	LeftWeaponSelection = WeaponAlias.IsEmpty() ? TEXT("None") : WeaponAlias;
	if (ComboBox_LeftWeapon)
	{
		ComboBox_LeftWeapon->SetSelectedOption(LeftWeaponSelection);
	}
}

void UBalanceSandboxWidget::SetRightWeaponSelection(const FString& WeaponAlias)
{
	RightWeaponSelection = WeaponAlias.IsEmpty() ? TEXT("None") : WeaponAlias;
	if (ComboBox_RightWeapon)
	{
		ComboBox_RightWeapon->SetSelectedOption(RightWeaponSelection);
	}
}

void UBalanceSandboxWidget::HandleCoreSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	CoreSelection = SelectedItem.IsEmpty() ? TEXT("None") : SelectedItem;
}

void UBalanceSandboxWidget::HandleLeftWeaponSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	LeftWeaponSelection = SelectedItem.IsEmpty() ? TEXT("None") : SelectedItem;
}

void UBalanceSandboxWidget::HandleRightWeaponSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	RightWeaponSelection = SelectedItem.IsEmpty() ? TEXT("None") : SelectedItem;
}

bool UBalanceSandboxWidget::ApplyLoadout()
{
	ABalanceSandboxGameMode* SandboxGameMode = GetSandboxGameMode();
	if (!SandboxGameMode)
	{
		SetStatusText(TEXT("실패: BalanceSandboxGameMode 없음"));
		return false;
	}

	// 콘솔 명령과 같은 경로다. 재고 확인·교체 원자 커밋은 전부 기존 선택 경로가 한다.
	const bool bRequested = SandboxGameMode->ApplySandboxLoadoutForServer(CoreSelection, LeftWeaponSelection, RightWeaponSelection);
	SetStatusText(FString::Printf(TEXT("Loadout %s — Core=%s Left=%s Right=%s"),
		bRequested ? TEXT("요청됨") : TEXT("실패"),
		*CoreSelection,
		*LeftWeaponSelection,
		*RightWeaponSelection));
	return bRequested;
}

bool UBalanceSandboxWidget::StartBattle()
{
	ABalanceSandboxGameMode* SandboxGameMode = GetSandboxGameMode();
	if (!SandboxGameMode)
	{
		SetStatusText(TEXT("실패: BalanceSandboxGameMode 없음"));
		return false;
	}

	const bool bStarted = SandboxGameMode->StartSandboxBattleForServer();
	SetStatusText(bStarted ? TEXT("전투 시작 요청됨") : TEXT("전투 시작 실패"));
	return bStarted;
}

bool UBalanceSandboxWidget::RunCorruptedPattern()
{
	ABalanceSandboxGameMode* SandboxGameMode = GetSandboxGameMode();
	if (!SandboxGameMode)
	{
		SetStatusText(TEXT("실패: BalanceSandboxGameMode 없음"));
		return false;
	}

	const bool bRan = SandboxGameMode->RunSandboxPatternForServer(TEXT("corrupted"));
	SetStatusText(bRan ? TEXT("Corrupted 실행") : TEXT("Corrupted 실행 실패 — 보스가 있는지 확인"));
	return bRan;
}

bool UBalanceSandboxWidget::RunStellarPattern()
{
	ABalanceSandboxGameMode* SandboxGameMode = GetSandboxGameMode();
	if (!SandboxGameMode)
	{
		SetStatusText(TEXT("실패: BalanceSandboxGameMode 없음"));
		return false;
	}

	const bool bRan = SandboxGameMode->RunSandboxPatternForServer(TEXT("stellar"));
	SetStatusText(bRan ? TEXT("Stellar 실행") : TEXT("Stellar 실행 실패 — 보스가 있는지 확인"));
	return bRan;
}

bool UBalanceSandboxWidget::CreateDroneReport()
{
	ABalanceSandboxGameMode* SandboxGameMode = GetSandboxGameMode();
	if (!SandboxGameMode)
	{
		SetStatusText(TEXT("실패: BalanceSandboxGameMode 없음"));
		return false;
	}

	const bool bCreated = SandboxGameMode->CreateSandboxReportForServer();
	SetStatusText(bCreated ? TEXT("DroneReport 열기") : TEXT("DroneReport 거부 — 저장된 전투 기록 없음"));
	return bCreated;
}

bool UBalanceSandboxWidget::ResetSandbox()
{
	ABalanceSandboxGameMode* SandboxGameMode = GetSandboxGameMode();
	if (!SandboxGameMode)
	{
		SetStatusText(TEXT("실패: BalanceSandboxGameMode 없음"));
		return false;
	}

	const bool bReset = SandboxGameMode->ResetSandboxRaidForServer();
	// 초기화 직후의 새 상태를 바로 보여 준다. 타이머를 기다리면 이전 값이 잠깐 남는다.
	RefreshStatusPanel();
	SetStatusText(bReset ? TEXT("초기화 완료 — 선택 단계로 복귀") : TEXT("초기화 실패"));
	return bReset;
}

void UBalanceSandboxWidget::HandleApplyLoadoutClicked()
{
	ApplyLoadout();
}

void UBalanceSandboxWidget::HandleStartBattleClicked()
{
	StartBattle();
}

void UBalanceSandboxWidget::HandleCorruptedClicked()
{
	RunCorruptedPattern();
}

void UBalanceSandboxWidget::HandleStellarClicked()
{
	RunStellarPattern();
}

void UBalanceSandboxWidget::HandleReportClicked()
{
	CreateDroneReport();
}

void UBalanceSandboxWidget::HandleResetClicked()
{
	ResetSandbox();
}

void UBalanceSandboxWidget::StartStatusRefreshTimer()
{
	UWorld* World = GetWorld();
	if (!World || StatusRefreshIntervalSeconds <= 0.0f)
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		StatusRefreshTimerHandle,
		this,
		&UBalanceSandboxWidget::RefreshStatusPanel,
		StatusRefreshIntervalSeconds,
		true);
}

void UBalanceSandboxWidget::StopStatusRefreshTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StatusRefreshTimerHandle);
	}
}

FBalanceSandboxStatus UBalanceSandboxWidget::GetSandboxStatus() const
{
	FBalanceSandboxStatus Status;

	UWorld* World = GetWorld();
	if (!World)
	{
		return Status;
	}

	ARaidGameState* RaidGameState = World->GetGameState<ARaidGameState>();
	ARaidPlayerController* RaidPC = Cast<ARaidPlayerController>(GetOwningPlayer());
	ADrone* Drone = RaidPC ? Cast<ADrone>(RaidPC->GetPawn()) : nullptr;
	ARaidBoss* Boss = RaidGameState ? RaidGameState->GetRaidBoss() : nullptr;
	ADronePartInventory* Inventory = RaidPC ? RaidPC->GetDronePartInventory() : nullptr;

	if (Drone && RaidPC)
	{
		Status.bHasDrone = true;

		// [LOADOUT] / [PLAYER] — authoritative 값을 그대로 읽는다.
		Status.EquippedCorePartID = RaidPC->GetEquippedPartIDBySlot(EPartSlot::Core);
		Status.EquippedLeftWeaponPartID = RaidPC->GetEquippedPartIDBySlot(EPartSlot::LeftWeapon);
		Status.EquippedRightWeaponPartID = RaidPC->GetEquippedPartIDBySlot(EPartSlot::RightWeapon);
		Status.CurrentHP = Drone->GetHealth();
		Status.MaxHP = Drone->GetMaxHealth();
		Status.CurrentMoveSpeed = Drone->GetCurrentMoveSpeed();

		// 코어 보정값은 기존 규칙 함수를 부작용 없이 한 번 호출해 얻는다.
		const FDroneCoreCalculationResult CoreSnapshot = Drone->GetCoreCalculationSnapshot();
		Status.CoreAttackModifier = CoreSnapshot.CoreAttackModifier;
		Status.CoreBonusAttackModifier = CoreSnapshot.CoreBonusAttackModifier;
		Status.CoreMoveSpeedBonus = CoreSnapshot.MoveSpeedBonus;

		// [LAST ATTACK] — 공격 경로가 기록해 둔 분해값이다. 여기서 다시 계산하지 않는다.
		const FDroneLastAttackBreakdown& LastAttack = Drone->GetLastAttackBreakdown();
		Status.bHasAttacked = LastAttack.bHasAttacked;
		Status.LastLeftWeaponDamage = LastAttack.LeftWeaponDamage;
		Status.LastRightWeaponDamage = LastAttack.RightWeaponDamage;
		Status.LastFinalDamage = LastAttack.FinalDamage;
		Status.LastDamageDealt = LastAttack.DamageDealt;
		Status.LastDrainHealAmount = LastAttack.HealAmount;

		// [WEAPON / CORE STATE]
		Status.LeftPulseAttackCount = Drone->GetPulseAttackCount(true);
		Status.RightPulseAttackCount = Drone->GetPulseAttackCount(false);
		Status.VectorAccumulatedMoveDistanceMeters = Drone->GetVectorAccumulatedMoveDistance();
		Status.BoosterAccumulatedMoveDistanceMeters = Drone->GetBoosterAccumulatedMoveDistance();

		// [RECORD] — 서버 전투 기록을 그대로 읽는다.
		const FDroneCombatRecord Record = Drone->GetCombatRecordForServer();
		Status.TotalDamageDealt = Record.BossDamage;
		Status.TotalHealAmount = Record.HealAmount;
		Status.MoveDistanceMeters = Record.MoveDistance;
		Status.SurvivalTimeSeconds = Record.SurvivalTime;
		Status.HitCount = Record.DamageTakenCount;

		// [DATA SOURCE] 코어·무기 — 해석 전에는 판정하지 않는다.
		// DroneCombatDataFallbackReason의 초기값이 MissingCoreTable이라 첫 해석 전에 그대로 읽으면
		// "아직 안 읽음"이 "표가 없음"으로 보인다. 그 오독이 부트스트랩 문제를 데이터 문제로 만든다.
		Status.bCombatDataSourceKnown = Drone->IsCombatDataResolved();
		if (Status.bCombatDataSourceKnown)
		{
			const EDroneCombatDataFallbackReason FallbackReason = Drone->GetCombatDataFallbackReason();
			const bool bCombatTablesInUse = FallbackReason == EDroneCombatDataFallbackReason::None;
			Status.bCoreDataTableInUse = bCombatTablesInUse;
			Status.bWeaponDataTableInUse = bCombatTablesInUse;
			Status.CombatDataFallbackReason = bCombatTablesInUse ? FName(TEXT("None")) : FName(TEXT("Fallback"));
		}
		else
		{
			Status.CombatDataFallbackReason = FName(TEXT("Unresolved"));
		}
	}

	// [RAID]
	if (Boss)
	{
		Status.bHasBoss = true;
		Status.BossCurrentHP = Boss->GetCurrentHP();
		Status.BossMaxHP = Boss->GetMaxHP();

		if (const UBossPatternComponent* PatternComponent = Boss->FindComponentByClass<UBossPatternComponent>())
		{
			switch (PatternComponent->GetCurrentPattern())
			{
			case EBossPatternKind::CorruptedActino:
				Status.CurrentBossPattern = FName(TEXT("CorruptedActino"));
				break;
			case EBossPatternKind::StellarRemnant:
				Status.CurrentBossPattern = FName(TEXT("StellarRemnant"));
				break;
			default:
				Status.CurrentBossPattern = FName(TEXT("None"));
				break;
			}
			Status.bBossPatternDataTableInUse = PatternComponent->IsPatternDataTableInUse();
			Status.bPatternDataSourceKnown = true;
		}
	}

	if (RaidGameState)
	{
		Status.RaidTimeRemainingSeconds = RaidGameState->GetRaidRemainingSeconds();
	}

	// [STOCK] — 공유 재고를 그대로 읽는다.
	if (Inventory)
	{
		Status.ZenithStock = Inventory->GetCurrentCount(ADronePartInventory::GetCoreZenithPartID());
		Status.BoosterStock = Inventory->GetCurrentCount(ADronePartInventory::GetCoreBoosterPartID());
		Status.DrainStock = Inventory->GetCurrentCount(ADronePartInventory::GetCoreDrainPartID());
		Status.PulseStock = Inventory->GetCurrentCount(ADronePartInventory::GetPulseLaserPartID());
		Status.FractureStock = Inventory->GetCurrentCount(ADronePartInventory::GetFractureBurstPartID());
		Status.VectorStock = Inventory->GetCurrentCount(ADronePartInventory::GetVectorCannonPartID());
	}

	// [DATA SOURCE] 리포트
	if (RaidPC)
	{
		Status.bReportDataTableInUse = RaidPC->AreDroneReportDataTablesLoaded();
	}

	// 판정할 수 있었던 것만 fallback 여부에 넣는다. 드론·보스가 아직 없는 상태를
	// fallback으로 표시하면 부트스트랩 문제를 데이터 문제로 오독하게 된다.
	Status.bAnyDataTableFallback =
		(Status.bCombatDataSourceKnown && (!Status.bCoreDataTableInUse || !Status.bWeaponDataTableInUse))
		|| (Status.bPatternDataSourceKnown && !Status.bBossPatternDataTableInUse)
		|| !Status.bReportDataTableInUse;

	return Status;
}

void UBalanceSandboxWidget::RefreshStatusPanel()
{
	const FBalanceSandboxStatus Status = GetSandboxStatus();

	auto SetIfBound = [](UTextBlock* TextBlock, const FString& Value)
	{
		if (TextBlock)
		{
			TextBlock->SetText(FText::FromString(Value));
		}
	};

	SetIfBound(Text_Loadout, FString::Printf(TEXT("Core %s / L %s / R %s"),
		*Status.EquippedCorePartID.ToString(),
		*Status.EquippedLeftWeaponPartID.ToString(),
		*Status.EquippedRightWeaponPartID.ToString()));

	SetIfBound(Text_Player, Status.bHasDrone
		? FString::Printf(TEXT("HP %d/%d  Speed %.0f  CoreAtk x%.3f  CoreBonus x%.3f"),
			Status.CurrentHP, Status.MaxHP, Status.CurrentMoveSpeed,
			Status.CoreAttackModifier, Status.CoreBonusAttackModifier)
		: FString(TEXT("드론 없음 — SandboxBootstrap 로그의 IsDrone 확인")));

	SetIfBound(Text_LastAttack, Status.bHasAttacked
		? FString::Printf(TEXT("L %.2f + R %.2f -> Final %.2f  Dealt %.2f  Heal %.2f"),
			Status.LastLeftWeaponDamage, Status.LastRightWeaponDamage,
			Status.LastFinalDamage, Status.LastDamageDealt, Status.LastDrainHealAmount)
		: FString(TEXT("아직 공격 없음")));

	SetIfBound(Text_WeaponCoreState, FString::Printf(TEXT("Pulse L%d R%d  Vector %.1fm  Booster %.1fm (bonus %.3f)"),
		Status.LeftPulseAttackCount, Status.RightPulseAttackCount,
		Status.VectorAccumulatedMoveDistanceMeters,
		Status.BoosterAccumulatedMoveDistanceMeters, Status.CoreMoveSpeedBonus));

	SetIfBound(Text_Raid, FString::Printf(TEXT("Boss %.0f/%.0f  Time %.1fs  Pattern %s"),
		Status.BossCurrentHP, Status.BossMaxHP, Status.RaidTimeRemainingSeconds,
		*Status.CurrentBossPattern.ToString()));

	SetIfBound(Text_Record, FString::Printf(TEXT("Damage %.1f  Heal %.1f  Move %.1fm  Survive %.1fs  Hit %d"),
		Status.TotalDamageDealt, Status.TotalHealAmount,
		Status.MoveDistanceMeters, Status.SurvivalTimeSeconds, Status.HitCount));

	SetIfBound(Text_Stock, FString::Printf(TEXT("Zen %d / Boo %d / Dra %d | Pul %d / Fra %d / Vec %d"),
		Status.ZenithStock, Status.BoosterStock, Status.DrainStock,
		Status.PulseStock, Status.FractureStock, Status.VectorStock));

	// 판정 불가(드론·보스 없음)를 fallback과 구분해 표시한다.
	auto SourceLabel = [](bool bKnown, bool bInUse) -> const TCHAR*
	{
		if (!bKnown)
		{
			return TEXT("-");
		}
		return bInUse ? TEXT("OK") : TEXT("FALLBACK");
	};

	SetIfBound(Text_DataSource, FString::Printf(TEXT("%s  Core %s / Weapon %s / Pattern %s / Report %s"),
		Status.bAnyDataTableFallback ? TEXT("*** FALLBACK ***") : TEXT("DataTable"),
		SourceLabel(Status.bCombatDataSourceKnown, Status.bCoreDataTableInUse),
		SourceLabel(Status.bCombatDataSourceKnown, Status.bWeaponDataTableInUse),
		SourceLabel(Status.bPatternDataSourceKnown, Status.bBossPatternDataTableInUse),
		SourceLabel(true, Status.bReportDataTableInUse)));
}
