#include "pch.h"
#include "Psapi.h"
#include "Server.h"
#include <string>
#include <sstream>
#include <Core/Program.h>
#include <Core/Settings.h>
#include <fb/Engine/LevelSetup.h>
#include <fb/Engine/Server.h>
#include <fb/Main.h>
#include <fb/Engine/String.h>
#include <fb/Engine/ServerPlayerManager.h>
#include <fb/Engine/ServerPeer.h>
#include <fb/Engine/ExecutionContext.h>
#include <Core/Console/ConsoleFunctions.h>
#include <fb/Engine/ServerGameContext.h>

using namespace fb;

#if(HAS_DEDICATED_SERVER)
namespace Cypress
{
	void Server::ServerRestartLevel(fb::ConsoleContext& cc)
	{ 
		//fb::PVZServerLevelManager::restartLevel();
		reinterpret_cast<void(__fastcall*)()>(CYPRESS_GW_SELECT(0x14078EDA0, 0x140674180))();
	}

	void Server::ServerLoadLevel(fb::ConsoleContext& cc)
	{
		auto stream = cc.stream();
		std::string levelName;
		std::string inclusionOptions;
		std::string levelDescription;
		std::string loadScreenGameMode;
		std::string loadScreenLevelName;
		std::string loadScreenLevelDescription;

		stream >> levelName >> inclusionOptions >> levelDescription >> loadScreenGameMode >> loadScreenLevelName >> loadScreenLevelDescription;

		if (inclusionOptions.find("GameMode=") == std::string::npos)
		{
			cc.push("InclusionOptions must contain GameMode, syntax is \'GameMode=GameModeName\'");
			return;
		}

		if (inclusionOptions.find("TOD=") == std::string::npos)
		{
			cc.push("TOD InclusionOption not set, defaulting to Day");

			if (!inclusionOptions.ends_with(";"))
				inclusionOptions += ";";

			inclusionOptions += "TOD=Day";
		}

		if (inclusionOptions.find("HostedMode=") == std::string::npos)
		{
			cc.push("HostedMode InclusionOption not set, defaulting to ServerHosted");

			if (!inclusionOptions.ends_with(";"))
				inclusionOptions += ";";

			inclusionOptions += "HostedMode=ServerHosted";
		}

		LevelSetup setup;
		if (!levelName.starts_with("Levels/"))
		{
			setup.m_name = std::format("Levels/{}/{}", levelName, levelName);
		}
		else
		{
			setup.m_name = levelName;
		}
		setup.setInclusionOptions(inclusionOptions.c_str());

#ifdef CYPRESS_GW2
		if (!loadScreenGameMode.empty())
			setup.LoadScreen_GameMode = loadScreenGameMode;

		if (!loadScreenLevelName.empty())
			setup.LoadScreen_GameMode = loadScreenLevelName;

		if (!loadScreenLevelDescription.empty())
			setup.LoadScreen_LevelDescription = loadScreenLevelDescription;
#endif

		fb::PostServerLoadLevelMessage(&setup, true, false);
		cc.push("Loading {} {}", levelName, inclusionOptions);
	}

	void Server::ServerLoadNextPlaylistSetup(fb::ConsoleContext& cc)
	{
		if (!g_program->GetServer()->IsUsingPlaylist())
		{
			cc.push("Server is not using a playlist.");
			return;
		}

		const PlaylistLevelSetup* nextSetup = g_program->GetServer()->GetServerPlaylist()->GetNextSetup();
		g_program->GetServer()->LoadPlaylistSetup(nextSetup);
	}

	void Server::ServerKickPlayer(fb::ConsoleContext& cc)
	{
		auto stream = cc.stream();
		std::string playerName;
		std::string reason;

		stream >> playerName >> reason;

		ServerGameContext* gameContext = ServerGameContext::GetInstance();
		if (!gameContext) return;
		if (!gameContext->m_serverPlayerManager) return;

		ServerPlayer* player = gameContext->m_serverPlayerManager->findHumanByName(playerName.c_str());
		if (!player)
		{
			cc.push("Player {} not found!", playerName.c_str());
			return;
		}
		if (player->isAIOrPersistentAIPlayer())
		{
			cc.push("Player {} is an AI!", playerName.c_str());
			return;
		}

		eastl::string eastlReason = "Kicked by Admin";
		if (!reason.empty())
		{
			eastlReason = reason.c_str();
		}
		cc.push("Kicked {} ({})", player->m_name, eastlReason.c_str());
		player->disconnect(SecureReason_KickedOut, eastlReason);
	}

