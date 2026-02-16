
#include "ChannelManager.h"
#include "Logger.h"

#include "routing/PostOffice.h"
#include "mq/Plugin.h"

PreSetup("MQRemote");
PLUGIN_VERSION(0.1);

constexpr std::chrono::milliseconds UPDATE_TICK_MILLISECONDS{ 1000 };

constexpr std::string_view GLOBAL_HELP = "/rc [+self] global <message>\n/rc global <character> <message>";
constexpr std::string_view SERVER_HELP = "/rc [+self] server <message>\n/rc server <character> <message>\n/rc <character> <message>";
constexpr std::string_view GROUP_HELP = "/rc [+self] group <message>\n/rc group <character> <message>";
constexpr std::string_view RAID_HELP = "/rc [+self] raid <message>\n/rc raid <character> <message>";
constexpr std::string_view ZONE_HELP = "/rc [+self] zone <message>\n/rc zone <character> <message>";

static remote::ChannelManager* gChannels = nullptr;
static remote::Logger* gLogger = nullptr;

// Current settings stored as bitmask
static int loggingSettings = remote::Logger::LogFlag::LOG_GENERAL | remote::Logger::LogFlag::LOG_RECEIVE; // General and Receive enabled

struct RemoteCommandArgs
{
	bool includeSelf = false;
	std::string channel;
	std::optional<std::string> receiver;   // NEW optional receiver
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

	// Need at least channel + message
	if (i + 1 >= args.size())
	{
		return std::nullopt;
	}

	// Parse channel
	result.channel = mq::to_lower_copy(args[i]);
	++i;

	// Optional receiver (only if next arg does NOT start with '/')
	if (i < args.size() && !args[i].empty() && args[i][0] != '/')
	{
		result.receiver = std::string(args[i]);
		++i;
	}

	// Now args[i] should be the message component (starts with '/')
	if (i >= args.size() || args[i].empty() || args[i][0] != '/')
	{
		// No valid message component
		return std::nullopt;
	}

	// Message = remainder of original line starting at first message token
	result.message = std::string(line.substr(args[i].data() - line.data()));

	return result;
}

static void RcCmd(const PlayerClient*, const char* szLine)
{
	auto commandArgs = GetRemoteCommandArgs(szLine);
	if (!commandArgs)
	{
		WriteChatf("\am[%s]\ax Syntax: /rc [+self] <channel> [character] <message>", mqplugin::PluginName);
		return;
	}

	std::string unescaped = unescape_args(commandArgs->message);
	auto* channel = gChannels->FindChannel(commandArgs->channel);
	if (!channel) // No valid channel available
	{
		channel = gChannels->GetServerChannel();
		channel->SendCommand(std::move(commandArgs->channel), std::move(unescaped));
	}
	else if (commandArgs->receiver)
	{
		channel->SendCommand(std::move(*commandArgs->receiver), std::move(unescaped));
	}
	else 
	{
		channel->SendCommand(std::move(unescaped), commandArgs->includeSelf);
	}
}


static void RcJoinCmd(const PlayerClient*, const char* szLine)
{
	char szName[MAX_STRING] = { 0 };
	char szAuto[MAX_STRING] = { 0 };

	GetArg(szName, szLine, 1);
	GetArg(szAuto, szLine, 2); // optional

	gChannels->JoinCustomChannel(szName, szAuto);
}

static void RcLeaveCmd(const PlayerClient*, const char* szLine)
{
	char szName[MAX_STRING] = { 0 };
	char szAuto[MAX_STRING] = { 0 };

	GetArg(szName, szLine, 1);
	GetArg(szAuto, szLine, 2); // optional

	gChannels->LeaveCustomChannel(szName, szAuto);
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

static void UpdateLogSettings()
{
	gLogger->SetSettings(loggingSettings);
	sprintf_s(INIFileName, "%s\\%s_%s.ini", gPathConfig, mqplugin::PluginName, GetServerShortName());
	WritePrivateProfileInt("MQRemote", "Logging", loggingSettings, INIFileName);
}

static void DrawSubscriptionsPanel()
{
	static char newChannelBuf[128] = "";

	int parentFlag = remote::Logger::LogFlag::LOG_GENERAL | remote::Logger::LogFlag::LOG_SEND | remote::Logger::LogFlag::LOG_RECEIVE;

	// Parent checkbox: tri-state handled automatically by ImGui
	if(ImGui::CheckboxFlags("Enable Logging", &loggingSettings, parentFlag))
	{
		UpdateLogSettings();
	}

	// Children
	ImGui::Indent();
	if (ImGui::CheckboxFlags("General", &loggingSettings, remote::Logger::LogFlag::LOG_GENERAL))
	{
		UpdateLogSettings();
	}
	
	ImGui::SameLine();
	if(ImGui::CheckboxFlags("Send", &loggingSettings, remote::Logger::LogFlag::LOG_SEND))
	{
		UpdateLogSettings();
	}
	
	ImGui::SameLine();
	if(ImGui::CheckboxFlags("Receive", &loggingSettings, remote::Logger::LogFlag::LOG_RECEIVE))
	{
		UpdateLogSettings();
	}

	ImGui::Unindent();

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
		if (gChannels->GetClassChannel())
		{
			auto subName = std::string(gChannels->GetClassChannel()->GetSubName());
			std::string class_help = fmt::format("/rc [+self] {} <message>\n/rc {} <character> <message>", subName, subName);
			DrawCustomChannelRow(*gChannels->GetClassChannel(), class_help);
		}

		// Safe iteration with erase
		for (auto it = gChannels->GetCustomChannels().begin(); it != gChannels->GetCustomChannels().end(); )
		{
			std::string help = fmt::format("/rc [+self] {} <message>\n/rc {} <character> <message>", it->first, it->first);

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

	sprintf_s(INIFileName, "%s\\%s_%s.ini", gPathConfig, mqplugin::PluginName, GetServerShortName());
	loggingSettings = GetPrivateProfileInt("MQRemote", "Logging", loggingSettings, INIFileName);

	gLogger = new remote::Logger();
	gLogger->SetSettings(loggingSettings);

	gChannels = new remote::ChannelManager(gLogger);
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
	delete gChannels;

	delete gLogger;

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
