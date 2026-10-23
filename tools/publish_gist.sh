#!/usr/bin/env bash
# Publish all 4 pages to the gist with a build label: "Build N · <short hash> · Mon D H:MM AM/PM CT".
# Increments tools/BUILD_NUMBER, rebuilds, commits LOCALLY (never pushes), gh gist edit, verifies 200 + match.
set -euo pipefail
cd "$(dirname "$0")/.."
G=1788c8fcff378fb83d20d0198b5290cc
N=$(( $(cat tools/BUILD_NUMBER 2>/dev/null || echo 0) + 1 ))
H=$(git rev-parse --short HEAD)
D=$(TZ=America/Chicago date '+%b %-d %-I:%M %p')
export BUILD_LABEL="Build $N · $H · $D CT"
echo "$BUILD_LABEL"
scripts/build_bms_sim_html.sh
python3 tools/build_wiring.py
python3 tools/build_checklist.py
echo "$N" > tools/BUILD_NUMBER
git add tools/parts.json tools/parts.js tools/BUILD_NUMBER tools/bms_sim.html tools/dash.html tools/wiring.html tools/checklist.html
git commit -qm "Publish $BUILD_LABEL (gist)"
gh gist edit $G -f index.html tools/bms_sim.html
gh gist edit $G -f dash.html tools/dash.html
gh gist edit $G -f wiring.html tools/wiring.html
gh gist edit $G -f checklist.html tools/checklist.html
R=$(gh api gists/$G --jq '.history[0].version'); echo "gist rev $R"
ok=1
for p in index.html:tools/bms_sim.html dash.html:tools/dash.html wiring.html:tools/wiring.html checklist.html:tools/checklist.html; do
  n=${p%%:*}; l=${p#*:}
  c=$(curl -s -o /tmp/pub_$n -w '%{http_code}' "https://gist.githubusercontent.com/Adampaleski/$G/raw/$R/$n")
  if cmp -s /tmp/pub_$n "$l"; then m=match; else m=DIFF; ok=0; fi
  echo "$n $c $m"
done
[ $ok = 1 ]
