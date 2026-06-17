#include "RaidNoServerWidget.h"
#include "RaidSessionSubsystem.h"

void URaidNoServerWidget::OnConfirmClicked()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (URaidSessionSubsystem* SS = GI->GetSubsystem<URaidSessionSubsystem>())
		{
			SS->CancelMatchmaking();
		}
	}
}
