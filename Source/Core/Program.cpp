#include "pch.h"

#include <Unknwn.h>
#include <cassert>
#include <format>
#include <Core/Program.h>
#include <Core/VersionInfo.h>
#include <Core/Logging.h>
#include <Core/Assert.h>
#include <Core/Config.h>
#include <Core/Console/ConsoleFunctions.h>
#ifdef CYPRESS_GW1
#include <GameModules/GW1Module.h>
#else
#include <GameModules/GW2Module.h>
#endif

#include <fb/Engine/ExecutionContext.h>
#include <fb/Engine/Server.h>
#include <fb/Main.h>
#include <fb/Engine/ServerConnection.h>
#include <fb/Engine/ServerPlayerManager.h>
#include <fb/Engine/ServerPlayer.h>

Cypress::Program* g_program = nullptr;

#pragma region DirectInput8 Hijacking

typedef HRESULT(*DirectInput8Create_t)(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID* ppvOut, LPUNKNOWN punkOuter);
static DirectInput8Create_t DirectInput8CreateOriginal;

extern "C"
{
	HRESULT __declspec(dllexport) DirectInput8Create(
		HINSTANCE hinst,
		DWORD dwVersion,
		REFIID riidltf,
		LPVOID* ppvOut,
		LPUNKNOWN punkOuter
	)
	{
		if (DirectInput8CreateOriginal)
			return DirectInput8CreateOriginal(hinst, dwVersion, riidltf, ppvOut, punkOuter);

		return S_FALSE;
	}
}

void InitDirectInput8Exports()
{
	char dinputDLLName[MAX_PATH];
	GetSystemDirectoryA(dinputDLLName, MAX_PATH);
	strcat_s(dinputDLLName, "\\dinput8.dll");

	HMODULE dinputModule = LoadLibraryA(dinputDLLName);
	if (!dinputModule)
		dinputModule = LoadLibraryA("dinput8_org.dll");
	if (!dinputModule)
	{
		printf("Failed to load dinput8 library.\n (Attempted system dinput8.dll and dinput8_org.dll).\n");
		return;
	}
	DirectInput8CreateOriginal = (DirectInput8Create_t)GetProcAddress(dinputModule, "DirectInput8Create");
}

#pragma endregion

#if(HAS_DEDICATED_SERVER)

// CreateMutexA hook
// Required for running multiple instances of frostbite games on the same machine.

DECLARE_HOOK(
	CreateMutexA,
	__stdcall,
	HANDLE,

	LPSECURITY_ATTRIBUTES lpMutexAttributes,
	BOOL bInitialOwner,
	LPCSTR lpName
);

DEFINE_HOOK(
	CreateMutexA,
	__stdcall,
	HANDLE,

	LPSECURITY_ATTRIBUTES lpMutexAttributes,
	BOOL bInitialOwner,
	LPCSTR lpName
)
{
	if (lpName
		&& g_program->AllowMultipleInstances()
		&& strcmp(lpName, "Global\\{5C009BF0-B353-4fc3-A37D-CC14511238AC}_Instance_Mutex") == 0)
	{
		return nullptr;
	}

	return Orig_CreateMutexA(lpMutexAttributes, bInitialOwner, lpName);
}

#endif

BOOL WINAPI ConsoleCloseHandler(DWORD ctrlType)
{
	switch (ctrlType)
	{
	case CTRL_CLOSE_EVENT: return TRUE;
	default: return FALSE;
	}
}

