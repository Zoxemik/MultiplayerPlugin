// Copyright (c) 2026 Zoxemik. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultiplayerSessionsTypes.generated.h"

UENUM(BlueprintType)
enum class EMultiplayerSessionFlowState : uint8
{
	Idle UMETA(DisplayName = "Idle"),
	Creating UMETA(DisplayName = "Creating"),
	Finding UMETA(DisplayName = "Finding"),
	Joining UMETA(DisplayName = "Joining"),
	Destroying UMETA(DisplayName = "Destroying"),
	Updating UMETA(DisplayName = "Updating"),
	Traveling UMETA(DisplayName = "Traveling"),
	Recovering UMETA(DisplayName = "Recovering"),
	Starting UMETA(DisplayName = "Starting"),
	Ending UMETA(DisplayName = "Ending")
};

UENUM(BlueprintType)
enum class EMultiplayerAdvertisedSessionStatus : uint8
{
	Unknown UMETA(DisplayName = "Unknown"),
	Lobby UMETA(DisplayName = "Lobby"),
	Starting UMETA(DisplayName = "Starting"),
	InMatch UMETA(DisplayName = "In Match"),
	Full UMETA(DisplayName = "Full")
};

UENUM(BlueprintType)
enum class EMultiplayerJoinBlockReason : uint8
{
	None UMETA(DisplayName = "None"),
	IncompatibleBuild UMETA(DisplayName = "Incompatible Build"),
	IncompatibleSchema UMETA(DisplayName = "Incompatible Session Schema"),
	StatusUnavailable UMETA(DisplayName = "Status Unavailable"),
	MatchStarting UMETA(DisplayName = "Match Starting"),
	SessionFull UMETA(DisplayName = "Session Full"),
	JoinInProgressDisabled UMETA(DisplayName = "Join In Progress Disabled")
};

UENUM(BlueprintType)
enum class EMultiplayerSessionFailureReason : uint8
{
	None UMETA(DisplayName = "None"),
	NoOnlineSubsystem UMETA(DisplayName = "No Online Subsystem"),
	NoSessionInterface UMETA(DisplayName = "No Session Interface"),
	CreateFailed UMETA(DisplayName = "Create Failed"),
	FindFailed UMETA(DisplayName = "Find Failed"),
	JoinFailed UMETA(DisplayName = "Join Failed"),
	TravelFailed UMETA(DisplayName = "Travel Failed"),
	DestroyFailed UMETA(DisplayName = "Destroy Failed"),
	UpdateFailed UMETA(DisplayName = "Update Failed"),
	InvalidSearchResultIndex UMETA(DisplayName = "Invalid Search Result Index"),
	InvalidAddress UMETA(DisplayName = "Invalid Address"),
	Busy UMETA(DisplayName = "Busy"),
	Timeout UMETA(DisplayName = "Timeout"),
	Cancelled UMETA(DisplayName = "Cancelled"),
	InvalidLocalPlayer UMETA(DisplayName = "Invalid Local Player"),
	NotLoggedIn UMETA(DisplayName = "Not Logged In"),
	NetworkFailure UMETA(DisplayName = "Network Failure"),
	RecoveryFailed UMETA(DisplayName = "Recovery Failed"),
	IncompatibleBuild UMETA(DisplayName = "Incompatible Build"),
	InvalidSessionSchema UMETA(DisplayName = "Invalid Session Schema"),
	StartFailed UMETA(DisplayName = "Start Failed"),
	EndFailed UMETA(DisplayName = "End Failed"),
	SessionAlreadyExists UMETA(DisplayName = "Session Already Exists"),
	NotSessionOwner UMETA(DisplayName = "Not Session Owner"),
	InvalidFriendId UMETA(DisplayName = "Invalid Friend Id"),
	InviteFailed UMETA(DisplayName = "Invite Failed"),
	PlatformUiUnavailable UMETA(DisplayName = "Platform UI Unavailable"),
	FriendSessionNotFound UMETA(DisplayName = "Friend Session Not Found")
};

