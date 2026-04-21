"""
adiboupk runner — loads the import hook then executes the user script.

Usage: python adiboupk_run.py <script.py> [args...]

Environment variables:
  ADIBOUPK_PACKAGE_MAP  Path to package_map.json
"""
import os
import sys
import runpy

# Install the import hook before running the user script
import adiboupk_loader
adiboupk_loader.install()

if len(sys.argv) < 2:
    print("Usage: python adiboupk_run.py <script.py> [args...]", file=sys.stderr)
    sys.exit(1)

script_path = sys.argv[1]
sys.argv = sys.argv[1:]  # Remove adiboupk_run.py from argv

# Run the user script as __main__
script_abs = os.path.abspath(script_path)
script_dir = os.path.dirname(script_abs)

# Add script's directory to sys.path (standard Python behavior)
if script_dir not in sys.path:
    sys.path.insert(0, script_dir)

# Execute
runpy.run_path(script_abs, run_name="__main__")
