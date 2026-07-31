// Copyright (c) 2026 Zoxemik. All rights reserved.

#include "MultiplayerEntryWidget.h"

#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/EditableTextBox.h"
#include "Components/ListView.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Kismet/GameplayStatics.h"
#include "MultiplayerSessionListItem.h"
#include "MultiplayerSessionProfileProvider.h"
#include "MultiplayerSessionsSubsystem.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"

void UMultiplayerEntryWidget::MenuSetup(int32 NumberOfPublicConnections, const FString& TypeOfMatch, const FString& InLobbyMapPath, const FString& InMainMenuMapPath)
{
	NumPublicConnections = FMath::Max(1, NumberOfPublicConnections);
	MatchType = TypeOfMatch;
	LobbyMapPath = InLobbyMapPath;
	MainMenuMapPath = InMainMenuMapPath;

	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance != nullptr)
	{
		MultiplayerSessionsSubsystem = GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>();
	}

	ResolveSubsystemMode();
	UpdateConnectionModeText();
	BindSubsystemDelegates();
	ClearSessionListItems();
	SetBusyState(false);
	SetStatusText(BuildCurrentModeStatusText());
}

void UMultiplayerEntryWidget::DebugPopulateFakeSessions()
{
	TArray<FMultiplayerSessionBrowserEntry> DebugEntries;

	FMultiplayerSessionBrowserEntry FirstEntry;
	FirstEntry.EntryId = TEXT("DebugSession-1");
	FirstEntry.SearchResultIndex = 0;
	FirstEntry.HostDisplayName = TEXT("PlayerOne");
	FirstEntry.StatusText = TEXT("Lobby");
	FirstEntry.OpenPublicConnections = 3;
	FirstEntry.PingInMs = 28;
	FirstEntry.bCanJoin = true;
	DebugEntries.Add(FirstEntry);

	FMultiplayerSessionBrowserEntry SecondEntry;
	SecondEntry.EntryId = TEXT("DebugSession-2");
	SecondEntry.SearchResultIndex = 1;
	SecondEntry.HostDisplayName = TEXT("PlayerTwo");
	SecondEntry.StatusText = TEXT("In Match");
	SecondEntry.OpenPublicConnections = 1;
	SecondEntry.PingInMs = 54;
	SecondEntry.bCanJoin = true;
	DebugEntries.Add(SecondEntry);

	FMultiplayerSessionBrowserEntry ThirdEntry;
	ThirdEntry.EntryId = TEXT("DebugSession-3");
	ThirdEntry.SearchResultIndex = 2;
	ThirdEntry.HostDisplayName = TEXT("PlayerThree");
	ThirdEntry.StatusText = TEXT("Full");
	ThirdEntry.OpenPublicConnections = 0;
	ThirdEntry.PingInMs = 87;
	ThirdEntry.bCanJoin = false;
	ThirdEntry.JoinBlockReason = EMultiplayerJoinBlockReason::SessionFull;
	ThirdEntry.JoinDisabledReasonText = TEXT("Session is full.");
	DebugEntries.Add(ThirdEntry);

	BuildSessionListItems(DebugEntries);
	SetStatusText(TEXT("Debug session rows loaded."));
}

bool UMultiplayerEntryWidget::Initialize()
{
	if (Super::Initialize() == false)
	{
		return false;
	}

	HostButton->OnClicked.AddDynamic(this, &ThisClass::HandleHostButtonClicked);
	RefreshButton->OnClicked.AddDynamic(this, &ThisClass::HandleRefreshButtonClicked);
	JoinSelectedButton->OnClicked.AddDynamic(this, &ThisClass::HandleJoinSelectedButtonClicked);
	JoinByIpButton->OnClicked.AddDynamic(this, &ThisClass::HandleJoinByIpButtonClicked);
	BackButton->OnClicked.AddDynamic(this, &ThisClass::HandleBackButtonClicked);
	LanModeCheckBox->OnCheckStateChanged.AddDynamic(this, &ThisClass::HandleLanModeCheckStateChanged);
	bForceLanMode = LanModeCheckBox->IsChecked();
	SessionsListView->SetSelectionMode(ESelectionMode::Single);
	SessionsListView->OnItemSelectionChanged().AddUObject(this, &ThisClass::HandleSessionListSelectionChanged);
	SessionsHeaderContainer->SetVisibility(ESlateVisibility::Visible);
	return true;
}