	void Server::ServerKickPlayerById(fb::ConsoleContext& cc)
	{
		auto stream = cc.stream();
		int playerIndex;
		std::string reason;

		stream >> playerIndex >> reason;

		ServerGameContext* gameContext = ServerGameContext::GetInstance();
		if (!gameContext) return;
		if (!gameContext->m_serverPlayerManager) return;

		ServerPlayer* player = gameContext->m_serverPlayerManager->getById(playerIndex);
		if (!player)
		{
			cc.push("No player found at index {}", playerIndex);
			return;
		}
		if (player->isAIOrPersistentAIPlayer())
		{
			cc.push("Player {} (index {}) is an AI!", player->m_name, playerIndex);
			return;
		}

		eastl::string eastlReason = "Kicked by Admin";
		if (!reason.empty())
		{
			eastlReason = reason.c_str();
		}
		cc.push("Kicked {} ({})", player->m_name, eastlReason.c_str());
		player->disconnect(SecureReason_KickedOut, eastlReason);
	}

	void Server::ServerBanPlayer(fb::ConsoleContext& cc)
	{
		auto stream = cc.stream();
		std::string playerName;
		std::string reason;

		stream >> playerName >> reason;

		ServerGameContext* gameContext = ServerGameContext::GetInstance();
		if (!gameContext) return;
		if (!gameContext->m_serverPlayerManager) return;

		ServerPlayer* player = gameContext->m_serverPlayerManager->findHumanByName(playerName.c_str());
		if (!player)
		{
			cc.push("Player {} not found!", playerName.c_str());
			return;
		}
		if (player->isAIOrPersistentAIPlayer())
		{
			cc.push("Player {} is an AI!", playerName.c_str());
			return;
		}

		ServerConnection* connection = gameContext->m_serverPeer->connectionForPlayer(player);
		if (!connection) return;

		eastl::string reasonText = "The Ban Hammer has spoken!";
		if (!reason.empty())
		{
			reasonText = reason.c_str();
		}

		g_program->GetServer()->GetServerBanlist()->AddToList(player->m_name, connection->m_machineId.c_str(), reasonText.c_str());
		gameContext->m_serverPeer->m_bannedMachines.push_back(connection->m_machineId);
		gameContext->m_serverPeer->m_bannedPlayers.push_back(player->m_name);
		player->disconnect(SecureReason_Banned, reasonText);
	}

	void Server::ServerBanPlayerById(fb::ConsoleContext& cc)
	{
		auto stream = cc.stream();
		int playerIndex;
		std::string reason;

		stream >> playerIndex >> reason;

		ServerGameContext* gameContext = ServerGameContext::GetInstance();
		if (!gameContext) return;
		if (!gameContext->m_serverPlayerManager) return;

		ServerPlayer* player = gameContext->m_serverPlayerManager->getById(playerIndex);
		if (!player)
		{
			cc.push("No player found at index {}", playerIndex);
			return;
		}
		if (player->isAIOrPersistentAIPlayer())
		{
			cc.push("Player {} (index {}) is an AI!", player->m_name, playerIndex);
			return;
		}

		ServerConnection* connection = gameContext->m_serverPeer->connectionForPlayer(player);
		if (!connection) return;

		eastl::string reasonText = "Banned by server admin";
		if (!reason.empty())
		{
			reasonText = reason.c_str();
		}
		g_program->GetServer()->GetServerBanlist()->AddToList(player->m_name, connection->m_machineId.c_str(), reasonText.c_str());
		gameContext->m_serverPeer->m_bannedMachines.push_back(connection->m_machineId);
		gameContext->m_serverPeer->m_bannedPlayers.push_back(player->m_name);
		player->disconnect(SecureReason_Banned, reasonText);
	}

	void Server::ServerUnbanPlayer(fb::ConsoleContext& cc)
	{
		auto stream = cc.stream();
		std::string playerName;

		stream >> playerName;

		ServerGameContext* gameContext = ServerGameContext::GetInstance();
		if (!gameContext) return;

		if (!g_program->GetServer()->GetServerBanlist()->IsBanned(playerName.c_str()))
		{
			cc.push("Player {} is not banned", playerName);
			return;
		}

		auto entry = g_program->GetServer()->GetServerBanlist()->GetPlayerEntry(playerName.c_str());
		gameContext->m_serverPeer->removeBannedMachine(entry.MachineId.c_str());
		gameContext->m_serverPeer->removeBannedPlayer(entry.Name.c_str());
		g_program->GetServer()->GetServerBanlist()->RemoveFromList(playerName.c_str());

		cc.push("Player {} has been unbanned", playerName);
	}

