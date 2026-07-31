// Copyright (c) 2026 Zoxemik. All rights reserved.

#include "MultiplayerSessionsSubsystem.h"

#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/OnlineReplStructs.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/OnlineExternalUIInterface.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Misc/NetworkVersion.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY(LogMultiplayerSessionsSubsystem);

UMultiplayerSessionsSubsystem::UMultiplayerSessionsSubsystem()
{
}

void UMultiplayerSessionsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	EMultiplayerSessionFailureReason FailureReason = EMultiplayerSessionFailureReason::None;
	EnsureSessionInterface(TEXT("Initialize"), FailureReason);

	if (GEngine != nullptr)
	{
		NetworkFailureDelegateHandle = GEngine->OnNetworkFailure().AddUObject(this, &ThisClass::HandleNetworkFailure);
		TravelFailureDelegateHandle = GEngine->OnTravelFailure().AddUObject(this, &ThisClass::HandleTravelFailure);
	}

	PostLoadMapDelegateHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMap);
	OperationTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::TickOperationTimeouts), 0.25f);
}

void UMultiplayerSessionsSubsystem::Deinitialize()
{
	if (GEngine != nullptr)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureDelegateHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureDelegateHandle);
	}

	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapDelegateHandle);
	FTSTicker::GetCoreTicker().RemoveTicker(OperationTickerHandle);

	ClearAllDelegateHandles();
	CachedSearchResults.Reset();
	CachedBrowserEntries.Reset();
	ResetCommittedSessionState();
	SessionInterface.Reset();
	CachedOnlineSubsystem = nullptr;
	ActiveOperation = FOperationContext();

	Super::Deinitialize();
}

void UMultiplayerSessionsSubsystem::CreateSession(int32 NumPublicConnections, const FString& MatchType, bool bUseLan)
{
	FMultiplayerSessionCreateRequest CreateRequest;
	CreateRequest.NumPublicConnections = NumPublicConnections;
	CreateRequest.MatchType = MatchType;
	CreateRequest.bUseLan = bUseLan;
	CreateSessionFromRequest(CreateRequest);
}

void UMultiplayerSessionsSubsystem::FindSessions(int32 MaxSearchResults, const FString& DesiredMatchType, bool bUseLan)
{
	FMultiplayerSessionSearchRequest SearchRequest;
	SearchRequest.MaxSearchResults = MaxSearchResults;
	SearchRequest.DesiredMatchType = DesiredMatchType;
	SearchRequest.bUseLan = bUseLan;
	FindSessionsFromRequest(SearchRequest);
}

void UMultiplayerSessionsSubsystem::JoinSession(const FOnlineSessionSearchResult& SessionResult)
{
	EMultiplayerSessionFailureReason FailureReason = EMultiplayerSessionFailureReason::None;
	if (TryBeginOperation(EOperationType::Join, EMultiplayerSessionFlowState::Joining, nullptr, JoinTimeoutSeconds, FailureReason) == false)
	{
		BroadcastImmediateFailureForOperation(EOperationType::Join, FailureReason);
		return;
	}

	ActiveOperation.JoinResult = SessionResult;

	if (ActiveOperation.JoinResult.IsValid() == false)
	{
		CompleteJoinOperation(EOnJoinSessionCompleteResult::SessionDoesNotExist, EMultiplayerJoinSessionResult::SessionDoesNotExist, EMultiplayerSessionFailureReason::JoinFailed);
		return;
	}

	FMultiplayerSessionSearchRequest CompatibilityRequest;
	CompatibilityRequest.DesiredMatchType.Reset();
	SanitizeSearchRequest(CompatibilityRequest);
	const EMultiplayerJoinBlockReason JoinBlockReason = ResolveJoinBlockReason(ActiveOperation.JoinResult, CompatibilityRequest);
	if (JoinBlockReason != EMultiplayerJoinBlockReason::None)
	{
		FMultiplayerSessionBrowserEntry BrowserEntry;
		BrowserEntry.JoinBlockReason = JoinBlockReason;
		CompleteJoinOperation(EOnJoinSessionCompleteResult::UnknownError, ResolvePreJoinFailureResult(BrowserEntry), ResolveFailureReasonForJoinBlock(JoinBlockReason));
		return;
	}

	if (ActiveOperation.LocalUser.UniqueNetId.IsValid() == false && IsOnlineLoginRequired() == true && ActiveOperation.JoinResult.Session.SessionSettings.bIsLANMatch == false)
	{
		CompleteJoinOperation(EOnJoinSessionCompleteResult::UnknownError, EMultiplayerJoinSessionResult::UnknownError, EMultiplayerSessionFailureReason::NotLoggedIn);
		return;
	}

	BeginJoinAfterExistingSessionCleanup();
}

void UMultiplayerSessionsSubsystem::DestroySession()
{
	LeaveCurrentSession();
}

bool UMultiplayerSessionsSubsystem::JoinByAddress(const FString& Address)
{
	FString TravelAddress = Address;
	TravelAddress.TrimStartAndEndInline();

	if (TravelAddress.IsEmpty() == true)
	{
		BroadcastImmediateFailureForOperation(EOperationType::DirectTravel, EMultiplayerSessionFailureReason::InvalidAddress);
		return false;
	}

	EMultiplayerSessionFailureReason FailureReason = EMultiplayerSessionFailureReason::None;
	if (TryBeginOperation(EOperationType::DirectTravel, EMultiplayerSessionFlowState::Traveling, nullptr, TravelTimeoutSeconds, FailureReason) == false)
	{
		BroadcastImmediateFailureForOperation(EOperationType::DirectTravel, FailureReason);
		return false;
	}

	if (BeginTravel(TravelAddress) == false)
	{
		CompleteDirectTravelOperation(false, EMultiplayerSessionFailureReason::TravelFailed);
		return false;
	}

	return true;
}

