// Fill out your copyright notice in the Description page of Project Settings.


#include "MenuWidget.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "MultiplayerSessionsSubsystem.h"


void UMenuWidget::MenuSetup(int32 NumberOfPublicConnections, FString TypeOfMatch, FString LobbyPath )
{
	PathToLobby = FString::Printf(TEXT("%s?listen"), *LobbyPath);
	NumPublicConnections = NumberOfPublicConnections;
	MatchType = TypeOfMatch;

	AddToViewport();
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);

	UWorld* World = GetWorld();
	if (World)
	{
		APlayerController* PlayerController = World->GetFirstPlayerController();
		if (PlayerController)
		{
			FInputModeUIOnly InputModeData;
			InputModeData.SetWidgetToFocus(TakeWidget());
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			PlayerController->SetInputMode(InputModeData);
			PlayerController->SetShowMouseCursor(true);
		}
	}

	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance)
	{
		MultiplayerSessionsSubsystem = GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>();
	}

	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	if (Subsystem && Subsystem->GetSubsystemName() == "NULL")
	{
		if (LANButton) LANButton->SetVisibility(ESlateVisibility::Visible);
		if (HostButton) HostButton->SetVisibility(ESlateVisibility::Collapsed);
		if (JoinButton) JoinButton->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		if (LANButton) LANButton->SetVisibility(ESlateVisibility::Collapsed);
		if (HostButton) HostButton->SetVisibility(ESlateVisibility::Visible);
		if (JoinButton) JoinButton->SetVisibility(ESlateVisibility::Visible);
	}

	if (MultiplayerSessionsSubsystem)
	{
		MultiplayerSessionsSubsystem->MultiplayerOnCreateSessionComplete.AddDynamic(this, &ThisClass::OnCreateSession);
		MultiplayerSessionsSubsystem->MultiplayerOnFindSessionsComplete.AddUObject(this, &ThisClass::OnFindSessions);
		MultiplayerSessionsSubsystem->MultiplayerOnJoinSessionComplete.AddUObject(this, &ThisClass::OnJoinSession);
		MultiplayerSessionsSubsystem->MultiplayerOnDestroySessionComplete.AddDynamic(this, &ThisClass::OnDestroySession);
		MultiplayerSessionsSubsystem->MultiplayerOnStartSessionComplete.AddDynamic(this, &ThisClass::OnStartSession);
	}
}

bool UMenuWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (HostButton)
	{
		HostButton->OnClicked.AddDynamic(this, &UMenuWidget::HostButtonClicked);
	}
	
	if (JoinButton)
	{
		JoinButton->OnClicked.AddDynamic(this, &UMenuWidget::JoinButtonClicked);
	}

	if (LANButton)
	{
		LANButton->OnClicked.AddDynamic(this, &UMenuWidget::LANButtonClicked);
	}

	if (ConnectLanButton)
	{
		ConnectLanButton->OnClicked.AddDynamic(this, &UMenuWidget::ConnectLanToIP);
	}

	if (LANOverlay)
	{
		LANOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}

	return true;
}

void UMenuWidget::NativeDestruct()
{
	MenuTeardown();
	Super::NativeDestruct();
}

void UMenuWidget::OnCreateSession(bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			World->ServerTravel(PathToLobby);
		}
		if (MultiplayerSessionsSubsystem)
		{
			MultiplayerSessionsSubsystem->StartSession();
		}
	}
	else
	{
		if (!HostButton) return;
		HostButton->SetIsEnabled(true);
	}
}

void UMenuWidget::OnFindSessions(const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful)
{
	for (auto Result : SessionResults)
	{
		FString SettingsMatchType;
		if (Result.Session.SessionSettings.Get(FName("MatchType"), SettingsMatchType))
		{
			// Ensure these flags are set to avoid lobby join issues
			Result.Session.SessionSettings.bUseLobbiesIfAvailable = true;
			Result.Session.SessionSettings.bUsesPresence = true;

			if (SettingsMatchType == MatchType)
			{
				MultiplayerSessionsSubsystem->JoinSession(Result);
				return;
			}
		}
	}
	
	if (!bWasSuccessful || SessionResults.Num() == 0)
	{
		JoinButton->SetIsEnabled(true);
	}
}

void UMenuWidget::OnJoinSession(EOnJoinSessionCompleteResult::Type Result)
{
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	if (Subsystem)
	{
		IOnlineSessionPtr SessionInterface = Subsystem->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			FString Address;
			SessionInterface->GetResolvedConnectString(NAME_GameSession, Address);

			APlayerController* PlayerController = GetGameInstance()->GetFirstLocalPlayerController();
			if (PlayerController)
			{
				PlayerController->ClientTravel(Address, ETravelType::TRAVEL_Absolute);
			}
		}
	}

	if (Result != EOnJoinSessionCompleteResult::Success)
	{
		JoinButton->SetIsEnabled(true);
	}
}

void UMenuWidget::OnDestroySession(bool bWasSuccessful)
{
	//Working depends on project
}

void UMenuWidget::OnStartSession(bool bWasSuccessfull)
{
	//Working depends on project
}

void UMenuWidget::HostButtonClicked()
{
	HostButton->SetIsEnabled(false);
	if (MultiplayerSessionsSubsystem)
	{
		MultiplayerSessionsSubsystem->CreateSession(NumPublicConnections, MatchType); 
	}
}

void UMenuWidget::JoinButtonClicked()
{
	JoinButton->SetIsEnabled(false);
	if (MultiplayerSessionsSubsystem)
	{
		MultiplayerSessionsSubsystem->FindSessions(10000);
	}
}

void UMenuWidget::LANButtonClicked()
{
	if (LANOverlay)
	{
		LANOverlay->SetVisibility(ESlateVisibility::Visible);
	}

	if (LANButton)
	{
		LANButton->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UMenuWidget::ConnectLanToIP()
{
	if (!IPEditBox) return;

	FString IPAddress = IPEditBox->GetText().ToString();

	if (APlayerController* PlayerController = GetGameInstance()->GetFirstLocalPlayerController())
	{
		FString TravelAddress = FString::Printf(TEXT("%s"), *IPAddress);
		PlayerController->ClientTravel(TravelAddress, ETravelType::TRAVEL_Absolute);
	}
}

void UMenuWidget::MenuTeardown()
{
	RemoveFromParent();
	UWorld* World = GetWorld();
	if (World)
	{
		APlayerController* PlayerController = World->GetFirstPlayerController();
		if (PlayerController)
		{
			FInputModeGameOnly InputModeData;
			PlayerController->SetInputMode(InputModeData);
			PlayerController->SetShowMouseCursor(false);
		}
	}
}
