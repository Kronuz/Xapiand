#!/usr/bin/env bash
# Create/migrate/deploy the multi-tenant Worker. Tenant configuration is submitted
# separately with PUT /:tenant/config after deployment.
set -uo pipefail
cd "$(dirname "$0")"

WRANGLER="npx wrangler"
say()  { printf '\n\033[1;32m==> %s\033[0m\n' "$*"; }
die()  { printf '\033[1;31mERROR: %s\033[0m\n' "$*" >&2; exit 1; }
sedi() { sed -i.bak "$@" && rm -f "${@: -1}.bak" 2>/dev/null || true; }
random_secret() { openssl rand -hex 32; }

# Optional local source of the deployment-wide secrets. This file is gitignored. Empty
# values mean "preserve the remote secret if it exists, otherwise generate one".
if [ -f secrets.sh ]; then
  set -a
  # shellcheck disable=SC1091
  . ./secrets.sh
  set +a
fi

command -v npx >/dev/null || die "node/npx not found"
command -v openssl >/dev/null || die "openssl not found"
[ -f wrangler.toml ] || die "Copy wrangler.toml.example to wrangler.toml and configure PUBLIC_BASE_URL"
$WRANGLER whoami >/dev/null 2>&1 || die "Run: npx wrangler login"

if grep -q 'database_id = "local-dev-placeholder"' wrangler.toml; then
  say "Creating D1 database"
  out=$($WRANGLER d1 create discussions) || die "d1 create failed"
  printf '%s\n' "$out"
  id=$(printf '%s' "$out" | grep -oiE '[0-9a-f]{8}-[0-9a-f-]{27,}' | head -1)
  [ -n "$id" ] || die "Could not parse the D1 database ID"
  sedi "s/database_id = \"local-dev-placeholder\"/database_id = \"$id\"/" wrangler.toml
fi

say "Applying D1 migrations"
$WRANGLER d1 migrations apply discussions --remote || die "migrations failed"

for name in SESSION_SECRET CONFIG_MASTER_KEY SERVICE_ADMIN_TOKEN; do
  if $WRANGLER secret list 2>/dev/null | grep -q "\"name\": \"$name\""; then
    say "$name already exists"
  else
    say "Creating $name"
    value=${!name:-}
    generated=0
    if [ -z "$value" ]; then
      value=$(random_secret)
      generated=1
    fi
    printf '%s' "$value" | $WRANGLER secret put "$name" || die "failed to set $name"
    if { [ "$name" = CONFIG_MASTER_KEY ] || [ "$name" = SERVICE_ADMIN_TOKEN ]; } && [ "$generated" -eq 1 ]; then
      printf '\nSave this %s now; it will not be shown again:\n%s\n' "$name" "$value"
    fi
  fi
done

say "Deploying Worker"
$WRANGLER deploy || die "deploy failed"

base=$(grep -oE 'PUBLIC_BASE_URL = "[^"]*"' wrangler.toml | sed 's/.*"\(.*\)"/\1/')
cat <<TXT

Deploy complete.

Create or replace a tenant by copying tenant-config.example.json, filling every value,
then sending the complete document:

  curl -X PUT '$base/my-blog/config' \\
    -H 'Authorization: Bearer <SERVICE_ADMIN_TOKEN>' \\
    -H 'Content-Type: application/json' \\
    --data-binary @tenant-config.json

OAuth callback for that tenant:
  $base/my-blog/auth/callback
TXT