void UMultiplayerSessionsSubsystem::CreateSessionFromRequest(const FMultiplayerSessionCreateRequest& CreateRequest)
{
	CreateSessionForLocalPlayer(nullptr, CreateRequest);
}

void UMultiplayerSessionsSubsystem::CreateSessionForLocalPlayer(ULocalPlayer* LocalPlayer, const FMultiplayerSessionCreateRequest& CreateRequest)
{
	EMultiplayerSessionFailureReason FailureReason = EMultiplayerSessionFailureReason::None;
	if (TryBeginOperation(EOperationType::Create, EMultiplayerSessionFlowState::Creating, LocalPlayer, CreateTimeoutSeconds, FailureReason) == false)
	{
		BroadcastImmediateFailureForOperation(EOperationType::Create, FailureReason);
		return;
	}

	ActiveOperation.CreateRequest = CreateRequest;
	SanitizeCreateRequest(ActiveOperation.CreateRequest);

	const UWorld* World = GetWorld();
	const bool bDedicatedServer = World != nullptr && World->GetNetMode() == NM_DedicatedServer;
	if (bDedicatedServer == false && ActiveOperation.LocalUser.UniqueNetId.IsValid() == false && IsOnlineLoginRequired() == true && ActiveOperation.CreateRequest.bUseLan == false)
	{
		CompleteCreateOperation(false, EMultiplayerSessionFailureReason::NotLoggedIn);
		return;
	}

	FNamedOnlineSession* ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);
	if (ExistingSession != nullptr)
	{
		if (ActiveOperation.CreateRequest.bReplaceExistingSession == false)
		{
			CompleteCreateOperation(false, EMultiplayerSessionFailureReason::SessionAlreadyExists);
			return;
		}

		ActiveOperation.Step = EOperationStep::DestroyExistingForCreate;
		BeginDestroyOperation();
		return;
	}

	BeginCreateOperation();
}

void UMultiplayerSessionsSubsystem::FindSessionsFromRequest(const FMultiplayerSessionSearchRequest& SearchRequest)
{
	FindSessionsForLocalPlayer(nullptr, SearchRequest);
}

void UMultiplayerSessionsSubsystem::FindSessionsForLocalPlayer(ULocalPlayer* LocalPlayer, const FMultiplayerSessionSearchRequest& SearchRequest)
{
	EMultiplayerSessionFailureReason FailureReason = EMultiplayerSessionFailureReason::None;
	if (TryBeginOperation(EOperationType::Find, EMultiplayerSessionFlowState::Finding, LocalPlayer, FindTimeoutSeconds, FailureReason) == false)
	{
		BroadcastImmediateFailureForOperation(EOperationType::Find, FailureReason);
		return;
	}

	ActiveOperation.SearchRequest = SearchRequest;
	SanitizeSearchRequest(ActiveOperation.SearchRequest);

	if (ActiveOperation.LocalUser.UniqueNetId.IsValid() == false && IsOnlineLoginRequired() == true && ActiveOperation.SearchRequest.bUseLan == false)
	{
		TArray<FOnlineSessionSearchResult> EmptySearchResults;
		TArray<FMultiplayerSessionBrowserEntry> EmptyBrowserEntries;
		CompleteFindOperation(false, EMultiplayerSessionFailureReason::NotLoggedIn, MoveTemp(EmptySearchResults), MoveTemp(EmptyBrowserEntries));
		return;
	}

	BeginFindOperation();
}

bool UMultiplayerSessionsSubsystem::JoinSessionBySearchResultIndex(int32 SearchResultIndex)
{
	if (CachedSearchResults.IsValidIndex(SearchResultIndex) == false)
	{
		BroadcastImmediateFailureForOperation(EOperationType::Join, EMultiplayerSessionFailureReason::InvalidSearchResultIndex);
		return false;
	}

	if (CachedBrowserEntries.IsValidIndex(SearchResultIndex) == true)
	{
		const FMultiplayerSessionBrowserEntry& BrowserEntry = CachedBrowserEntries[SearchResultIndex];
		if (BrowserEntry.bCanJoin == false)
		{
			const EMultiplayerJoinSessionResult FailureResult = ResolvePreJoinFailureResult(BrowserEntry);
			const EMultiplayerSessionFailureReason FailureReason = ResolveFailureReasonForJoinBlock(BrowserEntry.JoinBlockReason);
			SetLastFailureReason(FailureReason);
			MultiplayerOnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
			OnJoinSessionRequestCompleted.Broadcast(FailureResult);
			return false;
		}
	}

	if (CachedBrowserEntries.IsValidIndex(SearchResultIndex) == true)
	{
		return JoinSessionByEntryIdForLocalPlayer(nullptr, CachedBrowserEntries[SearchResultIndex].EntryId);
	}

	EMultiplayerSessionFailureReason FailureReason = EMultiplayerSessionFailureReason::None;
	if (TryBeginOperation(EOperationType::Join, EMultiplayerSessionFlowState::Joining, nullptr, JoinTimeoutSeconds, FailureReason) == false)
	{
		BroadcastImmediateFailureForOperation(EOperationType::Join, FailureReason);
		return false;
	}

	ActiveOperation.JoinResult = CachedSearchResults[SearchResultIndex];
	BeginJoinAfterExistingSessionCleanup();
	return ActiveOperation.Type == EOperationType::Join;
}

bool UMultiplayerSessionsSubsystem::JoinSessionByEntryId(const FString& EntryId)
{
	return JoinSessionByEntryIdForLocalPlayer(nullptr, EntryId);
}

