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
	void Server::ServerRestartLevel(ArgList args)
	{
		void* fbServer = g_program->GetServer()->GetFbServerInstance();
		if (!fbServer) return;

#ifdef CYPRESS_GW2
		void* curLevel = ptrread<void*>(fbServer, 0xF0);
		if (!curLevel) return;
#endif

#ifdef CYPRESS_GW1
		fb::LevelSetup* setup = (fb::LevelSetup*)((__int64)fbServer + 0x40);
#else
		fb::LevelSetup* setup = (fb::LevelSetup*)((__int64)curLevel + 0x118);
#endif
		if (!setup) return;

		fb::PostServerLoadLevelMessage(setup, true, false);
	}

	void Server::ServerLoadLevel(ArgList args)
	{
		int numArgs = args.size();
		if (numArgs < 2) return;

		std::string& levelName = args[0];
		std::string& inclusion = args[1];

		LevelSetup setup;
		if (strstr(levelName.c_str(), "Levels/") == 0)
		{
			setup.m_name = std::format("Levels/{}/{}", levelName.c_str(), levelName.c_str());
		}
		else
		{
			setup.m_name = levelName.c_str();
		}
		setup.setInclusionOptions(inclusion.c_str());

#ifdef CYPRESS_GW2
		// there's probably a better way to do this, but it works
		switch (numArgs)
		{
		case 3:
			setup.LoadScreen_GameMode = args[2].c_str();
			break;
		case 4:
			setup.LoadScreen_GameMode = args[2].c_str();
			setup.LoadScreen_LevelName = args[3].c_str();
			break;
		case 5:
			setup.LoadScreen_GameMode = args[2].c_str();
			setup.LoadScreen_LevelName = args[3].c_str();
			setup.LoadScreen_LevelDescription = args[4].c_str();
			break;
		}
#endif

		fb::PostServerLoadLevelMessage(&setup, true, false);
	}

	void ServerLoadNextPlaylistSetup(ArgList args)
	{
		if (!g_program->GetServer()->IsUsingPlaylist())
		{
			CYPRESS_LOGTOSERVER(LogLevel::Info, "Server is not using a playlist.");
			return;
		}

		const PlaylistLevelSetup* nextSetup = g_program->GetServer()->GetServerPlaylist()->GetNextSetup();
		g_program->GetServer()->LoadPlaylistSetup(nextSetup);
	}

	void Server::ServerKickPlayer(ArgList args)
	{
		ServerGameContext* gameContext = ServerGameContext::GetInstance();
		if (!gameContext) return;
		if (!gameContext->m_serverPlayerManager) return;

		ServerPlayer* player = gameContext->m_serverPlayerManager->findHumanByName(args[0].c_str());
		if (!player)
		{
			CYPRESS_LOGTOSERVER(LogLevel::Error, "Player {} not found!", args[0].c_str());
			return;
		}
		if (player->isAIOrPersistentAIPlayer())
		{
			CYPRESS_LOGTOSERVER(LogLevel::Error, "Player {} is an AI!", args[0].c_str());
			return;
		}

		eastl::string reason = "Kicked by Admin";
		if (args.size() > 1)
		{
			reason = args[1].c_str();
		}
		CYPRESS_LOGTOSERVER(LogLevel::Info, "Kicked {} ({})", player->m_name, reason.c_str());
		player->disconnect(SecureReason_KickedOut, reason);
	}

	void Server::ServerKickPlayerById(ArgList args)
	{
		ServerGameContext* gameContext = ServerGameContext::GetInstance();
		if (!gameContext) return;
		if (!gameContext->m_serverPlayerManager) return;

		ServerPlayer* player = gameContext->m_serverPlayerManager->getById(std::atoi(args[0].c_str()));
		if (!player)
		{
			CYPRESS_LOGTOSERVER(LogLevel::Error, "Player {} not found!", args[0].c_str());
			return;
		}
		if (player->isAIOrPersistentAIPlayer())
		{
			CYPRESS_LOGTOSERVER(LogLevel::Error, "Player {} is an AI!", args[0].c_str());
			return;
		}

		eastl::string reason = "Kicked by Admin";
		if (args.size() > 1)
		{
			reason = args[1].c_str();
		}
		CYPRESS_LOGTOSERVER(LogLevel::Info, "Kicked {} ({})", player->m_name, reason.c_str());
		player->disconnect(SecureReason_KickedOut, reason);
	}

	void Server::ServerBanPlayer(ArgList args)
	{
		ServerGameContext* gameContext = ServerGameContext::GetInstance();
		if (!gameContext) return;
		if (!gameContext->m_serverPlayerManager) return;

		ServerPlayer* player = gameContext->m_serverPlayerManager->findHumanByName(args[0].c_str());
		if (!player)
		{
			CYPRESS_LOGTOSERVER(LogLevel::Error, "Player {} not found!", args[0].c_str());
			return;
		}
		if (player->isAIOrPersistentAIPlayer())
		{
			CYPRESS_LOGTOSERVER(LogLevel::Error, "Player {} is an AI!", args[0].c_str());
			return;
		}

		ServerConnection* connection = gameContext->m_serverPeer->connectionForPlayer(player);
		if (!connection) return;

		eastl::string reasonText = "The Ban Hammer has spoken!";
		if (args.size() > 1)
		{
			reasonText = args[1].c_str();
		}
		g_program->GetServer()->GetServerBanlist()->AddToList(player->m_name, connection->m_machineId.c_str(), reasonText.c_str());
		gameContext->m_serverPeer->m_bannedMachines.push_back(connection->m_machineId);
		gameContext->m_serverPeer->m_bannedPlayers.push_back(player->m_name);
		player->disconnect(SecureReason_Banned, reasonText);
	}

	void Server::ServerBanPlayerById(ArgList args)
	{
		ServerGameContext* gameContext = ServerGameContext::GetInstance();
		if (!gameContext) return;
		if (!gameContext->m_serverPlayerManager) return;

		ServerPlayer* player = gameContext->m_serverPlayerManager->getById(std::atoi(args[0].c_str()));
		if (!player)
		{
			CYPRESS_LOGTOSERVER(LogLevel::Error, "Player {} not found!", args[0].c_str());
			return;
		}
		if (player->isAIOrPersistentAIPlayer())
		{
			CYPRESS_LOGTOSERVER(LogLevel::Error, "Player {} is an AI!", args[0].c_str());
			return;
		}

		ServerConnection* connection = gameContext->m_serverPeer->connectionForPlayer(player);
		if (!connection) return;

		eastl::string reasonText = "Banned by server admin";
		if (args.size() > 1)
		{
			reasonText = args[1].c_str();
		}
		g_program->GetServer()->GetServerBanlist()->AddToList(player->m_name, connection->m_machineId.c_str(), reasonText.c_str());
		gameContext->m_serverPeer->m_bannedMachines.push_back(connection->m_machineId);
		gameContext->m_serverPeer->m_bannedPlayers.push_back(player->m_name);
		player->disconnect(SecureReason_Banned, reasonText);
	}

	void Server::ServerUnbanPlayer(ArgList args)
	{
		ServerGameContext* gameContext = ServerGameContext::GetInstance();
		if (!gameContext) return;

		const char* playerName = args[0].c_str();
		if (!g_program->GetServer()->GetServerBanlist()->IsBanned(playerName))
		{
			CYPRESS_LOGTOSERVER(LogLevel::Info, "Player {} is not banned", playerName);
			return;
		}

		auto entry = g_program->GetServer()->GetServerBanlist()->GetPlayerEntry(playerName);
		gameContext->m_serverPeer->removeBannedMachine(entry.MachineId.c_str());
		gameContext->m_serverPeer->removeBannedPlayer(entry.Name.c_str());
		g_program->GetServer()->GetServerBanlist()->RemoveFromList(playerName);

		CYPRESS_LOGTOSERVER(LogLevel::Info, "Player {} has been unbanned", playerName);
	}

	void Server::ServerSay(ArgList args)
	{
		if (args.size() < 2) return;

		auto func = reinterpret_cast<void* (*)(__int64 arena, int localPlayerId)>(0x141FCEE70);
		void* msg = func(0x1429386E0, 0);

		fb::String msgText = args[0].c_str();
		float duration = std::clamp(std::stof(args[1].c_str()), 1.0f, 10.0f);
		ptrset<fb::String>(msg, 0x48, msgText);
		ptrset<float>(msg, 0x50, duration);

		ServerGameContext::GetInstance()->m_serverPeer->sendMessage(msg, nullptr);
	}

	void Server::ServerSayToPlayer(ArgList args)
	{
		if (args.size() < 3) return;

		fb::ServerPlayer* player = ServerGameContext::GetInstance()->m_serverPlayerManager->findHumanByName(args[0].c_str());
		if (!player)
		{
			CYPRESS_LOGTOSERVER(LogLevel::Error, "Player {} not found!", args[0].c_str());
			return;
		}

		fb::ServerConnection* connection = ServerGameContext::GetInstance()->m_serverPeer->connectionForPlayer(player);
		if (!connection)
		{
			CYPRESS_LOGTOSERVER(LogLevel::Error, "Connection for player {} not found!", args[0].c_str());
			return;
		}

		auto func = reinterpret_cast<void* (*)(__int64 arena, int localPlayerId)>(0x141FCEE70);
		void* msg = func(0x1429386E0, 0);

		fb::String msgText = args[1].c_str();
		float duration = std::clamp(std::stof(args[2].c_str()), 1.0f, 10.0f);
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
		setup->setInlusionOption("TOD", playlistSetup->TOD.c_str());
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