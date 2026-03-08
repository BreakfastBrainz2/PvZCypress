#include "pch.h"

#ifdef CYPRESS_GW2

#include <GameModules/GW2Module.h>
#include <GameHooks/fbMainHooks.h>
#include <GameHooks/fbEnginePeerHooks.h>
#include <GameHooks/fbServerHooks.h>
#include <GameHooks/fbClientHooks.h>
#include <Core/Server.h>
#include <Core/Program.h>
#include <Core/Console/ConsoleFunctions.h>

void Cypress::GW2Module::InitGameHooks()
{
	// main hooks
	INIT_HOOK(fb_realMain, OFFSET_FB_REALMAIN);
	INIT_HOOK(fb_main_createWindow, OFFSET_FB_MAIN_CREATEWINDOW);
	INIT_HOOK(fb_Environment_getHostIdentifier, OFFSET_ENVIRONMENT_GETHOSTIDENTIFIER);
	INIT_HOOK(fb_Console_writeConsole, OFFSET_CONSOLE__WRITECONSOLE);
	INIT_HOOK(fb_isInteractionAllowed, OFFSET_FB_ISINTERACTIONALLOWED);
	INIT_HOOK(fb_tickersAllowedToShow, OFFSET_FB_TICKERSALLOWEDTOSHOW);

	// fb::EnginePeer hooks
	INIT_HOOK(fb_EnginePeer_init, OFFSET_FB_ENGINEPEER_INIT);

#if(HAS_DEDICATED_SERVER)
	// fb::Server hooks
	INIT_HOOK(fb_Server_start, OFFSET_FB_SERVER_START);
	INIT_HOOK(fb_Server_update, OFFSET_FB_SERVER_UPDATE);
	INIT_HOOK(fb_editBoxWndProcProxy, OFFSET_EDITBOXWNDPROCPROXY)
		INIT_HOOK(fb_windowProcedure, OFFSET_WINDOWPROCEDURE);
	INIT_HOOK(fb_ServerPVZLevelControlEntity_loadLevel, OFFSET_FB_SERVERPVZLEVELCONTROLENTITY_LOADLEVEL);
	INIT_HOOK(fb_ServerLoadLevelMessage_post, OFFSET_FB_SERVERLOADLEVELMESSAGE_POST);
	INIT_HOOK(fb_ServerConnection_onCreatePlayerMsg, OFFSET_SERVERCONNECTION__ONCREATEPLAYERMESSAGE);
	INIT_HOOK(fb_ServerPlayerManager_addPlayer, OFFSET_SERVERPLAYERMANAGER_ADDPLAYER);
	INIT_HOOK(fb_ServerPlayer_disconnect, OFFSET_SERVERPLAYER_DISCONNECT);
	INIT_HOOK(fb_OnlineManager_connectToAddress, OFFSET_FB_ONLINEMANAGER_CONNECTTOADDRESS);
#endif

	// fb::Client hooks
	INIT_HOOK(fb_Client_enterState, OFFSET_FB_CLIENT_ENTERSTATE);
	// no idea what this is, it creates some kind of persistence related class
	INIT_HOOK(fb_140DA9B90, OFFSET_FB_140DA9B90);
	INIT_HOOK(fb_OnlineManager_onGotDisconnected, OFFSET_FB_ONLINEMANAGER_ONGOTDISCONNECTED);
}

void Cypress::GW2Module::InitMemPatches()
{
	MemSet(0x1401328DA, 0xE6, 1); //allowCommandlineSettings true
	MemSet(0x1422DC782, 0x00, 1); //change player name format str from %s_%u to %s
#if(HAS_DEDICATED_SERVER)
	MemSet(0x1401A9AE0, 0x90, 6); //unlock all commands
#endif
	MemSet(0x140B7C4CC, 0x90, 2); //streaming install skip, perhaps there's a better way than this
}

void Cypress::GW2Module::InitDedicatedServerPatches(Cypress::Server* pServer)
{
	uintptr_t initDedicatedServerPtr = reinterpret_cast<uintptr_t>(&Server::InitDedicatedServer);
	MemPatch(0x1421E21D8, (unsigned char*)&initDedicatedServerPtr, 8);

	//fb::createWindow temp crash fix
	MemSet(0x140139E2E, 0xEB, 1);

	//set server mode in realMain
	BYTE ptch1[] = { 0x01, 0x00 };
	MemPatch(0x14013A5D5, (unsigned char*)ptch1, sizeof(ptch1));

	if (pServer->GetServerLogEnabled())
	{
		MemSet(0x140139B6C, 0x90, 1); //change "Enable Logs" to "Disable Logs"
	}
}

void Cypress::GW2Module::RegisterCommands()
{
	//optional arguments are labeled as opt<argument>
	CYPRESS_REGISTER_CONSOLE_FUNCTION("Server", "RestartLevel", "", Server::ServerRestartLevel);
	CYPRESS_REGISTER_CONSOLE_FUNCTION("Server", "LoadLevel", "<levelPath> <inclusionOptions> opt<loadScreenGameMode> opt<loadScreenLevelName> opt<loadScreenLevelDescription>", Server::ServerLoadLevel);
	CYPRESS_REGISTER_CONSOLE_FUNCTION("Server", "KickPlayer", "<playerName> opt<reason>", Server::ServerKickPlayer);
	CYPRESS_REGISTER_CONSOLE_FUNCTION("Server", "KickPlayerById", "<playerIndex> opt<reason>", Server::ServerKickPlayerById);
	CYPRESS_REGISTER_CONSOLE_FUNCTION("Server", "BanPlayer", "<playerName> opt<reason>", Server::ServerBanPlayer);
	CYPRESS_REGISTER_CONSOLE_FUNCTION("Server", "BanPlayerById", "<playerIndex> opt<reason>", Server::ServerBanPlayerById);
	CYPRESS_REGISTER_CONSOLE_FUNCTION("Server", "Say", "<message> opt<duration>", Server::ServerSay);
	CYPRESS_REGISTER_CONSOLE_FUNCTION("Server", "SayToPlayer", "<message> opt<duration>", Server::ServerSayToPlayer);
	CYPRESS_REGISTER_CONSOLE_FUNCTION("Server", "LoadNextPlaylistSetup", "", Server::ServerLoadNextPlaylistSetup);
	CYPRESS_REGISTER_CONSOLE_FUNCTION("Server", "UnbanPlayer", "<playerName>", Server::ServerUnbanPlayer);
}

#endif