bool UMultiplayerSessionsSubsystem::JoinSessionByEntryIdForLocalPlayer(ULocalPlayer* LocalPlayer, const FString& EntryId)
{
	const int32 SearchResultIndex = FindCachedSearchResultIndexByEntryId(EntryId);
	if (SearchResultIndex == INDEX_NONE)
	{
		BroadcastImmediateFailureForOperation(EOperationType::Join, EMultiplayerSessionFailureReason::InvalidSearchResultIndex);
		return false;
	}

	if (CachedBrowserEntries.IsValidIndex(SearchResultIndex) == true)
	{
		const FMultiplayerSessionBrowserEntry& BrowserEntry = CachedBrowserEntries[SearchResultIndex];
		if (BrowserEntry.bCanJoin == false)
		{
			const EMultiplayerJoinSessionResult FailureResult = ResolvePreJoinFailureResult(BrowserEntry);
			const EMultiplayerSessionFailureReason FailureReason = ResolveFailureReasonForJoinBlock(BrowserEntry.JoinBlockReason);
			SetLastFailureReason(FailureReason);
			MultiplayerOnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
			OnJoinSessionRequestCompleted.Broadcast(FailureResult);
			return false;
		}
	}

	EMultiplayerSessionFailureReason FailureReason = EMultiplayerSessionFailureReason::None;
	if (TryBeginOperation(EOperationType::Join, EMultiplayerSessionFlowState::Joining, LocalPlayer, JoinTimeoutSeconds, FailureReason) == false)
	{
		BroadcastImmediateFailureForOperation(EOperationType::Join, FailureReason);
		return false;
	}

	ActiveOperation.JoinResult = CachedSearchResults[SearchResultIndex];
	if (ActiveOperation.LocalUser.UniqueNetId.IsValid() == false && IsOnlineLoginRequired() == true && ActiveOperation.JoinResult.Session.SessionSettings.bIsLANMatch == false)
	{
		CompleteJoinOperation(EOnJoinSessionCompleteResult::UnknownError, EMultiplayerJoinSessionResult::UnknownError, EMultiplayerSessionFailureReason::NotLoggedIn);
		return false;
	}

	BeginJoinAfterExistingSessionCleanup();
	return ActiveOperation.Type == EOperationType::Join;
}

bool UMultiplayerSessionsSubsystem::SendSessionInviteToFriend(const FUniqueNetIdRepl& FriendId)
{
	return SendSessionInviteToFriendForLocalPlayer(nullptr, FriendId);
}

bool UMultiplayerSessionsSubsystem::SendSessionInviteToFriendForLocalPlayer(ULocalPlayer* LocalPlayer, const FUniqueNetIdRepl& FriendId)
{
	EMultiplayerSessionFailureReason FailureReason = EMultiplayerSessionFailureReason::None;
	if (EnsureSessionInterface(TEXT("SendSessionInviteToFriend"), FailureReason) == false)
	{
		SetLastFailureReason(FailureReason);
		OnSessionInviteSent.Broadcast(false, FailureReason);
		return false;
	}

	FLocalUserContext LocalUser;
	if (ResolveLocalUser(LocalPlayer, LocalUser, FailureReason) == false)
	{
		SetLastFailureReason(FailureReason);
		OnSessionInviteSent.Broadcast(false, FailureReason);
		return false;
	}

	const FUniqueNetIdPtr FriendUniqueNetId = FriendId.GetUniqueNetId();
	if (FriendId.IsValid() == false || FriendUniqueNetId.IsValid() == false)
	{
		FailureReason = EMultiplayerSessionFailureReason::InvalidFriendId;
		SetLastFailureReason(FailureReason);
		OnSessionInviteSent.Broadcast(false, FailureReason);
		return false;
	}

	const FNamedOnlineSession* HostedSession = SessionInterface->GetNamedSession(NAME_GameSession);
	if (HostedSession == nullptr || HostedSession->SessionSettings.bIsLANMatch == true || HostedSession->SessionSettings.bAllowInvites == false)
	{
		FailureReason = EMultiplayerSessionFailureReason::InviteFailed;
		SetLastFailureReason(FailureReason);
		OnSessionInviteSent.Broadcast(false, FailureReason);
		return false;
	}

	if (IsOnlineLoginRequired() == true && LocalUser.UniqueNetId.IsValid() == false)
	{
		FailureReason = EMultiplayerSessionFailureReason::NotLoggedIn;
		SetLastFailureReason(FailureReason);
		OnSessionInviteSent.Broadcast(false, FailureReason);
		return false;
	}

	const bool bInviteSent = SessionInterface->SendSessionInviteToFriend(LocalUser.LocalUserNum, NAME_GameSession, *FriendUniqueNetId);

	if (bInviteSent == false)
	{
		FailureReason = EMultiplayerSessionFailureReason::InviteFailed;
	}

	SetLastFailureReason(FailureReason);
	OnSessionInviteSent.Broadcast(bInviteSent, FailureReason);
	return bInviteSent;
}

bool UMultiplayerSessionsSubsystem::ShowPlatformInviteUI()
{
	return ShowPlatformInviteUIForLocalPlayer(nullptr);
}

