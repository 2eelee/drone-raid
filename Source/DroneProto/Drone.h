#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "Drone.generated.h"

class UFloatingPawnMovement;
class UInputMappingContext;
class UInputAction;
class UDronePart;

UCLASS()
class DRONEPROTO_API ADrone : public APawn
{
	GENERATED_BODY()

public:
	ADrone();

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
	                         AController* EventInstigator, AActor* DamageCauser) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(VisibleAnywhere)
	UFloatingPawnMovement* FloatingMovement;

	// D6~8 발사 위치용 — 지금은 배치만
	UPROPERTY(VisibleAnywhere)
	USceneComponent* MuzzlePoint;

	// ---- Replicated Stats (부품 합산 결과만 복제) ----
	UPROPERTY(ReplicatedUsing = OnRep_Health)
	int32 Health = 100;

	UPROPERTY(Replicated)
	int32 MaxHealth = 100;

	UPROPERTY(Replicated)
	int32 AttackPower = 0;

	UFUNCTION()
	void OnRep_Health();

	// ---- Enhanced Input ----
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputMappingContext* DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* MoveAction;

	void Move(const FInputActionValue& Value);

	// ---- 장착 부품 (서버 전용, 복제 안 함) ----
	TArray<UDronePart*> EquippedParts;

	void ServerEquipPart(TSubclassOf<UDronePart> PartClass);
	void RecalculateStats();
};
