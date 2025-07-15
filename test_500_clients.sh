#!/bin/bash
# test_500_clients.sh

SOCKS5_PROXY="socks5h://legoat:legches@127.0.0.2:1080"
TARGET_URL="http://www.google.com"

for i in $(seq 1 500); do
  curl --silent --socks5-basic --socks5-hostname "$SOCKS5_PROXY" "$TARGET_URL" > /dev/null &
  echo "Started client $i"
  sleep 0.01  # Evita saturar la tabla de sockets del sistema
done

wait
echo "All clients finished."
