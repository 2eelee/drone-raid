#include "Drone.h"
#include "DronePart.h"
#include "DummyParts.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Components/SceneComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Engine/LocalPlayer.h"
#include "Net/UnrealNetwork.h"
#include "Engine/Engine.h"

ADrone::ADrone()
{
	bReplicates = true;
	// bReplicateMovement = true (APawn 기본값)
	// → 서버가 자신의 위치를 simulated proxy에 복제
	// TODO(D5 이전): FloatingPawnMovement는 CharacterMovement와 달리 클라 입력을
	// 서버에 전달하는 파이프라인이 없다. autonomous proxy는 로컬에서 움직이지만
	// 서버 Pawn은 제자리 → 서버가 제자리를 다시 복제 → rubber-banding 발생.
	// 2클라 PIE에서 이동 확인 후, Server RPC로 입력을 전달하는 방식을 결정할 것.

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
	if (HasAuthority())
	{
		ServerEquipPart(UCorePart::StaticClass());
		ServerEquipPart(ULeftWeaponPart::StaticClass());
		ServerEquipPart(URightWeaponPart::StaticClass());
	}
}

void ADrone::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (MoveAction)
			EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ADrone::Move);
	}
}

// autonomous proxy가 로컬에서 이동 (서버 권한 이동 없음 — TODO 참조)
void ADrone::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (!GetController())
		return;

	const FRotator YawRot(0.f, GetController()->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(YawRot).GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y), Axis.X);
}

float ADrone::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
                         AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority())
		return 0.f;

	const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	Health = FMath::Max(0, Health - FMath::RoundToInt(Applied));
	UE_LOG(LogTemp, Log, TEXT("[Server] TakeDamage: Health=%d"), Health);
	return Applied;
}

void ADrone::OnRep_Health()
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Cyan,
			FString::Printf(TEXT("[Client] Health=%d  MaxHealth=%d  AttackPower=%d"),
				Health, MaxHealth, AttackPower));
	}
}

void ADrone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADrone, Health);
	DOREPLIFETIME(ADrone, MaxHealth);
	DOREPLIFETIME(ADrone, AttackPower);
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
