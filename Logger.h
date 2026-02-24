#pragma once

#include "mq/base/Enum.h"
#include "mq/Plugin.h"

#define PLUGIN_MSG "\am[MQRemote]\ax "

namespace remote {

class Logger
{
public:
	// Logging flags exposed to other classes
	enum class LogFlags : int
	{
		LOG_ERROR         = 0x0001,
		LOG_SEND          = 0x0002,
		LOG_RECEIVE       = 0x0004,
		LOG_CONNECTIONS   = 0x0008,

		ALL_FLAGS = LOG_ERROR | LOG_SEND | LOG_RECEIVE | LOG_CONNECTIONS,
		DEFAULT_FLAGS = LOG_ERROR | LOG_CONNECTIONS,
	};

	Logger() = default;

	void Log(LogFlags flag, const char* szFormat, ...) const
	{
		if (!IsEnabled(flag))
			return;

		va_list args;
		va_start(args, szFormat);

		mq::VWriteChatColor(szFormat, args);

		va_end(args);
	}

	// Check if a specific logging flag is enabled
	bool IsEnabled(LogFlags flag) const;

	void SetFlags(LogFlags flags) { m_flags = flags; }
	LogFlags GetFlags() const { return m_flags; }

private:
	LogFlags m_flags{ LogFlags::DEFAULT_FLAGS };
};

constexpr bool has_bitwise_operations(Logger::LogFlags) { return true; }

inline bool Logger::IsEnabled(LogFlags flag) const
{
	return !!(m_flags & flag);
}

} // namespace remote
