#include "BalanceSandboxPlayerController.h"

#include "Balance/BalanceSandboxGameMode.h"
#include "Engine/World.h"
#include "Raid/DroneReportWidget.h"
#include "UObject/ConstructorHelpers.h"

ABalanceSandboxPlayerController::ABalanceSandboxPlayerController()
{
	// BalanceMap 직접 진입 부트스트랩. DefaultPawnClass 때와 같은 형태의 결손이다 —
	// DroneReportWidgetClass는 EditDefaultsOnly라 값이 BP_RaidPlayerController에만 들어 있고,
	// 샌드박스는 이 C++ 컨트롤러를 직접 쓰므로 null이 된다. 그러면 서버가 리포트를 정상 생성해
	// Client_ReceiveDroneReport까지 도달해도 ShowDroneReportWidget이 클래스 없음으로 조기 반환해
	// 사망 시 UI가 뜨지 않는다(그 뒤 REPORT 버튼은 dedup에 걸려 "이미 생성됨"을 돌려준다).
	//
	// 프로덕션이 쓰는 WBP_DroneReport를 그대로 재사용한다 — 샌드박스 전용 위젯을 만들지 않는다.
	// 이 값은 이 클래스의 기본값일 뿐이라 BP_RaidPlayerController의 설정에는 영향이 없다.
	static ConstructorHelpers::FClassFinder<UDroneReportWidget> DroneReportWidgetFinder(
		TEXT("/Game/WBP_DroneReport"));
	if (DroneReportWidgetFinder.Succeeded())
	{
		DroneReportWidgetClass = DroneReportWidgetFinder.Class;
	}
}

bool ABalanceSandboxPlayerController::ShouldAutoConfirmSelectionForServer() const
{
	return false;
}

bool ABalanceSandboxPlayerController::TryHandleDroneReportConfirmedForLocalPlayer(UDroneReportWidget* ReportWidget)
{
	// 기존 제거 경로를 그대로 탄다 — 컨트롤러가 캐시한 위젯 정리와 ReportWidgetHidden 로그가 여기 있다.
	HideDroneReportWidget();

	// 확인을 보낸 인스턴스도 확실히 닫는다. 뷰포트 제거와 확인 버튼 재무장은 위젯이 스스로 한다 —
	// 샌드박스가 UI 내부를 뒤지거나 위젯 에셋을 다시 찾지 않는다.
	if (ReportWidget)
	{
		ReportWidget->DismissReport();
	}

	// 리포트를 닫은 뒤 WBP_BalanceSandbox를 다시 조작할 수 있어야 한다.
	// HideDroneReportWidget은 부품 선택 UI가 떠 있지 않으면 GameOnly + 커서 숨김으로 되돌리는데,
	// 샌드박스에는 그 위젯이 없어 그대로 두면 샌드박스 패널을 클릭할 수 없다.
	SetShowMouseCursor(true);
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReportDismissed Reason=BalanceSandbox Player=%s Widget=%s"),
		*GetNameSafe(this),
		*GetNameSafe(ReportWidget));
	return true;
}

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
		SandboxGameMode->RunSandboxPatternForServer(PatternAlias);
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
