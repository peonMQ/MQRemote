#include "ChannelManager.h"
#include <mq/Plugin.h>

namespace remote
{

static const std::chrono::milliseconds UPDATE_TICK_MILLISECONDS(1000);

static const char* GetClassName()
{
	if (auto pChar2 = GetPcProfile())
	{
		return ClassInfo[pChar2->Class].ShortName;
	}
	return nullptr;
}

static const char* GetGroupLeaderName()
{
	if (pLocalPC
		&& pLocalPC->pGroupInfo
		&& pLocalPC->pGroupInfo->pLeader)
	{
		return pLocalPC->pGroupInfo->pLeader->Name.c_str();
	}
	return nullptr;
}

static const char* GetRaidLeaderName()
{
	if (pRaid && pRaid->RaidLeaderName[0])
	{
		return pRaid->RaidLeaderName;
	}

	return nullptr;
}

void ChannelManager::Initialize()
{
	m_global_channel.emplace(m_logger, "global");

	if (GetGameState() == GAMESTATE_INGAME)
	{
		std::string server = GetServerShortName();
		to_lower(server);
		m_server_channel.emplace(m_logger, "server", server);

		m_zone_channel.emplace(m_logger, "zone", pZoneInfo->ShortName);

		std::string classname = GetClassName();
		to_lower(classname);
		m_class_channel.emplace(m_logger, "class", classname);

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
		if(m_logger)
		{
			m_logger->Log(remote::Logger::LogFlag::LOG_GENERAL, "\am[%s]\ax Syntax: /rcjoin <channel>  [auto|noauto] -- join channel", mqplugin::PluginName);
		}
		return;
	}

	std::string name = mq::to_lower_copy(nameArg);
	auto [_, inserted] = m_custom_channels.try_emplace(name, m_logger, "custom", name);
	if (!inserted)
	{
		if(m_logger)
		{
			m_logger->Log(remote::Logger::LogFlag::LOG_GENERAL, "\am[%s]\ax Already joined channel %s", mqplugin::PluginName, name.c_str());
		}
	}
	
	if (auto_join)
	{
		sprintf_s(INIFileName, "%s\\%s_%s.ini", gPathConfig, mqplugin::PluginName, GetServerShortName());
		WritePrivateProfileBool(pLocalPC->Name, name, true, INIFileName);
		if(m_logger)
		{
			m_logger->Log(remote::Logger::LogFlag::LOG_GENERAL, "\am[%s]\ax Enable autojoin for: \aw%s\ax", mqplugin::PluginName, name.c_str());
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
		if(m_logger)
		{	
			m_logger->Log(remote::Logger::LogFlag::LOG_GENERAL, "\am[%s]\ax Syntax: /rcleave <channel>  [auto|noauto] -- leave channel", mqplugin::PluginName);
		}
		return;
	}

	std::string name = mq::to_lower_copy(nameArg);
	if (auto it = m_custom_channels.find(name); it != m_custom_channels.end())
	{
		auto dns_name = std::string(it->second.GetDnsName());  // store value before erasing
		m_custom_channels.erase(it);

		if(m_logger)
		{
			m_logger->Log(remote::Logger::LogFlag::LOG_GENERAL, "\am[%s]\ax Left channel: \aw%s\ax", mqplugin::PluginName, dns_name.c_str());
		}
	}

	if (!auto_join)
	{
		sprintf_s(INIFileName, "%s\\%s_%s.ini", gPathConfig, mqplugin::PluginName, GetServerShortName());
		if (PrivateProfileKeyExists(pLocalPC->Name, name, INIFileName))
		{
			WritePrivateProfileBool(pLocalPC->Name, name, false, INIFileName);
			if(m_logger)
			{
				m_logger->Log(remote::Logger::LogFlag::LOG_GENERAL, "\am[%s]\ax Disable autojoin for: \aw%s\ax", mqplugin::PluginName, name.c_str());
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
	sprintf_s(INIFileName, "%s\\%s_%s.ini", gPathConfig, mqplugin::PluginName, GetServerShortName());
	auto channels = GetPrivateProfileKeys(pLocalPC->Name, INIFileName);
	for (const auto& channel : channels)
	{
		if (GetPrivateProfileValue(pLocalPC->Name, channel.c_str(), false, INIFileName))
		{
			m_custom_channels.try_emplace(channel, m_logger, "custom", channel);
		}
	}
}

void ChannelManager::UpdateGroupChannel()
{
	const char* leaderName = GetGroupLeaderName();
	if (leaderName && leaderName[0])
	{
		if (!m_group_channel || !ci_equals(m_group_channel->GetSubName(), leaderName))
		{
			m_group_channel.emplace(m_logger, "group", mq::to_lower_copy(leaderName));
		}
	}
	else if (m_group_channel) 
	{
		m_group_channel.reset();
	}
}

void ChannelManager::UpdateRaidChannel()
{
	const char* leaderName = GetRaidLeaderName();
	if (leaderName && leaderName[0])
	{
		std::string leader_lower = leaderName;
		to_lower(leader_lower);
		if (!m_raid_channel || !ci_equals(m_raid_channel->GetSubName(), leaderName))
		{
			m_raid_channel.emplace(m_logger, "raid", mq::to_lower_copy(leaderName));
		}
	}
	else if (m_raid_channel)
	{
		m_raid_channel.reset();
	}
}

void ChannelManager::SetGameState(int gameState)
{
	if (gameState < GAMESTATE_INGAME)
	{
		m_server_channel.reset();
		m_group_channel.reset();
		m_raid_channel.reset();
		m_zone_channel.reset();
		m_class_channel.reset();
		m_custom_channels.clear();
	}

	if (gameState > GAMESTATE_PRECHARSELECT)
	{
		if (!m_server_channel)
		{
			std::string server = GetServerShortName();
			to_lower(server);
			m_server_channel.emplace(m_logger, "server", server);
		}

		if (gameState == GAMESTATE_INGAME)
		{
			if (!m_class_channel)
			{
				std::string classname = GetClassName();
				to_lower(classname);
				m_class_channel.emplace(m_logger, "class", classname);
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
		if (now > m_pulseTimer)
		{
			m_pulseTimer = now + UPDATE_TICK_MILLISECONDS;
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
		m_zone_channel.emplace(m_logger, "zone", pZoneInfo->ShortName);
	}
}
}