	void Server::ServerSay(fb::ConsoleContext& cc)
	{
		auto stream = cc.stream();
		std::string message;
		float duration;

		stream >> message >> duration;

		if (message.empty())
		{
			cc.push("Wrong parameters, missing message");
			return;
		}

		//TODO: IMPL GW1
		auto func = reinterpret_cast<void* (*)(__int64 arena, int localPlayerId)>(0x141FCEE70);
		void* msg = func(0x1429386E0, 0);

		fb::String msgText = message.c_str();
		float messageDuration = std::clamp(duration, 1.0f, 10.0f);

		ptrset<fb::String>(msg, 0x48, msgText);
		ptrset<float>(msg, 0x50, messageDuration);

		ServerGameContext::GetInstance()->m_serverPeer->sendMessage(msg, nullptr);
	}

	void Server::ServerSayToPlayer(fb::ConsoleContext& cc)
	{
		auto stream = cc.stream();
		std::string playerName;
		std::string message;
		float duration;

		stream >> playerName >> message >> duration;

		fb::ServerPlayer* player = ServerGameContext::GetInstance()->m_serverPlayerManager->findHumanByName(playerName.c_str());
		if (!player)
		{
			cc.push("Player {} not found!", playerName.c_str());
			return;
		}

		fb::ServerConnection* connection = ServerGameContext::GetInstance()->m_serverPeer->connectionForPlayer(player);
		if (!connection)
		{
			cc.push("Connection for player {} not found!", playerName.c_str());
			return;
		}

		//TODO: IMPL GW1
		auto func = reinterpret_cast<void* (*)(__int64 arena, int localPlayerId)>(0x141FCEE70);
		void* msg = func(0x1429386E0, 0);

		fb::String msgText = message.c_str();
		float messageDuration = std::clamp(duration, 1.0f, 10.0f);

		ptrset<fb::String>(msg, 0x48, msgText);
		ptrset<float>(msg, 0x50, messageDuration);

		connection->sendMessage(msg);
	}

	Server::Server()
		: m_socketManager(new Kyber::SocketManager(Kyber::ProtocolDirection::Clientbound, Kyber::SocketSpawnInfo(false, "", "")))
		, m_fbServerInstance(nullptr)
		, m_mainWindow((HWND*)(OFFSET_MAINWND))
		, m_commandBox((HWND*)(OFFSET_COMMANDBOX))
		, m_toggleLogButtonBox((HWND*)(OFFSET_TOGGLELOGBUTTONBOX))
		, m_running(false)
		, m_statusUpdated(false)
		, m_serverLogEnabled(false)
		, m_usingPlaylist(false)
		, m_loadRequestFromLevelControl(false)
		, m_statusCol1()
		, m_statusCol2()
		, m_banlist()
		, m_playlist()
	{

	}

	Server::~Server()
	{
	}

