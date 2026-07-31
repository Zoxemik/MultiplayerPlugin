// Copyright (c) 2026 Zoxemik. All rights reserved.

#include "MultiplayerSessionsSubsystem.h"

#include "MultiplayerSessionsPrivate.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSessionSettings.h"

bool UMultiplayerSessionsSubsystem::TickOperationTimeouts(float DeltaTime)
{
	(void)DeltaTime;
	if (ActiveOperation.Type == EOperationType::None)
	{
		return true;
	}

	const double CurrentSeconds = FPlatformTime::Seconds();

	if (ActiveOperation.Step == EOperationStep::Recovering)
	{
		ContinueRecovery();
		return true;
	}

	if (ActiveOperation.DeadlineSeconds > 0.0 && CurrentSeconds >= ActiveOperation.DeadlineSeconds)
	{
		HandleOperationTimeout();
	}

	return true;
}

bool UMultiplayerSessionsSubsystem::OperationRequiresSessionInterface(EOperationType OperationType) const
{
	return OperationType != EOperationType::DirectTravel;
}

bool UMultiplayerSessionsSubsystem::OperationRequiresLocalPlayer(EOperationType OperationType) const
{
	if (OperationType == EOperationType::Create || OperationType == EOperationType::Find || OperationType == EOperationType::FindFriend || OperationType == EOperationType::Join || OperationType == EOperationType::DirectTravel)
	{
		return true;
	}

	return false;
}

bool UMultiplayerSessionsSubsystem::TryBeginOperation(EOperationType OperationType, EMultiplayerSessionFlowState FlowState, ULocalPlayer* LocalPlayer, double TimeoutSeconds, EMultiplayerSessionFailureReason& OutFailureReason)
{
	OutFailureReason = EMultiplayerSessionFailureReason::None;

	if (ActiveOperation.Type != EOperationType::None || CurrentFlowState != EMultiplayerSessionFlowState::Idle)
	{
		OutFailureReason = EMultiplayerSessionFailureReason::Busy;
		return false;
	}

	if (OperationRequiresSessionInterface(OperationType) == true)
	{
		if (EnsureSessionInterface(TEXT("TryBeginOperation"), OutFailureReason) == false)
		{
			return false;
		}
	}

	FLocalUserContext LocalUser;
	if (OperationRequiresLocalPlayer(OperationType) == true)
	{
		const UWorld* World = GetWorld();
		const bool bDedicatedServerCreate = OperationType == EOperationType::Create && World != nullptr && World->GetNetMode() == NM_DedicatedServer;
		if (bDedicatedServerCreate == false)
		{
			if (ResolveLocalUser(LocalPlayer, LocalUser, OutFailureReason) == false)
			{
				return false;
			}
		}
	}

	NextOperationGeneration++;
	if (NextOperationGeneration == 0)
	{
		NextOperationGeneration++;
	}

	ActiveOperation = FOperationContext();
	ActiveOperation.Generation = NextOperationGeneration;
	ActiveOperation.Type = OperationType;
	ActiveOperation.Step = EOperationStep::Executing;
	ActiveOperation.LocalUser = LocalUser;
	ActiveOperation.DeadlineSeconds = FPlatformTime::Seconds() + FMath::Max(1.0, TimeoutSeconds);
	SetLastFailureReason(EMultiplayerSessionFailureReason::None);

	SetFlowState(FlowState);
	return true;
}

bool UMultiplayerSessionsSubsystem::IsCurrentOperation(uint64 CallbackGeneration, EOperationType ExpectedType, EOperationStep ExpectedStep) const
{
	if (CallbackGeneration != ActiveOperation.Generation)
	{
		return false;
	}

	if (ActiveOperation.Type != ExpectedType)
	{
		return false;
	}

	return ActiveOperation.Step == ExpectedStep;
}

void UMultiplayerSessionsSubsystem::SetOperationStep(EOperationStep NewStep, double TimeoutSeconds)
{
	ActiveOperation.Step = NewStep;
	ActiveOperation.DeadlineSeconds = FPlatformTime::Seconds() + FMath::Max(1.0, TimeoutSeconds);
}

EMultiplayerSessionFlowState UMultiplayerSessionsSubsystem::ResetActiveOperation()
{
	const EMultiplayerSessionFlowState PreviousFlowState = CurrentFlowState;
	ActiveOperation = FOperationContext();
	return PreviousFlowState;
}

void UMultiplayerSessionsSubsystem::BroadcastIdleStateIfUnchanged(EMultiplayerSessionFlowState PreviousFlowState, uint64 ExpectedOperationGeneration)
{
	if (PreviousFlowState == EMultiplayerSessionFlowState::Idle)
	{
		return;
	}

	if (CurrentFlowState != PreviousFlowState)
	{
		return;
	}

	if (NextOperationGeneration != ExpectedOperationGeneration)
	{
		return;
	}

	SetFlowState(EMultiplayerSessionFlowState::Idle);
}

