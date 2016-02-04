#/bin/sh
endpoint=$1
start=${2:-1}
end=${3:-100}

if [ -z "$endpoint" ]; then
	echo "usage: $0 <endpoint> [start] [end]"
	echo "example: $0 http://127.0.0.1:8880/fortune.db 1 10"
	exit 64
fi

for id in $(seq $start $end); do
	message="$(fortune | tr '\n' $'\x01' | sed -e 's/\([\"\\]\)/\\\1/g' -e $'s/\x01/\\\\n/g' -e $'s/\b/\\\\b/g' -e $'s/\f/\\\\f/g' -e $'s/\r/\\\\r/g' -e $'s/\t/\\\\t/g')"
	data="{\"user\" : \"$USER\", \"postDate\" : \"$(date -u +'%Y-%m-%dT%H:%M:%SZ')\", \"message\" : \"$message\"}"
	curl -H "Content-Type: application/json" -XPUT "$endpoint/$id" -d "$data" $4 $5 $6 $7 $8 $9
done
