#include "DummyParts.h"

// base(100) + Core(100) + Left(0) + Right(20) = MaxHealth 220
// base(0)   + Core(0)   + Left(20) + Right(15) = AttackPower 35

UCorePart::UCorePart()
{
	Slot = EPartSlot::Core;
	StatContribution.HealthBonus = 100;
	StatContribution.AttackBonus = 0;
}

ULeftWeaponPart::ULeftWeaponPart()
{
	Slot = EPartSlot::LeftWeapon;
	StatContribution.HealthBonus = 0;
	StatContribution.AttackBonus = 20;
}

URightWeaponPart::URightWeaponPart()
{
	Slot = EPartSlot::RightWeapon;
	StatContribution.HealthBonus = 20;
	StatContribution.AttackBonus = 15;
}
