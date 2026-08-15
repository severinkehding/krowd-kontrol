#include "HerdableTestActor.h"

void AHerdableTestActor::SetControlled(bool bNewControlled)
{
	bIsControlled = bNewControlled;
}

void AHerdableTestActor::SetHerdColourTag(FName NewColourTag)
{
	HerdColourTag = NewColourTag;
}

bool AHerdableTestActor::IsControlled() const
{
	return bIsControlled;
}

FName AHerdableTestActor::GetHerdColourTag() const
{
	return HerdColourTag;
}
