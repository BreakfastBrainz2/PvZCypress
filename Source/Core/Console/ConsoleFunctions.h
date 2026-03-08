#pragma once
#define OFFSET_CONSOLEOBJECTUTIL_PROPERTYFUNC CYPRESS_GW_SELECT(0x140390470, 0x1401A89E0)

namespace Cypress
{
	void registerConsoleMethod(const char* groupName, const char* name, const char* description, void* pfn = (void*)OFFSET_CONSOLEOBJECTUTIL_PROPERTYFUNC, void* juiceFn = (void*)0);

#define CYPRESS_REGISTER_CONSOLE_FUNCTION(groupName, name, description, pfn) \
		registerConsoleMethod(groupName, name, description, pfn, 0)
}