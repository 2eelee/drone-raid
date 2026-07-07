#include "RaidBoss.h"

#include "Drone.h"
#include "RaidGameState.h"
#include "RaidBossAttackTelegraph.h"
#include "RaidPlayerController.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
const TCHAR* ToBossAttackRaidStateLogString(const ARaidGameState* RaidGameState)
{
	if (!RaidGameState)
	{
		return TEXT("None");
	}

	switch (RaidGameState->RaidState)
	{
	case ERaidState::Waiting:
		return TEXT("Waiting");
	case ERaidState::Drafting:
		return TEXT("Drafting");
	case ERaidState::Battle:
		return TEXT("Battle");
	case ERaidState::End:
		return TEXT("End");
	default:
		return TEXT("Unknown");
	}
}

FString BuildBossAttackControllerLogString(const AController* Controller)
{
	return ARaidPlayerController::BuildStableControllerLogString(Controller);
}
}

ARaidBoss::ARaidBoss()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	NetDormancy = DORM_Never;
	SetReplicatingMovement(false);

	BossID = FName(TEXT("BOSS_MAIN"));
	BossDisplayName = FText::FromString(TEXT("Raid Boss"));
	bIsTargetable = true;
	TargetMarkerSocketName = FName(TEXT("TargetMarker"));

	PrototypeVisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PrototypeVisualMesh"));
	SetRootComponent(PrototypeVisualMesh);
	PrototypeVisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PrototypeVisualMesh->SetGenerateOverlapEvents(false);
	PrototypeVisualMesh->SetCanEverAffectNavigation(false);
	PrototypeVisualMesh->SetVisibility(true, true);
	PrototypeVisualMesh->SetHiddenInGame(false, true);
	PrototypeVisualMesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMeshFinder.Succeeded())
	{
		PrototypeVisualMesh->SetStaticMesh(SphereMeshFinder.Object);
	}

	PrototypeVisualLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PrototypeVisualLabel"));
	PrototypeVisualLabel->SetupAttachment(PrototypeVisualMesh);
	PrototypeVisualLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PrototypeVisualLabel->SetGenerateOverlapEvents(false);
	PrototypeVisualLabel->SetCanEverAffectNavigation(false);
	PrototypeVisualLabel->SetVisibility(true, true);
	PrototypeVisualLabel->SetHiddenInGame(false, true);
	PrototypeVisualLabel->SetCastShadow(false);
	PrototypeVisualLabel->SetText(FText::FromString(TEXT("RAID BOSS")));
	PrototypeVisualLabel->SetHorizontalAlignment(EHTA_Center);
	PrototypeVisualLabel->SetVerticalAlignment(EVRTA_TextCenter);
	PrototypeVisualLabel->SetTextRenderColor(FColor::Red);
	PrototypeVisualLabel->SetWorldSize(120.0f);

	ApplyPrototypeVisualSettings();
	RefreshPrototypeVisualHPText();
}

void ARaidBoss::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		CurrentHP = MaxHP;
		ForceNetUpdate();
	}
	RefreshPrototypeVisualHPText();

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Boss VisualReady: Boss=%s VisualReady=%s Mesh=%s MeshVisible=%s LabelVisible=%s Collision=%s Location=%s"),
		*GetName(),
		IsVisualReadyForCamera() ? TEXT("True") : TEXT("False"),
		(PrototypeVisualMesh && PrototypeVisualMesh->GetStaticMesh()) ? *PrototypeVisualMesh->GetStaticMesh()->GetName() : TEXT("None"),
		(PrototypeVisualMesh && PrototypeVisualMesh->IsVisible()) ? TEXT("True") : TEXT("False"),
		(PrototypeVisualLabel && PrototypeVisualLabel->IsVisible()) ? TEXT("True") : TEXT("False"),
		(PrototypeVisualMesh && PrototypeVisualMesh->GetCollisionEnabled() != ECollisionEnabled::NoCollision) ? TEXT("CollisionEnabled") : TEXT("NoCollision"),
		*GetActorLocation().ToString());
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] D20PIEChecklist CombatVisual: Confirm=CombatVisual Attack,CombatVisual BossDamaged,Dodge VisualHidden,Dodge VisualShown,CombatVisual TelegraphStart,CombatVisual TelegraphEnd,CombatVisual DroneDamaged,CombatVisual DroneDamageIgnored"));

	if (!IsVisualReadyForCamera())
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Boss VisualWarning: Boss=%s Reason=NoVisibleMesh"),
			*GetName());
	}
}