void UMultiplayerEntryWidget::NativeDestruct()
{
	SessionsListView->OnItemSelectionChanged().RemoveAll(this);
	UnbindSubsystemDelegates();
	ClearSessionListItems();
	Super::NativeDestruct();
}

void UMultiplayerEntryWidget::HandleHostButtonClicked()
{
	RequestCreateSession();
}

void UMultiplayerEntryWidget::HandleRefreshButtonClicked()
{
	RequestFindSessions();
}

void UMultiplayerEntryWidget::HandleJoinSelectedButtonClicked()
{
	if (MultiplayerSessionsSubsystem == nullptr)
	{
		SetStatusText(TEXT("Session subsystem is missing."));
		return;
	}

	const FMultiplayerSessionBrowserEntry* BrowserEntry = GetSelectedBrowserEntry();
	if (BrowserEntry == nullptr)
	{
		SetStatusText(TEXT("Select a session first."));
		return;
	}

	if (BrowserEntry->bCanJoin == false)
	{
		if (BrowserEntry->JoinDisabledReasonText.IsEmpty() == false)
		{
			SetStatusText(BrowserEntry->JoinDisabledReasonText);
		}
		else
		{
			SetStatusText(TEXT("The selected session cannot be joined."));
		}
		return;
	}

	SetBusyState(true);
	SetStatusText(TEXT("Joining session..."));
	MultiplayerSessionsSubsystem->JoinSessionByEntryIdForLocalPlayer(GetOwningLocalPlayer(), BrowserEntry->EntryId);
}

void UMultiplayerEntryWidget::HandleJoinByIpButtonClicked()
{
	if (MultiplayerSessionsSubsystem == nullptr)
	{
		SetStatusText(TEXT("Session subsystem is missing."));
		return;
	}

	FString Address = DirectIpTextBox->GetText().ToString();
	Address.TrimStartAndEndInline();
	if (Address.IsEmpty() == true)
	{
		SetStatusText(TEXT("Enter a server address first."));
		return;
	}

	SetBusyState(true);
	SetStatusText(TEXT("Connecting to server..."));
	MultiplayerSessionsSubsystem->JoinByAddress(Address);
}

void UMultiplayerEntryWidget::HandleBackButtonClicked()
{
	if (bIsBusy == true)
	{
		return;
	}

	BackRequestedEvent.Broadcast();
	if (MainMenuMapPath.IsEmpty() == false)
	{
		UGameplayStatics::OpenLevel(this, FName(*MainMenuMapPath));
	}
}

void UMultiplayerEntryWidget::HandleLanModeCheckStateChanged(bool bIsChecked)
{
	bForceLanMode = bIsChecked;
	UpdateConnectionModeText();
	ClearSessionListItems();
	SetStatusText(BuildCurrentModeStatusText());
}

void UMultiplayerEntryWidget::HandleCreateSessionCompleted(bool bWasSuccessful)
{
	SetBusyState(false);
	if (bWasSuccessful == false)
	{
		EMultiplayerSessionFailureReason FailureReason = EMultiplayerSessionFailureReason::CreateFailed;
		if (MultiplayerSessionsSubsystem != nullptr)
		{
			FailureReason = MultiplayerSessionsSubsystem->GetLastFailureReason();
		}
		SetStatusText(BuildFailureMessage(FailureReason));
		return;
	}

	if (LobbyMapPath.IsEmpty() == true)
	{
		SetStatusText(TEXT("Lobby map path is empty."));
		return;
	}

	SetStatusText(TEXT("Session created. Opening lobby..."));
	UGameplayStatics::OpenLevel(this, FName(*LobbyMapPath), true, TEXT("listen"));
}

