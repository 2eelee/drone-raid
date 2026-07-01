#include "Drone.h"
#include "DronePart.h"
#include "DummyParts.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/SceneComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Raid/DronePartInventory.h"
#include "Raid/RaidBoss.h"
#include "Raid/RaidGameMode.h"
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
constexpr float MoveDistanceSummaryLogIntervalSeconds = 1.00f;
constexpr float MoveDistanceIgnoredLogIntervalSeconds = 3.00f;
constexpr float OwnerMoveCorrectionThresholdCm = 5.0f;
constexpr float DefaultBaseMoveSpeedCmPerSecond = 450.0f;
constexpr float CombatCameraSummaryLogIntervalSeconds = 5.00f;
constexpr float MoveAcceptedSummaryLogIntervalSeconds = 1.00f;
constexpr float MoveAcceptedSummaryAxisDeltaThreshold = 0.10f;
constexpr float InputConversionSummaryLogIntervalSeconds = 1.00f;
constexpr float MoveBossMinClampSummaryLogIntervalSeconds = 1.00f;

const TCHAR* ToCombatCoreTypeLogString(EDroneCombatCoreType CoreType)
{
	switch (CoreType)
	{
	case EDroneCombatCoreType::Zenith:
		return TEXT("Zenith");
	case EDroneCombatCoreType::Booster:
		return TEXT("Booster");
	case EDroneCombatCoreType::Drain:
		return TEXT("Drain");
	default:
		return TEXT("None");
	}
}

const TCHAR* ToCombatWeaponTypeLogString(EDroneCombatWeaponType WeaponType)
{
	switch (WeaponType)
	{
	case EDroneCombatWeaponType::PulseLaser:
		return TEXT("PulseLaser");
	case EDroneCombatWeaponType::FractureBurst:
		return TEXT("FractureBurst");
	case EDroneCombatWeaponType::VectorCannon:
		return TEXT("VectorCannon");
	default:
		return TEXT("None");
	}
}

const TCHAR* BuildLocalViewTargetResult(const APlayerController* PC, const APawn* Pawn, const AActor* ViewTarget)
{
	if (!PC || !PC->IsLocalController())
	{
		return TEXT("NotLocalController");
	}

	if (!Pawn)
	{
		return TEXT("PawnNull");
	}

	if (!ViewTarget)
	{
		return TEXT("ViewTargetNull");
	}

	return ViewTarget == Pawn ? TEXT("ViewTargetMatchesPawn") : TEXT("ViewTargetMismatch");
}
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
	if (FloatingMovement)
	{
		FloatingMovement->MaxSpeed = BaseMoveSpeedCmPerSecond;
	}

	MuzzlePoint = CreateDefaultSubobject<USceneComponent>(TEXT("MuzzlePoint"));
	MuzzlePoint->SetupAttachment(RootComponent);
	MuzzlePoint->SetRelativeLocation(FVector(80.f, 0.f, 0.f));

	CombatCameraSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CombatCameraSpringArm"));
	CombatCameraSpringArm->SetupAttachment(RootComponent);
	CombatCameraSpringArm->TargetArmLength = 0.0f;
	CombatCameraSpringArm->bUsePawnControlRotation = false;
	CombatCameraSpringArm->bInheritPitch = false;
	CombatCameraSpringArm->bInheritYaw = false;
	CombatCameraSpringArm->bInheritRoll = false;
	CombatCameraSpringArm->bDoCollisionTest = false;

	CombatCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CombatCamera"));
	CombatCameraComponent->SetupAttachment(CombatCameraSpringArm, USpringArmComponent::SocketName);
	CombatCameraComponent->bUsePawnControlRotation = false;
	CombatCameraComponent->SetFieldOfView(CombatCameraFOV);
}

void ADrone::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		if (BaseMoveSpeedCmPerSecond <= KINDA_SMALL_NUMBER)
		{
			BaseMoveSpeedCmPerSecond = DefaultBaseMoveSpeedCmPerSecond;
		}
		RefreshMoveSpeedForServer();
		FVector SpawnLocation = GetActorLocation();
		LockZPositionForServer(SpawnLocation);
		SetActorLocation(SpawnLocation, false, nullptr, ETeleportType::TeleportPhysics);
		RefreshMovementBoundaryCenterForServer();
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
		ApplyPendingServerMoveInputForServer(DeltaSeconds);
		UpdateMoveDistanceForServer(DeltaSeconds);
	}

	UpdateLocalCombatCamera(DeltaSeconds);
}

void ADrone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bCombatCameraRotationInputDisabled)
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			if (PC->IsLocalController())
			{
				PC->SetIgnoreLookInput(false);
			}
		}
		bCombatCameraRotationInputDisabled = false;
	}

	Super::EndPlay(EndPlayReason);
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
		{
			EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ADrone::Move);
			EIC->BindAction(MoveAction, ETriggerEvent::Completed, this, &ADrone::ClearMoveInputForDodge);
			EIC->BindAction(MoveAction, ETriggerEvent::Canceled, this, &ADrone::ClearMoveInputForDodge);
		}
		if (DodgeAction)
		{
			EIC->BindAction(DodgeAction, ETriggerEvent::Started, this, &ADrone::Dodge);
		}
	}

	PlayerInputComponent->BindKey(EKeys::Z, IE_Pressed, this, &ADrone::RequestAttackBoss);
}

void ADrone::Move(const FInputActionValue& Value)
{
	const FVector2D RawAxis = Value.Get<FVector2D>();
	if (RawAxis.IsNearlyZero())
	{
		ClearCachedMoveInputForDodge();
		return;
	}

	float CameraYaw = 0.0f;
	const FVector2D WorldAxis = ConvertLocalScreenInputToWorldMoveDirection(RawAxis, CameraYaw);
	if (WorldAxis.IsNearlyZero())
	{
		ClearCachedMoveInputForDodge();
		return;
	}

	CachedRawMoveInputForDodge = RawAxis;
	CachedMoveInputCameraYawForDodge = CameraYaw;
	CacheMoveInputForDodge(WorldAxis);
	LogInputConversionSummary(
		TEXT("MoveInputConverted"),
		RawAxis,
		WorldAxis,
		CameraYaw,
		LastMoveInputConvertedLogTime,
		LastMoveInputConvertedRawAxis,
		LastMoveInputConvertedWorldAxis,
		LastMoveInputConvertedCameraYaw);

	if (HasAuthority())
	{
		ApplyMoveInputForServer(WorldAxis);
		return;
	}

	Server_SetMoveInput(WorldAxis);
}

void ADrone::Dodge(const FInputActionValue& Value)
{
	(void)Value;
	if (!CachedMoveInputForDodge.IsNearlyZero())
	{
		LogInputConversionSummary(
			TEXT("DodgeInputConverted"),
			CachedRawMoveInputForDodge,
			CachedMoveInputForDodge,
			CachedMoveInputCameraYawForDodge,
			LastDodgeInputConvertedLogTime,
			LastDodgeInputConvertedRawAxis,
			LastDodgeInputConvertedWorldAxis,
			LastDodgeInputConvertedCameraYaw);
	}
	RequestDodgeFromCurrentMoveInput();
}

void ADrone::ClearMoveInputForDodge(const FInputActionValue& Value)
{
	(void)Value;
	ClearCachedMoveInputForDodge();
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

	if (bIsInvincible)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] DroneDamageIgnored PC=%s Drone=%s Reason=Invincible Damage=%d"),
			*BuildDroneControllerLogString(Cast<AController>(GetController())),
			*GetName(),
			FMath::Max(0, DamageAmount));
		return;
	}

	const int32 AppliedDamage = FMath::Max(0, DamageAmount);
	if (AppliedDamage <= 0)
	{
		return;
	}

	Health = FMath::Clamp(Health - static_cast<float>(AppliedDamage), 0.0f, static_cast<float>(MaxHealth));
	if (bCombatRecordActive)
	{
		CombatRecord.DamageTakenCount++;
	}
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
	const float AppliedHealAmount = FMath::Max(0.0f, Health - PreviousHealth);
	AddHealToCombatRecordForServer(AppliedHealAmount);
	ForceNetUpdate();
	return Health > PreviousHealth + KINDA_SMALL_NUMBER;
}

