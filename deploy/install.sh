#!/bin/bash
#
# MikroClaw Secure Installer - Phase 1
# Deploys encrypted package to RouterOS
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "========================================"
echo "MikroClaw Secure Installer"
echo "========================================"
echo ""

# Parse arguments
DEPLOYMENT_NAME=""
ROUTER_IP=""
ROUTER_USER="admin"
ROUTER_PASS=""
DEPLOY_DIR=""
PASSPHRASE=""

usage() {
    echo "Usage: $0 --deployment NAME --router-ip IP [--router-user USER] [--deployment-dir DIR]"
    echo ""
    echo "Required:"
    echo "  --deployment NAME    Deployment name (e.g., router-home-01)"
    echo "  --router-ip IP       RouterOS IP address"
    echo ""
    echo "Optional:"
    echo "  --router-user USER   RouterOS username (default: admin)"
    echo "  --deployment-dir DIR Local deployment directory"
    echo "  --passphrase PHRASE  Decryption passphrase (will prompt if not provided)"
    echo ""
    exit 1
}

# Parse args
while [[ $# -gt 0 ]]; do
    case $1 in
        --deployment)
            DEPLOYMENT_NAME="$2"
            shift 2
            ;;
        --router-ip)
            ROUTER_IP="$2"
            shift 2
            ;;
        --router-user)
            ROUTER_USER="$2"
            shift 2
            ;;
        --deployment-dir)
            DEPLOY_DIR="$2"
            shift 2
            ;;
        --passphrase)
            PASSPHRASE="$2"
            shift 2
            ;;
        --help)
            usage
            ;;
        *)
            echo "Unknown option: $1"
            usage
            ;;
    esac
done

# Validate required args
if [[ -z "$DEPLOYMENT_NAME" ]] || [[ -z "$ROUTER_IP" ]]; then
    echo "❌ Error: --deployment and --router-ip are required"
    usage
fi

# Find deployment directory
if [[ -z "$DEPLOY_DIR" ]]; then
    # Look in build directory
    if [[ -d "$SCRIPT_DIR/build/$DEPLOYMENT_NAME" ]]; then
        DEPLOY_DIR="$SCRIPT_DIR/build/$DEPLOYMENT_NAME"
    elif [[ -d "$SCRIPT_DIR/../build/$DEPLOYMENT_NAME" ]]; then
        DEPLOY_DIR="$SCRIPT_DIR/../build/$DEPLOYMENT_NAME"
    else
        echo "❌ Error: Deployment directory not found"
        echo "   Searched: $SCRIPT_DIR/build/$DEPLOYMENT_NAME"
        echo "   Use --deployment-dir to specify location"
        exit 1
    fi
fi

if [[ ! -d "$DEPLOY_DIR" ]]; then
    echo "❌ Error: Deployment directory does not exist: $DEPLOY_DIR"
    exit 1
fi

echo "📦 Deployment: $DEPLOYMENT_NAME"
echo "🌐 Router: $ROUTER_USER@$ROUTER_IP"
echo "📁 Source: $DEPLOY_DIR"
echo ""

# Check required files
if [[ ! -f "$DEPLOY_DIR/mikroclaw" ]]; then
    echo "❌ Error: mikroclaw binary not found in $DEPLOY_DIR"
    exit 1
fi

if [[ ! -f "$DEPLOY_DIR/container.env.enc" ]]; then
    echo "❌ Error: container.env.enc not found in $DEPLOY_DIR"
    exit 1
fi

if [[ ! -f "$DEPLOY_DIR/install.rsc" ]]; then
    echo "❌ Error: install.rsc not found in $DEPLOY_DIR"
    exit 1
fi

# Get router password
if [[ -z "$ROUTER_PASS" ]]; then
    read -s -p "🔑 Router password: " ROUTER_PASS
    echo ""
fi

# Get deployment passphrase
if [[ -z "$PASSPHRASE" ]]; then
    read -s -p "🔐 Deployment passphrase: " PASSPHRASE
    echo ""
fi

