#!/usr/bin/env bash
# Purpose: set up RYB Comms on PYNQ boards: which module a board runs and which pins each hop of
# the chain uses, written into include/ryb/ryb_comms.json; then build and start at boot.
# Owning module: management (mgmt/).
#
# Usage:
#   ./tools/deploy.sh                    this board, asking everything
#   ./tools/deploy.sh chain              every module's pins at once, to copy to all boards
#   ./tools/deploy.sh <module> --quick   this board as <module>, pins as the JSON has them,
#                                        no questions: build and (re)start at boot
# Answers can also be piped in, one per line.
set -euo pipefail

cd "$(dirname "$0")/.."
ROOT="$(pwd)"
JSON="$ROOT/include/ryb/ryb_comms.json"
EDIT="$ROOT/include/ryb/cmake/RybDeployEdit.cmake"
BUILD="$ROOT/build"
BOARD_FILE="$HOME/.ryb-board"
SERVICE="ryb-comms"
UNIT="/etc/systemd/system/$SERVICE.service"

MODULES=(heartbeat sound decision motor)
LCD_RPI=(RBPI40 RBPI38 RBPI37 RBPI36 RBPI35 RBPI33)
LCD_ARDUINO=(AR10 AR11 AR12 AR13 AR_SCL AR_SDA)

say() { printf '%s\n' "$*"; }
die() {
    printf 'error: %s\n' "$*" >&2
    exit 1
}

# ask VAR "question" [default]: prompt, read a line, fall back to the default on an empty answer.
ask() {
    local reply=""
    if [ -n "${3-}" ]; then
        printf '%s [%s]: ' "$2" "$3"
    else
        printf '%s: ' "$2"
    fi
    IFS= read -r reply || true
    # Piped answers are not echoed by the terminal; print them so the log reads like a session.
    [ -t 0 ] || printf '%s\n' "$reply"
    reply="$(printf '%s' "$reply" | tr -d '\r' | sed 's/^ *//; s/ *$//')"
    printf -v "$1" '%s' "${reply:-${3-}}"
}

# ask_yes VAR "question" y|n: sets VAR to 1 or 0.
ask_yes() {
    local answer
    while true; do
        ask answer "$2 (y/n)" "$3"
        case "$answer" in
            y | Y | yes) printf -v "$1" 1 && return ;;
            n | N | no) printf -v "$1" 0 && return ;;
        esac
        say "  please answer y or n"
    done
}

in_list() {
    local item="$1"
    shift
    local x
    for x in "$@"; do
        [ "$x" = "$item" ] && return 0
    done
    return 1
}

# Accept AR3, ar3, RBPI29, rbpi29, pi29 or plain 29 (a Raspberry Pi header pin number).
normalize_pin() {
    local p
    p="$(printf '%s' "$1" | tr '[:lower:]' '[:upper:]')"
    case "$p" in
        AR_SDA | AR_SCL | NONE | -) printf '%s' "${p/-/NONE}" ;;
        AR[0-9]*) printf 'AR%d' "$((10#${p#AR}))" ;;
        RBPI[0-9]* | RPI[0-9]* | PI[0-9]* | [0-9]*)
            local n="${p//[!0-9]/}"
            printf 'RBPI%02d' "$((10#$n))"
            ;;
        *) printf '%s' "$p" ;;
    esac
}

upstream_of() {
    case "$1" in
        sound) printf heartbeat ;;
        decision) printf sound ;;
        motor) printf decision ;;
    esac
}

# --- Arguments and checks --------------------------------------------------------------------------

MODE=board
MODULE=""
QUICK=0
for arg in "$@"; do
    case "$arg" in
        chain) MODE=chain ;;
        --quick | --yes | -y) QUICK=1 ;;
        -h | --help)
            sed -n '5,12p' "$0" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *)
            in_list "$arg" "${MODULES[@]}" || die "unknown argument '$arg' (try --help)"
            MODULE="$arg"
            ;;
    esac
done
[ "$QUICK" = 1 ] && [ -z "$MODULE" ] && [ -f "$BOARD_FILE" ] && MODULE="$(cat "$BOARD_FILE")"
[ "$QUICK" = 1 ] && [ -z "$MODULE" ] && die "--quick needs the module: ./tools/deploy.sh decision --quick"