void ARaidBoss::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// defeat 경로 없이 Destroy/레벨 언로드되는 경우에도 서버의 모든 PC 타겟을 무효화한다.
	if (HasAuthority())
	{
		StopBossPatternForServer(FName(TEXT("BossEndPlay")));
		ClearThisBossFromAllTargetsForServer(FName(TEXT("BossDestroyed")));
	}
	Super::EndPlay(EndPlayReason);
}

void ARaidBoss::Destroyed()
{
	// EndPlay는 BeginPlay를 거친 액터에만 오므로, 명시적 Destroy는 여기서도 서버 정리를 보장한다.
	// (타겟 해제/패턴 정지는 멱등이라 EndPlay와 중복 호출돼도 무해)
	if (HasAuthority())
	{
		StopBossPatternForServer(FName(TEXT("BossDestroyed")));
		ClearThisBossFromAllTargetsForServer(FName(TEXT("BossDestroyed")));
	}
	Super::Destroyed();
}

bool ARaidBoss::StartBossPatternForServer()
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossPattern StartIgnored: Reason=NotAuthority Boss=%s"), *GetName());
		return false;
	}

	if (IsDefeated())
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossPattern StartIgnored: Reason=BossDead Boss=%s HP=%.2f"), *GetName(), CurrentHP);
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossPattern StartIgnored: Reason=NoWorld Boss=%s"), *GetName());
		return false;
	}

	if (World->GetTimerManager().IsTimerActive(BossPatternTimerHandle))
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossPattern StartIgnored: Reason=AlreadyActive Boss=%s"), *GetName());
		return false;
	}

	const float ClampedInterval = FMath::Max(0.5f, BossPatternIntervalSeconds);
	World->GetTimerManager().SetTimer(
		BossPatternTimerHandle,
		this,
		&ARaidBoss::HandleBossPatternTimerFiredForServer,
		ClampedInterval,
		true);
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossPattern Started: Boss=%s Interval=%.2f Radius=%.2f Damage=%d Telegraph=%.2f"),
		*GetName(),
		ClampedInterval,
		BossPatternRadiusCm,
		BossPatternDamage,
		BossPatternTelegraphSeconds);
	return true;
}

void ARaidBoss::StopBossPatternForServer(FName Reason)
{
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (World->GetTimerManager().TimerExists(BossPatternTimerHandle))
	{
		World->GetTimerManager().ClearTimer(BossPatternTimerHandle);
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossPattern Stopped: Boss=%s Reason=%s LastSeq=%d"),
			*GetName(),
			Reason.IsNone() ? TEXT("Unknown") : *Reason.ToString(),
			BossPatternFireSequence);
	}
}

void ARaidBoss::SetStunnedForServer(bool bInStunned, FName Reason)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossStun Ignored: Reason=NotAuthority Boss=%s"), *GetName());
		return;
	}

	if (bInStunned && IsDefeated())
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossStun Ignored: Reason=BossDead Boss=%s HP=%.2f"), *GetName(), CurrentHP);
		return;
	}

	if (bIsStunned == bInStunned)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossStun Ignored: Reason=NoChange Boss=%s State=%s"),
			*GetName(),
			bIsStunned ? TEXT("Begin") : TEXT("End"));
		return;
	}

	bIsStunned = bInStunned;
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossStun State=%s Reason=%s Boss=%s StunMultiplier=%.2f"),
		bIsStunned ? TEXT("Begin") : TEXT("End"),
		Reason.IsNone() ? TEXT("Unknown") : *Reason.ToString(),
		*GetName(),
		FMath::Max(1.0f, StunDamageMultiplier));

	// OnRep은 클라에서만 발화하므로 standalone(서버=뷰어)에서는 여기서 visual 훅을 호출한다.
	if (GetNetMode() == NM_Standalone)
	{
		BP_OnBossStunChangedVisual(bIsStunned);
	}
}

