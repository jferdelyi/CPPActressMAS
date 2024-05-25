/**************************************************************************
 *                                                                        *
 *  Description: CPPActressMas multi-agent framework                      *
 *  Website:     https://github.com/jferdelyi/CPPActressMAS               *
 *  Copyright:   (c) 2023-Today, Jean-François Erdelyi                    *
 *                                                                        *
 *  CPP version of ActressMAS by Florin Leon                              *
 *  https://github.com/florinleon/ActressMas                              *
 *                                                                        *
 *  This program is free software; you can redistribute it and/or modify  *
 *  it under the terms of the GNU General License as published by         *
 *  the Free Software Foundation. This program is distributed in the      *
 *  hope that it will be useful, but WITHOUT ANY WARRANTY; without even   *
 *  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR   *
 *  PURPOSE. See the GNU General License for more details.                *
 *                                                                        *
 **************************************************************************/

#pragma once

#include <mqtt/async_client.h>

#include <chrono>

#include "EnvironmentMas.h"

#define PAHO_ERR_NOT_INIT (-1000)
#define PAHO_ERR_CONNECTION_NOT_INITIALIZED_ONCE (-1010)
#define PAHO_ERR_ALREADY_CONNECTED (-1011)
#define PAHO_ERR_NOT_CONNECTED (-1012)

/**
 * CPPActressMAS
 */
namespace cam {

	struct MQTTTopics {
		/**
		 * Post message to this environment
		 **/
		std::string m_post_message_topic;

		/**
		 * Post message by agent name to this environment
		 */
		std::string m_post_message_by_name_topic;

		/**
		 * Post agent to this environment
		 */
		std::string m_post_agent_topic;

		/**
		 * Discovery environment callback topic
		 */
		std::string m_callback_discovery_topic;

		/**
		 * Post message topic topic
		 */
		std::string m_post_broadcast_message_topic;

		/**
		 * Post message by name topic
		 */
		std::string m_post_broadcast_message_by_name_topic;

		/**
		 * Post message in broadcast topic
		 */
		std::string m_post_broadcast_message_broadcast_topic;

		/**
		 * Discovery environment topic
		 */
		std::string m_broadcast_discovery_topic;
	};

	/**
	 * Callbacks for the success or failures of requested actions.
	 * This could be used to initiate further action
	 */
	class SubscribeActionListener : public virtual mqtt::iaction_listener {
	protected:

		/**
		 * Reference to the environment
		 */
		cam::EnvironmentMas& m_environment;

	public:

		/**
		 * Constructor
		 * @param p_name the name of the callback
		 */
		explicit SubscribeActionListener(EnvironmentMas& p_environment) :
				m_environment(p_environment) {}

	protected:

		/**
		 * This method is invoked when an action fails.
		 * @param p_token the token
		 */
		void on_failure(const mqtt::token& p_token) override {
			m_environment.on_subscribe_error("Subscribe failed", p_token.get_reason_code());
		}

		/**
		 * This method is invoked when an action has completed successfully.
		 * @param p_token the token
		 */
		void on_success(const mqtt::token& p_token) override {
			m_environment.on_subscribe(p_token.get_message_id(), (*p_token.get_topics())[0]);
		}
	};

	/**
	 * Local callback & listener class for use with the client connection.
	 * This is primarily intended to receive messages, but it will also monitor
	 * the connection to the broker. If the connection is lost, it will attempt
	 * to restore the connection and re-subscribe to the topic.
	 */
	class Callback : public virtual mqtt::callback, public virtual mqtt::iaction_listener {
	protected:

		/**
		 * MAS environment
		 */
		cam::EnvironmentMas& m_environment;

		/**
		 * Topics
		 */
		const MQTTTopics& m_topics;

	public:

		/**
		 * Constructor
		 * @param p_environment the environment
		 * @param p_topics topics
		 */
		explicit Callback(cam::EnvironmentMas& p_environment, const MQTTTopics& p_topics) :
				m_environment(p_environment),
				m_topics(p_topics) {}

		/**
		 * Re-connection failure
		 * @param p_token the token
		 */
		void on_failure(const mqtt::token& p_token) override {
			m_environment.on_connection_error("Connection attempt failed", p_token.get_reason_code());
		}

		/**
		 * Re-connection success
		 * @param p_token the token
		 */
		void on_success(const mqtt::token& p_token) override {
			m_environment.on_connection_success("Connection attempt success", p_token.get_reason_code());

		}

		/**
		 * (Re)connection success
		 * @param p_cause the cause
		 */
		void connected(const std::string& p_cause) override {
			m_environment.on_log(p_cause);
			m_environment.on_connect(mqtt::ReasonCode::SUCCESS);
		}

