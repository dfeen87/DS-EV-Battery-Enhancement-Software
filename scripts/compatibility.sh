#!/usr/bin/env bash
# ============================================================================
# DS EV Battery Enhancement - Compatibility Validation Utility
# ============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DIAG_SCRIPT="${SCRIPT_DIR}/diagnostics.py"

echo "===================================================================="
echo "DS EV VEHICLE & COMPATIBILITY VALIDATION"
echo "===================================================================="

if [ -f "$DIAG_SCRIPT" ]; then
    python3 "$DIAG_SCRIPT" "$@"
else
    echo "Error: diagnostics.py not found in ${SCRIPT_DIR}"
    exit 1
fi
