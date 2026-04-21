#!/usr/bin/env bash
# Démo : le problème des conflits de dépendances et la solution adiboupk
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

ADIBOUPK="${SCRIPT_DIR}/../build/adiboupk"
ADIBOUPK="$(realpath "${1:-$ADIBOUPK}")"

red()   { echo -e "\033[1;31m$*\033[0m"; }
green() { echo -e "\033[1;32m$*\033[0m"; }
blue()  { echo -e "\033[1;34m$*\033[0m"; }
sep()   { echo "────────────────────────────────────────────────────"; }

# ═══════════════════════════════════════════════════════
# PARTIE 1 : Le problème (pip install global)
# ═══════════════════════════════════════════════════════
blue "PARTIE 1 : Le problème — pip install global"
sep
echo ""
echo "Deux groupes de scripts avec des dépendances conflictuelles :"
echo "  Enrichments/requirements.txt → requests==2.28.0"
echo "  Responses/requirements.txt   → requests==2.32.3"
echo ""

blue "Création d'un venv global et installation des deux requirements.txt..."
python3 -m venv /tmp/adiboupk_demo_global_venv
GLOBAL_PIP="/tmp/adiboupk_demo_global_venv/bin/pip"
GLOBAL_PYTHON="/tmp/adiboupk_demo_global_venv/bin/python"

$GLOBAL_PIP install -q -r Enrichments/requirements.txt
echo "  pip install -r Enrichments/requirements.txt  ✓"
$GLOBAL_PIP install -q -r Responses/requirements.txt
echo "  pip install -r Responses/requirements.txt   ✓"
echo ""

blue "Résultat : quelle version de requests est installée ?"
$GLOBAL_PYTHON -c "import requests; print(f'  requests=={requests.__version__}')"
echo ""

red "Le dernier pip install a écrasé les versions du premier !"
echo ""

blue "Test : exécution de Enrichments/virustotal_lookup.py avec le venv global..."
if $GLOBAL_PYTHON Enrichments/virustotal_lookup.py 8.8.8.8 2>&1; then
    green "  Succès (inattendu)"
else
    red "  ÉCHEC : le script plante car requests n'est pas 2.28.0"
fi
echo ""

blue "Test : exécution de Responses/send_alert.py avec le venv global..."
if $GLOBAL_PYTHON Responses/send_alert.py malware_detected 2>&1; then
    green "  Succès (celui-ci marche car c'est le dernier installé)"
else
    red "  ÉCHEC"
fi

rm -rf /tmp/adiboupk_demo_global_venv
echo ""
sep
echo ""

# ═══════════════════════════════════════════════════════
# PARTIE 2 : La solution (adiboupk)
# ═══════════════════════════════════════════════════════
blue "PARTIE 2 : La solution — adiboupk"
sep
echo ""

blue "adiboupk setup (scan + install + audit)"
$ADIBOUPK --root "$SCRIPT_DIR" setup
echo ""

blue "Test : exécution de Enrichments/virustotal_lookup.py via adiboupk..."
$ADIBOUPK --root "$SCRIPT_DIR" run Enrichments/virustotal_lookup.py 8.8.8.8
echo ""

blue "Test : exécution de Responses/send_alert.py via adiboupk..."
$ADIBOUPK --root "$SCRIPT_DIR" run Responses/send_alert.py malware_detected
echo ""

sep
green "Les deux scripts fonctionnent avec leurs propres versions !"
green "Chaque groupe a son venv isolé, aucun conflit."
echo ""

blue "Nettoyage..."
$ADIBOUPK --root "$SCRIPT_DIR" clean
rm -f "$SCRIPT_DIR/adiboupk.json" "$SCRIPT_DIR/adiboupk.lock"
