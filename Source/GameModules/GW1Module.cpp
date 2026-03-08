#include "pch.h"

#ifdef CYPRESS_GW1
#include <GameModules/GW1Module.h>
#include <GameHooks/fbMainHooks.h>
#include <GameHooks/fbEnginePeerHooks.h>
#include <GameHooks/fbServerHooks.h>
#include <GameHooks/fbClientHooks.h>
#include <Core/Server.h>
#include <Core/Console/ConsoleFunctions.h>

void Cypress::GW1Module::InitGameHooks()
{
	// main hooks
	INIT_HOOK(fb_realMain, OFFSET_FB_REALMAIN);
	INIT_HOOK(fb_PVZMain_Ctor, 0x140004230);
	INIT_HOOK(fb_main_createWindow, OFFSET_FB_MAIN_CREATEWINDOW);
	INIT_HOOK(fb_Environment_getHostIdentifier, OFFSET_ENVIRONMENT_GETHOSTIDENTIFIER);
	INIT_HOOK(fb_Console_writeConsole, OFFSET_CONSOLE__WRITECONSOLE);
	INIT_HOOK(fb_getServerBackendType, 0x1401DEF70);

	// fb::EnginePeer hooks
	INIT_HOOK(fb_EnginePeer_init, OFFSET_FB_ENGINEPEER_INIT);

#if(HAS_DEDICATED_SERVER)
	// fb::Server hooks
	INIT_HOOK(fb_Server_start, OFFSET_FB_SERVER_START);
	INIT_HOOK(fb_Server_update, OFFSET_FB_SERVER_UPDATE);
	INIT_HOOK(fb_editBoxWndProcProxy, OFFSET_EDITBOXWNDPROCPROXY);
	//INIT_HOOK(fb_windowProcedure, OFFSET_WINDOWPROCEDURE);
	INIT_HOOK(fb_ServerPlayerManager_addPlayer, OFFSET_SERVERPLAYERMANAGER_ADDPLAYER);
	INIT_HOOK(fb_ServerPlayer_disconnect, OFFSET_SERVERPLAYER_DISCONNECT);
	INIT_HOOK(fb_OnlineManager_connectToAddress, OFFSET_FB_ONLINEMANAGER_CONNECTTOADDRESS);
	INIT_HOOK(fb_sub_140112AA0, 0x140112AA0);
#endif

	// fb::Client hooks
	INIT_HOOK(fb_Client_enterState, OFFSET_FB_CLIENT_ENTERSTATE);
	INIT_HOOK(fb_PVZGetNumTutorialVideos, 0x140B43910);
	INIT_HOOK(fb_ClientConnection_onDisconnected, 0x1405BF3D0);
}

void Cypress::GW1Module::InitMemPatches()
{
	MemSet(0x14000D3FA, 0xDE, 1); //allowCommandlineSettings true
	MemSet(0x140390B91, 0x90, 6); //unlock all commands
	MemSet(0x140B862D8, 0x00, 1); //no tutorial popups
	MemSet(0x140B3F69D, 0xC9, 1); //infinite consumables (rcx always has a value of 1 at this point, so take the value from that instead of rbx)
}

void Cypress::GW1Module::InitDedicatedServerPatches(Cypress::Server* pServer)
{
	uintptr_t initDedicatedServerPtr = reinterpret_cast<uintptr_t>(&Server::InitDedicatedServer);
	MemPatch(0x1415780B0, (unsigned char*)&initDedicatedServerPtr, 8);

	//fb::createWindow temp crash fix
	MemSet(0x140008E15, 0xEB, 1);

	//set server mode in realMain
	BYTE ptch1[] = { 0x01, 0x00 };
	MemPatch(0x1400109C9, (unsigned char*)ptch1, sizeof(ptch1));

	//== crash fixes for dedicated server ==
	BYTE ptch2[] = { 0xEB, 0x1C };
	MemPatch(0x140B2D3DB, (unsigned __int8*)ptch2, sizeof(ptch2)); //unknown, something related to client maybe
	MemPatch(0x140C6D7BC, (unsigned __int8*)ptch2, sizeof(ptch2)); //prevent crash before LocalServerBackend::create, probably another thing related to client

	MemSet(0x140C700E9, 0x90, 4); //something related to presence

	/*
	if (pServer->GetServerLogEnabled())
	{
		MemSet(0x140139B6C, 0x90, 1); //change "Enable Logs" to "Disable Logs"
	}
	*/
}

void Cypress::GW1Module::RegisterCommands()
{
	CYPRESS_REGISTER_CONSOLE_FUNCTION("Server", "RestartLevel", "", Server::ServerRestartLevel);
	CYPRESS_REGISTER_CONSOLE_FUNCTION("Server", "LoadLevel", "<levelPath> <inclusionOptions> opt<loadScreenGameMode> opt<loadScreenLevelName> opt<loadScreenLevelDescription>", Server::ServerLoadLevel);
	CYPRESS_REGISTER_CONSOLE_FUNCTION("Server", "KickPlayer", "<playerName> opt<reason>", Server::ServerKickPlayer);
	CYPRESS_REGISTER_CONSOLE_FUNCTION("Server", "KickPlayerById", "<playerIndex> opt<reason>", Server::ServerKickPlayerById);
	CYPRESS_REGISTER_CONSOLE_FUNCTION("Server", "BanPlayer", "<playerName> opt<reason>", Server::ServerBanPlayer);
	CYPRESS_REGISTER_CONSOLE_FUNCTION("Server", "BanPlayerById", "<playerIndex> opt<reason>", Server::ServerBanPlayerById);
}
#endif