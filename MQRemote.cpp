#include <mq/Plugin.h>
#include "routing/PostOffice.h"
#include "SubscriptionController.h"

using namespace mq::proto::remote;
using namespace remote;

PreSetup("MQRemote");
PLUGIN_VERSION(0.1);

const std::chrono::milliseconds UPDATE_TICK_MILLISECONDS = std::chrono::milliseconds(1000); 
std::unordered_map<SubscriptionType, remote::SubscriptionController> s_controllers = {};

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

void RccCmd(PlayerClient* pcClient, char* szLine)
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
		WriteChatf("\am[%s]\ax Syntax: /rcc <channel> <message> -- send message to channel", mqplugin::PluginName, USERCOLOR_DEFAULT);
	}
	else
	{
		std::string unescaped_message = unescape_args(message);

		// Try to get a runtime subscription type by name 
		if (auto opt = SubscriptionRegistry::FromName(name); opt)
		{
			if (auto it = s_controllers.find(opt.value()); it != s_controllers.end())
			{
				it->second.SendCommand(unescaped_message, false);
			}
		}

	}
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
		if (auto it = s_controllers.find(SubscriptionRegistry::All); it != s_controllers.end())
		{
			it->second.SendCommand(name, unescaped_message);
		}
	}
}

void RcgCmd(PlayerClient* pcClient, char* szLine)
{
	if (auto it = s_controllers.find(SubscriptionRegistry::Group); it != s_controllers.end())
	{
		it->second.SendCommand(std::string(szLine), false);
	}
}

void RcgaCmd(PlayerClient* pcClient, char* szLine)
{
	if (auto it = s_controllers.find(SubscriptionRegistry::Group); it != s_controllers.end())
	{
		it->second.SendCommand(std::string(szLine), true);
	}
}

void RcrCmd(PlayerClient* pcClient, char* szLine)
{
	if (auto it = s_controllers.find(SubscriptionRegistry::Raid); it != s_controllers.end())
	{
		it->second.SendCommand(std::string(szLine), false);
	}
}

void RcraCmd(PlayerClient* pcClient, char* szLine)
{
	if (auto it = s_controllers.find(SubscriptionRegistry::Raid); it != s_controllers.end())
	{
		it->second.SendCommand(std::string(szLine), true);
	}
}

void RcaCmd(PlayerClient* pcClient, char* szLine)
{
	if (auto it = s_controllers.find(SubscriptionRegistry::All); it != s_controllers.end())
	{
		it->second.SendCommand(std::string(szLine), false);
	}
}

void RcaaCmd(PlayerClient* pcClient, char* szLine)
{
	if (auto it = s_controllers.find(SubscriptionRegistry::All); it != s_controllers.end())
	{
		it->second.SendCommand(std::string(szLine), true);
	}
}

void RczCmd(PlayerClient* pcClient, char* szLine)
{
	if (auto it = s_controllers.find(SubscriptionRegistry::Zone); it != s_controllers.end())
	{
		it->second.SendCommand(std::string(szLine), false);
	}
}

void RczaCmd(PlayerClient* pcClient, char* szLine)
{
	if (auto it = s_controllers.find(SubscriptionRegistry::Zone); it != s_controllers.end())
	{
		it->second.SendCommand(std::string(szLine), true);
	}
}

void RcJoinCmd(PlayerClient* pcClient, char* szLine)
{
	CHAR szName[MAX_STRING] = { 0 };
	GetArg(szName, szLine, 1);
	auto name = init_name(szName);

	if (name.empty())
	{
		WriteChatf("\am[%s]\ax Syntax: /rcjoin <channel> -- join channel", mqplugin::PluginName, USERCOLOR_DEFAULT);
		return;
	}

	SubscriptionType type = SubscriptionRegistry::GetOrAdd(name);
	auto [it, inserted] = s_controllers.try_emplace(type, type);
	if (!inserted)
	{
		WriteChatf("\am[%s]\ax Already joined channel", mqplugin::PluginName);
	}
}

void RcLeaveCmd(PlayerClient* pcClient, char* szLine)
{
	CHAR szName[MAX_STRING] = { 0 };
	GetArg(szName, szLine, 1);
	auto name = init_name(szName);

	if (name.empty())
	{
		WriteChatf("\am[%s]\ax Syntax: /rcleave <channel> -- leave channel", mqplugin::PluginName, USERCOLOR_DEFAULT);
		return;
	}

	// Try to get the subscription type (runtime or built‑in)
	auto subOpt = SubscriptionRegistry::FromName(name);
	if (!subOpt.has_value())
	{
		WriteChatf("\am[%s]\ax Unknown channel", mqplugin::PluginName);
		return;
	}

	const SubscriptionType& type = *subOpt;

	// Try to remove the controller
	auto it = s_controllers.find(type);
	if (it != s_controllers.end())
	{
		s_controllers.erase(it);
		WriteChatf("\am[%s]\ax Left channel: \aw%s\ax", mqplugin::PluginName, type.name.data());
	}
}

const char* GetGroupLeader() {
    if (pLocalPC &&
        pLocalPC->pGroupInfo &&
        pLocalPC->pGroupInfo->pLeader)
    {
        return pLocalPC->pGroupInfo->pLeader->Name.c_str();
    }
    return nullptr;
}

const char* GetRaidLeader() {
    if (pRaid && pRaid->RaidLeaderName[0]) {
        return pRaid->RaidLeaderName;
    }
    return nullptr;
}