bool ARaidBoss::IsStunned() const
{
	return bIsStunned;
}

void ARaidBoss::OnRep_IsStunned()
{
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] CombatVisual BossStunChanged: Boss=%s Stunned=%s"),
		*GetName(),
		bIsStunned ? TEXT("true") : TEXT("false"));
	BP_OnBossStunChangedVisual(bIsStunned);
}

void ARaidBoss::BP_OnBossStunChangedVisual_Implementation(bool bNewStunned)
{
	// visual 전용 훅 — Blueprint 오버라이드가 없으면 아무것도 하지 않는다.
	(void)bNewStunned;
}

void ARaidBoss::HandleBossPatternTimerFiredForServer()
{
	if (!HasAuthority())
	{
		return;
	}

	// 정지 호출이 누락된 경로가 있어도 죽은 보스/종료된 레이드에서 패턴이 지속되지 않게 자체 정지한다.
	if (IsDefeated())
	{
		StopBossPatternForServer(FName(TEXT("BossDead")));
		return;
	}

	UWorld* World = GetWorld();
	const ARaidGameState* RaidGameState = World ? World->GetGameState<ARaidGameState>() : nullptr;
	if (RaidGameState && RaidGameState->RaidState == ERaidState::End)
	{
		StopBossPatternForServer(FName(TEXT("RaidEnd")));
		return;
	}

	BossPatternFireSequence++;
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossPattern Fired: Seq=%d Boss=%s Center=%s"),
		BossPatternFireSequence,
		*GetName(),
		*GetActorLocation().ToString());
	StartDebugTelegraphedAreaAttackForServer(
		GetActorLocation(),
		BossPatternRadiusCm,
		BossPatternDamage,
		BossPatternTelegraphSeconds);
}

void ARaidBoss::ClearThisBossFromAllTargetsForServer(FName Reason)
{
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!IsValid(PC))
		{
			continue;
		}

		if (ARaidPlayerController* RaidPC = Cast<ARaidPlayerController>(PC))
		{
			// EndPlay 시점엔 this가 pending kill이라 안전 getter 비교가 항상 실패한다 → raw 비교 사용.
			if (RaidPC->IsTargetingBossForServer(this))
			{
				RaidPC->ClearBossTargetForServer(Reason);
			}
		}
	}
}

void ARaidBoss::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyPrototypeVisualSettings();
	RefreshPrototypeVisualHPText();
}

void ARaidBoss::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARaidBoss, MaxHP);
	DOREPLIFETIME(ARaidBoss, CurrentHP);
	DOREPLIFETIME(ARaidBoss, bIsStunned);
}