echo ""
echo "🔍 Step 1: Verifying router connectivity..."
if ! ssh -o StrictHostKeyChecking=no -o ConnectTimeout=5 -o BatchMode=no "$ROUTER_USER@$ROUTER_IP" \
    ":put \"Connected\"" </dev/null 2>&1 | grep -q "Connected"; then
    echo "❌ Error: Cannot connect to router at $ROUTER_IP"
    echo "   Check: IP address, username, password, SSH access"
    exit 1
fi
echo "   ✓ Connected to router"
echo ""

# Get router serial
echo "🔍 Step 2: Fetching router information..."
ROUTER_SERIAL=$(ssh -o StrictHostKeyChecking=no "$ROUTER_USER@$ROUTER_IP" \
    '/system/routerboard/serial-number print' 2>/dev/null | grep -o '[A-Z0-9]\{8,\}' | head -1 || echo "")

if [[ -z "$ROUTER_SERIAL" ]]; then
    echo "⚠️  Warning: Could not fetch router serial number"
    echo "   Decryption may fail if package was bound to different serial"
    ROUTER_SERIAL="UNKNOWN"
else
    echo "   ✓ Router serial: $ROUTER_SERIAL"
fi

# Verify disk space
echo "🔍 Step 3: Checking disk space..."
DISK_FREE=$(ssh -o StrictHostKeyChecking=no "$ROUTER_USER@$ROUTER_IP" \
    '/disk/print where name=disk1' 2>/dev/null | grep -o '[0-9]*\.[0-9]*GiB' | head -1 || echo "")

if [[ -n "$DISK_FREE" ]]; then
    echo "   ✓ Disk space available: $DISK_FREE"
else
    echo "   ⚠️  Could not determine disk space"
fi
echo ""

# Create remote directory
echo "📂 Step 4: Creating remote directories..."
ssh -o StrictHostKeyChecking=no "$ROUTER_USER@$ROUTER_IP" \
    "/file/remove disk1/mikroclaw; /file/add name=disk1/mikroclaw type=directory" 2>/dev/null || true
echo "   ✓ Directories ready"
echo ""

# Upload files
echo "📤 Step 5: Uploading files..."
echo "   - mikroclaw binary ($(ls -lh $DEPLOY_DIR/mikroclaw | awk '{print $5}'))"
scp -o StrictHostKeyChecking=no "$DEPLOY_DIR/mikroclaw" "$ROUTER_USER@$ROUTER_IP:/disk1/mikroclaw/" 2>&1 | grep -v "100%" || true

echo "   - encrypted environment"
scp -o StrictHostKeyChecking=no "$DEPLOY_DIR/container.env.enc" "$ROUTER_USER@$ROUTER_IP:/disk1/mikroclaw/" 2>&1 | grep -v "100%" || true

echo "   - install script"
scp -o StrictHostKeyChecking=no "$DEPLOY_DIR/install.rsc" "$ROUTER_USER@$ROUTER_IP:/disk1/mikroclaw/" 2>&1 | grep -v "100%" || true

echo "   ✓ Files uploaded"
echo ""

# Decrypt and inject secrets
echo "🔓 Step 6: Decrypting and injecting secrets..."

# Create encryption key from passphrase + serial
ENCRYPTION_KEY=$(echo -n "${PASSPHRASE}${ROUTER_SERIAL}" | sha256sum | cut -d' ' -f1)

# Decrypt locally to temp
ENV_DECRYPTED=$(mktemp)
if ! openssl enc -aes-256-cbc -d -in "$DEPLOY_DIR/container.env.enc" -out "$ENV_DECRYPTED" -k "$ENCRYPTION_KEY" -pbkdf2 2>/dev/null; then
    echo "❌ Error: Decryption failed"
    echo "   Possible causes:"
    echo "   - Wrong passphrase"
    echo "   - Router serial mismatch (package bound to different device)"
    echo "   - Corrupted encryption"
    rm -f "$ENV_DECRYPTED"
    exit 1
fi