void UMultiplayerEntryWidget::HandleSessionSearchCompleted(bool bWasSuccessful, const TArray<FMultiplayerSessionBrowserEntry>& BrowserEntries)
{
	SetBusyState(false);
	if (bWasSuccessful == false)
	{
		ClearSessionListItems();
		EMultiplayerSessionFailureReason FailureReason = EMultiplayerSessionFailureReason::FindFailed;
		if (MultiplayerSessionsSubsystem != nullptr)
		{
			FailureReason = MultiplayerSessionsSubsystem->GetLastFailureReason();
		}
		SetStatusText(BuildFailureMessage(FailureReason));
		return;
	}

	BuildSessionListItems(BrowserEntries);
	if (BrowserEntries.IsEmpty() == true)
	{
		SetStatusText(TEXT("No matching sessions were found."));
		return;
	}

	SetStatusText(FString::Printf(TEXT("Found %d session(s)."), BrowserEntries.Num()));
}

void UMultiplayerEntryWidget::HandleJoinSessionRequestCompleted(EMultiplayerJoinSessionResult Result)
{
	SetBusyState(false);

	switch (Result)
	{
	case EMultiplayerJoinSessionResult::Success:
		SetStatusText(TEXT("Connected to session."));
		break;
	case EMultiplayerJoinSessionResult::SessionIsFull:
		SetStatusText(TEXT("The selected session is full."));
		break;
	case EMultiplayerJoinSessionResult::SessionDoesNotExist:
		SetStatusText(TEXT("The selected session no longer exists."));
		break;
	case EMultiplayerJoinSessionResult::SessionNotJoinable:
		SetStatusText(TEXT("The selected session is not joinable."));
		break;
	case EMultiplayerJoinSessionResult::CouldNotRetrieveAddress:
		SetStatusText(TEXT("The session address could not be resolved."));
		break;
	case EMultiplayerJoinSessionResult::AlreadyInSession:
		SetStatusText(TEXT("You are already in this session."));
		break;
	case EMultiplayerJoinSessionResult::Busy:
		SetStatusText(TEXT("Another online operation is still running."));
		break;
	case EMultiplayerJoinSessionResult::Timeout:
		SetStatusText(TEXT("Joining the session timed out."));
		break;
	case EMultiplayerJoinSessionResult::TravelFailed:
		SetStatusText(TEXT("The connection was created, but travel to the server failed."));
		break;
	case EMultiplayerJoinSessionResult::IncompatibleBuild:
		SetStatusText(TEXT("The selected session uses an incompatible game build."));
		break;
	case EMultiplayerJoinSessionResult::IncompatibleSchema:
		SetStatusText(TEXT("The selected session uses an incompatible session data version."));
		break;
	default:
		{
			EMultiplayerSessionFailureReason FailureReason = EMultiplayerSessionFailureReason::JoinFailed;
			if (MultiplayerSessionsSubsystem != nullptr)
			{
				FailureReason = MultiplayerSessionsSubsystem->GetLastFailureReason();
			}
			SetStatusText(BuildFailureMessage(FailureReason));
			break;
		}
	}
}

void UMultiplayerEntryWidget::HandleTravelRequestCompleted(bool bWasSuccessful)
{
	SetBusyState(false);
	if (bWasSuccessful == true)
	{
		SetStatusText(TEXT("Connected to server."));
		return;
	}

	EMultiplayerSessionFailureReason FailureReason = EMultiplayerSessionFailureReason::TravelFailed;
	if (MultiplayerSessionsSubsystem != nullptr)
	{
		FailureReason = MultiplayerSessionsSubsystem->GetLastFailureReason();
	}
	SetStatusText(BuildFailureMessage(FailureReason));
}

void UMultiplayerEntryWidget::HandleSessionFailure(EMultiplayerSessionFailureReason FailureReason)
{
	SetBusyState(false);
	SetStatusText(BuildFailureMessage(FailureReason));
}

