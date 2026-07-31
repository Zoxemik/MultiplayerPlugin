// Copyright (c) 2026 Zoxemik. All rights reserved.

#include "MultiplayerSessionRowWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ListView.h"
#include "Components/TextBlock.h"
#include "MultiplayerSessionListItem.h"

bool UMultiplayerSessionRowWidget::Initialize()
{
	if (Super::Initialize() == false)
	{
		return false;
	}

	RowButton->OnClicked.AddDynamic(this, &ThisClass::HandleRowButtonClicked);
	SetSelectedVisual(false);
	return true;
}

void UMultiplayerSessionRowWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);

	CurrentListItem = Cast<UMultiplayerSessionListItem>(ListItemObject);
	RefreshFromListItem();
	SetSelectedVisual(IsListItemSelected());
}

void UMultiplayerSessionRowWidget::NativeOnItemSelectionChanged(bool bIsSelected)
{
	IUserObjectListEntry::NativeOnItemSelectionChanged(bIsSelected);
	SetSelectedVisual(bIsSelected);
}

void UMultiplayerSessionRowWidget::NativeOnEntryReleased()
{
	IUserObjectListEntry::NativeOnEntryReleased();
	CurrentListItem = nullptr;
	SetSelectedVisual(false);
}

void UMultiplayerSessionRowWidget::HandleRowButtonClicked()
{
	if (CurrentListItem == nullptr)
	{
		return;
	}

	UListView* OwningListView = Cast<UListView>(GetOwningListView());
	if (OwningListView == nullptr)
	{
		return;
	}

	OwningListView->SetSelectedItem(CurrentListItem);
}

void UMultiplayerSessionRowWidget::RefreshFromListItem()
{
	if (CurrentListItem == nullptr)
	{
		HostValueText->SetText(FText::GetEmpty());
		StatusValueText->SetText(FText::GetEmpty());
		FreeSlotsValueText->SetText(FText::GetEmpty());
		PingValueText->SetText(FText::GetEmpty());
		return;
	}

	const FMultiplayerSessionBrowserEntry& BrowserEntry = CurrentListItem->GetBrowserEntry();
	HostValueText->SetText(FText::FromString(BrowserEntry.HostDisplayName));
	StatusValueText->SetText(FText::FromString(BrowserEntry.StatusText));
	FreeSlotsValueText->SetText(FText::AsNumber(FMath::Max(0, BrowserEntry.OpenPublicConnections)));

	if (BrowserEntry.PingInMs >= 0)
	{
		PingValueText->SetText(FText::Format(NSLOCTEXT("MultiplayerSessions", "PingFormat", "{0} ms"), FText::AsNumber(BrowserEntry.PingInMs)));
	}
	else
	{
		PingValueText->SetText(NSLOCTEXT("MultiplayerSessions", "UnknownPing", "--"));
	}
}

void UMultiplayerSessionRowWidget::SetSelectedVisual(bool bInSelected)
{
	ESlateVisibility SelectionVisibility = ESlateVisibility::Hidden;
	if (bInSelected == true)
	{
		SelectionVisibility = ESlateVisibility::Visible;
	}

	SelectionBorder->SetVisibility(SelectionVisibility);
}