command -v cmake > /dev/null || die "cmake is not installed"
CMAKE_VERSION="$(cmake --version | head -n 1 | awk '{print $3}')"
if [ "$(printf '%s\n3.19\n' "$CMAKE_VERSION" | sort -V | head -n 1)" != "3.19" ]; then
    die "CMake $CMAKE_VERSION is too old, 3.19 or newer is needed (sudo pip3 install cmake)"
fi
[ -f "$JSON" ] || die "$JSON not found; run this from a copy of the whole repository"

BACKEND=mmio
ON_PYNQ=1
if [ "$(uname -m)" != "armv7l" ] || [ ! -e /sys/class/fpga_manager/fpga0 ]; then
    BACKEND=sim
    ON_PYNQ=0
fi

# --- What the JSON says now ------------------------------------------------------------------------

DUMP="$(mktemp)"
trap 'rm -f "$DUMP"' EXIT
load_json() {
    cmake -DJSON="$JSON" -DOUT="$DUMP" -DACTION=dump -P "$EDIT" > /dev/null || die "cannot read $JSON"
    declare -gA ADDR TX REP RX PIN
    LCD_NOW=""
    while read -r kind a b c d e f; do
        case "$kind" in
            module)
                ADDR[$a]="$b"
                TX[$a]="$c"
                REP[$a]="$d"
                RX[$a]="$e"
                ;;
            pin) PIN[$a]="$b" ;;
            lcd) LCD_NOW="$a${b:+ $b}${c:+ $c}${d:+ $d}${e:+ $e}${f:+ $f}" ;;
            line)
                BIT_US="$a"
                DRIVE="$b"
                ;;
        esac
    done < "$DUMP"
    case "$LCD_NOW" in
        "${LCD_RPI[*]}") LCD_NOW_NAME=rpi ;;
        "${LCD_ARDUINO[*]}") LCD_NOW_NAME=arduino ;;
        none) LCD_NOW_NAME=none ;;
        *) LCD_NOW_NAME=custom ;;
    esac
}
load_json

describe() {
    local m="$1" tx="${TX[$1]}" rx="${RX[$1]}"
    [ "$tx" = "-" ] && tx=nothing
    [ "$rx" = "-" ] && rx=nothing
    printf '%-9s sends on %-7s receives %s%s' "$m" "$tx" "$rx" \
        "$([ "${REP[$m]}" = 1 ] && printf ', repeater')"
}

# --- Questions -------------------------------------------------------------------------------------

ask_display() {
    say ""
    say "Display: 'rpi' is the RYB display seated on the Raspberry Pi header (pins 33-40),"
    say "'arduino' an LCD on AR10-AR13 + AR_SDA/AR_SCL, 'none' for no display."
    local default="$LCD_NOW_NAME" choices="rpi, arduino, none"
    [ "$default" = custom ] && default=keep && choices="$choices, keep"
    while true; do
        ask LCD "Where is the display ($choices)" "$default"
        case "$LCD" in
            rpi) RESERVED=("${LCD_RPI[@]}") && return ;;
            arduino) RESERVED=("${LCD_ARDUINO[@]}") && return ;;
            none) RESERVED=() && return ;;
            keep) [ "$LCD_NOW_NAME" = custom ] && read -r -a RESERVED <<< "$LCD_NOW" && return ;;
        esac
        say "  one of: $choices"
    done
}

# ask_pin VAR "question" default: a pin from the table, not the display's, or NONE if allowed.
ask_pin() {
    local answer
    while true; do
        ask answer "$2" "$3"
        answer="$(normalize_pin "$answer")"
        if [ "$answer" = NONE ]; then
            [ "${4-}" = allow_none ] && printf -v "$1" '' && return
            say "  this hop needs a pin"
            continue
        fi
        if [ -z "${PIN[$answer]+x}" ]; then
            say "  $answer is not in ryb_comms.json: use AR0-AR13 or a Raspberry Pi header pin like 29"
        elif in_list "$answer" "${RESERVED[@]+"${RESERVED[@]}"}"; then
            say "  $answer is used by the display"
        else
            printf -v "$1" '%s' "$answer"
            return
        fi
    done
}

ask_line() {
    say ""
    say "Line settings, shared by every board: ${BIT_US} us per bit, $DRIVE."
    NEW_BIT_US=""
    NEW_DRIVE=""
    ask_yes KEEP "Keep them" y
    if [ "$KEEP" = 0 ]; then
        ask NEW_BIT_US "Bit period in us (500 is the tested default)" "$BIT_US"
        while true; do
            ask NEW_DRIVE "Drive: push_pull (no pull-up needed) or open_drain (needs pull-ups)" "$DRIVE"
            in_list "$NEW_DRIVE" push_pull open_drain && break
        done
    fi
}