bool UMultiplayerSessionsSubsystem::ShowPlatformInviteUIForLocalPlayer(ULocalPlayer* LocalPlayer)
{
	EMultiplayerSessionFailureReason FailureReason = EMultiplayerSessionFailureReason::None;
	if (EnsureSessionInterface(TEXT("ShowPlatformInviteUI"), FailureReason) == false)
	{
		SetLastFailureReason(FailureReason);
		OnPlatformInviteUIOpened.Broadcast(false, FailureReason);
		return false;
	}

	FLocalUserContext LocalUser;
	if (ResolveLocalUser(LocalPlayer, LocalUser, FailureReason) == false)
	{
		SetLastFailureReason(FailureReason);
		OnPlatformInviteUIOpened.Broadcast(false, FailureReason);
		return false;
	}

	const FNamedOnlineSession* HostedSession = SessionInterface->GetNamedSession(NAME_GameSession);
	if (HostedSession == nullptr || HostedSession->SessionSettings.bIsLANMatch == true || HostedSession->SessionSettings.bAllowInvites == false)
	{
		FailureReason = EMultiplayerSessionFailureReason::InviteFailed;
		SetLastFailureReason(FailureReason);
		OnPlatformInviteUIOpened.Broadcast(false, FailureReason);
		return false;
	}

	if (CachedOnlineSubsystem == nullptr)
	{
		FailureReason = EMultiplayerSessionFailureReason::NoOnlineSubsystem;
		SetLastFailureReason(FailureReason);
		OnPlatformInviteUIOpened.Broadcast(false, FailureReason);
		return false;
	}

	const IOnlineExternalUIPtr ExternalUIInterface = CachedOnlineSubsystem->GetExternalUIInterface();
	if (ExternalUIInterface.IsValid() == false)
	{
		FailureReason = EMultiplayerSessionFailureReason::PlatformUiUnavailable;
		SetLastFailureReason(FailureReason);
		OnPlatformInviteUIOpened.Broadcast(false, FailureReason);
		return false;
	}

	const bool bWasOpened = ExternalUIInterface->ShowInviteUI(LocalUser.LocalUserNum, NAME_GameSession);
	if (bWasOpened == false)
	{
		FailureReason = EMultiplayerSessionFailureReason::PlatformUiUnavailable;
	}

	SetLastFailureReason(FailureReason);
	OnPlatformInviteUIOpened.Broadcast(bWasOpened, FailureReason);
	return bWasOpened;
}

bool UMultiplayerSessionsSubsystem::JoinFriendSession(const FUniqueNetIdRepl& FriendId)
{
	return JoinFriendSessionForLocalPlayer(nullptr, FriendId);
}

bool UMultiplayerSessionsSubsystem::JoinFriendSessionForLocalPlayer(ULocalPlayer* LocalPlayer, const FUniqueNetIdRepl& FriendId)
{
	EMultiplayerSessionFailureReason FailureReason = EMultiplayerSessionFailureReason::None;
	if (TryBeginOperation(EOperationType::FindFriend, EMultiplayerSessionFlowState::Finding, LocalPlayer, FindTimeoutSeconds, FailureReason) == false)
	{
		BroadcastImmediateFailureForOperation(EOperationType::FindFriend, FailureReason);
		return false;
	}

	ActiveOperation.FriendId = FriendId.GetUniqueNetId();
	if (FriendId.IsValid() == false || ActiveOperation.FriendId.IsValid() == false)
	{
		CompleteFindFriendOperation(false, EMultiplayerSessionFailureReason::InvalidFriendId);
		return false;
	}

	if (IsOnlineLoginRequired() == true && ActiveOperation.LocalUser.UniqueNetId.IsValid() == false)
	{
		CompleteFindFriendOperation(false, EMultiplayerSessionFailureReason::NotLoggedIn);
		return false;
	}

	BeginFindFriendOperation();
	return ActiveOperation.Type == EOperationType::FindFriend || ActiveOperation.Type == EOperationType::Join;
}

bool UMultiplayerSessionsSubsystem::LeaveCurrentSession()
{
	return LeaveCurrentSessionForLocalPlayer(nullptr);
}

bool UMultiplayerSessionsSubsystem::LeaveCurrentSessionForLocalPlayer(ULocalPlayer* LocalPlayer)
{
	EMultiplayerSessionFailureReason FailureReason = EMultiplayerSessionFailureReason::None;
	if (TryBeginOperation(EOperationType::Destroy, EMultiplayerSessionFlowState::Destroying, LocalPlayer, DestroyTimeoutSeconds, FailureReason) == false)
	{
		BroadcastImmediateFailureForOperation(EOperationType::Destroy, FailureReason);
		return false;
	}

	BeginDestroyOperation();
	return true;
}

void UMultiplayerSessionsSubsystem::UpdateHostedSessionStatus(EMultiplayerAdvertisedSessionStatus NewStatus)
{
	EMultiplayerSessionFailureReason FailureReason = EMultiplayerSessionFailureReason::None;
	if (TryBeginOperation(EOperationType::Update, EMultiplayerSessionFlowState::Updating, nullptr, UpdateTimeoutSeconds, FailureReason) == false)
	{
		BroadcastImmediateFailureForOperation(EOperationType::Update, FailureReason);
		return;
	}

	ActiveOperation.RequestedStatus = NewStatus;
	BeginUpdateOperation();
}

void UMultiplayerSessionsSubsystem::StartHostedSession()
{
	EMultiplayerSessionFailureReason FailureReason = EMultiplayerSessionFailureReason::None;
	if (TryBeginOperation(EOperationType::Start, EMultiplayerSessionFlowState::Starting, nullptr, StartEndTimeoutSeconds, FailureReason) == false)
	{
		BroadcastImmediateFailureForOperation(EOperationType::Start, FailureReason);
		return;
	}

	BeginStartOperation();
}

void UMultiplayerSessionsSubsystem::EndHostedSession()
{
	EMultiplayerSessionFailureReason FailureReason = EMultiplayerSessionFailureReason::None;
	if (TryBeginOperation(EOperationType::End, EMultiplayerSessionFlowState::Ending, nullptr, StartEndTimeoutSeconds, FailureReason) == false)
	{
		BroadcastImmediateFailureForOperation(EOperationType::End, FailureReason);
		return;
	}

	BeginEndOperation();
}

bool UMultiplayerSessionsSubsystem::HasNamedSession() const
{
	if (SessionInterface.IsValid() == false)
	{
		return false;
	}

	return SessionInterface->GetNamedSession(NAME_GameSession) != nullptr;
}

int32 UMultiplayerSessionsSubsystem::GetLocalCompatibilityBuildId() const
{
	return ResolveBuildId(0);
}

