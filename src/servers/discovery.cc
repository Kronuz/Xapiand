/*
 * Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "discovery.h"

#ifdef XAPIAND_CLUSTERING

#include "endpoint.h"
#include "ignore_unused.h"
#include "manager.h"


Discovery::Discovery(const std::shared_ptr<XapiandManager>& manager_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, int port_, const std::string& group_)
	: BaseUDP(manager_, ev_loop_, ev_flags_, port_, "Discovery", XAPIAND_DISCOVERY_PROTOCOL_VERSION, group_),
	  heartbeat(*ev_loop),
	  enter_async(*ev_loop),
	  wait_longer_async(*ev_loop)
{
	heartbeat.set<Discovery, &Discovery::heartbeat_cb>(this);

	enter_async.set<Discovery, &Discovery::enter_async_cb>(this);
	enter_async.start();
	L_EV("Start discovery's async enter event");

	wait_longer_async.set<Discovery, &Discovery::wait_longer_async_cb>(this);
	wait_longer_async.start();
	L_EV("Start discovery's async wait_longer event");

	L_OBJ("CREATED DISCOVERY");
}


Discovery::~Discovery()
{
	heartbeat.stop();
	L_EV("Stop discovery's heartbeat event");

	L_OBJ("DELETED DISCOVERY");
}


void
Discovery::start() {
	heartbeat.start(0, WAITING_FAST);
	L_EV("Start discovery's heartbeat exploring event (%f)", heartbeat.repeat);

	L_DISCOVERY("Discovery was started! (exploring)");
}


void
Discovery::stop() {
	heartbeat.stop();
	L_EV("Stop discovery's heartbeat event");

	auto local_node_ = local_node.load();
	send_message(Message::BYE, local_node_->serialise());

	L_DISCOVERY("Discovery was stopped!");
}


void
Discovery::enter_async_cb(ev::async&, int revents)
{
	L_CALL("Discovery::enter_async_cb(<watcher>, 0x%x (%s))", revents, readable_revents(revents));

	ignore_unused(revents);

	_enter();
}


void
Discovery::wait_longer_async_cb(ev::async&, int revents)
{
	L_CALL("Discovery::enter_async_cb(<watcher>, 0x%x (%s))", revents, readable_revents(revents));

	ignore_unused(revents);

	_wait_longer();
}


void
Discovery::_enter()
{
	auto local_node_ = local_node.load();
	send_message(Message::ENTER, local_node_->serialise());

	heartbeat.repeat = random_real(HEARTBEAT_MIN, HEARTBEAT_MAX);
	heartbeat.again();
	L_EV("Reset discovery's heartbeat event (%f)", heartbeat.repeat);

	L_DISCOVERY("Discovery was started! (heartbeat)");
}


void
Discovery::_wait_longer()
{
	heartbeat.repeat = WAITING_SLOW;
	heartbeat.again();
}


void
Discovery::heartbeat_cb(ev::timer&, int revents)
{
	L_CALL("Discovery::heartbeat_cb(<watcher>, 0x%x (%s))", revents, readable_revents(revents));

	ignore_unused(revents);

	L_EV_BEGIN("Discovery::heartbeat_cb:BEGIN");

	auto local_node_ = local_node.load();
	send_message(Message::HEARTBEAT, local_node_->serialise());

	L_EV_END("Discovery::heartbeat_cb:END");
}


void
Discovery::send_message(Message type, const std::string& message)
{
	if (type != Discovery::Message::HEARTBEAT) {
		L_DISCOVERY("<< send_message(%s)", MessageNames(type));
		L_DISCOVERY_PROTO("message: %s", repr(message));
	}
	BaseUDP::send_message(toUType(type), message);
}


std::string
Discovery::getDescription() const noexcept
{
	return "UDP:" + std::to_string(port) + " (" + description + " v" + std::to_string(XAPIAND_DISCOVERY_PROTOCOL_MAJOR_VERSION) + "." + std::to_string(XAPIAND_DISCOVERY_PROTOCOL_MINOR_VERSION) + ")";
}

#endif