bool ADrone::RequestDodgeForServer(FVector2D RawDirection)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Dodge Ignored: Reason=NotAuthority Player=%s Drone=%s Direction=%s"),
			*BuildDroneControllerLogString(Cast<AController>(GetController())),
			*GetName(),
			*RawDirection.ToString());
		return false;
	}

	bool bWasClamped = false;
	const FVector2D Direction = ClampMoveInputAxisForServer(RawDirection, bWasClamped);
	const auto LogIgnored = [this, &Direction](const TCHAR* Reason, float CooldownRemaining = 0.0f)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Dodge Ignored: Reason=%s Player=%s Drone=%s Direction=%s CooldownRemaining=%.2f"),
			Reason ? Reason : TEXT("Unknown"),
			*BuildDroneControllerLogString(Cast<AController>(GetController())),
			*GetName(),
			*Direction.ToString(),
			CooldownRemaining);
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Dodge Ignored Reason=%s PC=%s Drone=%s Direction=%s"),
			Reason ? Reason : TEXT("Unknown"),
			*BuildDroneControllerLogString(Cast<AController>(GetController())),
			*GetName(),
			*Direction.ToString());
	};

	if (Direction.IsNearlyZero())
	{
		LogIgnored(TEXT("NoDirection"));
		return false;
	}

	FName IgnoreReason;
	if (!IsDodgeAllowedForServer(Direction, IgnoreReason))
	{
		if (IgnoreReason == FName(TEXT("Dead")))
		{
			LogDeadInputIgnored(TEXT("Dodge"));
		}
		LogIgnored(IgnoreReason.IsNone() ? TEXT("Unknown") : *IgnoreReason.ToString(), GetDodgeCooldownRemaining());
		return false;
	}

	const float SafeDodgeDistanceCm = FMath::Max(0.0f, DodgeDistanceCm);
	if (SafeDodgeDistanceCm <= KINDA_SMALL_NUMBER)
	{
		LogIgnored(TEXT("NoDistance"));
		return false;
	}

	ARaidPlayerController* RaidPC = Cast<ARaidPlayerController>(GetController());
	const FVector DodgeDirection = FVector(Direction.X, Direction.Y, 0.0f).GetSafeNormal();
	if (DodgeDirection.IsNearlyZero())
	{
		LogIgnored(TEXT("NoDirection"));
		return false;
	}

	const FVector StartLocation = GetActorLocation();
	FVector RequestedLocation = StartLocation + DodgeDirection.GetSafeNormal() * SafeDodgeDistanceCm;
	if (!FMath::IsNearlyEqual(RequestedLocation.Z, FixedZPosition, 0.1f))
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Move ZLocked: FixedZ=%.2f Player=%s Before=%s"),
			FixedZPosition,
			*BuildDroneControllerLogString(RaidPC),
			*RequestedLocation.ToString());
	}
	LockZPositionForServer(RequestedLocation);

	const FVector BoundaryInput = RequestedLocation;
	FVector FinalLocation = ClampPositionToMovementBoundaryForServer(BoundaryInput);
	if (!FinalLocation.Equals(BoundaryInput, 0.1f))
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Dodge BoundaryClamp: Before=%s After=%s"),
			*BoundaryInput.ToString(),
			*FinalLocation.ToString());
	}

	const FVector BossMinInput = FinalLocation;
	FinalLocation = ClampPositionOutsideBossCenterForServer(FinalLocation);
	if (!FinalLocation.Equals(BossMinInput, 0.1f))
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Dodge BossMinClamp: Before=%s After=%s MinDistance=5"),
			*BossMinInput.ToString(),
			*FinalLocation.ToString());
	}

	bIsDodging = true;
	bIsInvincible = true;
	LastServerMoveInput = FVector2D::ZeroVector;
	BP_OnDodgeVisualStateChanged(true);

	SetActorLocation(FinalLocation, false, nullptr, ETeleportType::TeleportPhysics);
	const FVector CurrentLocation = GetActorLocation();
	const float ActualDistanceMeters = FVector::Dist2D(StartLocation, CurrentLocation) * MoveDistanceCmToMeters;
	AddDodgeMoveDistanceForServer(ActualDistanceMeters);
	UpdateOwnerMoveSyncForServer(CurrentLocation, GetActorRotation());
	ForceNetUpdate();

	LastMoveDistanceLocation = CurrentLocation;
	bHasMoveDistanceSample = true;

	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(DodgeInvincibleTimerHandle);
		World->GetTimerManager().ClearTimer(DodgeEndTimerHandle);
		World->GetTimerManager().SetTimer(
			DodgeInvincibleTimerHandle,
			this,
			&ADrone::EndDodgeInvincibilityForServer,
			FMath::Max(0.0f, DodgeInvincibleDurationSeconds),
			false);
		World->GetTimerManager().SetTimer(
			DodgeEndTimerHandle,
			this,
			&ADrone::EndDodgeForServer,
			FMath::Max(0.0f, DodgeDurationSeconds),
			false);
	}

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Dodge InvincibleStart Player=%s Duration=%.2f"),
		*BuildDroneControllerLogString(RaidPC),
		FMath::Max(0.0f, DodgeInvincibleDurationSeconds));
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Dodge Accepted: Player=%s Dir=%s Start=%s End=%s ActualDistance=%.2f Invincible=%.2f Duration=%.2f"),
		*BuildDroneControllerLogString(RaidPC),
		*DodgeDirection.GetSafeNormal().ToString(),
		*StartLocation.ToString(),
		*CurrentLocation.ToString(),
		ActualDistanceMeters,
		FMath::Max(0.0f, DodgeInvincibleDurationSeconds),
		FMath::Max(0.0f, DodgeDurationSeconds));

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Dodge PC=%s Direction=%s Result=Success Reason=%s DistanceCm=%.2f Cooldown=%.2f MoveDistancePolicy=Included"),
		*BuildDroneControllerLogString(RaidPC),
		*Direction.ToString(),
		bWasClamped ? TEXT("Clamped") : TEXT("OK"),
		ActualDistanceMeters / MoveDistanceCmToMeters,
		FMath::Max(0.0f, DodgeCooldownSeconds));

	const AActor* ViewTarget = RaidPC->GetViewTarget();
	const APawn* ControlledPawn = RaidPC->GetPawn();
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ServerDodgeApplied PC=%s Drone=%s Pawn=%s bPawnMatchesDrone=%s Direction=%s Delta=%s ServerLocation=%s State=%s ReplicateMovement=%s ViewTarget=%s ViewTargetResult=%s MoveDistancePolicy=Included"),
		*BuildDroneControllerLogString(RaidPC),
		*GetName(),
		ControlledPawn ? *ControlledPawn->GetName() : TEXT("None"),
		ControlledPawn == this ? TEXT("true") : TEXT("false"),
		*Direction.ToString(),
		*(CurrentLocation - StartLocation).ToString(),
		*CurrentLocation.ToString(),
		ARaidPlayerController::SelectionStateToLogString(RaidPC->GetPlayerSelectionState()),
		IsReplicatingMovement() ? TEXT("true") : TEXT("false"),
		ViewTarget ? *ViewTarget->GetName() : TEXT("None"),
		TEXT("ServerObserved"));

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

void ADrone::ClearEquippedLoadoutForServer(FName Reason)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Client] ClearEquippedLoadoutForServer rejected: server authority required"));
		return;
	}

	EquippedParts.Reset();
	EquippedCorePartID = NAME_None;
	EquippedLeftWeaponPartID = NAME_None;
	EquippedRightWeaponPartID = NAME_None;
	AttackPower = 0;
	RefreshMoveSpeedForServer();
	ForceNetUpdate();

	UE_LOG(LogTemp, Log, TEXT("[Server] ClearEquippedLoadout: Drone=%s Reason=%s"),
		*GetName(),
		Reason.IsNone() ? TEXT("Cleanup") : *Reason.ToString());
}

FDroneCombatRecord ADrone::GetCombatRecordForServer() const
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Client] GetCombatRecordForServer rejected: server authority required"));
		return FDroneCombatRecord();
	}

	return BuildCombatRecordSnapshotForServer();
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
	ResetCombatRuntimeStateForReason(FName(TEXT("Ready")));
	RecalculateStats();
	bIsDead = false;
	StartCombatRecordForServer();
	RefreshMoveSpeedForServer();
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

void ADrone::RequestDodge(FVector2D RawDirection)
{
	if (HasAuthority())
	{
		RequestDodgeForServer(RawDirection);
		return;
	}

	Server_RequestDodge(RawDirection);
}

bool ADrone::RequestDodgeFromCurrentMoveInput()
{
	const FVector2D DodgeDirection = CachedMoveInputForDodge;
	ClearCachedMoveInputForDodge();

	if (DodgeDirection.IsNearlyZero())
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] DodgeInputIgnored PC=%s Reason=NoDirection"),
			*BuildDroneControllerLogString(Cast<AController>(GetController())));
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Dodge Ignored: Reason=NoDirection Player=%s"),
			*BuildDroneControllerLogString(Cast<AController>(GetController())));
		return false;
	}

	if (HasAuthority())
	{
		return RequestDodgeForServer(DodgeDirection);
	}

	RequestDodge(DodgeDirection);
	return true;
}

void ADrone::Server_RequestAttackBoss_Implementation()
{
	HandleAttackBossForServer();
}

void ADrone::Server_RequestDodge_Implementation(FVector2D RawDirection)
{
	RequestDodgeForServer(RawDirection);
}

void ADrone::Server_SetMoveInput_Implementation(FVector2D RawAxis)
{
	ApplyMoveInputForServer(RawAxis);
}

bool ADrone::CacheMoveInputForDodge(FVector2D RawAxis)
{
	bool bWasClamped = false;
	const FVector2D Axis = ClampMoveInputAxisForServer(RawAxis, bWasClamped);
	if (Axis.IsNearlyZero())
	{
		CachedMoveInputForDodge = FVector2D::ZeroVector;
		return false;
	}

	CachedMoveInputForDodge = Axis;
	return true;
}