		/**
		 * Callback for when the connection is lost.
		 * @param p_cause the cause
		 */
		void connection_lost(const std::string& p_cause) override {
			std::string l_log = p_cause;
			if (l_log.empty()) {
				l_log = "connection lost";
			}
			m_environment.on_log(l_log);
			m_environment.on_disconnect();
		}

		/**
		 * Callback for when a message arrives.
		 * @param p_message the message
		 */
		void message_arrived(mqtt::const_message_ptr p_message) override {
			// From RAW to JSON
			const auto& l_payload = p_message->get_payload();
			std::vector<uint8_t> l_raw_message;
			l_raw_message.assign(l_payload.begin(), l_payload.end());
			const auto& l_json_message = cam::Message::to_json(l_raw_message);

			// Get data and message
			const auto& l_data = l_json_message["data"];

			// Do not process self messages
			if (l_json_message["emitter"] == m_environment.get_id()) {
				return;
			}

			// Execute commands
			const auto& l_topic = p_message->get_topic();
			if (l_topic == m_topics.m_post_broadcast_message_topic || l_topic == m_topics.m_post_message_topic) {
				const std::vector<uint8_t> l_message = l_data["message"];
				m_environment.send(l_data["sender_id"], l_data["receiver_id"], l_message.data(), l_message.size(),
								   l_data["binary_format"], true);

			} else if (l_topic == m_topics.m_post_broadcast_message_by_name_topic ||
					   l_topic == m_topics.m_post_message_by_name_topic) {
				const std::vector<uint8_t> l_message = l_data["message"];
				m_environment.send_by_name(l_data["sender_id"], l_data["receiver_name"], l_message.data(),
										   l_message.size(), l_data["is_fragment"], l_data["first_only"],
										   l_data["binary_format"], true);

			} else if (l_topic == m_topics.m_post_broadcast_message_broadcast_topic) {
				const std::vector<uint8_t> l_message = l_data["message"];
				m_environment.broadcast(l_data["sender_id"], l_message.data(), l_message.size(),
										l_data["binary_format"], true);

			} else if (l_topic == m_topics.m_post_agent_topic) {
				m_environment.on_new_agent(l_data);

			} else if (l_topic == m_topics.m_broadcast_discovery_topic) {
				m_environment.on_new_environment(static_cast<std::string>(l_data), true);

			} else if (l_topic == m_topics.m_callback_discovery_topic) {
				m_environment.on_new_environment(static_cast<std::string>(l_data), false);
			}

			m_environment.on_message(l_json_message, l_topic);
		}

		/**
		 * Called when delivery for a message has been completed, and all
		 * acknowledgments have been received.
		 * @param p_token the token
		 */
		void delivery_complete(mqtt::delivery_token_ptr p_token) override {
			m_environment.on_publish((p_token ? p_token->get_message_id() : -1));
		}
	};

	/**
	 * The MQTTClient wrapper
	 */
	class PahoWrapper : public mqtt::async_client {

	protected:

		/**
		 * Reference to the main class of the plugin
		 */
		cam::EnvironmentMas& m_environment;

		/**
		 * The server address.
		 */
		std::string m_server_address;

		/**
		 * An action listener to display the result of actions.
		 */
		SubscribeActionListener m_subscription_listener;

		/**
		 * Connection options.
		 */
		mqtt::connect_options m_connection_options;

		/**
		 * Callback.
		 */
		Callback* m_callback;

		/**
		 * If true, the connection parameter has been set at least once.
		 */
		bool m_connection_initialized_once;

		/**
 		 * Watchdog
 		 */
		std::future<void> m_watchdog;

		/**
		 * Watchdog is running
		 */
		bool m_running;

		/**
		 * Topics
		 */
		MQTTTopics m_topics;

	public:

		/**
		 * Constructor
		 * @param p_environment the environment
		 * @param p_id the client ID (if empty then use only the UUID)
		 * @param p_host the hostname or ip address of the broker to connect to
		 */
		PahoWrapper(cam::EnvironmentMas& p_environment, const std::string& p_id,
					const std::string& p_host = "tcp://localhost:1883") :
				mqtt::async_client(p_host, !p_id.empty() ? p_id + "_" + UUID::generate_uuid() : UUID::generate_uuid()),
				m_environment(p_environment),
				m_server_address(p_host),
				m_subscription_listener(p_environment),
				m_connection_options(),
				m_callback(nullptr),
				m_connection_initialized_once(false),
				m_running(true) {
		}

		/**
		 * Destructor
		 */
		~PahoWrapper() override {
			delete m_callback;
			m_running = false;
			m_watchdog.wait();
		}

		/**
		 * Set username & password
		 * @param p_username username
		 * @param p_password password
		 */
		void username_pw_set(const std::string& p_username, const std::string& p_password) {
			m_connection_options.set_user_name(p_username);
			m_connection_options.set_password(p_password);
		}

