// Copyright (c) 2026 Zoxemik. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "GameFramework/OnlineReplStructs.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MultiplayerSessionsTypes.h"
#include "MultiplayerSessionsSubsystem.generated.h"

class IOnlineSubsystem;
class ULocalPlayer;
class UNetDriver;
class UWorld;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnCreateSessionComplete, bool, bWasSuccessful);
DECLARE_MULTICAST_DELEGATE_TwoParams(FMultiplayerOnFindSessionsComplete, const TArray<FOnlineSessionSearchResult>&, bool);
DECLARE_MULTICAST_DELEGATE_OneParam(FMultiplayerOnJoinSessionComplete, EOnJoinSessionCompleteResult::Type);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnDestroySessionComplete, bool, bWasSuccessful);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnSessionFlowStateChanged, EMultiplayerSessionFlowState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnSessionFailure, EMultiplayerSessionFailureReason, FailureReason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMultiplayerOnSessionSearchCompleted, bool, bWasSuccessful, const TArray<FMultiplayerSessionBrowserEntry>&, BrowserEntries);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnJoinSessionRequestCompleted, EMultiplayerJoinSessionResult, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnUpdateHostedSessionCompleted, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnStartHostedSessionCompleted, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnEndHostedSessionCompleted, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnTravelRequestCompleted, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMultiplayerOnSessionInviteSent, bool, bWasSuccessful, EMultiplayerSessionFailureReason, FailureReason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMultiplayerOnPlatformInviteUIOpened, bool, bWasOpened, EMultiplayerSessionFailureReason, FailureReason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMultiplayerOnSessionInviteAccepted, bool, bJoinStarted, EMultiplayerSessionFailureReason, FailureReason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMultiplayerOnFriendSessionSearchCompleted, bool, bWasSuccessful, EMultiplayerSessionFailureReason, FailureReason);

DECLARE_LOG_CATEGORY_EXTERN(LogMultiplayerSessionsSubsystem, Log, All);