void UMultiplayerSessionsSubsystem::OnCreateSessionCompleteInternal(FName SessionName, bool bWasSuccessful, uint64 CallbackGeneration)
{
	if (IsCurrentOperation(CallbackGeneration, EOperationType::Create, EOperationStep::Executing) == false)
	{
		return;
	}

	ClearOperationDelegate(EOperationType::Create);

	if (bWasSuccessful == true)
	{
		CommittedSessionSettings = ActiveOperation.PendingSessionSettings;
		bOwnsNamedSession = true;
		bHasCommittedJoinInProgressPolicy = true;
		bCommittedAllowJoinInProgress = ActiveOperation.CreateRequest.bAllowJoinInProgress;
		CompleteCreateOperation(true, EMultiplayerSessionFailureReason::None);
		return;
	}

	CompleteCreateOperation(false, EMultiplayerSessionFailureReason::CreateFailed);

	UE_LOG(LogMultiplayerSessionsSubsystem, Warning, TEXT("Create callback failed for session %s."), *SessionName.ToString());
}

void UMultiplayerSessionsSubsystem::OnFindSessionsCompleteInternal(bool bWasSuccessful, uint64 CallbackGeneration)
{
	if (IsCurrentOperation(CallbackGeneration, EOperationType::Find, EOperationStep::Executing) == false)
	{
		return;
	}

	ClearOperationDelegate(EOperationType::Find);

	TArray<FOnlineSessionSearchResult> NewSearchResults;
	TArray<FMultiplayerSessionBrowserEntry> NewBrowserEntries;

	if (bWasSuccessful == false || ActiveOperation.PendingSearch.IsValid() == false)
	{
		CompleteFindOperation(false, EMultiplayerSessionFailureReason::FindFailed, MoveTemp(NewSearchResults), MoveTemp(NewBrowserEntries));
		return;
	}

	for (const FOnlineSessionSearchResult& SearchResult : ActiveOperation.PendingSearch->SearchResults)
	{
		if (IsSearchResultRelevantToRequest(SearchResult, ActiveOperation.SearchRequest) == false)
		{
			continue;
		}

		NewSearchResults.Add(SearchResult);
	}

	for (int32 SearchResultIndex = 0; SearchResultIndex < NewSearchResults.Num(); SearchResultIndex++)
	{
		NewBrowserEntries.Add(BuildBrowserEntry(NewSearchResults[SearchResultIndex], SearchResultIndex, ActiveOperation.SearchRequest));
	}

	SortSearchResultsAndBrowserEntries(NewSearchResults, NewBrowserEntries);
	CompleteFindOperation(true, EMultiplayerSessionFailureReason::None, MoveTemp(NewSearchResults), MoveTemp(NewBrowserEntries));
}

void UMultiplayerSessionsSubsystem::OnFindFriendSessionCompleteInternal(int32 LocalUserNum, bool bWasSuccessful, const TArray<FOnlineSessionSearchResult>& SearchResults, uint64 CallbackGeneration)
{
	if (IsCurrentOperation(CallbackGeneration, EOperationType::FindFriend, EOperationStep::Executing) == false)
	{
		return;
	}

	if (LocalUserNum != ActiveOperation.LocalUser.LocalUserNum)
	{
		return;
	}

	ClearOperationDelegate(EOperationType::FindFriend);

	const FOnlineSessionSearchResult* FriendSessionResult = nullptr;
	if (bWasSuccessful == true)
	{
		for (const FOnlineSessionSearchResult& SearchResult : SearchResults)
		{
			if (SearchResult.IsValid() == true)
			{
				FriendSessionResult = &SearchResult;
				break;
			}
		}
	}

	if (FriendSessionResult == nullptr)
	{
		CompleteFindFriendOperation(false, EMultiplayerSessionFailureReason::FriendSessionNotFound);
		return;
	}

	FMultiplayerSessionSearchRequest CompatibilityRequest;
	CompatibilityRequest.DesiredMatchType.Reset();
	SanitizeSearchRequest(CompatibilityRequest);
	const EMultiplayerJoinBlockReason JoinBlockReason = ResolveJoinBlockReason(*FriendSessionResult, CompatibilityRequest);
	if (JoinBlockReason != EMultiplayerJoinBlockReason::None)
	{
		CompleteFindFriendOperation(false, ResolveFailureReasonForJoinBlock(JoinBlockReason));
		return;
	}

	ActiveOperation.Type = EOperationType::Join;
	ActiveOperation.JoinResult = *FriendSessionResult;
	ActiveOperation.FriendId.Reset();
	SetLastFailureReason(EMultiplayerSessionFailureReason::None);
	OnFriendSessionSearchCompleted.Broadcast(true, EMultiplayerSessionFailureReason::None);
	SetFlowState(EMultiplayerSessionFlowState::Joining);
	SetOperationStep(EOperationStep::Executing, JoinTimeoutSeconds);
	BeginJoinAfterExistingSessionCleanup();
}