void ADrone::ClearCachedMoveInputForDodge()
{
	CachedMoveInputForDodge = FVector2D::ZeroVector;
	CachedRawMoveInputForDodge = FVector2D::ZeroVector;
	CachedMoveInputCameraYawForDodge = 0.0f;
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

float ADrone::GetAccumulatedMoveDistance() const
{
	return AccumulatedMoveDistanceMeters;
}

void ADrone::ResetAccumulatedMoveDistanceForServer()
{
	ResetMoveDistanceForServer(FName(TEXT("Ready")));
}

float ADrone::GetCurrentMoveSpeed() const
{
	const float MoveSpeedCmPerSecond = FloatingMovement
		? FloatingMovement->MaxSpeed
		: FMath::Max(BaseMoveSpeedCmPerSecond, DefaultBaseMoveSpeedCmPerSecond);
	return FMath::Max(0.0f, MoveSpeedCmPerSecond) * MoveDistanceCmToMeters;
}

bool ADrone::CalculateFixedBossFacingQuarterView(
	const FVector& PlayerPosition,
	const FVector* BossPosition,
	FVector FallbackForwardDirection,
	float CameraDistanceCm,
	float CameraHeightCm,
	float FallbackPitchDegrees,
	FDroneCombatCameraView& OutCameraView)
{
	const bool bBossValid = BossPosition != nullptr;
	FVector CameraForwardDirection = FVector::ZeroVector;
	if (bBossValid)
	{
		CameraForwardDirection = *BossPosition - PlayerPosition;
		CameraForwardDirection.Z = 0.0f;
	}

	if (CameraForwardDirection.IsNearlyZero())
	{
		CameraForwardDirection = FallbackForwardDirection;
		CameraForwardDirection.Z = 0.0f;
	}
	if (CameraForwardDirection.IsNearlyZero())
	{
		CameraForwardDirection = FVector::ForwardVector;
	}
	CameraForwardDirection.Normalize();

	const float SafeDistanceCm = FMath::Max(0.0f, CameraDistanceCm);
	const float SafeHeightCm = FMath::Max(0.0f, CameraHeightCm);
	OutCameraView.CameraLocation =
		PlayerPosition - CameraForwardDirection * SafeDistanceCm + FVector::UpVector * SafeHeightCm;
	OutCameraView.bBossValid = bBossValid;

	if (bBossValid)
	{
		OutCameraView.CameraRotation = (*BossPosition - OutCameraView.CameraLocation).Rotation();
	}
	else
	{
		OutCameraView.CameraRotation = FRotator(
			FallbackPitchDegrees,
			CameraForwardDirection.Rotation().Yaw,
			0.0f);
	}

	return bBossValid;
}

bool ADrone::ShouldApplyFixedBossFacingCamera(
	bool bPawnLocallyControlled,
	bool bControllerIsPlayerController,
	bool bPlayerControllerIsLocal)
{
	return bPawnLocallyControlled && bControllerIsPlayerController && bPlayerControllerIsLocal;
}

FVector2D ADrone::ConvertScreenInputToWorldMoveDirection(FVector2D RawAxis, FRotator CameraRotation)
{
	if (!FMath::IsFinite(RawAxis.X) || !FMath::IsFinite(RawAxis.Y))
	{
		return FVector2D::ZeroVector;
	}

	if (RawAxis.IsNearlyZero())
	{
		return FVector2D::ZeroVector;
	}

	if (RawAxis.SizeSquared() > 1.0f)
	{
		RawAxis.Normalize();
	}

	FVector CameraForward = CameraRotation.Vector();
	CameraForward.Z = 0.0f;
	if (!CameraForward.Normalize())
	{
		CameraForward = FVector::ForwardVector;
	}

	FVector CameraRight = FRotationMatrix(CameraRotation).GetUnitAxis(EAxis::Y);
	CameraRight.Z = 0.0f;
	if (!CameraRight.Normalize())
	{
		CameraRight = FVector::RightVector;
	}

	FVector WorldDirection = CameraForward * RawAxis.Y + CameraRight * RawAxis.X;
	WorldDirection.Z = 0.0f;
	if (!WorldDirection.Normalize())
	{
		return FVector2D::ZeroVector;
	}

	return FVector2D(WorldDirection.X, WorldDirection.Y);
}

bool ADrone::ResolveFixedBossFacingCameraTargetForWorld(UWorld* World, FDroneCombatCameraTarget& OutTarget)
{
	OutTarget = FDroneCombatCameraTarget();
	if (!World)
	{
		OutTarget.InvalidReason = FName(TEXT("NoWorld"));
		return false;
	}

	if (const ARaidGameState* RaidGameState = World->GetGameState<ARaidGameState>())
	{
		if (ARaidBoss* OfficialBoss = RaidGameState->GetRaidBoss())
		{
			OutTarget.Boss = OfficialBoss;
			OutTarget.Source = FName(TEXT("GameState"));
			FName InvalidReason;
			OutTarget.bValid = IsValidFixedBossFacingCameraTarget(OfficialBoss, InvalidReason);
			OutTarget.InvalidReason = OutTarget.bValid ? NAME_None : InvalidReason;
			return OutTarget.bValid;
		}
	}

	ARaidBoss* FirstInvalidBoss = nullptr;
	FName FirstInvalidReason = FName(TEXT("NoBoss"));
	for (TActorIterator<ARaidBoss> It(World); It; ++It)
	{
		ARaidBoss* Candidate = *It;
		FName InvalidReason;
		if (IsValidFixedBossFacingCameraTarget(Candidate, InvalidReason))
		{
			OutTarget.Boss = Candidate;
			OutTarget.Source = FName(TEXT("FallbackSearch"));
			OutTarget.InvalidReason = NAME_None;
			OutTarget.bValid = true;
			return true;
		}

		if (!FirstInvalidBoss)
		{
			FirstInvalidBoss = Candidate;
			FirstInvalidReason = InvalidReason;
		}
	}

	OutTarget.Boss = FirstInvalidBoss;
	OutTarget.Source = FirstInvalidBoss ? FName(TEXT("FallbackSearch")) : NAME_None;
	OutTarget.InvalidReason = FirstInvalidBoss ? FirstInvalidReason : FName(TEXT("NoBoss"));
	OutTarget.bValid = false;
	return false;
}

bool ADrone::IsValidFixedBossFacingCameraTarget(const ARaidBoss* Boss, FName& OutInvalidReason)
{
	OutInvalidReason = NAME_None;
	if (!Boss)
	{
		OutInvalidReason = FName(TEXT("NoBoss"));
		return false;
	}

	if (!IsValid(Boss) || Boss->IsActorBeingDestroyed())
	{
		OutInvalidReason = FName(TEXT("InvalidBoss"));
		return false;
	}

	if (Boss->IsHidden())
	{
		OutInvalidReason = FName(TEXT("HiddenBoss"));
		return false;
	}

	if (!Boss->IsVisualReadyForCamera())
	{
		OutInvalidReason = FName(TEXT("NoVisibleMesh"));
		return false;
	}

	const FVector BossLocation = Boss->GetActorLocation();
	if (BossLocation.ContainsNaN()
		|| !FMath::IsFinite(BossLocation.X)
		|| !FMath::IsFinite(BossLocation.Y)
		|| !FMath::IsFinite(BossLocation.Z))
	{
		OutInvalidReason = FName(TEXT("InvalidBoss"));
		return false;
	}

	return true;
}

FVector ADrone::BuildFixedBossFacingFallbackForward(float FallbackYawDegrees)
{
	return FRotationMatrix(FRotator(0.0f, FallbackYawDegrees, 0.0f)).GetUnitAxis(EAxis::X);
}

float ADrone::ResolveFixedBossFacingFallbackYaw(
	bool& bHasCachedYaw,
	float& InOutCachedYaw,
	float CandidateYawDegrees,
	float DefaultYawDegrees)
{
	if (!bHasCachedYaw)
	{
		InOutCachedYaw = FMath::IsFinite(CandidateYawDegrees)
			? CandidateYawDegrees
			: DefaultYawDegrees;
		bHasCachedYaw = true;
	}

	return InOutCachedYaw;
}

bool ADrone::ShouldEmitThrottledSummaryLog(
	float Now,
	float LastLogTime,
	float MinIntervalSeconds,
	bool bForce)
{
	return bForce
		|| LastLogTime <= -999.0f
		|| Now - LastLogTime >= FMath::Max(0.0f, MinIntervalSeconds);
}
bool ADrone::IsMoveAcceptedSummaryAxisChangeSignificant(
	FVector2D PreviousAxis,
	FVector2D CurrentAxis)
{
	const bool bPreviousZero = PreviousAxis.IsNearlyZero();
	const bool bCurrentZero = CurrentAxis.IsNearlyZero();
	if (bPreviousZero || bCurrentZero)
	{
		return bPreviousZero != bCurrentZero;
	}

	const FVector2D PreviousNormal = PreviousAxis.GetSafeNormal();
	const FVector2D CurrentNormal = CurrentAxis.GetSafeNormal();
	return (PreviousNormal - CurrentNormal).Size() >= MoveAcceptedSummaryAxisDeltaThreshold;
}


bool ADrone::IsDodging() const
{
	return bIsDodging;
}

float ADrone::GetDodgeCooldownRemaining() const
{
	if (LastDodgeEndTime <= -999.0f)
	{
		return 0.0f;
	}

	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : LastDodgeEndTime;
	return FMath::Max(0.0f, FMath::Max(0.0f, DodgeCooldownSeconds) - (Now - LastDodgeEndTime));
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

bool ADrone::ApplyPendingServerMoveInputForTest(float DeltaSeconds)
{
	return ApplyPendingServerMoveInputForServer(DeltaSeconds);
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

FDroneCombatRecord ADrone::GetCombatRecordForTest() const
{
	return BuildCombatRecordSnapshotForServer();
}

FVector2D ADrone::GetLastServerMoveInputForTest() const
{
	return LastServerMoveInput;
}

bool ADrone::CacheMoveInputForDodgeForTest(FVector2D RawAxis)
{
	return CacheMoveInputForDodge(RawAxis);
}

FVector2D ADrone::GetCachedMoveInputForDodgeForTest() const
{
	return CachedMoveInputForDodge;
}

void ADrone::ClearMoveInputForDodgeForTest()
{
	ClearCachedMoveInputForDodge();
}

bool ADrone::RequestDodgeFromCurrentMoveInputForTest()
{
	return RequestDodgeFromCurrentMoveInput();
}

bool ADrone::IsDodgingForTest() const
{
	return bIsDodging;
}

bool ADrone::IsInvincibleForTest() const
{
	return bIsInvincible;
}

void ADrone::SetIsAttackingForTest(bool bInIsAttacking)
{
	bIsAttacking = bInIsAttacking;
}

void ADrone::SetLastDodgeEndTimeForTest(float InLastDodgeEndTime)
{
	LastDodgeEndTime = InLastDodgeEndTime;
}

FName ADrone::GetEquippedCorePartIDForTest() const
{
	return EquippedCorePartID;
}

FName ADrone::GetEquippedLeftWeaponPartIDForTest() const
{
	return EquippedLeftWeaponPartID;
}

FName ADrone::GetEquippedRightWeaponPartIDForTest() const
{
	return EquippedRightWeaponPartID;
}

bool ADrone::HasEquippedLoadoutForTest() const
{
	return !EquippedCorePartID.IsNone()
		|| !EquippedLeftWeaponPartID.IsNone()
		|| !EquippedRightWeaponPartID.IsNone()
		|| EquippedParts.Num() > 0;
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

void ADrone::OnRep_IsDodging()
{
	BP_OnDodgeVisualStateChanged(bIsDodging);
}

void ADrone::OnRep_ReplicatedMovement()
{
	Super::OnRep_ReplicatedMovement();

	if (HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;
	if (Now - LastReplicatedLocationSummaryLogTime < MoveDistanceSummaryLogIntervalSeconds)
	{
		return;
	}

	LastReplicatedLocationSummaryLogTime = Now;
	const bool bLocallyControlled = IsLocallyControlled();
	const APlayerController* PC = bLocallyControlled ? Cast<APlayerController>(GetController()) : nullptr;
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const AActor* ViewTarget = PC ? PC->GetViewTarget() : nullptr;
	const TCHAR* ViewTargetResult = bLocallyControlled
		? BuildLocalViewTargetResult(PC, Pawn, ViewTarget)
		: TEXT("SimProxyObserved");
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReplicatedLocation PC=%s Pawn=%s Location=%s LocalRole=%d ViewTarget=%s ViewTargetResult=%s"),
		*BuildDroneControllerLogString(PC),
		Pawn ? *Pawn->GetName() : TEXT("None"),
		*GetActorLocation().ToString(),
		static_cast<int32>(GetLocalRole()),
		ViewTarget ? *ViewTarget->GetName() : TEXT("None"),
		ViewTargetResult);
}

void ADrone::OnRep_OwnerMoveSync()
{
	if (HasAuthority() || !IsLocallyControlled())
	{
		return;
	}

	const FVector LocalBefore = GetActorLocation();
	const FVector ServerLocation = OwnerMoveSync.Location;
	const float DeltaCm = FVector::Dist(LocalBefore, ServerLocation);
	if (DeltaCm <= OwnerMoveCorrectionThresholdCm)
	{
		return;
	}

	SetActorLocationAndRotation(ServerLocation, OwnerMoveSync.Rotation, false, nullptr, ETeleportType::TeleportPhysics);

	UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;
	if (Now - LastOwnerMoveCorrectedLogTime >= MoveDistanceSummaryLogIntervalSeconds)
	{
		LastOwnerMoveCorrectedLogTime = Now;
		const APlayerController* PC = Cast<APlayerController>(GetController());
		const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		const AActor* ViewTarget = PC ? PC->GetViewTarget() : nullptr;
		const bool bPawnMatchesCorrectedActor = Pawn == this;
		const bool bViewTargetMatchesPawn = Pawn && ViewTarget && ViewTarget == Pawn;
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] OwnerMoveCorrected PC=%s CorrectedActor=%s Pawn=%s ViewTarget=%s bPawnMatchesCorrectedActor=%s bViewTargetMatchesPawn=%s LocalRole=%d RemoteRole=%d LocalBefore=%s ServerLocation=%s Delta=%.2f Sequence=%u ViewTargetResult=%s"),
			*BuildDroneControllerLogString(PC),
			*GetName(),
			Pawn ? *Pawn->GetName() : TEXT("None"),
			ViewTarget ? *ViewTarget->GetName() : TEXT("None"),
			bPawnMatchesCorrectedActor ? TEXT("true") : TEXT("false"),
			bViewTargetMatchesPawn ? TEXT("true") : TEXT("false"),
			static_cast<int32>(GetLocalRole()),
			static_cast<int32>(GetRemoteRole()),
			*LocalBefore.ToString(),
			*ServerLocation.ToString(),
			DeltaCm,
			OwnerMoveSync.Sequence,
			BuildLocalViewTargetResult(PC, Pawn, ViewTarget));
	}
}

void ADrone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADrone, Health);
	DOREPLIFETIME(ADrone, MaxHealth);
	DOREPLIFETIME(ADrone, AttackPower);
	DOREPLIFETIME(ADrone, bIsDead);
	DOREPLIFETIME(ADrone, bIsDodging);
	DOREPLIFETIME(ADrone, LastDodgeEndTime);
	DOREPLIFETIME_CONDITION(ADrone, OwnerMoveSync, COND_OwnerOnly);
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
		RaidPC->TryCreateDroneReportForServer(EDroneReportTrigger::Death, false);
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReturnAfterReport Player=%s Trigger=%s"),
			*BuildDroneControllerLogString(RaidPC),
			TEXT("Death"));
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

	ClearEquippedLoadoutForServer(FName(TEXT("Death")));

	// TODO(D5 UI): show drone death state when the combat death UI flow exists.
	UE_LOG(LogTemp, Log, TEXT("[Server] Drone death handled: equipped parts returned"));
}