void UMultiplayerEntryWidget::BindSubsystemDelegates()
{
	UnbindSubsystemDelegates();
	if (MultiplayerSessionsSubsystem == nullptr)
	{
		return;
	}

	MultiplayerSessionsSubsystem->OnCreateSessionRequestComplete.AddDynamic(this, &ThisClass::HandleCreateSessionCompleted);
	MultiplayerSessionsSubsystem->OnSessionSearchCompleted.AddDynamic(this, &ThisClass::HandleSessionSearchCompleted);
	MultiplayerSessionsSubsystem->OnJoinSessionRequestCompleted.AddDynamic(this, &ThisClass::HandleJoinSessionRequestCompleted);
	MultiplayerSessionsSubsystem->OnTravelRequestCompleted.AddDynamic(this, &ThisClass::HandleTravelRequestCompleted);
	MultiplayerSessionsSubsystem->OnSessionFailure.AddDynamic(this, &ThisClass::HandleSessionFailure);
}

void UMultiplayerEntryWidget::UnbindSubsystemDelegates()
{
	if (MultiplayerSessionsSubsystem == nullptr)
	{
		return;
	}

	MultiplayerSessionsSubsystem->OnCreateSessionRequestComplete.RemoveAll(this);
	MultiplayerSessionsSubsystem->OnSessionSearchCompleted.RemoveAll(this);
	MultiplayerSessionsSubsystem->OnJoinSessionRequestCompleted.RemoveAll(this);
	MultiplayerSessionsSubsystem->OnTravelRequestCompleted.RemoveAll(this);
	MultiplayerSessionsSubsystem->OnSessionFailure.RemoveAll(this);
}

void UMultiplayerEntryWidget::ResolveSubsystemMode()
{
	bAutoFallbackToLan = false;
	ActiveSubsystemName = TEXT("Unavailable");

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
		ActiveSubsystemName = TEXT("None");
		bAutoFallbackToLan = true;
		return;
	}

	ActiveSubsystemName = OnlineSubsystem->GetSubsystemName().ToString();
	if (OnlineSubsystem->GetSubsystemName() == FName(TEXT("NULL")))
	{
		bAutoFallbackToLan = true;
	}
}

void UMultiplayerEntryWidget::RequestCreateSession()
{
	if (MultiplayerSessionsSubsystem == nullptr)
	{
		SetStatusText(TEXT("Session subsystem is missing."));
		return;
	}
	if (LobbyMapPath.IsEmpty() == true)
	{
		SetStatusText(TEXT("Lobby map path is empty."));
		return;
	}

	FMultiplayerSessionCreateRequest CreateRequest;
	CreateRequest.NumPublicConnections = FMath::Max(1, NumPublicConnections);
	CreateRequest.MatchType = MatchType;
	CreateRequest.HostDisplayName = ResolveLocalNicknameForSession();
	CreateRequest.SessionDisplayName = BuildSessionDisplayName(CreateRequest.HostDisplayName);
	CreateRequest.MapName = LobbyMapPath;
	CreateRequest.InitialStatus = EMultiplayerAdvertisedSessionStatus::Lobby;
	CreateRequest.BuildId = SessionBuildId;
	CreateRequest.SessionSchemaVersion = 1;
	CreateRequest.bUseLan = ShouldUseLanMode();
	CreateRequest.bAllowJoinInProgress = true;
	CreateRequest.bAllowInvites = true;
	CreateRequest.bAllowJoinViaPresence = true;
	CreateRequest.bAllowJoinViaPresenceFriendsOnly = false;

	SetBusyState(true);
	SetStatusText(TEXT("Creating session..."));
	MultiplayerSessionsSubsystem->CreateSessionForLocalPlayer(GetOwningLocalPlayer(), CreateRequest);
}

void UMultiplayerEntryWidget::RequestFindSessions()
{
	if (MultiplayerSessionsSubsystem == nullptr)
	{
		SetStatusText(TEXT("Session subsystem is missing."));
		return;
	}

	FMultiplayerSessionSearchRequest SearchRequest;
	SearchRequest.MaxSearchResults = 200;
	SearchRequest.DesiredMatchType = MatchType;
	SearchRequest.DesiredBuildId = SessionBuildId;
	SearchRequest.DesiredSessionSchemaVersion = 1;
	SearchRequest.bUseLan = ShouldUseLanMode();

	SetBusyState(true);
	ClearSessionListItems();
	SetStatusText(TEXT("Searching sessions..."));
	MultiplayerSessionsSubsystem->FindSessionsForLocalPlayer(GetOwningLocalPlayer(), SearchRequest);
}