ask_module() {
    local previous=""
    [ -f "$BOARD_FILE" ] && previous="$(cat "$BOARD_FILE")"
    while true; do
        ask MODULE "Which module does this board run (${MODULES[*]})" "${MODULE:-$previous}"
        in_list "$MODULE" "${MODULES[@]}" && return
        say "  one of: ${MODULES[*]}"
    done
}

# Pins for one board, asked in full.
ask_board_pins() {
    local tx_default="${TX[$MODULE]}" rx_now="${RX[$MODULE]}" up
    up="$(upstream_of "$MODULE")"
    [ "$tx_default" = "-" ] && tx_default=none
    [ "$rx_now" = "-" ] && rx_now=""

    say ""
    say "Pins: AR0-AR13 on the Arduino header, or a Raspberry Pi header pin number such as 29."
    [ "$MODULE" = motor ] && say "The motor ends the chain, so it normally sends nothing."
    ask_pin TX_PIN "Which pin does $MODULE send on (or none)" "$tx_default" allow_none

    say ""
    if [ -n "$up" ]; then
        say "Upstream of $MODULE in the chain: $up."
    else
        say "$MODULE starts the chain, so it normally receives nothing."
    fi
    RX_SPEC=""
    local first="${rx_now%%,*}" pin_default from_default more from
    pin_default="${first%%=*}"
    from_default="${first#*=}"
    [ -z "$pin_default" ] && pin_default=none
    [ -z "$from_default" ] && from_default="$up"
    while true; do
        ask_pin RX_PIN "Which pin does $MODULE receive on (or none)" "$pin_default" allow_none
        [ -z "$RX_PIN" ] && break
        if [ "$RX_PIN" = "$TX_PIN" ]; then
            say "  that is also its send pin"
            continue
        fi
        while true; do
            ask from "Which modules send on $RX_PIN, comma separated" "${from_default//+/,}"
            SENDERS=()
            IFS=', ' read -r -a SENDERS <<< "$from"
            local s bad=""
            for s in "${SENDERS[@]}"; do
                in_list "$s" "${MODULES[@]}" || bad="$s"
            done
            [ ${#SENDERS[@]} -gt 0 ] && [ -z "$bad" ] && break
            say "  module names, from: ${MODULES[*]}"
        done
        RX_SPEC="${RX_SPEC:+$RX_SPEC,}$RX_PIN=$(IFS=+ && printf '%s' "${SENDERS[*]}")"
        ask_yes more "Another receive pin" n
        [ "$more" = 1 ] || break
        pin_default=none
        from_default=""
    done

    REPEATER=0
    if [ -n "$TX_PIN" ] && [ -n "$RX_SPEC" ]; then
        say ""
        say "A repeater passes on everything it receives, ahead of its own packets (sound does"
        say "this in the guideline's chain, so decision hears heartbeat too)."
        local rep_default=n
        [ "${REP[$MODULE]}" = 1 ] && rep_default=y
        ask_yes REPEATER "Is $MODULE a repeater" "$rep_default"
    fi
}

# --- Writing ---------------------------------------------------------------------------------------

# apply MODULE TX RX REPEATER: one module into the JSON, with the display and line answers.
apply() {
    RYB_MODULE="$1" RYB_TX="$2" RYB_RX="$3" RYB_REPEATER="$4" RYB_LCD="${LCD:-keep}" \
        RYB_BIT_PERIOD_US="${NEW_BIT_US:-}" RYB_DRIVE="${NEW_DRIVE:-}" \
        cmake -DJSON="$JSON" -DACTION=apply -P "$EDIT" > /dev/null || die "could not update $JSON"
}

# --- Building and starting -------------------------------------------------------------------------

build_and_start() {
    say ""
    if [ "$ON_PYNQ" = 1 ] && systemctl is-active --quiet "$SERVICE" 2> /dev/null; then
        say "Stopping $SERVICE while its program is rebuilt."
        sudo systemctl stop "$SERVICE"
    fi
    say "Building $MODULE..."
    mkdir -p "$BUILD"
    if ! cmake -S "$ROOT" -B "$BUILD" -DRYB_MODULE="$MODULE" -DRYB_GPIO_BACKEND="$BACKEND" \
        -DRYB_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release > "$BUILD/configure.log" 2>&1; then
        cat "$BUILD/configure.log"
        [ -f "$JSON.bak" ] && cp "$JSON.bak" "$JSON"
        die "configuration failed (see above); ryb_comms.json restored from the backup"
    fi
    grep -E "RYB Comms ryb_comms_$MODULE|RYB: " "$BUILD/configure.log" | sed 's/^-- /  /' || true
    printf '%s\n' "$MODULE" > "$BOARD_FILE"
    cmake --build "$BUILD" -j 2 > "$BUILD/build.log" 2>&1 || {
        tail -n 30 "$BUILD/build.log"
        die "the build failed (full log: $BUILD/build.log)"
    }
    local have_module=0
    [ -x "$BUILD/$MODULE" ] && have_module=1
    say "Built ${MODULE}_monitor$([ "$have_module" = 1 ] && printf " and $MODULE"), and ryb_pinfind."

    [ "$ON_PYNQ" = 1 ] && command -v systemctl > /dev/null || return 0
    local boot=""
    if [ "$QUICK" = 1 ]; then
        # Demo-day default: the module program once it exists, the monitor until then.
        boot=monitor
        [ "$have_module" = 1 ] && boot=module
    else
        say ""
        say "Start at boot: 'monitor' runs ${MODULE}_monitor (shows packets, sends test values),"
        [ "$have_module" = 1 ] && say "'module' runs the module's own program,"
        say "'no' leaves the board alone."
        local default=no
        [ -f "$UNIT" ] && default=monitor
        [ -f "$UNIT" ] && [ "$have_module" = 1 ] && default=module
        while true; do
            ask boot "Start at boot (monitor$([ "$have_module" = 1 ] && printf ', module'), no)" "$default"
            case "$boot" in
                monitor | no) break ;;
                module) [ "$have_module" = 1 ] && break ;;
            esac
        done
    fi

    if [ "$boot" = no ]; then
        if [ -f "$UNIT" ]; then
            sudo systemctl disable --now "$SERVICE" > /dev/null 2>&1 || true
            sudo rm -f "$UNIT"
            sudo systemctl daemon-reload
            say "Removed the start-at-boot service."
        fi
        return 0
    fi
    local exec="$BUILD/${MODULE}_monitor -q"
    [ "$boot" = module ] && exec="$BUILD/$MODULE"
    # After bootpy.service: it loads the overlay, and touching the GPIO before that can hang the
    # board.
    sudo tee "$UNIT" > /dev/null << EOF
[Unit]
Description=RYB Comms ($MODULE)
Requires=bootpy.service
After=bootpy.service

[Service]
ExecStart=$exec
Restart=on-failure
RestartSec=2

[Install]
WantedBy=multi-user.target
EOF
    sudo systemctl daemon-reload
    sudo systemctl enable "$SERVICE" > /dev/null 2>&1
    sudo systemctl restart "$SERVICE"
    say "Running $boot at boot as $SERVICE (journalctl -u $SERVICE -f to watch it)."
}