void ARaidBoss::ApplyDamageForServer(float DamageAmount, AController* InstigatorController, AActor* DamageCauser)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Client] RaidBoss damage rejected: server authority required"));
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossDamageFailed: Reason=NotAuthority Damage=%.2f Instigator=%s Causer=%s"),
			DamageAmount,
			InstigatorController ? *InstigatorController->GetName() : TEXT("None"),
			DamageCauser ? *DamageCauser->GetName() : TEXT("None"));
		return;
	}

	const float ClampedDamage = FMath::Max(0.0f, DamageAmount);
	if (ClampedDamage <= KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossDamageIgnored: Reason=NoDamage HP=%.2f Damage=%.2f Instigator=%s Causer=%s"),
			CurrentHP,
			ClampedDamage,
			InstigatorController ? *InstigatorController->GetName() : TEXT("None"),
			DamageCauser ? *DamageCauser->GetName() : TEXT("None"));
		return;
	}

	if (IsDefeated())
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossDamageIgnored: Reason=BossDead HP=%.2f Damage=%.2f Instigator=%s Causer=%s"),
			CurrentHP,
			ClampedDamage,
			InstigatorController ? *InstigatorController->GetName() : TEXT("None"),
			DamageCauser ? *DamageCauser->GetName() : TEXT("None"));
		return;
	}

	const float AppliedStunMultiplier = bIsStunned ? FMath::Max(1.0f, StunDamageMultiplier) : 1.0f;
	const float StunAdjustedDamage = ClampedDamage * AppliedStunMultiplier;
	const float PreviousHP = CurrentHP;
	CurrentHP = FMath::Clamp(CurrentHP - StunAdjustedDamage, 0.0f, MaxHP);
	ForceNetUpdate();
	RefreshPrototypeVisualHPText();
	const float ActualDamage = FMath::Max(0.0f, PreviousHP - CurrentHP);
	if (ActualDamage <= KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossDamageIgnored: Reason=NoHPChange HP=%.2f Damage=%.2f Instigator=%s Causer=%s"),
			CurrentHP,
			ClampedDamage,
			InstigatorController ? *InstigatorController->GetName() : TEXT("None"),
			DamageCauser ? *DamageCauser->GetName() : TEXT("None"));
		return;
	}

	if (ActualDamage > KINDA_SMALL_NUMBER)
	{
		Multicast_PlayBossDamagedVisual(ActualDamage, PreviousHP, CurrentHP, DamageCauser);
		if (GetNetMode() == NM_Standalone)
		{
			PlayBossDamagedVisualLocally(ActualDamage, PreviousHP, CurrentHP, DamageCauser);
		}
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] CombatVisual BossDamaged: Damage=%.2f OldHP=%.2f NewHP=%.2f Causer=%s"),
			ActualDamage,
			PreviousHP,
			CurrentHP,
			DamageCauser ? *DamageCauser->GetName() : TEXT("None"));
	}

	UE_LOG(LogTemp, Log, TEXT("[Server] RaidBoss DamageApplied: Damage=%.2f HP=%.2f/%.2f PreviousHP=%.2f Instigator=%s Causer=%s"),
		ClampedDamage,
		CurrentHP,
		MaxHP,
		PreviousHP,
		InstigatorController ? *InstigatorController->GetName() : TEXT("None"),
		DamageCauser ? *DamageCauser->GetName() : TEXT("None"));

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossDamage: OldHP=%.2f Damage=%.2f NewHP=%.2f MaxHP=%.2f StunMultiplier=%.2f Instigator=%s Causer=%s"),
		PreviousHP,
		StunAdjustedDamage,
		CurrentHP,
		MaxHP,
		AppliedStunMultiplier,
		InstigatorController ? *InstigatorController->GetName() : TEXT("None"),
		DamageCauser ? *DamageCauser->GetName() : TEXT("None"));

	if (PreviousHP > 0.0f && CurrentHP <= 0.0f)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossDeath: OldHP=%.2f Damage=%.2f Instigator=%s Causer=%s"),
			PreviousHP,
			ClampedDamage,
			InstigatorController ? *InstigatorController->GetName() : TEXT("None"),
			DamageCauser ? *DamageCauser->GetName() : TEXT("None"));

		StopBossPatternForServer(FName(TEXT("BossDead")));
		ClearThisBossFromAllTargetsForServer(FName(TEXT("BossDead")));
	}
}

