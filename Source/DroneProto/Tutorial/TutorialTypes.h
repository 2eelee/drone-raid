#pragma once

#include "CoreMinimal.h"
#include "TutorialTypes.generated.h"

UENUM(BlueprintType)
enum class ETutorialStep : uint8
{
	None            UMETA(DisplayName = "None"),
	Start           UMETA(DisplayName = "1. Start"),
	WorldBriefing   UMETA(DisplayName = "2. World Briefing"),
	Move            UMETA(DisplayName = "3. Move"),
	Attack          UMETA(DisplayName = "4. Attack"),
	Dodge           UMETA(DisplayName = "5. Dodge"),
	CombatBriefing  UMETA(DisplayName = "6. Combat Briefing"),
	DebrisCombat    UMETA(DisplayName = "7. Debris Combat"),
	ClosingBriefing UMETA(DisplayName = "8. Closing Briefing"),
	Complete        UMETA(DisplayName = "Complete"),
};

inline const TCHAR* ToTutorialStepLogString(ETutorialStep Step)
{
	switch (Step)
	{
	case ETutorialStep::None:
		return TEXT("None");
	case ETutorialStep::Start:
		return TEXT("Start");
	case ETutorialStep::WorldBriefing:
		return TEXT("WorldBriefing");
	case ETutorialStep::Move:
		return TEXT("Move");
	case ETutorialStep::Attack:
		return TEXT("Attack");
	case ETutorialStep::Dodge:
		return TEXT("Dodge");
	case ETutorialStep::CombatBriefing:
		return TEXT("CombatBriefing");
	case ETutorialStep::DebrisCombat:
		return TEXT("DebrisCombat");
	case ETutorialStep::ClosingBriefing:
		return TEXT("ClosingBriefing");
	case ETutorialStep::Complete:
		return TEXT("Complete");
	default:
		return TEXT("Unknown");
	}
}
