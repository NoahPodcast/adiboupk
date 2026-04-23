# adiboupk — Documentation

Documentation for the [adiboupk](https://github.com/NoahPodcast/adiboupk) project, built with [MkDocs](https://www.mkdocs.org/) and the [Material](https://squidfunk.github.io/mkdocs-material/) theme.

## Contents

| Page | Description |
|------|-------------|
| [Home](docs/index.md) | Project overview |
| [Installation](docs/installation.md) | Installation guide |
| [Concepts](docs/concepts.md) | How it works |
| [Configuration](docs/configuration.md) | `adiboupk.json` reference |
| [Commands](docs/commands.md) | CLI commands |
| [Per-Package Isolation](docs/isolation.md) | Per-package isolation mode |
| [Tutorial](docs/tutorial.md) | Step-by-step guide |

## Deploy on Ubuntu Server

The setup script creates a dedicated system user, installs Docker, and starts MkDocs behind a Cloudflare Tunnel. A cron job checks for updates every 5 minutes.

### Prerequisites

1. An Ubuntu Server with root access
2. A [Cloudflare Tunnel](https://developers.cloudflare.com/cloudflare-one/connections/connect-networks/) token — create one in the Cloudflare Zero Trust dashboard and point it to `http://docs:8000`

### Quick start

```bash
git clone --branch docs --single-branch https://github.com/NoahPodcast/adiboupk.git
cd adiboupk
sudo ./setup.sh --tunnel-token <YOUR_TUNNEL_TOKEN>
```

This will:
- Create a system user `adiboupk` (no login shell)
- Install Docker if not already present
- Start MkDocs on `127.0.0.1:8000` (not exposed publicly)
- Start a Cloudflare Tunnel container to expose the site on your domain
- Install a cron job that pulls and rebuilds every 5 minutes if changes are detected

### Options

```bash
sudo ./setup.sh --tunnel-token <TOKEN>                # Required
sudo ./setup.sh --tunnel-token <TOKEN> --port 9000    # Custom port
sudo ./setup.sh --tunnel-token <TOKEN> --user docs    # Custom username
sudo ./setup.sh --tunnel-token <TOKEN> --install-dir /srv/docs  # Custom path
```

### Manage the services

```bash
cd /opt/adiboupk-docs

# Logs
sudo -u adiboupk docker compose logs -f

# Restart
sudo -u adiboupk docker compose restart

# Stop
sudo -u adiboupk docker compose down

# Auto-update logs
tail -f /var/log/adiboupk-docs-update.log

# Force manual update
sudo -u adiboupk ./update.sh
```

## Local development

### Using docker compose

```bash
docker compose up -d
```

Docs are available at `http://localhost:8000` with live reload.

### Using docker directly

```bash
docker build -t adiboupk-docs .
docker run -d -p 8000:8000 \
  -v ./docs:/docs/docs \
  -v ./mkdocs.yml:/docs/mkdocs.yml \
  adiboupk-docs
```
