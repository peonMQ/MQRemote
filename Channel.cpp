#include "Channel.h"

namespace remote {
	void Channel::SendCommand(std::string command, bool includeSelf) {
		auto channelName = DnsName();
		WriteChatf("\am[%s]\ax \a-t[ \ax\at-->\ax\a-t(%s) ]\ax \aw%s\ax", mqplugin::PluginName, channelName.c_str(), command.c_str());
		postoffice::Address address;
		address.Server = GetServerShortName();
		if (!channelName.empty())
		{
			address.Mailbox = channelName;
		}
		proto::remote::Message message;
		message.set_id(proto::remote::MessageId::Broadcast);
		message.set_command(command);
		message.set_includeself(includeSelf);
		m_dropbox.Post(address, message);
	}

	void Channel::SendCommand(std::string receiver, std::string command) {
		auto channelName = DnsName();
		WriteChatf("\am[%s]\ax \a-t[ \ax\at-->\ax\a-t(%s) ]\ax \aw%s\ax", mqplugin::PluginName, receiver.c_str(), command.c_str());
		postoffice::Address address;
		address.Server = GetServerShortName();
		address.Character = receiver;
		if (!channelName.empty())
		{
			address.Mailbox = channelName;
		}
		proto::remote::Message message;
		message.set_id(proto::remote::MessageId::Personal);
		message.set_command(command);
		m_dropbox.Post(address, message, [receiver](int code, const std::shared_ptr<postoffice::Message>&) {
			if (code < 0)
			{
				WriteChatf("\am[%s]\ax Failed sending command to \ay%s\ax.", mqplugin::PluginName, receiver.c_str());
			}
			});
	}

	void Channel::ReceivedMessageHandler(const std::shared_ptr<postoffice::Message>& message)
	{
		if (GetGameState() != GAMESTATE_INGAME) return;
		if (!pLocalPlayer) return;

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

			DoCommand((PSPAWNINFO)pLocalPlayer, msg.command().c_str());
		}
		break;
		case mq::proto::remote::MessageId::Personal:
		{
			DoCommand((PSPAWNINFO)pLocalPlayer, msg.command().c_str());
			proto::remote::Message reply;
			reply.set_id(mq::proto::remote::MessageId::Success);
			m_dropbox.PostReply(message, reply);
		}
		break;
		}
	}

} // namespace remote
