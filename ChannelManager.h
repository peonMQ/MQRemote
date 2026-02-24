#pragma once

#include "Channel.h"

#include <optional>
#include <unordered_map>
#include <string>

namespace remote {

class Logger;

class ChannelManager
{
public:
	ChannelManager(Logger* logger)
		: m_logger(logger)
	{
	}

	// lifecycle
	void Initialize();
	void Shutdown();

	void SetGameState(int gameState);
	void OnPulse();
	void OnBeginZone();
	void OnEndZone();

	// channels
	void JoinCustomChannel(std::string_view name, std::string_view autoArg = {});
	void LeaveCustomChannel(std::string_view name, std::string_view autoArg = {});
	Channel* FindChannel(std::string_view name);

	Channel* GetGlobalChannel() { return opt_ptr(m_global_channel); }
	Channel* GetServerChannel() { return opt_ptr(m_server_channel); }
	Channel* GetGroupChannel() { return opt_ptr(m_group_channel); }
	Channel* GetRaidChannel() { return opt_ptr(m_raid_channel); }
	Channel* GetZoneChannel() { return opt_ptr(m_zone_channel); }
	Channel* GetClassChannel() { return opt_ptr(m_class_channel); }

	std::unordered_map<std::string, Channel>& GetCustomChannels() { return m_custom_channels; }

	void LoadPersistentChannels();

private:
	static Channel* opt_ptr(std::optional<Channel>& o)
	{
		return o ? &*o : nullptr;
	}

	void UpdateGroupChannel();
	void UpdateRaidChannel();

private:
	std::string m_channelINISection;

	std::optional<Channel> m_global_channel;
	std::optional<Channel> m_server_channel;
	std::optional<Channel> m_group_channel;
	std::optional<Channel> m_raid_channel;
	std::optional<Channel> m_zone_channel;
	std::optional<Channel> m_class_channel;

	std::unordered_map<std::string, Channel> m_custom_channels;

	Logger* m_logger;
	std::chrono::steady_clock::time_point m_nextGroupUpdate = std::chrono::steady_clock::now();
};

} // namespace remote
