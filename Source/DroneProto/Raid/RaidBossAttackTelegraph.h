#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RaidBossAttackTelegraph.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class USceneComponent;

UCLASS(Blueprintable)
class DRONEPROTO_API ARaidBossAttackTelegraph : public AActor
{
	GENERATED_BODY()

public:
	ARaidBossAttackTelegraph();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Raid|Boss|Telegraph")
	void InitializeForServer(FVector InCenter, float InRadiusCm, float InTelegraphSeconds);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Raid|Boss|Telegraph")
	void MarkExpiredForServer();

	UFUNCTION(BlueprintPure, Category = "Raid|Boss|Telegraph")
	FVector GetCenter() const;

	UFUNCTION(BlueprintPure, Category = "Raid|Boss|Telegraph")
	float GetRadiusCm() const;

	UFUNCTION(BlueprintPure, Category = "Raid|Boss|Telegraph")
	float GetTelegraphSeconds() const;

	UFUNCTION(BlueprintPure, Category = "Raid|Boss|Telegraph")
	float GetRemainingSeconds() const;

	UFUNCTION(BlueprintNativeEvent, Category = "Raid|Boss|Telegraph|Visual")
	void BP_OnTelegraphStartedVisual(FVector VisualCenter, float VisualRadiusCm, float VisualDurationSeconds);

	UFUNCTION(BlueprintNativeEvent, Category = "Raid|Boss|Telegraph|Visual")
	void BP_OnTelegraphEndedVisual();

#if WITH_DEV_AUTOMATION_TESTS
	int32 GetCombatVisualTelegraphStartCountForTest() const;
#endif

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Raid|Boss|Telegraph", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Raid|Boss|Telegraph", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> DebugVisualMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Raid|Boss|Telegraph", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTextRenderComponent> DebugText = nullptr;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Raid|Boss|Telegraph", meta = (AllowPrivateAccess = "true"))
	FVector Center = FVector::ZeroVector;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Raid|Boss|Telegraph", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float RadiusCm = 0.0f;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Raid|Boss|Telegraph", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s"))
	float TelegraphSeconds = 0.0f;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Raid|Boss|Telegraph", meta = (AllowPrivateAccess = "true", Units = "s"))
	float StartServerTime = 0.0f;

#if WITH_DEV_AUTOMATION_TESTS
	int32 CombatVisualTelegraphStartCountForTest = 0;
#endif
};
