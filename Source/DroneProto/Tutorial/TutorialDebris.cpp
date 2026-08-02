#include "Tutorial/TutorialDebris.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "Tutorial/TutorialPlayerController.h"
#include "UObject/ConstructorHelpers.h"

ATutorialDebris::ATutorialDebris()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	CollisionComponent->InitSphereRadius(100.0f);
	CollisionComponent->SetCollisionProfileName(TEXT("Pawn"));
	RootComponent = CollisionComponent;

	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(CollisionComponent);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMesh->SetGenerateOverlapEvents(false);
	VisualMesh->SetCanEverAffectNavigation(false);
	VisualMesh->SetRelativeScale3D(FVector(1.5f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DebrisMeshAsset(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (DebrisMeshAsset.Succeeded())
	{
		VisualMesh->SetStaticMesh(DebrisMeshAsset.Object);
	}
}

void ATutorialDebris::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || bTutorialDebrisDestroyed || DeltaSeconds <= 0.0f)
	{
		return;
	}

	if (bTutorialDebrisDestructionPending)
	{
		PendingDestructionTimeRemaining -= DeltaSeconds;
		if (PendingDestructionTimeRemaining <= 0.0f)
		{
			CompleteTutorialDestructionForServer();
		}
		return;
	}

	if (!TargetPawn)
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
	if (!HasAuthority() || bTutorialDebrisDestroyed || bTutorialDebrisDestructionPending)
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
		bTutorialDebrisDestructionPending = true;
		PendingDestructionController = TutorialPC;
		PendingDestructionTimeRemaining = DestructionDelaySeconds;
		if (PendingDestructionTimeRemaining <= 0.0f)
		{
			CompleteTutorialDestructionForServer();
		}
	}

	ForceNetUpdate();
	return true;
}

void ATutorialDebris::CompleteTutorialDestructionForServer()
{
	if (!HasAuthority() || bTutorialDebrisDestroyed || !bTutorialDebrisDestructionPending)
	{
		return;
	}

	bTutorialDebrisDestructionPending = false;
	bTutorialDebrisDestroyed = true;
	BP_OnTutorialDebrisDestroyed();
	ApplyDestroyedPresentation();
	if (PendingDestructionController)
	{
		PendingDestructionController->NotifyTutorialDebrisDestroyed();
	}
	PendingDestructionController = nullptr;
	ForceNetUpdate();
}

void ATutorialDebris::OnRep_TutorialHitCount()
{
	BP_OnTutorialDebrisHit(TutorialHitCount, RequiredHitCount);
}

void ATutorialDebris::OnRep_TutorialDebrisDestroyed()
{
	if (bTutorialDebrisDestroyed)
	{
		BP_OnTutorialDebrisDestroyed();
		ApplyDestroyedPresentation();
	}
}

void ATutorialDebris::ApplyDestroyedPresentation()
{
	SetActorEnableCollision(false);
	SetActorTickEnabled(false);
	SetActorHiddenInGame(true);
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