UCLASS(Config = Game)
class MULTIPLAYERSESSIONS_API UMultiplayerSessionsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UMultiplayerSessionsSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void CreateSession(int32 NumPublicConnections, const FString& MatchType, bool bUseLan);
	void FindSessions(int32 MaxSearchResults, const FString& DesiredMatchType, bool bUseLan);
	void JoinSession(const FOnlineSessionSearchResult& SessionResult);
	void DestroySession();

	UFUNCTION(BlueprintCallable, Category = "Multiplayer Sessions")
	bool JoinByAddress(const FString& Address);

	UFUNCTION(BlueprintCallable, Category = "Multiplayer Sessions")
	void CreateSessionFromRequest(const FMultiplayerSessionCreateRequest& CreateRequest);

	UFUNCTION(BlueprintCallable, Category = "Multiplayer Sessions")
	void CreateSessionForLocalPlayer(ULocalPlayer* LocalPlayer, const FMultiplayerSessionCreateRequest& CreateRequest);

	UFUNCTION(BlueprintCallable, Category = "Multiplayer Sessions")
	void FindSessionsFromRequest(const FMultiplayerSessionSearchRequest& SearchRequest);

	UFUNCTION(BlueprintCallable, Category = "Multiplayer Sessions")
	void FindSessionsForLocalPlayer(ULocalPlayer* LocalPlayer, const FMultiplayerSessionSearchRequest& SearchRequest);

	UFUNCTION(BlueprintCallable, Category = "Multiplayer Sessions")
	bool JoinSessionBySearchResultIndex(int32 SearchResultIndex);

	UFUNCTION(BlueprintCallable, Category = "Multiplayer Sessions")
	bool JoinSessionByEntryId(const FString& EntryId);

	UFUNCTION(BlueprintCallable, Category = "Multiplayer Sessions")
	bool JoinSessionByEntryIdForLocalPlayer(ULocalPlayer* LocalPlayer, const FString& EntryId);

	UFUNCTION(BlueprintCallable, Category = "Multiplayer Sessions")
	bool SendSessionInviteToFriend(const FUniqueNetIdRepl& FriendId);

	UFUNCTION(BlueprintCallable, Category = "Multiplayer Sessions")
	bool SendSessionInviteToFriendForLocalPlayer(ULocalPlayer* LocalPlayer, const FUniqueNetIdRepl& FriendId);

	UFUNCTION(BlueprintCallable, Category = "Multiplayer Sessions")
	bool ShowPlatformInviteUI();

	UFUNCTION(BlueprintCallable, Category = "Multiplayer Sessions")
	bool ShowPlatformInviteUIForLocalPlayer(ULocalPlayer* LocalPlayer);

	UFUNCTION(BlueprintCallable, Category = "Multiplayer Sessions")
	bool JoinFriendSession(const FUniqueNetIdRepl& FriendId);

	UFUNCTION(BlueprintCallable, Category = "Multiplayer Sessions")
	bool JoinFriendSessionForLocalPlayer(ULocalPlayer* LocalPlayer, const FUniqueNetIdRepl& FriendId);

	UFUNCTION(BlueprintCallable, Category = "Multiplayer Sessions")
	bool LeaveCurrentSession();

	UFUNCTION(BlueprintCallable, Category = "Multiplayer Sessions")
	bool LeaveCurrentSessionForLocalPlayer(ULocalPlayer* LocalPlayer);

	UFUNCTION(BlueprintCallable, Category = "Multiplayer Sessions")
	void UpdateHostedSessionStatus(EMultiplayerAdvertisedSessionStatus NewStatus);

	UFUNCTION(BlueprintCallable, Category = "Multiplayer Sessions")
	void StartHostedSession();

	UFUNCTION(BlueprintCallable, Category = "Multiplayer Sessions")
	void EndHostedSession();

	UFUNCTION(BlueprintPure, Category = "Multiplayer Sessions")
	EMultiplayerSessionFlowState GetCurrentFlowState() const
	{
		return CurrentFlowState;
	}

	UFUNCTION(BlueprintPure, Category = "Multiplayer Sessions")
	bool IsBusy() const
	{
		return CurrentFlowState != EMultiplayerSessionFlowState::Idle;
	}

	UFUNCTION(BlueprintPure, Category = "Multiplayer Sessions")
	bool HasNamedSession() const;

	UFUNCTION(BlueprintPure, Category = "Multiplayer Sessions")
	int32 GetLocalCompatibilityBuildId() const;

	UFUNCTION(BlueprintPure, Category = "Multiplayer Sessions")
	EMultiplayerSessionFailureReason GetLastFailureReason() const
	{
		return LastFailureReason;
	}

	UFUNCTION(BlueprintPure, Category = "Multiplayer Sessions")
	TArray<FMultiplayerSessionBrowserEntry> GetCachedBrowserEntries() const
	{
		return CachedBrowserEntries;
	}

	UFUNCTION(BlueprintPure, Category = "Multiplayer Sessions")
	int32 GetCachedBrowserEntryCount() const
	{
		return CachedBrowserEntries.Num();
	}

	UPROPERTY(BlueprintAssignable, Category = "Multiplayer Sessions")
	FMultiplayerOnCreateSessionComplete OnCreateSessionRequestComplete;

	UPROPERTY(BlueprintAssignable, Category = "Multiplayer Sessions")
	FMultiplayerOnDestroySessionComplete OnDestroySessionRequestComplete;

	UPROPERTY(BlueprintAssignable, Category = "Multiplayer Sessions")
	FMultiplayerOnSessionFlowStateChanged OnSessionFlowStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Multiplayer Sessions")
	FMultiplayerOnSessionFailure OnSessionFailure;

	UPROPERTY(BlueprintAssignable, Category = "Multiplayer Sessions")
	FMultiplayerOnSessionSearchCompleted OnSessionSearchCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Multiplayer Sessions")
	FMultiplayerOnJoinSessionRequestCompleted OnJoinSessionRequestCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Multiplayer Sessions")
	FMultiplayerOnUpdateHostedSessionCompleted OnUpdateHostedSessionCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Multiplayer Sessions")
	FMultiplayerOnStartHostedSessionCompleted OnStartHostedSessionCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Multiplayer Sessions")
	FMultiplayerOnEndHostedSessionCompleted OnEndHostedSessionCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Multiplayer Sessions")
	FMultiplayerOnTravelRequestCompleted OnTravelRequestCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Multiplayer Sessions|Invites")
	FMultiplayerOnSessionInviteSent OnSessionInviteSent;

	UPROPERTY(BlueprintAssignable, Category = "Multiplayer Sessions|Invites")
	FMultiplayerOnPlatformInviteUIOpened OnPlatformInviteUIOpened;

	UPROPERTY(BlueprintAssignable, Category = "Multiplayer Sessions|Invites")
	FMultiplayerOnSessionInviteAccepted OnSessionInviteAccepted;

	UPROPERTY(BlueprintAssignable, Category = "Multiplayer Sessions|Friends")
	FMultiplayerOnFriendSessionSearchCompleted OnFriendSessionSearchCompleted;

