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

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>

#include "AgentId.h"
#include "util/RecyclingAllocator.hpp"

using json = nlohmann::json;

/**
 * CPPActressMAS
 */
namespace cam {

	/**
	 * Encoding of a message content on the wire: RAW for opaque bytes, Text for UTF-8 text, JSON for
	 * JSON text, the others for the binary encodings of a JSON value. A local message is never encoded.
	 */
	enum class MessageBinaryFormat {
		RAW, BJData, BSON, CBOR, MessagePack, UBJSON, Text, JSON
	};

	/**
	 * What a message carries.
	 */
	enum class ContentKind {
		None, Text, Json, Bytes
	};

	/**
	 * Immutable text shared between messages, like a C# string: sending it to many agents, or many times, copies a
	 * pointer instead of the characters. Optional: a std::string or a literal is sent as before.
	 **/
	class Text final {
	private:
		/**
		 * The characters, shared.
		 **/
		std::shared_ptr<const std::string> m_text;

	public:
		/**
		 * Text from its characters.
		 * @param p_text The characters
		 **/
		explicit Text(std::string p_text);

		/**
		 * Text from a C string.
		 * @param p_text The characters (null gives an empty text)
		 **/
		explicit Text(const char* p_text);

		/**
		 * The characters.
		 * @return The characters
		 **/
		[[nodiscard]] const std::string& str() const noexcept { return *m_text; }
	};

	/**
	 * Content of a message: nothing, UTF-8 text, an immutable JSON value shared between the receivers of
	 * a broadcast, opaque bytes, or a shared text (read as text: kind() Text, as_text()).
	 **/
	using MessageContent = std::variant<std::monostate, std::string, std::shared_ptr<const json>, std::vector<std::uint8_t>, Text>;

	/**
	 * A message that the agents use to communicate. In an agent-based system, the
	 * communication between the agents is exclusively performed by exchanging
	 * messages. The content stays typed as long as the message is local; the
	 * transport encodes it once when it crosses the network.
	 **/
	class Environment;
	class MessagePointer;

	class Message final {
		friend Environment;
		friend MessagePointer;

	protected:
		/**
		 * Number of MessagePointer sharing the message, atomic: a message is often dropped by another thread than the
		 * one that built it.
		 **/
		mutable std::atomic<std::uint32_t> m_references{0};

		/**
		 * Sender.
		 **/
		AgentId m_sender;

		/**
		 * Environment the sender belongs to when the message came through the transport; empty for a
		 * local message.
		 **/
		std::string m_sender_environment;

		/**
		 * Receiver.
		 **/
		AgentId m_receiver;

		/**
		 * Content.
		 **/
		MessageContent m_content;

	public:
		/**
		 * Key of the constructor kept for the environment: only the environment can make one.
		 **/
		class Key final {
			friend Environment;

			Key() = default;
		};

		/**
		 * Message built by the environment: identifiers copied once, content moved in.
		 * @param p_key Key of the environment.
		 * @param p_sender Sender.
		 * @param p_receiver Receiver.
		 * @param p_content Content.
		 * @param p_sender_environment Environment of the sender when the message came through the transport, else empty.
		 **/
		Message(Key p_key, const AgentId& p_sender, const AgentId& p_receiver, MessageContent&& p_content,
				const std::string& p_sender_environment);

		/**
		 * Message carrying a JSON value.
		 * @param p_sender Sender.
		 * @param p_receiver Receiver.
		 * @param p_message JSON value.
		 **/
		Message(AgentId p_sender, AgentId p_receiver, const json& p_message);

		/**
		 * Message carrying a JSON value already shared.
		 * @param p_sender Sender.
		 * @param p_receiver Receiver.
		 * @param p_message Shared JSON value.
		 **/
		Message(AgentId p_sender, AgentId p_receiver, std::shared_ptr<const json> p_message);

		/**
		 * Message carrying text.
		 * @param p_sender Sender.
		 * @param p_receiver Receiver.
		 * @param p_text UTF-8 text.
		 **/
		Message(AgentId p_sender, AgentId p_receiver, std::string p_text);

