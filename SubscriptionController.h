#pragma once
#include <mq/Plugin.h>
#include "Remote.pb.h"

namespace remote {

	class SubscriptionController
	{
	public:
		SubscriptionController(std::optional<std::string> channelPrefix)
			: channelPrefix(std::move(channelPrefix)),
			channelSuffix(std::nullopt),
			dropbox()
		{
		}

		void Connect(const std::optional<std::string> channel = std::nullopt);
		void Disconnect();
		void SendCommand(std::string command, bool includeSelf);
		void SendCommand(std::string reciever, std::string command);
		bool IsChannelFor(std::string suffix) const 
		{
			if (channelSuffix.has_value()) 
			{
				return channelSuffix.value() == suffix;
			}
			else 
			{
				return false;
			}
		}

	private:
		// Handlers
		void ReceivedMessageHandler(const std::shared_ptr<postoffice::Message>& message);

		const std::string GetChannelName() const {
			return fmt::format("{}{}", channelPrefix.value_or(""), channelSuffix.value_or(""));
		}

	private:
		bool isConnected = false;
		std::optional<std::string> channelPrefix;
		std::optional<std::string> channelSuffix;
		postoffice::DropboxAPI dropbox;
	};
}