public:
	FMultiplayerOnCreateSessionComplete MultiplayerOnCreateSessionComplete;
	FMultiplayerOnFindSessionsComplete MultiplayerOnFindSessionsComplete;
	FMultiplayerOnJoinSessionComplete MultiplayerOnJoinSessionComplete;
	FMultiplayerOnDestroySessionComplete MultiplayerOnDestroySessionComplete;

private:
	enum class EOperationType : uint8
	{
		None,
		Create,
		Find,
		Join,
		Destroy,
		Update,
		Start,
		End,
		FindFriend,
		DirectTravel
	};

	enum class EOperationStep : uint8
	{
		None,
		DestroyExistingForCreate,
		DestroyExistingForJoin,
		Executing,
		WaitingForTravel,
		Recovering,
		RecoveryDestroy
	};

	struct FLocalUserContext
	{
		TWeakObjectPtr<ULocalPlayer> LocalPlayer;
		FUniqueNetIdPtr UniqueNetId;
		int32 LocalUserNum = 0;
	};

	struct FOperationContext
	{
		uint64 Generation = 0;
		EOperationType Type = EOperationType::None;
		EOperationType RecoverySourceType = EOperationType::None;
		EOperationStep Step = EOperationStep::None;
		FLocalUserContext LocalUser;
		FMultiplayerSessionCreateRequest CreateRequest;
		FMultiplayerSessionSearchRequest SearchRequest;
		FOnlineSessionSearchResult JoinResult;
		FUniqueNetIdPtr FriendId;
		TSharedPtr<FOnlineSessionSettings> PendingSessionSettings;
		TSharedPtr<FOnlineSessionSearch> PendingSearch;
		EMultiplayerAdvertisedSessionStatus RequestedStatus = EMultiplayerAdvertisedSessionStatus::Unknown;
		double DeadlineSeconds = 0.0;
		double RecoveryNotBeforeSeconds = 0.0;
		bool bResultBroadcast = false;
		bool bRecoveryAttemptedDestroy = false;
	};

