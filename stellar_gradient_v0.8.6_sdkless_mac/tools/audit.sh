#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
python3 tools/verify_metadata.py
python3 tools/verify_metal_perf.py
python3 tools/verify_ae_contract.py
python3 tools/verify_code_freeze.py
python3 tools/verify_sdkless_host.py
python3 tools/verify_v08_host_freeze.py

rm -rf build-strict build-asan build-tsan
cmake -S . -B build-strict -G Ninja -DCMAKE_BUILD_TYPE=Release -DSTELLAR_WARNINGS_AS_ERRORS=ON >/dev/null
cmake --build build-strict -j2 >/dev/null
ctest --test-dir build-strict --output-on-failure

cmake -S . -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSTELLAR_SANITIZE=ON >/dev/null
cmake --build build-asan -j2 >/dev/null
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1 ctest --test-dir build-asan --output-on-failure

cmake -S . -B build-tsan -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSTELLAR_TSAN=ON >/dev/null
cmake --build build-tsan -j2 >/dev/null
TSAN_OPTIONS=halt_on_error=1 ctest --test-dir build-tsan --output-on-failure

echo "--- SDK-less native bridge regression ---"
BRIDGE_TMP="$ROOT/.bridge-audit"
rm -rf "$BRIDGE_TMP" && mkdir -p "$BRIDGE_TMP"
CXX="${CXX:-c++}"
COMMON=(sdkless/bridge/StellarBridge.cpp src/core/RenderPlan.cpp src/cpu/ReferenceRenderer.cpp)

for test in bridge_parity_test bridge_formats_test bridge_mfr_test; do
  "$CXX" -std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror -fno-fast-math -ffp-contract=off \
    "sdkless/bridge/${test}.cpp" "${COMMON[@]}" -pthread -o "$BRIDGE_TMP/$test"
  "$BRIDGE_TMP/$test"
done

"$CXX" -std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  sdkless/bridge/bridge_parity_test.cpp "${COMMON[@]}" -pthread -o "$BRIDGE_TMP/bridge_asan"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1 "$BRIDGE_TMP/bridge_asan"

"$CXX" -std=c++17 -O1 -g -fsanitize=thread -fno-omit-frame-pointer \
  sdkless/bridge/bridge_mfr_test.cpp "${COMMON[@]}" -pthread -o "$BRIDGE_TMP/bridge_tsan"
TSAN_OPTIONS=halt_on_error=1 "$BRIDGE_TMP/bridge_tsan"

echo "--- CPU regression benchmark ---"
./build-strict/stellar_bench