void UpdateSubscription(SubscriptionType type) 
{
	if(type != SubscriptionRegistry::Group && type != SubscriptionRegistry::Raid) {
		return;
	}

	char* leaderName;
	if(type == SubscriptionRegistry::Group) {
		leaderName = const_cast<char*>(GetGroupLeader());
	} else {
		leaderName = const_cast<char*>(GetRaidLeader());
	}

	if (leaderName && leaderName[0]) {
		std::string name = leaderName;

		if (auto it = s_controllers.find(type); it != s_controllers.end()) {
			if (!it->second.IsChannelFor(name)) {
				s_controllers.try_emplace(type, type, name);
			}
		}
		else {
			s_controllers.try_emplace(type, type, name);
		}
	}
	else {
		s_controllers.erase(type);
	}
}

// Buffer for new channel input
static char newChannelBuf[128] = "";

inline bool IsBuiltinSubscription(const SubscriptionType& type) { 
	return std::find(SubscriptionRegistry::BuiltinSubscriptionTypes.begin(), SubscriptionRegistry::BuiltinSubscriptionTypes.end(), type) != SubscriptionRegistry::BuiltinSubscriptionTypes.end(); 
}

void DrawSubscriptionsPanel()
{
	// --- Add new channel section ---
	ImGui::Text("Add New Channel");
	ImGui::SameLine();
	ImGui::InputText("##newChannel", newChannelBuf, IM_ARRAYSIZE(newChannelBuf));

	ImGui::SameLine();
	if (ImGui::Button("Add"))
	{
		std::string channelName = newChannelBuf;
		if (!channelName.empty())
		{
			RcJoinCmd(nullptr, newChannelBuf);
			/*SubscriptionType type = SubscriptionRegistry::GetOrAdd(channelName);
			auto [it, inserted] = s_controllers.try_emplace(type, type);
			if (!inserted)
			{
				WriteChatf("\am[%s]\ax Already joined channel", mqplugin::PluginName);
			}*/

			newChannelBuf[0] = '\0'; // Clear input
		}
	}

	// --- List existing subscriptions ---
	if (ImGui::BeginTable("subs_table", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
	{
		// Table headers
		ImGui::TableSetupColumn("Channel");
		ImGui::TableSetupColumn(""); // empty header
		ImGui::TableHeadersRow();

		// Safe iteration with erase
		for (auto it = s_controllers.begin(); it != s_controllers.end(); )
		{
			auto& type = it->first;
			auto& controller = it->second;

			ImGui::PushID(&type);

			ImGui::TableNextRow();

			// Column 0: Channel name
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("%s", type.name.c_str());

			// Column 1: Button (or disabled)
			ImGui::TableSetColumnIndex(1);

			bool isRestricted = IsBuiltinSubscription(type);
			bool erase_this = false;

			if (!isRestricted)
			{
				if (ImGui::Button("Leave channel"))
				{
					erase_this = true;
				}
			}

			ImGui::PopID();

			if (erase_this)
				it = s_controllers.erase(it);
			else
				++it;
		}

		ImGui::EndTable();
	}

}


PLUGIN_API void InitializePlugin()
{
	DebugSpewAlways("\am[%s]\ax Initializing version %f", mqplugin::PluginName, MQ2Version);
	WriteChatf("\am[%s]\ax Initializing version %f", mqplugin::PluginName, MQ2Version);
	s_controllers.try_emplace(SubscriptionRegistry::All, SubscriptionRegistry::All);
	if (GetGameState() == GAMESTATE_INGAME) 
	{
		s_controllers.try_emplace(SubscriptionRegistry::Zone, SubscriptionRegistry::Zone, pZoneInfo->ShortName);
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

	AddCommand("/rcc", RccCmd);
	AddCommand("/rcjoin", RcJoinCmd);
	AddCommand("/rcleave", RcLeaveCmd);

	AddSettingsPanel("plugins/Remote", DrawSubscriptionsPanel);
}

PLUGIN_API void ShutdownPlugin()
{
	DebugSpewAlways("\am[%s]\ax Shutting down", mqplugin::PluginName);
	WriteChatf("\am[%s]\ax Shutting down", mqplugin::PluginName);
	s_controllers.clear();
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

	RemoveCommand("/rcc");
	RemoveCommand("/rcjoin");
	RemoveCommand("/rcleave");

	RemoveSettingsPanel("plugins/Remote");
}

PLUGIN_API void SetGameState(int gameState)
{
	if (gameState == GAMESTATE_CHARSELECT)
	{
		s_controllers.erase(SubscriptionRegistry::Group);
		s_controllers.erase(SubscriptionRegistry::Raid);
		s_controllers.erase(SubscriptionRegistry::Zone);
	}
}

PLUGIN_API void OnPulse() {
	static std::chrono::steady_clock::time_point s_pulse_timer = std::chrono::steady_clock::now();

	if (GetGameState() != GAMESTATE_INGAME) return;

	auto now = std::chrono::steady_clock::now();
	if (now > s_pulse_timer) 
	{
		s_pulse_timer = now + UPDATE_TICK_MILLISECONDS; 
		UpdateSubscription(SubscriptionRegistry::Group);
		UpdateSubscription(SubscriptionRegistry::Raid);
	}
}

PLUGIN_API void OnBeginZone()
{
	s_controllers.erase(SubscriptionRegistry::Zone);
}

PLUGIN_API void OnEndZone()
{
	if (GetGameState() == GAMESTATE_INGAME) 
	{
		s_controllers.try_emplace(SubscriptionRegistry::Zone, SubscriptionRegistry::Zone, pZoneInfo->ShortName);
	}
}

PLUGIN_API void OnUpdateImGui() {
	if (GetGameState() == GAMESTATE_INGAME) 
	{
	}
}