int32 ARaidBoss::PerformDebugAreaAttackForServer(FVector AttackCenter, float RadiusCm, int32 DamageAmount)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossAttackIgnored Reason=NotAuthority Boss=%s Center=%s RadiusCm=%.2f Damage=%d"),
			*GetName(),
			*AttackCenter.ToString(),
			RadiusCm,
			DamageAmount);
		return 0;
	}

	const float SafeRadiusCm = FMath::Max(0.0f, RadiusCm);
	const int32 AppliedDamage = FMath::Max(0, DamageAmount);
	if (SafeRadiusCm <= KINDA_SMALL_NUMBER || AppliedDamage <= 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossAttackIgnored Reason=InvalidParams Boss=%s Center=%s RadiusCm=%.2f Damage=%d"),
			*GetName(),
			*AttackCenter.ToString(),
			RadiusCm,
			DamageAmount);
		return 0;
	}

	if (IsDefeated())
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossAttackIgnored Reason=BossDead Boss=%s Center=%s RadiusCm=%.2f Damage=%d HP=%.2f"),
			*GetName(),
			*AttackCenter.ToString(),
			SafeRadiusCm,
			AppliedDamage,
			CurrentHP);
		return 0;
	}

	UWorld* World = GetWorld();
	ARaidGameState* RaidGameState = World ? World->GetGameState<ARaidGameState>() : nullptr;
	if (RaidGameState && RaidGameState->RaidState == ERaidState::End)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossAttackIgnored Reason=RaidEnd Boss=%s Center=%s RadiusCm=%.2f Damage=%d RaidState=End"),
			*GetName(),
			*AttackCenter.ToString(),
			SafeRadiusCm,
			AppliedDamage);
		return 0;
	}

	if (!World)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossAttackIgnored Reason=NoWorld Boss=%s Center=%s RadiusCm=%.2f Damage=%d"),
			*GetName(),
			*AttackCenter.ToString(),
			SafeRadiusCm,
			AppliedDamage);
		return 0;
	}

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossAttack Boss=%s Center=%s RadiusCm=%.2f Damage=%d RaidState=%s"),
		*GetName(),
		*AttackCenter.ToString(),
		SafeRadiusCm,
		AppliedDamage,
		ToBossAttackRaidStateLogString(RaidGameState));

	int32 HitCount = 0;
	const float RadiusSq = FMath::Square(SafeRadiusCm);
	for (TActorIterator<ADrone> It(World); It; ++It)
	{
		ADrone* Drone = *It;
		if (!Drone)
		{
			continue;
		}

		AController* DroneController = Cast<AController>(Drone->GetController());
		ARaidPlayerController* RaidPC = Cast<ARaidPlayerController>(DroneController);
		const FString PlayerLog = BuildBossAttackControllerLogString(DroneController);

		if (Drone->IsDead())
		{
			UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossAttackMiss Reason=Dead Player=%s Drone=%s"),
				*PlayerLog,
				*Drone->GetName());
			continue;
		}

		if (!RaidPC || RaidPC->GetPawn() != Drone)
		{
			UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossAttackMiss Reason=InvalidController Player=%s Drone=%s"),
				*PlayerLog,
				*Drone->GetName());
			continue;
		}

		if (RaidPC->GetPlayerSelectionState() != EPlayerSelectionState::InBattle)
		{
			UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossAttackMiss Reason=NotInBattle Player=%s Drone=%s State=%s"),
				*PlayerLog,
				*Drone->GetName(),
				ARaidPlayerController::SelectionStateToLogString(RaidPC->GetPlayerSelectionState()));
			continue;
		}

		const float DistanceSq = FVector::DistSquared2D(Drone->GetActorLocation(), AttackCenter);
		const float DistanceCm = FMath::Sqrt(DistanceSq);
		if (DistanceSq > RadiusSq)
		{
			UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossAttackMiss Reason=OutOfRange Player=%s Drone=%s DistanceCm=%.2f RadiusCm=%.2f"),
				*PlayerLog,
				*Drone->GetName(),
				DistanceCm,
				SafeRadiusCm);
			continue;
		}

		const int32 HPBefore = Drone->GetHealth();
		Drone->ApplyDamageForServer(AppliedDamage, FName(TEXT("BossAttack")));
		HitCount++;

		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossAttackHit Player=%s Drone=%s DistanceCm=%.2f RadiusCm=%.2f Damage=%d HP=%d->%d Dead=%s"),
			*PlayerLog,
			*Drone->GetName(),
			DistanceCm,
			SafeRadiusCm,
			AppliedDamage,
			HPBefore,
			Drone->GetHealth(),
			Drone->IsDead() ? TEXT("true") : TEXT("false"));
	}

	UE_LOG(LogTemp, Log, TEXT("[Server] BossAttack completed: Boss=%s HitCount=%d Center=%s RadiusCm=%.2f Damage=%d"),
		*GetName(),
		HitCount,
		*AttackCenter.ToString(),
		SafeRadiusCm,
		AppliedDamage);

	return HitCount;
}

