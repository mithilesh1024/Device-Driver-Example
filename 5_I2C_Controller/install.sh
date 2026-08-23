#!/usr/bin/env bash

set -e # Exit immediately if a command exits with a non-zero status

OLD_MODULE="hello_driver"
NEW_MODULE="at24c256_driver"
KO_FILE="${NEW_MODULE}.ko"

echo "=========================================="
echo "    AT24C256 Driver Build & Install       "
echo "=========================================="

# 1. Unload old driver if currently loaded
if lsmod | grep -q "$OLD_MODULE"; then
    echo "[+] Unloading legacy module: $OLD_MODULE..."
    sudo rmmod "$OLD_MODULE" || true
fi

# 2. Unload current driver if already loaded
if lsmod | grep -q "$NEW_MODULE"; then
    echo "[+] Unloading existing module: $NEW_MODULE..."
    sudo rmmod "$NEW_MODULE"
fi

# 3. Clean and build module
echo "[+] Cleaning and building driver..."
make clean
make

# 4. Verify .ko binary exists before trying to load
if [ ! -f "$KO_FILE" ]; then
    echo "[-] Error: $KO_FILE not found after build!"
    exit 1
fi

# 5. Load the new module
echo "[+] Loading module: $KO_FILE..."
sudo insmod "$KO_FILE"

# 6. Verify kernel module loaded successfully
if lsmod | grep -q "$NEW_MODULE"; then
    echo "[+] Success: $NEW_MODULE loaded."
else
    echo "[-] Error: Failed to load $NEW_MODULE"
    exit 1
fi

# 7. Check if device node got instantiated in /dev
echo "[+] Checking generated device node:"
ls -l /dev/my_i2c-* 2>/dev/null || echo "[-] Warning: No /dev/my_i2c-* node found. Make sure DT overlay or i2c_new_client_device is set up!"

# 8. Show recent kernel logs
echo ""
echo "================ Kernel Logs ==============="
sudo dmesg | tail -n 15