#!/usr/bin/env bash
# ============================================================================
# DS EV Battery Enhancement - Battery Health Check Utility
# ============================================================================
set -euo pipefail

# Locate diagnostics script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DIAG_SCRIPT="${SCRIPT_DIR}/diagnostics.py"

echo "===================================================================="
echo "DS EV BATTERY HEALTH CHECK"
echo "===================================================================="

# Run diagnostics with focus on SOH and battery metrics
if [ -f "$DIAG_SCRIPT" ]; then
    python3 "$DIAG_SCRIPT" "$@"
else
    echo "Error: diagnostics.py not found in ${SCRIPT_DIR}"
    exit 1
fi
