#!/bin/bash
# nRF Connect SDK Environment Setup Script
# Usage: source setup_ncs_env.sh

# nRF Connect SDK v3.1.1 environment variables
export NCS_ROOT="/opt/nordic/ncs/ncs-v3.2.1"
export ZEPHYR_BASE="$NCS_ROOT/zephyr"
export ZEPHYR_TOOLCHAIN_VARIANT="zephyr"
export ZEPHYR_SDK_INSTALL_DIR="/opt/nordic/ncs/toolchains/322ac893fe/opt/zephyr-sdk"

# Add west and toolchain to PATH
export PATH="$ZEPHYR_SDK_INSTALL_DIR/bin:/opt/nordic/ncs/toolchains/322ac893fe/bin:$PATH"

# Set CMAKE_PREFIX_PATH for Zephyr SDK
export CMAKE_PREFIX_PATH="$ZEPHYR_SDK_INSTALL_DIR:$CMAKE_PREFIX_PATH"

# Python virtual environment for nRF Connect SDK
if [ -f "$NCS_ROOT/toolchain/bin/python" ]; then
    export PYTHON_EXECUTABLE="$NCS_ROOT/toolchain/bin/python"
fi

# Set west workspace
export WEST_TOPDIR="$PWD"

echo "✅ nRF Connect SDK v3.2.1 environment loaded"
echo "📁 NCS_ROOT: $NCS_ROOT"
echo "🔧 ZEPHYR_SDK: $ZEPHYR_SDK_INSTALL_DIR"
echo "💻 West command: $(which west)"
echo ""
echo "Ready to use west commands in this terminal!"
