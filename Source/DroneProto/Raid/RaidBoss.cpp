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
	DOREPLIFETIME(ARaidBoss, CurrentHP);
}

void ARaidBoss::ApplyDamageForServer(float DamageAmount, AController* InstigatorController, AActor* DamageCauser)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Client] RaidBoss damage rejected: server authority required"));
		return;
	}

	const float ClampedDamage = FMath::Max(0.0f, DamageAmount);
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

	// TODO(D6): hook ContributionManager, DroneReport, VFX, and boss pattern reactions here.
}

float ARaidBoss::GetCurrentHP() const
{
	return CurrentHP;
}

float ARaidBoss::GetMaxHP() const
{
	return MaxHP;
}

void ARaidBoss::OnRep_CurrentHP()
{
	UE_LOG(LogTemp, Log, TEXT("[Client] RaidBoss HP replicated: %.2f/%.2f"), CurrentHP, MaxHP);
}