void UMultiplayerSessionsSubsystem::ClearOperationDelegate(EOperationType OperationType)
{
	if (OperationType == EOperationType::Create)
	{
		if (SessionInterface.IsValid() == true)
		{
			SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		}

		CreateSessionCompleteDelegateHandle = FDelegateHandle();
		return;
	}

	if (OperationType == EOperationType::Find)
	{
		if (SessionInterface.IsValid() == true)
		{
			SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		}

		FindSessionsCompleteDelegateHandle = FDelegateHandle();
		return;
	}

	if (OperationType == EOperationType::FindFriend)
	{
		if (SessionInterface.IsValid() == true && FindFriendDelegateLocalUserNum != INDEX_NONE)
		{
			SessionInterface->ClearOnFindFriendSessionCompleteDelegate_Handle(FindFriendDelegateLocalUserNum, FindFriendSessionCompleteDelegateHandle);
		}

		FindFriendSessionCompleteDelegateHandle = FDelegateHandle();
		FindFriendDelegateLocalUserNum = INDEX_NONE;
		return;
	}

	if (OperationType == EOperationType::Join)
	{
		if (SessionInterface.IsValid() == true)
		{
			SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		}

		JoinSessionCompleteDelegateHandle = FDelegateHandle();
		return;
	}

	if (OperationType == EOperationType::Destroy)
	{
		if (SessionInterface.IsValid() == true)
		{
			SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
			SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(RecoveryDestroyCompleteDelegateHandle);
		}

		DestroySessionCompleteDelegateHandle = FDelegateHandle();
		RecoveryDestroyCompleteDelegateHandle = FDelegateHandle();
		return;
	}

	if (OperationType == EOperationType::Update)
	{
		if (SessionInterface.IsValid() == true)
		{
			SessionInterface->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateSessionCompleteDelegateHandle);
		}

		UpdateSessionCompleteDelegateHandle = FDelegateHandle();
		return;
	}

	if (OperationType == EOperationType::Start)
	{
		if (SessionInterface.IsValid() == true)
		{
			SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
		}

		StartSessionCompleteDelegateHandle = FDelegateHandle();
		return;
	}

	if (OperationType == EOperationType::End)
	{
		if (SessionInterface.IsValid() == true)
		{
			SessionInterface->ClearOnEndSessionCompleteDelegate_Handle(EndSessionCompleteDelegateHandle);
		}

		EndSessionCompleteDelegateHandle = FDelegateHandle();
	}
}

void UMultiplayerSessionsSubsystem::ClearAllDelegateHandles()
{
	if (SessionInterface.IsValid() == true)
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		if (FindFriendDelegateLocalUserNum != INDEX_NONE)
		{
			SessionInterface->ClearOnFindFriendSessionCompleteDelegate_Handle(FindFriendDelegateLocalUserNum, FindFriendSessionCompleteDelegateHandle);
		}
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(RecoveryDestroyCompleteDelegateHandle);
		SessionInterface->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateSessionCompleteDelegateHandle);
		SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
		SessionInterface->ClearOnEndSessionCompleteDelegate_Handle(EndSessionCompleteDelegateHandle);
	}

	ClearPersistentSessionDelegates();
	CreateSessionCompleteDelegateHandle = FDelegateHandle();
	FindSessionsCompleteDelegateHandle = FDelegateHandle();
	FindFriendSessionCompleteDelegateHandle = FDelegateHandle();
	FindFriendDelegateLocalUserNum = INDEX_NONE;
	JoinSessionCompleteDelegateHandle = FDelegateHandle();
	DestroySessionCompleteDelegateHandle = FDelegateHandle();
	RecoveryDestroyCompleteDelegateHandle = FDelegateHandle();
	UpdateSessionCompleteDelegateHandle = FDelegateHandle();
	StartSessionCompleteDelegateHandle = FDelegateHandle();
	EndSessionCompleteDelegateHandle = FDelegateHandle();
}

void UMultiplayerSessionsSubsystem::ResetCommittedSessionState()
{
	CommittedSessionSettings.Reset();
	bOwnsNamedSession = false;
	bHasCommittedJoinInProgressPolicy = false;
	bCommittedAllowJoinInProgress = true;
}

void UMultiplayerSessionsSubsystem::BeginCreateOperation()
{
	if (SessionInterface.IsValid() == false)
	{
		CompleteCreateOperation(false, EMultiplayerSessionFailureReason::NoSessionInterface);
		return;
	}

	SetOperationStep(EOperationStep::Executing, CreateTimeoutSeconds);
	ActiveOperation.PendingSessionSettings = MakeShared<FOnlineSessionSettings>();
	ApplyCreateRequestToSessionSettings(*ActiveOperation.PendingSessionSettings, ActiveOperation.CreateRequest);

	const uint64 Generation = ActiveOperation.Generation;
	const FOnCreateSessionCompleteDelegate CompletionDelegate = FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnCreateSessionCompleteInternal, Generation);
	CreateSessionCompleteDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(CompletionDelegate);

	bool bStarted = false;
	if (ActiveOperation.LocalUser.UniqueNetId.IsValid() == true)
	{
		bStarted = SessionInterface->CreateSession(*ActiveOperation.LocalUser.UniqueNetId, NAME_GameSession, *ActiveOperation.PendingSessionSettings);
	}
	else
	{
		bStarted = SessionInterface->CreateSession(ActiveOperation.LocalUser.LocalUserNum, NAME_GameSession, *ActiveOperation.PendingSessionSettings);
	}

	if (bStarted == false)
	{
		ClearOperationDelegate(EOperationType::Create);
		CompleteCreateOperation(false, EMultiplayerSessionFailureReason::CreateFailed);
	}
}

