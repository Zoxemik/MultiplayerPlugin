// Copyright (c) 2026 Zoxemik. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "MultiplayerSessionRowWidget.generated.h"

class UBorder;
class UButton;
class UTextBlock;
class UMultiplayerSessionListItem;

UCLASS()
class MULTIPLAYERSESSIONS_API UMultiplayerSessionRowWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

protected:
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	virtual void NativeOnItemSelectionChanged(bool bIsSelected) override;
	virtual void NativeOnEntryReleased() override;

private:
	UFUNCTION()
	void HandleRowButtonClicked();

	void RefreshFromListItem();
	void SetSelectedVisual(bool bInSelected);

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> RowButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> SelectionBorder;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> HostValueText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> StatusValueText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> FreeSlotsValueText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PingValueText;

	UPROPERTY(Transient)
	TObjectPtr<UMultiplayerSessionListItem> CurrentListItem;
};
