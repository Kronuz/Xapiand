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

#include "binary.h"

#ifdef XAPIAND_CLUSTERING

#include <netinet/tcp.h>                      // for TCP_NODELAY

#include "binary_server.h"                    // For BinaryServer
#include "endpoint.h"                         // for Endpoint
#include "io.hh"                              // for io::*
#include "node.h"                             // for Node, local_node
#include "remote_protocol.h"                  // for XAPIAN_REMOTE_PROTOCOL_MAJOR_VERSION, XAPIAN_REMOTE_PROTOCOL_MAINOR_VERSION


// #undef L_DEBUG
// #define L_DEBUG L_GREY
// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY


Binary::Binary(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, int port_)
	: BaseTCP(parent_, ev_loop_, ev_flags_, port_, "Binary", TCP_TCP_NODELAY)
{
	bind(port_ == XAPIAND_BINARY_SERVERPORT ? 10 : 1);
	auto local_node = Node::local_node();
	auto node_copy = std::make_unique<Node>(*local_node);
	node_copy->binary_port = port;
	Node::local_node(std::shared_ptr<const Node>(node_copy.release()));
}


void
Binary::start()
{
	L_CALL("Binary::start()");

	auto weak_children = gather_children();
	for (auto& weak_child : weak_children) {
		if (auto child = weak_child.lock()) {
			std::static_pointer_cast<BinaryServer>(child)->start();
		}
	}
}


void
Binary::trigger_replication(const TriggerReplicationArgs& args)
{
	L_CALL("Binary::trigger_replication(%s, %s, %s)", repr(src_endpoint.to_string()), repr(dst_endpoint.to_string()), cluster_database ? "true" : "false");

	trigger_replication_args.enqueue(args);

	auto weak_children = gather_children();
	for (auto& weak_child : weak_children) {
		if (auto child = weak_child.lock()) {
			std::static_pointer_cast<BinaryServer>(child)->trigger_replication();
		}
	}
}


std::string
Binary::__repr__() const
{
	return string::format("<Binary {cnt:%ld}%s%s%s>",
		use_count(),
		is_runner() ? " (runner)" : " (worker)",
		is_running_loop() ? " (running loop)" : " (stopped loop)",
		is_detaching() ? " (deteaching)" : "");
}


std::string
Binary::getDescription() const
{
	std::string proxy((port == XAPIAND_BINARY_SERVERPORT && XAPIAND_BINARY_SERVERPORT != XAPIAND_BINARY_PROXY) ? "->" + std::to_string(XAPIAND_BINARY_PROXY) : "");
	return "TCP:" + std::to_string(port) + proxy + " (xapian v" + std::to_string(XAPIAN_REMOTE_PROTOCOL_MAJOR_VERSION) + "." + std::to_string(XAPIAN_REMOTE_PROTOCOL_MINOR_VERSION) + ")";
}

#endif  /* XAPIAND_CLUSTERING */