void UMultiplayerSessionsSubsystem::OnSessionUserInviteAcceptedInternal(bool bWasSuccessful, int32 LocalUserNum, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult)
{
	(void)UserId;

	if (bWasSuccessful == false || InviteResult.IsValid() == false)
	{
		SetLastFailureReason(EMultiplayerSessionFailureReason::InviteFailed);
		OnSessionInviteAccepted.Broadcast(false, EMultiplayerSessionFailureReason::InviteFailed);
		return;
	}

	FMultiplayerSessionSearchRequest CompatibilityRequest;
	CompatibilityRequest.DesiredMatchType.Reset();
	SanitizeSearchRequest(CompatibilityRequest);
	const EMultiplayerJoinBlockReason JoinBlockReason = ResolveJoinBlockReason(InviteResult, CompatibilityRequest);
	if (JoinBlockReason != EMultiplayerJoinBlockReason::None)
	{
		const EMultiplayerSessionFailureReason FailureReason = ResolveFailureReasonForJoinBlock(JoinBlockReason);
		SetLastFailureReason(FailureReason);
		OnSessionInviteAccepted.Broadcast(false, FailureReason);
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	ULocalPlayer* LocalPlayer = nullptr;
	if (GameInstance != nullptr && LocalUserNum >= 0 && LocalUserNum < GameInstance->GetNumLocalPlayers())
	{
		LocalPlayer = GameInstance->GetLocalPlayerByIndex(LocalUserNum);
	}

	EMultiplayerSessionFailureReason FailureReason = EMultiplayerSessionFailureReason::None;
	if (TryBeginOperation(EOperationType::Join, EMultiplayerSessionFlowState::Joining, LocalPlayer, JoinTimeoutSeconds, FailureReason) == false)
	{
		SetLastFailureReason(FailureReason);
		OnSessionInviteAccepted.Broadcast(false, FailureReason);
		return;
	}

	ActiveOperation.JoinResult = InviteResult;
	if (ActiveOperation.LocalUser.UniqueNetId.IsValid() == false && IsOnlineLoginRequired() == true && ActiveOperation.JoinResult.Session.SessionSettings.bIsLANMatch == false)
	{
		SetLastFailureReason(EMultiplayerSessionFailureReason::NotLoggedIn);
		const uint64 CompletedOperationGeneration = ActiveOperation.Generation;
		const EMultiplayerSessionFlowState PreviousFlowState = ResetActiveOperation();
		OnSessionInviteAccepted.Broadcast(false, EMultiplayerSessionFailureReason::NotLoggedIn);
		BroadcastIdleStateIfUnchanged(PreviousFlowState, CompletedOperationGeneration);
		return;
	}

	SetLastFailureReason(EMultiplayerSessionFailureReason::None);
	OnSessionInviteAccepted.Broadcast(true, EMultiplayerSessionFailureReason::None);
	BeginJoinAfterExistingSessionCleanup();
}

void UMultiplayerSessionsSubsystem::OnJoinSessionCompleteInternal(FName SessionName, EOnJoinSessionCompleteResult::Type Result, uint64 CallbackGeneration)
{
	if (IsCurrentOperation(CallbackGeneration, EOperationType::Join, EOperationStep::Executing) == false)
	{
		return;
	}

	ClearOperationDelegate(EOperationType::Join);

	if (Result != EOnJoinSessionCompleteResult::Success)
	{
		CompleteJoinOperation(Result, MapJoinResult(Result), EMultiplayerSessionFailureReason::JoinFailed);
		return;
	}

	FString ConnectString;
	if (SessionInterface.IsValid() == false || SessionInterface->GetResolvedConnectString(NAME_GameSession, ConnectString) == false || ConnectString.IsEmpty() == true)
	{
		if (ActiveOperation.bResultBroadcast == false)
		{
			ActiveOperation.bResultBroadcast = true;
			SetLastFailureReason(EMultiplayerSessionFailureReason::TravelFailed);
			MultiplayerOnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::CouldNotRetrieveAddress);
			OnJoinSessionRequestCompleted.Broadcast(EMultiplayerJoinSessionResult::CouldNotRetrieveAddress);
		}

		BeginRecovery(EOperationType::Join);
		return;
	}

	if (BeginTravel(ConnectString) == false)
	{
		HandleTravelFailureInternal(EMultiplayerSessionFailureReason::TravelFailed);
		return;
	}

	UE_LOG(LogMultiplayerSessionsSubsystem, Log, TEXT("Join backend succeeded for session %s. Waiting for network travel confirmation."), *SessionName.ToString());
}

void UMultiplayerSessionsSubsystem::OnDestroySessionCompleteInternal(FName SessionName, bool bWasSuccessful, uint64 CallbackGeneration)
{
	if (CallbackGeneration != ActiveOperation.Generation)
	{
		return;
	}

	const bool bCreateContinuation = ActiveOperation.Type == EOperationType::Create && ActiveOperation.Step == EOperationStep::DestroyExistingForCreate;
	const bool bJoinContinuation = ActiveOperation.Type == EOperationType::Join && ActiveOperation.Step == EOperationStep::DestroyExistingForJoin;
	if (bCreateContinuation == true || bJoinContinuation == true)
	{
		ClearOperationDelegate(EOperationType::Destroy);

		const bool bSessionNoLongerExists = SessionInterface.IsValid() == true && SessionInterface->GetNamedSession(NAME_GameSession) == nullptr;
		if (bWasSuccessful == false && bSessionNoLongerExists == false)
		{
			if (bCreateContinuation == true)
			{
				CompleteCreateOperation(false, EMultiplayerSessionFailureReason::DestroyFailed);
			}
			else
			{
				CompleteJoinOperation(EOnJoinSessionCompleteResult::UnknownError, EMultiplayerJoinSessionResult::UnknownError, EMultiplayerSessionFailureReason::DestroyFailed);
			}
			return;
		}

		ResetCommittedSessionState();
		if (bCreateContinuation == true)
		{
			BeginCreateOperation();
		}
		else
		{
			BeginJoinOperation();
		}
		return;
	}

	if (IsCurrentOperation(CallbackGeneration, EOperationType::Destroy, EOperationStep::Executing) == false)
	{
		return;
	}

	ClearOperationDelegate(EOperationType::Destroy);

	if (bWasSuccessful == true)
	{
		ResetCommittedSessionState();
		CompleteDestroyOperation(true, EMultiplayerSessionFailureReason::None);
		return;
	}

	const bool bSessionNoLongerExists = SessionInterface.IsValid() == true && SessionInterface->GetNamedSession(NAME_GameSession) == nullptr;
	if (bSessionNoLongerExists == true)
	{
		ResetCommittedSessionState();
		CompleteDestroyOperation(true, EMultiplayerSessionFailureReason::None);
		return;
	}

	CompleteDestroyOperation(false, EMultiplayerSessionFailureReason::DestroyFailed);
	UE_LOG(LogMultiplayerSessionsSubsystem, Warning, TEXT("Destroy callback failed for session %s."), *SessionName.ToString());
}

