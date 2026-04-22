# adiboupk — Documentation

Documentation du projet [adiboupk](https://github.com/NoahPodcast/adiboupk), générée avec [MkDocs](https://www.mkdocs.org/) et le thème [Material](https://squidfunk.github.io/mkdocs-material/).

## Contenu

| Page | Description |
|------|-------------|
| [Home](docs/index.md) | Présentation du projet |
| [Installation](docs/installation.md) | Guide d'installation |
| [Concepts](docs/concepts.md) | Fonctionnement interne |
| [Configuration](docs/configuration.md) | Référence `adiboupk.json` |
| [Commands](docs/commands.md) | Toutes les commandes CLI |
| [Per-Package Isolation](docs/isolation.md) | Isolation par paquet |
| [Tutorial](docs/tutorial.md) | Guide pas à pas |

## Déployer sur Ubuntu Server

Le script `setup.sh` installe les dépendances et lance le serveur MkDocs.

### Lancer le serveur

```bash
sudo ./setup.sh
```

Par défaut, le serveur écoute sur `0.0.0.0:8000`.

### Options

```bash
# Port et host personnalisés
sudo ./setup.sh --port 9000 --host 127.0.0.1

# Générer le site statique sans serveur
sudo ./setup.sh --build-only

# Installer en tant que service systemd (démarrage automatique)
sudo ./setup.sh --systemd
```

### Gérer le service systemd

```bash
sudo systemctl status mkdocs-adiboupk
sudo systemctl stop mkdocs-adiboupk
sudo systemctl restart mkdocs-adiboupk
sudo journalctl -u mkdocs-adiboupk -f
```
