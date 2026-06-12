#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "RaidGameState.generated.h"

UENUM(BlueprintType)
enum class ERaidState : uint8
{
	Waiting  UMETA(DisplayName = "Waiting"),
	Drafting UMETA(DisplayName = "Drafting"),
	Battle   UMETA(DisplayName = "Battle"),
	End      UMETA(DisplayName = "End"),
};

UCLASS()
class DRONEPROTO_API ARaidGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ARaidGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(ReplicatedUsing = OnRep_RaidState, BlueprintReadOnly, Category = "Raid")
	ERaidState RaidState;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Raid")
	int32 CurrentPlayers;

private:
	UFUNCTION()
	void OnRep_RaidState();
};
