// Copyright (c) 2026 Zoxemik. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MultiplayerSessionProfileProvider.generated.h"

UINTERFACE(BlueprintType)
class MULTIPLAYERSESSIONS_API UMultiplayerSessionProfileProvider : public UInterface
{
	GENERATED_BODY()
};

class MULTIPLAYERSESSIONS_API IMultiplayerSessionProfileProvider
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Multiplayer Sessions")
	FString GetMultiplayerSessionDisplayName() const;
};
