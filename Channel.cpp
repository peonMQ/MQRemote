#include "Channel.h"
#include "Logger.h"
#include "fmt/format.h"

namespace remote {
Channel::Channel(Logger* logger, std::string name, std::string sub_name)
	: m_logger(logger)
	, m_name(std::move(name))
	, m_sub_name(std::move(sub_name))
	, m_dnsName(m_sub_name.empty() ? m_name : fmt::format("{}.{}", m_name, m_sub_name))
{
	// Initialization logic (was in Initialize())
	if (m_dnsName.empty())
	{
		if(m_logger)
		{
			m_logger->Log(Logger::LogFlag::LOG_GENERAL, "\am[%s]\ax Connecting ()", mqplugin::PluginName);
		}		

		m_dropbox = postoffice::AddActor([this](const std::shared_ptr<postoffice::Message>& msg) {
			ReceivedMessageHandler(msg);
		});
	}
	else
	{
		if(m_logger)
		{
			m_logger->Log(Logger::LogFlag::LOG_GENERAL, "\am[%s]\ax Connecting (\aw%s\ax)", mqplugin::PluginName, m_dnsName.c_str());
		}
		
		m_dropbox = postoffice::AddActor(m_dnsName.c_str(), [this](const std::shared_ptr<postoffice::Message>& msg) {
			ReceivedMessageHandler(msg);
		});
	}
}

Channel::~Channel()
{
	if (pEverQuest) {
		if(m_logger) 
		{
			if (m_dnsName.empty())
			{
				m_logger->Log(Logger::LogFlag::LOG_GENERAL, "\am[%s]\ax Disconnecting ()", mqplugin::PluginName);
			}
			else 
			{
				m_logger->Log(Logger::LogFlag::LOG_GENERAL, "\am[%s]\ax Disconnecting (\aw%s\ax)", mqplugin::PluginName, m_dnsName.c_str());
			}
		}

		m_dropbox.Remove();
	}
}

void Channel::SendCommand(std::string command, const bool includeSelf)
{
	if(m_logger) 
	{
		m_logger->Log(Logger::LogFlag::LOG_SEND, "\am[%s]\ax \a-t[ \ax\at-->\ax\a-t(%s) ]\ax \aw%s\ax", mqplugin::PluginName,
			m_dnsName.c_str(), command.c_str());
	}

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
	if(m_logger)
	{
		m_logger->Log(Logger::LogFlag::LOG_SEND, "\am[%s]\ax \a-t[ \ax\at-->\ax\a-t(%s->%s) ]\ax \aw%s\ax", mqplugin::PluginName,
			m_dnsName.c_str(), receiver.c_str(), command.c_str());
	}

	postoffice::Address address;
	address.Server = GetServerShortName();
	address.Character = std::string(receiver);
	if (!m_dnsName.empty())
	{
		address.Mailbox = m_dnsName;
	}

	proto::remote::Message message;
	message.set_id(proto::remote::MessageId::Personal);
	message.set_command(std::move(command));

	m_dropbox.Post(address, message,
		[receiverStr = std::move(receiver), dnsName = m_dnsName, logger = m_logger](int code, const std::shared_ptr<postoffice::Message>&)
	{
		if (code < 0)
		{
			if (logger) // always check pointer
			{
				logger->Log(Logger::LOG_GENERAL,
					"\am[%s]\ax Failed sending command to \ay%s->%s\ax.",
					mqplugin::PluginName, dnsName.c_str(), receiverStr.c_str());
			}
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

				if(m_logger)
				{
					m_logger->Log(Logger::LogFlag::LOG_RECEIVE, "\am[%s]\ax \a-t[ \ax\at<--\ax\a-t(%s) ]\ax \aw%s\ax", mqplugin::PluginName,
						m_dnsName.c_str(), msg.command().c_str());
				}

				DoCommand(msg.command().c_str());
			}
			break;

		case mq::proto::remote::MessageId::Personal:
			{
				if(m_logger) {
					m_logger->Log(Logger::LogFlag::LOG_RECEIVE, "\am[%s]\ax \a-t[ \ax\at<--\ax\a-t(%s<-%s) ]\ax \aw%s\ax", mqplugin::PluginName,
						m_dnsName.c_str(), message->Sender->Character.value().c_str(), msg.command().c_str());
				}
				
				DoCommand(msg.command().c_str());

				proto::remote::Message reply;
				reply.set_id(mq::proto::remote::MessageId::Success);
				m_dropbox.PostReply(message, reply);
			}
			break;
	}
}
}