namespace Cypress
{
	Program::Program(HMODULE inModule)
		: m_hModule(inModule)
		, m_client(nullptr)
#if(HAS_DEDICATED_SERVER)
		, m_server(nullptr)
#endif
		, m_initialized(false)
		, m_wsaStartupInitialized(false)
		, m_allowMultipleInstances(false)
		, m_consoleEnabled(false)
		, m_headlessMode(false)
		, m_logFileEnabled(false)
	{
		InitDirectInput8Exports();

		MH_STATUS mhInitStatus = MH_Initialize();
		CYPRESS_ASSERT(mhInitStatus == MH_OK, "MinHook failed to initialize");

		//if (fb::ExecutionContext::getOptionValue("console"))
		{
			AllocConsole();
			FILE* dummy;
			freopen_s(&dummy, "CONOUT$", "w", stdout);

			HANDLE stdHandle = GetStdHandle(STD_OUTPUT_HANDLE);
			DWORD dwMode;
			GetConsoleMode(stdHandle, &dwMode);
			dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
			SetConsoleMode(stdHandle, dwMode);

			char consoleTitle[512];
			snprintf(consoleTitle, sizeof(consoleTitle), "%s Cypress Client - %s v%s", CYPRESS_GAME_NAME, CYPRESS_BUILD_CONFIG, GetCypressVersion().c_str());
			SetConsoleTitleA(consoleTitle);

			HWND consoleWnd = GetConsoleWindow();
			LONG style = GetWindowLongA(consoleWnd, GWL_STYLE);
			style &= ~WS_SYSMENU;

			SetWindowLongA(consoleWnd, GWL_STYLE, style);
			SetWindowPos(consoleWnd, NULL, 0, 0, 0, 0,
				SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

			SetConsoleCtrlHandler(ConsoleCloseHandler, TRUE);
		}
		
#ifdef CYPRESS_GW1
		m_gameModule = new GW1Module();
#elif defined(CYPRESS_GW2)
		m_gameModule = new GW2Module();
#endif

#if(HAS_DEDICATED_SERVER)
		INIT_HOOK(CreateMutexA, CreateMutexA);
#endif

		// setup our game hooks
		m_gameModule->InitGameHooks();
		CYPRESS_LOGMESSAGE(LogLevel::Debug, "Initialized Game Hooks");
		m_gameModule->InitMemPatches();
		CYPRESS_LOGMESSAGE(LogLevel::Debug, "Initialized Patches");
	}

	Program::~Program()
	{

	}

	void Program::InitConfig()
	{
		CYPRESS_LOGMESSAGE(LogLevel::Info, "Initializing Config");

		if (fb::ExecutionContext::argc() <= 1)
		{
			char launchArgsBuf[8192];
			if (GetEnvironmentVariableA("GW_LAUNCH_ARGS", launchArgsBuf, sizeof(launchArgsBuf)))
			{
				fb::ExecutionContext::addOptions(false, launchArgsBuf);
				CYPRESS_LOGMESSAGE(LogLevel::Info, "Added options from Cypress launcher: {}", launchArgsBuf);
			}
		}

		CYPRESS_ASSERT(fb::ExecutionContext::argc() >= 3, "No commandline arguments provided! Are you using the launcher?");

		m_allowMultipleInstances = fb::ExecutionContext::getOptionValue("allowMultipleInstances");
		
#if(HAS_DEDICATED_SERVER)
		if (fb::ExecutionContext::getOptionValue("server"))
		{
			m_server = new Server();
			m_server->SetServerLogEnabled(fb::ExecutionContext::getOptionValue("enableServerLog") != nullptr);
			m_server->m_usingPlaylist = fb::ExecutionContext::getOptionValue("usePlaylist") != nullptr;

			if (m_server->m_usingPlaylist)
			{
				const char* playlistFilename = fb::ExecutionContext::getOptionValue("playlistFilename");
				CYPRESS_ASSERT(playlistFilename != nullptr, "Playlist filename must be provided with the -playlistFilename argument!");
				CYPRESS_ASSERT(m_server->m_playlist.LoadFromFile(playlistFilename), "Playlist %s could not be loaded!", playlistFilename);
			}
		}
		if (!m_server)
#else
		bool tryingToRunAsServer = fb::ExecutionContext::getOptionValue("server") != nullptr;
		CYPRESS_ASSERT(tryingToRunAsServer == false, "This build cannot be run as a server!");
#endif
		{
			m_client = new Client();
			m_client->m_playerName = fb::ExecutionContext::getOptionValue("playerName");

			CYPRESS_ASSERT(m_client->m_playerName != nullptr, "A username must be provided with the -playerName argument!");
		}
		m_consoleEnabled = fb::ExecutionContext::getOptionValue("console");
		m_headlessMode = fb::ExecutionContext::getOptionValue("headless");
		m_logFileEnabled = fb::ExecutionContext::getOptionValue("enableLog");

		// we don't really need this right now
		// restore fb::Main::setClientTitle in ClientCallbacks
		//__int64 clt = 0x14012F490;
		//MemPatch(0x1421E2050, (byte*)&clt, 8);

#if(HAS_DEDICATED_SERVER)
		if (IsServer())
		{
			char serverConsoleTitle[512];
			snprintf(serverConsoleTitle, sizeof(serverConsoleTitle), "%s Cypress Server - %s v%s", CYPRESS_GAME_NAME, CYPRESS_BUILD_CONFIG, GetCypressVersion().c_str());
			SetConsoleTitleA(serverConsoleTitle);

			m_gameModule->InitDedicatedServerPatches(m_server);
		}
#endif
	}

	bool Program::InitWSA()
	{
		CYPRESS_ASSERT(!m_wsaStartupInitialized, "WSA has already been initialized!");

		WSADATA wsaData;
		int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData); //equals to WSAStartup(0x202u, &WSAData);
		if (iResult)
		{
			CYPRESS_LOGMESSAGE(LogLevel::Error, "WSAStartup failed! Result: {}", iResult);
			return false;
		}
		if (wsaData.wVersion != 514)
		{
			CYPRESS_LOGMESSAGE(LogLevel::Error, "Wrong version of sockets detected (expected: 514, got: {})", wsaData.wVersion);
			WSACleanup();
			return false;
		}
		CYPRESS_LOGMESSAGE(LogLevel::Info, "WSAStartup Success!");
		return true;
	}
}