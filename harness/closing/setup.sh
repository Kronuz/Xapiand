#!/usr/bin/env bash
# Lease setup for the closing benchmark (`lease run ... --setup harness/closing/setup.sh`): idempotent, run by ssh
# from the synced tree (~/work, which every sync replaces), so everything it installs lives in ~/data.
#
#   ~/data/elasticsearch-VERSION   the tarball with its bundled JDK, sha512 checked
#   ~/data/quickwit-vVERSION       the tarball, sha256 checked
#   ~/data/xapiand/xapiand         built from the synced tree (Release, gcc-13), rebuilt when src/ or CMakeLists.txt changes
#   ~/data/tools/ab_compare.py     cpp-libraries' paired comparison at a pinned commit, sha256 checked
#   ~/data/ccache, ~/data/build    the build's caches
# The Ubuntu 24.04 packages are the ones .github/workflows/ci.yml installs, plus bzip2, curl, fio and python3.
set -euo pipefail

main() {
	local src=$HOME/work data=$HOME/data
	# shellcheck source=harness/closing/pins.env
	. "$src/harness/closing/pins.env"
	mkdir -p "$data/dl"

	if [ ! -e "$data/.packages" ]; then
		sudo DEBIAN_FRONTEND=noninteractive apt-get update -qq
		sudo DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends \
			g++-13 cmake ninja-build pkg-config ccache git ca-certificates \
			libzstd-dev uuid-dev zlib1g-dev libcap-dev libssl-dev bzip2 curl fio python3 rsync
		touch "$data/.packages"
	fi
	sudo sysctl -q -w vm.max_map_count=262144

	local es=elasticsearch-$ES_VERSION-linux-x86_64.tar.gz
	if [ ! -x "$data/elasticsearch-$ES_VERSION/bin/elasticsearch" ]; then
		curl -fsSL --retry 3 -o "$data/dl/$es" "https://artifacts.elastic.co/downloads/elasticsearch/$es"
		echo "$ES_SHA512  $data/dl/$es" | sha512sum -c -
		tar -C "$data" -xzf "$data/dl/$es"
	fi
	local qw=quickwit-v$QW_VERSION-x86_64-unknown-linux-gnu.tar.gz
	if [ ! -x "$data/quickwit-v$QW_VERSION/quickwit" ]; then
		curl -fsSL --retry 3 -o "$data/dl/$qw" "https://github.com/quickwit-oss/quickwit/releases/download/v$QW_VERSION/$qw"
		echo "$QW_SHA256  $data/dl/$qw" | sha256sum -c -
		tar -C "$data" -xzf "$data/dl/$qw"
	fi

	if [ ! -s "$data/tools/ab_compare.py" ]; then
		mkdir -p "$data/tools"
		curl -fsSL --retry 3 -o "$data/tools/ab_compare.py" \
			"https://raw.githubusercontent.com/Kronuz/cpp-libraries/$AB_COMPARE_REV/tools/ab_compare.py"
		echo "$AB_COMPARE_SHA256  $data/tools/ab_compare.py" | sha256sum -c -
	fi

	local stamp
	stamp=$(cd "$src" && { find src cmake -type f 2>/dev/null; echo CMakeLists.txt; } | LC_ALL=C sort | xargs sha256sum | sha256sum | cut -d' ' -f1)
	if [ "$(cat "$data/xapiand/stamp" 2>/dev/null)" != "$stamp" ]; then
		export CCACHE_DIR=$data/ccache CCACHE_BASEDIR=$src CCACHE_NOHASHDIR=true CCACHE_COMPRESS=true
		CC=gcc-13 CXX=g++-13 cmake -GNinja -S "$src" -B "$data/build" -DCMAKE_BUILD_TYPE=Release -DLTO=OFF \
			-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
		ninja -C "$data/build" xapiand
		mkdir -p "$data/xapiand"
		cp "$data/build/bin/xapiand" "$data/xapiand/xapiand"
		echo "$stamp" >"$data/xapiand/stamp"
	fi
	"$data/xapiand/xapiand" --version 2>&1 | head -2 || true
	"$data/quickwit-v$QW_VERSION/quickwit" --version 2>&1 | head -1 || true
	"$data/elasticsearch-$ES_VERSION/jdk/bin/java" -version 2>&1 | head -1 || true
}

main "$@" </dev/null
