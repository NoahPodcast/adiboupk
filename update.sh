#!/usr/bin/env bash
# Auto-update adiboupk docs: pull latest changes and rebuild if needed.
# Designed to run via cron every 5 minutes.

set -euo pipefail

INSTALL_DIR="${INSTALL_DIR:-/opt/adiboupk-docs}"
LOG_FILE="/var/log/adiboupk-docs-update.log"

log() { printf '[%s] %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$*" >> "$LOG_FILE"; }

cd "$INSTALL_DIR"

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
