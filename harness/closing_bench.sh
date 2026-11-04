#!/usr/bin/env bash
# The closing benchmark's runner (docs/design/closing-benchmark.md, D10 and D14). On a lease:
#
#   lease run closing-metal c7i.metal-24xl --timeout T --sync BUNDLE --setup BUNDLE/harness/closing/setup.sh -- \
#       harness/closing_bench.sh --units LIST --deadline-min N
#
#   closing_bench.sh --list                              the units with their estimated minutes
#   closing_bench.sh --plan [--times FILE] [--approved USD] [--resume-min M]
#                                                        pack whole units into runs of at most 45 minutes (D14)
#   closing_bench.sh --units LIST --deadline-min N [--smoke] [--times FILE]
#                                                        run the units in order into $OUT/results; one that does not
#                                                        fit before the deadline is skipped and recorded (CB-R14)
#
# Each run first records the host (DMI identity, CPU, microcode, turbo and governor) and 10 s of fsync latency from fio
# beside the results (D2, D6), and pins turbo off and the governor to performance where the host has them.
set -euo pipefail

here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
units=$here/closing_units.py

host_identity() {
	local dir=$1 data=${CLOSING_DATA:-$HOME/data}
	mkdir -p "$dir"
	if [ -w /sys/devices/system/cpu/intel_pstate/no_turbo ] || sudo -n true 2>/dev/null; then
		echo 1 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo >/dev/null 2>&1 || true
		for g in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
			[ -e "$g" ] && echo performance | sudo tee "$g" >/dev/null 2>&1 || true
		done
	fi
	{
		echo "date $(date -u +%FT%TZ)"
		echo "uname $(uname -a)"
		for f in product_uuid board_serial product_name sys_vendor bios_version; do
			echo "dmi.$f $(sudo -n cat /sys/class/dmi/id/$f 2>/dev/null || cat /sys/class/dmi/id/$f 2>/dev/null || echo unreadable)"
		done
		grep -m1 -E 'model name|Model name' /proc/cpuinfo || true
		grep -m1 microcode /proc/cpuinfo || true
		echo "no_turbo $(cat /sys/devices/system/cpu/intel_pstate/no_turbo 2>/dev/null || echo none)"
		echo "governor $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || echo none)"
		echo "instance $(curl -s --max-time 2 http://169.254.169.254/latest/meta-data/instance-id 2>/dev/null || echo none)"
		lscpu
	} >"$dir/identity.txt" 2>&1
	if command -v fio >/dev/null; then
		mkdir -p "$data/fio"
		fio --name=fsync --directory="$data/fio" --rw=write --bs=4k --size=32m --fsync=1 --runtime=10 --time_based \
			--output-format=json >"$dir/fio.json" 2>"$dir/fio.err" || true
		rm -rf "$data/fio"
	fi
}

case ${1:-} in
--list) exec python3 "$units" list ;;
--plan)
	shift
	exec python3 "$units" plan "$@"
	;;
--units | --smoke)
	out=${OUT:-$PWD/out}
	mkdir -p "$out"
	host_identity "$out/host"
	exec python3 "$units" run --root "$out/results" "$@"
	;;
*)
	sed -n '2,15p' "${BASH_SOURCE[0]}" >&2
	exit 2
	;;
esac
