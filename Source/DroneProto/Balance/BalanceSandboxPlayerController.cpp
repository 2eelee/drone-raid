#include "BalanceSandboxPlayerController.h"

#include "Balance/BalanceSandboxGameMode.h"
#include "Engine/World.h"

ABalanceSandboxGameMode* ABalanceSandboxPlayerController::GetSandboxGameMode() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	ABalanceSandboxGameMode* SandboxGameMode = World->GetAuthGameMode<ABalanceSandboxGameMode>();
	if (!SandboxGameMode)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[DR_SUMMARY] SandboxCommand Result=Fail Reason=NotSandboxGameMode — 맵의 World Settings에서 GameMode를 BalanceSandboxGameMode로 지정해야 한다"));
	}

	return SandboxGameMode;
}

void ABalanceSandboxPlayerController::BalanceLoadout(FString CoreAlias, FString LeftWeaponAlias, FString RightWeaponAlias)
{
	if (ABalanceSandboxGameMode* SandboxGameMode = GetSandboxGameMode())
	{
		SandboxGameMode->ApplySandboxLoadoutForServer(CoreAlias, LeftWeaponAlias, RightWeaponAlias);
	}
}

void ABalanceSandboxPlayerController::BalanceStart()
{
	if (ABalanceSandboxGameMode* SandboxGameMode = GetSandboxGameMode())
	{
		SandboxGameMode->StartSandboxBattleForServer();
	}
}

void ABalanceSandboxPlayerController::BalanceReset()
{
	if (ABalanceSandboxGameMode* SandboxGameMode = GetSandboxGameMode())
	{
		SandboxGameMode->ResetSandboxRaidForServer();
	}
}

void ABalanceSandboxPlayerController::BalancePattern(FString PatternAlias)
{
	if (ABalanceSandboxGameMode* SandboxGameMode = GetSandboxGameMode())
	{
		SandboxGameMode->SetSandboxNextPatternForServer(PatternAlias);
	}
}

void ABalanceSandboxPlayerController::BalanceBossDamage(float DamageAmount)
{
	if (ABalanceSandboxGameMode* SandboxGameMode = GetSandboxGameMode())
	{
		SandboxGameMode->DamageSandboxBossForServer(DamageAmount);
	}
}

void ABalanceSandboxPlayerController::BalanceReport()
{
	if (ABalanceSandboxGameMode* SandboxGameMode = GetSandboxGameMode())
	{
		SandboxGameMode->CreateSandboxReportForServer();
	}
}
