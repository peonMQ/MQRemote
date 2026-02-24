
#include "ChannelManager.h"
#include "Logger.h"

#include "routing/PostOffice.h"
#include "mq/Plugin.h"

PreSetup("MQRemote");
PLUGIN_VERSION(0.1);

namespace remote {

constexpr std::chrono::milliseconds UPDATE_TICK_MILLISECONDS{ 1000 };

constexpr std::string_view GLOBAL_HELP = "/rc [+self] global <message>\n/rc global <character> <message>";
constexpr std::string_view SERVER_HELP = "/rc [+self] server <message>\n/rc server <character> <message>\n/rc <character> <message>";
constexpr std::string_view GROUP_HELP = "/rc [+self] group <message>\n/rc group <character> <message>";
constexpr std::string_view RAID_HELP = "/rc [+self] raid <message>\n/rc raid <character> <message>";
constexpr std::string_view ZONE_HELP = "/rc [+self] zone <message>\n/rc zone <character> <message>";

static ChannelManager* gChannels = nullptr;
static Logger* gLogger = nullptr;

struct RemoteCommandArgs
{
	bool includeSelf = false;
	std::string channel;
	std::optional<std::string> receiver;
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
	std::optional<RemoteCommandArgs> commandArgs = GetRemoteCommandArgs(szLine);
	if (!commandArgs)
	{
		WriteChatf(PLUGIN_MSG "Syntax: /rc [+self] <channel> [character] <message>");
		return;
	}

	std::string unescaped = unescape_args(commandArgs->message);
	Channel* channel = gChannels->FindChannel(commandArgs->channel);
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
	char szName[MAX_STRING] = {};
	char szAuto[MAX_STRING] = {};

	GetArg(szName, szLine, 1);
	GetArg(szAuto, szLine, 2); // optional

	gChannels->JoinCustomChannel(szName, szAuto);
}

static void RcLeaveCmd(const PlayerClient*, const char* szLine)
{
	char szName[MAX_STRING] = {};
	char szAuto[MAX_STRING] = {};

	GetArg(szName, szLine, 1);
	GetArg(szAuto, szLine, 2); // optional

	gChannels->LeaveCustomChannel(szName, szAuto);
}

static bool DrawCustomChannelRow(const Channel& channel, const std::string_view& helpText, const bool canLeave = false)
{
	bool erase_this = false;
	std::string_view dnsName = channel.GetDnsName();

	ImGui::PushID(&channel);

	ImGui::TableNextRow();

	// Column 0: Channel name
	ImGui::TableNextColumn();
	ImGui::TextUnformatted(dnsName.data(), dnsName.data() + dnsName.size());

	// Column 1: Command help
	ImGui::TableNextColumn();
	ImGui::TextUnformatted(helpText.data(), helpText.data() + helpText.size());

	ImGui::TableNextColumn();
	if (canLeave)
	{
		// Column 2: Action button
		if (ImGui::Button("Leave"))
		{
			erase_this = true;
		}
	}

	ImGui::PopID();

	return erase_this;
}

static void UpdateLogFlags(Logger::LogFlags flags)
{
	gLogger->SetFlags(flags);
	WritePrivateProfileInt("MQRemote", "LoggingFlags", +flags, INIFileName);
}

static void DrawSubscriptionsPanel()
{
	static char newChannelBuf[128] = "";

	using enum Logger::LogFlags;
	int flags = +gLogger->GetFlags();

	if (ImGui::CheckboxFlags("Console Logging Enabled", &flags, +ALL_FLAGS))
	{
		UpdateLogFlags(static_cast<Logger::LogFlags>(flags));
	}

	// Children
	ImGui::Indent();
	if (ImGui::CheckboxFlags("Errors", &flags, +LOG_ERROR))
	{
		UpdateLogFlags(static_cast<Logger::LogFlags>(flags));
	}

	ImGui::SameLine();
	if (ImGui::CheckboxFlags("Connection Messages", &flags, +LOG_CONNECTIONS))
	{
		UpdateLogFlags(static_cast<Logger::LogFlags>(flags));
	}

	if (ImGui::CheckboxFlags("Sent Messages", &flags, +LOG_SEND))
	{
		UpdateLogFlags(static_cast<Logger::LogFlags>(flags));
	}
	
	ImGui::SameLine();
	if (ImGui::CheckboxFlags("Received Messages", &flags, +LOG_RECEIVE))
	{
		UpdateLogFlags(static_cast<Logger::LogFlags>(flags));
	}

	ImGui::Unindent();

	ImGui::Separator();

	// --- Add new channel section ---
	ImGui::Text("Add New Channel");
	ImGui::SameLine();

	float availWidth = ImGui::GetContentRegionAvail().x;
	ImGui::SetNextItemWidth(availWidth - 42); // TODO calculate frame padding for button
	ImGui::InputText("##newChannel", newChannelBuf, IM_ARRAYSIZE(newChannelBuf));

	ImGui::SameLine();
	if (ImGui::Button("Add"))
	{
		if (newChannelBuf[0])
		{
			gChannels->JoinCustomChannel(newChannelBuf);
			newChannelBuf[0] = '\0';
		}
	}

	// --- List existing subscriptions ---
	if (ImGui::BeginTable("channels_table", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable))
	{
		// Table headers
		ImGui::TableSetupColumn("Channel", ImGuiTableColumnFlags_WidthFixed, 150.0f);
		ImGui::TableSetupColumn("Usage", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn("##Delete", ImGuiTableColumnFlags_WidthFixed, 60.0f);
		ImGui::TableSetupScrollFreeze(0, 1);
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
			std::string_view subName = gChannels->GetClassChannel()->GetSubName();
			std::string class_help = fmt::format("/rc [+self] {} <message>\n/rc {} <character> <message>", subName, subName);
			DrawCustomChannelRow(*gChannels->GetClassChannel(), class_help);
		}

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

} // namespace remote

using namespace remote;

PLUGIN_API void InitializePlugin()
{
	gLogger = new Logger();

	int flags = GetPrivateProfileInt("MQRemote", "LoggingFlags", static_cast<int>(Logger::LogFlags::DEFAULT_FLAGS), INIFileName);
	gLogger->SetFlags(static_cast<Logger::LogFlags>(flags));

	gChannels = new ChannelManager(gLogger);
	gChannels->Initialize();

	AddCommand("/rc", RcCmd);
	AddCommand("/rcjoin", RcJoinCmd);
	AddCommand("/rcleave", RcLeaveCmd);

	AddSettingsPanel("plugins/Remote", DrawSubscriptionsPanel);
}

PLUGIN_API void ShutdownPlugin()
{
	gChannels->Shutdown();
	delete gChannels;
	delete gLogger;

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
