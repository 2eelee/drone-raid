#include "RaidLobbyWidget.h"
#include "RaidSessionSubsystem.h"

void URaidLobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UGameInstance* GI = GetGameInstance())
	{
		RaidSubsystem = GI->GetSubsystem<URaidSessionSubsystem>();
	}
}

void URaidLobbyWidget::RequestEntry(const FString& SlotId)
{
	if (RaidSubsystem)
	{
		RaidSubsystem->RequestRaidEntry(SlotId);
	}
}

bool URaidLobbyWidget::IsSlotEnabled(const FString& SlotId) const
{
	return RaidSubsystem ? RaidSubsystem->IsSlotEnabled(SlotId) : false;
}
