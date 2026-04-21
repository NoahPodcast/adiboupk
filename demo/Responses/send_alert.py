"""
Simule l'envoi d'une alerte sécurité.
Requiert requests==2.32.3 (nouvelle API qui utilise les dernières features).
"""
import sys
import requests

def main():
    alert = sys.argv[1] if len(sys.argv) > 1 else "malware_detected"

    print(f"[Responses/send_alert.py]")
    print(f"  Alert:            {alert}")
    print(f"  requests version: {requests.__version__}")
    print()

    # Vérifie qu'on a bien la bonne version
    assert requests.__version__ == "2.32.3", \
        f"ERREUR: attendu requests==2.32.3, obtenu {requests.__version__}"

    print(f"  -> OK: version correcte, alerte '{alert}' envoyée (simulé)")

if __name__ == "__main__":
    main()
