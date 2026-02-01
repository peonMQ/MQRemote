#pragma once
#include <mq/Plugin.h>
#include "Remote.pb.h"

namespace remote {

	class Channel
	{
	public:
		Channel(std::string name, std::string sub_name = "");
		~Channel();

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

