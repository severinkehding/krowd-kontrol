#include "ReentrantTerminalListener.h"
#include "PlaceholderTerminalActor.h"

void UReentrantTerminalListener::HandleBarkTriggered(FName BarkID, TArray<FString> Lines)
{
	++CallCount;

	if (ActorToReenter)
	{
		ActorToReenter->Interact();
	}
}