# --- Chain mode: every hop's pins at once ----------------------------------------------------------

if [ "$MODE" = chain ]; then
    say ""
    say "Chain set-up: heartbeat -> sound -> decision -> motor, one pin per hop."
    say "Give each hop's pin on the sending board and on the receiving board. They are the same"
    say "pin when the backbone joins the same pin of every slot; ./tools/pinfind.sh finds out."
    say "Pins: AR0-AR13 on the Arduino header, or a Raspberry Pi header pin number such as 29."
    ask_display
    HOPS=(heartbeat:sound sound:decision decision:motor)
    declare -A HOP_TX HOP_RX
    for hop in "${HOPS[@]}"; do
        from="${hop%%:*}"
        to="${hop#*:}"
        say ""
        tx_default="${TX[$from]}"
        [ "$tx_default" = "-" ] && tx_default=""
        ask_pin "HOP_TX[$hop]" "$from sends to $to on pin" "$tx_default"
        # Same pin on both ends unless the JSON already pairs this sender pin with another.
        rx_default="${HOP_TX[$hop]}"
        first="${RX[$to]%%,*}"
        if [ "$first" != "-" ] && [ -n "$first" ] && [ "${TX[$from]}" = "${HOP_TX[$hop]}" ]; then
            rx_default="${first%%=*}"
        fi
        ask_pin "HOP_RX[$hop]" "$to hears it on pin" "$rx_default"
    done
    for pair in "sound:${HOP_RX[heartbeat:sound]}:${HOP_TX[sound:decision]}" \
        "decision:${HOP_RX[sound:decision]}:${HOP_TX[decision:motor]}"; do
        IFS=: read -r m rx tx <<< "$pair"
        [ "$rx" = "$tx" ] && die "$m would send and receive on the same pin $rx"
    done
    say ""
    REP_DEFAULT=y
    [ "${REP[sound]}" = 0 ] && [ "${TX[sound]}" != "-" ] && REP_DEFAULT=n
    ask_yes SOUND_REPEATS "Does sound pass heartbeat's packets on to decision (repeater, as the guideline has it)" "$REP_DEFAULT"
    ask_line

    DECISION_FROM=sound
    [ "$SOUND_REPEATS" = 1 ] && DECISION_FROM=heartbeat+sound
    say ""
    say "The chain:"
    say "  heartbeat sends on ${HOP_TX[heartbeat:sound]}"
    say "  sound     hears it on ${HOP_RX[heartbeat:sound]}, sends on ${HOP_TX[sound:decision]}$([ "$SOUND_REPEATS" = 1 ] && printf ', repeats heartbeat')"
    say "  decision  hears it on ${HOP_RX[sound:decision]}, sends on ${HOP_TX[decision:motor]}"
    say "  motor     hears it on ${HOP_RX[decision:motor]}"
    say "  display:  $LCD;  line: ${NEW_BIT_US:-$BIT_US} us, ${NEW_DRIVE:-$DRIVE}"
    ask_yes GO "Write this into ryb_comms.json" y
    [ "$GO" = 1 ] || die "nothing changed"
    cp "$JSON" "$JSON.bak"
    apply heartbeat "${HOP_TX[heartbeat:sound]}" "" 0
    apply sound "${HOP_TX[sound:decision]}" "${HOP_RX[heartbeat:sound]}=heartbeat" "$SOUND_REPEATS"
    apply decision "${HOP_TX[decision:motor]}" "${HOP_RX[sound:decision]}=$DECISION_FROM" 0
    apply motor "" "${HOP_RX[decision:motor]}=decision" 0
    say "Written (backup in ryb_comms.json.bak)."
    say ""
    say "Next: copy include/ryb/ryb_comms.json to every board, and on each run"
    say "  ./tools/deploy.sh <its module> --quick"
    load_json
    ask_yes NOW "Deploy this board now" y
    [ "$NOW" = 1 ] || exit 0
    ask_module
    QUICK=1
    build_and_start
    exit 0