	void Server::UpdateStatus(void* fbServerInstance, float deltaTime)
	{
		static unsigned int startSystemTime = GetSystemTime();
		unsigned int sec = (GetSystemTime() - startSystemTime) / 1000;
		unsigned int min = (sec / 60) % 60;
		unsigned int hour = (sec / (60 * 60));

		static unsigned int lastSec = 0;
		static float currentDeltaTime = 0;
		static float sumDeltaTime = 0;
		static unsigned int frameCount = 0;

		m_fbServerInstance = fbServerInstance;

		sumDeltaTime += deltaTime;
		++frameCount;

		if (lastSec != sec)
		{
			lastSec = sec;
			currentDeltaTime = sumDeltaTime / float(frameCount);
			sumDeltaTime = 0;
			frameCount = 0;
		}

		fb::ServerPlayerManager* playerMgr = ptrread<fb::ServerPlayerManager*>(fbServerInstance, CYPRESS_GW_SELECT(0x98, 0xA0));
		fb::ServerPeer* serverPeer = ptrread<fb::ServerPeer*>(fbServerInstance, CYPRESS_GW_SELECT(0x90, 0x98));
		__int64 ghostMgr = (__int64)serverPeer->GetGhostManager();
		unsigned int numGhosts = ghostMgr ? (*(unsigned int*)(ghostMgr + 0x468) - *(unsigned int*)(ghostMgr + 0x460)) >> 3 : 0;

		//@TODO: GW1
#ifndef CYPRESS_GW1
		unsigned int maxPlayerCount = *(int*)(*(__int64*)0x142C01048 + 0x30);
		if (serverPeer)
			maxPlayerCount = std::min(maxPlayerCount, serverPeer->maxClientCount());

		std::string playerCountStr = std::format("{}/{} ({}/{}) [{}]",
			playerMgr->humanPlayerCount(),
			maxPlayerCount - playerMgr->maxSpectatorCount(),
			playerMgr->spectatorCount(),
			playerMgr->maxSpectatorCount(),
			*(int*)(*(__int64*)0x142C01048 + 0x30));
		std::string aiPlayerCountStr = std::format("{}/{} [{}]", playerMgr->aiPlayerCount(), 64, 64 - (playerMgr->humanPlayerCount() + playerMgr->aiPlayerCount()));

		g_program->GetServer()->SetStatusColumn1(
			std::format(
			"FPS: {} \t\t\t\t"
			"UpTime: {}:{}:{} \t\t\t"
			"PlayerCount: {} \t\t\t"
			"GhostCount: {} \t\t\t"
			"Memory (CPU): {} MB",
			int(1.0f / currentDeltaTime),
			hour,
			min,
			sec % 60,
			playerCountStr,
			numGhosts,
			GetMemoryUsage()
		));

		void* curLevel = ptrread<void*>(fbServerInstance, 0xF0);
		if (curLevel)
		{
			fb::LevelSetup* setup = (fb::LevelSetup*)((__int64)curLevel + 0x118);
			g_program->GetServer()->SetStatusColumn2(
				std::format(
					"Level: {} \t\t"
					"GameMode: {} \t\t"
					"HostedMode: {} \t\t"
					"TOD: {}\t\t\t\t"
					"Platform: {}",
					extractFileName(setup->m_name.c_str()),
					setup->getInclusionOption("GameMode"),
					setup->getInclusionOption("HostedMode"),
					setup->getInclusionOption("TOD"),
					*(const char**)0x14294E410
				));
		}
		else
		{
			g_program->GetServer()->SetStatusColumn2("Level: No level");
		}
#endif

		static size_t tick = 0;
		size_t fps = int(1.0f / currentDeltaTime);
		if (tick != fps)
		{
			tick = fps;
			g_program->GetServer()->SetStatusUpdated(false);
		}
	}

