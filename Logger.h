#pragma once
#include <cstdarg>
#include <cstdio>
#include "mq/Plugin.h"

namespace remote {
class Logger
{
public:
	// Logging flags exposed to other classes
	enum LogFlag : int
	{
		LOG_GENERAL = 1 << 0,
		LOG_SEND = 1 << 1,
		LOG_RECEIVE = 1 << 2,
	};

	Logger() : m_settings(LOG_GENERAL | LOG_SEND | LOG_RECEIVE) {}

	// Variadic log function
	void Log(LogFlag flag, const char* szFormat, ...) const
	{
		if (!IsEnabled(flag))
			return;

		va_list vaList;
		va_start(vaList, szFormat);

		// _vscprintf doesn't count // terminating '\0'
		int len = _vscprintf(szFormat, vaList) + 1;

		auto out = std::make_unique<char[]>(len);
		char* szOutput = out.get();

		vsprintf_s(szOutput, len, szFormat, vaList);

		va_end(vaList);  // REQUIRED

		WriteChatColor(szOutput);
	}

	// Check if a specific logging flag is enabled
	bool IsEnabled(LogFlag flag) const
	{
		return (m_settings & flag) != 0;
	}

	// Set logging settings (bitmask)
	void SetSettings(int flags) { m_settings = flags; }

	// Get current logging settings
	int GetSettings() const { return m_settings; }

private:
	int m_settings;
};
}