void UMultiplayerEntryWidget::BuildSessionListItems(const TArray<FMultiplayerSessionBrowserEntry>& BrowserEntries)
{
	SessionsListView->ClearSelection();
	SessionsListView->ClearListItems();
	SessionListItems.Reset();
	SessionListItems.Reserve(BrowserEntries.Num());

	for (const FMultiplayerSessionBrowserEntry& BrowserEntry : BrowserEntries)
	{
		UMultiplayerSessionListItem* ListItem = NewObject<UMultiplayerSessionListItem>(this);
		if (ListItem == nullptr)
		{
			continue;
		}

		ListItem->Initialize(BrowserEntry);
		SessionListItems.Add(ListItem);
		SessionsListView->AddItem(ListItem);
	}

	UpdateJoinButtonState();
	SetBusyState(false);
}

void UMultiplayerEntryWidget::ClearSessionListItems()
{
	SessionsListView->ClearSelection();
	SessionsListView->ClearListItems();
	SessionListItems.Reset();
	UpdateJoinButtonState();
}

void UMultiplayerEntryWidget::HandleSessionListSelectionChanged(UObject* SelectedItem)
{
	UpdateJoinButtonState();

	const UMultiplayerSessionListItem* ListItem = Cast<UMultiplayerSessionListItem>(SelectedItem);
	if (ListItem != nullptr)
	{
		SetStatusText(BuildSelectedSessionStatusText(ListItem->GetBrowserEntry()));
	}
}

void UMultiplayerEntryWidget::SetBusyState(bool bInIsBusy)
{
	bIsBusy = bInIsBusy;
	const bool bControlsEnabled = bIsBusy == false;
	HostButton->SetIsEnabled(bControlsEnabled);
	RefreshButton->SetIsEnabled(bControlsEnabled);
	BackButton->SetIsEnabled(bControlsEnabled);
	JoinByIpButton->SetIsEnabled(bControlsEnabled);
	LanModeCheckBox->SetIsEnabled(bControlsEnabled);
	DirectIpTextBox->SetIsEnabled(bControlsEnabled);
	SessionsListView->SetIsEnabled(bControlsEnabled);
	UpdateJoinButtonState();
}

void UMultiplayerEntryWidget::SetStatusText(const FString& InStatusText)
{
	StatusText->SetText(FText::FromString(InStatusText));
}

void UMultiplayerEntryWidget::UpdateJoinButtonState()
{
	bool bCanJoinSelected = false;
	if (bIsBusy == false)
	{
		const FMultiplayerSessionBrowserEntry* SelectedBrowserEntry = GetSelectedBrowserEntry();
		if (SelectedBrowserEntry != nullptr)
		{
			bCanJoinSelected = SelectedBrowserEntry->bCanJoin;
		}
	}

	JoinSelectedButton->SetIsEnabled(bCanJoinSelected);
}

void UMultiplayerEntryWidget::UpdateConnectionModeText()
{
	ConnectionModeText->SetText(FText::FromString(BuildCurrentModeLabelText()));
}

bool UMultiplayerEntryWidget::ShouldUseLanMode() const
{
	if (bForceLanMode == true)
	{
		return true;
	}
	if (bAutoFallbackToLan == true)
	{
		return true;
	}
	return false;
}