		/**
		 * Connect to the broker
		 * @param p_clean_session set to true to instruct the broker to clean all messages and subscriptions on disconnect, false to instruct it to keep them
		 * @param p_keep_alive keep alive time
		 * @param p_automatic_reconnect if true then automatic reconnect
		 * @return the reason code, if something wrong happen. 0 = OK (see https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901031)
		 */
		int connect_to_broker(const bool p_clean_session = true, const int p_keep_alive = 60,
							  const bool p_automatic_reconnect = true) {
			if (is_connected()) {
				return PAHO_ERR_ALREADY_CONNECTED;
			}
			m_connection_initialized_once = true;

			m_connection_options.set_clean_session(p_clean_session);
			m_connection_options.set_keep_alive_interval(p_keep_alive);
			m_connection_options.set_automatic_reconnect(p_automatic_reconnect);
			m_callback = new Callback(m_environment, m_topics);

			set_callback(*m_callback);

			try {
				const auto& l_token = connect(m_connection_options, nullptr, *m_callback);
				while (!is_connected());
				return l_token->get_reason_code();
			} catch (const mqtt::exception& p_exception) {
				return p_exception.get_reason_code();
			}
		}

		/**
		 * Reconnect to the broker
		 * @return the reason code, if something wrong happen. 0 = OK (see https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901031)
		 */
		int reconnect_to_broker() {
			if (is_connected()) {
				return PAHO_ERR_ALREADY_CONNECTED;
			}
			if (!m_connection_initialized_once) {
				return PAHO_ERR_CONNECTION_NOT_INITIALIZED_ONCE;
			}
			const auto& l_token = reconnect();
			return l_token->get_reason_code();
		}

		/**
		 * Connect to the broker
		 * @return the reason code, if something wrong happen. 0 = OK (see https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901031)
		 */
		int disconnect_to_broker() {
			if (!is_connected()) {
				return PAHO_ERR_NOT_CONNECTED;
			}
			const auto& l_token = disconnect();
			return l_token->get_reason_code();
		}

		/**
		 * Connect to the broker
		 * @param p_topic the name of the topic
		 * @param p_qos the QoS used
		 * @return the reason code, if something wrong happen. 0 = OK (see https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901031)
		 */
		int subscribe_to(const std::string& p_topic, const int p_qos = 1) {
			if (!is_connected()) {
				return PAHO_ERR_NOT_CONNECTED;
			}
			const auto& l_token = subscribe(p_topic, p_qos, nullptr, m_subscription_listener);
			return l_token->get_reason_code();
		}

		/**
		 * Unsubscribe to specific topic
		 * @param p_topic the name of the topic
		 * @return the reason code, if something wrong happen. 0 = OK (see https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901031)
		 */
		int unsubscribe_to(const std::string& p_topic) {
			if (!is_connected()) {
				return PAHO_ERR_NOT_CONNECTED;
			}
			const auto& l_token = unsubscribe(p_topic);
			if (!l_token->get_reason_code()) {
				m_environment.on_unsubscribe(l_token->get_message_id(), p_topic);
			}
			return l_token->get_reason_code();
		}

		/**
		 * Blocking unsubscribe to specific topic
		 * @param p_topic the name of the topic
		 * @return the reason code, if something wrong happen. 0 = OK (see https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901031)
		 */
		int unsubscribe_wait_to(const std::string& p_topic) {
			if (!is_connected()) {
				return PAHO_ERR_NOT_CONNECTED;
			}
			const auto& l_token = unsubscribe(p_topic);
			l_token->wait();
			if (!l_token->get_reason_code()) {
				m_environment.on_unsubscribe(l_token->get_message_id(), p_topic);
			}
			return l_token->get_reason_code();
		}

		/**
		 * Connect to the broker
		 * @param p_topic the name of the topic
		 * @param p_data data to send
		 * @param p_qos the QoS used
		 * @param p_retain if true the data is retained
		 * @return the reason code, if something wrong happen. 0 = OK (see https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901031)
		 */
		int publish_to(const std::string& p_topic, const std::string& p_data, const int p_qos = 1,
					   const bool p_retain = false) {
			if (!is_connected()) {
				return PAHO_ERR_NOT_CONNECTED;
			}
			const auto& l_token = publish(p_topic, p_data, p_qos, p_retain);
			return l_token->get_reason_code();
		}

		/**
		 * Connect to the broker
		 * @param p_topic the name of the topic
		 * @param p_data data to send
		 * @param p_qos the QoS used
		 * @param p_retain if true the data is retained
		 * @return the reason code, if something wrong happen. 0 = OK (see https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901031)
		 */
		int publish_to(const std::string& p_topic, const std::vector<uint8_t>& p_data, const int p_qos = 1,
					   const bool p_retain = false) {
			if (!is_connected()) {
				return PAHO_ERR_NOT_CONNECTED;
			}
			const auto& l_token = publish(p_topic, &p_data[0], p_data.size(), p_qos, p_retain);
			return l_token->get_reason_code();
		}

