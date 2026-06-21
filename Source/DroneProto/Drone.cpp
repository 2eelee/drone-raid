#include "Drone.h"
#include "DronePart.h"
#include "DummyParts.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Components/SceneComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "GameFramework/Controller.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Raid/DronePartInventory.h"
#include "Raid/RaidBoss.h"
#include "Raid/RaidGameState.h"
#include "Raid/RaidPlayerController.h"

namespace
{
bool TryGetStatsForPartID(FName PartID, EPartSlot Slot, FDronePartStats& OutStats)
{
	OutStats = FDronePartStats();

	if (Slot == EPartSlot::Core)
	{
		if (PartID == ADronePartInventory::GetCoreZenithPartID())
		{
			OutStats.HealthBonus = 0;
			OutStats.AttackBonus = 0;
			return true;
		}
		if (PartID == ADronePartInventory::GetCoreBoosterPartID())
		{
			OutStats.HealthBonus = 0;
			OutStats.AttackBonus = 0;
			return true;
		}
		if (PartID == ADronePartInventory::GetCoreDrainPartID())
		{
			OutStats.HealthBonus = 0;
			OutStats.AttackBonus = 0;
			return true;
		}
		return false;
	}

	if (PartID == ADronePartInventory::GetPulseLaserPartID())
	{
		OutStats.HealthBonus = 0;
		OutStats.AttackBonus = 8;
		return true;
	}
	if (PartID == ADronePartInventory::GetFractureBurstPartID())
	{
		OutStats.HealthBonus = 0;
		OutStats.AttackBonus = 11;
		return true;
	}
	if (PartID == ADronePartInventory::GetVectorCannonPartID())
	{
		OutStats.HealthBonus = 0;
		OutStats.AttackBonus = 7;
		return true;
	}

	return false;
}

UDronePart* CreateLoadoutPart(UObject* Outer, FName PartID, EPartSlot Slot, const FDronePartStats& Stats)
{
	UDronePart* NewPart = nullptr;
	switch (Slot)
	{
	case EPartSlot::Core:
		NewPart = NewObject<UCorePart>(Outer);
		break;
	case EPartSlot::LeftWeapon:
		NewPart = NewObject<ULeftWeaponPart>(Outer);
		break;
	case EPartSlot::RightWeapon:
		NewPart = NewObject<URightWeaponPart>(Outer);
		break;
	default:
		return nullptr;
	}

	if (NewPart)
	{
		NewPart->Slot = Slot;
		NewPart->StatContribution = Stats;
	}

	return NewPart;
}

FString BuildDroneControllerLogString(const AController* Controller)
{
	return ARaidPlayerController::BuildStableControllerLogString(Controller);
}

const TCHAR* ToWeaponSlotLogString(bool bIsLeftWeapon)
{
	return bIsLeftWeapon ? TEXT("Left") : TEXT("Right");
}

constexpr float MoveDistanceCmToMeters = 0.01f;
constexpr float MoveDistanceMaxDeltaSeconds = 0.25f;
constexpr float MoveDistanceHardMaxDeltaMeters = 20.0f;
constexpr float MoveDistanceExpectedDeltaMultiplier = 2.0f;
constexpr float MoveDistanceExpectedDeltaSlackMeters = 3.0f;
constexpr float MoveInputSummaryLogIntervalSeconds = 0.50f;
constexpr float MoveDistanceSummaryLogIntervalSeconds = 0.50f;
constexpr float MoveDistanceIgnoredLogIntervalSeconds = 1.00f;
}

ADrone::ADrone()
{
	PrimaryActorTick.bCanEverTick = true;
	SetReplicates(true);
	SetReplicateMovement(true);
	// FloatingPawnMovement 이동은 클라 위치를 믿지 않고 입력 축만 Server RPC로 전달한다.
	// 서버가 이동을 적용하고 ReplicateMovement로 위치를 동기화한다.

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	FloatingMovement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("FloatingMovement"));

	MuzzlePoint = CreateDefaultSubobject<USceneComponent>(TEXT("MuzzlePoint"));
	MuzzlePoint->SetupAttachment(RootComponent);
	MuzzlePoint->SetRelativeLocation(FVector(80.f, 0.f, 0.f));
}

void ADrone::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		ResetMoveDistanceForServer(FName(TEXT("Spawn")));
	}

	// 로컬 컨트롤러(autonomous proxy)에만 입력 컨텍스트 등록
	if (IsLocallyControlled())
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			if (UEnhancedInputLocalPlayerSubsystem* Sub =
				ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
			{
				if (DefaultMappingContext)
					Sub->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}

	// D3 검증용, 추후 제거 — 슬롯별 3종 장착 → MaxHealth=220, AttackPower=35 복제 확인
	// D3 temporary auto-equip is intentionally disabled.
	// TODO(D5 Ready): Apply selected parts after RequestReadyForRaidFromUI.
}

