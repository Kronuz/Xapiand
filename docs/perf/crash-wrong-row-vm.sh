#!/usr/bin/env bash
# crash-wrong-row-vm.sh build|run KIND N: crash scenario D on the campaign's FreeBSD VM (port 18784, ~/.scratch/vm-document-gate).
#   build: this commit's tree into /var/cwr/src, then the Release (rel) and ASan+UBSan (asan) builds, as CI's freebsd.yml.
#   run:   harness/engine_crash_repeat.sh KIND N there, as nobody (CI's rows run as nobody); failures kept in /var/cwr/home.
set -euo pipefail
V="$HOME/.scratch/vm-document-gate/vm.sh"
case "$1" in
build)
	git archive --format=tar HEAD | "$V" freebsd ssh 'rm -rf /var/cwr && mkdir -p /var/cwr/src /var/cwr/home && tar -xf - -C /var/cwr/src'
	"$V" freebsd ssh 'set -e; export PATH=$PATH:/usr/local/bin; pkg install -y cmake ninja git pkgconf zstd asio bash python3 curl >/dev/null
		cd /var/cwr/src
		cmake -S . -B build-rel -G Ninja -DCMAKE_BUILD_TYPE=Release -DLTO=OFF -DFETCHCONTENT_BASE_DIR=$PWD/.deps -DASIO_INCLUDE_DIR=/usr/local/include >/var/cwr/build-rel.log 2>&1
		cmake --build build-rel -j2 --target xapiand strata_check >>/var/cwr/build-rel.log 2>&1 || { tail -30 /var/cwr/build-rel.log; exit 1; }
		cmake -S . -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DLTO=OFF -DASAN=ON -DUBSAN=ON -DFETCHCONTENT_BASE_DIR=$PWD/.deps -DASIO_INCLUDE_DIR=/usr/local/include >/var/cwr/build-asan.log 2>&1
		cmake --build build-asan -j2 --target xapiand strata_check >>/var/cwr/build-asan.log 2>&1 || { tail -30 /var/cwr/build-asan.log; exit 1; }
		chown -R nobody:nobody /var/cwr; uname -srm; ls -la build-rel/bin build-asan/bin'
	;;
run)
	"$V" freebsd ssh "cd /var/cwr/src && su -m nobody -c 'export PATH=\$PATH:/usr/local/bin HOME=/var/cwr/home TMPDIR=/var/cwr/home; bash harness/engine_crash_repeat.sh $2 $3'"
	;;
esac
