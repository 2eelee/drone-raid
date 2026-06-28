#include "RaidBoss.h"

#include "Drone.h"
#include "RaidGameState.h"
#include "RaidPlayerController.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

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
}

void ARaidBoss::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		CurrentHP = MaxHP;
		ForceNetUpdate();
	}
}

void ARaidBoss::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARaidBoss, MaxHP);
	DOREPLIFETIME(ARaidBoss, CurrentHP);
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
	if (IsDefeated())
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossDamageIgnored: Reason=BossDead HP=%.2f Damage=%.2f Instigator=%s Causer=%s"),
			CurrentHP,
			ClampedDamage,
			InstigatorController ? *InstigatorController->GetName() : TEXT("None"),
			DamageCauser ? *DamageCauser->GetName() : TEXT("None"));
		return;
	}

	const float PreviousHP = CurrentHP;
	CurrentHP = FMath::Clamp(CurrentHP - ClampedDamage, 0.0f, MaxHP);
	ForceNetUpdate();

	UE_LOG(LogTemp, Log, TEXT("[Server] RaidBoss DamageApplied: Damage=%.2f HP=%.2f/%.2f PreviousHP=%.2f Instigator=%s Causer=%s"),
		ClampedDamage,
		CurrentHP,
		MaxHP,
		PreviousHP,
		InstigatorController ? *InstigatorController->GetName() : TEXT("None"),
		DamageCauser ? *DamageCauser->GetName() : TEXT("None"));

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossDamage: OldHP=%.2f Damage=%.2f NewHP=%.2f MaxHP=%.2f Instigator=%s Causer=%s"),
		PreviousHP,
		ClampedDamage,
		CurrentHP,
		MaxHP,
		InstigatorController ? *InstigatorController->GetName() : TEXT("None"),
		DamageCauser ? *DamageCauser->GetName() : TEXT("None"));

	if (PreviousHP > 0.0f && CurrentHP <= 0.0f)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossDeath: OldHP=%.2f Damage=%.2f Instigator=%s Causer=%s"),
			PreviousHP,
			ClampedDamage,
			InstigatorController ? *InstigatorController->GetName() : TEXT("None"),
			DamageCauser ? *DamageCauser->GetName() : TEXT("None"));
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

void ARaidBoss::OnRep_CurrentHP()
{
	UE_LOG(LogTemp, Log, TEXT("[Client] RaidBoss HP replicated: %.2f/%.2f"), CurrentHP, MaxHP);
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossHPReplicated: HP=%.2f MaxHP=%.2f"), CurrentHP, MaxHP);
}