bool ARaidBoss::StartDebugTelegraphedAreaAttackForServer(FVector AttackCenter, float RadiusCm, int32 DamageAmount, float TelegraphSeconds)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossAttackIgnored Reason=NotAuthority Boss=%s Center=%s RadiusCm=%.2f Damage=%d Delay=%.2f"),
			*GetName(),
			*AttackCenter.ToString(),
			RadiusCm,
			DamageAmount,
			TelegraphSeconds);
		return false;
	}

	const float SafeRadiusCm = FMath::Max(0.0f, RadiusCm);
	const int32 AppliedDamage = FMath::Max(0, DamageAmount);
	const float SafeTelegraphSeconds = FMath::Max(0.0f, TelegraphSeconds);
	if (SafeRadiusCm <= KINDA_SMALL_NUMBER || AppliedDamage <= 0 || SafeTelegraphSeconds <= KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossAttackIgnored Reason=InvalidParams Boss=%s Center=%s RadiusCm=%.2f Damage=%d Delay=%.2f"),
			*GetName(),
			*AttackCenter.ToString(),
			RadiusCm,
			DamageAmount,
			TelegraphSeconds);
		return false;
	}

	if (IsDefeated())
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossAttackIgnored Reason=BossDead Boss=%s Center=%s RadiusCm=%.2f Damage=%d Delay=%.2f HP=%.2f"),
			*GetName(),
			*AttackCenter.ToString(),
			SafeRadiusCm,
			AppliedDamage,
			SafeTelegraphSeconds,
			CurrentHP);
		return false;
	}

	UWorld* World = GetWorld();
	ARaidGameState* RaidGameState = World ? World->GetGameState<ARaidGameState>() : nullptr;
	if (RaidGameState && RaidGameState->RaidState == ERaidState::End)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossAttackIgnored Reason=RaidEnd Boss=%s Center=%s RadiusCm=%.2f Damage=%d Delay=%.2f RaidState=End"),
			*GetName(),
			*AttackCenter.ToString(),
			SafeRadiusCm,
			AppliedDamage,
			SafeTelegraphSeconds);
		return false;
	}

	if (!World)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossAttackIgnored Reason=NoWorld Boss=%s Center=%s RadiusCm=%.2f Damage=%d Delay=%.2f"),
			*GetName(),
			*AttackCenter.ToString(),
			SafeRadiusCm,
			AppliedDamage,
			SafeTelegraphSeconds);
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossTelegraphStart Center=%s Radius=%.2f Damage=%d Delay=%.2f Boss=%s RaidState=%s"),
		*AttackCenter.ToString(),
		SafeRadiusCm,
		AppliedDamage,
		SafeTelegraphSeconds,
		*GetName(),
		ToBossAttackRaidStateLogString(RaidGameState));
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] CombatVisual TelegraphStart: Shape=Circle Location=%s Radius=%.2f Duration=%.2f Boss=%s"),
		*AttackCenter.ToString(),
		SafeRadiusCm,
		SafeTelegraphSeconds,
		*GetName());

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARaidBossAttackTelegraph* TelegraphActor = World->SpawnActor<ARaidBossAttackTelegraph>(
		ARaidBossAttackTelegraph::StaticClass(),
		AttackCenter,
		FRotator::ZeroRotator,
		SpawnParameters);

	if (!TelegraphActor)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossAttackIgnored Reason=TelegraphSpawnFailed Boss=%s Center=%s RadiusCm=%.2f Damage=%d Delay=%.2f"),
			*GetName(),
			*AttackCenter.ToString(),
			SafeRadiusCm,
			AppliedDamage,
			SafeTelegraphSeconds);
		return false;
	}

	TelegraphActor->InitializeForServer(AttackCenter, SafeRadiusCm, SafeTelegraphSeconds);
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossTelegraphSpawned Actor=%s Radius=%.2f"),
		*TelegraphActor->GetName(),
		SafeRadiusCm);

	const TWeakObjectPtr<ARaidBoss> WeakBoss(this);
	const TWeakObjectPtr<ARaidBossAttackTelegraph> WeakTelegraph(TelegraphActor);
	FTimerDelegate AttackDelegate = FTimerDelegate::CreateLambda(
		[WeakBoss, WeakTelegraph, AttackCenter, SafeRadiusCm, AppliedDamage]()
		{
			if (ARaidBoss* Boss = WeakBoss.Get())
			{
				Boss->ExecuteDebugTelegraphedAreaAttackForServer(
					AttackCenter,
					SafeRadiusCm,
					AppliedDamage,
					WeakTelegraph.Get());
			}
		});

	FTimerHandle AttackTimerHandle;
	World->GetTimerManager().SetTimer(AttackTimerHandle, AttackDelegate, SafeTelegraphSeconds, false);
	return true;
}