fi

# --- Board mode ------------------------------------------------------------------------------------

if [ "$QUICK" = 1 ]; then
    say "Deploying $MODULE as ryb_comms.json has it: $(describe "$MODULE")."
    build_and_start
    exit 0
fi

say ""
say "RYB Comms deployment for this board"
say "The chain is one way: heartbeat -> sound -> decision -> motor. To set every hop at once,"
say "run ./tools/deploy.sh chain instead."
ask_module

say ""
say "ryb_comms.json has:  $(describe "$MODULE")"
say "                     display $LCD_NOW_NAME, ${BIT_US} us bits, $DRIVE"
ask_yes KEEP_PINS "Use that as it is" y
if [ "$KEEP_PINS" = 1 ]; then
    build_and_start
    exit 0
fi

ask_display
ask_board_pins
ask_line

say ""
say "This board:  $MODULE (address ${ADDR[$MODULE]})"
say "  sends on:  ${TX_PIN:-nothing}"
say "  receives:  ${RX_SPEC:-nothing}${RX_SPEC:+  (pin=senders)}"
[ "$REPEATER" = 1 ] && say "  repeater:  yes"
say "  display:   $LCD"
say "  line:      ${NEW_BIT_US:-$BIT_US} us, ${NEW_DRIVE:-$DRIVE}"
ask_yes GO "Write this and build" y
[ "$GO" = 1 ] || die "nothing changed"
cp "$JSON" "$JSON.bak"
apply "$MODULE" "$TX_PIN" "$RX_SPEC" "$REPEATER"
say "Updated ryb_comms.json (backup in ryb_comms.json.bak)."
build_and_start
say ""
say "Wire GND between boards as well as the signal pins."
