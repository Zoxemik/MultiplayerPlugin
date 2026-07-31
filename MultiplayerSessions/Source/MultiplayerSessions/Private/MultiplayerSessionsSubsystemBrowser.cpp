// Copyright (c) 2026 Zoxemik. All rights reserved.

#include "MultiplayerSessionsSubsystem.h"

#include "MultiplayerSessionsPrivate.h"
#include "Misc/NetworkVersion.h"
#include "OnlineSessionSettings.h"

void UMultiplayerSessionsSubsystem::SanitizeCreateRequest(FMultiplayerSessionCreateRequest& InOutCreateRequest) const
{
	InOutCreateRequest.NumPublicConnections = FMath::Max(1, InOutCreateRequest.NumPublicConnections);
	InOutCreateRequest.MatchType.TrimStartAndEndInline();
	if (InOutCreateRequest.MatchType.IsEmpty() == true)
	{
		InOutCreateRequest.MatchType = TEXT("Default");
	}

	InOutCreateRequest.BuildId = ResolveBuildId(InOutCreateRequest.BuildId);
	InOutCreateRequest.SessionSchemaVersion = FMath::Max(1, InOutCreateRequest.SessionSchemaVersion);
}

void UMultiplayerSessionsSubsystem::SanitizeSearchRequest(FMultiplayerSessionSearchRequest& InOutSearchRequest) const
{
	InOutSearchRequest.MaxSearchResults = FMath::Max(1, InOutSearchRequest.MaxSearchResults);
	InOutSearchRequest.DesiredMatchType.TrimStartAndEndInline();
	InOutSearchRequest.DesiredBuildId = ResolveBuildId(InOutSearchRequest.DesiredBuildId);
	InOutSearchRequest.DesiredSessionSchemaVersion = FMath::Max(1, InOutSearchRequest.DesiredSessionSchemaVersion);
}

int32 UMultiplayerSessionsSubsystem::ResolveBuildId(int32 RequestedBuildId) const
{
	if (bAllowBuildIdOverride == true && RequestedBuildId > 0)
	{
		return RequestedBuildId;
	}

	const uint32 NetworkVersion = FNetworkVersion::GetLocalNetworkVersion();
	int32 BuildId = static_cast<int32>(NetworkVersion & 0x7fffffffu);
	if (BuildId <= 0)
	{
		BuildId = 1;
	}

	return BuildId;
}

