#pragma once

#include "CoreMinimal.h"
#include "TutorialTypes.generated.h"

UENUM(BlueprintType)
enum class ETutorialStep : uint8
{
	None     UMETA(DisplayName = "None"),
	MoveLeft UMETA(DisplayName = "MoveLeft"),
	Attack   UMETA(DisplayName = "Attack"),
	Dodge    UMETA(DisplayName = "Dodge"),
	Complete UMETA(DisplayName = "Complete"),
};

inline const TCHAR* ToTutorialStepLogString(ETutorialStep Step)
{
	switch (Step)
	{
	case ETutorialStep::None:
		return TEXT("None");
	case ETutorialStep::MoveLeft:
		return TEXT("MoveLeft");
	case ETutorialStep::Attack:
		return TEXT("Attack");
	case ETutorialStep::Dodge:
		return TEXT("Dodge");
	case ETutorialStep::Complete:
		return TEXT("Complete");
	default:
		return TEXT("Unknown");
	}
}