# Inject each secret as RouterOS container env
while IFS='=' read -r key value; do
    # Skip empty lines and comments
    [[ -z "$key" ]] && continue
    [[ "$key" =~ ^# ]] && continue
    
    # Remove existing env if present
    ssh -o StrictHostKeyChecking=no "$ROUTER_USER@$ROUTER_IP" \
        "/container/envs/remove [find name=mikroclaw-env and key=$key]" 2>/dev/null || true
    
    # Add new env
    ssh -o StrictHostKeyChecking=no "$ROUTER_USER@$ROUTER_IP" \
        "/container/envs/add name=mikroclaw-env key=$key value=\"$value\"" >/dev/null 2>&1
    
    echo "   ✓ Set: $key"
done < "$ENV_DECRYPTED"

# Securely remove decrypted file
shred -u "$ENV_DECRYPTED" 2>/dev/null || rm -f "$ENV_DECRYPTED"

echo "   ✓ Secrets injected"
echo ""

# Run install script
echo "🚀 Step 7: Running RouterOS install script..."
ssh -o StrictHostKeyChecking=no "$ROUTER_USER@$ROUTER_IP" \
    "/import disk1/mikroclaw/install.rsc" 2>&1 | tail -20

echo "   ✓ Install script executed"
echo ""

# Wait and verify
echo "⏳ Step 8: Waiting for container startup..."
sleep 5

echo ""
echo "🔍 Step 9: Verifying deployment..."
CONTAINER_STATUS=$(ssh -o StrictHostKeyChecking=no "$ROUTER_USER@$ROUTER_IP" \
    "/container/print where name=mikroclaw" 2>/dev/null | grep -o "running\|stopped\|invalid" || echo "unknown")

if [[ "$CONTAINER_STATUS" == "running" ]]; then
    echo "   ✓ Container is running"
    
    # Check logs
    echo ""
    echo "📋 Recent logs:"
    ssh -o StrictHostKeyChecking=no "$ROUTER_USER@$ROUTER_IP" \
        "/container/logs mikroclaw once" 2>/dev/null | tail -10 || echo "   (No logs available)"
    
    echo ""
    echo "========================================"
    echo "✅ Deployment Successful!"
    echo "========================================"
    echo ""
    echo "MikroClaw is now running on your RouterOS device."
    echo ""
    echo "Test your bot:"
    echo "   Send a message to your Telegram bot"
    echo ""
    echo "View logs:"
    echo "   ssh $ROUTER_USER@$ROUTER_IP '/container/logs mikroclaw follow'"
    echo ""
    echo "Stop/Start:"
    echo "   ssh $ROUTER_USER@$ROUTER_IP '/container/stop mikroclaw'"
    echo "   ssh $ROUTER_USER@$ROUTER_IP '/container/start mikroclaw'"
    echo ""
    
elif [[ "$CONTAINER_STATUS" == "stopped" ]]; then
    echo "   ⚠️  Container is stopped"
    echo ""
    echo "Check logs for errors:"
    ssh -o StrictHostKeyChecking=no "$ROUTER_USER@$ROUTER_IP" \
        "/container/logs mikroclaw once" 2>/dev/null | tail -20 || echo "   (No logs available)"
    echo ""
    echo "========================================"
    echo "⚠️  Deployment Incomplete"
    echo "========================================"
    
else
    echo "   ❌ Container status: $CONTAINER_STATUS"
    echo ""
    echo "========================================"
    echo "❌ Deployment Failed"
    echo "========================================"
    echo ""
    echo "Troubleshooting:"
    echo "1. Check container manually:"
    echo "   ssh $ROUTER_USER@$ROUTER_IP '/container/print'"
    echo ""
    echo "2. Check for errors:"
    echo "   ssh $ROUTER_USER@$ROUTER_IP '/log/print'"
    echo ""
    exit 1
fi

echo ""
echo "Rollback (if needed):"
echo "   ssh $ROUTER_USER@$ROUTER_IP '/container/stop mikroclaw; /container/remove mikroclaw'"
echo ""