void UMultiplayerSessionsSubsystem::BeginFindOperation()
{
	if (SessionInterface.IsValid() == false)
	{
		TArray<FOnlineSessionSearchResult> EmptySearchResults;
		TArray<FMultiplayerSessionBrowserEntry> EmptyBrowserEntries;
		CompleteFindOperation(false, EMultiplayerSessionFailureReason::NoSessionInterface, MoveTemp(EmptySearchResults), MoveTemp(EmptyBrowserEntries));
		return;
	}

	SetOperationStep(EOperationStep::Executing, FindTimeoutSeconds);
	ActiveOperation.PendingSearch = MakeShared<FOnlineSessionSearch>();
	ActiveOperation.PendingSearch->MaxSearchResults = ActiveOperation.SearchRequest.MaxSearchResults;
	ActiveOperation.PendingSearch->bIsLanQuery = ActiveOperation.SearchRequest.bUseLan;
	ActiveOperation.PendingSearch->PingBucketSize = 50;

	if (ActiveOperation.SearchRequest.bUseLan == false)
	{
		ActiveOperation.PendingSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);

		if (ActiveOperation.SearchRequest.DesiredMatchType.IsEmpty() == false)
		{
			ActiveOperation.PendingSearch->QuerySettings.Set(MultiplayerSessionsKeys::MatchType, ActiveOperation.SearchRequest.DesiredMatchType, EOnlineComparisonOp::Equals);
		}

	}

	const uint64 Generation = ActiveOperation.Generation;
	const FOnFindSessionsCompleteDelegate CompletionDelegate = FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnFindSessionsCompleteInternal, Generation);
	FindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(CompletionDelegate);

	bool bStarted = false;
	if (ActiveOperation.LocalUser.UniqueNetId.IsValid() == true)
	{
		bStarted = SessionInterface->FindSessions(*ActiveOperation.LocalUser.UniqueNetId, ActiveOperation.PendingSearch.ToSharedRef());
	}
	else
	{
		bStarted = SessionInterface->FindSessions(ActiveOperation.LocalUser.LocalUserNum, ActiveOperation.PendingSearch.ToSharedRef());
	}

	if (bStarted == false)
	{
		ClearOperationDelegate(EOperationType::Find);
		TArray<FOnlineSessionSearchResult> EmptySearchResults;
		TArray<FMultiplayerSessionBrowserEntry> EmptyBrowserEntries;
		CompleteFindOperation(false, EMultiplayerSessionFailureReason::FindFailed, MoveTemp(EmptySearchResults), MoveTemp(EmptyBrowserEntries));
	}
}

void UMultiplayerSessionsSubsystem::BeginFindFriendOperation()
{
	if (SessionInterface.IsValid() == false)
	{
		CompleteFindFriendOperation(false, EMultiplayerSessionFailureReason::NoSessionInterface);
		return;
	}

	if (ActiveOperation.FriendId.IsValid() == false)
	{
		CompleteFindFriendOperation(false, EMultiplayerSessionFailureReason::InvalidFriendId);
		return;
	}

	SetOperationStep(EOperationStep::Executing, FindTimeoutSeconds);
	const uint64 Generation = ActiveOperation.Generation;
	FindFriendDelegateLocalUserNum = ActiveOperation.LocalUser.LocalUserNum;
	const FOnFindFriendSessionCompleteDelegate CompletionDelegate = FOnFindFriendSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnFindFriendSessionCompleteInternal, Generation);
	FindFriendSessionCompleteDelegateHandle = SessionInterface->AddOnFindFriendSessionCompleteDelegate_Handle(FindFriendDelegateLocalUserNum, CompletionDelegate);

	const bool bStarted = SessionInterface->FindFriendSession(ActiveOperation.LocalUser.LocalUserNum, *ActiveOperation.FriendId);

	if (bStarted == false)
	{
		ClearOperationDelegate(EOperationType::FindFriend);
		CompleteFindFriendOperation(false, EMultiplayerSessionFailureReason::FindFailed);
	}
}

void UMultiplayerSessionsSubsystem::BeginJoinAfterExistingSessionCleanup()
{
	if (SessionInterface.IsValid() == false)
	{
		CompleteJoinOperation(EOnJoinSessionCompleteResult::UnknownError, EMultiplayerJoinSessionResult::UnknownError, EMultiplayerSessionFailureReason::NoSessionInterface);
		return;
	}

	if (SessionInterface->GetNamedSession(NAME_GameSession) != nullptr)
	{
		ActiveOperation.Step = EOperationStep::DestroyExistingForJoin;
		BeginDestroyOperation();
		return;
	}

	ResetCommittedSessionState();
	BeginJoinOperation();
}

void UMultiplayerSessionsSubsystem::BeginJoinOperation()
{
	if (SessionInterface.IsValid() == false)
	{
		CompleteJoinOperation(EOnJoinSessionCompleteResult::UnknownError, EMultiplayerJoinSessionResult::UnknownError, EMultiplayerSessionFailureReason::NoSessionInterface);
		return;
	}

	SetOperationStep(EOperationStep::Executing, JoinTimeoutSeconds);

	const uint64 Generation = ActiveOperation.Generation;
	const FOnJoinSessionCompleteDelegate CompletionDelegate = FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnJoinSessionCompleteInternal, Generation);
	JoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(CompletionDelegate);

	bool bStarted = false;
	if (ActiveOperation.LocalUser.UniqueNetId.IsValid() == true)
	{
		bStarted = SessionInterface->JoinSession(*ActiveOperation.LocalUser.UniqueNetId, NAME_GameSession, ActiveOperation.JoinResult);
	}
	else
	{
		bStarted = SessionInterface->JoinSession(ActiveOperation.LocalUser.LocalUserNum, NAME_GameSession, ActiveOperation.JoinResult);
	}

	if (bStarted == false)
	{
		ClearOperationDelegate(EOperationType::Join);
		CompleteJoinOperation(EOnJoinSessionCompleteResult::UnknownError, EMultiplayerJoinSessionResult::UnknownError, EMultiplayerSessionFailureReason::JoinFailed);
	}
}

