#pragma once
#include <mq/Plugin.h>
#include "Remote.pb.h"

namespace remote {

	class Channel
	{
	public:
		Channel(std::string name, std::string sub_name = "");
		~Channel();

		void SendCommand(std::string command, bool includeSelf);
		void SendCommand(std::string reciever, std::string command);

		std::string_view GetName() const { return m_name; }
		std::string_view GetSubName() const { return m_sub_name; }
		std::string_view GetDnsName() const { return m_dnsName;}

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