		/**
		 * Message carrying opaque bytes.
		 * @param p_sender Sender.
		 * @param p_receiver Receiver.
		 * @param p_message Bytes (may be null when the length is 0).
		 * @param p_length Number of bytes.
		 **/
		Message(AgentId p_sender, AgentId p_receiver, const uint8_t* p_message, const size_t& p_length);

		/**
		 * Message carrying any content.
		 * @param p_sender Sender.
		 * @param p_receiver Receiver.
		 * @param p_content Content.
		 **/
		Message(AgentId p_sender, AgentId p_receiver, MessageContent p_content);

		Message();

		/**
		 * Nothing to delete.
		 **/
		/*virtual*/ ~Message() = default;

		/**
		 * Get sender.
		 * @return Sender.
		 **/
		[[nodiscard]] const AgentId& get_sender() const { return m_sender; }

		/**
		 * Get receiver.
		 * @return Receiver.
		 **/
		[[nodiscard]] const AgentId& get_receiver() const { return m_receiver; }

		/**
		 * Get the environment of the sender: empty for a local message, the id of the origin environment
		 * for a message received through the transport.
		 * @return Environment id or empty
		 **/
		[[nodiscard]] const std::string& get_sender_environment() const { return m_sender_environment; }

		/**
		 * Kind of content carried.
		 * @return The kind
		 **/
		[[nodiscard]] ContentKind kind() const;

		/**
		 * The content as it was given.
		 * @return The content
		 **/
		[[nodiscard]] const MessageContent& get_content() const { return m_content; }

		/**
		 * Text content; empty when the message carries something else (see kind()).
		 * @return The text
		 **/
		[[nodiscard]] const std::string& as_text() const;

		/**
		 * JSON content; null when the message carries something else (see kind()).
		 * @return The JSON value
		 **/
		[[nodiscard]] const json& as_json() const;

		/**
		 * Bytes content; empty when the message carries something else (see kind()).
		 * @return The bytes
		 **/
		[[nodiscard]] const std::vector<std::uint8_t>& as_bytes() const;

		/**
		 * Get the content as a JSON value: the value itself, a JSON string for text and for bytes, null
		 * for an empty message.
		 * @return message JSON
		 **/
		[[nodiscard]] json content() const;

		/**
		 * Format message to string: the text itself, the JSON dump, the bytes as characters.
		 * @return string message
		 **/
		[[nodiscard]] std::string to_string() const;

		/**
		 * Format message: sender and receiver ids (Agent::format gives their names, as the C#).
		 * @return string formated message
		 **/
		[[nodiscard]] /*virtual*/ std::string format() const;

		/**
		 * As Parse(out action, out List<string> parameters) of the C#: the content split at every white
		 * space (Unicode, as String.Split() without argument), empty words kept; the first word is the action.
		 * @return The action and the other words
		 **/
		[[nodiscard]] std::pair<std::string, std::vector<std::string>> parse() const;

		/**
		 * As Parse(out action, out string parameters) of the C#: the action and the other words joined by single spaces.
		 * @return The action and the rest
		 **/
		[[nodiscard]] std::pair<std::string, std::string> parse_rest() const;

		/**
		 * As Parse1P(out action, out parameter) of the C#: the action and the second word.
		 * @return The action and the parameter
		 * @throw std::out_of_range If the content has a single word (IndexOutOfRangeException in C#)
		 **/
		[[nodiscard]] std::pair<std::string, std::string> parse_one() const;

		// Delete copy constructor
		Message(const Message&) = delete;

		Message& operator=(Message&) = delete;

		/**
		 * Memory of a message built on the heap, from the free blocks of the current thread.
		 * @param p_size Size of the message
		 * @return The memory
		 **/
		static void* operator new(std::size_t p_size);

		/**
		 * Give the memory of a message back to the free blocks of the current thread.
		 * @param p_pointer The memory
		 **/
		static void operator delete(void* p_pointer) noexcept;
	};

