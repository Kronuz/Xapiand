/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
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

// Hosts over loopback TLS for the cluster tests: a CA and node certificates made in process
// (ECDSA P-256), each host's loops on their own threads, its transport pinned to every node.

#pragma once

#include <chrono>                                    // for std::chrono
#include <cstdio>                                    // for std::fprintf
#include <filesystem>                                // for std::filesystem
#include <future>                                    // for std::packaged_task
#include <map>                                       // for std::map
#include <memory>                                    // for std::unique_ptr
#include <stdexcept>                                 // for std::runtime_error
#include <string>                                    // for std::string
#include <thread>                                    // for std::thread
#include <vector>                                    // for std::vector

#include <asio/executor_work_guard.hpp>
#include <asio/io_context.hpp>
#include <consensus/transport/host_transport.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>

namespace cluster_test {

using namespace kronuz::consensus::transport;
using kronuz::consensus::Identity;
using kronuz::consensus::Member;
using kronuz::consensus::MemberRole;
using kronuz::consensus::NodeId;
using namespace std::chrono_literals;
using steady = std::chrono::steady_clock;

// Certificates in `dir`: ca.pem, and nodeN.pem with nodeN.key for each node; rogue-ca signs
// rogue.pem, which no host pins.
class Certificates {
  public:
	Certificates(std::filesystem::path dir, NodeId nodes) : dir_(std::move(dir)) {
		std::filesystem::create_directories(dir_);
		auto ca_key = key();
		auto ca = make(ca_key.get(), nullptr, ca_key.get(), "Xapiand-Test-CA", true);
		write(ca.get(), nullptr, "ca");
		for (NodeId n = 1; n <= nodes; ++n) {
			auto k = key();
			auto leaf = make(k.get(), ca.get(), ca_key.get(), "node" + std::to_string(n), false);
			write(leaf.get(), k.get(), "node" + std::to_string(n));
		}
		auto rogue_ca_key = key();
		auto rogue_ca = make(rogue_ca_key.get(), nullptr, rogue_ca_key.get(), "Rogue-CA", true);
		auto rogue_key = key();
		auto rogue = make(rogue_key.get(), rogue_ca.get(), rogue_ca_key.get(), "rogue", false);
		write(rogue.get(), rogue_key.get(), "rogue");
	}
	std::filesystem::path path(const std::string& name) const { return dir_ / name; }
	Fingerprint pin(const std::string& name) const {
		std::unique_ptr<BIO, decltype(&BIO_free)> in(BIO_new_file(path(name + ".pem").c_str(), "r"), BIO_free);
		std::unique_ptr<X509, decltype(&X509_free)> leaf(PEM_read_bio_X509(in.get(), nullptr, nullptr, nullptr), X509_free);
		if (!leaf)
			throw std::runtime_error("certificates: cannot read " + name);
		return certificate_fingerprint(leaf.get());
	}
	std::shared_ptr<asio::ssl::context> context(const std::string& name) const {
		auto c = std::make_shared<asio::ssl::context>(asio::ssl::context::tls);
		reactor::tls::configure(*c, {path(name + ".pem").string(), path(name + ".key").string(), path("ca.pem").string()},
								reactor::tls::Role::Peer);
		return c;
	}

