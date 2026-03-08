#pragma once
#include <Core/Logging.h>
#include <sstream>

//ConsoleStream class from KYBER
class ConsoleStream
{
private:
	std::string input;
	std::string delimiter;
	std::istringstream stream;

public:
	ConsoleStream(const std::string& inputStr, const std::string& delim)
		: input(inputStr)
		, delimiter(delim)
		, stream(input)
	{
	}

	template<typename T>
	ConsoleStream& operator>>(T& variable)
	{
		stream >> variable;
		return *this;
	}

	//test check ' as a delimiter
	ConsoleStream& operator>>(std::string& variable)
	{
		stream >> std::ws;

		if (stream.peek() == '\'')
		{
			stream.get();
			std::getline(stream, variable, '\'');
		}
		else
		{
			stream >> variable;
		}

		return *this;
	}

	//ConsoleStream& operator>>(eastl::string& variable)
	//{
	//	std::string token;
	//	if (std::getline(stream, token, delimiter[0]))
	//	{
	//		variable = token.c_str();
	//	}
	//	return *this;
	//}
};

namespace fb
{
	class ConsoleContext {
	public:
		char pad_0000[352]; //0x0000
		char* m_args; //0x0160
		char pad_0168[32]; //0x0168
		char* m_groupName; //0x0188
		char pad_0190[24]; //0x0190
		char* m_method; //0x01A8

		template<typename... Args>
		ConsoleContext& push(std::format_string<Args...> fmt, Args&&... args)
		{
			auto str = std::format(fmt, std::forward<Args>(args)...);
			CYPRESS_LOGTOSERVER(LogLevel::Info, "{}", str.c_str());
			return *this;
		}

		ConsoleStream stream(const char* delimiter = " ")
		{
			return ConsoleStream(m_args, delimiter);
		}
	}; //Size: 0x0880
}