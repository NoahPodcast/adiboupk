#!/usr/bin/env bash
# Auto-update adiboupk docs: pull latest changes, rebuild, and purge CDN cache.
# Designed to run via cron every 5 minutes.

set -euo pipefail

INSTALL_DIR="${INSTALL_DIR:-/opt/adiboupk-docs}"
LOG_FILE="/var/log/adiboupk-docs-update.log"

log() { printf '[%s] %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$*" >> "$LOG_FILE"; }

cd "$INSTALL_DIR"

# Load .env if present (for CF_ZONE_ID and CF_API_TOKEN)
if [ -f .env ]; then
    set -a
    source .env
    set +a
fi

# Fetch latest changes
git fetch origin docs --quiet 2>/dev/null || { log "ERROR: git fetch failed"; exit 1; }

# Check if there are new commits
LOCAL=$(git rev-parse HEAD)
REMOTE=$(git rev-parse origin/docs)

if [ "$LOCAL" = "$REMOTE" ]; then
    exit 0
fi

log "Update detected: $LOCAL -> $REMOTE"

# Pull changes
git reset --hard origin/docs --quiet
log "Pulled latest changes"

# Rebuild and restart docs container only
docker compose up -d --build docs
log "Rebuild complete"

# Purge Cloudflare cache
if [ -n "${CF_ZONE_ID:-}" ] && [ -n "${CF_API_TOKEN:-}" ]; then
    response=$(curl -s -X POST \
        "https://api.cloudflare.com/client/v4/zones/${CF_ZONE_ID}/purge_cache" \
        -H "Authorization: Bearer ${CF_API_TOKEN}" \
        -H "Content-Type: application/json" \
        --data '{"purge_everything":true}')
    if echo "$response" | grep -q '"success":true'; then
        log "Cloudflare cache purged"
    else
        log "WARNING: Cloudflare cache purge failed: $response"
    fi
fi
