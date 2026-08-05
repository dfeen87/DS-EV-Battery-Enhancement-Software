#!/usr/bin/env bash
# ============================================================================
# DS EV Battery Enhancement - Firmware Version Detection
# ============================================================================
set -euo pipefail

# This script detects the current vehicle firmware version from vehicle_status.json
# or overridden environment variables/parameters.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Resolve status json path
STATUS_FILE="/tmp/vehicle_status.json"
if [ ! -f "$STATUS_FILE" ]; then
    STATUS_FILE="${BASE_DIR}/config/vehicle_status.json"
fi

if [ ! -f "$STATUS_FILE" ] && [ -d "/opt/ds_enhancement/config" ]; then
    STATUS_FILE="/opt/ds_enhancement/config/vehicle_status.json"
fi

echo "===================================================================="
echo "DS EV FIRMWARE DETECTION"
echo "===================================================================="

if [ -n "${DS_FIRMWARE:-}" ]; then
    echo "Current Firmware: ${DS_FIRMWARE} (Overridden via DS_FIRMWARE env)"
elif [ -f "$STATUS_FILE" ]; then
    # Parse firmware_version using python standard json module to avoid jq dependency
    FW_VERSION=$(python3 -c "import json; print(json.load(open('$STATUS_FILE')).get('firmware_version', 'Unknown'))" 2>/dev/null || echo "Unknown")
    MODEL=$(python3 -c "import json; print(json.load(open('$STATUS_FILE')).get('model', 'Unknown'))" 2>/dev/null || echo "Unknown")
    echo "Vehicle Model:    ${MODEL}"
    echo "Current Firmware: ${FW_VERSION} (Read from vehicle_status.json)"
else
    echo "Error: Could not locate vehicle_status.json and no DS_FIRMWARE override set."
    exit 1
fi
