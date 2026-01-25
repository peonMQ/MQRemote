#pragma once
#include <mq/Plugin.h>
#include "Remote.pb.h"

namespace remote {
	class Channel
	{
	public:
		Channel(std::string name, std::string sub_name = "")
			: m_name(std::move(name)), m_sub_name(std::move(sub_name))
		{
			// Initialization logic (was in Initialize())
			if (DnsName().empty())
			{
				WriteChatf("\am[%s]\ax Connecting ()", mqplugin::PluginName);
				m_dropbox = postoffice::AddActor([this](const std::shared_ptr<postoffice::Message>& msg) {
					ReceivedMessageHandler(msg);
					});
			}
			else
			{
				auto channelName = DnsName();
				WriteChatf("\am[%s]\ax Connecting (\aw%s\ax)", mqplugin::PluginName, channelName.c_str());
				m_dropbox = postoffice::AddActor(channelName.c_str(), [this](const std::shared_ptr<postoffice::Message>& msg) {
					ReceivedMessageHandler(msg);
					});
			}
		}

		~Channel()
		{
			if (DnsName().empty())
			{
				WriteChatf("\am[%s]\ax Disconnecting ()", mqplugin::PluginName);
			}
			else {
				auto channelName = DnsName();
				WriteChatf("\am[%s]\ax Disconnecting (\aw%s\ax)", mqplugin::PluginName, channelName.c_str());
			}

			m_dropbox.Remove();
		}

		void SendCommand(std::string command, bool includeSelf);
		void SendCommand(std::string reciever, std::string command);
		std::string Name() const { return m_name; }
		std::string SubName() const { return m_sub_name; }
		std::string DnsName() const
		{
			if (m_sub_name.empty())
				return m_name;
			return m_name + "." + m_sub_name;
		}

		// Delete copy constructor/assignment
		Channel(const Channel&) = delete;
		Channel& operator=(const Channel&) = delete;

		// Default move constructor/assignment
		Channel(Channel&&) = default;
		Channel& operator=(Channel&&) = default;

	private:
		void ReceivedMessageHandler(const std::shared_ptr<postoffice::Message>& message);

	private:
		std::string m_name;
		std::string m_sub_name;
		postoffice::DropboxAPI m_dropbox;
	};
}