void ARaidBoss::ExecuteDebugTelegraphedAreaAttackForServer(FVector AttackCenter, float RadiusCm, int32 DamageAmount, ARaidBossAttackTelegraph* TelegraphActor)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossAttackIgnored Reason=NotAuthority Boss=%s Center=%s RadiusCm=%.2f Damage=%d"),
			*GetName(),
			*AttackCenter.ToString(),
			RadiusCm,
			DamageAmount);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossAttackExecute Reason=TelegraphExpired Boss=%s Center=%s Radius=%.2f Damage=%d Telegraph=%s"),
		*GetName(),
		*AttackCenter.ToString(),
		RadiusCm,
		DamageAmount,
		TelegraphActor ? *TelegraphActor->GetName() : TEXT("None"));

	PerformDebugAreaAttackForServer(AttackCenter, RadiusCm, DamageAmount);

	if (TelegraphActor && !TelegraphActor->IsActorBeingDestroyed())
	{
		TelegraphActor->MarkExpiredForServer();
	}
}

float ARaidBoss::GetCurrentHP() const
{
	return CurrentHP;
}

float ARaidBoss::GetMaxHP() const
{
	return MaxHP;
}

bool ARaidBoss::IsDefeated() const
{
	return CurrentHP <= 0.0f;
}

FName ARaidBoss::GetBossID() const
{
	return BossID;
}

FText ARaidBoss::GetBossDisplayName() const
{
	return BossDisplayName;
}

bool ARaidBoss::IsAliveForTargeting() const
{
	return !IsDefeated();
}

bool ARaidBoss::IsTargetableForDrone() const
{
	return bIsTargetable && IsAliveForTargeting();
}

FVector ARaidBoss::GetTargetMarkerWorldLocation() const
{
	const USceneComponent* Root = GetRootComponent();
	if (Root && !TargetMarkerSocketName.IsNone() && Root->DoesSocketExist(TargetMarkerSocketName))
	{
		return Root->GetSocketLocation(TargetMarkerSocketName);
	}

	return GetActorLocation() + FVector(0.0f, 0.0f, 200.0f);
}
bool ARaidBoss::IsVisualReadyForCamera() const
{
	if (IsHidden())
	{
		return false;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	for (const UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (PrimitiveComponent && PrimitiveComponent->IsVisible())
		{
			return true;
		}
	}

	return false;
}

void ARaidBoss::Multicast_PlayBossDamagedVisual_Implementation(float Damage, float OldHP, float NewHP, AActor* DamageCauser)
{
	PlayBossDamagedVisualLocally(Damage, OldHP, NewHP, DamageCauser);
}

void ARaidBoss::PlayBossDamagedVisualLocally(float Damage, float OldHP, float NewHP, AActor* DamageCauser)
{
	RefreshPrototypeVisualHPText();
	BP_OnBossDamagedVisual(Damage, OldHP, NewHP, DamageCauser);
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		DrawDebugSphere(World, GetActorLocation(), PrototypeVisualRadiusCm + 16.0f, 16, FColor::Orange, false, 0.40f, 0, 4.0f);
	}
}

