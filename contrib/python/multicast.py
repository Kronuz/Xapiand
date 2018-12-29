#!/usr/bin/env python
"""
Google Cloud as well as other cloud providers do not allow UDP multicasting
across its network. This script works by receiving multicast messages and
sending them to all other listed nodes in the network using UDP unicast.

Google Kubernetes is an example where this script is needed to be run as a
node daemon so multicasted messages in a node get propagated to all other
nodes.
"""

import os
import sys
import ssl
import json
import time
import select
import socket
import struct
import urllib2
import httplib


MCAST_GRP = '239.192.168.1'
MCAST_PORT = 58880

K8S_SCHEME = os.environ.get('K8S_SCHEME', "https")
K8S_HOST = os.environ.get('K8S_HOST', "kubernetes.default.svc.cluster.local")
K8S_PORT = os.environ.get('K8S_PORT', 443)
K8S_TOKEN_PATH = os.environ.get('K8S_TOKEN_PATH', "/var/run/secrets/kubernetes.io/serviceaccount/token")
K8S_CERT_PATH = os.environ.get('K8S_CERT_PATH', "/var/run/secrets/kubernetes.io/serviceaccount/ca.crt")
K8S_NAMESPACE_PATH = os.environ.get('K8S_NAMESPACE_PATH', "/var/run/secrets/kubernetes.io/serviceaccount/namespace")
K8S_LABEL_SELECTOR = os.environ.get('K8S_LABEL_SELECTOR', "app%3Dxapiand%2Ccomponent%3Dmulticast")
K8S_TOKEN = open(K8S_TOKEN_PATH).read().replace('\n', '')
K8S_NAMESPACE = open(K8S_NAMESPACE_PATH).read().replace('\n', '')


class VerifiedHTTPSConnection(httplib.HTTPSConnection):
    def connect(self):
        sock = socket.create_connection((self.host, self.port), self.timeout)
        if self._tunnel_host:
            self.sock = sock
            self._tunnel()
        self.sock = ssl.wrap_socket(sock,
                                    self.key_file,
                                    self.cert_file,
                                    cert_reqs=ssl.CERT_REQUIRED,
                                    ca_certs=K8S_CERT_PATH)


class VerifiedHTTPSHandler(urllib2.HTTPSHandler):
    def __init__(self, connection_class=VerifiedHTTPSConnection):
        self.specialized_conn_class = connection_class
        urllib2.HTTPSHandler.__init__(self)

    def https_open(self, req):
        return self.do_open(self.specialized_conn_class, req)


def urlopen(req):
    https_handler = VerifiedHTTPSHandler()
    url_opener = urllib2.build_opener(https_handler)
    return url_opener.open(req)


def request(path):
    url = "{}://{}:{}{}".format(K8S_SCHEME, K8S_HOST, K8S_PORT, path)
    headers = {"Authorization": "Bearer {}".format(K8S_TOKEN)}
    req = urllib2.Request(url, None, headers)
    response = urlopen(req)
    return response.read()


class Server:
    def __init__(self):
        self.host = socket.gethostbyname(socket.gethostname())

        mcast_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        mcast_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, True)
        mcast_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, True)
        mcast_sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 3)
        mcast_sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_LOOP, False)
        mreq = struct.pack('4sl', socket.inet_aton(MCAST_GRP), socket.INADDR_ANY)
        mcast_sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
        mcast_sock.bind((MCAST_GRP, MCAST_PORT))
        self.mcast_sock = mcast_sock

        udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        udp_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, True)
        udp_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, True)
        udp_sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 4194304)
        udp_sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4194304)
        udp_sock.bind((self.host, MCAST_PORT))
        self.udp_sock = udp_sock

        self.servers = set()
        self.sockets = {}

    def add(self, addr):
        self.servers.add(addr)

    def mcast_recv(self):
        try:
            msg, (addr, port) = self.mcast_sock.recvfrom(1500)
            # sys.stderr.write("multicast received msg: %r\n" % msg)
            return msg
        except Exception:
            pass

    def mcast_send(self, msg):
        try:
            self.mcast_sock.sendto(msg, (MCAST_GRP, MCAST_PORT))
        except Exception:
            pass

    def udp_recv(self):
        try:
            msg, (addr, port) = self.udp_sock.recvfrom(1500)
            self.add(addr)
            if msg != '\x00':
                # sys.stderr.write("udp received msg: %r\n" % msg)
                return msg
        except Exception:
            pass

    def udp_send(self, msg):
        for addr in self.servers:
            if addr != self.host:
                try:
                    self.udp_sock.sendto(msg, (addr, MCAST_PORT))
                except Exception:
                    pass

    def addrs(self):
        while True:
            try:
                servers = set()
                result = json.loads(request("/api/v1/namespaces/{}/pods?labelSelector={}".format(K8S_NAMESPACE, K8S_LABEL_SELECTOR)))
                for item in result['items']:
                    podIP = item['status']['podIP']
                    servers.add(podIP)
                return servers
            except Exception:
                time.sleep(0.1)

    def serve(self):
        sys.stderr.write("Multicasting ready!\n")
        while True:
            ready = select.select([self.udp_sock, self.mcast_sock], [], [], 1)[0]
            if self.udp_sock in ready:
                msg = self.udp_recv()
                if msg:
                    self.mcast_send(msg)
            if self.mcast_sock in ready:
                msg = self.mcast_recv()
                if msg:
                    self.udp_send(msg)


def main(addrs):
    server = Server()
    for addr in addrs:
        server.add(socket.gethostbyname(addr))

    for addr in server.addrs():
        server.add(addr)

    server.udp_send('\x00')

    server.serve()


if __name__ == '__main__':
    try:
        main(sys.argv[1:])
    except (KeyboardInterrupt, SystemExit):
        pass
