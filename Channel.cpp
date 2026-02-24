#include "Channel.h"
#include "Logger.h"
#include "fmt/format.h"

namespace remote {

Channel::Channel(Logger* logger, std::string name, std::string_view sub_name)
	: m_logger(logger)
	, m_name(std::move(name))
	, m_sub_name(mq::to_lower_copy(sub_name))
	, m_dnsName(m_sub_name.empty() ? m_name : fmt::format("{}.{}", m_name, m_sub_name))
{
	m_logger->Log(Logger::LogFlags::LOG_CONNECTIONS,
		PLUGIN_MSG "Connecting (\aw%s\ax)", m_dnsName.c_str());
		
	m_dropbox = postoffice::AddActor(m_dnsName.c_str(), [this](const std::shared_ptr<postoffice::Message>& msg) {
		ReceivedMessageHandler(msg);
	});
}

Channel::~Channel()
{
	m_logger->Log(Logger::LogFlags::LOG_CONNECTIONS,
		PLUGIN_MSG "Disconnecting (\aw%s\ax)", m_dnsName.c_str());

	m_dropbox.Remove();
}

void Channel::SendCommand(std::string command, const bool includeSelf)
{
	m_logger->Log(Logger::LogFlags::LOG_SEND,
		PLUGIN_MSG "\a-t[ \ax\at-->\ax\a-t(%s) ]\ax \aw%s\ax", m_dnsName.c_str(), command.c_str());

	postoffice::Address address;
	address.Server = GetServerShortName();

	if (!m_dnsName.empty())
	{
		address.Mailbox = m_dnsName;
	}

	proto::remote::Message message;
	message.set_id(proto::remote::MessageId::Broadcast);
	message.set_command(std::move(command));
	message.set_includeself(includeSelf);

	m_dropbox.Post(address, message);
}

void Channel::SendCommand(std::string receiver, std::string command)
{
	m_logger->Log(Logger::LogFlags::LOG_SEND, PLUGIN_MSG "\a-t[ \ax\at-->\ax\a-t(%s->%s) ]\ax \aw%s\ax",
		m_dnsName.c_str(), receiver.c_str(), command.c_str());

	postoffice::Address address;
	address.Server = GetServerShortName();
	address.Character = receiver;
	if (!m_dnsName.empty())
	{
		address.Mailbox = m_dnsName;
	}

	proto::remote::Message message;
	message.set_id(proto::remote::MessageId::Personal);
	message.set_command(std::move(command));

	m_dropbox.Post(address, message,
		[receiverStr = std::move(receiver), dnsName = m_dnsName, this](int code, const std::shared_ptr<postoffice::Message>&)
	{
		if (code < 0)
		{
			m_logger->Log(Logger::LogFlags::LOG_ERROR,
				PLUGIN_MSG "Failed sending command to \ay%s->%s\ax.", dnsName.c_str(), receiverStr.c_str());
		}
	});
}

void Channel::ReceivedMessageHandler(const std::shared_ptr<postoffice::Message>& message)
{
	mq::proto::remote::Message msg;
	msg.ParseFromString(*message->Payload);

	switch (msg.id())
	{
	case mq::proto::remote::MessageId::Broadcast:
		{
			if (message->Sender && message->Sender->Character.has_value())
			{
				if (!pLocalPlayer)
					return;

				if (mq::ci_equals(message->Sender->Character.value(), pLocalPlayer->Name)
					&& msg.includeself() == false)
				{
					return;
				}
			}

			m_logger->Log(Logger::LogFlags::LOG_RECEIVE, PLUGIN_MSG "\a-t[ \ax\at<--\ax\a-t(%s) ]\ax \aw%s\ax",
				m_dnsName.c_str(), msg.command().c_str());

			DoCommand(msg.command().c_str());
		}
		break;

	case mq::proto::remote::MessageId::Personal:
		{
			m_logger->Log(Logger::LogFlags::LOG_RECEIVE, PLUGIN_MSG "\a-t[ \ax\at<--\ax\a-t(%s<-%s) ]\ax \aw%s\ax",
				m_dnsName.c_str(), message->Sender->Character.value().c_str(), msg.command().c_str());
				
			DoCommand(msg.command().c_str());

			proto::remote::Message reply;
			reply.set_id(mq::proto::remote::MessageId::Success);
			m_dropbox.PostReply(message, reply);
		}
		break;
	}
}

} // namespace remote
