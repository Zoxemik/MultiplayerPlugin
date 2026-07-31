// Copyright (c) 2026 Zoxemik. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MultiplayerSessionsTypes.h"
#include "MultiplayerEntryWidget.generated.h"

class UButton;
class UCheckBox;
class UEditableTextBox;
class UListView;
class UMultiplayerSessionListItem;
class UMultiplayerSessionsSubsystem;
class UTextBlock;
class UWidget;

DECLARE_MULTICAST_DELEGATE(FOnMultiplayerBackRequested);

UCLASS()
class MULTIPLAYERSESSIONS_API UMultiplayerEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	void MenuSetup(int32 NumberOfPublicConnections, const FString& TypeOfMatch, const FString& InLobbyMapPath, const FString& InMainMenuMapPath);

	FOnMultiplayerBackRequested& OnBackRequested()
	{
		return BackRequestedEvent;
	}

	UFUNCTION(BlueprintCallable)
	void DebugPopulateFakeSessions();

protected:
	virtual bool Initialize() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleHostButtonClicked();

	UFUNCTION()
	void HandleRefreshButtonClicked();

	UFUNCTION()
	void HandleJoinSelectedButtonClicked();

	UFUNCTION()
	void HandleJoinByIpButtonClicked();

	UFUNCTION()
	void HandleBackButtonClicked();

	UFUNCTION()
	void HandleLanModeCheckStateChanged(bool bIsChecked);

	UFUNCTION()
	void HandleCreateSessionCompleted(bool bWasSuccessful);

	UFUNCTION()
	void HandleSessionSearchCompleted(bool bWasSuccessful, const TArray<FMultiplayerSessionBrowserEntry>& BrowserEntries);

	UFUNCTION()
	void HandleJoinSessionRequestCompleted(EMultiplayerJoinSessionResult Result);

	UFUNCTION()
	void HandleTravelRequestCompleted(bool bWasSuccessful);

	UFUNCTION()
	void HandleSessionFailure(EMultiplayerSessionFailureReason FailureReason);

	void HandleSessionListSelectionChanged(UObject* SelectedItem);
	void BindSubsystemDelegates();
	void UnbindSubsystemDelegates();
	void ResolveSubsystemMode();
	void RequestCreateSession();
	void RequestFindSessions();
	void BuildSessionListItems(const TArray<FMultiplayerSessionBrowserEntry>& BrowserEntries);
	void ClearSessionListItems();
	void SetBusyState(bool bInIsBusy);
	void SetStatusText(const FString& InStatusText);
	void UpdateJoinButtonState();
	void UpdateConnectionModeText();

	bool ShouldUseLanMode() const;
	FString ResolveLocalNicknameForSession() const;
	FString ResolvePendingNicknameFromGameInstance() const;
	FString BuildSessionDisplayName(const FString& InHostNickname) const;
	FString BuildSelectedSessionStatusText(const FMultiplayerSessionBrowserEntry& BrowserEntry) const;
	FString BuildCurrentModeStatusText() const;
	FString BuildCurrentModeLabelText() const;
	FString BuildFailureMessage(EMultiplayerSessionFailureReason FailureReason) const;
	const FMultiplayerSessionBrowserEntry* GetSelectedBrowserEntry() const;

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> HostButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> RefreshButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> JoinSelectedButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> JoinByIpButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BackButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCheckBox> LanModeCheckBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> DirectIpTextBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UListView> SessionsListView;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ConnectionModeText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> SessionsHeaderContainer;

	UPROPERTY(EditDefaultsOnly, Category = "Multiplayer")
	int32 SessionBuildId = 0;

	UPROPERTY(Transient)
	TObjectPtr<UMultiplayerSessionsSubsystem> MultiplayerSessionsSubsystem;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMultiplayerSessionListItem>> SessionListItems;

	int32 NumPublicConnections = 10;
	FString MatchType = TEXT("TowerOnline");
	FString LobbyMapPath = TEXT("/Game/Levels/LobbyLevel");
	FString MainMenuMapPath = TEXT("/Game/Levels/MainMenuLevel");
	bool bForceLanMode = false;
	bool bAutoFallbackToLan = false;
	bool bIsBusy = false;
	FString ActiveSubsystemName = TEXT("Unknown");
	FOnMultiplayerBackRequested BackRequestedEvent;
};
