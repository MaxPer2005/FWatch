#!/usr/bin/env bash
# Установка и запуск FWatch-клиента на macOS одной командой:
#   curl -fsSL https://raw.githubusercontent.com/MaxPer2005/FWatch/main/install.sh | bash
set -euo pipefail

SERVER="91.228.153.31"
PORT="9000"
REPO="MaxPer2005/FWatch"
DEST="$HOME/.local/bin"
BIN="$DEST/sync"

if [ "$(uname -s)" != "Darwin" ]; then
    echo "Этот скрипт для macOS. На Windows используй install.ps1 (см. README)." >&2
    exit 1
fi

echo ">> качаю sync..."
mkdir -p "$DEST"
curl -fsSL "https://github.com/$REPO/releases/latest/download/sync-macos" -o "$BIN"
chmod +x "$BIN"
# снять карантин Gatekeeper на случай, если он проставлен
xattr -dr com.apple.quarantine "$BIN" 2>/dev/null || true

echo ">> установлено: $BIN"
echo ">> подключаюсь к $SERVER:$PORT ..."
echo
echo "   ВАЖНО: при первом запуске macOS попросит разрешение Accessibility."
echo "   System Settings > Privacy & Security > Accessibility — включи свой терминал,"
echo "   затем запусти снова:  $BIN client $SERVER $PORT"
echo

exec "$BIN" client "$SERVER" "$PORT"
