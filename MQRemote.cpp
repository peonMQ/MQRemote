#include <mq/Plugin.h>
#include "routing/PostOffice.h"
#include "Channel.h"

using namespace mq::proto::remote;
using namespace remote;

PreSetup("MQRemote");
PLUGIN_VERSION(0.1);

const std::chrono::milliseconds UPDATE_TICK_MILLISECONDS = std::chrono::milliseconds(1000); 
std::optional<remote::Channel> s_global_channel;
std::optional<remote::Channel> s_server_channel;
std::optional<remote::Channel> s_group_channel;
std::optional<remote::Channel> s_raid_channel;
std::optional<remote::Channel> s_zone_channel;
std::unordered_map<std::string, remote::Channel> s_custom_channels = {};

constexpr std::string_view GLOBAL_HELP = "/rca <message>\n/rcaa <message>";
constexpr std::string_view SERVER_HELP = "/rcs <message>\n/rcsa <message>\n/rct <name> <message>";
constexpr std::string_view GROUP_HELP = "/rcg <message>\n/rcga <message>";
constexpr std::string_view RAID_HELP = "/rcr <message>\n/rcra <message>";
constexpr std::string_view ZONE_HELP = "/rcz <message>\n/rcza <message>";


void RccCmd(PlayerClient* pcClient, char* szLine)
{
	if (GetGameState() == GAMESTATE_INGAME)
	{
		CHAR szName[MAX_STRING] = { 0 };
		GetArg(szName, szLine, 1);
		std::string name(szName);
		to_lower(name);
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
			if (auto it = s_custom_channels.find(name); it != s_custom_channels.end()) 
			{
				std::string unescaped_message = unescape_args(message);
				it->second.SendCommand(unescaped_message, false);
			}
		}
	} else 
	{
		WriteChatf("\am[%s]\ax Can only join custom channels when you are ingame", mqplugin::PluginName, USERCOLOR_DEFAULT);
	}
}

void RctCmd(PlayerClient* pcClient, char* szLine)
{
	CHAR szName[MAX_STRING] = { 0 };
	GetArg(szName, szLine, 1);
	std::string name(szName);
	to_lower(name);
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
		if (s_server_channel)
		{
			std::string unescaped_message = unescape_args(message);
			s_server_channel->SendCommand(name, unescaped_message);
		}
	}
}

void RcgCmd(PlayerClient* pcClient, char* szLine)
{
	if (s_group_channel)
	{
		s_group_channel->SendCommand(std::string(szLine), false);
	}
}

void RcgaCmd(PlayerClient* pcClient, char* szLine)
{
	if (s_group_channel)
	{
		s_group_channel->SendCommand(std::string(szLine), true);
	}
}

void RcrCmd(PlayerClient* pcClient, char* szLine)
{
	if (s_raid_channel)
	{
		s_raid_channel->SendCommand(std::string(szLine), false);
	}
}

void RcraCmd(PlayerClient* pcClient, char* szLine)
{
	if (s_raid_channel)
	{
		s_raid_channel->SendCommand(std::string(szLine), true);
	}
}

void RcaCmd(PlayerClient* pcClient, char* szLine)
{
	if (s_global_channel) 
	{
		s_global_channel->SendCommand(std::string(szLine), false);
	}
}

void RcaaCmd(PlayerClient* pcClient, char* szLine)
{
	if (s_global_channel)
	{
		s_global_channel->SendCommand(std::string(szLine), true);
	}
}

void RczCmd(PlayerClient* pcClient, char* szLine)
{
	if (s_zone_channel)
	{
		s_zone_channel->SendCommand(std::string(szLine), false);
	}
}

void RczaCmd(PlayerClient* pcClient, char* szLine)
{
	if (s_zone_channel)
	{
		s_zone_channel->SendCommand(std::string(szLine), true);
	}
}

void RcsCmd(PlayerClient* pcClient, char* szLine)
{
	if (s_server_channel)
	{
		s_server_channel->SendCommand(std::string(szLine), false);
	}
}

void RcsaCmd(PlayerClient* pcClient, char* szLine)
{
	if (s_server_channel)
	{
		s_server_channel->SendCommand(std::string(szLine), true);
	}
}

