#!/bin/bash
# Деплой relay на сервере: сборка + systemd-юнит с автоперезапуском.
cd /root/fwatch || exit 1

echo "=== build ==="
gcc -std=c2x -Wall -Wextra -O2 -o sync \
    src/main.c src/relay.c src/client.c src/net.c src/input_stub.c -lpthread
rc=$?
if [ $rc -ne 0 ]; then
    echo "BUILD FAILED ($rc)"; exit 1
fi
echo "build OK"; ls -la sync

echo "=== systemd unit ==="
cat > /etc/systemd/system/fwatch-relay.service <<'UNIT'
[Unit]
Description=FWatch space-sync relay
After=network.target

[Service]
Type=simple
ExecStart=/root/fwatch/sync relay 9000
Restart=always
RestartSec=2
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
UNIT

# Убрать прежний transient-юнит с тем же именем, если остался.
systemctl stop fwatch-relay 2>/dev/null
systemctl reset-failed fwatch-relay 2>/dev/null
systemctl daemon-reload
systemctl enable fwatch-relay
systemctl restart fwatch-relay
sleep 1

echo "=== status ==="
systemctl is-active fwatch-relay
echo "=== listening ==="
ss -ltn | grep 9000 || echo "NOT LISTENING"
echo "=== logs ==="
journalctl -u fwatch-relay --no-pager -n 6