void UMultiplayerSessionsSubsystem::ApplyCreateRequestToSessionSettings(FOnlineSessionSettings& SessionSettings, const FMultiplayerSessionCreateRequest& CreateRequest) const
{
	SessionSettings.bIsLANMatch = CreateRequest.bUseLan;

	const UWorld* World = GetWorld();
	SessionSettings.bIsDedicated = World != nullptr && World->GetNetMode() == NM_DedicatedServer;

	SessionSettings.NumPublicConnections = CreateRequest.NumPublicConnections;
	SessionSettings.NumPrivateConnections = 0;
	SessionSettings.bAllowJoinInProgress = CreateRequest.bAllowJoinInProgress;
	SessionSettings.bShouldAdvertise = true;
	SessionSettings.bAllowInvites = false;
	SessionSettings.bAllowJoinViaPresence = false;
	SessionSettings.bAllowJoinViaPresenceFriendsOnly = false;
	SessionSettings.bUsesPresence = false;
	SessionSettings.bUseLobbiesIfAvailable = false;
	SessionSettings.BuildUniqueId = GetBuildUniqueId();

	if (CreateRequest.bUseLan == false)
	{
		SessionSettings.bAllowInvites = CreateRequest.bAllowInvites;
		SessionSettings.bAllowJoinViaPresence = CreateRequest.bAllowJoinViaPresence;
		SessionSettings.bAllowJoinViaPresenceFriendsOnly = CreateRequest.bAllowJoinViaPresenceFriendsOnly;
		SessionSettings.bUsesPresence = true;
		SessionSettings.bUseLobbiesIfAvailable = true;
	}

	SessionSettings.Set(MultiplayerSessionsKeys::MatchType, CreateRequest.MatchType, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	SessionSettings.Set(MultiplayerSessionsKeys::Status, SessionStatusToString(CreateRequest.InitialStatus), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	SessionSettings.Set(MultiplayerSessionsKeys::BuildId, CreateRequest.BuildId, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	SessionSettings.Set(MultiplayerSessionsKeys::SchemaVersion, CreateRequest.SessionSchemaVersion, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	SessionSettings.Set(MultiplayerSessionsKeys::ConfiguredAllowJoinInProgress, CreateRequest.bAllowJoinInProgress, EOnlineDataAdvertisementType::DontAdvertise);

	if (CreateRequest.SessionDisplayName.IsEmpty() == false)
	{
		SessionSettings.Set(MultiplayerSessionsKeys::DisplayName, CreateRequest.SessionDisplayName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	}

	if (CreateRequest.HostDisplayName.IsEmpty() == false)
	{
		SessionSettings.Set(MultiplayerSessionsKeys::HostDisplayName, CreateRequest.HostDisplayName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	}

	if (CreateRequest.MapName.IsEmpty() == false)
	{
		SessionSettings.Set(MultiplayerSessionsKeys::MapName, CreateRequest.MapName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	}

	FString InvitesText = TEXT("false");
	if (SessionSettings.bAllowInvites == true)
	{
		InvitesText = TEXT("true");
	}

	FString PresenceJoinText = TEXT("false");
	if (SessionSettings.bAllowJoinViaPresence == true)
	{
		PresenceJoinText = TEXT("true");
	}

	UE_LOG(
		LogMultiplayerSessionsSubsystem,
		Log,
		TEXT("Prepared session settings. OnlineBuildUniqueId=0x%08x, CompatibilityBuildId=%d, SchemaVersion=%d, Invites=%s, PresenceJoin=%s."),
		static_cast<uint32>(SessionSettings.BuildUniqueId),
		CreateRequest.BuildId,
		CreateRequest.SessionSchemaVersion,
		*InvitesText,
		*PresenceJoinText
	);
}

bool UMultiplayerSessionsSubsystem::IsSearchResultRelevantToRequest(const FOnlineSessionSearchResult& SearchResult, const FMultiplayerSessionSearchRequest& SearchRequest) const
{
	if (SearchResult.IsValid() == false)
	{
		return false;
	}

	if (SearchRequest.DesiredMatchType.IsEmpty() == false)
	{
		FString FoundMatchType;
		if (SearchResult.Session.SessionSettings.Get(MultiplayerSessionsKeys::MatchType, FoundMatchType) == false)
		{
			return false;
		}

		if (FoundMatchType.Equals(SearchRequest.DesiredMatchType, ESearchCase::IgnoreCase) == false)
		{
			return false;
		}
	}

	return true;
}

EMultiplayerJoinBlockReason UMultiplayerSessionsSubsystem::ResolveJoinBlockReason(const FOnlineSessionSearchResult& SearchResult, const FMultiplayerSessionSearchRequest& SearchRequest) const
{
	if (SearchResult.IsValid() == false)
	{
		return EMultiplayerJoinBlockReason::StatusUnavailable;
	}

	int32 FoundBuildId = 0;
	if (SearchResult.Session.SessionSettings.Get(MultiplayerSessionsKeys::BuildId, FoundBuildId) == false || FoundBuildId <= 0 || FoundBuildId != SearchRequest.DesiredBuildId)
	{
		return EMultiplayerJoinBlockReason::IncompatibleBuild;
	}

	int32 FoundSchemaVersion = 0;
	if (SearchResult.Session.SessionSettings.Get(MultiplayerSessionsKeys::SchemaVersion, FoundSchemaVersion) == false || FoundSchemaVersion != SearchRequest.DesiredSessionSchemaVersion)
	{
		return EMultiplayerJoinBlockReason::IncompatibleSchema;
	}

	FString AdvertisedStatusText;
	if (SearchResult.Session.SessionSettings.Get(MultiplayerSessionsKeys::Status, AdvertisedStatusText) == false)
	{
		return EMultiplayerJoinBlockReason::StatusUnavailable;
	}

	const EMultiplayerAdvertisedSessionStatus AdvertisedStatus = SessionStatusFromString(AdvertisedStatusText);
	if (AdvertisedStatus == EMultiplayerAdvertisedSessionStatus::Unknown)
	{
		return EMultiplayerJoinBlockReason::StatusUnavailable;
	}

	const int32 OpenPublicConnections = FMath::Max(0, SearchResult.Session.NumOpenPublicConnections);
	const EMultiplayerAdvertisedSessionStatus DisplayStatus = ResolveDisplayStatus(AdvertisedStatus, OpenPublicConnections);
	if (DisplayStatus == EMultiplayerAdvertisedSessionStatus::Starting)
	{
		return EMultiplayerJoinBlockReason::MatchStarting;
	}

	if (OpenPublicConnections <= 0 || DisplayStatus == EMultiplayerAdvertisedSessionStatus::Full)
	{
		return EMultiplayerJoinBlockReason::SessionFull;
	}

	if (DisplayStatus == EMultiplayerAdvertisedSessionStatus::InMatch && SearchResult.Session.SessionSettings.bAllowJoinInProgress == false)
	{
		return EMultiplayerJoinBlockReason::JoinInProgressDisabled;
	}

	return EMultiplayerJoinBlockReason::None;
}

FMultiplayerSessionBrowserEntry UMultiplayerSessionsSubsystem::BuildBrowserEntry(const FOnlineSessionSearchResult& SearchResult, int32 SearchResultIndex, const FMultiplayerSessionSearchRequest& SearchRequest) const
{
	FMultiplayerSessionBrowserEntry BrowserEntry;
	BrowserEntry.SearchResultIndex = SearchResultIndex;
	BrowserEntry.SessionId = SearchResult.GetSessionIdStr();
	BrowserEntry.EntryId = BrowserEntry.SessionId;
	BrowserEntry.bIsLan = SearchResult.Session.SessionSettings.bIsLANMatch;
	BrowserEntry.PingInMs = SearchResult.PingInMs;
	BrowserEntry.MaxPlayers = SearchResult.Session.SessionSettings.NumPublicConnections;
	BrowserEntry.OpenPublicConnections = FMath::Max(0, SearchResult.Session.NumOpenPublicConnections);
	BrowserEntry.CurrentPlayers = FMath::Max(0, BrowserEntry.MaxPlayers - BrowserEntry.OpenPublicConnections);
	BrowserEntry.BuildId = 0;

	SearchResult.Session.SessionSettings.Get(MultiplayerSessionsKeys::BuildId, BrowserEntry.BuildId);
	SearchResult.Session.SessionSettings.Get(MultiplayerSessionsKeys::SchemaVersion, BrowserEntry.SessionSchemaVersion);

	FString Value;
	if (SearchResult.Session.SessionSettings.Get(MultiplayerSessionsKeys::DisplayName, Value) == true)
	{
		BrowserEntry.SessionDisplayName = Value;
	}

	if (SearchResult.Session.SessionSettings.Get(MultiplayerSessionsKeys::HostDisplayName, Value) == true)
	{
		BrowserEntry.HostDisplayName = Value;
	}
	else
	{
		BrowserEntry.HostDisplayName = SearchResult.Session.OwningUserName;
	}

	if (BrowserEntry.HostDisplayName.IsEmpty() == true)
	{
		BrowserEntry.HostDisplayName = TEXT("Host");
	}

	if (SearchResult.Session.SessionSettings.Get(MultiplayerSessionsKeys::MatchType, Value) == true)
	{
		BrowserEntry.MatchType = Value;
	}

	if (SearchResult.Session.SessionSettings.Get(MultiplayerSessionsKeys::MapName, Value) == true)
	{
		BrowserEntry.MapName = Value;
	}

	if (SearchResult.Session.SessionSettings.Get(MultiplayerSessionsKeys::Status, Value) == true)
	{
		BrowserEntry.AdvertisedStatus = SessionStatusFromString(Value);
	}

	BrowserEntry.AdvertisedStatusText = SessionStatusToString(BrowserEntry.AdvertisedStatus);
	BrowserEntry.Status = ResolveDisplayStatus(BrowserEntry.AdvertisedStatus, BrowserEntry.OpenPublicConnections);
	BrowserEntry.StatusText = ResolveDisplayStatusText(BrowserEntry.Status);

	if (BrowserEntry.SessionDisplayName.IsEmpty() == true)
	{
		BrowserEntry.SessionDisplayName = BrowserEntry.HostDisplayName;
	}

	if (BrowserEntry.EntryId.IsEmpty() == true)
	{
		FString OwningUserId;
		if (SearchResult.Session.OwningUserId.IsValid() == true)
		{
			OwningUserId = SearchResult.Session.OwningUserId->ToString();
		}

		BrowserEntry.EntryId = FString::Printf(TEXT("%s|%s|%s"), *OwningUserId, *BrowserEntry.HostDisplayName, *BrowserEntry.MatchType);
	}

	ResolveJoinability(BrowserEntry, SearchResult, SearchRequest);
	return BrowserEntry;
}

void UMultiplayerSessionsSubsystem::ResolveJoinability(FMultiplayerSessionBrowserEntry& BrowserEntry, const FOnlineSessionSearchResult& SearchResult, const FMultiplayerSessionSearchRequest& SearchRequest) const
{
	BrowserEntry.JoinBlockReason = ResolveJoinBlockReason(SearchResult, SearchRequest);
	BrowserEntry.bCanJoin = BrowserEntry.JoinBlockReason == EMultiplayerJoinBlockReason::None;
	BrowserEntry.JoinDisabledReasonText = ResolveJoinBlockReasonText(BrowserEntry.JoinBlockReason);
}

void UMultiplayerSessionsSubsystem::SortSearchResultsAndBrowserEntries(TArray<FOnlineSessionSearchResult>& SearchResults, TArray<FMultiplayerSessionBrowserEntry>& BrowserEntries) const
{
	struct FSortedSessionPair
	{
		FOnlineSessionSearchResult SearchResult;
		FMultiplayerSessionBrowserEntry BrowserEntry;
	};

	TArray<FSortedSessionPair> SessionPairs;
	SessionPairs.Reserve(SearchResults.Num());

	for (int32 Index = 0; Index < SearchResults.Num(); Index++)
	{
		if (BrowserEntries.IsValidIndex(Index) == false)
		{
			continue;
		}

		FSortedSessionPair Pair;
		Pair.SearchResult = SearchResults[Index];
		Pair.BrowserEntry = BrowserEntries[Index];
		SessionPairs.Add(Pair);
	}

	SessionPairs.Sort(
		[](const FSortedSessionPair& Left, const FSortedSessionPair& Right)
		{
			if (Left.BrowserEntry.bCanJoin != Right.BrowserEntry.bCanJoin)
			{
				return Left.BrowserEntry.bCanJoin > Right.BrowserEntry.bCanJoin;
			}

			const int32 LeftStatusPriority = UMultiplayerSessionsSubsystem::ResolveStatusSortPriority(Left.BrowserEntry.Status);
			const int32 RightStatusPriority = UMultiplayerSessionsSubsystem::ResolveStatusSortPriority(Right.BrowserEntry.Status);
			if (LeftStatusPriority != RightStatusPriority)
			{
				return LeftStatusPriority < RightStatusPriority;
			}

			int32 LeftPing = Left.BrowserEntry.PingInMs;
			int32 RightPing = Right.BrowserEntry.PingInMs;
			if (LeftPing < 0)
			{
				LeftPing = TNumericLimits<int32>::Max();
			}
			if (RightPing < 0)
			{
				RightPing = TNumericLimits<int32>::Max();
			}
			if (LeftPing != RightPing)
			{
				return LeftPing < RightPing;
			}

			if (Left.BrowserEntry.OpenPublicConnections != Right.BrowserEntry.OpenPublicConnections)
			{
				return Left.BrowserEntry.OpenPublicConnections > Right.BrowserEntry.OpenPublicConnections;
			}

			return Left.BrowserEntry.HostDisplayName < Right.BrowserEntry.HostDisplayName;
		}
	);

	SearchResults.Reset();
	BrowserEntries.Reset();

	for (int32 SortedIndex = 0; SortedIndex < SessionPairs.Num(); SortedIndex++)
	{
		FSortedSessionPair& Pair = SessionPairs[SortedIndex];
		Pair.BrowserEntry.SearchResultIndex = SortedIndex;
		SearchResults.Add(Pair.SearchResult);
		BrowserEntries.Add(Pair.BrowserEntry);
	}
}

int32 UMultiplayerSessionsSubsystem::FindCachedSearchResultIndexByEntryId(const FString& EntryId) const
{
	for (int32 Index = 0; Index < CachedBrowserEntries.Num(); Index++)
	{
		if (CachedBrowserEntries[Index].EntryId.Equals(EntryId, ESearchCase::CaseSensitive) == true)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

EMultiplayerAdvertisedSessionStatus UMultiplayerSessionsSubsystem::ResolveDisplayStatus(EMultiplayerAdvertisedSessionStatus AdvertisedStatus, int32 OpenPublicConnections)
{
	if (OpenPublicConnections <= 0)
	{
		return EMultiplayerAdvertisedSessionStatus::Full;
	}

	return AdvertisedStatus;
}

FString UMultiplayerSessionsSubsystem::ResolveDisplayStatusText(EMultiplayerAdvertisedSessionStatus Status)
{
	if (Status == EMultiplayerAdvertisedSessionStatus::Lobby)
	{
		return TEXT("Lobby");
	}
	if (Status == EMultiplayerAdvertisedSessionStatus::Starting)
	{
		return TEXT("Starting");
	}
	if (Status == EMultiplayerAdvertisedSessionStatus::InMatch)
	{
		return TEXT("In Match");
	}
	if (Status == EMultiplayerAdvertisedSessionStatus::Full)
	{
		return TEXT("Full");
	}

	return TEXT("Unknown");
}

int32 UMultiplayerSessionsSubsystem::ResolveStatusSortPriority(EMultiplayerAdvertisedSessionStatus Status)
{
	if (Status == EMultiplayerAdvertisedSessionStatus::Lobby)
	{
		return 0;
	}
	if (Status == EMultiplayerAdvertisedSessionStatus::Starting)
	{
		return 1;
	}
	if (Status == EMultiplayerAdvertisedSessionStatus::Unknown)
	{
		return 2;
	}
	if (Status == EMultiplayerAdvertisedSessionStatus::InMatch)
	{
		return 3;
	}
	if (Status == EMultiplayerAdvertisedSessionStatus::Full)
	{
		return 4;
	}

	return 5;
}

EMultiplayerJoinSessionResult UMultiplayerSessionsSubsystem::ResolvePreJoinFailureResult(const FMultiplayerSessionBrowserEntry& BrowserEntry)
{
	if (BrowserEntry.JoinBlockReason == EMultiplayerJoinBlockReason::SessionFull)
	{
		return EMultiplayerJoinSessionResult::SessionIsFull;
	}
	if (BrowserEntry.JoinBlockReason == EMultiplayerJoinBlockReason::IncompatibleBuild)
	{
		return EMultiplayerJoinSessionResult::IncompatibleBuild;
	}
	if (BrowserEntry.JoinBlockReason == EMultiplayerJoinBlockReason::IncompatibleSchema)
	{
		return EMultiplayerJoinSessionResult::IncompatibleSchema;
	}

	return EMultiplayerJoinSessionResult::SessionNotJoinable;
}

EMultiplayerSessionFailureReason UMultiplayerSessionsSubsystem::ResolveFailureReasonForJoinBlock(EMultiplayerJoinBlockReason JoinBlockReason)
{
	if (JoinBlockReason == EMultiplayerJoinBlockReason::None)
	{
		return EMultiplayerSessionFailureReason::None;
	}
	if (JoinBlockReason == EMultiplayerJoinBlockReason::IncompatibleBuild)
	{
		return EMultiplayerSessionFailureReason::IncompatibleBuild;
	}
	if (JoinBlockReason == EMultiplayerJoinBlockReason::IncompatibleSchema)
	{
		return EMultiplayerSessionFailureReason::InvalidSessionSchema;
	}

	return EMultiplayerSessionFailureReason::JoinFailed;
}

FString UMultiplayerSessionsSubsystem::ResolveJoinBlockReasonText(EMultiplayerJoinBlockReason JoinBlockReason)
{
	if (JoinBlockReason == EMultiplayerJoinBlockReason::IncompatibleBuild)
	{
		return TEXT("Session uses an incompatible game build.");
	}
	if (JoinBlockReason == EMultiplayerJoinBlockReason::IncompatibleSchema)
	{
		return TEXT("Session data version is incompatible.");
	}
	if (JoinBlockReason == EMultiplayerJoinBlockReason::StatusUnavailable)
	{
		return TEXT("Session status is unavailable.");
	}
	if (JoinBlockReason == EMultiplayerJoinBlockReason::MatchStarting)
	{
		return TEXT("Match is starting.");
	}
	if (JoinBlockReason == EMultiplayerJoinBlockReason::SessionFull)
	{
		return TEXT("Session is full.");
	}
	if (JoinBlockReason == EMultiplayerJoinBlockReason::JoinInProgressDisabled)
	{
		return TEXT("Match already started.");
	}

	return TEXT("");
}

FString UMultiplayerSessionsSubsystem::SessionStatusToString(EMultiplayerAdvertisedSessionStatus Status)
{
	if (Status == EMultiplayerAdvertisedSessionStatus::Lobby)
	{
		return TEXT("Lobby");
	}
	if (Status == EMultiplayerAdvertisedSessionStatus::Starting)
	{
		return TEXT("Starting");
	}
	if (Status == EMultiplayerAdvertisedSessionStatus::InMatch)
	{
		return TEXT("InMatch");
	}
	if (Status == EMultiplayerAdvertisedSessionStatus::Full)
	{
		return TEXT("Full");
	}

	return TEXT("Unknown");
}

EMultiplayerAdvertisedSessionStatus UMultiplayerSessionsSubsystem::SessionStatusFromString(const FString& StatusText)
{
	if (StatusText.Equals(TEXT("Lobby"), ESearchCase::IgnoreCase) == true)
	{
		return EMultiplayerAdvertisedSessionStatus::Lobby;
	}
	if (StatusText.Equals(TEXT("Starting"), ESearchCase::IgnoreCase) == true)
	{
		return EMultiplayerAdvertisedSessionStatus::Starting;
	}
	if (StatusText.Equals(TEXT("InMatch"), ESearchCase::IgnoreCase) == true || StatusText.Equals(TEXT("In Match"), ESearchCase::IgnoreCase) == true)
	{
		return EMultiplayerAdvertisedSessionStatus::InMatch;
	}
	if (StatusText.Equals(TEXT("Full"), ESearchCase::IgnoreCase) == true)
	{
		return EMultiplayerAdvertisedSessionStatus::Full;
	}

	return EMultiplayerAdvertisedSessionStatus::Unknown;
}

EMultiplayerJoinSessionResult UMultiplayerSessionsSubsystem::MapJoinResult(EOnJoinSessionCompleteResult::Type Result)
{
	if (Result == EOnJoinSessionCompleteResult::Success)
	{
		return EMultiplayerJoinSessionResult::Success;
	}
	if (Result == EOnJoinSessionCompleteResult::SessionIsFull)
	{
		return EMultiplayerJoinSessionResult::SessionIsFull;
	}
	if (Result == EOnJoinSessionCompleteResult::SessionDoesNotExist)
	{
		return EMultiplayerJoinSessionResult::SessionDoesNotExist;
	}
	if (Result == EOnJoinSessionCompleteResult::CouldNotRetrieveAddress)
	{
		return EMultiplayerJoinSessionResult::CouldNotRetrieveAddress;
	}
	if (Result == EOnJoinSessionCompleteResult::AlreadyInSession)
	{
		return EMultiplayerJoinSessionResult::AlreadyInSession;
	}

	return EMultiplayerJoinSessionResult::UnknownError;
}