		/**
		 * Format message
		 * @return formated message
		 */
		json format_message(const json& p_data, const std::string& p_type, const std::string& p_unit) {
			json l_formated_message;

			l_formated_message["data"] = p_data;
			l_formated_message["type"] = p_type;
			l_formated_message["unit"] = p_unit;
			l_formated_message["timestamp"] = std::to_string(duration_cast<std::chrono::milliseconds>(
					std::chrono::system_clock::now().time_since_epoch()).count());
			l_formated_message["emitter"] = mqtt::async_client::get_client_id();

			return l_formated_message;
		}

		/**
		 * Get ID
		 * @return ID
		 */
		[[nodiscard]] std::string get_id() {
			return mqtt::async_client::get_client_id();
		}

		/**
		 * Get string reason code
		 * @param p_rc the reason code
		 * @return the string reason code
		 */
		static std::string reason_code_string(const int p_rc) {
			if (p_rc == PAHO_ERR_NOT_INIT) {
				return "MQTT client is not initialized, initialise() must be call first";
			} else if (p_rc == PAHO_ERR_CONNECTION_NOT_INITIALIZED_ONCE) {
				return "MQTT client is not completely initialized, connect() must be called at least once";
			} else if (p_rc == PAHO_ERR_ALREADY_CONNECTED) {
				return "MQTT client is already connected";
			} else if (p_rc == PAHO_ERR_NOT_CONNECTED) {
				return "MQTT client is not connected";
			}
			return mqtt::exception::reason_code_str(p_rc);
		}

		/**
		 * Create a new paho client instance
		 * @return the reason code, if something wrong happen. 0 = OK (see https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901031)
		 */
		int initialise_remote_connection_by_address() {
			try {
				const auto& l_rc = connect_to_broker();

				// Create topics
				m_topics.m_post_message_topic = m_environment.get_id() + "/post/message";
				m_topics.m_post_message_by_name_topic = m_environment.get_id() + "/post/message/by_name";
				m_topics.m_post_agent_topic = m_environment.get_id() + "/post/agent";
				m_topics.m_callback_discovery_topic = m_environment.get_id() + "/discovery";
				m_topics.m_post_broadcast_message_topic = "post/message";
				m_topics.m_post_broadcast_message_by_name_topic = "post/message/by_name";
				m_topics.m_post_broadcast_message_broadcast_topic = "post/message/broadcast";
				m_topics.m_broadcast_discovery_topic = "discovery";

				// Message
				subscribe_to(m_topics.m_post_message_topic);
				subscribe_to(m_topics.m_post_message_by_name_topic);

				// Agent move
				subscribe_to(m_topics.m_post_agent_topic);

				// Message broadcast
				subscribe_to(m_topics.m_post_broadcast_message_topic);
				subscribe_to(m_topics.m_post_broadcast_message_by_name_topic);
				subscribe_to(m_topics.m_post_broadcast_message_broadcast_topic);

				// Discovery
				subscribe_to(m_topics.m_callback_discovery_topic);
				subscribe_to(m_topics.m_broadcast_discovery_topic);

				const json& l_data = get_id();
				const auto& l_message = format_message(l_data, "string", "environment_id");
				publish_to(m_topics.m_broadcast_discovery_topic, cam::Message::to_binary(l_message));

				m_watchdog = std::async(std::launch::async, [&] {
					do {
						if (m_running) {
							m_environment.send_discovery_callback();
						}
						std::this_thread::sleep_for(std::chrono::milliseconds(100));
					} while (m_running);
				});

				return l_rc;
			} catch (std::exception& p_exception) {
				return static_cast<int>(strtol(p_exception.what(), nullptr, 10));
			}
		}

		void broadcast_message_by_name(const std::vector<uint8_t>& p_binary_message) {
			publish_to("post/message/by_name", p_binary_message);
		}

		void send_message_by_id(const std::vector<uint8_t>& p_binary_message) {
			publish_to("post/message", p_binary_message);
		}

		void broadcast_message(const std::vector<uint8_t>& p_binary_message) {
			publish_to("post/message/broadcast", p_binary_message);
		}

		void post_agent(const std::string& p_environment_id, const std::vector<uint8_t>& p_binary_message) {
			publish_to(p_environment_id + "/post/agent", p_binary_message);
		}

		void send_callback_discovery(const std::string& p_environment_id, const std::vector<uint8_t>& p_binary_message) {
			publish_to(p_environment_id + "/discovery", p_binary_message);
		}
	};
} // namespace cam
