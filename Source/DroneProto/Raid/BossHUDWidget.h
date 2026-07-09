#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BossHUDWidget.generated.h"

class ARaidBoss;
class ARaidGameState;
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

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Drone|BossHUD")
	TObjectPtr<UTextBlock> BossHPText = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Drone|BossHUD")
	TObjectPtr<UTextBlock> RaidTimerText = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Drone|BossHUD")
	TObjectPtr<UProgressBar> BossHPProgressBar = nullptr;

private:
	ARaidGameState* GetRaidGameState() const;
	ARaidBoss* GetRaidBoss() const;

	static FText BuildHPText(const ARaidBoss* Boss);
	static FText BuildTimerText(float RemainingSeconds);
	static void SetOptionalText(UTextBlock* TextBlock, const FText& Text);
};