void ADrone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority())
	{
		UpdateMoveDistanceForServer(DeltaSeconds);
	}
}

void ADrone::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (HasAuthority())
	{
		ResetMoveDistanceForServer(FName(TEXT("Possess")));
	}
}

void ADrone::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (MoveAction)
			EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ADrone::Move);
	}

	PlayerInputComponent->BindKey(EKeys::Z, IE_Pressed, this, &ADrone::RequestAttackBoss);
}

void ADrone::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (Axis.IsNearlyZero())
	{
		return;
	}

	if (HasAuthority())
	{
		ApplyMoveInputForServer(Axis);
		return;
	}

	Server_SetMoveInput(Axis);
}

float ADrone::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
                         AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority())
		return 0.f;

	if (bIsDead)
	{
		return 0.f;
	}

	const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	ApplyDamageForServer(FMath::RoundToInt(Applied), FName(TEXT("TakeDamage")));

	return Applied;
}

void ADrone::ApplyDamageForServer(int32 DamageAmount, FName Reason)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Client] ApplyDamageForServer rejected: server authority required"));
		return;
	}

	if (bIsDead)
	{
		return;
	}

	const int32 AppliedDamage = FMath::Max(0, DamageAmount);
	if (AppliedDamage <= 0)
	{
		return;
	}

	Health = FMath::Clamp(Health - static_cast<float>(AppliedDamage), 0.0f, static_cast<float>(MaxHealth));
	ForceNetUpdate();

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] DroneDamage PC=%s Drone=%s Damage=%d HP=%.2f/%d"),
		*BuildDroneControllerLogString(Cast<AController>(GetController())),
		*GetName(),
		AppliedDamage,
		Health,
		MaxHealth);

	UE_LOG(LogTemp, Log, TEXT("[Server] DroneDamage: Drone=%s Reason=%s Health=%.2f/%d"),
		*GetName(),
		Reason.IsNone() ? TEXT("None") : *Reason.ToString(),
		Health,
		MaxHealth);

	if (Health <= 0.0f)
	{
		HandleDeath();
	}
}

bool ADrone::HealForServer(int32 HealAmount)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Client] HealForServer rejected: server authority required"));
		return false;
	}

	if (bIsDead)
	{
		LogDeadInputIgnored(TEXT("Heal"));
		return false;
	}

	const int32 AppliedHeal = FMath::Max(0, HealAmount);
	if (AppliedHeal <= 0)
	{
		return false;
	}

	const float PreviousHealth = Health;
	Health = FMath::Clamp(Health + static_cast<float>(AppliedHeal), 0.0f, static_cast<float>(MaxHealth));
	ForceNetUpdate();
	return Health > PreviousHealth + KINDA_SMALL_NUMBER;
}

bool ADrone::RequestDodgeForServer()
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Client] RequestDodgeForServer rejected: server authority required"));
		return false;
	}

	if (bIsDead)
	{
		LogDeadInputIgnored(TEXT("Dodge"));
		return false;
	}

	UE_LOG(LogTemp, VeryVerbose, TEXT("[Server] Dodge request accepted for future implementation: Drone=%s"), *GetName());
	return true;
}

void ADrone::ResetCombatRuntimeStateForServer()
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Client] ResetCombatRuntimeStateForServer rejected: server authority required"));
		return;
	}

	ResetCombatRuntimeStateForReason(FName(TEXT("RaidEnd")));
}