void ADrone::HandleAttackBossForServer()
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Attack Failed: Reason=NotAuthority Player=%s Drone=%s"),
			*BuildDroneControllerLogString(Cast<AController>(GetController())),
			*GetName());
		return;
	}

	ARaidPlayerController* RaidPC = Cast<ARaidPlayerController>(GetController());
	if (!RaidPC || RaidPC->GetPawn() != this)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] AttackIgnored PC=%s Reason=PossessMismatch Drone=%s"),
			*BuildDroneControllerLogString(Cast<AController>(GetController())),
			*GetName());
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Attack Failed: Reason=InvalidPawn Player=%s Drone=%s CurrentPawn=%s"),
			*BuildDroneControllerLogString(Cast<AController>(GetController())),
			*GetName(),
			RaidPC && RaidPC->GetPawn() ? *RaidPC->GetPawn()->GetName() : TEXT("None"));
		return;
	}

	if (bIsDodging)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Attack Ignored: Reason=Dodging Player=%s Drone=%s"),
			*BuildDroneControllerLogString(RaidPC),
			*GetName());
		return;
	}

	if (const ARaidGameState* RaidGameState = GetWorld() ? GetWorld()->GetGameState<ARaidGameState>() : nullptr)
	{
		if (RaidGameState->RaidState == ERaidState::End)
		{
			UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Attack Ignored: Reason=RaidEnd Player=%s Drone=%s RaidState=End"),
				*BuildDroneControllerLogString(RaidPC),
				*GetName());
			return;
		}
	}

	if (RaidPC && RaidPC->GetPlayerSelectionState() != EPlayerSelectionState::InBattle)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] AttackIgnored PC=%s Reason=NotInBattle SelectionState=%s"),
			*BuildDroneControllerLogString(RaidPC),
			ARaidPlayerController::SelectionStateToLogString(RaidPC->GetPlayerSelectionState()));
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Attack Ignored: Reason=NotInBattle Player=%s SelectionState=%s"),
			*BuildDroneControllerLogString(RaidPC),
			ARaidPlayerController::SelectionStateToLogString(RaidPC->GetPlayerSelectionState()));
		return;
	}

	if (bIsDead)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] AttackIgnored PC=%s Reason=Dead Drone=%s"),
			*BuildDroneControllerLogString(RaidPC),
			*GetName());
		LogDeadInputIgnored(TEXT("Attack"));
		return;
	}

	ARaidBoss* Boss = FindRaidBossForServer();
	if (!Boss)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] AttackIgnored PC=%s Reason=NoBoss Drone=%s"),
			*BuildDroneControllerLogString(Cast<AController>(GetController())),
			*GetName());
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Attack Failed: Reason=NoBoss Player=%s Drone=%s"),
			*BuildDroneControllerLogString(Cast<AController>(GetController())),
			*GetName());
		return;
	}

	if (Boss->IsDefeated())
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Attack Ignored: Reason=BossDead Player=%s Drone=%s BossHP=%.2f"),
			*BuildDroneControllerLogString(Cast<AController>(GetController())),
			*GetName(),
			Boss->GetCurrentHP());
		return;
	}

	struct FScopedDroneAttackFlag
	{
		explicit FScopedDroneAttackFlag(ADrone* InDrone)
			: Drone(InDrone)
		{
			if (Drone)
			{
				Drone->bIsAttacking = true;
			}
		}

		~FScopedDroneAttackFlag()
		{
			if (Drone)
			{
				Drone->bIsAttacking = false;
			}
		}

		ADrone* Drone = nullptr;
	};
	FScopedDroneAttackFlag ScopedAttackFlag(this);

	const FDroneWeaponCalculationResult LeftWeaponResult = CalculateWeaponDamageForServer(EquippedLeftWeaponPartID, true);
	const FDroneWeaponCalculationResult RightWeaponResult = CalculateWeaponDamageForServer(EquippedRightWeaponPartID, false);
	const float LeftWeaponDamage = LeftWeaponResult.WeaponDamage;
	const float RightWeaponDamage = RightWeaponResult.WeaponDamage;
	const float TotalWeaponDamage = LeftWeaponDamage + RightWeaponDamage;
	const FDroneCoreCalculationResult CoreResult = CalculateCoreForServer(EquippedCorePartID);
	const float FinalDamage = TotalWeaponDamage * CoreResult.CoreAttackModifier * CoreResult.CoreBonusAttackModifier;

	const float BossHPBeforeAttack = Boss->GetCurrentHP();
	Boss->ApplyDamageForServer(FinalDamage, Cast<AController>(GetController()), this);
	const float DamageDealt = FMath::Max(0.0f, BossHPBeforeAttack - Boss->GetCurrentHP());
	const bool bBossDefeatedByThisAttack = BossHPBeforeAttack > 0.0f && Boss->GetCurrentHP() <= 0.0f;
	AddBossDamageToCombatRecordForServer(DamageDealt, Boss);
	float HealAmount = 0.0f;
	if (EquippedCorePartID == ADronePartInventory::GetCoreDrainPartID())
	{
		HealAmount = ApplyDrainHealForServer(DamageDealt);
	}
	if (LeftWeaponResult.bResetVectorDistance || RightWeaponResult.bResetVectorDistance)
	{
		ResetVectorMoveDistanceForServer(FName(TEXT("VectorAttack")));
	}

	UE_LOG(LogTemp, Log, TEXT("[Server] Drone ZAttack: Drone=%s CorePartID=%s LeftWeaponPartID=%s RightWeaponPartID=%s LeftDamage=%.2f RightDamage=%.2f TotalWeaponDamage=%.2f CoreAttackModifier=%.2f CoreBonusAttackModifier=%.2f FinalDamage=%.2f BossHP=%.2f/%.2f"),
		*GetName(),
		*EquippedCorePartID.ToString(),
		*EquippedLeftWeaponPartID.ToString(),
		*EquippedRightWeaponPartID.ToString(),
		LeftWeaponDamage,
		RightWeaponDamage,
		TotalWeaponDamage,
		CoreResult.CoreAttackModifier,
		CoreResult.CoreBonusAttackModifier,
		FinalDamage,
		Boss->GetCurrentHP(),
		Boss->GetMaxHP());

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] AttackCalc Player=%s Core=%s LeftWeapon=%s RightWeapon=%s LeftDamage=%.2f RightDamage=%.2f TotalWeaponDamage=%.2f CoreModifier=%.2f CoreBonusAttackModifier=%.2f FinalDamage=%.2f DamageDealt=%.2f HealAmount=%.2f BossHP=%.2f/%.2f"),
		*BuildDroneControllerLogString(Cast<AController>(GetController())),
		*EquippedCorePartID.ToString(),
		*EquippedLeftWeaponPartID.ToString(),
		*EquippedRightWeaponPartID.ToString(),
		LeftWeaponDamage,
		RightWeaponDamage,
		TotalWeaponDamage,
		CoreResult.CoreAttackModifier,
		CoreResult.CoreBonusAttackModifier,
		FinalDamage,
		DamageDealt,
		HealAmount,
		Boss->GetCurrentHP(),
		Boss->GetMaxHP());
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Attack Accepted: Player=%s Core=%s LeftWeapon=%s RightWeapon=%s Damage=%.2f LeftDamage=%.2f RightDamage=%.2f BossHPBefore=%.2f BossHPAfter=%.2f"),
		*BuildDroneControllerLogString(Cast<AController>(GetController())),
		*EquippedCorePartID.ToString(),
		*EquippedLeftWeaponPartID.ToString(),
		*EquippedRightWeaponPartID.ToString(),
		FinalDamage,
		LeftWeaponDamage,
		RightWeaponDamage,
		BossHPBeforeAttack,
		Boss->GetCurrentHP());
	LogCombatRecordForServer();

	if (bBossDefeatedByThisAttack)
	{
		if (ARaidGameMode* RaidGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ARaidGameMode>() : nullptr)
		{
			RaidGameMode->HandleBossDefeatedForServer();
		}
	}
}