private:
	void OnCreateSessionCompleteInternal(FName SessionName, bool bWasSuccessful, uint64 CallbackGeneration);
	void OnFindSessionsCompleteInternal(bool bWasSuccessful, uint64 CallbackGeneration);
	void OnFindFriendSessionCompleteInternal(int32 LocalUserNum, bool bWasSuccessful, const TArray<FOnlineSessionSearchResult>& SearchResults, uint64 CallbackGeneration);
	void OnSessionUserInviteAcceptedInternal(bool bWasSuccessful, int32 LocalUserNum, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult);
	void OnJoinSessionCompleteInternal(FName SessionName, EOnJoinSessionCompleteResult::Type Result, uint64 CallbackGeneration);
	void OnDestroySessionCompleteInternal(FName SessionName, bool bWasSuccessful, uint64 CallbackGeneration);
	void OnUpdateSessionCompleteInternal(FName SessionName, bool bWasSuccessful, uint64 CallbackGeneration);
	void OnStartSessionCompleteInternal(FName SessionName, bool bWasSuccessful, uint64 CallbackGeneration);
	void OnEndSessionCompleteInternal(FName SessionName, bool bWasSuccessful, uint64 CallbackGeneration);
	void OnRecoveryDestroyCompleteInternal(FName SessionName, bool bWasSuccessful, uint64 CallbackGeneration);

	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);
	void HandlePostLoadMap(UWorld* LoadedWorld);
	bool TickOperationTimeouts(float DeltaTime);

	bool EnsureSessionInterface(const TCHAR* Context, EMultiplayerSessionFailureReason& OutFailureReason);
	void RegisterPersistentSessionDelegates();
	void ClearPersistentSessionDelegates();
	bool ResolveLocalUser(ULocalPlayer* RequestedLocalPlayer, FLocalUserContext& OutLocalUser, EMultiplayerSessionFailureReason& OutFailureReason);
	ULocalPlayer* ResolveDefaultLocalPlayer() const;
	bool IsOnlineLoginRequired() const;
	bool OperationRequiresSessionInterface(EOperationType OperationType) const;
	bool OperationRequiresLocalPlayer(EOperationType OperationType) const;

	bool TryBeginOperation(EOperationType OperationType, EMultiplayerSessionFlowState FlowState, ULocalPlayer* LocalPlayer, double TimeoutSeconds, EMultiplayerSessionFailureReason& OutFailureReason);
	bool IsCurrentOperation(uint64 CallbackGeneration, EOperationType ExpectedType, EOperationStep ExpectedStep) const;
	void SetOperationStep(EOperationStep NewStep, double TimeoutSeconds);
	EMultiplayerSessionFlowState ResetActiveOperation();
	void BroadcastIdleStateIfUnchanged(EMultiplayerSessionFlowState PreviousFlowState, uint64 ExpectedOperationGeneration);
	void ClearOperationDelegate(EOperationType OperationType);
	void ClearAllDelegateHandles();
	void ResetCommittedSessionState();

	void BeginCreateOperation();
	void BeginFindOperation();
	void BeginFindFriendOperation();
	void BeginJoinAfterExistingSessionCleanup();
	void BeginJoinOperation();
	void BeginDestroyOperation();
	void BeginUpdateOperation();
	void BeginStartOperation();
	void BeginEndOperation();
	bool BeginTravel(const FString& TravelAddress);
	void BeginRecovery(EOperationType SourceType);
	void ContinueRecovery();
	void BeginRecoveryDestroy();

	void CompleteCreateOperation(bool bWasSuccessful, EMultiplayerSessionFailureReason FailureReason);
	void CompleteFindOperation(bool bWasSuccessful, EMultiplayerSessionFailureReason FailureReason, TArray<FOnlineSessionSearchResult>&& SearchResults, TArray<FMultiplayerSessionBrowserEntry>&& BrowserEntries);
	void CompleteFindFriendOperation(bool bWasSuccessful, EMultiplayerSessionFailureReason FailureReason);
	void CompleteJoinOperation(EOnJoinSessionCompleteResult::Type LegacyResult, EMultiplayerJoinSessionResult Result, EMultiplayerSessionFailureReason FailureReason);
	void CompleteDestroyOperation(bool bWasSuccessful, EMultiplayerSessionFailureReason FailureReason);
	void CompleteUpdateOperation(bool bWasSuccessful, EMultiplayerSessionFailureReason FailureReason);
	void CompleteStartOperation(bool bWasSuccessful, EMultiplayerSessionFailureReason FailureReason);
	void CompleteEndOperation(bool bWasSuccessful, EMultiplayerSessionFailureReason FailureReason);
	void CompleteDirectTravelOperation(bool bWasSuccessful, EMultiplayerSessionFailureReason FailureReason);
	void CompleteRecovery(bool bWasSuccessful);
	void HandleOperationTimeout();
	void HandleTravelFailureInternal(EMultiplayerSessionFailureReason FailureReason);

	void SetFlowState(EMultiplayerSessionFlowState NewState);
	void BroadcastFailure(EMultiplayerSessionFailureReason FailureReason);
	void SetLastFailureReason(EMultiplayerSessionFailureReason FailureReason);
	void BroadcastImmediateFailureForOperation(EOperationType OperationType, EMultiplayerSessionFailureReason FailureReason);

	void SanitizeCreateRequest(FMultiplayerSessionCreateRequest& InOutCreateRequest) const;
	void SanitizeSearchRequest(FMultiplayerSessionSearchRequest& InOutSearchRequest) const;
	int32 ResolveBuildId(int32 RequestedBuildId) const;

	void ApplyCreateRequestToSessionSettings(FOnlineSessionSettings& SessionSettings, const FMultiplayerSessionCreateRequest& CreateRequest) const;
	bool IsSearchResultRelevantToRequest(const FOnlineSessionSearchResult& SearchResult, const FMultiplayerSessionSearchRequest& SearchRequest) const;
	EMultiplayerJoinBlockReason ResolveJoinBlockReason(const FOnlineSessionSearchResult& SearchResult, const FMultiplayerSessionSearchRequest& SearchRequest) const;
	FMultiplayerSessionBrowserEntry BuildBrowserEntry(const FOnlineSessionSearchResult& SearchResult, int32 SearchResultIndex, const FMultiplayerSessionSearchRequest& SearchRequest) const;
	void ResolveJoinability(FMultiplayerSessionBrowserEntry& BrowserEntry, const FOnlineSessionSearchResult& SearchResult, const FMultiplayerSessionSearchRequest& SearchRequest) const;
	void SortSearchResultsAndBrowserEntries(TArray<FOnlineSessionSearchResult>& SearchResults, TArray<FMultiplayerSessionBrowserEntry>& BrowserEntries) const;
	int32 FindCachedSearchResultIndexByEntryId(const FString& EntryId) const;

	static EMultiplayerAdvertisedSessionStatus ResolveDisplayStatus(EMultiplayerAdvertisedSessionStatus AdvertisedStatus, int32 OpenPublicConnections);
	static FString ResolveDisplayStatusText(EMultiplayerAdvertisedSessionStatus Status);
	static int32 ResolveStatusSortPriority(EMultiplayerAdvertisedSessionStatus Status);
	static EMultiplayerJoinSessionResult ResolvePreJoinFailureResult(const FMultiplayerSessionBrowserEntry& BrowserEntry);
	static EMultiplayerSessionFailureReason ResolveFailureReasonForJoinBlock(EMultiplayerJoinBlockReason JoinBlockReason);
	static FString ResolveJoinBlockReasonText(EMultiplayerJoinBlockReason JoinBlockReason);
	static FString SessionStatusToString(EMultiplayerAdvertisedSessionStatus Status);
	static EMultiplayerAdvertisedSessionStatus SessionStatusFromString(const FString& StatusText);
	static EMultiplayerJoinSessionResult MapJoinResult(EOnJoinSessionCompleteResult::Type Result);

