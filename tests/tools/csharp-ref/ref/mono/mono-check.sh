#!/usr/bin/env bash
# Runs INSIDE ubuntu:24.04 (podman). Installs Mono from Ubuntu universe (container only),
# runs the SHIPPED .NET Framework 4.7.2 binaries of ActressMAS v3.1 and cross-checks RNG vectors / traces.
set -u
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq >/dev/null && apt-get install -y -qq --no-install-recommends \
  mono-runtime mono-mcs libmono-microsoft-csharp4.0-cil libmono-system-core4.0-cil >/dev/null 2>&1 \
  || { echo "apt install failed"; exit 1; }
echo "## $(mono --version | head -1)"; echo "## mcs $(mcs --version)"; echo "## arch $(uname -m)"
V31="/w/ActressMas/ActressMAS v3.1"
B="$V31/Source Code and Examples/Examples/E30 Benchmarks"
TIMEFORMAT="process wall=%Rs user=%Us sys=%Ss"
for exe in "Skynet/bin/Release/Skynet.exe" "SkynetNumeric/bin/Release/SkynetNumeric.exe" "PingPong/bin/Release/PingPong.exe"; do
  echo "== mono shipped $exe"; time timeout 600 mono "$B/$exe"; echo "exit=$?"
done
mkdir -p /tmp/m && cp "$V31/ActressMas Dll/ActressMas.dll" /tmp/m/
echo "== mcs RngVectors"; mcs -nologo -out:/tmp/m/RngVectors.exe -r:System.Core /w/ref/RngVectors/Program.cs && mono /tmp/m/RngVectors.exe | tee /w/logs/rng-mono.txt
echo "== mcs TraceOrder (against shipped ActressMas Dll/ActressMas.dll)"
mcs -nologo -out:/tmp/m/TraceOrder.exe -r:/tmp/m/ActressMas.dll -r:System.Core -r:Microsoft.CSharp /w/ref/TraceOrder/Program.cs \
  && mono /tmp/m/TraceOrder.exe | tee /w/logs/trace-mono.txt
