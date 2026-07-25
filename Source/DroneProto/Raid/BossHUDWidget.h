#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BossHUDWidget.generated.h"

class ARaidBoss;
class ARaidGameState;
class ARaidPlayerController;
class UProgressBar;
class UTextBlock;

UCLASS()
class DRONEPROTO_API UBossHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Drone|BossHUD")
	float GetBossHPPercent() const;

	UFUNCTION(BlueprintPure, Category = "Drone|BossHUD")
	FText GetBossHPText() const;

	UFUNCTION(BlueprintPure, Category = "Drone|BossHUD")
	float GetRaidRemainingSeconds() const;

	UFUNCTION(BlueprintPure, Category = "Drone|BossHUD")
	FText GetRaidTimerText() const;

	UFUNCTION(BlueprintCallable, Category = "Drone|BossHUD")
	void RefreshBossHUD();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Drone|BossHUD")
	TObjectPtr<UTextBlock> BossHPText = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Drone|BossHUD")
	TObjectPtr<UTextBlock> RaidTimerText = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Drone|BossHUD")
	TObjectPtr<UProgressBar> BossHPProgressBar = nullptr;

private:
	ARaidGameState* GetObservedRaidGameState() const;
	ARaidBoss* FindObservedBoss() const;
	ARaidGameState* GetRaidGameState() const;
	ARaidBoss* GetRaidBoss() const;
	ARaidPlayerController* GetOwningRaidPlayerController() const;

	void RefreshBossHUDWithReason(FName Reason);
	static FText BuildTimerText(float RemainingSeconds);
	static void SetOptionalText(UTextBlock* TextBlock, const FText& Text);

	float BossHUDRefreshAccumulatorSeconds = 0.0f;
};
