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

constexpr std::string_view GLOBAL_HELP = "/rc [+self] global <message>";
constexpr std::string_view SERVER_HELP = "/rc [+self] server <message>\n/rc <character> <message>";
constexpr std::string_view GROUP_HELP = "/rc [+self] group <message>";
constexpr std::string_view RAID_HELP = "/rc [+self] raid <message>";
constexpr std::string_view ZONE_HELP = "/rc [+self] zone <message>";

struct RemoteCommandArgs
{
	bool includeSelf = false;
	std::string channel;
	std::string message;
};

std::optional<RemoteCommandArgs> GetRemoteCommandArgs(const char* szLine)
{
	if (!szLine || !*szLine)
	{
		return std::nullopt;
	}

	RemoteCommandArgs result{};
	std::string_view line(szLine);
	auto args = tokenize_args(line);
	size_t i = 0;
	// Optional +self
	if (i < args.size() && args[i] == "+self")
	{
		result.includeSelf = true;
		++i;
	}

	if (i >= args.size()) 
	{
		std::nullopt;
	}

	result.channel.assign(args[i].data(), args[i].size());
	to_lower(result.channel);
	++i;

	if (i >= args.size()) 
	{
		return std::nullopt;
	}

	// Message = remainder of original line starting at first message token
	const char* msgStart = args[i].data();
	const char* lineEnd = line.data() + line.size();
	result.message.assign(msgStart, lineEnd - msgStart);

	return result;
}

remote::Channel* FindChannel(const std::string_view name)
{
	// Built-in channels
	if (s_global_channel && s_global_channel->GetName() == name)
	{
		return &*s_global_channel;
	}

	if (s_server_channel && s_server_channel->GetName() == name)
	{
		return &*s_server_channel;
	}

	if (s_group_channel && s_group_channel->GetName() == name)
	{
		return &*s_group_channel;
	}

	if (s_raid_channel && s_raid_channel->GetName() == name)
	{
		return &*s_raid_channel;
	}

	if (s_zone_channel && s_zone_channel->GetName() == name)
	{
		return &*s_zone_channel;
	}

	// Custom channels (requires std::string key)
	auto it = s_custom_channels.find(std::string(name));
	if (it != s_custom_channels.end())
	{
		return &it->second;
	}

	return nullptr;
}

void RcCmd(const PlayerClient* pcClient, const char* szLine)
{
	auto commandArgs = GetRemoteCommandArgs(szLine);
	if (!commandArgs)
	{
		WriteChatf("\am[%s]\ax Syntax: /rc [+self] <channel> <message>", mqplugin::PluginName, USERCOLOR_DEFAULT);
		return;
	}

	if (remote::Channel* channel = FindChannel(commandArgs->channel))
	{
		std::string unescaped_message = unescape_args(commandArgs->message);
		channel->SendCommand(std::move(unescaped_message), commandArgs->includeSelf);
	}
	else if (s_server_channel)
	{
		std::string unescaped_message = unescape_args(commandArgs->message);
		s_server_channel->SendCommand(commandArgs->channel, std::move(unescaped_message));
	}
}

void RcJoinCmd(const PlayerClient* pcClient, const char* szLine)
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

void RcLeaveCmd(const PlayerClient* pcClient, const char* szLine)
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
		auto dns_name = std::string(it->second.GetDnsName());  // store value before erasing
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

const char* GetGroupLeaderName()
{
	if (pLocalPC
		&& pLocalPC->pGroupInfo
		&& pLocalPC->pGroupInfo->pLeader)
	{
		return pLocalPC->pGroupInfo->pLeader->Name.c_str();
	}
	return nullptr;
}

const char* GetRaidLeaderName()
{
	if (pRaid && pRaid->RaidLeaderName[0])
	{
		return pRaid->RaidLeaderName;
	}
	return nullptr;
}

