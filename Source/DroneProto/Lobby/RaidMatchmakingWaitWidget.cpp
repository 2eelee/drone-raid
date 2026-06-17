#include "RaidMatchmakingWaitWidget.h"
#include "RaidSessionSubsystem.h"

void URaidMatchmakingWaitWidget::OnCancelClicked()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (URaidSessionSubsystem* SS = GI->GetSubsystem<URaidSessionSubsystem>())
		{
			SS->CancelMatchmaking();
		}
	}
}