void ADrone::UpdateLocalCombatCamera(float DeltaSeconds)
{
	(void)DeltaSeconds;

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!ShouldApplyFixedBossFacingCamera(IsLocallyControlled(), PC != nullptr, PC ? PC->IsLocalController() : false))
	{
		return;
	}

	DisableLocalCombatCameraRotationInput(PC);
	if (!CombatCameraComponent)
	{
		return;
	}

	if (PC->GetViewTarget() != this)
	{
		PC->SetViewTarget(this);
	}

	FDroneCombatCameraTarget CameraTarget;
	ResolveFixedBossFacingCameraTargetForWorld(GetWorld(), CameraTarget);
	ARaidBoss* Boss = CameraTarget.bValid ? CameraTarget.Boss.Get() : nullptr;
	FVector BossLocation = FVector::ZeroVector;
	const FVector* BossLocationPtr = nullptr;
	if (Boss)
	{
		BossLocation = Boss->GetActorLocation();
		BossLocationPtr = &BossLocation;
	}

	const float FixedFallbackYaw = ResolveFixedBossFacingFallbackYaw(
		bHasCombatCameraFallbackYaw,
		CombatCameraFallbackYawDegrees,
		PC ? PC->GetControlRotation().Yaw : GetActorRotation().Yaw,
		CombatCameraDefaultFallbackYawDegrees);

	FDroneCombatCameraView CameraView;
	const bool bBossValid = CalculateFixedBossFacingQuarterView(
		GetActorLocation(),
		BossLocationPtr,
		BuildFixedBossFacingFallbackForward(FixedFallbackYaw),
		CombatCameraDistanceCm,
		CombatCameraHeightCm,
		CombatCameraFallbackPitchDegrees,
		CameraView);

	if (CombatCameraSpringArm)
	{
		CombatCameraSpringArm->TargetArmLength = 0.0f;
		CombatCameraSpringArm->SocketOffset = CombatCameraSocketOffsetCm;
		CombatCameraSpringArm->SetWorldLocationAndRotation(CameraView.CameraLocation, CameraView.CameraRotation);
		CombatCameraComponent->SetRelativeLocation(FVector::ZeroVector);
		CombatCameraComponent->SetRelativeRotation(FRotator::ZeroRotator);
	}
	else
	{
		CombatCameraComponent->SetWorldLocationAndRotation(
			CameraView.CameraLocation + CombatCameraSocketOffsetCm,
			CameraView.CameraRotation);
	}
	CombatCameraComponent->SetFieldOfView(CombatCameraFOV);
	CombatCameraComponent->SetActive(true);

	UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;
	const bool bTargetChanged = !bHasLastCombatCameraTargetSummary
		|| LastCombatCameraTargetSummaryBoss.Get() != CameraTarget.Boss.Get()
		|| LastCombatCameraTargetSummarySource != CameraTarget.Source
		|| LastCombatCameraTargetSummaryReason != CameraTarget.InvalidReason;
	if (bTargetChanged)
	{
		LastCombatCameraTargetSummaryBoss = CameraTarget.Boss.Get();
		LastCombatCameraTargetSummarySource = CameraTarget.Source;
		LastCombatCameraTargetSummaryReason = CameraTarget.InvalidReason;
		bHasLastCombatCameraTargetSummary = true;

		if (ARaidBoss* TargetBoss = CameraTarget.Boss.Get())
		{
			TArray<UPrimitiveComponent*> PrimitiveComponents;
			TargetBoss->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
			bool bHasVisiblePrimitive = false;
			bool bHasVisibleMesh = false;
			for (const UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
			{
				if (PrimitiveComponent && PrimitiveComponent->IsVisible())
				{
					bHasVisiblePrimitive = true;
					if (const UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(PrimitiveComponent))
					{
						bHasVisibleMesh = StaticMeshComponent->GetStaticMesh() != nullptr;
					}

					if (bHasVisibleMesh)
					{
						break;
					}
				}
			}

			const UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(TargetBoss->GetRootComponent());
			const bool bRootVisible = RootPrimitive && RootPrimitive->IsVisible();
			const bool bVisualReady = TargetBoss->IsVisualReadyForCamera();
			const bool bVisibleForCameraLog = !TargetBoss->IsHidden() && bHasVisiblePrimitive;
			UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Camera BossTarget: Source=%s BossName=%s BossClass=%s Location=%s Hidden=%s Visible=%s RootVisible=%s MeshVisible=%s VisualReady=%s TickEnabled=%s Valid=%s Reason=%s"),
				CameraTarget.Source.IsNone() ? TEXT("None") : *CameraTarget.Source.ToString(),
				*TargetBoss->GetName(),
				TargetBoss->GetClass() ? *TargetBoss->GetClass()->GetName() : TEXT("None"),
				*TargetBoss->GetActorLocation().ToString(),
				TargetBoss->IsHidden() ? TEXT("True") : TEXT("False"),
				bVisibleForCameraLog ? TEXT("True") : TEXT("False"),
				RootPrimitive ? (bRootVisible ? TEXT("True") : TEXT("False")) : TEXT("Unknown"),
				bHasVisibleMesh ? TEXT("True") : TEXT("False"),
				bVisualReady ? TEXT("True") : TEXT("False"),
				TargetBoss->IsActorTickEnabled() ? TEXT("True") : TEXT("False"),
				CameraTarget.bValid ? TEXT("True") : TEXT("False"),
				CameraTarget.InvalidReason.IsNone() ? TEXT("None") : *CameraTarget.InvalidReason.ToString());

			if (!bHasVisibleMesh)
			{
				UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Boss VisualWarning: Boss=%s Reason=NoVisibleMesh VisualReady=%s"),
					*TargetBoss->GetName(),
					bVisualReady ? TEXT("True") : TEXT("False"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Camera BossTarget: Source=%s BossName=None BossClass=None Location=None Hidden=False Visible=False RootVisible=Unknown MeshVisible=False VisualReady=False TickEnabled=False Valid=False Reason=%s"),
				CameraTarget.Source.IsNone() ? TEXT("None") : *CameraTarget.Source.ToString(),
				CameraTarget.InvalidReason.IsNone() ? TEXT("Unknown") : *CameraTarget.InvalidReason.ToString());
		}

		if (!CameraTarget.bValid)
		{
			UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Camera Fallback: Reason=%s LocalPlayer=%s Source=%s FixedYaw=%.2f"),
				CameraTarget.InvalidReason.IsNone() ? TEXT("Unknown") : *CameraTarget.InvalidReason.ToString(),
				*BuildDroneControllerLogString(PC),
				CameraTarget.Source.IsNone() ? TEXT("None") : *CameraTarget.Source.ToString(),
				FixedFallbackYaw);
		}
	}

	const bool bAppliedConfigChanged = !bHasLastCombatCameraAppliedConfig
		|| !FMath::IsNearlyEqual(LastCombatCameraAppliedDistanceCm, CombatCameraDistanceCm, 0.01f)
		|| !FMath::IsNearlyEqual(LastCombatCameraAppliedHeightCm, CombatCameraHeightCm, 0.01f)
		|| !FMath::IsNearlyEqual(LastCombatCameraAppliedFOV, CombatCameraFOV, 0.01f)
		|| !LastCombatCameraAppliedSocketOffsetCm.Equals(CombatCameraSocketOffsetCm, 0.01f)
		|| !LastCombatCameraAppliedTargetScreenPosition.Equals(CombatCameraTargetScreenPosition, 0.001f)
		|| !LastCombatCameraAppliedBossScreenYRange.Equals(CombatCameraBossScreenYRange, 0.001f);
	if (ShouldEmitThrottledSummaryLog(
		Now,
		LastCombatCameraAppliedSummaryLogTime,
		CombatCameraSummaryLogIntervalSeconds,
		bAppliedConfigChanged))
	{
		LastCombatCameraAppliedSummaryLogTime = Now;
		bHasLastCombatCameraAppliedConfig = true;
		LastCombatCameraAppliedDistanceCm = CombatCameraDistanceCm;
		LastCombatCameraAppliedHeightCm = CombatCameraHeightCm;
		LastCombatCameraAppliedFOV = CombatCameraFOV;
		LastCombatCameraAppliedSocketOffsetCm = CombatCameraSocketOffsetCm;
		LastCombatCameraAppliedTargetScreenPosition = CombatCameraTargetScreenPosition;
		LastCombatCameraAppliedBossScreenYRange = CombatCameraBossScreenYRange;
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Camera Applied: LocalPlayer=%s Distance=%.0f Height=%.1f FOV=%.0f TargetScreen=(%.2f,%.2f) BossScreenY=(%.2f,%.2f)"),
			*BuildDroneControllerLogString(PC),
			CombatCameraDistanceCm * MoveDistanceCmToMeters,
			CombatCameraHeightCm * MoveDistanceCmToMeters,
			CombatCameraFOV,
			CombatCameraTargetScreenPosition.X,
			CombatCameraTargetScreenPosition.Y,
			CombatCameraBossScreenYRange.X,
			CombatCameraBossScreenYRange.Y);
	}

	const bool bBossValidityChanged = !bHasLastCombatCameraBossValid || bLastCombatCameraBossValid != bBossValid;
	if (ShouldEmitThrottledSummaryLog(
		Now,
		LastCombatCameraBossFacingSummaryLogTime,
		CombatCameraSummaryLogIntervalSeconds,
		bBossValidityChanged || bTargetChanged))
	{
		LastCombatCameraBossFacingSummaryLogTime = Now;
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Camera BossFacing: BossValid=%s LocalPlayer=%s CameraLocation=%s CameraRotation=%s"),
			bBossValid ? TEXT("True") : TEXT("False"),
			*BuildDroneControllerLogString(PC),
			*CameraView.CameraLocation.ToString(),
			*CameraView.CameraRotation.ToString());
	}
	bHasLastCombatCameraBossValid = true;
	bLastCombatCameraBossValid = bBossValid;
}

void ADrone::DisableLocalCombatCameraRotationInput(APlayerController* PC)
{
	if (!PC || bCombatCameraRotationInputDisabled)
	{
		return;
	}

	PC->SetIgnoreLookInput(true);
	bCombatCameraRotationInputDisabled = true;
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Camera RotationInputDisabled LocalPlayer=%s"),
		*BuildDroneControllerLogString(PC));
}

ARaidBoss* ADrone::FindRaidBossForLocalCamera() const
{
	const UWorld* World = GetWorld();
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
		if (*It)
		{
			return *It;
		}
	}

	return nullptr;
}

FRotator ADrone::ResolveLocalCameraRelativeInputRotation() const
{
	if (CombatCameraComponent && CombatCameraComponent->IsActive())
	{
		return CombatCameraComponent->GetComponentRotation();
	}

	if (CombatCameraSpringArm)
	{
		return CombatCameraSpringArm->GetComponentRotation();
	}

	if (bHasCombatCameraFallbackYaw)
	{
		return FRotator(CombatCameraFallbackPitchDegrees, CombatCameraFallbackYawDegrees, 0.0f);
	}

	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		return PC->GetControlRotation();
	}

	return GetActorRotation();
}

FVector2D ADrone::ConvertLocalScreenInputToWorldMoveDirection(FVector2D RawAxis, float& OutCameraYaw) const
{
	const FRotator CameraRotation = ResolveLocalCameraRelativeInputRotation();
	OutCameraYaw = CameraRotation.Yaw;
	return ConvertScreenInputToWorldMoveDirection(RawAxis, CameraRotation);
}

void ADrone::LogInputConversionSummary(
	const TCHAR* LogName,
	const FVector2D& RawAxis,
	const FVector2D& WorldAxis,
	float CameraYaw,
	float& InOutLastLogTime,
	FVector2D& InOutLastRawAxis,
	FVector2D& InOutLastWorldAxis,
	float& InOutLastCameraYaw)
{
	UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;
	const bool bRawAxisChanged = !InOutLastRawAxis.Equals(RawAxis, 0.001f);

	if (!ShouldEmitThrottledSummaryLog(
		Now,
		InOutLastLogTime,
		InputConversionSummaryLogIntervalSeconds,
		bRawAxisChanged))
	{
		return;
	}

	InOutLastLogTime = Now;
	InOutLastRawAxis = RawAxis;
	InOutLastWorldAxis = WorldAxis;
	InOutLastCameraYaw = CameraYaw;
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] %s PC=%s RawAxis=%s CameraYaw=%.2f WorldAxis=%s"),
		LogName ? LogName : TEXT("InputConverted"),
		*BuildDroneControllerLogString(Cast<AController>(GetController())),
		*RawAxis.ToString(),
		CameraYaw,
		*WorldAxis.ToString());
}

