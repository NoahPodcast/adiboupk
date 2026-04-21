"""
Simule un enrichissement VirusTotal.
Requiert requests==2.28.0 (ancienne API interne qui dépend de cette version).
"""
import sys
import requests

def main():
    target = sys.argv[1] if len(sys.argv) > 1 else "8.8.8.8"

    print(f"[Enrichments/virustotal_lookup.py]")
    print(f"  Target:           {target}")
    print(f"  requests version: {requests.__version__}")
    print()

    # Vérifie qu'on a bien la bonne version
    assert requests.__version__ == "2.28.0", \
        f"ERREUR: attendu requests==2.28.0, obtenu {requests.__version__}"

    print(f"  -> OK: version correcte, enrichissement simulé pour {target}")

if __name__ == "__main__":
    main()
