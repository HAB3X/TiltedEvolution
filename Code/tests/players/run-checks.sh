#!/usr/bin/env bash
# Fake-player checks on Windows (CI, Git Bash): runs a server from the build folder and FakePlayers against it.
#   run-checks.sh <build folder with SkyrimTogetherServer.exe, STServer.dll and FakePlayers.exe>
# On a Mac, skyrim-mac-tools/launch/check-host-world.sh runs the same under Wine, and against an official server.
set -u
BUILD=$(cd "${1:?build folder}" && pwd)
WORK=$(mktemp -d)
FAILED=0
trap 'taskkill //F //IM SkyrimTogetherServer.exe > /dev/null 2>&1; rm -rf "$WORK"' EXIT

# suite <name> <ini text> <required mods or ""> <role>...: a fresh server; the last role runs in front, the others before
# it in the background
suite() {
    local name=$1 ini=$2 mods=$3; shift 3
    local dir="$WORK/$name"
    mkdir -p "$dir/config"
    cp "$BUILD"/SkyrimTogetherServer.exe "$BUILD"/STServer.dll "$dir"/
    printf '%s\n' "$ini" > "$dir/config/STServer.ini"
    [ -n "$mods" ] && printf '%s\n' "$mods" > "$dir/config/required-mods.txt"

    echo "== $name"
    (cd "$dir" && ./SkyrimTogetherServer.exe < /dev/null > server.log 2>&1 &)
    for _ in $(seq 1 30); do grep -aq "started on port" "$dir/server.log" 2>/dev/null && break; sleep 1; done
    if ! grep -aq "started on port" "$dir/server.log"; then echo "FAIL the server didn't start"; tail -5 "$dir/server.log"; FAILED=1; return; fi

    local roles=("$@") last=$(( $# - 1 )) i
    for (( i = 0; i < last; i++ )); do
        ("$BUILD/FakePlayers.exe" "${roles[$i]}" 127.0.0.1:10578 > "$dir/${roles[$i]}.log" 2>&1 &)
        sleep 5
    done
    "$BUILD/FakePlayers.exe" "${roles[$last]}" 127.0.0.1:10578 > "$dir/${roles[$last]}.log" 2>&1
    sleep 3
    cat "$dir"/*.log | grep -a "PASS\|FAIL" | tr -d '\r'
    grep -aq "FAIL" "$dir"/*.log && FAILED=1
    grep -aq "ALL CHECKS PASSED" "$dir/${roles[$last]}.log" || FAILED=1
    taskkill //F //IM SkyrimTogetherServer.exe > /dev/null 2>&1; sleep 2
}

suite host-world $'[GameServer]\nsPassword=\nbUPnP=false\n[Gameplay]\nbHostWorld=true' "" host join
suite kick $'[GameServer]\nsPassword=\nbUPnP=false\nbLanDiscovery=false' "" kicker kicked
suite required-mods $'[GameServer]\nsPassword=\nbUPnP=false\nbLanDiscovery=false\n[ModPolicy]\nbRequireMods=true' $'SomeMod.esp\nccBGSSSE001-Fish.esm' mods
exit $FAILED