void UpdateGroupChannel()
{
	const char* leaderName = GetGroupLeaderName();

	if (leaderName && leaderName[0]) 
	{
		if (!s_group_channel || !ci_equals(s_group_channel->GetSubName(), leaderName))
		{
			s_group_channel.emplace("group", mq::to_lower_copy(leaderName));
		}
	}
	else if (s_group_channel)
	{
		s_group_channel.reset();
	}
}

void UpdateRaidChannel()
{
	const char* leaderName = GetRaidLeaderName();

	if (leaderName && leaderName[0]) 
	{
		std::string leader_lower = leaderName;
		to_lower(leader_lower);
		if (!s_raid_channel || !ci_equals(s_raid_channel->GetSubName(), leaderName))
		{
			s_raid_channel.emplace("raid", mq::to_lower_copy(leaderName));
		}
	}
	else if (s_raid_channel)
	{
		s_raid_channel.reset();
	}
}

bool DrawCustomChannelRow(const remote::Channel& controller, const std::string_view helpText, const bool canLeave = false)
{
	bool erase_this = false;
	std::string_view dnsName = controller.GetDnsName();

	ImGui::PushID(dnsName.data(), dnsName.data() + dnsName.size());

	ImGui::TableNextRow();

	// Column 0: Channel name
	ImGui::TableSetColumnIndex(0);
	ImGui::Text("%.*s", (int)dnsName.size(), dnsName.data());

	// Column 1: Command help
	ImGui::TableSetColumnIndex(1);
	ImGui::Text("%.*s", (int)helpText.size(), helpText.data());

	if (canLeave) 
	{
		// Column 2: Action button
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
	if (ImGui::BeginTable("channels_table", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
	{
		// Table headers
		ImGui::TableSetupColumn("Channel");
		ImGui::TableSetupColumn("Usage");
		ImGui::TableSetupColumn(""); // empty header
		ImGui::TableHeadersRow();

		if (s_global_channel) 
		{
			DrawCustomChannelRow(*s_global_channel, GLOBAL_HELP);
		}
		if (s_server_channel) 
		{
			DrawCustomChannelRow(*s_server_channel, SERVER_HELP);
		}
		if (s_group_channel) 
		{
			DrawCustomChannelRow(*s_group_channel, GROUP_HELP);
		}
		if (s_raid_channel) 
		{
			DrawCustomChannelRow(*s_raid_channel, RAID_HELP);
		}
		if (s_zone_channel) 
		{
			DrawCustomChannelRow(*s_zone_channel, ZONE_HELP);
		}

		// Safe iteration with erase
		for (auto it = s_custom_channels.begin(); it != s_custom_channels.end(); )
		{
			std::string help = fmt::format("/rc [+self] {} <message>", it->first);
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
	for (const auto& channel : channels) 
	{
		if(GetPrivateProfileValue(pLocalPC->Name, channel.c_str(), false, INIFileName)) 
		{
			s_custom_channels.try_emplace(channel, "custom", channel);
		}
	}
}

PLUGIN_API void InitializePlugin()
{
	DebugSpewAlways("\am[%s]\ax Initializing version %f", mqplugin::PluginName, MQ2Version);
	WriteChatf("\am[%s]\ax Initializing version %f", mqplugin::PluginName, MQ2Version);
	s_global_channel.emplace("global");
	if (GetGameState() == GAMESTATE_INGAME) 
	{
		std::string server = GetServerShortName();
		to_lower(server);
		s_server_channel.emplace("server", server);
		s_zone_channel.emplace("zone", pZoneInfo->ShortName);
		LoadAndJoinPersistentChannels();
	}

	AddCommand("/rc", RcCmd);
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

	RemoveCommand("/rc");
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

	if (GetGameState() == GAMESTATE_INGAME) 
	{
		auto now = std::chrono::steady_clock::now();
		if (now > s_pulse_timer)
		{
			s_pulse_timer = now + UPDATE_TICK_MILLISECONDS;
			UpdateGroupChannel();
			UpdateRaidChannel();
		}
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
