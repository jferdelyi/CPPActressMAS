#!/usr/bin/env bash
# Runs INSIDE ubuntu:24.04 (podman, --network=host), started by "csharp-ref.sh WORKDIR e20mono" next to a .NET 8
# container that runs the Tap (:5001 -> :5000) and RemoteMessages.Console. Installs Mono (container only) and runs the
# SHIPPED .NET Framework 4.7.2 binaries of E20 (bin/Release, copied to /w/e20-mono/$RUN with settings.txt pointing to
# the Tap; the reference clone is never written): MyServerConsole.exe on :5000, then MasMobileConsole.exe Container2,
# Container3 and Container1 (the mobile agent visits every container, Request Move Agent, and comes back home).
# Synchronisation with the .NET 8 side through marker files in $L: server.ready, tap.ready, mobile.done.
set -u
export DEBIAN_FRONTEND=noninteractive TERM=dumb
RUN="${RUN:?RUN}"; L="/w/logs/e20mono-$RUN"; M="/w/e20-mono/$RUN"
apt-get update -qq >/dev/null && apt-get install -y -qq --no-install-recommends mono-runtime libmono-system4.0-cil \
  libmono-system-core4.0-cil libmono-microsoft-csharp4.0-cil libmono-system-configuration4.0-cil libmono-system-xml4.0-cil \
  >/dev/null 2>&1 || { echo "apt install failed"; exit 1; }
echo "## $(mono --version | head -1)"
E="/w/ActressMas/ActressMAS v3.1/Source Code and Examples/Examples/E20 Mobile Examples"
mkdir -p "$M"
cp -R "$E/MyServerConsole/bin/Release" "$M/server"
cp -R "$E/MasMobileConsole/bin/Release" "$M/c1"
for n in 2 3; do
  mkdir -p "$M/c$n"; cp "$E/MasMobileConsole/bin/Release/ActressMas.dll" "$E/MasMobileConsole/bin/Release/MasMobileConsole.exe.config" "$M/c$n/"
  cp "$E/MasMobileConsole/bin/Release/$n/MasMobileConsole.exe" "$M/c$n/"
done
for n in 1 2 3; do printf 'ServerIP 127.0.0.1\nPort 5001\nName Container%s\n' "$n" > "$M/c$n/settings.txt"; done
md5sum "$M"/c*/MasMobileConsole.exe "$M"/c*/ActressMas.dll "$M/server/ActressMas.dll" | sed "s|$M/||"

# The server waits for ENTER: stdin stays open for 150 s, then the server stops.
cd "$M/server" && { (sleep 150; echo) | timeout 160 mono MyServerConsole.exe > "$L/server.txt" 2>&1; } &
SERVER=$!
sleep 3; touch "$L/server.ready"
for i in $(seq 120); do [ -f "$L/tap.ready" ] && break; sleep 1; done
[ -f "$L/tap.ready" ] || { echo "no tap"; exit 1; }

# The original readme: start the containers 2 and 3 before container 1; ENTER starts the MAS.
cd "$M/c2" && { (sleep 10; echo) | timeout 40 mono MasMobileConsole.exe > "$L/c2.txt" 2>&1; } &
C2=$!
cd "$M/c3" && { (sleep 10; echo) | timeout 40 mono MasMobileConsole.exe > "$L/c3.txt" 2>&1; } &
C3=$!
sleep 4
cd "$M/c1" && { (sleep 10; echo) | timeout 36 mono MasMobileConsole.exe > "$L/c1.txt" 2>&1; } &
C1=$!
wait $C1 $C2 $C3
touch "$L/mobile.done"
wait $SERVER
echo "mono side done"