void RcJoinCmd(PlayerClient* pcClient, char* szLine)
{
	CHAR szName[MAX_STRING] = { 0 };
	CHAR szAuto[MAX_STRING] = { 0 };

	GetArg(szName, szLine, 1);
	GetArg(szAuto, szLine, 2); // optional

	bool auto_join = true; // default
	if (szAuto[0])
	{
		std::string autoArg(szAuto);
		to_lower(autoArg);

		if (autoArg == "auto")
		{
			auto_join = true;
		}
		else if (autoArg == "noauto")
		{
			auto_join = false;
		}
	}

	std::string name(szName);
	to_lower(name);

	if (name.empty())
	{
		WriteChatf("\am[%s]\ax Syntax: /rcjoin <channel>  [auto|noauto] -- join channel", mqplugin::PluginName, USERCOLOR_DEFAULT);
		return;
	}

	auto [it, inserted] = s_custom_channels.try_emplace(name, "custom", name);
	if (!inserted)
	{
		WriteChatf("\am[%s]\ax Already joined channel %s", mqplugin::PluginName, name);
	}
	else if(auto_join) 
	{
		sprintf_s(INIFileName, "%s\\%s_%s.ini", gPathConfig, mqplugin::PluginName, GetServerShortName());
		WritePrivateProfileBool(pLocalPC->Name, name, true, INIFileName);
		WriteChatf("\am[%s]\ax Enable autojoin for: \aw%s\ax", mqplugin::PluginName, name.c_str());
	}
}