void UMultiplayerSessionsSubsystem::OnUpdateSessionCompleteInternal(FName SessionName, bool bWasSuccessful, uint64 CallbackGeneration)
{
	if (IsCurrentOperation(CallbackGeneration, EOperationType::Update, EOperationStep::Executing) == false)
	{
		return;
	}

	ClearOperationDelegate(EOperationType::Update);

	if (bWasSuccessful == true)
	{
		CommittedSessionSettings = ActiveOperation.PendingSessionSettings;
		CompleteUpdateOperation(true, EMultiplayerSessionFailureReason::None);
		return;
	}

	CompleteUpdateOperation(false, EMultiplayerSessionFailureReason::UpdateFailed);
	UE_LOG(LogMultiplayerSessionsSubsystem, Warning, TEXT("Update callback failed for session %s."), *SessionName.ToString());
}

void UMultiplayerSessionsSubsystem::OnStartSessionCompleteInternal(FName SessionName, bool bWasSuccessful, uint64 CallbackGeneration)
{
	if (IsCurrentOperation(CallbackGeneration, EOperationType::Start, EOperationStep::Executing) == false)
	{
		return;
	}

	ClearOperationDelegate(EOperationType::Start);

	if (bWasSuccessful == true)
	{
		CompleteStartOperation(true, EMultiplayerSessionFailureReason::None);
		return;
	}

	CompleteStartOperation(false, EMultiplayerSessionFailureReason::StartFailed);
	UE_LOG(LogMultiplayerSessionsSubsystem, Warning, TEXT("Start callback failed for session %s."), *SessionName.ToString());
}

void UMultiplayerSessionsSubsystem::OnEndSessionCompleteInternal(FName SessionName, bool bWasSuccessful, uint64 CallbackGeneration)
{
	if (IsCurrentOperation(CallbackGeneration, EOperationType::End, EOperationStep::Executing) == false)
	{
		return;
	}

	ClearOperationDelegate(EOperationType::End);

	if (bWasSuccessful == true)
	{
		CompleteEndOperation(true, EMultiplayerSessionFailureReason::None);
		return;
	}

	CompleteEndOperation(false, EMultiplayerSessionFailureReason::EndFailed);
	UE_LOG(LogMultiplayerSessionsSubsystem, Warning, TEXT("End callback failed for session %s."), *SessionName.ToString());
}

void UMultiplayerSessionsSubsystem::OnRecoveryDestroyCompleteInternal(FName SessionName, bool bWasSuccessful, uint64 CallbackGeneration)
{
	if (CallbackGeneration != ActiveOperation.Generation || ActiveOperation.Step != EOperationStep::RecoveryDestroy)
	{
		return;
	}

	ClearOperationDelegate(EOperationType::Destroy);
	RecoveryDestroyCompleteDelegateHandle = FDelegateHandle();

	const bool bSessionNoLongerExists = SessionInterface.IsValid() == true && SessionInterface->GetNamedSession(NAME_GameSession) == nullptr;
	if (bWasSuccessful == true || bSessionNoLongerExists == true)
	{
		ResetCommittedSessionState();
		CompleteRecovery(true);
		return;
	}

	UE_LOG(LogMultiplayerSessionsSubsystem, Error, TEXT("Recovery destroy failed for session %s."), *SessionName.ToString());
	CompleteRecovery(false);
}

void UMultiplayerSessionsSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	(void)NetDriver;
	if (World != nullptr && World->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	UE_LOG(LogMultiplayerSessionsSubsystem, Warning, TEXT("Network failure. Type=%d Error=%s"), static_cast<int32>(FailureType), *ErrorString);

	if (ActiveOperation.Step == EOperationStep::WaitingForTravel)
	{
		HandleTravelFailureInternal(EMultiplayerSessionFailureReason::NetworkFailure);
		return;
	}

	BroadcastFailure(EMultiplayerSessionFailureReason::NetworkFailure);

	const bool bClientWorld = World != nullptr && World->GetNetMode() == NM_Client;
	if (bClientWorld == true && CurrentFlowState == EMultiplayerSessionFlowState::Idle && HasNamedSession() == true)
	{
		LeaveCurrentSession();
	}
}

void UMultiplayerSessionsSubsystem::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	if (World != nullptr && World->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	UE_LOG(LogMultiplayerSessionsSubsystem, Warning, TEXT("Travel failure. Type=%d Error=%s"), static_cast<int32>(FailureType), *ErrorString);

	if (ActiveOperation.Step == EOperationStep::WaitingForTravel)
	{
		HandleTravelFailureInternal(EMultiplayerSessionFailureReason::TravelFailed);
		return;
	}

	BroadcastFailure(EMultiplayerSessionFailureReason::TravelFailed);

	const bool bClientWorld = World != nullptr && World->GetNetMode() == NM_Client;
	if (bClientWorld == true && CurrentFlowState == EMultiplayerSessionFlowState::Idle && HasNamedSession() == true)
	{
		LeaveCurrentSession();
	}
}

void UMultiplayerSessionsSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (LoadedWorld == nullptr || LoadedWorld->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	if (ActiveOperation.Step != EOperationStep::WaitingForTravel)
	{
		return;
	}

	if (LoadedWorld->GetNetMode() != NM_Client)
	{
		return;
	}

	if (ActiveOperation.Type == EOperationType::Join)
	{
		CompleteJoinOperation(EOnJoinSessionCompleteResult::Success, EMultiplayerJoinSessionResult::Success, EMultiplayerSessionFailureReason::None);
		return;
	}

	if (ActiveOperation.Type == EOperationType::DirectTravel)
	{
		CompleteDirectTravelOperation(true, EMultiplayerSessionFailureReason::None);
	}
}

