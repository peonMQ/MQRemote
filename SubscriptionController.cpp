#include "SubscriptionController.h"

namespace remote {
	void SubscriptionController::Connect(const std::optional<std::string> channel)
	{
		if (isConnected) return;

		channelSuffix = channel;
		if(channel.has_value())
		{
			auto channelName = GetChannelName();
			WriteChatf("\am[%s]\ax Connecting (\aw%s\ax)", mqplugin::PluginName, channelName.c_str());
			dropbox = postoffice::AddActor(channelName.c_str(), [this](const std::shared_ptr<postoffice::Message>& msg) {
				ReceivedMessageHandler(msg);
				});
		}
		else
		{
			WriteChatf("\am[%s]\ax Connecting ()", mqplugin::PluginName);
			dropbox = postoffice::AddActor([this](const std::shared_ptr<postoffice::Message>& msg) {
				ReceivedMessageHandler(msg);
				});
		}
		isConnected = true;
	}

	void SubscriptionController::Disconnect()
	{
		if (isConnected) 
		{
			if (channelSuffix.has_value())
			{
				auto channelName = GetChannelName();
				WriteChatf("\am[%s]\ax Disconnecting (\aw%s\ax)", mqplugin::PluginName, channelName.c_str());
			}
			else {

				WriteChatf("\am[%s]\ax Disconnecting ()", mqplugin::PluginName);
			}
			channelSuffix = std::nullopt;
			dropbox.Remove();
			isConnected = false;
		}
	}

	void SubscriptionController::SendCommand(std::string command, bool includeSelf) {
		auto channelName = GetChannelName();
		WriteChatf("\am[%s]\ax \a-t[ \ax\at-->\ax\a-t(%s) ]\ax \aw%s\ax", mqplugin::PluginName, channelName.c_str(), command.c_str());
		postoffice::Address address;
		address.Server = GetServerShortName();
		if (channelPrefix.has_value()) 
		{
			address.Mailbox = channelName;
		}
		proto::remote::Message message;
		message.set_id(proto::remote::MessageId::Broadcast);
		message.set_command(command);
		message.set_includeself(includeSelf);
		dropbox.Post(address, message);
	}

	void SubscriptionController::SendCommand(std::string receiver, std::string command) {
		auto channelName = GetChannelName();
		WriteChatf("\am[%s]\ax \a-t[ \ax\at-->\ax\a-t(%s) ]\ax \aw%s\ax", mqplugin::PluginName, receiver.c_str(), command.c_str());
		postoffice::Address address;
		address.Server = GetServerShortName();
		address.Character = receiver;
		if (channelPrefix.has_value()) 
		{
			address.Mailbox = channelName;
		}
		proto::remote::Message message;
		message.set_id(proto::remote::MessageId::Personal);
		message.set_command(command);
		dropbox.Post(address, message, [receiver](int code, const std::shared_ptr<postoffice::Message>&) {
			if (code < 0)
			{
				WriteChatf("\am[%s]\ax Failed sending command to \ay%s\ax.", mqplugin::PluginName, receiver.c_str());
			}
		});
	}

	void SubscriptionController::ReceivedMessageHandler(const std::shared_ptr<postoffice::Message>& message)
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
					if(character == pLocalPlayer->Name && msg.includeself() == false) return;
				}

				DoCommand((PSPAWNINFO)pLocalPlayer, msg.command().c_str());
			}
			break; 
		case mq::proto::remote::MessageId::Personal:
			{
				DoCommand((PSPAWNINFO)pLocalPlayer, msg.command().c_str());
				proto::remote::Message reply;
				reply.set_id(mq::proto::remote::MessageId::Success);
				dropbox.PostReply(message, reply);
			}
			break;
		}
	}

} // namespace remote
