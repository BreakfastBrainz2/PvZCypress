#include "pch.h"
#include "ConsoleFunctions.h"

#include <fb/Engine/ConsoleRegistry.h>

namespace Cypress
{
    void registerConsoleMethod(const char* groupName, const char* name, const char* description, void* pfn, void* juiceFn) 
    {
		fb::ConsoleMethod* newMethod = new fb::ConsoleMethod();
		newMethod->pfn = pfn;
		newMethod->name = name;
		newMethod->groupName = groupName;
		newMethod->description = description;
		newMethod->juiceFn = juiceFn;

		fb::ConsoleRegistry::registerConsoleMethods(groupName, newMethod);
    }
}