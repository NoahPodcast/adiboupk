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

The `setup.sh` script installs dependencies and starts the MkDocs server.

### Start the server

```bash
sudo ./setup.sh
```

By default, the server listens on `0.0.0.0:8000`.

### Options

```bash
# Custom port and host
sudo ./setup.sh --port 9000 --host 127.0.0.1

# Build static site without starting a server
sudo ./setup.sh --build-only

# Install as a systemd service (auto-start on boot)
sudo ./setup.sh --systemd
```

### Manage the systemd service

```bash
sudo systemctl status mkdocs-adiboupk
sudo systemctl stop mkdocs-adiboupk
sudo systemctl restart mkdocs-adiboupk
sudo journalctl -u mkdocs-adiboupk -f
```