bool ADrone::ApplyLoadout(FName CorePartID, FName LeftWeaponPartID, FName RightWeaponPartID)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Client] ApplyLoadout rejected: server authority required"));
		return false;
	}

	FDronePartStats CoreStats;
	FDronePartStats LeftStats;
	FDronePartStats RightStats;
	FName ResolvedCorePartID = CorePartID;
	FName ResolvedLeftWeaponPartID = LeftWeaponPartID;
	FName ResolvedRightWeaponPartID = RightWeaponPartID;

	const auto ResolvePart = [this](FName RequestedPartID, EPartSlot Slot, FDronePartStats& OutStats, FName& OutResolvedPartID) -> bool
	{
		OutStats = FDronePartStats();
		OutResolvedPartID = RequestedPartID;

		if (RequestedPartID.IsNone())
		{
			return true;
		}

		if (TryGetStatsForPartID(RequestedPartID, Slot, OutStats))
		{
			return true;
		}

		UE_LOG(LogTemp, Warning, TEXT("[Server] ApplyLoadout MissingData: Slot=%d Part=%s treated as empty"),
			static_cast<int32>(Slot),
			*RequestedPartID.ToString());
		OutResolvedPartID = NAME_None;
		return true;
	};

	ResolvePart(CorePartID, EPartSlot::Core, CoreStats, ResolvedCorePartID);
	ResolvePart(LeftWeaponPartID, EPartSlot::LeftWeapon, LeftStats, ResolvedLeftWeaponPartID);
	ResolvePart(RightWeaponPartID, EPartSlot::RightWeapon, RightStats, ResolvedRightWeaponPartID);

	const auto AddLoadoutPart = [this](FName PartID, EPartSlot Slot, const FDronePartStats& Stats) -> bool
	{
		if (PartID.IsNone())
		{
			return true;
		}

		UDronePart* NewPart = CreateLoadoutPart(this, PartID, Slot, Stats);
		if (!NewPart)
		{
			return false;
		}

		EquippedParts.Add(NewPart);
		return true;
	};

	EquippedParts.Reset();
	if (!AddLoadoutPart(ResolvedCorePartID, EPartSlot::Core, CoreStats)
		|| !AddLoadoutPart(ResolvedLeftWeaponPartID, EPartSlot::LeftWeapon, LeftStats)
		|| !AddLoadoutPart(ResolvedRightWeaponPartID, EPartSlot::RightWeapon, RightStats))
	{
		EquippedParts.Reset();
		UE_LOG(LogTemp, Warning, TEXT("[Server] ApplyLoadout Failed: Core=%s Left=%s Right=%s Reason=Part object creation failed"),
			*ResolvedCorePartID.ToString(),
			*ResolvedLeftWeaponPartID.ToString(),
			*ResolvedRightWeaponPartID.ToString());
		return false;
	}

	EquippedCorePartID = ResolvedCorePartID;
	EquippedLeftWeaponPartID = ResolvedLeftWeaponPartID;
	EquippedRightWeaponPartID = ResolvedRightWeaponPartID;
	ResetCombatRuntimeStateForReason(FName(TEXT("Loadout")));
	RecalculateStats();
	bIsDead = false;
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[Server] ApplyLoadout Success: Core=%s Left=%s Right=%s MaxHealth=%d AttackPower=%d"),
		*EquippedCorePartID.ToString(),
		*EquippedLeftWeaponPartID.ToString(),
		*EquippedRightWeaponPartID.ToString(),
		MaxHealth,
		AttackPower);

	return true;
}

void ADrone::RequestAttackBoss()
{
	if (HasAuthority())
	{
		HandleAttackBossForServer();
		return;
	}

	Server_RequestAttackBoss();
}

void ADrone::Server_RequestAttackBoss_Implementation()
{
	HandleAttackBossForServer();
}

void ADrone::Server_SetMoveInput_Implementation(FVector2D RawAxis)
{
	ApplyMoveInputForServer(RawAxis);
}

int32 ADrone::GetHealth() const
{
	return FMath::RoundToInt(Health);
}

int32 ADrone::GetMaxHealth() const
{
	return MaxHealth;
}

int32 ADrone::GetAttackPower() const
{
	return AttackPower;
}

bool ADrone::IsDead() const
{
	return bIsDead;
}

#if WITH_DEV_AUTOMATION_TESTS
int32 ADrone::GetPulseAttackCountForTest(bool bIsLeftWeapon) const
{
	return bIsLeftWeapon ? LeftPulseAttackCount : RightPulseAttackCount;
}

float ADrone::GetHealthValueForTest() const
{
	return Health;
}

bool ADrone::ApplyMoveInputForServerForTest(FVector2D RawAxis)
{
	return ApplyMoveInputForServer(RawAxis);
}

void ADrone::UpdateMoveDistanceForServerForTest(float DeltaSeconds)
{
	UpdateMoveDistanceForServer(DeltaSeconds);
}

void ADrone::ResetMoveDistanceForServerForTest(FName Reason)
{
	ResetMoveDistanceForServer(Reason);
}

void ADrone::ResetVectorMoveDistanceForServerForTest(FName Reason)
{
	ResetVectorMoveDistanceForServer(Reason);
}

float ADrone::GetVectorAccumulatedMoveDistanceForTest() const
{
	return VectorAccumulatedMoveDistanceMeters;
}

float ADrone::GetBoosterAccumulatedMoveDistanceForTest() const
{
	return BoosterAccumulatedMoveDistanceMeters;
}

FVector2D ADrone::GetLastServerMoveInputForTest() const
{
	return LastServerMoveInput;
}
#endif

void ADrone::OnRep_Health()
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Cyan,
			FString::Printf(TEXT("[Client] Health=%.2f  MaxHealth=%d  AttackPower=%d"),
				Health, MaxHealth, AttackPower));
	}
}

