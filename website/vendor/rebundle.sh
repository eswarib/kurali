#!/usr/bin/env bash
# Rebuild ./supabase-js.mjs after changing @supabase/supabase-js version.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TARGET="$ROOT/website/vendor/supabase-js.mjs"
TMP="$(mktemp -d)"
cleanup() { rm -rf "$TMP"; }
trap cleanup EXIT
cd "$TMP"
npm init -y >/dev/null 2>&1
npm install "@supabase/supabase-js@${1:-2.49.8}" esbuild@0.24.2 >/dev/null 2>&1
npx esbuild ./node_modules/@supabase/supabase-js/dist/module/index.js \
  --bundle --format=esm --platform=browser --outfile="$TARGET"
echo "Wrote $TARGET ($(wc -c < "$TARGET") bytes)"
