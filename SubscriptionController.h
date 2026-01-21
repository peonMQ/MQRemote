#pragma once
#include <mq/Plugin.h>
#include "Remote.pb.h"

namespace remote {
	struct SubscriptionType
	{
		std::string name;
		std::string prefix;

		SubscriptionType() = default;

		SubscriptionType(std::string n, std::string p)
			: name(std::move(n)), prefix(std::move(p)) {
		}

		bool operator==(const SubscriptionType& other) const noexcept {
			return name == other.name && prefix == other.prefix;
		}

		bool operator!=(const SubscriptionType& other) const noexcept {
			return !(*this == other);
		}
	};

	class SubscriptionRegistry
	{
	public:
		// -----------------------------------------
		// Built‑in subscription types (inline static)
		// -----------------------------------------
		inline static const SubscriptionType All{ "All",   "" };
		inline static const SubscriptionType Group{ "Group", "group" };
		inline static const SubscriptionType Raid{ "Raid",  "raid" };
		inline static const SubscriptionType Zone{ "Zone",  "zone" };

		inline static const std::array<SubscriptionType, 4> BuiltinSubscriptionTypes = {
			All,
			Group,
			Raid,
			Zone
		};

		// GetOrAdd
		static SubscriptionType GetOrAdd(std::string name)
		{
			auto subOpt = SubscriptionRegistry::FromName(name);
			if (subOpt.has_value())
			{
				return *subOpt;
			}

			for (const auto& t : BuiltinSubscriptionTypes)
				if (t.name == name)
					return t;
			
			// Convert to lowercase for consistent keys 
			std::string nameLower{ name };
			std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), [](unsigned char c) { return std::tolower(c); });
			auto type = SubscriptionType(name, nameLower);
			std::scoped_lock lock(_mutex);
			_runtimeTypes[name] = type;
			return type;
		}

		// Lookup by name (runtime first, then built‑in)
		static std::optional<SubscriptionType> FromName(std::string name)
		{
			{
				std::scoped_lock lock(_mutex);
				auto it = _runtimeTypes.find(name);
				if (it != _runtimeTypes.end())
					return it->second;
			}

			for (const auto& t : BuiltinSubscriptionTypes)
				if (t.name == name)
					return t;

			return std::nullopt;
		}

	private:
		static inline std::unordered_map<std::string, SubscriptionType> _runtimeTypes;
		static inline std::mutex _mutex;
	};

	class SubscriptionController
	{
	public:
		SubscriptionController(const SubscriptionType& type, std::optional<std::string> suffix = std::nullopt)
		{
			if (type.prefix.empty()) {
				m_channelPrefix = std::nullopt;
				m_channelSuffix = std::nullopt;
			}
			else {
				m_channelPrefix = std::string(type.prefix);
				m_channelSuffix = std::move(suffix);
			}

			// Initialization logic (was in Initialize())
			if (m_channelPrefix)
			{
				auto channelName = GetChannelName();
				WriteChatf("\am[%s]\ax Connecting (\aw%s\ax)", mqplugin::PluginName, channelName.c_str());
				m_dropbox = postoffice::AddActor(channelName.c_str(), [this](const std::shared_ptr<postoffice::Message>& msg) {
					ReceivedMessageHandler(msg);
					});
			}
			else
			{
				WriteChatf("\am[%s]\ax Connecting ()", mqplugin::PluginName);
				m_dropbox = postoffice::AddActor([this](const std::shared_ptr<postoffice::Message>& msg) {
					ReceivedMessageHandler(msg);
					});
			}
		}

		~SubscriptionController() 
		{
			if (m_channelPrefix)
			{
				auto channelName = GetChannelName();
				WriteChatf("\am[%s]\ax Disconnecting (\aw%s\ax)", mqplugin::PluginName, channelName.c_str());
			}
			else {

				WriteChatf("\am[%s]\ax Disconnecting ()", mqplugin::PluginName);
			}
			m_channelSuffix = std::nullopt;
			m_dropbox.Remove();
		}

		void SendCommand(std::string command, bool includeSelf);
		void SendCommand(std::string reciever, std::string command);
		bool IsChannelFor(std::string suffix) const 
		{
			return m_channelSuffix == suffix;
		}

		// Delete copy constructor/assignment
		SubscriptionController(const SubscriptionController&) = delete;
		SubscriptionController& operator=(const SubscriptionController&) = delete;

		// Default move constructor/assignment
		SubscriptionController(SubscriptionController&&) = default;
		SubscriptionController& operator=(SubscriptionController&&) = default;

	private:
		void ReceivedMessageHandler(const std::shared_ptr<postoffice::Message>& message);

		const std::string GetChannelName() const {
			return fmt::format("{}{}", m_channelPrefix.value_or(""), m_channelSuffix.value_or(""));
		}

	private:
		std::optional<std::string> m_channelPrefix;
		std::optional<std::string> m_channelSuffix;
		postoffice::DropboxAPI m_dropbox;
	};
}

// Hash function specialization
namespace std {
	template<>
	struct hash<remote::SubscriptionType>
	{
		size_t operator()(const remote::SubscriptionType& t) const noexcept
		{
			// Fowler–Noll–Vo (FNV-1a) hash for good distribution
			const size_t fnv_offset = 1469598103934665603ULL;
			const size_t fnv_prime = 1099511628211ULL;

			size_t hash = fnv_offset;

			for (char c : t.name) {
				hash ^= static_cast<size_t>(c);
				hash *= fnv_prime;
			}
			for (char c : t.prefix) {
				hash ^= static_cast<size_t>(c);
				hash *= fnv_prime;
			}
			return hash;
		}
	};
}