UENUM(BlueprintType)
enum class EMultiplayerJoinSessionResult : uint8
{
	Success UMETA(DisplayName = "Success"),
	SessionIsFull UMETA(DisplayName = "Session Is Full"),
	SessionDoesNotExist UMETA(DisplayName = "Session Does Not Exist"),
	SessionNotJoinable UMETA(DisplayName = "Session Not Joinable"),
	CouldNotRetrieveAddress UMETA(DisplayName = "Could Not Retrieve Address"),
	AlreadyInSession UMETA(DisplayName = "Already In Session"),
	UnknownError UMETA(DisplayName = "Unknown Error"),
	Busy UMETA(DisplayName = "Busy"),
	Timeout UMETA(DisplayName = "Timeout"),
	TravelFailed UMETA(DisplayName = "Travel Failed"),
	IncompatibleBuild UMETA(DisplayName = "Incompatible Build"),
	IncompatibleSchema UMETA(DisplayName = "Incompatible Session Schema")
};

USTRUCT(BlueprintType)
struct FMultiplayerSessionCreateRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session")
	int32 NumPublicConnections = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session")
	FString MatchType = TEXT("TowerOnline");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session")
	FString SessionDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session")
	FString HostDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session")
	FString MapName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session")
	EMultiplayerAdvertisedSessionStatus InitialStatus = EMultiplayerAdvertisedSessionStatus::Lobby;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session", meta = (ToolTip = "Set to zero to use the local Unreal network version automatically."))
	int32 BuildId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session", meta = (ClampMin = "1"))
	int32 SessionSchemaVersion = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session")
	bool bUseLan = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session")
	bool bAllowJoinInProgress = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session|Presence")
	bool bAllowInvites = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session|Presence")
	bool bAllowJoinViaPresence = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session|Presence")
	bool bAllowJoinViaPresenceFriendsOnly = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session")
	bool bReplaceExistingSession = true;
};

USTRUCT(BlueprintType)
struct FMultiplayerSessionSearchRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session Search")
	int32 MaxSearchResults = 200;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session Search")
	FString DesiredMatchType = TEXT("TowerOnline");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session Search", meta = (ToolTip = "Set to zero to use the local Unreal network version automatically."))
	int32 DesiredBuildId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session Search", meta = (ClampMin = "1"))
	int32 DesiredSessionSchemaVersion = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session Search")
	bool bUseLan = false;
};

USTRUCT(BlueprintType)
struct FMultiplayerSessionBrowserEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Session Browser")
	FString EntryId;

	UPROPERTY(BlueprintReadOnly, Category = "Session Browser")
	int32 SearchResultIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Session Browser")
	FString SessionId;

	UPROPERTY(BlueprintReadOnly, Category = "Session Browser")
	FString SessionDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Session Browser")
	FString HostDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Session Browser")
	FString MatchType;

	UPROPERTY(BlueprintReadOnly, Category = "Session Browser")
	FString MapName;

	UPROPERTY(BlueprintReadOnly, Category = "Session Browser")
	EMultiplayerAdvertisedSessionStatus AdvertisedStatus = EMultiplayerAdvertisedSessionStatus::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Session Browser")
	FString AdvertisedStatusText;

	UPROPERTY(BlueprintReadOnly, Category = "Session Browser")
	EMultiplayerAdvertisedSessionStatus Status = EMultiplayerAdvertisedSessionStatus::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Session Browser")
	FString StatusText;

	UPROPERTY(BlueprintReadOnly, Category = "Session Browser")
	int32 BuildId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Session Browser")
	int32 SessionSchemaVersion = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Session Browser")
	int32 CurrentPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Session Browser")
	int32 MaxPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Session Browser")
	int32 OpenPublicConnections = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Session Browser")
	int32 PingInMs = -1;

	UPROPERTY(BlueprintReadOnly, Category = "Session Browser")
	bool bIsLan = false;

	UPROPERTY(BlueprintReadOnly, Category = "Session Browser")
	bool bCanJoin = false;

	UPROPERTY(BlueprintReadOnly, Category = "Session Browser")
	EMultiplayerJoinBlockReason JoinBlockReason = EMultiplayerJoinBlockReason::None;

	UPROPERTY(BlueprintReadOnly, Category = "Session Browser")
	FString JoinDisabledReasonText;
};