  private:
	using Key = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
	using Cert = std::unique_ptr<X509, decltype(&X509_free)>;
	static Key key() {
		Key k(EVP_EC_gen("P-256"), EVP_PKEY_free);
		if (!k)
			throw std::runtime_error("certificates: key");
		return k;
	}
	static void extension(X509* cert, X509* issuer, int nid, const char* value) {
		X509V3_CTX ctx;
		X509V3_set_ctx(&ctx, issuer ? issuer : cert, cert, nullptr, nullptr, 0);
		X509_EXTENSION* ext = X509V3_EXT_conf_nid(nullptr, &ctx, nid, value);
		if (!ext || !X509_add_ext(cert, ext, -1))
			throw std::runtime_error("certificates: extension");
		X509_EXTENSION_free(ext);
	}
	Cert make(EVP_PKEY* key, X509* issuer, EVP_PKEY* issuer_key, const std::string& cn, bool ca) {
		Cert owned(X509_new(), X509_free);
		X509* cert = owned.get();
		X509_set_version(cert, 2);
		ASN1_INTEGER_set(X509_get_serialNumber(cert), ++serial_);
		X509_gmtime_adj(X509_getm_notBefore(cert), -3600);
		X509_gmtime_adj(X509_getm_notAfter(cert), 86400);
		X509_set_pubkey(cert, key);
		std::unique_ptr<X509_NAME, decltype(&X509_NAME_free)> name(X509_NAME_new(), X509_NAME_free);
		X509_NAME_add_entry_by_txt(name.get(), "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char*>(cn.c_str()), -1, -1, 0);
		X509_set_subject_name(cert, name.get());
		X509_set_issuer_name(cert, issuer ? X509_get_subject_name(issuer) : name.get());
		extension(cert, issuer, NID_subject_key_identifier, "hash");
		if (ca) {
			extension(cert, issuer, NID_basic_constraints, "critical,CA:TRUE");
			extension(cert, issuer, NID_key_usage, "critical,keyCertSign,cRLSign");
		} else {
			extension(cert, issuer, NID_basic_constraints, "critical,CA:FALSE");
			extension(cert, issuer, NID_key_usage, "critical,digitalSignature");
			extension(cert, issuer, NID_ext_key_usage, "serverAuth,clientAuth");
			extension(cert, issuer, NID_subject_alt_name, "DNS:localhost");
			extension(cert, issuer, NID_authority_key_identifier, "keyid");
		}
		if (!X509_sign(cert, issuer_key, EVP_sha256()))
			throw std::runtime_error("certificates: sign");
		return owned;
	}
	void write(X509* cert, EVP_PKEY* key, const std::string& name) {
		std::unique_ptr<BIO, decltype(&BIO_free)> out(BIO_new_file(path(name + ".pem").c_str(), "w"), BIO_free);
		PEM_write_bio_X509(out.get(), cert);
		if (key) {
			std::unique_ptr<BIO, decltype(&BIO_free)> k(BIO_new_file(path(name + ".key").c_str(), "w"), BIO_free);
			PEM_write_bio_PrivateKey(k.get(), key, nullptr, nullptr, 0, nullptr, nullptr);
			std::filesystem::permissions(path(name + ".key"), std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
		}
	}
	std::filesystem::path dir_;
	long serial_ = 0;
};

struct Loop {
	asio::io_context io;
	task::asio_scheduler scheduler{io.get_executor()};
	asio::executor_work_guard<asio::io_context::executor_type> work = asio::make_work_guard(io);
	std::thread thread;
	void start() {
		thread = std::thread([this] { io.run(); });
	}
	void join() {
		work.reset();
		io.stop();
		if (thread.joinable())
			thread.join();
	}
	template <class F> auto at(F f) -> decltype(f()) {
		std::packaged_task<decltype(f())()> call(std::move(f));
		auto result = call.get_future();
		asio::post(io, [&call] { call(); });
		return result.get();
	}
	~Loop() { join(); }
};

// One node: its loops, acceptor and transport. open(), then the caller installs handlers, then start().
struct Host {
	NodeId node;
	std::vector<std::unique_ptr<Loop>> loops;
	asio::ip::tcp::acceptor acceptor;
	std::unique_ptr<HostTransport> transport;
	bool running = false;
	Host(NodeId n, std::uint32_t L = 1) : node(n), loops(make(L)), acceptor(loops[0]->io, {asio::ip::address_v4::loopback(), 0}) {}
	static std::vector<std::unique_ptr<Loop>> make(std::uint32_t L) {
		std::vector<std::unique_ptr<Loop>> loops;
		for (std::uint32_t k = 0; k != L; ++k)
			loops.push_back(std::make_unique<Loop>());
		return loops;
	}
	std::uint16_t port() const { return acceptor.local_endpoint().port(); }
	std::vector<asio::any_io_executor> executors() const {
		std::vector<asio::any_io_executor> r;
		for (auto& l : loops)
			r.push_back(l->io.get_executor());
		return r;
	}
	void open(const Certificates& certs, NodeId nodes, const std::map<NodeId, PeerAddress>& addresses) {
		auto tls = certs.context("node" + std::to_string(node));
		std::map<NodeId, Fingerprint> pins;
		for (NodeId n = 1; n <= nodes; ++n)
			pins[n] = certs.pin("node" + std::to_string(n));
		HostTransportOptions options;
		options.member_of = [](GroupId, NodeId) -> std::optional<Member> { return std::nullopt; };
		std::vector<HostLoop> at;
		for (auto& l : loops)
			at.push_back({l->io.get_executor(), task::scheduler_ref::of(l->scheduler)});
		Identity cluster{};
		cluster.fill('X');
		transport = std::make_unique<HostTransport>(at, tls, NodePins(node, pins, tls->native_handle()), cluster, addresses, options);
	}
	void start() {
		transport->serve(acceptor);
		for (auto& l : loops)
			l->start();
		running = true;
	}
	// Runs f on the loop of node `peer`'s connections and waits for it.
	template <class F> auto at(NodeId peer, F f) { return loops[transport->loop_of_node(peer)]->at(std::move(f)); }
	void stop() {
		if (!running)
			return;
		transport->stop();
		for (std::uint32_t k = 0; k != loops.size(); ++k)
			for (int i = 0; !loops[k]->at([&] { return transport->drained(k); }); ++i) {
				if (i > 2000)
					throw std::runtime_error("host: transport did not drain");
				std::this_thread::sleep_for(5ms);
			}
		for (auto& l : loops)
			l->join();
		running = false;
	}
	~Host() {
		try {
			stop();
		} catch (const std::exception& e) {
			std::fprintf(stderr, "host %llu: %s\n", static_cast<unsigned long long>(node), e.what());
		}
	}
};

inline std::map<NodeId, PeerAddress> addresses(std::initializer_list<const Host*> hosts) {
	std::map<NodeId, PeerAddress> r;
	for (auto* h : hosts)
		r[h->node] = {"127.0.0.1", h->port()};
	return r;
}

template <class P> bool until(P done, std::chrono::milliseconds limit) {
	const auto start = steady::now();
	while (!done()) {
		if (steady::now() - start > limit)
			return false;
		std::this_thread::sleep_for(2ms);
	}
	return true;
}

} // namespace cluster_test