void UMultiplayerSessionsSubsystem::BeginDestroyOperation()
{
	if (SessionInterface.IsValid() == false)
	{
		if (ActiveOperation.Type == EOperationType::Create)
		{
			CompleteCreateOperation(false, EMultiplayerSessionFailureReason::NoSessionInterface);
		}
		else if (ActiveOperation.Type == EOperationType::Join)
		{
			CompleteJoinOperation(EOnJoinSessionCompleteResult::UnknownError, EMultiplayerJoinSessionResult::UnknownError, EMultiplayerSessionFailureReason::NoSessionInterface);
		}
		else
		{
			CompleteDestroyOperation(false, EMultiplayerSessionFailureReason::NoSessionInterface);
		}
		return;
	}

	FNamedOnlineSession* ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);
	if (ExistingSession == nullptr)
	{
		ResetCommittedSessionState();
		if (ActiveOperation.Type == EOperationType::Create)
		{
			BeginCreateOperation();
		}
		else if (ActiveOperation.Type == EOperationType::Join)
		{
			BeginJoinOperation();
		}
		else
		{
			CompleteDestroyOperation(true, EMultiplayerSessionFailureReason::None);
		}
		return;
	}

	if (ActiveOperation.Type == EOperationType::Destroy)
	{
		SetOperationStep(EOperationStep::Executing, DestroyTimeoutSeconds);
	}
	else if (ActiveOperation.Type == EOperationType::Create)
	{
		SetOperationStep(EOperationStep::DestroyExistingForCreate, DestroyTimeoutSeconds);
	}
	else
	{
		SetOperationStep(EOperationStep::DestroyExistingForJoin, DestroyTimeoutSeconds);
	}

	const uint64 Generation = ActiveOperation.Generation;
	const FOnDestroySessionCompleteDelegate CompletionDelegate = FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::OnDestroySessionCompleteInternal, Generation);
	DestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(CompletionDelegate);

	const bool bStarted = SessionInterface->DestroySession(NAME_GameSession);
	if (bStarted == true)
	{
		return;
	}

	ClearOperationDelegate(EOperationType::Destroy);

	const bool bSessionNoLongerExists = SessionInterface->GetNamedSession(NAME_GameSession) == nullptr;
	if (bSessionNoLongerExists == true)
	{
		ResetCommittedSessionState();
		if (ActiveOperation.Type == EOperationType::Create)
		{
			BeginCreateOperation();
		}
		else if (ActiveOperation.Type == EOperationType::Join)
		{
			BeginJoinOperation();
		}
		else
		{
			CompleteDestroyOperation(true, EMultiplayerSessionFailureReason::None);
		}
		return;
	}

	if (ActiveOperation.Type == EOperationType::Create)
	{
		CompleteCreateOperation(false, EMultiplayerSessionFailureReason::DestroyFailed);
	}
	else if (ActiveOperation.Type == EOperationType::Join)
	{
		CompleteJoinOperation(EOnJoinSessionCompleteResult::UnknownError, EMultiplayerJoinSessionResult::UnknownError, EMultiplayerSessionFailureReason::DestroyFailed);
	}
	else
	{
		CompleteDestroyOperation(false, EMultiplayerSessionFailureReason::DestroyFailed);
	}
}

void UMultiplayerSessionsSubsystem::BeginUpdateOperation()
{
	if (SessionInterface.IsValid() == false)
	{
		CompleteUpdateOperation(false, EMultiplayerSessionFailureReason::NoSessionInterface);
		return;
	}

	FNamedOnlineSession* ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);
	if (ExistingSession == nullptr)
	{
		CompleteUpdateOperation(false, EMultiplayerSessionFailureReason::UpdateFailed);
		return;
	}

	if (bOwnsNamedSession == false)
	{
		CompleteUpdateOperation(false, EMultiplayerSessionFailureReason::NotSessionOwner);
		return;
	}

	SetOperationStep(EOperationStep::Executing, UpdateTimeoutSeconds);
	if (CommittedSessionSettings.IsValid() == true)
	{
		ActiveOperation.PendingSessionSettings = MakeShared<FOnlineSessionSettings>(*CommittedSessionSettings);
	}
	else
	{
		ActiveOperation.PendingSessionSettings = MakeShared<FOnlineSessionSettings>(ExistingSession->SessionSettings);
	}

	ActiveOperation.PendingSessionSettings->Set(MultiplayerSessionsKeys::Status, SessionStatusToString(ActiveOperation.RequestedStatus), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	bool bConfiguredAllowJoinInProgress = ActiveOperation.PendingSessionSettings->bAllowJoinInProgress;
	if (bHasCommittedJoinInProgressPolicy == true)
	{
		bConfiguredAllowJoinInProgress = bCommittedAllowJoinInProgress;
	}
	else
	{
		ActiveOperation.PendingSessionSettings->Get(MultiplayerSessionsKeys::ConfiguredAllowJoinInProgress, bConfiguredAllowJoinInProgress);
	}

	if (ActiveOperation.RequestedStatus == EMultiplayerAdvertisedSessionStatus::Starting || ActiveOperation.RequestedStatus == EMultiplayerAdvertisedSessionStatus::Full)
	{
		ActiveOperation.PendingSessionSettings->bAllowJoinInProgress = false;
	}
	else
	{
		ActiveOperation.PendingSessionSettings->bAllowJoinInProgress = bConfiguredAllowJoinInProgress;
	}

	const uint64 Generation = ActiveOperation.Generation;
	const FOnUpdateSessionCompleteDelegate CompletionDelegate = FOnUpdateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnUpdateSessionCompleteInternal, Generation);
	UpdateSessionCompleteDelegateHandle = SessionInterface->AddOnUpdateSessionCompleteDelegate_Handle(CompletionDelegate);

	const bool bStarted = SessionInterface->UpdateSession(NAME_GameSession, *ActiveOperation.PendingSessionSettings, true);
	if (bStarted == false)
	{
		ClearOperationDelegate(EOperationType::Update);
		CompleteUpdateOperation(false, EMultiplayerSessionFailureReason::UpdateFailed);
	}
}

