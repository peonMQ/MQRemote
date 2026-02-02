
#include "ChannelManager.h"

#include "routing/PostOffice.h"
#include "mq/Plugin.h"

PreSetup("MQRemote");
PLUGIN_VERSION(0.1);

constexpr std::chrono::milliseconds UPDATE_TICK_MILLISECONDS{ 1000 };

constexpr std::string_view GLOBAL_HELP = "/rc [+self] global <message>";
constexpr std::string_view SERVER_HELP = "/rc [+self] server <message>\n/rc <character> <message>";
constexpr std::string_view GROUP_HELP = "/rc [+self] group <message>";
constexpr std::string_view RAID_HELP = "/rc [+self] raid <message>";
constexpr std::string_view ZONE_HELP = "/rc [+self] zone <message>";

static remote::ChannelManager* gChannels = new remote::ChannelManager();

struct RemoteCommandArgs
{
	bool includeSelf = false;
	std::string channel;
	std::string message;
};

static std::optional<RemoteCommandArgs> GetRemoteCommandArgs(const char* szLine)
{
	if (!szLine || !*szLine)
	{
		return std::nullopt;
	}

	RemoteCommandArgs result{};
	std::string_view line(szLine);
	std::vector<std::string_view> args = tokenize_args(line);
	size_t i = 0;

	// Optional +self
	if (i < args.size() && args[i] == "+self")
	{
		result.includeSelf = true;
		++i;
	}

	if (i + 1 >= args.size()) 
	{
		return std::nullopt;
	}

	result.channel = mq::to_lower_copy(args[i]);
	++i;

	// Message = remainder of original line starting at first message token
	result.message = line.substr(args[i].data() - line.data());

	return result;
}

static void RcCmd(const PlayerClient*, const char* szLine)
{
	auto commandArgs = GetRemoteCommandArgs(szLine);
	if (!commandArgs)
	{
		WriteChatf("\am[%s]\ax Syntax: /rc [+self] <channel> <message>", mqplugin::PluginName);
		return;
	}

	if (remote::Channel* channel = gChannels->FindChannel(commandArgs->channel))
	{
		std::string unescaped_message = unescape_args(commandArgs->message);
		channel->SendCommand(std::move(unescaped_message), commandArgs->includeSelf);
	}
	else if (gChannels->GetServerChannel())
	{
		std::string unescaped_message = unescape_args(commandArgs->message);
		gChannels->GetServerChannel()->SendCommand(commandArgs->channel, std::move(unescaped_message));
	}
}

static void RcJoinCmd(const PlayerClient*, const char* szLine)
{
	char szName[MAX_STRING] = { 0 };
	char szAuto[MAX_STRING] = { 0 };

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

	std::string name = mq::to_lower_copy(szName);
	if (name.empty())
	{
		WriteChatf("\am[%s]\ax Syntax: /rcjoin <channel>  [auto|noauto] -- join channel", mqplugin::PluginName);
		return;
	}

	auto [_, inserted] = gChannels->GetCustomChannels().try_emplace(name, "custom", name);
	if (inserted)
	{
		WriteChatf("\am[%s]\ax Already joined channel %s", mqplugin::PluginName, name.c_str());
	}
	else if (auto_join) 
	{
		sprintf_s(INIFileName, "%s\\%s_%s.ini", gPathConfig, mqplugin::PluginName, GetServerShortName());
		WritePrivateProfileBool(pLocalPC->Name, name, true, INIFileName);
		WriteChatf("\am[%s]\ax Enable autojoin for: \aw%s\ax", mqplugin::PluginName, name.c_str());
	}
}

