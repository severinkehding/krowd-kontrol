#include "GizmoBarkTestListener.h"

void UGizmoBarkTestListener::HandleBarkTriggered(FName BarkID, TArray<FString> Lines)
{
	++CallCount;
	LastBarkID = BarkID;
	LastLines = Lines;
}