void UMultiplayerSessionsSubsystem::BeginStartOperation()
{
	if (SessionInterface.IsValid() == false || SessionInterface->GetNamedSession(NAME_GameSession) == nullptr)
	{
		CompleteStartOperation(false, EMultiplayerSessionFailureReason::StartFailed);
		return;
	}

	if (bOwnsNamedSession == false)
	{
		CompleteStartOperation(false, EMultiplayerSessionFailureReason::NotSessionOwner);
		return;
	}

	SetOperationStep(EOperationStep::Executing, StartEndTimeoutSeconds);
	const uint64 Generation = ActiveOperation.Generation;
	const FOnStartSessionCompleteDelegate CompletionDelegate = FOnStartSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnStartSessionCompleteInternal, Generation);
	StartSessionCompleteDelegateHandle = SessionInterface->AddOnStartSessionCompleteDelegate_Handle(CompletionDelegate);

	if (SessionInterface->StartSession(NAME_GameSession) == false)
	{
		ClearOperationDelegate(EOperationType::Start);
		CompleteStartOperation(false, EMultiplayerSessionFailureReason::StartFailed);
	}
}

void UMultiplayerSessionsSubsystem::BeginEndOperation()
{
	if (SessionInterface.IsValid() == false || SessionInterface->GetNamedSession(NAME_GameSession) == nullptr)
	{
		CompleteEndOperation(false, EMultiplayerSessionFailureReason::EndFailed);
		return;
	}

	if (bOwnsNamedSession == false)
	{
		CompleteEndOperation(false, EMultiplayerSessionFailureReason::NotSessionOwner);
		return;
	}

	SetOperationStep(EOperationStep::Executing, StartEndTimeoutSeconds);
	const uint64 Generation = ActiveOperation.Generation;
	const FOnEndSessionCompleteDelegate CompletionDelegate = FOnEndSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnEndSessionCompleteInternal, Generation);
	EndSessionCompleteDelegateHandle = SessionInterface->AddOnEndSessionCompleteDelegate_Handle(CompletionDelegate);

	if (SessionInterface->EndSession(NAME_GameSession) == false)
	{
		ClearOperationDelegate(EOperationType::End);
		CompleteEndOperation(false, EMultiplayerSessionFailureReason::EndFailed);
	}
}

bool UMultiplayerSessionsSubsystem::BeginTravel(const FString& TravelAddress)
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	ULocalPlayer* LocalPlayer = ActiveOperation.LocalUser.LocalPlayer.Get();
	if (LocalPlayer == nullptr)
	{
		return false;
	}

	APlayerController* PlayerController = LocalPlayer->GetPlayerController(World);
	if (PlayerController == nullptr)
	{
		return false;
	}

	SetOperationStep(EOperationStep::WaitingForTravel, TravelTimeoutSeconds);
	SetFlowState(EMultiplayerSessionFlowState::Traveling);

	UE_LOG(LogMultiplayerSessionsSubsystem, Log, TEXT("Starting client travel to %s."), *TravelAddress);
	PlayerController->ClientTravel(TravelAddress, ETravelType::TRAVEL_Absolute);
	return true;
}

void UMultiplayerSessionsSubsystem::BeginRecovery(EOperationType SourceType)
{
	ClearOperationDelegate(SourceType);
	if (SourceType == EOperationType::Create || SourceType == EOperationType::Join)
	{
		ClearOperationDelegate(EOperationType::Destroy);
	}
	ActiveOperation.RecoverySourceType = SourceType;
	ActiveOperation.Step = EOperationStep::Recovering;
	ActiveOperation.RecoveryNotBeforeSeconds = FPlatformTime::Seconds() + FMath::Max(0.0f, RecoveryGraceSeconds);
	ActiveOperation.DeadlineSeconds = FPlatformTime::Seconds() + FMath::Max(1.0f, RecoveryTimeoutSeconds);
	SetFlowState(EMultiplayerSessionFlowState::Recovering);
}

void UMultiplayerSessionsSubsystem::ContinueRecovery()
{
	const double CurrentSeconds = FPlatformTime::Seconds();
	if (CurrentSeconds < ActiveOperation.RecoveryNotBeforeSeconds)
	{
		return;
	}

	if (ActiveOperation.RecoverySourceType == EOperationType::Update)
	{
		CompleteRecovery(true);
		return;
	}

	if (ActiveOperation.RecoverySourceType == EOperationType::Start || ActiveOperation.RecoverySourceType == EOperationType::End)
	{
		CompleteRecovery(true);
		return;
	}

	if (SessionInterface.IsValid() == false)
	{
		CompleteRecovery(false);
		return;
	}

	const bool bHasSession = SessionInterface->GetNamedSession(NAME_GameSession) != nullptr;
	if (bHasSession == false)
	{
		ResetCommittedSessionState();

		if (ActiveOperation.RecoverySourceType == EOperationType::Destroy)
		{
			CompleteRecovery(true);
			return;
		}

		if (CurrentSeconds >= ActiveOperation.DeadlineSeconds)
		{
			CompleteRecovery(true);
		}
		return;
	}

	if (ActiveOperation.bRecoveryAttemptedDestroy == false)
	{
		BeginRecoveryDestroy();
		return;
	}

	if (CurrentSeconds >= ActiveOperation.DeadlineSeconds)
	{
		CompleteRecovery(false);
	}
}