bool ADrone::IsMovementAllowedForServer(FName& OutIgnoreReason) const
{
	OutIgnoreReason = NAME_None;
	if (!HasAuthority())
	{
		OutIgnoreReason = FName(TEXT("NotAuthority"));
		return false;
	}

	const ARaidPlayerController* RaidPC = Cast<ARaidPlayerController>(GetController());
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

	if (RaidPC->HasDroneReportGenerated())
	{
		OutIgnoreReason = FName(TEXT("Report"));
		return false;
	}

	if (RaidPC->GetPlayerSelectionState() != EPlayerSelectionState::InBattle)
	{
		OutIgnoreReason = FName(TEXT("Selecting"));
		return false;
	}

	const UWorld* World = GetWorld();
	const ARaidGameState* RaidGameState = World ? World->GetGameState<ARaidGameState>() : nullptr;
	if (!RaidGameState)
	{
		OutIgnoreReason = FName(TEXT("ServerLoading"));
		return false;
	}

	if (RaidGameState->RaidState == ERaidState::End)
	{
		OutIgnoreReason = FName(TEXT("RaidEnd"));
		return false;
	}

	if (RaidGameState->RaidState != ERaidState::Battle)
	{
		OutIgnoreReason = FName(TEXT("ServerLoading"));
		return false;
	}

	if (bIsDodging)
	{
		OutIgnoreReason = FName(TEXT("Dodging"));
		return false;
	}

	return true;
}

void ADrone::CancelDodgeForServer(FName Reason)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Client] CancelDodgeForServer rejected: server authority required"));
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DodgeInvincibleTimerHandle);
		World->GetTimerManager().ClearTimer(DodgeEndTimerHandle);
	}

	const bool bHadDodgeState = bIsDodging || bIsInvincible;
	bIsDodging = false;
	bIsInvincible = false;
	LastServerMoveInput = FVector2D::ZeroVector;
	if (bHadDodgeState)
	{
		BP_OnDodgeVisualStateChanged(false);
		ForceNetUpdate();
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Dodge Cleared: Reason=%s Player=%s"),
			Reason.IsNone() ? TEXT("Unknown") : *Reason.ToString(),
			*BuildDroneControllerLogString(Cast<AController>(GetController())));
	}
}

void ADrone::LockZPositionForServer(FVector& Position) const
{
	Position.Z = FixedZPosition;
}

FVector ADrone::ResolveMovementBoundaryCenterForServer() const
{
	if (bHasMovementBoundaryCenter)
	{
		FVector Center = MovementBoundaryCenter;
		Center.Z = FixedZPosition;
		return Center;
	}

	if (const ARaidBoss* Boss = FindRaidBossForServer())
	{
		FVector Center = Boss->GetActorLocation();
		Center.Z = FixedZPosition;
		return Center;
	}

	FVector Center = GetActorLocation();
	Center.Z = FixedZPosition;
	return Center;
}

void ADrone::RefreshMovementBoundaryCenterForServer()
{
	if (!HasAuthority())
	{
		return;
	}

	if (const ARaidBoss* Boss = FindRaidBossForServer())
	{
		MovementBoundaryCenter = Boss->GetActorLocation();
	}
	else
	{
		MovementBoundaryCenter = GetActorLocation();
	}
	MovementBoundaryCenter.Z = FixedZPosition;
	bHasMovementBoundaryCenter = true;
}

FVector ADrone::ClampPositionToMovementBoundaryForServer(FVector RequestedPosition)
{
	LockZPositionForServer(RequestedPosition);

	const float SafeRadiusCm = FMath::Max(0.0f, MovementBoundaryRadiusCm);
	if (SafeRadiusCm <= KINDA_SMALL_NUMBER)
	{
		return RequestedPosition;
	}

	const FVector Center = ResolveMovementBoundaryCenterForServer();
	FVector Offset = RequestedPosition - Center;
	Offset.Z = 0.0f;
	const float DistanceCm = Offset.Size();
	if (DistanceCm <= SafeRadiusCm + KINDA_SMALL_NUMBER)
	{
		return RequestedPosition;
	}

	FVector ClampedPosition = Center + (Offset / DistanceCm) * SafeRadiusCm;
	LockZPositionForServer(ClampedPosition);
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Move BoundaryClamp: Before=%s After=%s Radius=50 Player=%s"),
		*RequestedPosition.ToString(),
		*ClampedPosition.ToString(),
		*BuildDroneControllerLogString(Cast<AController>(GetController())));
	return ClampedPosition;
}

FVector ADrone::ClampPositionOutsideBossCenterForServer(FVector RequestedPosition)
{
	LockZPositionForServer(RequestedPosition);

	const ARaidBoss* Boss = FindRaidBossForServer();
	if (!Boss)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Move Boundary: Reason=NoBoss Player=%s Position=%s"),
			*BuildDroneControllerLogString(Cast<AController>(GetController())),
			*RequestedPosition.ToString());
		return RequestedPosition;
	}

	const float SafeMinDistanceCm = FMath::Max(0.0f, BossMinApproachDistanceCm);
	if (SafeMinDistanceCm <= KINDA_SMALL_NUMBER)
	{
		return RequestedPosition;
	}

	FVector BossLocation = Boss->GetActorLocation();
	BossLocation.Z = FixedZPosition;
	FVector Offset = RequestedPosition - BossLocation;
	Offset.Z = 0.0f;
	const float DistanceCm = Offset.Size();
	if (DistanceCm >= SafeMinDistanceCm - KINDA_SMALL_NUMBER)
	{
		return RequestedPosition;
	}

	FVector PushDirection = Offset.GetSafeNormal();
	if (PushDirection.IsNearlyZero())
	{
		PushDirection = (GetActorLocation() - BossLocation).GetSafeNormal();
		PushDirection.Z = 0.0f;
	}
	if (PushDirection.IsNearlyZero())
	{
		PushDirection = FVector::ForwardVector;
	}

	FVector ClampedPosition = BossLocation + PushDirection.GetSafeNormal() * SafeMinDistanceCm;
	LockZPositionForServer(ClampedPosition);
	UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;
	if (ShouldEmitThrottledSummaryLog(
		Now,
		LastMoveBossMinClampSummaryLogTime,
		MoveBossMinClampSummaryLogIntervalSeconds,
		false))
	{
		LastMoveBossMinClampSummaryLogTime = Now;
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Move BossMinClamp: Before=%s After=%s MinDistance=5 Player=%s"),
			*RequestedPosition.ToString(),
			*ClampedPosition.ToString(),
			*BuildDroneControllerLogString(Cast<AController>(GetController())));
	}
	return ClampedPosition;
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

bool ADrone::IsDodgeAllowedForServer(const FVector2D& Direction, FName& OutIgnoreReason) const
{
	OutIgnoreReason = NAME_None;
	if (!HasAuthority())
	{
		OutIgnoreReason = FName(TEXT("NotAuthority"));
		return false;
	}

	const ARaidPlayerController* RaidPC = Cast<ARaidPlayerController>(GetController());
	if (!RaidPC || RaidPC->GetPawn() != this)
	{
		OutIgnoreReason = FName(TEXT("NoPawn"));
		return false;
	}

	if (Direction.IsNearlyZero())
	{
		OutIgnoreReason = FName(TEXT("NoDirection"));
		return false;
	}

	if (bIsDead)
	{
		OutIgnoreReason = FName(TEXT("Dead"));
		return false;
	}

	if (RaidPC->HasDroneReportGenerated())
	{
		OutIgnoreReason = FName(TEXT("Report"));
		return false;
	}

	if (bIsDodging)
	{
		OutIgnoreReason = FName(TEXT("AlreadyDodging"));
		return false;
	}

	if (bIsAttacking)
	{
		OutIgnoreReason = FName(TEXT("Attacking"));
		return false;
	}

	if (RaidPC->GetPlayerSelectionState() != EPlayerSelectionState::InBattle)
	{
		OutIgnoreReason = FName(TEXT("Selecting"));
		return false;
	}

	const UWorld* World = GetWorld();
	const ARaidGameState* RaidGameState = World ? World->GetGameState<ARaidGameState>() : nullptr;
	if (!RaidGameState || RaidGameState->RaidState != ERaidState::Battle)
	{
		OutIgnoreReason = FName(TEXT("ServerLoading"));
		return false;
	}

	if (GetDodgeCooldownRemaining() > KINDA_SMALL_NUMBER)
	{
		OutIgnoreReason = FName(TEXT("Cooldown"));
		return false;
	}

	return true;
}

void ADrone::AddDodgeMoveDistanceForServer(float DeltaMeters)
{
	if (!HasAuthority())
	{
		return;
	}

	const float SafeDeltaMeters = FMath::Max(0.0f, DeltaMeters);
	if (SafeDeltaMeters <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	VectorAccumulatedMoveDistanceMeters += SafeDeltaMeters;
	BoosterAccumulatedMoveDistanceMeters += SafeDeltaMeters;
	AccumulatedMoveDistanceMeters += SafeDeltaMeters;
	AddMoveDistanceToCombatRecordForServer(SafeDeltaMeters);
	RefreshMoveSpeedForServer();
}

void ADrone::EndDodgeInvincibilityForServer()
{
	if (!HasAuthority())
	{
		return;
	}

	if (!bIsInvincible)
	{
		return;
	}

	bIsInvincible = false;
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Dodge InvincibleEnd Player=%s"),
		*BuildDroneControllerLogString(Cast<AController>(GetController())));
}

void ADrone::EndDodgeForServer()
{
	if (!HasAuthority())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DodgeEndTimerHandle);
		LastDodgeEndTime = World->GetTimeSeconds();
	}
	else
	{
		LastDodgeEndTime = 0.0f;
	}

	if (bIsInvincible)
	{
		EndDodgeInvincibilityForServer();
	}

	if (bIsDodging)
	{
		bIsDodging = false;
		BP_OnDodgeVisualStateChanged(false);
	}

	LastServerMoveInput = FVector2D::ZeroVector;
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Dodge End: LastDodgeEndTime=%.2f Player=%s"),
		LastDodgeEndTime,
		*BuildDroneControllerLogString(Cast<AController>(GetController())));
}

