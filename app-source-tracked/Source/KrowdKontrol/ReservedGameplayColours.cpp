#include "ReservedGameplayColours.h"

FLinearColor ReservedGameplayColours::GetPurple()
{
	return FLinearColor(0.5f, 0.0f, 1.0f, 1.0f);
}

FLinearColor ReservedGameplayColours::GetTeal()
{
	return FLinearColor(0.0f, 0.8f, 0.8f, 1.0f);
}

FLinearColor ReservedGameplayColours::GetOrange()
{
	return FLinearColor(1.0f, 0.5f, 0.0f, 1.0f);
}

FLinearColor ReservedGameplayColours::GetBlue()
{
	return FLinearColor(0.0f, 0.4f, 1.0f, 1.0f);
}

FLinearColor ReservedGameplayColours::GetWhite()
{
	return FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
}

TArray<FLinearColor> ReservedGameplayColours::GetAll()
{
	return { GetPurple(), GetTeal(), GetOrange(), GetBlue(), GetWhite() };
}

FLinearColor ReservedGameplayColours::GetBackground()
{
	return FLinearColor(0.02f, 0.02f, 0.03f, 1.0f);
}