void UMultiplayerSessionsSubsystem::BeginRecoveryDestroy()
{
	if (SessionInterface.IsValid() == false)
	{
		CompleteRecovery(false);
		return;
	}

	ActiveOperation.bRecoveryAttemptedDestroy = true;
	SetOperationStep(EOperationStep::RecoveryDestroy, DestroyTimeoutSeconds);

	const uint64 Generation = ActiveOperation.Generation;
	const FOnDestroySessionCompleteDelegate CompletionDelegate = FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::OnRecoveryDestroyCompleteInternal, Generation);
	RecoveryDestroyCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(CompletionDelegate);

	if (SessionInterface->DestroySession(NAME_GameSession) == false)
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(RecoveryDestroyCompleteDelegateHandle);
		RecoveryDestroyCompleteDelegateHandle = FDelegateHandle();

		if (SessionInterface->GetNamedSession(NAME_GameSession) == nullptr)
		{
			ResetCommittedSessionState();
			CompleteRecovery(true);
			return;
		}

		CompleteRecovery(false);
	}
}

void UMultiplayerSessionsSubsystem::CompleteCreateOperation(bool bWasSuccessful, EMultiplayerSessionFailureReason FailureReason)
{
	if (ActiveOperation.bResultBroadcast == true)
	{
		return;
	}

	ActiveOperation.bResultBroadcast = true;
	ClearOperationDelegate(EOperationType::Create);
	ClearOperationDelegate(EOperationType::Destroy);
	SetLastFailureReason(FailureReason);

	const uint64 CompletedOperationGeneration = ActiveOperation.Generation;
	const EMultiplayerSessionFlowState PreviousFlowState = ResetActiveOperation();

	MultiplayerOnCreateSessionComplete.Broadcast(bWasSuccessful);
	OnCreateSessionRequestComplete.Broadcast(bWasSuccessful);
	BroadcastIdleStateIfUnchanged(PreviousFlowState, CompletedOperationGeneration);
}

void UMultiplayerSessionsSubsystem::CompleteFindOperation(bool bWasSuccessful, EMultiplayerSessionFailureReason FailureReason, TArray<FOnlineSessionSearchResult>&& SearchResults, TArray<FMultiplayerSessionBrowserEntry>&& BrowserEntries)
{
	if (ActiveOperation.bResultBroadcast == true)
	{
		return;
	}

	ActiveOperation.bResultBroadcast = true;
	ClearOperationDelegate(EOperationType::Find);
	SetLastFailureReason(FailureReason);

	if (bWasSuccessful == true)
	{
		CachedSearchResults = MoveTemp(SearchResults);
		CachedBrowserEntries = MoveTemp(BrowserEntries);
	}
	else
	{
		CachedSearchResults.Reset();
		CachedBrowserEntries.Reset();
	}

	TArray<FOnlineSessionSearchResult> ResultsForBroadcast;
	TArray<FMultiplayerSessionBrowserEntry> EntriesForBroadcast;
	if (bWasSuccessful == true)
	{
		ResultsForBroadcast = CachedSearchResults;
		EntriesForBroadcast = CachedBrowserEntries;
	}

	const uint64 CompletedOperationGeneration = ActiveOperation.Generation;
	const EMultiplayerSessionFlowState PreviousFlowState = ResetActiveOperation();

	MultiplayerOnFindSessionsComplete.Broadcast(ResultsForBroadcast, bWasSuccessful);
	OnSessionSearchCompleted.Broadcast(bWasSuccessful, EntriesForBroadcast);
	BroadcastIdleStateIfUnchanged(PreviousFlowState, CompletedOperationGeneration);
}

void UMultiplayerSessionsSubsystem::CompleteFindFriendOperation(bool bWasSuccessful, EMultiplayerSessionFailureReason FailureReason)
{
	if (ActiveOperation.bResultBroadcast == true)
	{
		return;
	}

	ActiveOperation.bResultBroadcast = true;
	ClearOperationDelegate(EOperationType::FindFriend);
	SetLastFailureReason(FailureReason);

	const uint64 CompletedOperationGeneration = ActiveOperation.Generation;
	const EMultiplayerSessionFlowState PreviousFlowState = ResetActiveOperation();

	OnFriendSessionSearchCompleted.Broadcast(bWasSuccessful, FailureReason);
	BroadcastIdleStateIfUnchanged(PreviousFlowState, CompletedOperationGeneration);
}

void UMultiplayerSessionsSubsystem::CompleteJoinOperation(EOnJoinSessionCompleteResult::Type LegacyResult, EMultiplayerJoinSessionResult Result, EMultiplayerSessionFailureReason FailureReason)
{
	if (ActiveOperation.bResultBroadcast == true)
	{
		return;
	}

	ActiveOperation.bResultBroadcast = true;
	ClearOperationDelegate(EOperationType::Join);
	SetLastFailureReason(FailureReason);

	const uint64 CompletedOperationGeneration = ActiveOperation.Generation;
	const EMultiplayerSessionFlowState PreviousFlowState = ResetActiveOperation();

	MultiplayerOnJoinSessionComplete.Broadcast(LegacyResult);
	OnJoinSessionRequestCompleted.Broadcast(Result);
	BroadcastIdleStateIfUnchanged(PreviousFlowState, CompletedOperationGeneration);
}

void UMultiplayerSessionsSubsystem::CompleteDestroyOperation(bool bWasSuccessful, EMultiplayerSessionFailureReason FailureReason)
{
	if (ActiveOperation.bResultBroadcast == true)
	{
		return;
	}

	ActiveOperation.bResultBroadcast = true;
	ClearOperationDelegate(EOperationType::Destroy);
	SetLastFailureReason(FailureReason);

	const uint64 CompletedOperationGeneration = ActiveOperation.Generation;
	const EMultiplayerSessionFlowState PreviousFlowState = ResetActiveOperation();

	MultiplayerOnDestroySessionComplete.Broadcast(bWasSuccessful);
	OnDestroySessionRequestComplete.Broadcast(bWasSuccessful);
	BroadcastIdleStateIfUnchanged(PreviousFlowState, CompletedOperationGeneration);
}

