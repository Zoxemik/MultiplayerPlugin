// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Interfaces/OnlineFriendsInterface.h"
#include "MultiplayerSessionsSubsystem.generated.h"

/**
 * Declaring custom delegates for communication with the menu class or other listeners.
 * These delegates notify about session creation, session finding, join attempts, session
 * destruction, session start, and invite reception results.
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnCreateSessionComplete, bool, bWasSuccessful);
DECLARE_MULTICAST_DELEGATE_TwoParams(FMultiplayerOnFindSessionsComplete, const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful);
DECLARE_MULTICAST_DELEGATE_OneParam(FMultiplayerOnJoinSessionComplete, EOnJoinSessionCompleteResult::Type Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnDestroySessionComplete, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnStartSessionComplete, bool, bWasSuccessful);
DECLARE_MULTICAST_DELEGATE_TwoParams(FMultiplayerOnInviteReceiveComplete, const FOnlineSessionSearchResult& InviteResult, const FUniqueNetId& FriendInviting);

/**
 * UMultiplayerSessionsSubsystem is a GameInstanceSubsystem responsible for handling session-related
 * operations such as creating, finding, joining, destroying, and starting sessions. It also manages
 * friend invites and provides custom delegates to broadcast results back to the UI or other classes.
 */

UCLASS()
class UMultiplayerSessionsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	UMultiplayerSessionsSubsystem();

	/**
	 * Called when the subsystem is being initialized. Sets up any required delegates on session interfaces
	 * (e.g., handling invites, reading friends list).
	 *
	 * @param Collection A collection of other subsystems that may be initialized alongside this one.
	 */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * Creates a new online session (or LAN session) with the given number of connections and match type.
	 * If an existing session is found, it will be destroyed first before the new one is created.
	 *
	 * @param NumPublicConnections Number of open public connections in the session.
	 * @param MatchType A string defining the type of match (e.g., "FreeForAll"), used to filter sessions.
	 */
	void CreateSession(int32 NumPublicConnections, FString MatchType);

	/**
	 * Initiates a search for sessions. The results, if any, are broadcast via a custom delegate.
	 *
	 * @param MaxSearchResults The maximum number of search results to retrieve.
	 */
	void FindSessions(int32 MaxSearchResults);

	/**
	 * Attempts to join the specified session from a prior search result.
	 *
	 * @param SessionResult The FOnlineSessionSearchResult representing the session to join.
	 */
	void JoinSession(const FOnlineSessionSearchResult& SessionResult);

	/**
	 * Destroys the current session (if any).
	 */
	void DestroySession();

	/**
	 * Attempts to start the current session, if it exists and is valid.
	 */
	void StartSession();

	/**
	 * Accepts and joins an invited session result.
	 *
	 * @param InviteResult The result containing session data from an invite.
	 */
	void InviteAccept(const FOnlineSessionSearchResult& InviteResult);

	/**
	 * Custom delegates for external classes (like a MenuWidget) to bind to,
	 * to be notified of session state changes or invite acceptance.
	 */
	FMultiplayerOnCreateSessionComplete MultiplayerOnCreateSessionComplete;
	FMultiplayerOnFindSessionsComplete MultiplayerOnFindSessionsComplete;
	FMultiplayerOnJoinSessionComplete MultiplayerOnJoinSessionComplete;
	FMultiplayerOnDestroySessionComplete MultiplayerOnDestroySessionComplete;
	FMultiplayerOnStartSessionComplete MultiplayerOnStartSessionComplete;
	FMultiplayerOnInviteReceiveComplete MultiplayerOnInviteReceiveComplete;

	/**
	 * Attempts to invite a friend by their nickname. If found in the friend map, a session
	 * invite is sent through the SessionInterface.
	 *
	 * @param FriendNickname The nickname (display name) of the friend to invite.
	 */
	void InviteFriendByNickname(const FString& FriendNickname);