void ADrone::OnRep_IsDead()
{
	if (GEngine && bIsDead)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Red,
			FString::Printf(TEXT("[Client] Drone dead: %s"), *GetName()));
	}
}

void ADrone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADrone, Health);
	DOREPLIFETIME(ADrone, MaxHealth);
	DOREPLIFETIME(ADrone, AttackPower);
	DOREPLIFETIME(ADrone, bIsDead);
}

void ADrone::ServerEquipPart(TSubclassOf<UDronePart> PartClass)
{
	if (!HasAuthority() || !PartClass)
		return;

	UDronePart* NewPart = NewObject<UDronePart>(this, PartClass);
	EquippedParts.Add(NewPart);
	RecalculateStats();

	UE_LOG(LogTemp, Log, TEXT("[Server] Equipped: %s"), *PartClass->GetName());
}

void ADrone::RecalculateStats()
{
	int32 TotalHealth = 100;
	int32 TotalAttack = 0;

	for (const UDronePart* Part : EquippedParts)
	{
		if (Part)
		{
			TotalHealth += Part->StatContribution.HealthBonus;
			TotalAttack += Part->StatContribution.AttackBonus;
		}
	}

	MaxHealth   = TotalHealth;
	AttackPower = TotalAttack;
	Health      = MaxHealth;  // 장착 시 HP 최대치 초기화 — OnRep_Health 트리거 보장
	             // 암묵 제약: 전투 중 RecalculateStats 호출 금지 (풀피 회복 버그)

	UE_LOG(LogTemp, Log, TEXT("[Server] Stats: MaxHealth=%d, AttackPower=%d"), MaxHealth, AttackPower);
}

void ADrone::HandleDeath()
{
	if (!HasAuthority() || bIsDead)
	{
		return;
	}

	bIsDead = true;
	Health = 0.0f;
	ResetCombatRuntimeStateForReason(FName(TEXT("Death")));
	ForceNetUpdate();

	if (ARaidPlayerController* RaidPC = Cast<ARaidPlayerController>(GetController()))
	{
		const FName CorePartID = RaidPC->GetEquippedPartIDBySlot(EPartSlot::Core);
		const FName LeftWeaponPartID = RaidPC->GetEquippedPartIDBySlot(EPartSlot::LeftWeapon);
		const FName RightWeaponPartID = RaidPC->GetEquippedPartIDBySlot(EPartSlot::RightWeapon);
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] DroneDeath PC=%s Drone=%s Reason=HPZero Core=%s Left=%s Right=%s"),
			*BuildDroneControllerLogString(Cast<AController>(GetController())),
			*GetName(),
			*CorePartID.ToString(),
			*LeftWeaponPartID.ToString(),
			*RightWeaponPartID.ToString());
		RaidPC->ReturnEquippedPartsForServer(EDronePartReturnReason::Death);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] DroneDeath PC=%s Drone=%s Reason=HPZero Core=%s Left=%s Right=%s"),
			*BuildDroneControllerLogString(Cast<AController>(GetController())),
			*GetName(),
			*EquippedCorePartID.ToString(),
			*EquippedLeftWeaponPartID.ToString(),
			*EquippedRightWeaponPartID.ToString());
	}

	ClearEquippedPartsForServer();

	// TODO(D5 UI): show drone death state when the combat death UI flow exists.
	UE_LOG(LogTemp, Log, TEXT("[Server] Drone death handled: equipped parts returned"));
}

