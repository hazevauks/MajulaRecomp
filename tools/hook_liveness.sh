#!/usr/bin/env bash
# Which hooks of app/src name a function of the generated code (2026-10-29, the move to Scholar of the First Sin,
# docs/dlc.md): a hook on an address the generator did not make a function of never runs; one that lands on a function
# it was not written for replaces that function. Run after every codegen; compare with the previous run's list.
#   bash tools/hook_liveness.sh   -> out/hooks_live.txt (address, file)
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
cat app/generated/default/*_recomp.*.cpp | grep -oE "^DEFINE_REX_FUNC\(sub_[0-9A-F]{8}" | cut -c21- | sort -u > out/functions.txt
grep -rnoE "(BANJO_RENUT_HOOK(_RAW|_SHARED)?|REX_HOOK(_RAW)?|DS2_TRACE_FOG)\(\s*(sub_)?[0-9A-Fa-f]{8}" app/src \
    --include=*.cpp --include=*.h | sed -E 's/^([^:]+):[0-9]+:.*\(\s*(sub_)?([0-9A-Fa-f]{8})/\3 \1/' |
    awk '{print toupper($1), $2}' | sort -u > out/hooks_all.txt
join out/hooks_all.txt out/functions.txt > out/hooks_live.txt
echo "hooks $(cut -d' ' -f1 out/hooks_all.txt | sort -u | wc -l), on a function $(cut -d' ' -f1 out/hooks_live.txt | sort -u | wc -l): out/hooks_live.txt"