bool UMultiplayerSessionsSubsystem::EnsureSessionInterface(const TCHAR* Context, EMultiplayerSessionFailureReason& OutFailureReason)
{
	OutFailureReason = EMultiplayerSessionFailureReason::None;

	IOnlineSubsystem* OnlineSubsystem = nullptr;
	UWorld* World = GetWorld();
	if (World != nullptr)
	{
		OnlineSubsystem = Online::GetSubsystem(World);
	}

	if (OnlineSubsystem == nullptr)
	{
		OnlineSubsystem = IOnlineSubsystem::Get();
	}

	if (OnlineSubsystem == nullptr)
	{
		OutFailureReason = EMultiplayerSessionFailureReason::NoOnlineSubsystem;
		UE_LOG(LogMultiplayerSessionsSubsystem, Warning, TEXT("%s: No OnlineSubsystem found."), Context);
		return false;
	}

	if (CachedOnlineSubsystem != OnlineSubsystem)
	{
		ClearAllDelegateHandles();
		ResetCommittedSessionState();
		SessionInterface.Reset();
		CachedOnlineSubsystem = OnlineSubsystem;
	}

	if (SessionInterface.IsValid() == false)
	{
		SessionInterface = OnlineSubsystem->GetSessionInterface();
	}

	if (SessionInterface.IsValid() == false)
	{
		OutFailureReason = EMultiplayerSessionFailureReason::NoSessionInterface;
		UE_LOG(LogMultiplayerSessionsSubsystem, Warning, TEXT("%s: Session interface is invalid for subsystem %s."), Context, *OnlineSubsystem->GetSubsystemName().ToString());
		return false;
	}

	RegisterPersistentSessionDelegates();
	return true;
}

void UMultiplayerSessionsSubsystem::RegisterPersistentSessionDelegates()
{
	if (SessionInterface.IsValid() == false || SessionInviteAcceptedDelegateHandle.IsValid() == true)
	{
		return;
	}

	const FOnSessionUserInviteAcceptedDelegate InviteAcceptedDelegate = FOnSessionUserInviteAcceptedDelegate::CreateUObject(this, &ThisClass::OnSessionUserInviteAcceptedInternal);
	SessionInviteAcceptedDelegateHandle = SessionInterface->AddOnSessionUserInviteAcceptedDelegate_Handle(InviteAcceptedDelegate);
}

void UMultiplayerSessionsSubsystem::ClearPersistentSessionDelegates()
{
	if (SessionInterface.IsValid() == true && SessionInviteAcceptedDelegateHandle.IsValid() == true)
	{
		SessionInterface->ClearOnSessionUserInviteAcceptedDelegate_Handle(SessionInviteAcceptedDelegateHandle);
	}

	SessionInviteAcceptedDelegateHandle = FDelegateHandle();
}

bool UMultiplayerSessionsSubsystem::ResolveLocalUser(ULocalPlayer* RequestedLocalPlayer, FLocalUserContext& OutLocalUser, EMultiplayerSessionFailureReason& OutFailureReason)
{
	OutLocalUser = FLocalUserContext();
	OutFailureReason = EMultiplayerSessionFailureReason::None;

	ULocalPlayer* LocalPlayer = RequestedLocalPlayer;
	if (LocalPlayer == nullptr)
	{
		LocalPlayer = ResolveDefaultLocalPlayer();
	}

	if (LocalPlayer == nullptr)
	{
		OutFailureReason = EMultiplayerSessionFailureReason::InvalidLocalPlayer;
		return false;
	}

	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance == nullptr)
	{
		OutFailureReason = EMultiplayerSessionFailureReason::InvalidLocalPlayer;
		return false;
	}

	int32 LocalUserNum = INDEX_NONE;
	for (int32 LocalPlayerIndex = 0; LocalPlayerIndex < GameInstance->GetNumLocalPlayers(); LocalPlayerIndex++)
	{
		if (GameInstance->GetLocalPlayerByIndex(LocalPlayerIndex) == LocalPlayer)
		{
			LocalUserNum = LocalPlayerIndex;
			break;
		}
	}

	if (LocalUserNum == INDEX_NONE)
	{
		OutFailureReason = EMultiplayerSessionFailureReason::InvalidLocalPlayer;
		return false;
	}

	OutLocalUser.LocalPlayer = LocalPlayer;
	OutLocalUser.LocalUserNum = LocalUserNum;

	const FUniqueNetIdRepl PreferredUniqueNetId = LocalPlayer->GetPreferredUniqueNetId();
	if (PreferredUniqueNetId.IsValid() == true)
	{
		OutLocalUser.UniqueNetId = PreferredUniqueNetId.GetUniqueNetId();
	}

	if (OutLocalUser.UniqueNetId.IsValid() == false && CachedOnlineSubsystem != nullptr)
	{
		IOnlineIdentityPtr IdentityInterface = CachedOnlineSubsystem->GetIdentityInterface();
		if (IdentityInterface.IsValid() == true)
		{
			OutLocalUser.UniqueNetId = IdentityInterface->GetUniquePlayerId(LocalUserNum);
		}
	}

	return true;
}

ULocalPlayer* UMultiplayerSessionsSubsystem::ResolveDefaultLocalPlayer() const
{
	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance == nullptr)
	{
		return nullptr;
	}

	return GameInstance->GetFirstGamePlayer();
}

bool UMultiplayerSessionsSubsystem::IsOnlineLoginRequired() const
{
	if (CachedOnlineSubsystem == nullptr)
	{
		return false;
	}

	return CachedOnlineSubsystem->GetSubsystemName() != FName(TEXT("NULL"));
}

