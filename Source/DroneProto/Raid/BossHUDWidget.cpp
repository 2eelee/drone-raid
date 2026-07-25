#include "BossHUDWidget.h"

#include "RaidBoss.h"
#include "RaidGameState.h"
#include "RaidPlayerController.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
constexpr float BossHUDRefreshIntervalSeconds = 0.20f;
}

void UBossHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BossHUDRefreshAccumulatorSeconds = 0.0f;
	RefreshBossHUDWithReason(FName(TEXT("Construct")));
}

void UBossHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!IsInViewport())
	{
		return;
	}

	BossHUDRefreshAccumulatorSeconds += FMath::Max(0.0f, InDeltaTime);
	if (BossHUDRefreshAccumulatorSeconds < BossHUDRefreshIntervalSeconds)
	{
		return;
	}

	BossHUDRefreshAccumulatorSeconds = 0.0f;
	RefreshBossHUDWithReason(FName(TEXT("Tick")));
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
	return FText::GetEmpty();
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
	RefreshBossHUDWithReason(FName(TEXT("Manual")));
}

void UBossHUDWidget::RefreshBossHUDWithReason(FName Reason)
{
	const float HPPercent = GetBossHPPercent();
	const FText HPText = GetBossHPText();
	const FText TimerText = GetRaidTimerText();
	const float RemainingSeconds = GetRaidRemainingSeconds();

	if (BossHPProgressBar)
	{
		BossHPProgressBar->SetPercent(HPPercent);
	}

	SetOptionalText(BossHPText, HPText);
	SetOptionalText(RaidTimerText, TimerText);

	const ARaidGameState* RaidGameState = GetObservedRaidGameState();
	const ARaidBoss* Boss = FindObservedBoss();
	if (!RaidGameState)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] UIRefresh BossHUDSkipped Reason=NoGameState RefreshReason=%s HPPercent=%.3f Timer=%.2f"),
			Reason.IsNone() ? TEXT("Unknown") : *Reason.ToString(),
			HPPercent,
			RemainingSeconds);
		return;
	}

	if (!Boss)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] UIRefresh BossHUDSkipped Reason=NoBoss RefreshReason=%s HPPercent=%.3f Timer=%.2f"),
			Reason.IsNone() ? TEXT("Unknown") : *Reason.ToString(),
			HPPercent,
			RemainingSeconds);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] UIRefresh BossHUD Reason=%s HPPercent=%.3f Timer=%.2f Boss=%s"),
		Reason.IsNone() ? TEXT("Unknown") : *Reason.ToString(),
		HPPercent,
		RemainingSeconds,
		*Boss->GetName());
}

ARaidGameState* UBossHUDWidget::GetObservedRaidGameState() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		if (const ARaidPlayerController* RaidPC = GetOwningRaidPlayerController())
		{
			World = RaidPC->GetWorld();
		}
	}

	if (!World)
	{
		World = GetTypedOuter<UWorld>();
	}

	return World ? World->GetGameState<ARaidGameState>() : nullptr;
}

ARaidBoss* UBossHUDWidget::FindObservedBoss() const
{
	if (const ARaidPlayerController* RaidPC = GetOwningRaidPlayerController())
	{
		if (ARaidBoss* TargetBoss = RaidPC->GetCurrentTargetBoss())
		{
			return TargetBoss;
		}
	}

	if (const ARaidGameState* RaidGameState = GetObservedRaidGameState())
	{
		if (ARaidBoss* RaidBoss = RaidGameState->GetRaidBoss())
		{
			return RaidBoss;
		}
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		World = GetTypedOuter<UWorld>();
	}

	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<ARaidBoss> It(World); It; ++It)
	{
		ARaidBoss* Boss = *It;
		if (IsValid(Boss))
		{
			return Boss;
		}
	}

	return nullptr;
}

ARaidGameState* UBossHUDWidget::GetRaidGameState() const
{
	return GetObservedRaidGameState();
}

ARaidBoss* UBossHUDWidget::GetRaidBoss() const
{
	return FindObservedBoss();
}

ARaidPlayerController* UBossHUDWidget::GetOwningRaidPlayerController() const
{
	if (ARaidPlayerController* RaidPC = Cast<ARaidPlayerController>(GetOwningPlayer()))
	{
		return RaidPC;
	}

	return GetTypedOuter<ARaidPlayerController>();
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
