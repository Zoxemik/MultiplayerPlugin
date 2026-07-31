// Copyright (c) 2026 Zoxemik. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MultiplayerSessionsTypes.h"
#include "MultiplayerSessionListItem.generated.h"

UCLASS(BlueprintType)
class MULTIPLAYERSESSIONS_API UMultiplayerSessionListItem : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(const FMultiplayerSessionBrowserEntry& InBrowserEntry);

	const FMultiplayerSessionBrowserEntry& GetBrowserEntry() const
	{
		return BrowserEntry;
	}

private:
	UPROPERTY()
	FMultiplayerSessionBrowserEntry BrowserEntry;
};