void UMultiplayerSessionsSubsystem::CompleteUpdateOperation(bool bWasSuccessful, EMultiplayerSessionFailureReason FailureReason)
{
	if (ActiveOperation.bResultBroadcast == true)
	{
		return;
	}

	ActiveOperation.bResultBroadcast = true;
	ClearOperationDelegate(EOperationType::Update);
	SetLastFailureReason(FailureReason);

	const uint64 CompletedOperationGeneration = ActiveOperation.Generation;
	const EMultiplayerSessionFlowState PreviousFlowState = ResetActiveOperation();

	OnUpdateHostedSessionCompleted.Broadcast(bWasSuccessful);
	BroadcastIdleStateIfUnchanged(PreviousFlowState, CompletedOperationGeneration);
}

void UMultiplayerSessionsSubsystem::CompleteStartOperation(bool bWasSuccessful, EMultiplayerSessionFailureReason FailureReason)
{
	if (ActiveOperation.bResultBroadcast == true)
	{
		return;
	}

	ActiveOperation.bResultBroadcast = true;
	ClearOperationDelegate(EOperationType::Start);
	SetLastFailureReason(FailureReason);

	const uint64 CompletedOperationGeneration = ActiveOperation.Generation;
	const EMultiplayerSessionFlowState PreviousFlowState = ResetActiveOperation();

	OnStartHostedSessionCompleted.Broadcast(bWasSuccessful);
	BroadcastIdleStateIfUnchanged(PreviousFlowState, CompletedOperationGeneration);
}

void UMultiplayerSessionsSubsystem::CompleteEndOperation(bool bWasSuccessful, EMultiplayerSessionFailureReason FailureReason)
{
	if (ActiveOperation.bResultBroadcast == true)
	{
		return;
	}

	ActiveOperation.bResultBroadcast = true;
	ClearOperationDelegate(EOperationType::End);
	SetLastFailureReason(FailureReason);

	const uint64 CompletedOperationGeneration = ActiveOperation.Generation;
	const EMultiplayerSessionFlowState PreviousFlowState = ResetActiveOperation();

	OnEndHostedSessionCompleted.Broadcast(bWasSuccessful);
	BroadcastIdleStateIfUnchanged(PreviousFlowState, CompletedOperationGeneration);
}

void UMultiplayerSessionsSubsystem::CompleteDirectTravelOperation(bool bWasSuccessful, EMultiplayerSessionFailureReason FailureReason)
{
	if (ActiveOperation.bResultBroadcast == true)
	{
		return;
	}

	ActiveOperation.bResultBroadcast = true;
	SetLastFailureReason(FailureReason);

	const uint64 CompletedOperationGeneration = ActiveOperation.Generation;
	const EMultiplayerSessionFlowState PreviousFlowState = ResetActiveOperation();

	OnTravelRequestCompleted.Broadcast(bWasSuccessful);
	BroadcastIdleStateIfUnchanged(PreviousFlowState, CompletedOperationGeneration);
}

void UMultiplayerSessionsSubsystem::CompleteRecovery(bool bWasSuccessful)
{
	ClearOperationDelegate(EOperationType::Destroy);

	if (bWasSuccessful == false)
	{
		ResetCommittedSessionState();
		SetLastFailureReason(EMultiplayerSessionFailureReason::RecoveryFailed);
		UE_LOG(LogMultiplayerSessionsSubsystem, Error, TEXT("Session recovery failed."));
	}

	const uint64 CompletedOperationGeneration = ActiveOperation.Generation;
	const EMultiplayerSessionFlowState PreviousFlowState = ResetActiveOperation();
	BroadcastIdleStateIfUnchanged(PreviousFlowState, CompletedOperationGeneration);
}

void UMultiplayerSessionsSubsystem::HandleOperationTimeout()
{
	const EOperationType TimedOutType = ActiveOperation.Type;
	const EOperationStep TimedOutStep = ActiveOperation.Step;

	UE_LOG(LogMultiplayerSessionsSubsystem, Warning, TEXT("Session operation timed out. Type=%d Step=%d Generation=%llu"), static_cast<int32>(TimedOutType), static_cast<int32>(TimedOutStep), static_cast<unsigned long long>(ActiveOperation.Generation));

	if (TimedOutStep == EOperationStep::RecoveryDestroy)
	{
		ClearOperationDelegate(EOperationType::Destroy);
		CompleteRecovery(false);
		return;
	}

	if (TimedOutType == EOperationType::Find)
	{
		ClearOperationDelegate(EOperationType::Find);

		if (SessionInterface.IsValid() == true)
		{
			SessionInterface->CancelFindSessions();
		}

		TArray<FOnlineSessionSearchResult> EmptySearchResults;
		TArray<FMultiplayerSessionBrowserEntry> EmptyBrowserEntries;
		CompleteFindOperation(false, EMultiplayerSessionFailureReason::Timeout, MoveTemp(EmptySearchResults), MoveTemp(EmptyBrowserEntries));
		return;
	}

	if (TimedOutType == EOperationType::FindFriend)
	{
		CompleteFindFriendOperation(false, EMultiplayerSessionFailureReason::Timeout);
		return;
	}

	if (TimedOutType == EOperationType::DirectTravel)
	{
		CompleteDirectTravelOperation(false, EMultiplayerSessionFailureReason::Timeout);
		return;
	}

	if (ActiveOperation.bResultBroadcast == false)
	{
		ActiveOperation.bResultBroadcast = true;
		SetLastFailureReason(EMultiplayerSessionFailureReason::Timeout);

		if (TimedOutType == EOperationType::Create)
		{
			MultiplayerOnCreateSessionComplete.Broadcast(false);
			OnCreateSessionRequestComplete.Broadcast(false);
		}
		else if (TimedOutType == EOperationType::Join)
		{
			MultiplayerOnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
			OnJoinSessionRequestCompleted.Broadcast(EMultiplayerJoinSessionResult::Timeout);
		}
		else if (TimedOutType == EOperationType::Destroy)
		{
			MultiplayerOnDestroySessionComplete.Broadcast(false);
			OnDestroySessionRequestComplete.Broadcast(false);
		}
		else if (TimedOutType == EOperationType::Update)
		{
			OnUpdateHostedSessionCompleted.Broadcast(false);
		}
		else if (TimedOutType == EOperationType::Start)
		{
			OnStartHostedSessionCompleted.Broadcast(false);
		}
		else if (TimedOutType == EOperationType::End)
		{
			OnEndHostedSessionCompleted.Broadcast(false);
		}
	}

	BeginRecovery(TimedOutType);
}

