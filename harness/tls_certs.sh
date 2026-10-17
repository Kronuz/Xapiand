#!/usr/bin/env bash
#
# tls_certs.sh DIR
#
# Writes a throwaway test CA (ca.pem) and a server certificate it signs (server.pem,
# server.key at mode 0600), valid for localhost and 127.0.0.1, for the TLS checks.
set -euo pipefail

dir="${1:?usage: tls_certs.sh DIR}"
mkdir -p "$dir"
cd "$dir"
umask 077
openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:P-256 -nodes -days 30 \
	-subj "/CN=Xapiand test CA" -keyout ca.key -out ca.pem 2>/dev/null
openssl req -newkey ec -pkeyopt ec_paramgen_curve:P-256 -nodes \
	-subj "/CN=localhost" -keyout server.key -out server.csr 2>/dev/null
printf 'subjectAltName=DNS:localhost,IP:127.0.0.1\nextendedKeyUsage=serverAuth\n' > server.ext
openssl x509 -req -in server.csr -CA ca.pem -CAkey ca.key -CAcreateserial -days 30 \
	-extfile server.ext -out server.pem 2>/dev/null
chmod 644 ca.pem server.pem
rm -f server.csr server.ext ca.srl
echo "$dir: ca.pem server.pem server.key"
