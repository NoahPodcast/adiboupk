"""Test that both requests and urllib3 can be imported independently."""
import requests
import urllib3

# Use importlib.metadata to get versions (works with --target installs)
from importlib.metadata import version as get_version
try:
    req_ver = get_version("requests")
except Exception:
    req_ver = getattr(requests, "__version__", "unknown")

try:
    u3_ver = get_version("urllib3")
except Exception:
    u3_ver = getattr(urllib3, "__version__", "unknown")

print(f"requests version: {req_ver}")
print(f"urllib3 version:  {u3_ver}")

# Verify requests can actually make a session (uses its own urllib3 internally)
s = requests.Session()
print(f"requests.Session created: {s}")

# Verify top-level urllib3 is the one we explicitly installed
print(f"urllib3 module path: {urllib3.__file__}")

print("Both imports succeeded — no conflict!")
