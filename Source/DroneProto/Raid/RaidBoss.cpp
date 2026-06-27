#include "RaidBoss.h"

#include "Net/UnrealNetwork.h"

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