FString UMultiplayerEntryWidget::ResolveLocalNicknameForSession() const
{
	const APlayerController* OwningPlayerController = GetOwningPlayer();
	if (OwningPlayerController != nullptr && OwningPlayerController->PlayerState != nullptr)
	{
		const FString PlayerStateName = OwningPlayerController->PlayerState->GetPlayerName();
		if (PlayerStateName.IsEmpty() == false)
		{
			return PlayerStateName;
		}
	}

	const FString PendingNickname = ResolvePendingNicknameFromGameInstance();
	if (PendingNickname.IsEmpty() == false)
	{
		return PendingNickname;
	}

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
	if (OnlineSubsystem != nullptr)
	{
		IOnlineIdentityPtr IdentityInterface = OnlineSubsystem->GetIdentityInterface();
		if (IdentityInterface.IsValid() == true)
		{
			int32 LocalUserNum = 0;
			ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
			UGameInstance* GameInstance = GetGameInstance();
			if (LocalPlayer != nullptr && GameInstance != nullptr)
			{
				for (int32 LocalPlayerIndex = 0; LocalPlayerIndex < GameInstance->GetNumLocalPlayers(); LocalPlayerIndex++)
				{
					if (GameInstance->GetLocalPlayerByIndex(LocalPlayerIndex) == LocalPlayer)
					{
						LocalUserNum = LocalPlayerIndex;
						break;
					}
				}
			}

			const FUniqueNetIdPtr LocalUserId = IdentityInterface->GetUniquePlayerId(LocalUserNum);
			if (LocalUserId.IsValid() == true)
			{
				const FString NicknameFromSubsystem = IdentityInterface->GetPlayerNickname(*LocalUserId);
				if (NicknameFromSubsystem.IsEmpty() == false)
				{
					return NicknameFromSubsystem;
				}
			}
		}
	}

	return TEXT("Player");
}

FString UMultiplayerEntryWidget::ResolvePendingNicknameFromGameInstance() const
{
	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance == nullptr || GameInstance->GetClass()->ImplementsInterface(UMultiplayerSessionProfileProvider::StaticClass()) == false)
	{
		return TEXT("");
	}

	return IMultiplayerSessionProfileProvider::Execute_GetMultiplayerSessionDisplayName(GameInstance);
}

FString UMultiplayerEntryWidget::BuildSessionDisplayName(const FString& InHostNickname) const
{
	FString DisplayName = InHostNickname;
	if (DisplayName.IsEmpty() == true)
	{
		DisplayName = TEXT("Host");
	}
	DisplayName.Append(TEXT(" Lobby"));
	return DisplayName;
}

FString UMultiplayerEntryWidget::BuildSelectedSessionStatusText(const FMultiplayerSessionBrowserEntry& BrowserEntry) const
{
	FString PingText = TEXT("--");
	if (BrowserEntry.PingInMs >= 0)
	{
		PingText = FString::Printf(TEXT("%d ms"), BrowserEntry.PingInMs);
	}

	FString SelectedText = FString::Printf(TEXT("Host: %s | Status: %s | Free Slots: %d | Ping: %s"), *BrowserEntry.HostDisplayName, *BrowserEntry.StatusText, FMath::Max(0, BrowserEntry.OpenPublicConnections), *PingText);
	if (BrowserEntry.bCanJoin == false && BrowserEntry.JoinDisabledReasonText.IsEmpty() == false)
	{
		SelectedText.Append(TEXT(" | "));
		SelectedText.Append(BrowserEntry.JoinDisabledReasonText);
	}
	return SelectedText;
}

FString UMultiplayerEntryWidget::BuildCurrentModeStatusText() const
{
	if (ShouldUseLanMode() == true)
	{
		if (bForceLanMode == true)
		{
			return TEXT("Session browser ready. Current mode: LAN (forced). Press Refresh to search for sessions.");
		}
		return TEXT("Session browser ready. Current mode: LAN (automatic fallback). Press Refresh to search for sessions.");
	}
	return FString::Printf(TEXT("Session browser ready. Current mode: Online (%s). Press Refresh to search for sessions."), *ActiveSubsystemName);
}

FString UMultiplayerEntryWidget::BuildCurrentModeLabelText() const
{
	if (ShouldUseLanMode() == true)
	{
		if (bForceLanMode == true)
		{
			return TEXT("Mode: LAN");
		}
		return TEXT("Mode: LAN");
	}
	return FString::Printf(TEXT("Mode: Online (%s)"), *ActiveSubsystemName);
}

