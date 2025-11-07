#include <mq/Plugin.h>
#include "routing/PostOffice.h"
#include "SubscriptionController.h"

using namespace mq::proto::remote;
using namespace remote;

PreSetup("MQRemote");
PLUGIN_VERSION(0.1);


const std::chrono::milliseconds UPDATE_TICK_MILLISECONDS = std::chrono::milliseconds(1000);
std::unordered_map<SubscriptionType, remote::SubscriptionController> controllers = {};

std::string init_name(const char* szStr)
{
	if (!szStr || szStr[0] == '\0')
		return std::string();

	std::string s(szStr);

	// First character uppercase
	s[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));

	// Rest lowercase
	for (size_t i = 1; i < s.size(); ++i)
		s[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(s[i])));

	return s;
}

void RctCmd(PlayerClient* pcClient, char* szLine)
{
	CHAR szName[MAX_STRING] = { 0 };
	GetArg(szName, szLine, 1);
	auto name = init_name(szName);
	std::string message(szLine);
	std::string::size_type n = message.find_first_not_of(" \t", 0);
	n = message.find_first_of(" \t", n);
	message.erase(0, message.find_first_not_of(" \t", n));

	if (name.empty() || message.empty())
	{
		WriteChatf("\am[%s]\ax Syntax: /rct <name> <message> -- send message to name", mqplugin::PluginName, USERCOLOR_DEFAULT);
	}
	else 
	{
		std::string unescaped_message = unescape_args(message);
		if (auto it = controllers.find(SubscriptionType::All); it != controllers.end()) 
		{
			it->second.SendCommand(name, unescaped_message);
		}
	}
}

void RcgCmd(PlayerClient* pcClient, char* szLine)
{
	if (auto it = controllers.find(SubscriptionType::Group); it != controllers.end()) 
	{
		it->second.SendCommand(std::string(szLine), false);
	}
}

void RcgaCmd(PlayerClient* pcClient, char* szLine)
{
	if (auto it = controllers.find(SubscriptionType::Group); it != controllers.end()) 
	{
		it->second.SendCommand(std::string(szLine), true);
	}
}

void RcrCmd(PlayerClient* pcClient, char* szLine)
{
	if (auto it = controllers.find(SubscriptionType::Raid); it != controllers.end()) 
	{
		it->second.SendCommand(std::string(szLine), false);
	}
}

void RcraCmd(PlayerClient* pcClient, char* szLine)
{
	if (auto it = controllers.find(SubscriptionType::Raid); it != controllers.end()) 
	{
		it->second.SendCommand(std::string(szLine), true);
	}
}

void RcaCmd(PlayerClient* pcClient, char* szLine)
{
	if (auto it = controllers.find(SubscriptionType::All); it != controllers.end()) 
	{
		it->second.SendCommand(std::string(szLine), false);
	}
}

void RcaaCmd(PlayerClient* pcClient, char* szLine)
{
	if (auto it = controllers.find(SubscriptionType::All); it != controllers.end()) 
	{
		it->second.SendCommand(std::string(szLine), true);
	}
}

void RczCmd(PlayerClient* pcClient, char* szLine)
{
	if (auto it = controllers.find(SubscriptionType::Zone); it != controllers.end()) 
	{
		it->second.SendCommand(std::string(szLine), false);
	}
}

void RczaCmd(PlayerClient* pcClient, char* szLine)
{
	if (auto it = controllers.find(SubscriptionType::Zone); it != controllers.end()) 
	{
		it->second.SendCommand(std::string(szLine), true);
	}
}

PLUGIN_API void InitializePlugin()
{
	DebugSpewAlways("\am[%s]\ax Initializing version %f", mqplugin::PluginName, MQ2Version);
	WriteChatf("\am[%s]\ax Initializing version %f", mqplugin::PluginName, MQ2Version);
	controllers.try_emplace(SubscriptionType::All, SubscriptionType::All);
	if (GetGameState() == GAMESTATE_INGAME) 
	{
		controllers.try_emplace(SubscriptionType::Zone, SubscriptionType::Zone, pZoneInfo->ShortName);
	}

	AddCommand("/rct", RctCmd);
	AddCommand("/rca", RcaCmd);
	AddCommand("/rcaa", RcaaCmd);
	AddCommand("/rcg", RcgCmd);
	AddCommand("/rcga", RcgaCmd);
	AddCommand("/rcr", RcrCmd);
	AddCommand("/rcra", RcraCmd);
	AddCommand("/rcz", RczCmd);
	AddCommand("/rcza", RczaCmd);
}

PLUGIN_API void ShutdownPlugin()
{
	DebugSpewAlways("\am[%s]\ax Shutting down", mqplugin::PluginName);
	WriteChatf("\am[%s]\ax Shutting down", mqplugin::PluginName);
	controllers.clear();
	RemoveMQ2Data("Remote");

	RemoveCommand("/rct");
	RemoveCommand("/rca");
	RemoveCommand("/rcaa");
	RemoveCommand("/rcg");
	RemoveCommand("/rcga");
	RemoveCommand("/rcr");
	RemoveCommand("/rcra");
	RemoveCommand("/rcz");
	RemoveCommand("/rcza");
}

PLUGIN_API void SetGameState(int gameState)
{
	if (gameState == GAMESTATE_CHARSELECT)
	{
		controllers.clear();
	}
	else if (gameState == GAMESTATE_INGAME) 
	{
		controllers.try_emplace(SubscriptionType::All, SubscriptionType::All, std::nullopt);
	}
}
PLUGIN_API void OnPulse() {
	static std::chrono::steady_clock::time_point s_pulse_timer = std::chrono::steady_clock::now();

	if (GetGameState() != GAMESTATE_INGAME) return;

	auto now = std::chrono::steady_clock::now();
	if (now > s_pulse_timer) 
	{
		s_pulse_timer = now + UPDATE_TICK_MILLISECONDS; 
		if (pLocalPC->pGroupInfo && pLocalPC->pGroupInfo->pLeader) 
		{
			auto name = std::string(pLocalPC->pGroupInfo->pLeader->Name);
			if (auto it = controllers.find(SubscriptionType::Group); it != controllers.end()) 
			{
				if (!it->second.IsChannelFor(name))
				{
					controllers.try_emplace(SubscriptionType::Group, SubscriptionType::Group, name);
				}
			} else 
			{
				controllers.try_emplace(SubscriptionType::Group, SubscriptionType::Group, name);
			}
		}
		else 
		{
			controllers.erase(SubscriptionType::Group);
		}

		if (pRaid && pRaid->RaidLeaderName[0]) 
		{
			auto name = std::string(pRaid->RaidLeaderName);
			if (auto it = controllers.find(SubscriptionType::Raid); it != controllers.end()) 
			{
				if (!it->second.IsChannelFor(name))
				{
					controllers.try_emplace(SubscriptionType::Raid, SubscriptionType::Raid, name);
				}
			}
			else
			{
				controllers.try_emplace(SubscriptionType::Raid, SubscriptionType::Raid, name);
			}
		}
		else 
		{
			controllers.erase(SubscriptionType::Raid);
		}
	}
}

PLUGIN_API void OnBeginZone()
{
	controllers.erase(SubscriptionType::Zone);
}

PLUGIN_API void OnEndZone()
{
	if (GetGameState() == GAMESTATE_INGAME) 
	{
		controllers.try_emplace(SubscriptionType::Zone, SubscriptionType::Zone, pZoneInfo->ShortName);
	}
}

PLUGIN_API void OnUpdateImGui() {
	if (GetGameState() == GAMESTATE_INGAME) 
	{
	}
}