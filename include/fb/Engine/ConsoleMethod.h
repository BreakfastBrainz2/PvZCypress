#pragma once

namespace fb
{
	class ConsoleMethod
	{
	public:
		void* pfn; //0x0000
		const char* name; //0x0008
		const char* groupName; //0x0010
		const char* description; //0x0018
		void* juiceFn; //0x0020
	}; //Size: 0x0088
}