void ADrone::HandleAttackBossForServer()
{
	if (!HasAuthority())
	{
		return;
	}

	ARaidPlayerController* RaidPC = Cast<ARaidPlayerController>(GetController());
	if (RaidPC && RaidPC->GetPlayerSelectionState() != EPlayerSelectionState::InBattle)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] AttackIgnored PC=%s Reason=NotInBattle SelectionState=%s"),
			*BuildDroneControllerLogString(RaidPC),
			ARaidPlayerController::SelectionStateToLogString(RaidPC->GetPlayerSelectionState()));
		return;
	}

	if (bIsDead)
	{
		LogDeadInputIgnored(TEXT("Attack"));
		return;
	}

	ARaidBoss* Boss = FindRaidBossForServer();
	if (!Boss)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Server] Drone ZAttack failed: Drone=%s Reason=RaidBoss missing"), *GetName());
		return;
	}

	const float LeftWeaponDamage = CalculateWeaponDamageForServer(EquippedLeftWeaponPartID, true);
	const float RightWeaponDamage = CalculateWeaponDamageForServer(EquippedRightWeaponPartID, false);
	const float TotalWeaponDamage = LeftWeaponDamage + RightWeaponDamage;
	const float CoreAttackModifier = GetCoreAttackModifierForServer(EquippedCorePartID);
	const float CoreBonusAttackModifier = GetCoreBonusAttackModifierForServer(EquippedCorePartID);
	const float FinalDamage = TotalWeaponDamage * CoreAttackModifier * CoreBonusAttackModifier;

	const float BossHPBeforeAttack = Boss->GetCurrentHP();
	Boss->ApplyDamageForServer(FinalDamage, Cast<AController>(GetController()), this);
	const float DamageDealt = FMath::Max(0.0f, BossHPBeforeAttack - Boss->GetCurrentHP());
	if (EquippedCorePartID == ADronePartInventory::GetCoreDrainPartID())
	{
		ApplyDrainHealForServer(DamageDealt);
	}

	UE_LOG(LogTemp, Log, TEXT("[Server] Drone ZAttack: Drone=%s CorePartID=%s LeftWeaponPartID=%s RightWeaponPartID=%s LeftDamage=%.2f RightDamage=%.2f TotalWeaponDamage=%.2f CoreAttackModifier=%.2f CoreBonusAttackModifier=%.2f FinalDamage=%.2f BossHP=%.2f/%.2f"),
		*GetName(),
		*EquippedCorePartID.ToString(),
		*EquippedLeftWeaponPartID.ToString(),
		*EquippedRightWeaponPartID.ToString(),
		LeftWeaponDamage,
		RightWeaponDamage,
		TotalWeaponDamage,
		CoreAttackModifier,
		CoreBonusAttackModifier,
		FinalDamage,
		Boss->GetCurrentHP(),
		Boss->GetMaxHP());

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Attack PC=%s Drone=%s Core=%s Left=%s Right=%s Damage=%.2f BossHP=%.2f/%.2f"),
		*BuildDroneControllerLogString(Cast<AController>(GetController())),
		*GetName(),
		*EquippedCorePartID.ToString(),
		*EquippedLeftWeaponPartID.ToString(),
		*EquippedRightWeaponPartID.ToString(),
		FinalDamage,
		Boss->GetCurrentHP(),
		Boss->GetMaxHP());
}

FVector2D ADrone::ClampMoveInputAxisForServer(FVector2D RawAxis, bool& bOutWasClamped) const
{
	bOutWasClamped = false;
	if (!FMath::IsFinite(RawAxis.X) || !FMath::IsFinite(RawAxis.Y))
	{
		bOutWasClamped = true;
		return FVector2D::ZeroVector;
	}

	const float RawSize = RawAxis.Size();
	if (RawSize > 1.0f)
	{
		bOutWasClamped = true;
		return RawAxis / RawSize;
	}

	return RawAxis;
}

bool ADrone::ApplyMoveInputForServer(FVector2D RawAxis)
{
	if (!HasAuthority())
	{
		return false;
	}

	bool bWasClamped = false;
	const FVector2D Axis = ClampMoveInputAxisForServer(RawAxis, bWasClamped);
	LastServerMoveInput = FVector2D::ZeroVector;

	ARaidPlayerController* RaidPC = Cast<ARaidPlayerController>(GetController());
	if (!RaidPC)
	{
		LogMoveInputSummary(TEXT("Ignored"), TEXT("NoPawn"), Axis);
		return false;
	}

	if (RaidPC->GetPawn() != this)
	{
		LogMoveInputSummary(TEXT("Ignored"), TEXT("PossessMismatch"), Axis);
		return false;
	}

	if (bIsDead)
	{
		LogDeadInputIgnored(TEXT("Move"));
		LogMoveInputSummary(TEXT("Ignored"), TEXT("Dead"), Axis);
		return false;
	}

	if (RaidPC->GetPlayerSelectionState() != EPlayerSelectionState::InBattle)
	{
		LogMoveInputSummary(TEXT("Ignored"), TEXT("NotInBattle"), Axis);
		return false;
	}

	if (Axis.IsNearlyZero())
	{
		LogMoveInputSummary(TEXT("Ignored"), TEXT("ZeroAxis"), Axis);
		return false;
	}

	const FRotator YawRot(0.f, RaidPC->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(YawRot).GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y), Axis.X);
	LastServerMoveInput = Axis;

	LogMoveInputSummary(TEXT("Accepted"), bWasClamped ? TEXT("Clamped") : TEXT("OK"), Axis);
	return true;
}