void ARaidBoss::BP_OnBossDamagedVisual_Implementation(float Damage, float OldHP, float NewHP, AActor* DamageCauser)
{
	(void)DamageCauser;
#if WITH_DEV_AUTOMATION_TESTS
	CombatVisualBossDamagedCountForTest++;
	LastCombatVisualBossDamageForTest = Damage;
	LastCombatVisualBossOldHPForTest = OldHP;
	LastCombatVisualBossNewHPForTest = NewHP;
#endif
}

#if WITH_DEV_AUTOMATION_TESTS
int32 ARaidBoss::GetCombatVisualBossDamagedCountForTest() const
{
	return CombatVisualBossDamagedCountForTest;
}

float ARaidBoss::GetLastCombatVisualBossDamageForTest() const
{
	return LastCombatVisualBossDamageForTest;
}

float ARaidBoss::GetLastCombatVisualBossOldHPForTest() const
{
	return LastCombatVisualBossOldHPForTest;
}

float ARaidBoss::GetLastCombatVisualBossNewHPForTest() const
{
	return LastCombatVisualBossNewHPForTest;
}

FString ARaidBoss::GetPrototypeVisualLabelTextForTest() const
{
	return PrototypeVisualLabel ? PrototypeVisualLabel->Text.ToString() : FString();
}

bool ARaidBoss::IsBossPatternTimerActiveForTest() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimerManager().IsTimerActive(BossPatternTimerHandle);
}

int32 ARaidBoss::GetBossPatternFireSequenceForTest() const
{
	return BossPatternFireSequence;
}

void ARaidBoss::FireBossPatternOnceForTest()
{
	HandleBossPatternTimerFiredForServer();
}
#endif

void ARaidBoss::ApplyPrototypeVisualSettings()
{
	const float SafeRadiusCm = FMath::Max(50.0f, PrototypeVisualRadiusCm);
	if (PrototypeVisualMesh)
	{
		constexpr float EngineBasicSphereRadiusCm = 50.0f;
		const float MeshScale = SafeRadiusCm / EngineBasicSphereRadiusCm;
		PrototypeVisualMesh->SetRelativeScale3D(FVector(MeshScale));
		PrototypeVisualMesh->SetRelativeLocation(FVector::ZeroVector);
		PrototypeVisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PrototypeVisualMesh->SetGenerateOverlapEvents(false);
	}

	if (PrototypeVisualLabel)
	{
		PrototypeVisualLabel->SetRelativeLocation(FVector(0.0f, 0.0f, SafeRadiusCm + 120.0f));
		PrototypeVisualLabel->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
		PrototypeVisualLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PrototypeVisualLabel->SetGenerateOverlapEvents(false);
		PrototypeVisualLabel->SetWorldSize(FMath::Clamp(SafeRadiusCm * 0.55f, 80.0f, 180.0f));
	}
}

void ARaidBoss::RefreshPrototypeVisualHPText()
{
	if (!PrototypeVisualLabel)
	{
		return;
	}

	PrototypeVisualLabel->SetText(FText::FromString(FString::Printf(
		TEXT("Boss HP %.0f / %.0f"),
		CurrentHP,
		MaxHP)));
}

void ARaidBoss::OnRep_CurrentHP()
{
	RefreshPrototypeVisualHPText();
	UE_LOG(LogTemp, Log, TEXT("[Client] RaidBoss HP replicated: %.2f/%.2f"), CurrentHP, MaxHP);
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossHPReplicated: HP=%.2f MaxHP=%.2f"), CurrentHP, MaxHP);
}
