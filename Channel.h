#pragma once

#include "Remote.pb.h"
#include "mq/Plugin.h"

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

	// non-copyable
	Channel(const Channel&) = delete;
	Channel& operator=(const Channel&) = delete;

private:
	void ReceivedMessageHandler(const std::shared_ptr<postoffice::Message>& message);

	const std::string m_name;
	const std::string m_sub_name;
	const std::string m_dnsName;
	postoffice::DropboxAPI m_dropbox;
};
} // namespace remote
