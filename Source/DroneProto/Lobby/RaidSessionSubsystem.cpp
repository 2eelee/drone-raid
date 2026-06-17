#include "RaidSessionSubsystem.h"
#include "LocalAssignment.h"
#include "Kismet/GameplayStatics.h"

void URaidSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// D11 교체 지점: 아래 한 줄을 다른 Assignment 구현체로 교체
	Assignment = NewObject<ULocalAssignment>(this);
}

void URaidSessionSubsystem::RequestRaidEntry(const FString& SlotId)
{
	if (!Assignment) return;

	// [확인1] 서버 리스트 조회 실패 (D11)
	//   if (!bServerListOk) { ShowNoServer(); return; }   // TODO

	const FServerEndpoint E = Assignment->ResolveServer(SlotId);

	// [확인2] 전 서버 만석 → 매칭 대기 (D11)
	//   ResolveServer가 지금 항상 A를 반환하므로 이 분기는 현재 미발동
	//   if (bNoServerAvailable) { ShowMatchmakingWait(); /* StartRetry */ return; }   // TODO

	// 정상 경로 — 현재 유일하게 동작
	UWorld* World = GetGameInstance()->GetWorld();
	if (!World) return;

	if (E.bIsLevelName)
	{
		UGameplayStatics::OpenLevel(World, FName(*E.TravelTarget));
	}
	else
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			PC->ClientTravel(E.TravelTarget, TRAVEL_Absolute);
		}
	}
}

bool URaidSessionSubsystem::IsSlotEnabled(const FString& SlotId) const
{
	// D11: 서버 목록 수신 후 동적 갱신으로 교체
	return SlotId == TEXT("A");
}

// ── 팝업 제어 ──────────────────────────────────────────────────────────────

UUserWidget* URaidSessionSubsystem::CreateAndShowPopup(TSubclassOf<UUserWidget> WidgetClass)
{
	if (!WidgetClass) return nullptr;
	UWorld* World = GetGameInstance()->GetWorld();
	if (!World) return nullptr;
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC) return nullptr;

	UUserWidget* Widget = CreateWidget<UUserWidget>(PC, WidgetClass);
	if (Widget) Widget->AddToViewport();
	return Widget;
}

void URaidSessionSubsystem::ShowMatchmakingWait()
{
	HideEntryPopups();
	ActiveMatchmakingWidget = CreateAndShowPopup(MatchmakingWaitWidgetClass);
}

void URaidSessionSubsystem::ShowNoServer()
{
	HideEntryPopups();
	ActiveNoServerWidget = CreateAndShowPopup(NoServerWidgetClass);
}

void URaidSessionSubsystem::ShowLoadFailed()
{
	HideEntryPopups();
	ActiveLoadFailedWidget = CreateAndShowPopup(LoadFailedWidgetClass);
}

void URaidSessionSubsystem::HideEntryPopups()
{
	if (ActiveMatchmakingWidget) { ActiveMatchmakingWidget->RemoveFromParent(); ActiveMatchmakingWidget = nullptr; }
	if (ActiveNoServerWidget)    { ActiveNoServerWidget->RemoveFromParent();    ActiveNoServerWidget    = nullptr; }
	if (ActiveLoadFailedWidget)  { ActiveLoadFailedWidget->RemoveFromParent();  ActiveLoadFailedWidget  = nullptr; }
}

void URaidSessionSubsystem::CancelMatchmaking()
{
	HideEntryPopups();

	UWorld* World = GetGameInstance()->GetWorld();
	if (World)
	{
		UGameplayStatics::OpenLevel(World, FName(TEXT("LobbyMap")));
	}
}