void UMultiplayerSessionsSubsystem::HandleTravelFailureInternal(EMultiplayerSessionFailureReason FailureReason)
{
	if (ActiveOperation.Type == EOperationType::Join)
	{
		if (ActiveOperation.bResultBroadcast == false)
		{
			ActiveOperation.bResultBroadcast = true;
			SetLastFailureReason(FailureReason);
			MultiplayerOnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::CouldNotRetrieveAddress);
			OnJoinSessionRequestCompleted.Broadcast(EMultiplayerJoinSessionResult::TravelFailed);
		}

		BeginRecovery(EOperationType::Join);
		return;
	}

	if (ActiveOperation.Type == EOperationType::DirectTravel)
	{
		CompleteDirectTravelOperation(false, FailureReason);
	}
}

void UMultiplayerSessionsSubsystem::SetFlowState(EMultiplayerSessionFlowState NewState)
{
	if (CurrentFlowState == NewState)
	{
		return;
	}

	CurrentFlowState = NewState;
	OnSessionFlowStateChanged.Broadcast(CurrentFlowState);
}

void UMultiplayerSessionsSubsystem::BroadcastFailure(EMultiplayerSessionFailureReason FailureReason)
{
	if (FailureReason == EMultiplayerSessionFailureReason::None)
	{
		return;
	}

	SetLastFailureReason(FailureReason);
	OnSessionFailure.Broadcast(FailureReason);
}

void UMultiplayerSessionsSubsystem::SetLastFailureReason(EMultiplayerSessionFailureReason FailureReason)
{
	LastFailureReason = FailureReason;
}

void UMultiplayerSessionsSubsystem::BroadcastImmediateFailureForOperation(EOperationType OperationType, EMultiplayerSessionFailureReason FailureReason)
{
	SetLastFailureReason(FailureReason);

	if (OperationType == EOperationType::Create)
	{
		MultiplayerOnCreateSessionComplete.Broadcast(false);
		OnCreateSessionRequestComplete.Broadcast(false);
	}
	else if (OperationType == EOperationType::Find)
	{
		const TArray<FOnlineSessionSearchResult> EmptySearchResults;
		const TArray<FMultiplayerSessionBrowserEntry> EmptyBrowserEntries;
		MultiplayerOnFindSessionsComplete.Broadcast(EmptySearchResults, false);
		OnSessionSearchCompleted.Broadcast(false, EmptyBrowserEntries);
	}
	else if (OperationType == EOperationType::FindFriend)
	{
		OnFriendSessionSearchCompleted.Broadcast(false, FailureReason);
	}
	else if (OperationType == EOperationType::Join)
	{
		EMultiplayerJoinSessionResult JoinResult = EMultiplayerJoinSessionResult::UnknownError;
		if (FailureReason == EMultiplayerSessionFailureReason::Busy)
		{
			JoinResult = EMultiplayerJoinSessionResult::Busy;
		}
		else if (FailureReason == EMultiplayerSessionFailureReason::IncompatibleBuild)
		{
			JoinResult = EMultiplayerJoinSessionResult::IncompatibleBuild;
		}
		else if (FailureReason == EMultiplayerSessionFailureReason::InvalidSessionSchema)
		{
			JoinResult = EMultiplayerJoinSessionResult::IncompatibleSchema;
		}
		else if (FailureReason == EMultiplayerSessionFailureReason::InvalidSearchResultIndex || FailureReason == EMultiplayerSessionFailureReason::FriendSessionNotFound)
		{
			JoinResult = EMultiplayerJoinSessionResult::SessionDoesNotExist;
		}

		MultiplayerOnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
		OnJoinSessionRequestCompleted.Broadcast(JoinResult);
	}
	else if (OperationType == EOperationType::Destroy)
	{
		MultiplayerOnDestroySessionComplete.Broadcast(false);
		OnDestroySessionRequestComplete.Broadcast(false);
	}
	else if (OperationType == EOperationType::Update)
	{
		OnUpdateHostedSessionCompleted.Broadcast(false);
	}
	else if (OperationType == EOperationType::Start)
	{
		OnStartHostedSessionCompleted.Broadcast(false);
	}
	else if (OperationType == EOperationType::End)
	{
		OnEndHostedSessionCompleted.Broadcast(false);
	}
	else if (OperationType == EOperationType::DirectTravel)
	{
		OnTravelRequestCompleted.Broadcast(false);
	}
}
