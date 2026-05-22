#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

assert_contains() {
  local file="$1"
  local pattern="$2"
  if ! grep -q "$pattern" "$file"; then
    echo "missing expected JSONL pattern: $pattern" >&2
    exit 1
  fi
}

cd "$ROOT"

echo "== Mac runner and unit tests =="
(
  cd lastbreach-mac/lastbreach-mac
  make
  make test
)

echo "== Deterministic three-day scenario =="
SIM_START=$SECONDS
./lastbreach-mac/lastbreach-mac/lastbreach \
  dsl/joel.lbp dsl/mara.lbp \
  --world dsl/world.lbw \
  --catalog dsl/catalog.lbc \
  --days 3 \
  --seed 1337 \
  --json > "$TMP_DIR/run-a.jsonl"

./lastbreach-mac/lastbreach-mac/lastbreach \
  dsl/joel.lbp dsl/mara.lbp \
  --world dsl/world.lbw \
  --catalog dsl/catalog.lbc \
  --days 3 \
  --seed 1337 \
  --json > "$TMP_DIR/run-b.jsonl"
SIM_SECONDS=$((SECONDS - SIM_START))

cmp -s "$TMP_DIR/run-a.jsonl" "$TMP_DIR/run-b.jsonl"

TICK_COUNT="$(grep -c '"type":"tick_snapshot"' "$TMP_DIR/run-a.jsonl")"
if [ "$TICK_COUNT" -ne 72 ]; then
  echo "expected 72 tick snapshots, got $TICK_COUNT" >&2
  exit 1
fi

assert_contains "$TMP_DIR/run-a.jsonl" '"type":"run_start"'
assert_contains "$TMP_DIR/run-a.jsonl" '"type":"final_state"'
assert_contains "$TMP_DIR/run-a.jsonl" '"type":"simulation_complete","days":3'
assert_contains "$TMP_DIR/run-a.jsonl" '"type":"harvest"'
assert_contains "$TMP_DIR/run-a.jsonl" '"type":"breach"'
assert_contains "$TMP_DIR/run-a.jsonl" '"task":"Water filtration"'
assert_contains "$TMP_DIR/run-a.jsonl" '"task":"Watering plants"'
assert_contains "$TMP_DIR/run-a.jsonl" '"task":"Meal prep"'
assert_contains "$TMP_DIR/run-a.jsonl" '"task":"Sleeping"'
assert_contains "$TMP_DIR/run-a.jsonl" '"item_id":"carrot"'
assert_contains "$TMP_DIR/run-a.jsonl" '"item_id":"basil"'

if [ "$SIM_SECONDS" -gt 5 ]; then
  echo "warning: three-day JSONL run took ${SIM_SECONDS}s; expected under 5s on a local development machine" >&2
else
  echo "three-day JSONL run completed in ${SIM_SECONDS}s"
fi

echo "== iOS simulator build =="
IOS_START=$SECONDS
xcodebuild \
  -project lastbreach-ios/lastbreach-ios.xcodeproj \
  -scheme lastbreach-ios \
  -destination 'generic/platform=iOS Simulator' \
  -derivedDataPath "$TMP_DIR/DerivedData" \
  build
IOS_SECONDS=$((SECONDS - IOS_START))
echo "iOS simulator build completed in ${IOS_SECONDS}s"

echo "release-candidate check passed"
