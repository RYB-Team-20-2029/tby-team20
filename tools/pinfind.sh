#!/usr/bin/env bash
# Purpose: find which header pins connect between boards through the connector boards and
# backbone. Owning module: management (mgmt/).
#
# Usage:  ./tools/pinfind.sh talk   [seconds]   on ONE board: beacon on every free pin in turn
#         ./tools/pinfind.sh listen [seconds]   on every other board: report what it hears
#
# Every board in the backbone must be talking or listening, never running ryb-comms: the talker
# drives each pin in turn, and a board still running the comms would drive its send pin against
# it. This script stops ryb-comms on its own board and starts it again afterwards.
set -euo pipefail

cd "$(dirname "$0")/.."
BIN="$(pwd)/build/ryb_pinfind"
SERVICE="ryb-comms"

if [ $# -lt 1 ] || { [ "$1" != talk ] && [ "$1" != listen ]; }; then
    sed -n '5,10p' "$0" | sed 's/^# \{0,1\}//'
    exit 2
fi
if [ ! -x "$BIN" ]; then
    printf 'error: %s is missing; run ./tools/deploy.sh first, it builds it\n' "$BIN" >&2
    exit 1
fi

RESTART=0
if command -v systemctl > /dev/null && systemctl is-active --quiet "$SERVICE"; then
    printf 'Stopping %s for the pin search.\n' "$SERVICE"
    sudo systemctl stop "$SERVICE"
    RESTART=1
fi

if [ "$1" = talk ]; then
    printf 'Start "./tools/pinfind.sh listen" on every other board, then watch their reports.\n'
    printf 'Stop with Ctrl-C.\n'
fi
# Ctrl-C is for ryb_pinfind, which stops cleanly on it; this script carries on to restart the
# service.
trap '' INT
sudo "$BIN" "$@" || true

if [ "$RESTART" = 1 ]; then
    printf 'Starting %s again.\n' "$SERVICE"
    sudo systemctl start "$SERVICE"
fi