void ADrone::LogMoveInputSummary(const TCHAR* Result, const TCHAR* Reason, const FVector2D& Axis)
{
	UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;
	const FName ResultName(Result ? Result : TEXT("Unknown"));
	const FName ReasonName(Reason ? Reason : TEXT("None"));
	const bool bSameSummary = LastMoveInputSummaryResult == ResultName
		&& LastMoveInputSummaryReason == ReasonName;
	if (bSameSummary && Now - LastMoveInputSummaryLogTime < MoveInputSummaryLogIntervalSeconds)
	{
		return;
	}

	LastMoveInputSummaryResult = ResultName;
	LastMoveInputSummaryReason = ReasonName;
	LastMoveInputSummaryLogTime = Now;

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] MoveInput PC=%s Axis=%s Result=%s Reason=%s"),
		*BuildDroneControllerLogString(Cast<AController>(GetController())),
		*Axis.ToString(),
		Result ? Result : TEXT("Unknown"),
		Reason ? Reason : TEXT("None"));
}

bool ADrone::CanAccumulateMoveDistanceForServer(FName& OutIgnoreReason) const
{
	OutIgnoreReason = NAME_None;
	if (!HasAuthority())
	{
		OutIgnoreReason = FName(TEXT("NoAuthority"));
		return false;
	}

	ARaidPlayerController* RaidPC = Cast<ARaidPlayerController>(GetController());
	if (!RaidPC || RaidPC->GetPawn() != this)
	{
		OutIgnoreReason = FName(TEXT("NoPawn"));
		return false;
	}

	if (bIsDead)
	{
		OutIgnoreReason = FName(TEXT("Dead"));
		return false;
	}

	if (RaidPC->GetPlayerSelectionState() != EPlayerSelectionState::InBattle)
	{
		OutIgnoreReason = FName(TEXT("NotInBattle"));
		return false;
	}

	if (const UWorld* World = GetWorld())
	{
		if (const ARaidGameState* RaidGameState = World->GetGameState<ARaidGameState>())
		{
			if (RaidGameState->RaidState == ERaidState::End)
			{
				OutIgnoreReason = FName(TEXT("RaidEnd"));
				return false;
			}
		}
	}

	return true;
}

void ADrone::UpdateMoveDistanceForServer(float DeltaSeconds)
{
	if (!HasAuthority())
	{
		return;
	}

	const FVector CurrentLocation = GetActorLocation();

	FName IgnoreReason;
	if (!CanAccumulateMoveDistanceForServer(IgnoreReason))
	{
		LastMoveDistanceLocation = CurrentLocation;
		bHasMoveDistanceSample = true;
		LogMoveDistanceIgnored(IgnoreReason);
		return;
	}

	if (!bHasMoveDistanceSample)
	{
		LastMoveDistanceLocation = CurrentLocation;
		bHasMoveDistanceSample = true;
		LogMoveDistanceIgnored(FName(TEXT("FirstSample")));
		return;
	}

	if (DeltaSeconds > MoveDistanceMaxDeltaSeconds)
	{
		LastMoveDistanceLocation = CurrentLocation;
		LogMoveDistanceIgnored(FName(TEXT("Teleport")));
		return;
	}

	const FVector PreviousLocation = LastMoveDistanceLocation;
	const float DeltaMeters = FVector::Dist(CurrentLocation, PreviousLocation) * MoveDistanceCmToMeters;
	LastMoveDistanceLocation = CurrentLocation;
	if (DeltaMeters <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float MaxSpeedCmPerSecond = FloatingMovement ? FMath::Max(0.0f, FloatingMovement->MaxSpeed) : 0.0f;
	const float ExpectedMeters = MaxSpeedCmPerSecond * FMath::Max(DeltaSeconds, 0.0f) * MoveDistanceCmToMeters;
	const float AllowedMeters = FMath::Max(MoveDistanceExpectedDeltaSlackMeters, ExpectedMeters * MoveDistanceExpectedDeltaMultiplier);
	if (DeltaMeters > MoveDistanceHardMaxDeltaMeters || DeltaMeters > AllowedMeters)
	{
		LogMoveDistanceIgnored(FName(TEXT("TooLargeDelta")));
		return;
	}

	VectorAccumulatedMoveDistanceMeters += DeltaMeters;
	BoosterAccumulatedMoveDistanceMeters += DeltaMeters;

	UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;
	if (Now - LastMoveDistanceSummaryLogTime >= MoveDistanceSummaryLogIntervalSeconds)
	{
		LastMoveDistanceSummaryLogTime = Now;
		const ARaidPlayerController* RaidPC = Cast<ARaidPlayerController>(GetController());
		const TCHAR* StateLog = RaidPC
			? ARaidPlayerController::SelectionStateToLogString(RaidPC->GetPlayerSelectionState())
			: TEXT("NoPawn");
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] MoveAudit PC=%s CurrentLocation=%s LastLocation=%s DeltaMeters=%.2f State=%s"),
			*BuildDroneControllerLogString(Cast<AController>(GetController())),
			*CurrentLocation.ToString(),
			*PreviousLocation.ToString(),
			DeltaMeters,
			StateLog);
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] MoveDistance PC=%s Delta=%.2f VectorTotal=%.2f BoosterTotal=%.2f State=%s"),
			*BuildDroneControllerLogString(Cast<AController>(GetController())),
			DeltaMeters,
			VectorAccumulatedMoveDistanceMeters,
			BoosterAccumulatedMoveDistanceMeters,
			StateLog);
	}
}