	size_t Server::GetMemoryUsage()
	{
		PROCESS_MEMORY_COUNTERS_EX pmc;
		if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
			return pmc.WorkingSetSize / (1024 * 1024); //MegaBytes
		}
		return 0;
	}

	void Server::LoadPlaylistSetup(const PlaylistLevelSetup* nextSetup)
	{
		LevelSetup setup;
		LevelSetupFromPlaylistSetup(&setup, nextSetup);
		ApplySettingsFromPlaylistSetup(nextSetup);

		CYPRESS_LOGTOSERVER(LogLevel::Info, "Server is loading playlist setup ({} on {})", setup.m_name.c_str(), setup.getInclusionOption("GameMode"));
		fb::PostServerLoadLevelMessage(&setup, true, false);
	}

	void Server::LevelSetupFromPlaylistSetup(LevelSetup* setup, const PlaylistLevelSetup* playlistSetup)
	{
		setup->m_name = playlistSetup->LevelName.c_str();
		setup->setInlusionOption("GameMode", playlistSetup->GameMode.c_str());
		setup->setInlusionOption("HostedMode", "ServerHosted");
		setup->setInlusionOption("TOD", "Day");
#ifdef CYPRESS_GW2
		if (!playlistSetup->Loadscreen_GamemodeName.empty())
			setup->LoadScreen_GameMode = playlistSetup->Loadscreen_GamemodeName.c_str();
		if (!playlistSetup->Loadscreen_LevelName.empty())
			setup->LoadScreen_LevelName = playlistSetup->Loadscreen_LevelName.c_str();
		if (!playlistSetup->Loadscreen_LevelDescription.empty())
			setup->LoadScreen_LevelDescription = playlistSetup->Loadscreen_LevelDescription.c_str();
#endif
	}

	void Server::ApplySettingsFromPlaylistSetup(const PlaylistLevelSetup* playlistSetup)
	{
		if (!playlistSetup->SettingsToApply.empty())
		{
			std::vector<std::string> settings = splitString(playlistSetup->SettingsToApply, '|');

			for (const auto& setting : settings)
			{
				std::vector<std::string> settingAndValue = splitString(setting, ' ');
				if (settingAndValue.size() != 2) continue;

				CYPRESS_LOGTOSERVER(LogLevel::Info, "Playlist is setting {} to {}", settingAndValue[0].c_str(), settingAndValue[1].c_str());
				if (settingAndValue[1].c_str()[0] == '^') // empty string
				{
					fb::SettingsManager::GetInstance()->set(settingAndValue[0].c_str(), "");
				}
				else
				{
					fb::SettingsManager::GetInstance()->set(settingAndValue[0].c_str(), settingAndValue[1].c_str());
				}
			}
		}
	}

	void Server::InitDedicatedServer(void* thisPtr)
	{
		g_program->GetServer()->SetRunning(true);

		auto fb_createWindow = reinterpret_cast<__int64(*)()>(CYPRESS_GW_SELECT(0x140008D90, 0x14012F7E0));

		if (!g_program->IsHeadless())
		{
			fb_createWindow();
		}

		LevelSetup initialLevelSetup;
		if (g_program->GetServer()->m_usingPlaylist)
		{
			CYPRESS_LOGTOSERVER(LogLevel::Info, "Loading first setup in playlist");
			const PlaylistLevelSetup* playlistSetup = g_program->GetServer()->m_playlist.GetSetup(0);
			g_program->GetServer()->LevelSetupFromPlaylistSetup(&initialLevelSetup, playlistSetup);
			g_program->GetServer()->ApplySettingsFromPlaylistSetup(playlistSetup);
		}
		else
		{
			const char* initialLevel = fb::ExecutionContext::getOptionValue("level");
			const char* initialInclusion = fb::ExecutionContext::getOptionValue("inclusion");
			CYPRESS_ASSERT(initialLevel != nullptr, "Must provide a level name via the -level argument!");
			CYPRESS_ASSERT(initialInclusion != nullptr, "Must provide inclusion options via the -inclusion argument!");

			if (strstr(initialLevel, "Levels/") == 0)
			{
				// this is fine since Name is an fb::String, which copies the string its given
				initialLevelSetup.m_name = std::format("Levels/{}/{}", initialLevel, initialLevel);
			}
			else
			{
				initialLevelSetup.m_name = initialLevel;
			}
			initialLevelSetup.setInclusionOptions(initialInclusion);
		}

		ServerSpawnInfo* spawnInfo = new ServerSpawnInfo(&initialLevelSetup);

		auto fb_spawnServer = reinterpret_cast<void (*)(void* thisPtr, ServerSpawnInfo* info)>(OFFSET_FB_MAIN_SPAWNSERVER);
		fb_spawnServer(thisPtr, spawnInfo);

		g_program->GetGameModule()->RegisterCommands();

		g_program->GetServer()->m_banlist.LoadFromFile("bans.json");
		ServerPeer* peer = ServerGameContext::GetInstance()->m_serverPeer;
		for (const auto& player : g_program->GetServer()->m_banlist.GetBannedPlayers())
		{
			peer->m_bannedPlayers.push_back(player.Name.c_str());
			peer->m_bannedMachines.push_back(player.MachineId.c_str());
		}

		CYPRESS_LOGMESSAGE(LogLevel::Info, "Initialized Dedicated Server");
	}

	unsigned int Server::GetSystemTime()
	{
		static bool l_isInitialized = false;
		static LARGE_INTEGER    l_liPerformanceFrequency;
		static LARGE_INTEGER    l_liBaseTime;

		LARGE_INTEGER liEndTime;

		if (!l_isInitialized) {
			QueryPerformanceFrequency(&l_liPerformanceFrequency);
			QueryPerformanceCounter(&l_liBaseTime);
			l_isInitialized = true;

			return 0;
		}

		unsigned __int64 n;
		QueryPerformanceCounter(&liEndTime);
		n = liEndTime.QuadPart - l_liBaseTime.QuadPart;
		n = n * 1000 / l_liPerformanceFrequency.QuadPart;

		return (unsigned int)n;
	}
}
#endif