FString UMultiplayerEntryWidget::BuildFailureMessage(EMultiplayerSessionFailureReason FailureReason) const
{
	switch (FailureReason)
	{
	case EMultiplayerSessionFailureReason::None:
		return TEXT("Operation completed.");
	case EMultiplayerSessionFailureReason::NoOnlineSubsystem:
		return TEXT("No online subsystem is available.");
	case EMultiplayerSessionFailureReason::NoSessionInterface:
		return TEXT("The online session interface is unavailable.");
	case EMultiplayerSessionFailureReason::CreateFailed:
		return TEXT("Creating the session failed.");
	case EMultiplayerSessionFailureReason::FindFailed:
		return TEXT("Searching for sessions failed.");
	case EMultiplayerSessionFailureReason::JoinFailed:
		return TEXT("Joining the session failed.");
	case EMultiplayerSessionFailureReason::TravelFailed:
		return TEXT("Travel to the server failed.");
	case EMultiplayerSessionFailureReason::DestroyFailed:
		return TEXT("Leaving the current session failed.");
	case EMultiplayerSessionFailureReason::UpdateFailed:
		return TEXT("Updating the hosted session failed.");
	case EMultiplayerSessionFailureReason::InvalidSearchResultIndex:
		return TEXT("The selected session is no longer available.");
	case EMultiplayerSessionFailureReason::InvalidAddress:
		return TEXT("Provided address is invalid.");
	case EMultiplayerSessionFailureReason::Busy:
		return TEXT("Another online operation is still running.");
	case EMultiplayerSessionFailureReason::Timeout:
		return TEXT("The online operation timed out.");
	case EMultiplayerSessionFailureReason::Cancelled:
		return TEXT("The online operation was cancelled.");
	case EMultiplayerSessionFailureReason::InvalidLocalPlayer:
		return TEXT("No valid local player was found.");
	case EMultiplayerSessionFailureReason::NotLoggedIn:
		return TEXT("The selected platform user is not logged in.");
	case EMultiplayerSessionFailureReason::NetworkFailure:
		return TEXT("The network connection failed.");
	case EMultiplayerSessionFailureReason::RecoveryFailed:
		return TEXT("Online session cleanup did not complete.");
	case EMultiplayerSessionFailureReason::IncompatibleBuild:
		return TEXT("The selected session uses an incompatible game build.");
	case EMultiplayerSessionFailureReason::InvalidSessionSchema:
		return TEXT("The selected session uses an incompatible session data version.");
	case EMultiplayerSessionFailureReason::StartFailed:
		return TEXT("Starting the hosted session failed.");
	case EMultiplayerSessionFailureReason::EndFailed:
		return TEXT("Ending the hosted session failed.");
	case EMultiplayerSessionFailureReason::SessionAlreadyExists:
		return TEXT("A local session already exists.");
	case EMultiplayerSessionFailureReason::NotSessionOwner:
		return TEXT("Only the session host can change hosted session state.");
	case EMultiplayerSessionFailureReason::InvalidFriendId:
		return TEXT("The friend identifier is invalid.");
	case EMultiplayerSessionFailureReason::InviteFailed:
		return TEXT("The platform session invitation could not be sent or accepted.");
	case EMultiplayerSessionFailureReason::PlatformUiUnavailable:
		return TEXT("The platform invitation interface is unavailable.");
	case EMultiplayerSessionFailureReason::FriendSessionNotFound:
		return TEXT("The friend is not currently in a joinable session.");
	default:
		return TEXT("Unknown session failure.");
	}
}

const FMultiplayerSessionBrowserEntry* UMultiplayerEntryWidget::GetSelectedBrowserEntry() const
{
	const UMultiplayerSessionListItem* SelectedItem = Cast<UMultiplayerSessionListItem>(SessionsListView->GetSelectedItem());
	if (SelectedItem == nullptr)
	{
		return nullptr;
	}
	return &SelectedItem->GetBrowserEntry();
}

