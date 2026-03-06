#include "pch.h"
#include <Core/Program.h>
#include <Core/Logging.h>
#include <Core/Config.h>
#include <fb/Engine/Server.h>

HWND* g_listBox = (HWND*)OFFSET_LISTBOX;

void Cypress_LogToServer(const char* msg, const char* fileName, int lineNumber, LogLevel logLevel)
{
#if(HAS_DEDICATED_SERVER)
    time_t now = time(nullptr);
    struct tm* tm_info = localtime(&now);

    char timePrefix[64];
    strftime(timePrefix, sizeof(timePrefix), "[%d.%m.%Y %H:%M:%S]", tm_info);

    const char* filePath = strrchr(fileName, '\\');
    if (filePath == nullptr)
        filePath = fileName;
    else
        ++filePath;

#endif
    std::string formattedLog = std::format("{} [{}] [{}:{}] {}", timePrefix, Cypress_LogLevelToStr(logLevel), filePath, lineNumber, msg);

    //bool* m_logOutputEnabled = (bool*)(OFFSET_LOGOUTPUTENABLED);
    if (*g_listBox == NULL)
        std::cout << "\x1B[36m[SrvLog]" << formattedLog.c_str() << "\x1B[0m";
    else 
    {
        if(g_program->GetServer()->GetServerLogEnabled())
        {
            int pos = (int)SendMessageA(*g_listBox, LB_ADDSTRING, 0, (LPARAM)formattedLog.c_str());
            if (pos >= 1000)
            {
                SendMessage(*g_listBox, LB_DELETESTRING, 0, 0);
                pos--;
            }

            SendMessage(*g_listBox, LB_SETCURSEL, pos, 1);
        }
    }
}