void ADrone::ResetMoveDistanceForServer(FName Reason)
{
	if (!HasAuthority())
	{
		return;
	}

	VectorAccumulatedMoveDistanceMeters = 0.0f;
	BoosterAccumulatedMoveDistanceMeters = 0.0f;
	LastMoveDistanceLocation = GetActorLocation();
	bHasMoveDistanceSample = false;

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] MoveDistanceReset PC=%s Reason=%s"),
		*BuildDroneControllerLogString(Cast<AController>(GetController())),
		Reason.IsNone() ? TEXT("Unknown") : *Reason.ToString());
}

void ADrone::ResetVectorMoveDistanceForServer(FName Reason)
{
	if (!HasAuthority())
	{
		return;
	}

	VectorAccumulatedMoveDistanceMeters = 0.0f;
	LastMoveDistanceLocation = GetActorLocation();
	bHasMoveDistanceSample = true;

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] MoveDistanceReset PC=%s Reason=%s"),
		*BuildDroneControllerLogString(Cast<AController>(GetController())),
		Reason.IsNone() ? TEXT("VectorAttack") : *Reason.ToString());
}

void ADrone::LogMoveDistanceIgnored(FName Reason)
{
	UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;
	if (LastMoveDistanceIgnoredReason == Reason
		&& Now - LastMoveDistanceIgnoredLogTime < MoveDistanceIgnoredLogIntervalSeconds)
	{
		return;
	}

	LastMoveDistanceIgnoredReason = Reason;
	LastMoveDistanceIgnoredLogTime = Now;
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] MoveDistanceIgnored PC=%s Reason=%s"),
		*BuildDroneControllerLogString(Cast<AController>(GetController())),
		Reason.IsNone() ? TEXT("Unknown") : *Reason.ToString());
}

float ADrone::CalculateWeaponDamageForServer(FName WeaponPartID, bool bIsLeftWeapon)
{
	if (WeaponPartID.IsNone())
	{
		return 0.0f;
	}

	if (WeaponPartID == ADronePartInventory::GetPulseLaserPartID())
	{
		int32& PulseAttackCount = bIsLeftWeapon ? LeftPulseAttackCount : RightPulseAttackCount;
		PulseAttackCount++;
		if (PulseAttackCount >= 3)
		{
			UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] PulseAttack PC=%s Slot=%s Count=3 Damage=18 Reset=true"),
				*BuildDroneControllerLogString(Cast<AController>(GetController())),
				ToWeaponSlotLogString(bIsLeftWeapon));
			PulseAttackCount = 0;
			return 18.0f;
		}

		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] PulseAttack PC=%s Slot=%s Count=%d Damage=8 Reset=false"),
			*BuildDroneControllerLogString(Cast<AController>(GetController())),
			ToWeaponSlotLogString(bIsLeftWeapon),
			PulseAttackCount);
		return 8.0f;
	}

	if (WeaponPartID == ADronePartInventory::GetFractureBurstPartID())
	{
		constexpr float BaseDamage = 5.0f;
		constexpr int32 ShardCount = 3;
		constexpr float ShardDamage = 2.0f;
		constexpr int32 HitCount = 1 + ShardCount;
		const float Damage = BaseDamage + (static_cast<float>(ShardCount) * ShardDamage);
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] FractureAttack PC=%s Slot=%s Base=5 Shards=3 ShardDamage=2 Damage=11 HitCount=%d"),
			*BuildDroneControllerLogString(Cast<AController>(GetController())),
			ToWeaponSlotLogString(bIsLeftWeapon),
			HitCount);
		return Damage;
	}

	if (WeaponPartID == ADronePartInventory::GetVectorCannonPartID())
	{
		UE_LOG(LogTemp, Log, TEXT("[Server] TODO Vector Cannon: movement distance damage bonus not implemented yet"));
		return 7.0f;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Server] Weapon damage missing data: Part=%s treated as 0"), *WeaponPartID.ToString());
	return 0.0f;
}

