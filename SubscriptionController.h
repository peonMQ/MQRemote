#pragma once
#include <mq/Plugin.h>
#include "Remote.pb.h"

namespace remote {
	enum SubscriptionType
	{
		All, Group, Raid, Zone
	};

	class SubscriptionController
	{
	public:
		SubscriptionController(SubscriptionType subscriptionType, std::optional<std::string> suffix = std::nullopt)
		{
			// Determine channel prefix/suffix based on type
			switch (subscriptionType)
			{
			case SubscriptionType::All:
				m_channelPrefix = std::nullopt;
				m_channelSuffix = std::nullopt;
				break;
			case SubscriptionType::Group:
				m_channelPrefix = "group";
				m_channelSuffix = std::move(suffix);
				break;
			case SubscriptionType::Raid:
				m_channelPrefix = "raid";
				m_channelSuffix = std::move(suffix);
				break;
			case SubscriptionType::Zone:
				m_channelPrefix = "zone";
				m_channelSuffix = std::move(suffix);
				break;
			default:
				throw std::invalid_argument("Unknown subscription type");
			}

			// Initialization logic (was in Initialize())
			if (m_channelSuffix)
			{
				auto channelName = GetChannelName();
				WriteChatf("\am[%s]\ax Connecting (\aw%s\ax)", mqplugin::PluginName, channelName.c_str());
				m_dropbox = postoffice::AddActor(channelName.c_str(), [this](const std::shared_ptr<postoffice::Message>& msg) {
					ReceivedMessageHandler(msg);
					});
			}
			else
			{
				WriteChatf("\am[%s]\ax Connecting ()", mqplugin::PluginName);
				m_dropbox = postoffice::AddActor([this](const std::shared_ptr<postoffice::Message>& msg) {
					ReceivedMessageHandler(msg);
					});
			}
		}

		~SubscriptionController() 
		{
			if (m_channelSuffix)
			{
				auto channelName = GetChannelName();
				WriteChatf("\am[%s]\ax Disconnecting (\aw%s\ax)", mqplugin::PluginName, channelName.c_str());
			}
			else {

				WriteChatf("\am[%s]\ax Disconnecting ()", mqplugin::PluginName);
			}
			m_channelSuffix = std::nullopt;
			m_dropbox.Remove();
		}

		void SendCommand(std::string command, bool includeSelf);
		void SendCommand(std::string reciever, std::string command);
		bool IsChannelFor(std::string suffix) const 
		{
			return m_channelSuffix == suffix;
		}

		// Delete copy constructor/assignment
		SubscriptionController(const SubscriptionController&) = delete;
		SubscriptionController& operator=(const SubscriptionController&) = delete;

		// Default move constructor/assignment
		SubscriptionController(SubscriptionController&&) = default;
		SubscriptionController& operator=(SubscriptionController&&) = default;

	private:
		void ReceivedMessageHandler(const std::shared_ptr<postoffice::Message>& message);

		const std::string GetChannelName() const {
			return fmt::format("{}{}", m_channelPrefix.value_or(""), m_channelSuffix.value_or(""));
		}

	private:
		std::optional<std::string> m_channelPrefix;
		std::optional<std::string> m_channelSuffix;
		postoffice::DropboxAPI m_dropbox;
	};
}
