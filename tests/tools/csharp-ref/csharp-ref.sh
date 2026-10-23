#!/usr/bin/env bash
# csharp-ref.sh - build and run the C# reference ActressMAS v3.1 (commit eb0bb73) in ISOLATED containers
# on a dedicated Podman machine ("cam" by default, or $CAM_PODMAN_CONNECTION). Nothing is installed on the host; the
# original sources are compiled unchanged.
#
# Usage:  csharp-ref.sh WORKDIR ACTION [net10|net8]
#   WORKDIR : work folder outside the repository; ref/ (next to this script) is copied into WORKDIR/ref at each run,
#             builds stay there. ActressMas/ and ABM_Comparison/ (ref/Abm) are cloned there if missing, then only read.
#             Must live under $HOME or /private/tmp: plain /tmp is NOT shared with the VM (podman: "statfs ...: no such file").
#   ACTION  : build | rng | trace | format | bench | benchref | e00 | probe | mono | shimcheck | oracles | distrib | all
#             benchref : runs Skynet, SkynetNumeric, PingPong, PingPong.Seq, PingPong.Pairs (parallel and sequential),
#                        Abm/Wsg and Abm/Flockers CAM_BENCH_RUNS times (10 by default) and writes their medians into
#                        WORKDIR/logs/bench-reference-<flavor>.txt, the C# reference of the perf.vs_csharp tests on this
#                        machine (CAM_BENCH_REFERENCE)
#             format   : FormatVectors (number formatting and parsing of .NET), for tests/iso/vectors/format.txt
#             shimcheck: RngVectors through the seed shim, must equal ref/expected/rng-vectors.txt
#             oracles  : runs every project of ref/Oracles (seed shim, CAM_SEED or 0) from its output folder into
#                        WORKDIR/logs/oracles/<Name>.txt, the raw outputs to version in tests/iso/oracles/
#             distrib  : runs the oracle projects of CAM_DISTRIB (PredatorPrey Reactive ContractNet Shapley by default) at
#                        the seeds 1 to CAM_DISTRIB_SEEDS (30) into WORKDIR/logs/distrib/<Name>/seed<n>.txt, the C# samples
#                        of the distribution tests (tests/iso/iso_stats.cpp)
#             e20build | capture | e20 | e20mono   (ContainerServer harness, ref/E20; always net8: BinaryFormatter throws
#             on net9+; e20mono adds the SHIPPED .NET Framework server and mobile containers under Mono, ref/mono/e20-mono.sh)
#             e20gui   : MasMobileGui without WinForms (ref/E20/MasMobileGui.Console), server and three containers; its
#                        agent cannot move on .NET 8 (Agent.Move serializes a System.Type)
#             e20oracles: WORKDIR/logs/oracles/RemoteMessages.txt and MasMobileConsole.txt from the captures of e20 and
#                        of the latest e20mono (no container run): the console of each container in turn
#   flavor  : net10 (default, image mcr.microsoft.com/dotnet/sdk:10.0) or net8 (image mcr.microsoft.com/dotnet/sdk:8.0)
# Outputs : WORKDIR/logs/*.txt|*.log ; compare rng/trace outputs with ref/expected/*.txt from line 2 (line 1 = runtime banner).
set -euo pipefail
export CONTAINER_CONNECTION="${CAM_PODMAN_CONNECTION:-cam}"
TOOLS="$(cd "$(dirname "$0")" && pwd -P)"
W="$(cd "${1:?usage: csharp-ref.sh WORKDIR ACTION [net10|net8]}" && pwd -P)"
ACTION="${2:-all}"
FLAVOR="${3:-net10}"
REF_COMMIT=eb0bb73
ABM_COMMIT=eaf5dcd
case "$FLAVOR" in
  net10) IMG=mcr.microsoft.com/dotnet/sdk:10.0; TFM=net10.0 ;;
  net8)  IMG=mcr.microsoft.com/dotnet/sdk:8.0;  TFM=net8.0 ;;
  *) echo "flavor must be net10 or net8" >&2; exit 2 ;;
