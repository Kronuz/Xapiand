/*
 * Copyright (C) 2015,2016 deipi.com LLC and contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "discovery.h"

#ifdef XAPIAND_CLUSTERING

#include <assert.h>


constexpr const char* const Discovery::MessageNames[];


Discovery::Discovery(const std::shared_ptr<XapiandManager>& manager_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, int port_, const std::string& group_)
	: BaseUDP(manager_, ev_loop_, ev_flags_, port_, "Discovery", XAPIAND_DISCOVERY_PROTOCOL_VERSION, group_),
	  heartbeat(*ev_loop),
	  async_enter(*ev_loop)
{
	heartbeat.set<Discovery, &Discovery::heartbeat_cb>(this);

	async_enter.set<Discovery, &Discovery::async_enter_cb>(this);
	async_enter.start();
	L_EV(this, "Start discovery's async enter event");

	L_OBJ(this, "CREATED DISCOVERY");
}


Discovery::~Discovery()
{
	heartbeat.stop();
	L_EV(this, "Stop discovery's heartbeat event");

	L_OBJ(this, "DELETED DISCOVERY");
}


void
Discovery::start() {
	heartbeat.repeat = HEARTBEAT_EXPLORE;
	heartbeat.again();
	L_EV(this, "Start discovery's heartbeat exploring event (%f)", heartbeat.repeat);

	L_DISCOVERY(this, "Discovery was started! (exploring)");
}


void
Discovery::stop() {
	heartbeat.stop();
	L_EV(this, "Stop discovery's heartbeat event");

	auto node = std::atomic_load(&local_node);
	send_message(Message::BYE, node->serialise());

	L_DISCOVERY(this, "Discovery was stopped!");
}


void
Discovery::async_enter_cb(ev::async&, int)
{
	_enter();
}


void
Discovery::_enter()
{
	auto node = std::atomic_load(&local_node);
	send_message(Message::ENTER, node->serialise());

	heartbeat.repeat = random_real(HEARTBEAT_MIN, HEARTBEAT_MAX);
	heartbeat.again();
	L_EV(this, "Reset discovery's heartbeat event (%f)", heartbeat.repeat);

	L_DISCOVERY(this, "Discovery was started! (heartbeat)");
}


void
Discovery::heartbeat_cb(ev::timer&, int)
{
	L_EV(this, "Discovery::heartbeat_cb");

	L_EV_BEGIN(this, "Discovery::heartbeat_cb:BEGIN");

	if (XapiandManager::manager->state.load() != XapiandManager::State::READY) {
		L_DISCOVERY(this, "Waiting manager get ready!! (%s)", XapiandManager::StateNames[static_cast<int>(XapiandManager::manager->state.load())]);
	}

	switch (XapiandManager::manager->state.load()) {
		case XapiandManager::State::RESET: {
			auto local_node_ = std::atomic_load(&local_node);
			auto node_copy = std::make_unique<Node>(*local_node_);
			std::string drop = node_copy->name;

			if (XapiandManager::manager->node_name.empty()) {
				node_copy->name = name_generator();
			} else {
				node_copy->name = XapiandManager::manager->node_name;
			}
			std::atomic_store(&local_node, std::shared_ptr<const Node>(node_copy.release()));

			if (!drop.empty()) {
				XapiandManager::manager->drop_node(drop);
			}

			local_node_ = std::atomic_load(&local_node);
			L_INFO(this, "Advertising as %s (id: %016llX)...", local_node_->name.c_str(), local_node_->id);
			send_message(Message::HELLO, local_node_->serialise());
			XapiandManager::manager->state.store(XapiandManager::State::WAITING);
			break;
		}

		case XapiandManager::State::WAITING:
			XapiandManager::manager->state.store(XapiandManager::State::WAITING_);
			break;

		case XapiandManager::State::WAITING_:
			XapiandManager::manager->state.store(XapiandManager::State::SETUP);
			break;

		case XapiandManager::State::SETUP:
			XapiandManager::manager->setup_node();
			break;

		case XapiandManager::State::READY:
		{
			auto local_node_ = std::atomic_load(&local_node);
			send_message(Message::HEARTBEAT, local_node_->serialise());
			break;
		}

		case XapiandManager::State::BAD:
			L_ERR(this, "ERROR: Manager is in BAD state!!");
			break;
	}

	L_EV_END(this, "Discovery::heartbeat_cb:END");
}


std::string
Discovery::getDescription() const noexcept
{
	return "UDP:" + std::to_string(port) + " (" + description + " v" + std::to_string(XAPIAND_DISCOVERY_PROTOCOL_MAJOR_VERSION) + "." + std::to_string(XAPIAND_DISCOVERY_PROTOCOL_MINOR_VERSION) + ")";
}

#endif
