#!/usr/bin/env bash
# Deploy adiboupk documentation on Ubuntu Server.
#
# Creates a dedicated system user, installs Docker if needed,
# clones the docs branch, and starts MkDocs + Cloudflare Tunnel.
#
# Usage: sudo ./setup.sh --tunnel-token <CLOUDFLARE_TUNNEL_TOKEN>
#
# Options:
#   --tunnel-token TOKEN   Cloudflare Tunnel token (required)
#   --cf-zone-id ID        Cloudflare Zone ID (for cache purge, optional)
#   --cf-api-token TOKEN   Cloudflare API token with Cache Purge permission (optional)
#   --user USER            System user to create (default: adiboupk)
#   --install-dir DIR      Installation directory (default: /opt/adiboupk-docs)
#   --port PORT            MkDocs port (default: 8000)

set -euo pipefail

APP_USER="adiboupk"
INSTALL_DIR="/opt/adiboupk-docs"
PORT=8000
TUNNEL_TOKEN=""
CF_ZONE_ID=""
CF_API_TOKEN=""

info()  { printf '\033[1;34m==>\033[0m %s\n' "$*"; }
error() { printf '\033[1;31m==>\033[0m %s\n' "$*" >&2; exit 1; }

while [[ $# -gt 0 ]]; do
    case "$1" in
        --tunnel-token)  TUNNEL_TOKEN="$2"; shift 2 ;;
        --cf-zone-id)   CF_ZONE_ID="$2"; shift 2 ;;
        --cf-api-token) CF_API_TOKEN="$2"; shift 2 ;;
        --user)         APP_USER="$2"; shift 2 ;;
        --install-dir)  INSTALL_DIR="$2"; shift 2 ;;
        --port)         PORT="$2"; shift 2 ;;
        *)              error "Unknown option: $1" ;;
    esac
done

if [ -z "$TUNNEL_TOKEN" ]; then
    error "Missing required option: --tunnel-token <TOKEN>"
fi

if [ "$(id -u)" -ne 0 ]; then
    error "This script must be run as root (sudo)"
fi

# --- Create dedicated user ---
if ! id "$APP_USER" &>/dev/null; then
    info "Creating system user '$APP_USER'..."
    useradd --system --create-home --shell /usr/sbin/nologin "$APP_USER"
fi

# --- Install Docker if not present ---
if ! command -v docker &>/dev/null; then
    info "Installing Docker..."
    apt-get update -qq
    apt-get install -y -qq ca-certificates curl gnupg >/dev/null
    install -m 0755 -d /etc/apt/keyrings
    curl -fsSL https://download.docker.com/linux/ubuntu/gpg | gpg --dearmor -o /etc/apt/keyrings/docker.gpg
    chmod a+r /etc/apt/keyrings/docker.gpg
    echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] \
https://download.docker.com/linux/ubuntu $(. /etc/os-release && echo "$VERSION_CODENAME") stable" \
        > /etc/apt/sources.list.d/docker.list
    apt-get update -qq
    apt-get install -y -qq docker-ce docker-ce-cli containerd.io docker-compose-plugin >/dev/null
fi

# --- Add user to docker group ---
if ! groups "$APP_USER" | grep -q docker; then
    info "Adding '$APP_USER' to docker group..."
    usermod -aG docker "$APP_USER"
fi

# --- Clone or update docs branch ---
if [ -d "$INSTALL_DIR/.git" ]; then
    info "Updating existing installation..."
    sudo -u "$APP_USER" git -C "$INSTALL_DIR" pull --ff-only
else
    info "Cloning docs branch to $INSTALL_DIR..."
    git clone --branch docs --single-branch \
        https://github.com/NoahPodcast/adiboupk.git "$INSTALL_DIR"
    chown -R "$APP_USER:$APP_USER" "$INSTALL_DIR"
fi

# --- Write tunnel token to .env ---
info "Writing tunnel configuration..."
cat > "$INSTALL_DIR/.env" <<ENVEOF
CLOUDFLARE_TUNNEL_TOKEN=$TUNNEL_TOKEN
MKDOCS_PORT=$PORT
CF_ZONE_ID=$CF_ZONE_ID
CF_API_TOKEN=$CF_API_TOKEN
ENVEOF
chown "$APP_USER:$APP_USER" "$INSTALL_DIR/.env"
chmod 600 "$INSTALL_DIR/.env"

# --- Start services ---
info "Starting services..."
cd "$INSTALL_DIR"
sudo -u "$APP_USER" docker compose up -d --build

# --- Install auto-update cron (every 5 minutes) ---
CRON_CMD="cd $INSTALL_DIR && ./update.sh"
CRON_LINE="*/5 * * * * $CRON_CMD"
if ! crontab -u "$APP_USER" -l 2>/dev/null | grep -qF "$CRON_CMD"; then
    info "Installing auto-update cron job..."
    (crontab -u "$APP_USER" -l 2>/dev/null || true; echo "$CRON_LINE") | crontab -u "$APP_USER" -
fi

# --- Create log file ---
touch /var/log/adiboupk-docs-update.log
chown "$APP_USER:$APP_USER" /var/log/adiboupk-docs-update.log

info "Done."
info "MkDocs is running on port $PORT (localhost only)."
info "Cloudflare Tunnel is exposing it to your configured domain."
info "Auto-update checks GitHub every 5 minutes."
info ""
info "Manage with:"
info "  cd $INSTALL_DIR && sudo -u $APP_USER docker compose logs -f"
info "  cd $INSTALL_DIR && sudo -u $APP_USER docker compose restart"
info "  cd $INSTALL_DIR && sudo -u $APP_USER docker compose down"
info "  tail -f /var/log/adiboupk-docs-update.log"
