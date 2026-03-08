#pragma once
#include "ConsoleMethod.h"

namespace fb
{
	class ConsoleRegistry
	{
	public:
		static void registerConsoleMethods(const char* groupName, fb::ConsoleMethod* method)
		{
			reinterpret_cast<void(__fastcall*)(const char*, fb::ConsoleMethod*, int)>(CYPRESS_GW_SELECT(0x14036EA40, 0x14017E070))(groupName, method, 1);
		}
	};
}