static void RcLeaveCmd(const PlayerClient*, const char* szLine)
{
	char szName[MAX_STRING] = { 0 };
	char szAuto[MAX_STRING] = { 0 };

	GetArg(szName, szLine, 1);
	GetArg(szAuto, szLine, 2); // optional

	bool auto_join = true; // default
	if (szAuto[0])
	{
		if (ci_equals(szAuto, "auto"))
		{
			auto_join = true;
		}
		else if (ci_equals(szAuto, "noauto"))
		{
			auto_join = false;
		}
	}

	std::string name = mq::to_lower_copy(szName);
	if (name.empty())
	{
		WriteChatf("\am[%s]\ax Syntax: /rcleave <channel>  [auto|noauto] -- leave channel", mqplugin::PluginName);
		return;
	}

	if (auto it = gChannels->GetCustomChannels().find(name); it != gChannels->GetCustomChannels().end())
	{
		auto dns_name = std::string(it->second.GetDnsName());  // store value before erasing
		gChannels->GetCustomChannels().erase(it);

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

static bool DrawCustomChannelRow(const remote::Channel& controller, const std::string_view& helpText, const bool canLeave = false)
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

static void DrawSubscriptionsPanel()
{
	static char newChannelBuf[128] = "";

	// --- Add new channel section ---
	ImGui::Text("Add New Channel");
	ImGui::SameLine();
	ImGui::InputText("##newChannel", newChannelBuf, IM_ARRAYSIZE(newChannelBuf));

	ImGui::SameLine();
	if (ImGui::Button("Add"))
	{
		if (newChannelBuf[0])
		{
			RcJoinCmd(nullptr, newChannelBuf);
			newChannelBuf[0] = '\0';
		}
	}

	// --- List existing subscriptions ---
	if (ImGui::BeginTable("channels_table", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
	{
		// Table headers
		ImGui::TableSetupColumn("Channel");
		ImGui::TableSetupColumn("Usage");
		ImGui::TableSetupColumn("");
		ImGui::TableHeadersRow();

		if (gChannels->GetGlobalChannel()) 
		{
			DrawCustomChannelRow(*gChannels->GetGlobalChannel(), GLOBAL_HELP);
		}
		if (gChannels->GetServerChannel())
		{
			DrawCustomChannelRow(*gChannels->GetServerChannel(), SERVER_HELP);
		}
		if (gChannels->GetGroupChannel())
		{
			DrawCustomChannelRow(*gChannels->GetGroupChannel(), GROUP_HELP);
		}
		if (gChannels->GetRaidChannel())
		{
			DrawCustomChannelRow(*gChannels->GetRaidChannel(), RAID_HELP);
		}
		if (gChannels->GetZoneChannel())
		{
			DrawCustomChannelRow(*gChannels->GetZoneChannel(), ZONE_HELP);
		}

		// Safe iteration with erase
		for (auto it = gChannels->GetCustomChannels().begin(); it != gChannels->GetCustomChannels().end(); )
		{
			std::string help = fmt::format("/rc [+self] {} <message>", it->first);

			if (DrawCustomChannelRow(it->second, help, true))
				it = gChannels->GetCustomChannels().erase(it);
			else
				++it;
		}

		ImGui::EndTable();
	}
}

PLUGIN_API void InitializePlugin()
{
	WriteChatf("\am[%s]\ax Initializing version %f", mqplugin::PluginName, MQ2Version);
	
	gChannels->Initialize();

	AddCommand("/rc", RcCmd);
	AddCommand("/rcjoin", RcJoinCmd);
	AddCommand("/rcleave", RcLeaveCmd);

	AddSettingsPanel("plugins/Remote", DrawSubscriptionsPanel);
}

PLUGIN_API void ShutdownPlugin()
{
	WriteChatf("\am[%s]\ax Shutting down", mqplugin::PluginName);

	gChannels->Shutdown();

	RemoveMQ2Data("Remote");

	RemoveCommand("/rc");
	RemoveCommand("/rcjoin");
	RemoveCommand("/rcleave");

	RemoveSettingsPanel("plugins/Remote");
}

PLUGIN_API void SetGameState(int gameState)
{
	gChannels->SetGameState(gameState);
}

PLUGIN_API void OnPulse()
{
	gChannels->OnPulse();
}

PLUGIN_API void OnBeginZone()
{
	gChannels->OnBeginZone();
}

PLUGIN_API void OnEndZone()
{
	gChannels->OnEndZone();
}
