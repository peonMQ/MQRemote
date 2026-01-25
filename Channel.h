#pragma once
#include <mq/Plugin.h>
#include "Remote.pb.h"

namespace remote {
	class Channel
	{
	public:
		Channel(std::string name, std::string sub_name = "")
			: m_name(std::move(name)), m_sub_name(std::move(sub_name)), m_dnsName(m_sub_name.empty() ? m_name : m_name + "." + m_sub_name)
		{
			// Initialization logic (was in Initialize())
			if (m_dnsName.empty())
			{
				WriteChatf("\am[%s]\ax Connecting ()", mqplugin::PluginName);
				m_dropbox = postoffice::AddActor([this](const std::shared_ptr<postoffice::Message>& msg) {
					ReceivedMessageHandler(msg);
					});
			}
			else
			{
				WriteChatf("\am[%s]\ax Connecting (\aw%s\ax)", mqplugin::PluginName, m_dnsName.c_str());
				m_dropbox = postoffice::AddActor(m_dnsName.c_str(), [this](const std::shared_ptr<postoffice::Message>& msg) {
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
				WriteChatf("\am[%s]\ax Disconnecting (\aw%s\ax)", mqplugin::PluginName, m_dnsName.c_str());
			}

			m_dropbox.Remove();
		}

		void SendCommand(const std::string_view command, const bool includeSelf);
		void SendCommand(const std::string_view reciever, const std::string_view command);
		std::string_view Name() const { return m_name; }
		std::string_view SubName() const { return m_sub_name; }
		std::string_view DnsName() const noexcept { return m_dnsName;}

		// Delete copy constructor/assignment
		Channel(const Channel&) = delete;
		Channel& operator=(const Channel&) = delete;

		// Default move constructor/assignment
		Channel(Channel&&) = default;
		Channel& operator=(Channel&&) = default;

	private:
		void ReceivedMessageHandler(const std::shared_ptr<postoffice::Message>& message);

	private:
		const std::string m_name;
		const std::string m_sub_name;
		const std::string m_dnsName;
		postoffice::DropboxAPI m_dropbox;
	};
}