	/**
	 * Shared pointer to a message, the counter inside the message: a copy changes one atomic counter, and the
	 * message with its counter is one block, recycled per thread. Only the environment makes one; an agent receives,
	 * copies, keeps and drops them.
	 **/
	class MessagePointer final {
		friend Environment;

	private:
		/**
		 * The message, or null.
		 **/
		const Message* m_message = nullptr;

		/**
		 * Own a message just built on the heap, not shared yet (environment only): its counter is set, not incremented.
		 * @param p_message The message
		 **/
		explicit MessagePointer(const Message* p_message) noexcept :
				m_message(p_message) {
			if (m_message) {
				m_message->m_references.store(1, std::memory_order_relaxed);
			}
		}

		/**
		 * Share the message of another pointer.
		 **/
		void share() const noexcept {
			if (m_message) {
				m_message->m_references.fetch_add(1, std::memory_order_relaxed);
			}
		}

		/**
		 * Drop the share of this pointer: the last one deletes the message. A sole owner (counter at 1) cannot be
		 * raced, since no other pointer exists to copy it: it deletes without an atomic decrement.
		 **/
		void release() noexcept {
			if (!m_message) {
				return;
			}
			if (m_message->m_references.load(std::memory_order_acquire) == 1 ||
				m_message->m_references.fetch_sub(1, std::memory_order_acq_rel) == 1) {
				delete m_message;
			}
		}

	public:
		/**
		 * Null pointer.
		 **/
		MessagePointer() noexcept = default;

		/**
		 * Null pointer.
		 **/
		MessagePointer(std::nullptr_t) noexcept {}

		/**
		 * Share the message of another pointer.
		 * @param p_other The other pointer
		 **/
		MessagePointer(const MessagePointer& p_other) noexcept :
				m_message(p_other.m_message) {
			share();
		}

		/**
		 * Take the message of another pointer, which becomes null.
		 * @param p_other The other pointer
		 **/
		MessagePointer(MessagePointer&& p_other) noexcept :
				m_message(std::exchange(p_other.m_message, nullptr)) {}

		/**
		 * Share the message of another pointer.
		 * @param p_other The other pointer
		 * @return This pointer
		 **/
		MessagePointer& operator=(const MessagePointer& p_other) noexcept {
			MessagePointer(p_other).swap(*this);
			return *this;
		}

		/**
		 * Take the message of another pointer, which becomes null.
		 * @param p_other The other pointer
		 * @return This pointer
		 **/
		MessagePointer& operator=(MessagePointer&& p_other) noexcept {
			MessagePointer(std::move(p_other)).swap(*this);
			return *this;
		}

		/**
		 * Drop the share of this pointer.
		 **/
		~MessagePointer() {
			release();
		}

		/**
		 * Exchange the messages of two pointers.
		 * @param p_other The other pointer
		 **/
		void swap(MessagePointer& p_other) noexcept {
			std::swap(m_message, p_other.m_message);
		}

		/**
		 * Drop the share of this pointer, which becomes null.
		 **/
		void reset() noexcept {
			release();
			m_message = nullptr;
		}

		/**
		 * The message.
		 * @return The message, or null
		 **/
		[[nodiscard]] const Message* get() const noexcept { return m_message; }

		/**
		 * The message.
		 * @return The message
		 **/
		const Message* operator->() const noexcept { return m_message; }

		/**
		 * The message.
		 * @return The message
		 **/
		const Message& operator*() const noexcept { return *m_message; }

		/**
		 * True if the pointer has a message.
		 * @return True if not null
		 **/
		explicit operator bool() const noexcept { return m_message != nullptr; }

		/**
		 * True if both pointers share the same message.
		 * @param p_left A pointer
		 * @param p_right Another pointer
		 * @return True if the same message
		 **/
		friend bool operator==(const MessagePointer& p_left, const MessagePointer& p_right) noexcept {
			return p_left.m_message == p_right.m_message;
		}

		/**
		 * True if the pointer is null.
		 * @param p_pointer A pointer
		 * @return True if null
		 **/
		friend bool operator==(const MessagePointer& p_pointer, std::nullptr_t) noexcept {
			return p_pointer.m_message == nullptr;
		}
	};
} // namespace cam