esac
mkdir -p "$W/ref"
rsync -a --exclude bin --exclude obj "$TOOLS/ref/" "$W/ref/"
if [ ! -d "$W/ActressMas/.git" ]; then
  git clone -q https://github.com/florinleon/ActressMas "$W/ActressMas"
  git -C "$W/ActressMas" -c advice.detachedHead=false checkout -q "$REF_COMMIT"
fi
# The reference clone is read only: its commit is checked, nothing is written into it.
case "$(GIT_OPTIONAL_LOCKS=0 git -C "$W/ActressMas" rev-parse HEAD)" in
  "$REF_COMMIT"*) ;;
  *) echo "ActressMas is not at $REF_COMMIT: stop, the clone is read only" >&2; exit 2 ;;
esac
# The ActressMAS models of Antelmi et al. (ref/Abm), cloned the same way and only read
if [ ! -d "$W/ABM_Comparison/.git" ]; then
  git clone -q https://github.com/isislab-unisa/ABM_Comparison "$W/ABM_Comparison"
  git -C "$W/ABM_Comparison" -c advice.detachedHead=false checkout -q "$ABM_COMMIT"
fi
case "$(GIT_OPTIONAL_LOCKS=0 git -C "$W/ABM_Comparison" rev-parse HEAD)" in
  "$ABM_COMMIT"*) ;;
  *) echo "ABM_Comparison is not at $ABM_COMMIT: stop, the clone is read only" >&2; exit 2 ;;
esac
mkdir -p "$W/logs"
EX="$W/ActressMas/ActressMAS v3.1/Source Code and Examples/Examples"

# example folder of an oracle project (its <OracleSources>, relative to Examples/), model folder of a project of
# ref/Abm (its <AbmSources>, relative to the ActressMAS/ folder of ABM_Comparison)
oracle_sources() { sed -n 's:.*<OracleSources>\$(AmasEx)\(.*\)</OracleSources>.*:\1:p' "$1"; }
abm_sources() { sed -n 's:.*<AbmSources>\$(AbmModels)\(.*\)</AbmSources>.*:\1:p' "$1"; }