bool ADrone::ApplyMoveInputForServer(FVector2D RawAxis)
{
	if (!HasAuthority())
		bMoveAcceptedSummaryInputActive = false;
	{
		LogMoveInputSummary(TEXT("Ignored"), TEXT("NotAuthority"), RawAxis);
		return false;
	}

	bool bWasClamped = false;
	const FVector2D Axis = ClampMoveInputAxisForServer(RawAxis, bWasClamped);
	LastServerMoveInput = FVector2D::ZeroVector;

	FName IgnoreReason;
	if (!IsMovementAllowedForServer(IgnoreReason))
	{
		if (IgnoreReason == FName(TEXT("Dead")))
		{
			LogDeadInputIgnored(TEXT("Move"));
		bMoveAcceptedSummaryInputActive = false;
		}
		LogMoveInputSummary(TEXT("Ignored"), IgnoreReason.IsNone() ? TEXT("Unknown") : *IgnoreReason.ToString(), Axis);
		return false;
	}

	if (Axis.IsNearlyZero())
		bMoveAcceptedSummaryInputActive = false;
	{
		LogMoveInputSummary(TEXT("Ignored"), TEXT("ZeroAxis"), Axis);
		return false;
	}

	LastServerMoveInput = Axis;

	LogMoveInputSummary(TEXT("Accepted"), bWasClamped ? TEXT("Clamped") : TEXT("OK"), Axis);
	return true;
}

bool ADrone::ApplyPendingServerMoveInputForServer(float DeltaSeconds)
{
	if (!HasAuthority())
	{
		return false;
	}

	const FVector2D Axis = LastServerMoveInput;
	LastServerMoveInput = FVector2D::ZeroVector;

	if (Axis.IsNearlyZero() || DeltaSeconds <= 0.0f)
	{
		return false;
	}

	ARaidPlayerController* RaidPC = Cast<ARaidPlayerController>(GetController());
	FName IgnoreReason;
	if (!IsMovementAllowedForServer(IgnoreReason))
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ServerMoveIgnored PC=%s Reason=%s Axis=%s"),
			*BuildDroneControllerLogString(RaidPC),
			IgnoreReason.IsNone() ? TEXT("Unknown") : *IgnoreReason.ToString(),
			*Axis.ToString());
		return false;
	}

	const float MoveSpeedCmPerSecond = FMath::Max(0.0f, GetCurrentMoveSpeed() / MoveDistanceCmToMeters);
	if (MoveSpeedCmPerSecond <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FVector MoveDirection = FVector(Axis.X, Axis.Y, 0.0f).GetSafeNormal();
	if (MoveDirection.IsNearlyZero())
	{
		return false;
	}

	const FVector PreviousLocation = GetActorLocation();
	const FVector MoveDelta = MoveDirection * MoveSpeedCmPerSecond * DeltaSeconds;
	FVector RequestedLocation = PreviousLocation + MoveDelta;
	if (!FMath::IsNearlyEqual(RequestedLocation.Z, FixedZPosition, 0.1f))
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Move ZLocked: FixedZ=%.2f Player=%s Before=%s"),
			FixedZPosition,
			*BuildDroneControllerLogString(RaidPC),
			*RequestedLocation.ToString());
	}
	LockZPositionForServer(RequestedLocation);
	FVector FinalLocation = ClampPositionToMovementBoundaryForServer(RequestedLocation);
	FinalLocation = ClampPositionOutsideBossCenterForServer(FinalLocation);
	SetActorLocation(FinalLocation, false, nullptr, ETeleportType::TeleportPhysics);
	const FVector CurrentLocation = GetActorLocation();
	if (CurrentLocation.Equals(PreviousLocation, 0.1f))
	{
		return false;
	}

	UpdateOwnerMoveSyncForServer(CurrentLocation, GetActorRotation());
	ForceNetUpdate();

	UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;
	const float DeltaMetersForLog = FVector::Dist2D(CurrentLocation, PreviousLocation) * MoveDistanceCmToMeters;
	const bool bMoveAcceptedStart = !bMoveAcceptedSummaryInputActive;
	const bool bMoveAcceptedAxisChanged = !bHasLastMoveAcceptedSummaryAxis
		|| IsMoveAcceptedSummaryAxisChangeSignificant(LastMoveAcceptedSummaryAxis, Axis);
	if (bMoveAcceptedAxisChanged
		|| bMoveAcceptedStart
		|| Now - LastMoveAcceptedSummaryLogTime >= MoveAcceptedSummaryLogIntervalSeconds)
	{
		LastMoveAcceptedSummaryLogTime = Now;
		LastMoveAcceptedSummaryAxis = Axis;
		bHasLastMoveAcceptedSummaryAxis = true;
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Move Accepted: Player=%s Axis=%s Speed=%.2f Delta2D=%.2f TotalDistance=%.2f"),
			*BuildDroneControllerLogString(RaidPC),
			*Axis.ToString(),
			GetCurrentMoveSpeed(),
			DeltaMetersForLog,
			AccumulatedMoveDistanceMeters + DeltaMetersForLog);
	}
	bMoveAcceptedSummaryInputActive = true;

	if (Now - LastServerMoveAppliedSummaryLogTime >= MoveDistanceSummaryLogIntervalSeconds)
	{
		LastServerMoveAppliedSummaryLogTime = Now;
		const APawn* ControlledPawn = RaidPC->GetPawn();
		const AActor* ViewTarget = RaidPC->GetViewTarget();
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ServerMoveApplied PC=%s Drone=%s Pawn=%s bPawnMatchesDrone=%s Axis=%s Delta=%s ServerLocation=%s State=%s ReplicateMovement=%s ViewTarget=%s ViewTargetResult=%s"),
			*BuildDroneControllerLogString(RaidPC),
			*GetName(),
			ControlledPawn ? *ControlledPawn->GetName() : TEXT("None"),
			ControlledPawn == this ? TEXT("true") : TEXT("false"),
			*Axis.ToString(),
			*MoveDelta.ToString(),
			*CurrentLocation.ToString(),
			ARaidPlayerController::SelectionStateToLogString(RaidPC->GetPlayerSelectionState()),
			IsReplicatingMovement() ? TEXT("true") : TEXT("false"),
			ViewTarget ? *ViewTarget->GetName() : TEXT("None"),
			TEXT("ServerObserved"));
	}

	return true;
}

void ADrone::UpdateOwnerMoveSyncForServer(const FVector& ServerLocation, const FRotator& ServerRotation)
{
	if (!HasAuthority())
	{
		return;
	}

	OwnerMoveSync.Location = ServerLocation;
	OwnerMoveSync.Rotation = ServerRotation;
	++OwnerMoveSync.Sequence;

	UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;
	if (Now - LastOwnerMoveSyncSentLogTime >= MoveDistanceSummaryLogIntervalSeconds)
	{
		LastOwnerMoveSyncSentLogTime = Now;
		const ARaidPlayerController* RaidPC = Cast<ARaidPlayerController>(GetController());
		const APawn* ControlledPawn = RaidPC ? RaidPC->GetPawn() : nullptr;
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] OwnerMoveSyncSent PC=%s Drone=%s Pawn=%s bPawnMatchesDrone=%s Location=%s Rotation=%s Sequence=%u"),
			*BuildDroneControllerLogString(RaidPC),
			*GetName(),
			ControlledPawn ? *ControlledPawn->GetName() : TEXT("None"),
			ControlledPawn == this ? TEXT("true") : TEXT("false"),
			*OwnerMoveSync.Location.ToString(),
			*OwnerMoveSync.Rotation.ToString(),
			OwnerMoveSync.Sequence);
	}
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

	if (ResultName == FName(TEXT("Ignored")))
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Move Ignored: Reason=%s Player=%s Axis=%s"),
			Reason ? Reason : TEXT("Unknown"),
			*BuildDroneControllerLogString(Cast<AController>(GetController())),
			*Axis.ToString());
	}
}

bool ADrone::CanAccumulateMoveDistanceForServer(FName& OutIgnoreReason) const
{
	return IsMovementAllowedForServer(OutIgnoreReason);
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
	const float DeltaMeters = FVector::Dist2D(CurrentLocation, PreviousLocation) * MoveDistanceCmToMeters;
	LastMoveDistanceLocation = CurrentLocation;
	if (DeltaMeters <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float MaxSpeedCmPerSecond = FMath::Max(0.0f, GetCurrentMoveSpeed() / MoveDistanceCmToMeters);
	const float ExpectedMeters = MaxSpeedCmPerSecond * FMath::Max(DeltaSeconds, 0.0f) * MoveDistanceCmToMeters;
	const float AllowedMeters = FMath::Max(MoveDistanceExpectedDeltaSlackMeters, ExpectedMeters * MoveDistanceExpectedDeltaMultiplier);
	if (DeltaMeters > MoveDistanceHardMaxDeltaMeters || DeltaMeters > AllowedMeters)
	{
		LogMoveDistanceIgnored(FName(TEXT("TooLargeDelta")));
		return;
	}

	VectorAccumulatedMoveDistanceMeters += DeltaMeters;
	BoosterAccumulatedMoveDistanceMeters += DeltaMeters;
	AccumulatedMoveDistanceMeters += DeltaMeters;
	AddMoveDistanceToCombatRecordForServer(DeltaMeters);
	RefreshMoveSpeedForServer();

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
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] MoveDistance PC=%s Delta=%.2f VectorTotal=%.2f BoosterTotal=%.2f Total=%.2f State=%s"),
			*BuildDroneControllerLogString(Cast<AController>(GetController())),
			DeltaMeters,
			VectorAccumulatedMoveDistanceMeters,
			BoosterAccumulatedMoveDistanceMeters,
			AccumulatedMoveDistanceMeters,
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
	AccumulatedMoveDistanceMeters = 0.0f;
	RefreshMovementBoundaryCenterForServer();
	LastMoveDistanceLocation = GetActorLocation();
	bHasMoveDistanceSample = false;

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] MoveDistanceReset PC=%s Reason=%s"),
		*BuildDroneControllerLogString(Cast<AController>(GetController())),
		Reason.IsNone() ? TEXT("Unknown") : *Reason.ToString());
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Move DistanceReset: Reason=%s Player=%s"),
		Reason.IsNone() ? TEXT("Unknown") : *Reason.ToString(),
		*BuildDroneControllerLogString(Cast<AController>(GetController())));
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

FDroneWeaponCalculationResult ADrone::CalculateWeaponDamageForServer(FName WeaponPartID, bool bIsLeftWeapon)
{
	FDroneWeaponCalculationInput Input;
	Input.WeaponType = ResolveWeaponTypeForServer(WeaponPartID);
	Input.Slot = bIsLeftWeapon ? EDroneCombatWeaponSlot::Left : EDroneCombatWeaponSlot::Right;
	Input.PulseAttackCount = bIsLeftWeapon ? LeftPulseAttackCount : RightPulseAttackCount;
	Input.VectorAccumulatedMoveDistanceMeters = VectorAccumulatedMoveDistanceMeters;

	FDroneWeaponCalculationResult Result = FDroneCombatRules::CalculateWeaponDamage(Input);
	if (Input.WeaponType == EDroneCombatWeaponType::PulseLaser)
	{
		if (bIsLeftWeapon)
		{
			LeftPulseAttackCount = Result.PulseAttackCount;
		}
		else
		{
			RightPulseAttackCount = Result.PulseAttackCount;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] WeaponCalc Player=%s Slot=%s WeaponType=%s BaseDamage=%.2f BonusDamage=%.2f HitCount=%d AdditionalHitCount=%d PulseCount=%d VectorDistance=%.2f WeaponDamage=%.2f ResetVector=%s"),
		*BuildDroneControllerLogString(Cast<AController>(GetController())),
		ToWeaponSlotLogString(bIsLeftWeapon),
		ToCombatWeaponTypeLogString(Input.WeaponType),
		Result.BaseDamage,
		Result.BonusDamage,
		Result.HitCount,
		Result.AdditionalHitCount,
		Result.PulseAttackCount,
		Result.VectorDistance,
		Result.WeaponDamage,
		Result.bResetVectorDistance ? TEXT("true") : TEXT("false"));

	return Result;
}

