#!/usr/bin/env bash
# SR3 in the Linux VM (A2's absolute bound, lead R6). Run from a checkout at the commit under test:
#   wide_search_vm.sh build                   sync this tree, build build-branch (its src) and build-master (the branch's src diff reverted)
#   wide_search_vm.sh create N                create N gate indexes in ~/wsl/wideN with build-master
#   wide_search_vm.sh gate ARM N RUNS         SR3 at N indexes with build-ARM (branch|master)
# Each command boots the VM, holds its lock, and stops it on exit. VM=~/.scratch/vm-document-gate by default.
set -euo pipefail
VMD=${VMD:-$HOME/.scratch/vm-document-gate}
V="$VMD/vm.sh linux"
lock=$VMD/linux/.lock
until mkdir "$lock" 2>/dev/null; do sleep 10; done
# The guest's page cache reaches the disk before the VM stops: a stop that kills qemu loses unsynced writes, and a
# binary linked just before it came back as zeros ("Exec format error").
trap '$V ssh sync >/dev/null 2>&1 || true; $V stop >/dev/null 2>&1 || true; rmdir "$lock"' EXIT
[[ $($V status) == running ]] || $V start
$V ssh 'uname -srm; nproc; df -h ~ | tail -1'
cmd=$1; shift
case $cmd in
build)
	$V ssh 'sudo apt-get update -q >/dev/null && sudo DEBIAN_FRONTEND=noninteractive apt-get install -y -q --no-install-recommends g++-13 cmake ninja-build pkg-config ccache libzstd-dev uuid-dev zlib1g-dev libcap-dev libssl-dev python3 git >/dev/null'
	git archive --prefix=src-tree/ HEAD | $V ssh 'mkdir -p ~/wsl && rm -rf ~/wsl/src-tree && tar -xf - -C ~/wsl'
	git diff master...HEAD -- src | $V ssh 'cd ~/wsl && rm -rf master-tree && cp -R src-tree master-tree && cd master-tree && patch -R -p1 >/dev/null'
	for arm in branch master; do
		tree=src-tree; [[ $arm == master ]] && tree=master-tree
		$V ssh "cd ~/wsl/$tree && export CC=gcc-13 CXX=g++-13 CCACHE_DIR=~/wsl/.ccache && cmake -GNinja -B ~/wsl/build-$arm -DFETCHCONTENT_BASE_DIR=\$HOME/wsl/.deps -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DLTO=OFF >/dev/null && rm -f ~/wsl/build-$arm/bin/xapiand && ninja -C ~/wsl/build-$arm -j${JOBS:-2} xapiand > ~/wsl/build-$arm.log 2>&1 || { grep -E -A6 'error:|Killed' ~/wsl/build-$arm.log | head -40; exit 3; }"
		$V ssh "sync; ls -la ~/wsl/build-$arm/bin/xapiand && head -c4 ~/wsl/build-$arm/bin/xapiand | grep -q ELF" || { echo "build-$arm: no ELF binary" >&2; exit 3; }
	done
	;;
create)
	n=$1
	$V ssh "mkdir -p ~/wsl/wide$n && cd ~/wsl/src-tree && python3 harness/wide_search_check.py gate \$(find ~/wsl/build-master -maxdepth 2 -name xapiand -type f | head -1) ~/wsl/wide$n 18431 --indexes $n --create-only --reuse"
	;;
gate)
	arm=$1 n=$2 runs=$3
	$V ssh "cd ~/wsl/src-tree && python3 harness/wide_search_check.py gate \$(find ~/wsl/build-$arm -maxdepth 2 -name xapiand -type f | head -1) ~/wsl/wide$n 18431 --indexes $n --runs $runs --reuse"
	;;
*) echo "unknown: $cmd" >&2; exit 2;;
esac