# Patched/<File>.cs of each <File>.cs.sed of an oracle project or of a project of ref/Abm: the original file with one
# line rewritten (noTurns, a scenario that reaches a bug, a size written by hand), written in WORKDIR/ref (never in the
# repository nor in a clone), compiled instead of the original
for sedf in "$W"/ref/Oracles/*/*.cs.sed "$W"/ref/Abm/*/*.cs.sed; do
  [ -e "$sedf" ] || continue
  dir="$(dirname "$sedf")"; file="$(basename "$sedf" .sed)"
  case "$dir" in
    "$W"/ref/Abm/*) src="$W/ABM_Comparison/ActressMAS/$(abm_sources "$dir"/*.csproj)$file" ;;
    *) src="$EX/$(oracle_sources "$dir"/*.csproj)$file" ;;
  esac
  mkdir -p "$dir/Patched"
  sed -f "$sedf" "$src" > "$dir/Patched/$file"
  if [ "$(diff "$src" "$dir/Patched/$file" | grep -c '^>')" != 1 ]; then
    echo "$sedf must change exactly one line of $src" >&2; exit 1
  fi
done
for gen in "$W"/ref/Oracles/*/Patched/*.cs "$W"/ref/Abm/*/Patched/*.cs; do
  [ -e "$gen" ] || continue
  [ -e "$(dirname "$(dirname "$gen")")/$(basename "$gen").sed" ] || { echo "$gen has no $(basename "$gen").sed any more: remove it" >&2; exit 1; }
done

# one-shot .NET SDK container, WORKDIR mounted on /w, removed on exit; with CAM_DOTNET=native, the dotnet of the host
# instead, from WORKDIR/ref too (the CI, where the job installs .NET; actions build, bench and benchref)
if [ "${CAM_DOTNET:-}" = native ]; then
  dn() { (cd "$W/ref" && DOTNET_CLI_TELEMETRY_OPTOUT=1 DOTNET_NOLOGO=1 "$@"); }
  dn_sdk10() { dn "$@"; }
else
  dn() { podman run --rm --pull=never --name "cam-csref-$$-$RANDOM" -e DOTNET_CLI_TELEMETRY_OPTOUT=1 -e DOTNET_NOLOGO=1 \
           -v "$W":/w -w /w/ref "$IMG" "$@"; }
  dn_sdk10() { podman run --rm --pull=never --name "cam-csref-sln-$$" -e DOTNET_CLI_TELEMETRY_OPTOUT=1 -e DOTNET_NOLOGO=1 \
                 -v "$W":/w -w /w/ref mcr.microsoft.com/dotnet/sdk:10.0 "$@"; }
fi

do_build() {
  # generated at each build (SDK 10 CLI) instead of shipping a GUID-laden .sln, so that new projects are included
  dn_sdk10 bash -c \
    'dotnet new sln -n ActressMasRef --format sln --force >/dev/null && dotnet sln ActressMasRef.sln add $(find . -name "*.csproj" -not -path "./BfProbe/*" -not -path "./E20/*" | sort) >/dev/null' 
  dn dotnet build ActressMasRef.sln -c Release -p:RefTfm="$TFM" > "$W/logs/build-$FLAVOR.log" 2>&1 \
    || { tail -30 "$W/logs/build-$FLAVOR.log"; exit 1; }
  grep -E 'Warning\(s\)|Error\(s\)' "$W/logs/build-$FLAVOR.log"
}
do_rng()   { dn dotnet "RngVectors/bin/Release/$TFM/RngVectors.dll" | tee "$W/logs/rng-$FLAVOR.txt"; }
do_trace() { dn dotnet "TraceOrder/bin/Release/$TFM/TraceOrder.dll" | tee "$W/logs/trace-$FLAVOR.txt"; }
do_format() { dn dotnet "FormatVectors/bin/Release/$TFM/FormatVectors.dll" | tr -d '\r' > "$W/logs/format-$FLAVOR.txt"; wc -l "$W/logs/format-$FLAVOR.txt"; }
do_bench() {
  dn bash -c "TIMEFORMAT='process wall=%Rs user=%Us sys=%Ss'; echo nproc=\$(nproc);
    for b in Skynet SkynetNumeric PingPong; do echo \"== \$b\"; time dotnet \$b.Ref/bin/Release/$TFM/\$b.dll; done" 2>&1 \
    | tee "$W/logs/bench-$FLAVOR.txt"
}
do_benchref() {  # medians of the C# benchmarks on this machine, read by the perf.vs_csharp tests (tests/bench/README.md)
  local runs="${CAM_BENCH_RUNS:-10}" out="$W/logs/bench-reference-$FLAVOR.txt" spec name metric dll args i
  {
    echo "#cam bench reference: ActressMas $REF_COMMIT, $(dn bash -c 'dotnet --list-runtimes | grep Microsoft.NETCore.App | tail -1 | cut -d" " -f1,2; echo "$(uname -m), $(nproc) CPUs"' | tr -d '\r' | paste -sd, - | sed 's/,/, /'), $runs runs"
    for spec in "Skynet ms Skynet.Ref/bin/Release/$TFM/Skynet.dll" "SkynetNumeric ms SkynetNumeric.Ref/bin/Release/$TFM/SkynetNumeric.dll" \
                "PingPong msg_per_ms PingPong.Ref/bin/Release/$TFM/PingPong.dll" "PingPongSequential msg_per_ms PingPong.Seq/bin/Release/$TFM/PingPongSeq.dll" \
                "PingPongPairs msg_per_ms PingPong.Pairs/bin/Release/$TFM/PingPongPairs.dll" \
                "PingPongPairsSequential msg_per_ms PingPong.Pairs/bin/Release/$TFM/PingPongPairs.dll --sequential" \
                "Wsg ms Abm/Wsg/bin/Release/$TFM/Wsg.dll" "Flockers ms Abm/Flockers/bin/Release/$TFM/Flockers.dll"; do
      read -r name metric dll args <<< "$spec"
      # a run gives its time ("<t> ms"), or the mean msg/ms of its trials but the first ("<n> msg" then "<t> ms")
      for i in $(seq 1 "$runs"); do
        # $args unquoted: the arguments of a spec, split on purpose
        dn timeout 600 dotnet "$dll" $args | tr -d '\r' | awk -v metric="$metric" '
          /^[0-9]+ msg$/ { m = $1; next }
          /^[0-9]+(\.[0-9]+)? ms$/ { t = $1; if (m != "" && t > 0) { n++; if (n > 1) { s += m / t; k++ } } m = "" }
          END { if (metric == "ms") print t; else if (k > 0) print s / k }'
      done | awk -v name="$name" -v metric="$metric" '
        metric == "ms" && NR == 1 { next }   # the first run of a time warms up
        { v[++n] = $1 }
        END {
          for (i = 2; i <= n; i++) { x = v[i]; for (j = i - 1; j >= 1 && v[j] > x; j--) v[j + 1] = v[j]; v[j + 1] = x }
          printf "%s %s %.3f\n", name, metric, (n % 2) ? v[(n + 1) / 2] : (v[n / 2] + v[n / 2 + 1]) / 2 }'
    done
  } > "$out"
  cat "$out"
}
do_e00() {
  dn bash -c "for d in E00/*/; do n=\$(basename \$d); timeout 60 dotnet E00/\$n/bin/Release/$TFM/\$n.dll > /w/logs/e00-\$n-$FLAVOR.txt 2>&1;
    echo \"\$n exit=\$? lines=\$(wc -l < /w/logs/e00-\$n-$FLAVOR.txt)\"; done"
}
do_probe() {  # what ContainerServer/ContainerMessage.Serialize does at run time (BinaryFormatter)
  dn bash -c "dotnet build BfProbe -c Release -p:RefTfm=$TFM -o /tmp/bf >/tmp/b.log 2>&1 || tail -20 /tmp/b.log; dotnet /tmp/bf/BfProbe.dll" \
    | tee "$W/logs/bfprobe-$FLAVOR.txt"
}
do_shimcheck() {
  dn dotnet "Oracles/ShimCheck/bin/Release/$TFM/ShimCheck.dll" > "$W/logs/shimcheck-$FLAVOR.txt" 2> "$W/logs/shimcheck-$FLAVOR.err"
  if diff <(tail -n +2 "$W/logs/shimcheck-$FLAVOR.txt") <(tail -n +2 "$W/ref/expected/rng-vectors.txt") > /dev/null; then
    echo "shim check: identical to rng-vectors.txt"
  else
    echo "shim check: DIFFERENT from rng-vectors.txt" >&2; exit 1
  fi
}
do_oracles() {  # raw outputs of the oracle projects, CR removed, with a #cam header
  local seed="${CAM_SEED:-0}" runtime
  mkdir -p "$W/logs/oracles"
  runtime="$(dn bash -c 'dotnet --list-runtimes | grep Microsoft.NETCore.App | tail -1' | tr -d '\r')"
  for proj in "$W"/ref/Oracles/*/*.csproj; do
    local name dir sources stubs
    name="$(basename "$proj" .csproj)"; dir="$(dirname "$proj")"
    [ "$name" = ShimCheck ] && continue
    sources="$(oracle_sources "$proj")"
    stubs="$(sed -n 's:.*<OracleStubs>\$(MSBuildProjectDirectory)/\(.*\)</OracleStubs>.*:\1:p' "$proj")"
    stubs="$dir/${stubs:-Stubs/}"
    # the shim seeds every Random: a direct System.Random or Random.Shared would escape it
    if grep -rnE 'System\.Random|Random\.Shared' "$EX/$sources" --include='*.cs'; then
      echo "$name: System.Random used directly, the seed shim cannot apply" >&2; exit 1
    fi
    {
      echo "#cam oracle $name"
      if compgen -G "$stubs*.cs" > /dev/null || compgen -G "$dir/*.cs.sed" > /dev/null; then
        echo "#cam csharp ActressMas $REF_COMMIT, Examples/$sources (sources unchanged but the stubs and patch below, seed shim)"
      else
        echo "#cam csharp ActressMas $REF_COMMIT, Examples/$sources (sources unchanged, seed shim)"
      fi
      echo "#cam runtime $runtime"
      # what replaces original code, if anything: form stubs, bounded Program.cs
      for stub in "$stubs"*.cs; do [ -e "$stub" ] && echo "#cam stub $(basename "$stub") (form replaced, prints its state)"; done
      for sedf in "$dir"/*.cs.sed; do [ -e "$sedf" ] && sed "s/^/#cam patch $(basename "$sedf" .sed): /" "$sedf"; done
      echo "#cam command cd Oracles/$name/bin/Release/$TFM && dotnet $name.dll"
      echo "#cam seed $seed"
      # run from the output folder: data files (OracleData) are read from the working directory
      dn bash -c "cd Oracles/$name/bin/Release/$TFM && CAM_SEED=$seed timeout 300 dotnet $name.dll 2> /w/logs/oracles/$name.err" | tr -d '\r'
    } > "$W/logs/oracles/$name.txt"
    echo "$name: $(grep -vc '^#cam' "$W/logs/oracles/$name.txt") lines"
  done
}
do_mono() {   # shipped .NET Framework 4.7.2 binaries under Mono 6.8 (Ubuntu universe), installed in the container only
  podman run --rm --pull=never --name "cam-csref-mono-$$" -v "$W":/w docker.io/library/ubuntu:24.04 bash /w/ref/mono/mono-check.sh 2>&1 \
    | tee "$W/logs/mono-check.txt"
}
# E20 (ContainerServer): framework under its original identity "ActressMas, Version=3.1.0.0" (ActressMas.Wire),
# MyServerConsole and RemoteMessages without UI, BfCapture (bytes of every command), Tap (frame logging relay).
# --network=host: the Server listens on 127.0.0.1 only; host = the VM, so C++ programs in another container reach it.
E20_TFM=net8.0
e20() { podman run --rm --pull=never --network=host --name "cam-csref-e20-$$-$RANDOM" -e DOTNET_CLI_TELEMETRY_OPTOUT=1 \
          -e DOTNET_NOLOGO=1 -v "$W":/w -w /w/ref/E20 mcr.microsoft.com/dotnet/sdk:8.0 "$@"; }
do_e20build() {
  e20 bash -c "for p in ServerConsole RemoteMessages.Console MasMobileGui.Console BfCapture Tap; do dotnet build \$p -c Release -p:RefTfm=$E20_TFM || exit 1; done" \
    > "$W/logs/e20-build.log" 2>&1 || { tail -30 "$W/logs/e20-build.log"; exit 1; }
  grep -E 'Warning\(s\)|Error\(s\)' "$W/logs/e20-build.log"
}
do_capture() {
  mkdir -p "$W/logs/e20/capture"
  e20 dotnet "BfCapture/bin/Release/$E20_TFM/BfCapture.dll" /w/logs/e20/capture > "$W/logs/e20/capture.txt" 2>&1
  head -2 "$W/logs/e20/capture.txt"; grep -c '^== ' "$W/logs/e20/capture.txt"
}
do_e20() {    # E20 RemoteMessages through the Tap: Server :5000 <- Tap :5001 <- Container2 (pong), Container1 (ping), duplicate Container1
  mkdir -p "$W/logs/e20"
  e20 bash -c "B=bin/Release/$E20_TFM; L=/w/logs/e20
    timeout 40 dotnet ServerConsole/\$B/ServerConsole.dll 5000 3000 30 > \$L/server.txt 2>&1 &
    sleep 3
    timeout 38 dotnet Tap/\$B/Tap.dll 5001 5000 \$L/tap.log 28 > \$L/tap.txt 2>&1 &
    sleep 2
    timeout 30 dotnet RemoteMessages.Console/\$B/RemoteMessages.Console.dll 127.0.0.1 5001 Container2 2000 18 > \$L/container2.txt 2>&1 &
    sleep 2
    timeout 30 dotnet RemoteMessages.Console/\$B/RemoteMessages.Console.dll 127.0.0.1 5001 Container1 4000 12 > \$L/container1.txt 2>&1 &
    sleep 12
    timeout 15 dotnet RemoteMessages.Console/\$B/RemoteMessages.Console.dll 127.0.0.1 5001 Container1 1000 3 > \$L/container1-dup.txt 2>&1 &
    wait; echo done"
  for f in server container2 container1 container1-dup; do echo "== $f"; cat "$W/logs/e20/$f.txt"; done
}
do_e20mono() {  # runtime cross-check: shipped .NET Framework Server + MasMobileConsole (Mono) <-> Tap + RemoteMessages (.NET 8)
  local run; run="$(date +%Y%m%d-%H%M%S)"; mkdir -p "$W/logs/e20mono-$run"
  podman run --rm --pull=never --network=host --name "cam-csref-e20mono-$$" -e RUN="$run" -v "$W":/w docker.io/library/ubuntu:24.04 \
    bash /w/ref/mono/e20-mono.sh > "$W/logs/e20mono-$run/mono-side.txt" 2>&1 &
  local mono=$!
  e20 bash -c "B=bin/Release/$E20_TFM; L=/w/logs/e20mono-$run
    for i in \$(seq 240); do [ -f \$L/server.ready ] && break; sleep 1; done
    timeout 160 dotnet Tap/\$B/Tap.dll 5001 5000 \$L/tap.log 150 > \$L/tap.txt 2>&1 &
    sleep 3; touch \$L/tap.ready
    for i in \$(seq 120); do [ -f \$L/mobile.done ] && break; sleep 1; done
    sleep 2
    timeout 30 dotnet RemoteMessages.Console/\$B/RemoteMessages.Console.dll 127.0.0.1 5001 Container2 2000 15 > \$L/container2.txt 2>&1 &
    sleep 2
    timeout 30 dotnet RemoteMessages.Console/\$B/RemoteMessages.Console.dll 127.0.0.1 5001 Container1 3000 10 > \$L/container1.txt 2>&1
    wait; echo dotnet side done" > "$W/logs/e20mono-$run/dotnet-side.txt" 2>&1
  wait $mono
  echo "logs: $W/logs/e20mono-$run"
  for f in mono-side dotnet-side c1 c2 c3 container2 container1; do echo "== $f"; cat "$W/logs/e20mono-$run/$f.txt" 2>/dev/null; done
}
do_e20gui() {  # E20 MasMobileGui without WinForms (ref/E20/MasMobileGui.Console): ServerConsole :5000, Container2 and
               # Container3, then Container1 (the original readme: 2 and 3 before 1); "Run MAS" 10 s after "Connect".
               # The first move fails on .NET 8: Agent.Move serializes a System.Type, which BinaryFormatter refuses
               # outside .NET Framework; an oracle of this example needs Mono.
  mkdir -p "$W/logs/e20gui"
  e20 bash -c "B=bin/Release/$E20_TFM; L=/w/logs/e20gui
    timeout 45 dotnet ServerConsole/\$B/ServerConsole.dll 5000 3000 40 > \$L/server.txt 2>&1 &
    sleep 3
    for n in 2 3; do timeout 40 dotnet MasMobileGui.Console/\$B/MasMobileGui.Console.dll 127.0.0.1 5000 Container\$n 10000 24 > \$L/c\$n.txt 2>&1 & done
    sleep 4
    timeout 36 dotnet MasMobileGui.Console/\$B/MasMobileGui.Console.dll 127.0.0.1 5000 Container1 10000 20 > \$L/c1.txt 2>&1 &
    wait; echo done"
  for f in server c1 c2 c3; do echo "== $f"; cat "$W/logs/e20gui/$f.txt"; done
}
do_e20oracles() {
  local mono; mono="$(ls -d "$W"/logs/e20mono-* | sort | tail -1)"
  mkdir -p "$W/logs/oracles"
  e20_oracle RemoteMessages "$W/logs/e20" "E20 Mobile Examples/RemoteMessages/ (agents and MasSetup unchanged in ref/E20/RemoteMessages.Console, the form replaced by the console)" \
    "Microsoft.NETCore.App 8 (ActressMas.Wire), ref/E20/ServerConsole" container1:Container1 container2:Container2
  e20_oracle MasMobileConsole "$mono" "E20 Mobile Examples/MasMobileConsole/ (the shipped binaries of bin/)" \
    "Mono 6.8 (.NET Framework 4.7.2 binaries), the shipped MyServerConsole" c1:Container1 c2:Container2 c3:Container3
}
e20_oracle() {  # name, capture folder, sources, runtime, then file:container for each process
  local name="$1" dir="$2" sources="$3" runtime="$4"; shift 4
  {
    echo "#cam oracle $name"
    echo "#cam csharp ActressMas $REF_COMMIT, Examples/$sources"
    echo "#cam runtime $runtime"
    echo "#cam capture logs/$(basename "$dir") (csharp-ref.sh e20 or e20mono): one process per container, its console below"
    for part in "$@"; do
      echo "#cam container ${part#*:}"
      tr -d '\r' < "$dir/${part%%:*}.txt"
    done
  } > "$W/logs/oracles/$name.txt"
  echo "$name: $(grep -vc '^#cam' "$W/logs/oracles/$name.txt") lines"
}
do_distrib() {  # outputs of some oracle projects at many seeds, for the distribution tests
  local seeds="${CAM_DISTRIB_SEEDS:-30}" names="${CAM_DISTRIB:-PredatorPrey Reactive ContractNet Shapley}" name
  for name in $names; do
    mkdir -p "$W/logs/distrib/$name"
    dn bash -c "cd Oracles/$name/bin/Release/$TFM && for s in \$(seq 1 $seeds); do CAM_SEED=\$s timeout 300 dotnet $name.dll 2> /dev/null | tr -d '\\r' > /w/logs/distrib/$name/seed\$s.txt; done"
    echo "$name: $(ls "$W/logs/distrib/$name" | wc -l) runs"
  done
}

case "$ACTION" in
  build) do_build ;; rng) do_rng ;; trace) do_trace ;; format) do_format ;; bench) do_bench ;; benchref) do_benchref ;; e00) do_e00 ;; probe) do_probe ;; mono) do_mono ;;
  shimcheck) do_shimcheck ;; oracles) do_oracles ;; distrib) do_distrib ;;
  e20build) do_e20build ;; capture) do_capture ;; e20) do_e20 ;; e20mono) do_e20mono ;; e20gui) do_e20gui ;; e20oracles) do_e20oracles ;;
  all) do_build; do_rng; do_trace; do_format; do_bench; do_e00; do_probe; do_shimcheck; do_oracles ;;
  *) echo "unknown action $ACTION" >&2; exit 2 ;;
esac
