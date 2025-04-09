// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "MenuWidget.generated.h"

class UButton;
class UOverlay;
class UTextBlock;
class UEditableTextBox;
class UMultiplayerSessionsSubsystem;

/**
 * UMenuWidget handles the in-game menu, enabling players to Host or Join sessions
 * (including LAN sessions), transition to the lobby, and connect via direct IP.
 * It communicates with a custom UMultiplayerSessionsSubsystem to perform session operations.
 */

UCLASS()
class MULTIPLAYERSESSIONS_API UMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/**
	 * Setup the menu by adding it to the viewport, focusing on UI input, and configuring
	 * parameters for hosting/joining sessions.
	 *
	 * @param NumberOfPublicConnections The number of open spots for a session.
	 * @param TypeOfMatch The specific match type to host or find (e.g. "FreeForAll").
	 * @param LobbyPath The path to the lobby level, defaulting to "/Game/Levels/LobbyLevel".
	 */
	UFUNCTION(BlueprintCallable)
	void MenuSetup(int32 NumberOfPublicConnections = 4, FString TypeOfMatch = FString(TEXT("FreeForAll")), FString LobbyPath = FString(TEXT("/Game/Levels/LobbyLevel")));

protected:
	virtual bool Initialize() override;
	virtual void NativeDestruct() override;

	/**
	 * Callback triggered when creating a session completes.
	 *
	 * @param bWasSuccessful True if the session was created successfully, false otherwise.
	 */
	UFUNCTION()
	void OnCreateSession(bool bWasSuccessful);

	/**
	 * Callback triggered upon session search results being returned.
	 *
	 * @param SessionResults The array of found sessions matching the query.
	 * @param bWasSuccessful True if at least one session was found or query succeeded, false otherwise.
	 */
	void OnFindSessions(const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful);

	/**
	 * Callback triggered upon attempting to join a session.
	 *
	 * @param Result The result of the join attempt (Success, AlreadyInSession, etc.).
	 */
	void OnJoinSession(EOnJoinSessionCompleteResult::Type Result);

	/**
	 * Callback triggered when destroying a session completes.
	 *
	 * @param bWasSuccessful True if the session was destroyed successfully, false otherwise.
	 */
	UFUNCTION()
	void OnDestroySession(bool bWasSuccessful);

	/**
	 * Callback triggered when starting a session completes.
	 *
	 * @param bWasSuccessful True if the session was successfully started, false otherwise.
	 */
	UFUNCTION()
	void OnStartSession(bool bWasSuccessful);

private:
	UPROPERTY(meta = (BindWidget))
	UButton* HostButton;

	UPROPERTY(meta = (BindWidget))
	UButton* JoinButton;

	UPROPERTY(meta = (BindWidget))
	UButton* LANButton;

	UPROPERTY(meta = (BindWidget))
	UOverlay* LANOverlay;

	UPROPERTY(meta = (BindWidget))
	UEditableTextBox* IPEditBox;

	UPROPERTY(meta = (BindWidget))
	UButton* ConnectLanButton;

	UFUNCTION()
	void HostButtonClicked();
	UFUNCTION()
	void JoinButtonClicked();
	UFUNCTION()
	void LANButtonClicked();

	/**
	 * Reads the IP from IPEditBox and attempts to connect directly via ClientTravel.
	 */
	UFUNCTION()
	void ConnectLanToIP();

	/**
	 * Called when the menu is torn down. Removes it from the viewport, restores input to game-only mode.
	 */
	UFUNCTION()
	void MenuTeardown();

	/**
	 * Reference to the custom subsystem designed to handle all online session functionality
	 * (creating, finding, joining, destroying, starting, etc.).
	 */
	UMultiplayerSessionsSubsystem* MultiplayerSessionsSubsystem;

	int32 NumPublicConnections{4};
	FString MatchType{TEXT("FreeForAll")};
	FString PathToLobby{ TEXT("") };

};