#!/bin/bash

set -euo pipefail
trap 'echo "error: ${BASH_SOURCE[0]} Line: $LINENO ,exit status: $?" >&2' ERR

LOG_FILE="out/build.log"
OUT_DIR="out"
TIMESTAMP=$(date +"%Y%m%d")
PACKAGE_NAME="gn-linux-x86-${TIMESTAMP}.tar.gz"

init_logging() {
    mkdir -p "${OUT_DIR}"
    
    echo "====================" > "${LOG_FILE}"
    echo " BUILD START: $(date)" >> "${LOG_FILE}"
    echo "====================" >> "${LOG_FILE}"
    echo "" >> "${LOG_FILE}"
    
    exec > >(tee -a "${LOG_FILE}")
    exec 2>&1
}

log_section() {
    local section=$1
    echo -e "\n\033[1;34m[$(date +"%T")] === $section ===\033[0m" | tee -a "${LOG_FILE}"
}

run_command() {
    echo "[$(date +"%T")] RUN: $*" | tee -a "${LOG_FILE}"
    echo "----------------------------------------" >> "${LOG_FILE}"
    
    start_time=$(date +%s)
    "$@" | tee -a "${LOG_FILE}"
    local status=$?
    end_time=$(date +%s)
    duration=$((end_time - start_time))
    
    echo "----------------------------------------" >> "${LOG_FILE}"
    echo "[$(date +"%T")] STATUS: $status (Cost: ${duration} seconds)" | tee -a "${LOG_FILE}"
    echo "" >> "${LOG_FILE}"
    
    return $status
}

main() { 
    init_logging
    log_section "Stage 1/5: Start Config"
    if ! run_command python3 build/gen.py; then
        echo -e "\033[1;31mError!\033[0m"
        return 1
    fi
    
    log_section "Stage 2/5: Start Ninja"
    if ! run_command ninja -C "${OUT_DIR}"; then
        echo -e "\033[1;31mError!\033[0m"
        return 1
    fi
    
    log_section "Stage 3/5: Start strip"
    if ! run_command strip "${OUT_DIR}/gn"; then
        echo -e "\033[1;31mError!\033[0m"
        return 1
    fi

    log_section "Stage 4/5: Start Unit Tests"
    if ! run_command "${OUT_DIR}/gn_unittests"; then
        echo -e "\033[1;31mError!\033[0m"
        return 1
    fi
    
    log_section "Stage 5/5: Start Package"
    if run_command tar -zcf "${PACKAGE_NAME}" "${OUT_DIR}/gn"; then
        echo -e "\n\033[1;32mSuccess! Package: ${PACKAGE_NAME}\033[0m"
        echo "Log Loaction: ${LOG_FILE}"
        echo "Packge Size: $(du -h ${PACKAGE_NAME} | cut -f1)"
	mv "${PACKAGE_NAME}" "${OUT_DIR}"
    else
        echo -e "\033[1;31m✗ Error ${OUT_DIR}\033[0m"
    fi
}

main "$@"
