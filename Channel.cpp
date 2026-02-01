#include "Channel.h"
#include "fmt/format.h"

namespace remote {

	Channel::Channel(std::string name, std::string sub_name)
		: m_name(std::move(name))
		, m_sub_name(std::move(sub_name))
		, m_dnsName(m_sub_name.empty() ? m_name : fmt::format("{}.{}", m_name, m_sub_name))
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

	Channel::~Channel()
	{
		if (m_dnsName.empty())
		{
			WriteChatf("\am[%s]\ax Disconnecting ()", mqplugin::PluginName);
		}
		else
		{
			WriteChatf("\am[%s]\ax Disconnecting (\aw%s\ax)", mqplugin::PluginName, m_dnsName.c_str());
		}

		m_dropbox.Remove();
	}

	void Channel::SendCommand(const std::string_view command, const bool includeSelf)
	{
		WriteChatf("\am[%s]\ax \a-t[ \ax\at-->\ax\a-t(%s) ]\ax \aw%.*s\ax", mqplugin::PluginName, m_dnsName.c_str(), static_cast<int>(command.length()), command.data());
		postoffice::Address address;
		address.Server = GetServerShortName();
		if (!m_dnsName.empty())
		{
			address.Mailbox = m_dnsName;
		}
		proto::remote::Message message;
		message.set_id(proto::remote::MessageId::Broadcast);
		message.set_command(command.data(), command.size());
		message.set_includeself(includeSelf);
		m_dropbox.Post(address, message);
	}

	void Channel::SendCommand(const std::string_view receiver, const std::string_view command) 
	{
		WriteChatf("\am[%s]\ax \a-t[ \ax\at-->\ax\a-t(%s->%.*s) ]\ax \aw%.*s\ax", mqplugin::PluginName, m_dnsName.c_str(), static_cast<int>(receiver.length()), receiver.data(), static_cast<int>(command.length()), command.data());
		postoffice::Address address;
		address.Server = GetServerShortName();
		address.Character = std::string(receiver);
		if (!m_dnsName.empty())
		{
			address.Mailbox = m_dnsName;
		}
		proto::remote::Message message;
		message.set_id(proto::remote::MessageId::Personal);
		message.set_command(command.data(), command.size());
		m_dropbox.Post(address, message, [receiverStr = std::string(receiver), dnsName = m_dnsName](int code, const std::shared_ptr<postoffice::Message>&) 
			{
				if (code < 0)
				{
					WriteChatf("\am[%s]\ax Failed sending command to \ay%s->%s\ax.", mqplugin::PluginName, dnsName.c_str(), receiverStr.c_str());
				}
			});
	}

	void Channel::ReceivedMessageHandler(const std::shared_ptr<postoffice::Message>& message)
	{
		if (GetGameState() != GAMESTATE_INGAME) 
		{
			return;
		}

		if (!pLocalPlayer) 
		{
			return;
		}

		mq::proto::remote::Message msg;
		msg.ParseFromString(*message->Payload);
		switch (msg.id())
		{
			case mq::proto::remote::MessageId::Broadcast:
				{
					if (message->Sender && message->Sender->Character.has_value())
					{
						auto character = message->Sender->Character.value();
						if (character == pLocalPlayer->Name && msg.includeself() == false) return;
					}

					WriteChatf("\am[%s]\ax \a-t[ \ax\at<--\ax\a-t(%s<-%s) ]\ax \aw%s\ax", mqplugin::PluginName, m_dnsName.c_str(), message->Sender->Character.value().c_str(), msg.command().c_str());
					DoCommand((PSPAWNINFO)pLocalPlayer, msg.command().c_str());
				}
				break;
			case mq::proto::remote::MessageId::Personal:
				{
					WriteChatf("\am[%s]\ax \a-t[ \ax\at<--\ax\a-t(%s<-%s) ]\ax \aw%s\ax", mqplugin::PluginName, m_dnsName.c_str(), message->Sender->Character.value().c_str(), msg.command().c_str());
					DoCommand((PSPAWNINFO)pLocalPlayer, msg.command().c_str());
					proto::remote::Message reply;
					reply.set_id(mq::proto::remote::MessageId::Success);
					m_dropbox.PostReply(message, reply);
				}
				break;
		}
	}

} // namespace remote
