
#include "ChannelManager.h"
#include "Logger.h"

#include <mq/Plugin.h>

namespace remote {

static constexpr std::chrono::milliseconds GROUP_UPDATE_INTERVAL(1000);

static std::string_view GetClassName()
{
	if (pLocalPlayer)
	{
		return pLocalPlayer->GetClassThreeLetterCode();
	}

	return {};
}

static std::string_view GetGroupLeaderName()
{
	if (pLocalPC
		&& pLocalPC->pGroupInfo
		&& pLocalPC->pGroupInfo->pLeader)
	{
		return pLocalPC->pGroupInfo->pLeader->Name.c_str();
	}
	return {};
}

static std::string_view GetRaidLeaderName()
{
	if (pRaid && pRaid->RaidLeaderName[0])
	{
		return pRaid->RaidLeaderName;
	}

	return {};
}

void ChannelManager::Initialize()
{
	m_global_channel.emplace(m_logger, "global");

	if (GetGameState() == GAMESTATE_INGAME)
	{
		std::string_view server = GetServerShortName();
		if (!server.empty())
		{
			m_server_channel.emplace(m_logger, "server", server);
		}

		std::string_view shortName = pZoneInfo->ShortName;
		if (!shortName.empty())
		{
			m_zone_channel.emplace(m_logger, "zone", shortName);
		}

		std::string_view classname = GetClassName();
		if (classname.size() == 3)
		{
			m_class_channel.emplace(m_logger, "class", classname);
		}

		LoadPersistentChannels();
	}
}

void ChannelManager::Shutdown()
{
	m_global_channel.reset();
	m_server_channel.reset();
	m_group_channel.reset();
	m_raid_channel.reset();
	m_zone_channel.reset();
	m_custom_channels.clear();
}

void ChannelManager::JoinCustomChannel(std::string_view nameArg, std::string_view autoArg)
{
	bool auto_join = true;
	if (!autoArg.empty())
	{
		if (ci_equals(autoArg, "auto"))
		{
			auto_join = true;
		}
		else if (ci_equals(autoArg, "noauto"))
		{
			auto_join = false;
		}
	}

	if (nameArg.empty())
	{
		WriteChatf(PLUGIN_MSG "Syntax: /rcjoin <channel>  [auto|noauto] -- join channel");
		return;
	}

	std::string name = mq::to_lower_copy(nameArg);
	auto [_, inserted] = m_custom_channels.try_emplace(name, m_logger, "custom", name);
	if (!inserted)
	{
		WriteChatf(PLUGIN_MSG "Already joined channel %s", name.c_str());
	}
	
	if (auto_join)
	{
		if (!m_channelINISection.empty())
		{
			WritePrivateProfileBool(m_channelINISection, name, true, INIFileName);
			
			WriteChatf(PLUGIN_MSG "Enable autojoin for: \aw%s\ax", name.c_str());
		}
	}
}

void ChannelManager::LeaveCustomChannel(std::string_view nameArg, std::string_view autoArg)
{
	bool auto_join = true; // default
	if (!autoArg.empty())
	{
		if (ci_equals(autoArg, "auto"))
		{
			auto_join = true;
		}
		else if (ci_equals(autoArg, "noauto"))
		{
			auto_join = false;
		}
	}

	if (nameArg.empty())
	{
		WriteChatf(PLUGIN_MSG "Syntax: /rcleave <channel>  [auto|noauto] -- leave channel");
		return;
	}

	std::string name = mq::to_lower_copy(nameArg);
	if (auto it = m_custom_channels.find(name); it != m_custom_channels.end())
	{
		std::string dns_name = std::string(it->second.GetDnsName());  // store value before erasing
		m_custom_channels.erase(it);

		WriteChatf(PLUGIN_MSG "Left channel: \aw%s\ax", dns_name.c_str());
	}

	if (!auto_join)
	{
		if (!m_channelINISection.empty()) 
		{
			if (PrivateProfileKeyExists(m_channelINISection, name, INIFileName))
			{
				WritePrivateProfileBool(m_channelINISection, name, false, INIFileName);
				
				WriteChatf(PLUGIN_MSG "Disable autojoin for: \aw%s\ax", name.c_str());
			}
		}
	}
}

remote::Channel* ChannelManager::FindChannel(std::string_view name)
{
	if (m_global_channel && m_global_channel->GetName() == name) 
	{
		return &*m_global_channel;
	}
	
	if (m_server_channel && m_server_channel->GetName() == name) 
	{
		return &*m_server_channel;
	}

	if (m_group_channel && m_group_channel->GetName() == name) 
	{
		return &*m_group_channel;
	}

	if (m_raid_channel && m_raid_channel->GetName() == name) 
	{
		return &*m_raid_channel;
	}

	if (m_zone_channel && m_zone_channel->GetName() == name) 
	{
		return &*m_zone_channel;
	}

	if (m_class_channel && m_class_channel->GetSubName() == name)
	{
		return &*m_class_channel;
	}

	auto it = m_custom_channels.find(std::string(name));
	if (it != m_custom_channels.end())
	{
		return &it->second;
	}

	return nullptr;
}

void ChannelManager::LoadPersistentChannels()
{
	if (m_channelINISection.empty())
		return;

	std::vector<std::string> channels = GetPrivateProfileKeys(m_channelINISection, INIFileName);
	for (const std::string& channel : channels)
	{
		if (GetPrivateProfileBool(m_channelINISection, channel.c_str(), false, INIFileName))
		{
			m_custom_channels.try_emplace(channel, m_logger, "custom", channel);
		}
	}
}

void ChannelManager::UpdateGroupChannel()
{
	std::string_view leaderName = GetGroupLeaderName();
	if (!leaderName.empty())
	{
		if (!m_group_channel || !ci_equals(m_group_channel->GetSubName(), leaderName))
		{
			m_group_channel.emplace(m_logger, "group", leaderName);
		}
	}
	else if (m_group_channel) 
	{
		m_group_channel.reset();
	}
}

void ChannelManager::UpdateRaidChannel()
{
	std::string_view leaderName = GetRaidLeaderName();
	if (!leaderName.empty())
	{
		if (!m_raid_channel || !ci_equals(m_raid_channel->GetSubName(), leaderName))
		{
			m_raid_channel.emplace(m_logger, "raid", leaderName);
		}
	}
	else if (m_raid_channel)
	{
		m_raid_channel.reset();
	}
}

void ChannelManager::SetGameState(int gameState)
{
	if (gameState != GAMESTATE_INGAME)
	{
		m_server_channel.reset();
		m_group_channel.reset();
		m_raid_channel.reset();
		m_zone_channel.reset();
		m_class_channel.reset();
		m_custom_channels.clear();
		m_channelINISection.clear();
	}
	else if (gameState > GAMESTATE_PRECHARSELECT)
	{
		if (!m_server_channel)
		{
			std::string_view server = GetServerShortName();
			if (!server.empty())
			{
				m_server_channel.emplace(m_logger, "server", server);
			}
		}

		if (gameState == GAMESTATE_INGAME)
		{
			m_channelINISection = fmt::format("{}.{}", GetServerShortName(), pLocalPlayer->Name);

			if (!m_class_channel)
			{
				std::string_view className = GetClassName();
				if (className.size() == 3)
				{
					m_class_channel.emplace(m_logger, "class", className);
				}
			}

			LoadPersistentChannels();
		}
	}
}

void ChannelManager::OnPulse()
{
	if (GetGameState() == GAMESTATE_INGAME)
	{
		auto now = std::chrono::steady_clock::now();
		if (now > m_nextGroupUpdate)
		{
			m_nextGroupUpdate = now + GROUP_UPDATE_INTERVAL;
			UpdateGroupChannel();
			UpdateRaidChannel();
		}
	}
}

void ChannelManager::OnBeginZone()
{
	m_zone_channel.reset();
}

void ChannelManager::OnEndZone()
{
	if (GetGameState() == GAMESTATE_INGAME)
	{
		std::string_view shortName = pZoneInfo->ShortName;
		if (!shortName.empty())
		{
			m_zone_channel.emplace(m_logger, "zone", shortName);
		}
	}
}

} // namespace remote
