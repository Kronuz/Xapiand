#!/bin/sh
endpoint=$1
start=${2:-1}
end=${3:-100}
shift 3

if [ -z "$endpoint" ]; then
	echo "usage: $0 <endpoint> [start] [end]"
	echo "example: $0 http://127.0.0.1:8880/fortune.db 1 10"
	exit 64
fi

json_escape() {
	python -c 'import sys, json; json.dump(sys.stdin.read(), sys.stdout)'
}

for id in $(seq "$start" "$end"); do
	message="$(fortune | json_escape)"
	data="{\"user\" : \"$USER\", \"postDate\" : \"$(date -u +'%Y-%m-%dT%H:%M:%SZ')\", \"message\" : $message}"
	curl -H "Content-Type: application/json" -XPUT "$endpoint/$id" -d "$data" "$@"
done


# Usage Example:
# index_and_get_fortune() {
# 	endpoint=$1
# 	start=${2:-1}
# 	end=${3:-100}
# 	../index_fortune.sh "$endpoint" "$start" "$end"
# 	sleep 3
# 	for id in $(seq "$start" "$end"); do
# 		curl "$endpoint/$id"
# 	done
# }
# index_and_get_fortune "http://127.0.0.1:8880/test/fortune" 1 10
