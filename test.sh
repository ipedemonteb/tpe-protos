#!/bin/bash

PROXY="socks5h://localhost:1080"
DEST="http://example.org"

for i in $(seq 1 1100); do
    echo "Request $i"
    curl --socks5-hostname "$PROXY" \
         --header "Connection: keep-alive" \
         --max-time 30 \
         "$DEST" --output /dev/null --silent &
done

wait
echo "Finished all connections"