protected:
	
	/**
	 * Internal callback for when a session creation attempt finishes (success or fail).
	 *
	 * @param SessionName The name of the session that was attempted to be created.
	 * @param bWasSuccessful Whether or not the session creation succeeded.
	 */
	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);

	/**
	 * Internal callback for when a find sessions operation completes.
	 *
	 * @param bWasSuccessful Whether or not the session search succeeded.
	 */
	void OnFindSessionsComplete(bool bWasSuccessful);

	/**
	 * Internal callback for when a join session attempt completes.
	 *
	 * @param SessionName The name of the session that was attempted to be joined.
	 * @param Result The result code indicating success or some error type.
	 */
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);

	/**
	 * Internal callback for when a session destruction attempt finishes.
	 *
	 * @param SessionName The name of the session that was attempted to be destroyed.
	 * @param bWasSuccessful Whether or not session destruction succeeded.
	 */
	void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);

	/**
	 * Internal callback for when a StartSession call completes.
	 *
	 * @param SessionName The name of the session that was started.
	 * @param bWasSuccessful Whether or not the session was successfully started.
	 */
	void OnStartSessionComplete(FName SessionName, bool bWasSuccessful);

	/**
	 * Internal callback for when a session invite is accepted. If valid, joins the invited session.
	 *
	 * @param bWasSuccessful Whether or not the invite acceptance event was successfully retrieved.
	 * @param ControllerId The local user index who accepted the invite.
	 * @param InvitedPlayer The unique net ID of the player who got invited.
	 * @param InviteResult The actual session data from the invite.
	 */
	void OnInviteAcceptedComplete(bool bWasSuccessful, int32 ControllerId, TSharedPtr<const FUniqueNetId> InvitedPlayer, const FOnlineSessionSearchResult& InviteResult);

	/**
	 * Internal callback for reading the friends list once the read operation completes.
	 *
	 * @param LocalUserNum The local player index requesting the friends list.
	 * @param bWasSuccessful Whether or not the list was read successfully.
	 * @param ListName The name of the list read (e.g., "OnlinePlayers").
	 * @param ErrorStr An optional error string describing any failure.
	 */
	void OnReadFriendsListComplete(int32 LocalUserNum, bool bWasSuccessful, const FString& ListName, const FString& ErrorStr);
private:
	IOnlineSessionPtr SessionInterface;
	IOnlineFriendsPtr FriendsInterface;

	/** Shared pointer to the most recent session settings used when creating a session. */
	TSharedPtr<FOnlineSessionSettings> LastSessionSettings;

	/** Shared pointer to the most recent session search data used when finding sessions. */
	TSharedPtr<FOnlineSessionSearch> LastSessionSearch;

	/**
	 * Delegates and handles for hooking into the Online Session Interface delegate list.
	 * These are bound to local callback methods within this subsystem.
	 */

	FOnCreateSessionCompleteDelegate CreateSessionCompleteDelegate;
	FDelegateHandle CreateSessionCompleteDelegateHandle;

	FOnFindSessionsCompleteDelegate FindSessionsCompleteDelegate;
	FDelegateHandle FindSessionsCompleteDelegateHandle;

	FOnJoinSessionCompleteDelegate JoinSessionCompleteDelegate;
	FDelegateHandle JoinSessionCompleteDelegateHandle;

	FOnDestroySessionCompleteDelegate DestroySessionCompleteDelegate;
	FDelegateHandle DestroySessionCompleteDelegateHandle;

	FOnStartSessionCompleteDelegate StartSessionCompleteDelegate;
	FDelegateHandle StartSessionCompleteDelegateHandle;
	
	FOnSessionUserInviteAcceptedDelegate InviteAcceptedCompleteDelegate;
	FDelegateHandle InviteAcceptedCompleteDelegateHandle;

	/** If an existing session is found while creating a new one, indicates we should recreate after destroy. */
	bool bCreateSessionOnDestroy{ false };

	/** Caches the last number of public connections requested, used after destroying an old session. */
	int32 LastNumPublicConnections;

	/** Caches the last match type string requested, used after destroying an old session. */
	FString LastMatchType;

	/** Maps friend nicknames (lower-cased) to their unique net IDs for easy invite lookups. */
	TMap<FString, TSharedRef<const FUniqueNetId>> FriendNameToIdMap;
};
