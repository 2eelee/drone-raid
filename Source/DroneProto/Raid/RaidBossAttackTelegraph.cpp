#include "RaidBossAttackTelegraph.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ARaidBossAttackTelegraph::ARaidBossAttackTelegraph()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	NetDormancy = DORM_Never;
	SetReplicatingMovement(false);
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("TelegraphRoot"));
	SetRootComponent(SceneRoot);

	DebugVisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DebugVisualMesh"));
	DebugVisualMesh->SetupAttachment(SceneRoot);
	DebugVisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DebugVisualMesh->SetGenerateOverlapEvents(false);
	DebugVisualMesh->SetCanEverAffectNavigation(false);
	DebugVisualMesh->SetHiddenInGame(false);
	DebugVisualMesh->CastShadow = false;
	DebugVisualMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 2.0f));
	DebugVisualMesh->SetRelativeScale3D(FVector(2.0f, 2.0f, 0.02f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		DebugVisualMesh->SetStaticMesh(CylinderMesh.Object);
	}

	DebugText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("DebugText"));
	DebugText->SetupAttachment(SceneRoot);
	DebugText->SetHiddenInGame(false);
	DebugText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DebugText->SetGenerateOverlapEvents(false);
	DebugText->SetCanEverAffectNavigation(false);
	DebugText->CastShadow = false;
	DebugText->SetRelativeLocation(FVector(0.0f, 0.0f, 80.0f));
	DebugText->SetWorldSize(32.0f);
	DebugText->SetTextRenderColor(FColor::Red);
	DebugText->SetText(FText::FromString(TEXT("BOSS ATTACK")));
}

void ARaidBossAttackTelegraph::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARaidBossAttackTelegraph, Center);
	DOREPLIFETIME(ARaidBossAttackTelegraph, RadiusCm);
	DOREPLIFETIME(ARaidBossAttackTelegraph, TelegraphSeconds);
	DOREPLIFETIME(ARaidBossAttackTelegraph, StartServerTime);
}

void ARaidBossAttackTelegraph::InitializeForServer(FVector InCenter, float InRadiusCm, float InTelegraphSeconds)
{
	if (!HasAuthority())
	{
		return;
	}

	Center = InCenter;
	RadiusCm = FMath::Max(0.0f, InRadiusCm);
	TelegraphSeconds = FMath::Max(0.0f, InTelegraphSeconds);
	StartServerTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	SetActorLocation(Center);
	const float RadiusScale = FMath::Max(0.01f, RadiusCm / 100.0f);
	SetActorScale3D(FVector(RadiusScale, RadiusScale, 1.0f));
	if (DebugText)
	{
		DebugText->SetText(FText::FromString(FString::Printf(TEXT("BOSS ATTACK\n%.0fcm"), RadiusCm)));
	}
	SetLifeSpan(TelegraphSeconds + 1.0f);
	ForceNetUpdate();
}

void ARaidBossAttackTelegraph::MarkExpiredForServer()
{
	if (!HasAuthority())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossTelegraphExpired Actor=%s"),
		*GetName());
	Destroy();
}

FVector ARaidBossAttackTelegraph::GetCenter() const
{
	return Center;
}

float ARaidBossAttackTelegraph::GetRadiusCm() const
{
	return RadiusCm;
}

float ARaidBossAttackTelegraph::GetTelegraphSeconds() const
{
	return TelegraphSeconds;
}

float ARaidBossAttackTelegraph::GetRemainingSeconds() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return TelegraphSeconds;
	}

	return FMath::Max(0.0f, TelegraphSeconds - (World->GetTimeSeconds() - StartServerTime));
}