float ADrone::GetCoreAttackModifierForServer(FName CorePartID) const
{
	if (CorePartID.IsNone())
	{
		return 1.0f;
	}

	if (CorePartID == ADronePartInventory::GetCoreZenithPartID())
	{
		return 1.0f;
	}

	if (CorePartID == ADronePartInventory::GetCoreBoosterPartID())
	{
		UE_LOG(LogTemp, Log, TEXT("[Server] TODO Booster Core: movement speed bonus to attack conversion not implemented yet"));
		return 0.95f;
	}

	if (CorePartID == ADronePartInventory::GetCoreDrainPartID())
	{
		return 0.85f;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Server] Core modifier missing data: Part=%s treated as no core"), *CorePartID.ToString());
	return 1.0f;
}

float ADrone::GetCoreBonusAttackModifierForServer(FName CorePartID) const
{
	if (CorePartID == ADronePartInventory::GetCoreZenithPartID())
	{
		float CurrentHPRatio = MaxHealth > 0 ? Health / static_cast<float>(MaxHealth) : 0.0f;
		if (!FMath::IsFinite(CurrentHPRatio))
		{
			CurrentHPRatio = 0.0f;
		}

		CurrentHPRatio = FMath::Clamp(CurrentHPRatio, 0.0f, 1.0f);
		const float ZenithBonus = FMath::Min(FMath::FloorToFloat(CurrentHPRatio / 0.1f) * 0.02f, 0.20f);
		const float Modifier = 1.0f + ZenithBonus;
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ZenithBonus PC=%s HPRatio=%.2f Bonus=%.2f Modifier=%.2f"),
			*BuildDroneControllerLogString(Cast<AController>(GetController())),
			CurrentHPRatio,
			ZenithBonus,
			Modifier);
		return Modifier;
	}

	if (CorePartID == ADronePartInventory::GetCoreBoosterPartID())
	{
		UE_LOG(LogTemp, Log, TEXT("[Server] TODO Booster Core: accumulated movement distance bonus currently returns 0"));
		return 1.0f;
	}

	return 1.0f;
}

ARaidBoss* ADrone::FindRaidBossForServer() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	if (const ARaidGameState* RaidGameState = World->GetGameState<ARaidGameState>())
	{
		if (ARaidBoss* Boss = RaidGameState->GetRaidBoss())
		{
			return Boss;
		}
	}

	for (TActorIterator<ARaidBoss> It(World); It; ++It)
	{
		return *It;
	}

	return nullptr;
}

void ADrone::ApplyDrainHealForServer(float DamageDealt)
{
	if (!HasAuthority())
	{
		return;
	}

	if (bIsDead)
	{
		LogDeadInputIgnored(TEXT("Heal"));
		return;
	}

	const float SafeDamageDealt = FMath::Max(0.0f, DamageDealt);
	const float RawHealAmount = SafeDamageDealt * 0.12f;
	const float CappedHealAmount = FMath::Min(RawHealAmount, 3.0f);
	const float PreviousHealth = Health;
	Health = FMath::Clamp(Health + CappedHealAmount, 0.0f, static_cast<float>(MaxHealth));
	const float AppliedHealAmount = FMath::Max(0.0f, Health - PreviousHealth);
	const bool bCapped = RawHealAmount > CappedHealAmount + KINDA_SMALL_NUMBER
		|| AppliedHealAmount + KINDA_SMALL_NUMBER < RawHealAmount;

	if (AppliedHealAmount > KINDA_SMALL_NUMBER)
	{
		ForceNetUpdate();
	}

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] DrainHeal PC=%s DamageDealt=%.2f Heal=%.2f HP=%.2f/%d Capped=%s"),
		*BuildDroneControllerLogString(Cast<AController>(GetController())),
		SafeDamageDealt,
		AppliedHealAmount,
		Health,
		MaxHealth,
		bCapped ? TEXT("true") : TEXT("false"));
}

void ADrone::ResetCombatRuntimeStateForReason(FName Reason)
{
	LeftPulseAttackCount = 0;
	RightPulseAttackCount = 0;
	ResetMoveDistanceForServer(Reason);
}

void ADrone::ClearEquippedPartsForServer()
{
	if (!HasAuthority())
	{
		return;
	}

	EquippedParts.Reset();
	EquippedCorePartID = NAME_None;
	EquippedLeftWeaponPartID = NAME_None;
	EquippedRightWeaponPartID = NAME_None;
	AttackPower = 0;
	ForceNetUpdate();
}

void ADrone::LogDeadInputIgnored(const TCHAR* ActionName) const
{
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] DeadInputIgnored PC=%s Action=%s"),
		*BuildDroneControllerLogString(Cast<AController>(GetController())),
		ActionName ? ActionName : TEXT("Unknown"));
}