FDroneCoreCalculationResult ADrone::CalculateCoreForServer(FName CorePartID) const
{
	FDroneCoreCalculationInput Input;
	Input.CoreType = ResolveCoreTypeForServer(CorePartID);
	Input.CurrentHP = Health;
	Input.MaxHP = static_cast<float>(MaxHealth);
	Input.AccumulatedMoveDistanceMeters = BoosterAccumulatedMoveDistanceMeters;

	const FDroneCoreCalculationResult Result = FDroneCombatRules::CalculateCoreBonus(Input);
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] CoreCalc Player=%s CoreType=%s HPRatio=%.2f AccumulatedMoveDistance=%.2f CoreModifier=%.2f CoreBonusAttackModifier=%.2f MoveSpeedBonus=%.2f HealAmount=0.00"),
		*BuildDroneControllerLogString(Cast<AController>(GetController())),
		ToCombatCoreTypeLogString(Input.CoreType),
		Result.HPRatio,
		Input.AccumulatedMoveDistanceMeters,
		Result.CoreAttackModifier,
		Result.CoreBonusAttackModifier,
		Result.MoveSpeedBonus);
	return Result;
}

EDroneCombatWeaponType ADrone::ResolveWeaponTypeForServer(FName WeaponPartID) const
{
	if (WeaponPartID == ADronePartInventory::GetPulseLaserPartID())
	{
		return EDroneCombatWeaponType::PulseLaser;
	}
	if (WeaponPartID == ADronePartInventory::GetFractureBurstPartID())
	{
		return EDroneCombatWeaponType::FractureBurst;
	}
	if (WeaponPartID == ADronePartInventory::GetVectorCannonPartID())
	{
		return EDroneCombatWeaponType::VectorCannon;
	}
	return EDroneCombatWeaponType::None;
}

EDroneCombatCoreType ADrone::ResolveCoreTypeForServer(FName CorePartID) const
{
	if (CorePartID == ADronePartInventory::GetCoreZenithPartID())
	{
		return EDroneCombatCoreType::Zenith;
	}
	if (CorePartID == ADronePartInventory::GetCoreBoosterPartID())
	{
		return EDroneCombatCoreType::Booster;
	}
	if (CorePartID == ADronePartInventory::GetCoreDrainPartID())
	{
		return EDroneCombatCoreType::Drain;
	}
	return EDroneCombatCoreType::None;
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

float ADrone::ApplyDrainHealForServer(float DamageDealt)
{
	if (!HasAuthority())
	{
		return 0.0f;
	}

	if (bIsDead)
	{
		LogDeadInputIgnored(TEXT("Heal"));
		return 0.0f;
	}

	const float SafeDamageDealt = FMath::Max(0.0f, DamageDealt);
	const float CappedHealAmount = FDroneCombatRules::CalculateDrainHeal(SafeDamageDealt);
	const float PreviousHealth = Health;
	Health = FMath::Clamp(Health + CappedHealAmount, 0.0f, static_cast<float>(MaxHealth));
	const float AppliedHealAmount = FMath::Max(0.0f, Health - PreviousHealth);
	const bool bCapped = SafeDamageDealt * 0.12f > CappedHealAmount + KINDA_SMALL_NUMBER
		|| AppliedHealAmount + KINDA_SMALL_NUMBER < SafeDamageDealt * 0.12f;
	AddHealToCombatRecordForServer(AppliedHealAmount);

	if (AppliedHealAmount > KINDA_SMALL_NUMBER)
	{
		ForceNetUpdate();
	}

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] CoreCalc Player=%s CoreType=Drain HPRatio=%.2f AccumulatedMoveDistance=%.2f CoreBonusAttackModifier=1.00 MoveSpeedBonus=0.00 HealAmount=%.2f"),
		*BuildDroneControllerLogString(Cast<AController>(GetController())),
		MaxHealth > 0 ? FMath::Clamp(Health / static_cast<float>(MaxHealth), 0.0f, 1.0f) : 0.0f,
		BoosterAccumulatedMoveDistanceMeters,
		AppliedHealAmount);

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] DrainHeal PC=%s DamageDealt=%.2f Heal=%.2f HP=%.2f/%d Capped=%s"),
		*BuildDroneControllerLogString(Cast<AController>(GetController())),
		SafeDamageDealt,
		AppliedHealAmount,
		Health,
		MaxHealth,
		bCapped ? TEXT("true") : TEXT("false"));
	return AppliedHealAmount;
}

void ADrone::StartCombatRecordForServer()
{
	if (!HasAuthority())
	{
		return;
	}

	CombatRecord = FDroneCombatRecord();
	bCombatRecordActive = true;

	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;
	CombatRecord.RaidJoinTime = Now;
	CombatRecord.CombatStartTime = Now;
	CombatRecord.CombatEndTime = 0.0f;
	CombatRecord.bIsAliveAtReport = !bIsDead;

	if (const ARaidBoss* Boss = FindRaidBossForServer())
	{
		CombatRecord.BossMaxHP = Boss->GetMaxHP();
		CombatRecord.BossHPOnJoin = Boss->GetCurrentHP();
	}
}

FDroneCombatRecord ADrone::BuildCombatRecordSnapshotForServer() const
{
	FDroneCombatRecord Snapshot = CombatRecord;
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : Snapshot.CombatEndTime;
	const float EndTime = Snapshot.CombatEndTime > KINDA_SMALL_NUMBER ? Snapshot.CombatEndTime : Now;
	if (Snapshot.CombatStartTime > KINDA_SMALL_NUMBER)
	{
		Snapshot.SurvivalTime = FMath::Max(0.0f, EndTime - Snapshot.CombatStartTime);
	}
	Snapshot.bIsAliveAtReport = !bIsDead;
	return Snapshot;
}

void ADrone::AddBossDamageToCombatRecordForServer(float DamageDealt, const ARaidBoss* Boss)
{
	if (!HasAuthority() || !bCombatRecordActive)
	{
		return;
	}

	CombatRecord.BossDamage += FMath::Max(0.0f, DamageDealt);
	if (Boss)
	{
		CombatRecord.BossMaxHP = Boss->GetMaxHP();
		if (CombatRecord.BossHPOnJoin <= KINDA_SMALL_NUMBER)
		{
			CombatRecord.BossHPOnJoin = Boss->GetCurrentHP();
		}
	}
}

void ADrone::AddHealToCombatRecordForServer(float HealAmount)
{
	if (HasAuthority() && bCombatRecordActive)
	{
		CombatRecord.HealAmount += FMath::Max(0.0f, HealAmount);
	}
}

void ADrone::AddMoveDistanceToCombatRecordForServer(float DeltaMeters)
{
	if (HasAuthority() && bCombatRecordActive)
	{
		CombatRecord.MoveDistance += FMath::Max(0.0f, DeltaMeters);
	}
}

void ADrone::MarkCombatRecordEndedForServer()
{
	if (!HasAuthority() || !bCombatRecordActive)
	{
		return;
	}

	if (CombatRecord.CombatEndTime <= KINDA_SMALL_NUMBER)
	{
		const UWorld* World = GetWorld();
		CombatRecord.CombatEndTime = World ? World->GetTimeSeconds() : CombatRecord.CombatStartTime;
	}
	CombatRecord.bIsAliveAtReport = !bIsDead;
}

void ADrone::LogCombatRecordForServer() const
{
	const FDroneCombatRecord Snapshot = BuildCombatRecordSnapshotForServer();
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] CombatRecord Player=%s SurvivalTime=%.2f BossDamage=%.2f BossMaxHP=%.2f BossHPOnJoin=%.2f MoveDistance=%.2f HealAmount=%.2f DamageTakenCount=%d CombatStartTime=%.2f RaidJoinTime=%.2f IsAliveAtReport=%s"),
		*BuildDroneControllerLogString(Cast<AController>(GetController())),
		Snapshot.SurvivalTime,
		Snapshot.BossDamage,
		Snapshot.BossMaxHP,
		Snapshot.BossHPOnJoin,
		Snapshot.MoveDistance,
		Snapshot.HealAmount,
		Snapshot.DamageTakenCount,
		Snapshot.CombatStartTime,
		Snapshot.RaidJoinTime,
		Snapshot.bIsAliveAtReport ? TEXT("true") : TEXT("false"));
}

void ADrone::RefreshMoveSpeedForServer()
{
	if (!HasAuthority() || !FloatingMovement)
	{
		return;
	}

	if (BaseMoveSpeedCmPerSecond <= KINDA_SMALL_NUMBER)
	{
		BaseMoveSpeedCmPerSecond = DefaultBaseMoveSpeedCmPerSecond;
	}

	FDroneCoreCalculationInput Input;
	Input.CoreType = ResolveCoreTypeForServer(EquippedCorePartID);
	Input.CurrentHP = Health;
	Input.MaxHP = static_cast<float>(MaxHealth);
	Input.AccumulatedMoveDistanceMeters = BoosterAccumulatedMoveDistanceMeters;
	const FDroneCoreCalculationResult CoreResult = FDroneCombatRules::CalculateCoreBonus(Input);
	FloatingMovement->MaxSpeed = BaseMoveSpeedCmPerSecond * (1.0f + CoreResult.MoveSpeedBonus);
}

void ADrone::ResetCombatRuntimeStateForReason(FName Reason)
{
	LeftPulseAttackCount = 0;
	RightPulseAttackCount = 0;
	CancelDodgeForServer(Reason);
	LastServerMoveInput = FVector2D::ZeroVector;
	NextDodgeAllowedServerTime = 0.0f;
	LastDodgeEndTime = -1000.0f;
	bIsAttacking = false;
	ResetMoveDistanceForServer(Reason);
	if (Reason == FName(TEXT("Death")) || Reason == FName(TEXT("RaidEnd")))
	{
		MarkCombatRecordEndedForServer();
		LogCombatRecordForServer();
		bCombatRecordActive = false;
	}
	RefreshMoveSpeedForServer();
}

void ADrone::LogDeadInputIgnored(const TCHAR* ActionName) const
{
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] DeadInputIgnored PC=%s Action=%s"),
		*BuildDroneControllerLogString(Cast<AController>(GetController())),
		ActionName ? ActionName : TEXT("Unknown"));
}