private:
	IOnlineSubsystem* CachedOnlineSubsystem = nullptr;
	IOnlineSessionPtr SessionInterface;

	TSharedPtr<FOnlineSessionSettings> CommittedSessionSettings;
	bool bOwnsNamedSession = false;
	bool bHasCommittedJoinInProgressPolicy = false;
	bool bCommittedAllowJoinInProgress = true;

	TArray<FOnlineSessionSearchResult> CachedSearchResults;
	TArray<FMultiplayerSessionBrowserEntry> CachedBrowserEntries;

	FOperationContext ActiveOperation;
	uint64 NextOperationGeneration = 0;

	FDelegateHandle CreateSessionCompleteDelegateHandle;
	FDelegateHandle FindSessionsCompleteDelegateHandle;
	FDelegateHandle FindFriendSessionCompleteDelegateHandle;
	FDelegateHandle SessionInviteAcceptedDelegateHandle;
	int32 FindFriendDelegateLocalUserNum = INDEX_NONE;
	FDelegateHandle JoinSessionCompleteDelegateHandle;
	FDelegateHandle DestroySessionCompleteDelegateHandle;
	FDelegateHandle UpdateSessionCompleteDelegateHandle;
	FDelegateHandle StartSessionCompleteDelegateHandle;
	FDelegateHandle EndSessionCompleteDelegateHandle;
	FDelegateHandle RecoveryDestroyCompleteDelegateHandle;

	FDelegateHandle NetworkFailureDelegateHandle;
	FDelegateHandle TravelFailureDelegateHandle;
	FDelegateHandle PostLoadMapDelegateHandle;
	FTSTicker::FDelegateHandle OperationTickerHandle;

	EMultiplayerSessionFlowState CurrentFlowState = EMultiplayerSessionFlowState::Idle;
	EMultiplayerSessionFailureReason LastFailureReason = EMultiplayerSessionFailureReason::None;

	UPROPERTY(Config, EditAnywhere, Category = "Multiplayer Sessions|Timeouts", meta = (ClampMin = "1.0"))
	float CreateTimeoutSeconds = 20.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Multiplayer Sessions|Timeouts", meta = (ClampMin = "1.0"))
	float FindTimeoutSeconds = 15.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Multiplayer Sessions|Timeouts", meta = (ClampMin = "1.0"))
	float JoinTimeoutSeconds = 20.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Multiplayer Sessions|Timeouts", meta = (ClampMin = "1.0"))
	float DestroyTimeoutSeconds = 15.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Multiplayer Sessions|Timeouts", meta = (ClampMin = "1.0"))
	float UpdateTimeoutSeconds = 10.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Multiplayer Sessions|Timeouts", meta = (ClampMin = "1.0"))
	float StartEndTimeoutSeconds = 10.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Multiplayer Sessions|Timeouts", meta = (ClampMin = "1.0"))
	float TravelTimeoutSeconds = 30.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Multiplayer Sessions|Timeouts", meta = (ClampMin = "0.0"))
	float RecoveryGraceSeconds = 3.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Multiplayer Sessions|Timeouts", meta = (ClampMin = "1.0"))
	float RecoveryTimeoutSeconds = 12.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Multiplayer Sessions|Compatibility")
	bool bAllowBuildIdOverride = false;
};
