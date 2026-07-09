#include "BossHUDWidget.h"

#include "RaidBoss.h"
#include "RaidGameState.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"

void UBossHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshBossHUD();
}

float UBossHUDWidget::GetBossHPPercent() const
{
	const ARaidBoss* Boss = GetRaidBoss();
	if (!Boss || Boss->GetMaxHP() <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	return FMath::Clamp(Boss->GetCurrentHP() / Boss->GetMaxHP(), 0.0f, 1.0f);
}

FText UBossHUDWidget::GetBossHPText() const
{
	return BuildHPText(GetRaidBoss());
}

float UBossHUDWidget::GetRaidRemainingSeconds() const
{
	const ARaidGameState* RaidGameState = GetRaidGameState();
	return RaidGameState ? RaidGameState->GetRaidRemainingSeconds() : 0.0f;
}

FText UBossHUDWidget::GetRaidTimerText() const
{
	return BuildTimerText(GetRaidRemainingSeconds());
}

void UBossHUDWidget::RefreshBossHUD()
{
	const float HPPercent = GetBossHPPercent();
	const FText HPText = GetBossHPText();
	const FText TimerText = GetRaidTimerText();

	if (BossHPProgressBar)
	{
		BossHPProgressBar->SetPercent(HPPercent);
	}

	SetOptionalText(BossHPText, HPText);
	SetOptionalText(RaidTimerText, TimerText);

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] UIRefresh Source=BossHUDWidget BossHPPercent=%.3f RaidRemaining=%.2f"),
		HPPercent,
		GetRaidRemainingSeconds());
}

ARaidGameState* UBossHUDWidget::GetRaidGameState() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		World = GetTypedOuter<UWorld>();
	}

	return World ? World->GetGameState<ARaidGameState>() : nullptr;
}

ARaidBoss* UBossHUDWidget::GetRaidBoss() const
{
	const ARaidGameState* RaidGameState = GetRaidGameState();
	return RaidGameState ? RaidGameState->GetRaidBoss() : nullptr;
}

FText UBossHUDWidget::BuildHPText(const ARaidBoss* Boss)
{
	if (!Boss || Boss->GetMaxHP() <= KINDA_SMALL_NUMBER)
	{
		return FText::FromString(TEXT("0 / 0"));
	}

	const int32 CurrentHP = FMath::Max(0, FMath::RoundToInt(Boss->GetCurrentHP()));
	const int32 MaxHP = FMath::Max(0, FMath::RoundToInt(Boss->GetMaxHP()));
	return FText::FromString(FString::Printf(TEXT("%d / %d"), CurrentHP, MaxHP));
}

FText UBossHUDWidget::BuildTimerText(float RemainingSeconds)
{
	const int32 ClampedSeconds = FMath::Clamp(FMath::RoundToInt(RemainingSeconds), 0, 180);
	const int32 Minutes = ClampedSeconds / 60;
	const int32 Seconds = ClampedSeconds % 60;
	return FText::FromString(FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds));
}

void UBossHUDWidget::SetOptionalText(UTextBlock* TextBlock, const FText& Text)
{
	if (TextBlock)
	{
		TextBlock->SetText(Text);
	}
}