void RcLeaveCmd(PlayerClient* pcClient, char* szLine)
{
	CHAR szName[MAX_STRING] = { 0 };
	CHAR szAuto[MAX_STRING] = { 0 };

	GetArg(szName, szLine, 1);
	GetArg(szAuto, szLine, 2); // optional

	bool auto_join = true; // default
	if (szAuto[0])
	{
		std::string autoArg(szAuto);
		to_lower(autoArg);

		if (autoArg == "auto")
		{
			auto_join = true;
		}
		else if (autoArg == "noauto")
		{
			auto_join = false;
		}
	}

	std::string name(szName);
	to_lower(name);

	if (name.empty())
	{
		WriteChatf("\am[%s]\ax Syntax: /rcleave <channel>  [auto|noauto] -- leave channel", mqplugin::PluginName, USERCOLOR_DEFAULT);
		return;
	}

	if (auto it = s_custom_channels.find(name); it != s_custom_channels.end()) {
		auto dns_name = it->second.DnsName();  // store value before erasing
		s_custom_channels.erase(it);
		WriteChatf("\am[%s]\ax Left channel: \aw%s\ax", mqplugin::PluginName, dns_name.c_str());
	}
	
	if (!auto_join)
	{
		sprintf_s(INIFileName, "%s\\%s_%s.ini", gPathConfig, mqplugin::PluginName, GetServerShortName());
		if (PrivateProfileKeyExists(pLocalPC->Name, name, INIFileName))
		{
			WritePrivateProfileBool(pLocalPC->Name, name, false, INIFileName);
			WriteChatf("\am[%s]\ax Disable autojoin for: \aw%s\ax", mqplugin::PluginName, name.c_str());
		}
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

void UpdateGroupChannel()
{
	char* leaderName = const_cast<char*>(GetGroupLeader());
	if (leaderName && leaderName[0]) {
		std::string leader_lower = leaderName;
		to_lower(leader_lower);
		if (!s_group_channel || s_group_channel->SubName() != leader_lower) {
			s_group_channel.emplace("group", leader_lower);
		}
	}
	else if (s_group_channel)
	{
		s_group_channel.reset();
	}
}

void UpdateRaidChannel()
{
	char* leaderName = const_cast<char*>(GetRaidLeader());
	if (leaderName && leaderName[0]) {
		std::string leader_lower = leaderName;
		to_lower(leader_lower);
		if (!s_raid_channel || s_raid_channel->SubName() != leader_lower) {
			s_raid_channel.emplace("raid", leader_lower);
		}
	}
	else if (s_raid_channel)
	{
		s_raid_channel.reset();
	}
}

bool DrawCustomChannelRow(remote::Channel& controller, std::string_view helpText, bool canLeave = false)
{
	bool erase_this = false;

	ImGui::PushID(controller.DnsName().c_str());

	ImGui::TableNextRow();

	// Column 0: Channel name
	ImGui::TableSetColumnIndex(0);
	ImGui::Text("%s", controller.DnsName().c_str());

	ImGui::TableSetColumnIndex(1);
	ImGui::Text("%s", helpText.data());

	if (canLeave) {
		// Column 1: Button
		ImGui::TableSetColumnIndex(2);
		if (ImGui::Button("Leave channel"))
		{
			erase_this = true;
		}
	}

	ImGui::PopID();

	return erase_this;
}


// Buffer for new channel input
static char newChannelBuf[128] = "";

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
			newChannelBuf[0] = '\0'; // Clear input
		}
	}

	// --- List existing subscriptions ---
	if (ImGui::BeginTable("subs_table", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
	{
		// Table headers
		ImGui::TableSetupColumn("Channel");
		ImGui::TableSetupColumn("Usage");
		ImGui::TableSetupColumn(""); // empty header
		ImGui::TableHeadersRow();

		if (s_global_channel) {
			DrawCustomChannelRow(*s_global_channel, "/rca <message>\n/rcaa <message>");
		}
		if (s_server_channel) {
			DrawCustomChannelRow(*s_server_channel, "/rcs <message>\n/rcsa <message>\n/rct <name> <message>");
		}
		if (s_group_channel) {
			DrawCustomChannelRow(*s_group_channel, "/rcg <message>\n/rcga <message>");
		}
		if (s_raid_channel) {
			DrawCustomChannelRow(*s_raid_channel, "/rcr <message>\n/rcra <message>");
		}
		if (s_zone_channel) {
			DrawCustomChannelRow(*s_zone_channel, "/rcz <message>\n/rcza <message>");
		}

		// Safe iteration with erase
		for (auto it = s_custom_channels.begin(); it != s_custom_channels.end(); )
		{
			std::string help = fmt::format("/rcc {} <message>", it->first);
			if (DrawCustomChannelRow(it->second, help, true))
				it = s_custom_channels.erase(it);
			else
				++it;
		}


		ImGui::EndTable();
	}
}

void LoadAndJoinPersistentChannels()
{
	sprintf_s(INIFileName, "%s\\%s_%s.ini", gPathConfig, mqplugin::PluginName, GetServerShortName());
	auto channels = GetPrivateProfileKeys(pLocalPC->Name, INIFileName);
	for (const auto& channel : channels) {
		if(GetPrivateProfileValue(pLocalPC->Name, channel.c_str(), false, INIFileName)) {
			s_custom_channels.try_emplace(channel, "custom", channel);
		}
	}
}

PLUGIN_API void InitializePlugin()
{
	DebugSpewAlways("\am[%s]\ax Initializing version %f", mqplugin::PluginName, MQ2Version);
	WriteChatf("\am[%s]\ax Initializing version %f", mqplugin::PluginName, MQ2Version);
	s_global_channel.emplace("");
	if (GetGameState() == GAMESTATE_INGAME) 
	{
		std::string server = GetServerShortName();
		to_lower(server);
		s_server_channel.emplace("server", server);
		s_zone_channel.emplace("zone", pZoneInfo->ShortName);
		LoadAndJoinPersistentChannels();
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
	AddCommand("/rcs", RcsCmd);
	AddCommand("/rcsa", RcsaCmd);

	AddCommand("/rcc", RccCmd);
	AddCommand("/rcjoin", RcJoinCmd);
	AddCommand("/rcleave", RcLeaveCmd);

	AddSettingsPanel("plugins/Remote", DrawSubscriptionsPanel);
}

PLUGIN_API void ShutdownPlugin()
{
	DebugSpewAlways("\am[%s]\ax Shutting down", mqplugin::PluginName);
	WriteChatf("\am[%s]\ax Shutting down", mqplugin::PluginName);

	s_global_channel.reset();
	s_server_channel.reset();
	s_group_channel.reset();
	s_raid_channel.reset();
	s_zone_channel.reset();

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
	RemoveCommand("/rcs");
	RemoveCommand("/rcsa");

	RemoveCommand("/rcc");
	RemoveCommand("/rcjoin");
	RemoveCommand("/rcleave");

	RemoveSettingsPanel("plugins/Remote");
}

PLUGIN_API void SetGameState(int gameState)
{
	if (gameState < GAMESTATE_INGAME)
	{
		s_server_channel.reset();
		s_group_channel.reset();
		s_raid_channel.reset();
		s_zone_channel.reset();
		s_custom_channels.clear();
	}

	if (gameState > GAMESTATE_PRECHARSELECT) {
		if (!s_server_channel) {
			std::string server = GetServerShortName();
			to_lower(server);
			s_server_channel.emplace("server", server);
		}
	}

	if (gameState == GAMESTATE_INGAME)
	{
		LoadAndJoinPersistentChannels();
	}
}

PLUGIN_API void OnPulse() {
	static std::chrono::steady_clock::time_point s_pulse_timer = std::chrono::steady_clock::now();

	if (GetGameState() != GAMESTATE_INGAME) return;

	auto now = std::chrono::steady_clock::now();
	if (now > s_pulse_timer) 
	{
		s_pulse_timer = now + UPDATE_TICK_MILLISECONDS; 
		UpdateGroupChannel();
		UpdateRaidChannel();
	}
}

PLUGIN_API void OnBeginZone()
{
	s_zone_channel.reset();
}

PLUGIN_API void OnEndZone()
{
	if (GetGameState() == GAMESTATE_INGAME) 
	{
		s_zone_channel.emplace("zone", pZoneInfo->ShortName);
	}
}
