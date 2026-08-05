#!/usr/bin/env bash
# ============================================================================
# DS EV Battery Enhancement - Safe Backup & Rollback Utility
# ============================================================================
set -euo pipefail

INSTALL_DIR="/opt/ds_enhancement"
BACKUP_DIR="${INSTALL_DIR}/backups"
LOG_FILE="${INSTALL_DIR}/logs/install.log"

log() {
    local msg="[$(date +'%Y-%m-%d %H:%M:%S')] $1"
    echo "$msg"
    if [ -w "$LOG_FILE" ]; then
        echo "$msg" >> "$LOG_FILE"
    elif [ -w "$(dirname "$LOG_FILE")" ]; then
        echo "$msg" >> "$LOG_FILE"
    fi
}

error_exit() {
    log "ERROR: $1" >&2
    exit 1
}

# Ensure script is run with root permissions
if [ "$EUID" -ne 0 ]; then
    error_exit "This script must be run with sudo / root privileges."
fi

command_mode="${1:-}"

if [ -z "$command_mode" ]; then
    echo "Usage: $0 [backup | restore | rollback]"
    exit 1
fi

case "$command_mode" in
    backup)
        log "Initiating system backup..."
        if [ ! -d "$INSTALL_DIR" ]; then
            log "No existing installation at $INSTALL_DIR to backup. Proceeding."
            exit 0
        fi

        TIMESTAMP=$(date +%Y%m%d_%H%M%S)
        TARGET_BACKUP="${BACKUP_DIR}/${TIMESTAMP}"

        mkdir -p "$TARGET_BACKUP"

        # Copy installed directories if they exist, excluding logs and backups
        for dir in bin config lib python python_fallback docs; do
            if [ -d "${INSTALL_DIR}/${dir}" ]; then
                log "Backing up ${dir}..."
                cp -rp "${INSTALL_DIR}/${dir}" "${TARGET_BACKUP}/"
            fi
        done

        log "✓ Backup successfully created at: ${TARGET_BACKUP}"
        ;;

    restore|rollback)
        log "Initiating rollback of previous version..."

        if [ ! -d "$BACKUP_DIR" ]; then
            error_exit "Backup directory ${BACKUP_DIR} does not exist. Cannot rollback."
        fi

        # Find the most recent backup directory (sorted alphabetically by name YYYYMMDD_HHMMSS)
        NEWEST_BACKUP=$(find "$BACKUP_DIR" -mindepth 1 -maxdepth 1 -type d | sort | tail -n 1)

        if [ -z "$NEWEST_BACKUP" ]; then
            error_exit "No previous backups found under ${BACKUP_DIR}."
        fi

        log "Found most recent backup: ${NEWEST_BACKUP}"

        # Perform rollback
        log "Restoring files from backup..."
        for dir in bin config lib python python_fallback docs; do
            if [ -d "${NEWEST_BACKUP}/${dir}" ]; then
                log "Restoring ${dir}..."
                # Remove current directory first to ensure clean restore
                rm -rf "${INSTALL_DIR}/${dir}"
                cp -rp "${NEWEST_BACKUP}/${dir}" "${INSTALL_DIR}/"
            fi
        done

        log "✓ Rollback completed successfully. Restored version from $(basename "$NEWEST_BACKUP")."
        ;;

    *)
        echo "Unknown command: $command_mode"
        echo "Usage: $0 [backup | restore | rollback]"
        exit 1
        ;;
esac
