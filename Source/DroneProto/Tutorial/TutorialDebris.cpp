#include "Tutorial/TutorialDebris.h"

#include "Components/SphereComponent.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "Tutorial/TutorialPlayerController.h"

ATutorialDebris::ATutorialDebris()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	CollisionComponent->InitSphereRadius(100.0f);
	CollisionComponent->SetCollisionProfileName(TEXT("Pawn"));
	RootComponent = CollisionComponent;
}

void ATutorialDebris::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || bTutorialDebrisDestroyed || !TargetPawn || DeltaSeconds <= 0.0f)
	{
		return;
	}

	const FVector Offset = TargetPawn->GetActorLocation() - GetActorLocation();
	const FVector Direction = FVector(Offset.X, Offset.Y, 0.0f).GetSafeNormal();
	if (!Direction.IsNearlyZero())
	{
		SetActorLocation(GetActorLocation() + Direction * ApproachSpeed * DeltaSeconds);
	}
}

bool ATutorialDebris::ApplyTutorialHitForServer(AController* InstigatorController)
{
	if (!HasAuthority() || bTutorialDebrisDestroyed)
	{
		return false;
	}

	ATutorialPlayerController* TutorialPC = Cast<ATutorialPlayerController>(InstigatorController);
	if (!TutorialPC || TutorialPC->GetCurrentTutorialStep() != ETutorialStep::DebrisCombat)
	{
		return false;
	}

	TutorialHitCount = FMath::Min(TutorialHitCount + 1, FMath::Max(1, RequiredHitCount));
	BP_OnTutorialDebrisHit(TutorialHitCount, RequiredHitCount);

	if (TutorialHitCount >= RequiredHitCount)
	{
		bTutorialDebrisDestroyed = true;
		SetActorEnableCollision(false);
		SetActorTickEnabled(false);
		TutorialPC->NotifyTutorialDebrisDestroyed();
		BP_OnTutorialDebrisDestroyed();
	}

	ForceNetUpdate();
	return true;
}

void ATutorialDebris::SetTargetPawnForServer(APawn* InTargetPawn)
{
	if (HasAuthority())
	{
		TargetPawn = InTargetPawn;
	}
}

void ATutorialDebris::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATutorialDebris, TutorialHitCount);
	DOREPLIFETIME(ATutorialDebris, bTutorialDebrisDestroyed);
}
