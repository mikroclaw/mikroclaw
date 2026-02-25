#!/bin/bash
#
# MikroClaw Secure Deployment Builder - Phase 1
# Creates encrypted deployment package for RouterOS
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
DEPLOY_DIR="$SCRIPT_DIR"
BUILD_DIR="$DEPLOY_DIR/build"

echo "========================================"
echo "MikroClaw Secure Deployment Builder"
echo "========================================"
echo ""

# Parse arguments
DEPLOYMENT_NAME=""
ROUTER_IP=""
ROUTER_SERIAL=""
ENV_FILE=""
OUTPUT_DIR="$BUILD_DIR"

usage() {
    echo "Usage: $0 --deployment NAME --router-ip IP [--router-serial SERIAL] [--env-file FILE] [--output DIR]"
    echo ""
    echo "Required:"
    echo "  --deployment NAME    Deployment name (e.g., router-home-01)"
    echo "  --router-ip IP       RouterOS IP address"
    echo ""
    echo "Optional:"
    echo "  --router-serial      Router serial number (for device binding)"
    echo "  --env-file FILE      Environment file with secrets"
    echo "  --output DIR         Output directory (default: ./build)"
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
        --router-serial)
            ROUTER_SERIAL="$2"
            shift 2
            ;;
        --env-file)
            ENV_FILE="$2"
            shift 2
            ;;
        --output)
            OUTPUT_DIR="$2"
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

# Create output directory
mkdir -p "$OUTPUT_DIR/$DEPLOYMENT_NAME"

echo "📦 Building deployment package..."
echo "   Deployment: $DEPLOYMENT_NAME"
echo "   Router IP: $ROUTER_IP"
echo "   Output: $OUTPUT_DIR/$DEPLOYMENT_NAME"
echo ""

# Step 1: Build MikroClaw binary
echo "🔨 Step 1: Building MikroClaw binary..."
cd "$PROJECT_DIR"
make clean >/dev/null 2>&1 || true
make CHANNELS=telegram,gateway >/dev/null 2>&1

if [[ ! -f "$PROJECT_DIR/mikroclaw" ]]; then
    echo "❌ Build failed"
    exit 1
fi

cp "$PROJECT_DIR/mikroclaw" "$OUTPUT_DIR/$DEPLOYMENT_NAME/"
echo "   ✓ Binary built: $(ls -lh $OUTPUT_DIR/$DEPLOYMENT_NAME/mikroclaw | awk '{print $5}')"
echo ""

# Step 2: Generate checksum
echo "🔐 Step 2: Generating checksums..."
cd "$OUTPUT_DIR/$DEPLOYMENT_NAME"
sha256sum mikroclaw > verify.sha256
echo "   ✓ Checksum: $(cat verify.sha256 | cut -d' ' -f1 | head -c 16)..."
echo ""

# Step 3: Collect or load secrets
echo "🔑 Step 3: Configuring secrets..."
ENV_TEMP=$(mktemp)

if [[ -n "$ENV_FILE" ]] && [[ -f "$ENV_FILE" ]]; then
    echo "   Loading secrets from: $ENV_FILE"
    cp "$ENV_FILE" "$ENV_TEMP"
else
    echo "   Interactive mode - enter secrets:"
    echo ""
    
    read -p "   Telegram Bot Token: " BOT_TOKEN
    read -p "   LLM API Key: " LLM_API_KEY
    read -p "   LLM Provider (openrouter/openai/localai): [openrouter] " LLM_PROVIDER
    LLM_PROVIDER=${LLM_PROVIDER:-openrouter}
    read -p "   LLM Model: [google/gemini-flash] " LLM_MODEL
    LLM_MODEL=${LLM_MODEL:-google/gemini-flash}
    read -p "   Router API User: [mikroclaw] " ROUTER_USER
    ROUTER_USER=${ROUTER_USER:-mikroclaw}
    read -s -p "   Router API Password: " ROUTER_PASS
    echo ""
    read -p "   Router Host (container gateway): [172.17.0.1] " ROUTER_HOST
    ROUTER_HOST=${ROUTER_HOST:-172.17.0.1}
    
    # Optional MemU
    read -p "   MemU API Key (optional, press Enter to skip): " MEMU_API_KEY
    
    # Write to temp file
    cat > "$ENV_TEMP" << EOF
BOT_TOKEN=$BOT_TOKEN
LLM_API_KEY=$LLM_API_KEY
LLM_PROVIDER=$LLM_PROVIDER
LLM_MODEL=$LLM_MODEL
ROUTER_USER=$ROUTER_USER
ROUTER_PASS=$ROUTER_PASS
ROUTER_HOST=$ROUTER_HOST
MEMU_API_KEY=$MEMU_API_KEY
EOF
fi

echo "   ✓ Secrets configured"
echo ""

# Step 4: Encrypt secrets
echo "🔒 Step 4: Encrypting secrets..."

# Generate deployment passphrase if not provided
if [[ -z "$DEPLOY_PASSPHRASE" ]]; then
    DEPLOY_PASSPHRASE=$(openssl rand -base64 32)
    echo "   Generated deployment passphrase (SAVE THIS):"
    echo "   $DEPLOY_PASSPHRASE"
    echo ""
fi

# Get router serial if not provided
if [[ -z "$ROUTER_SERIAL" ]]; then
    echo "   Fetching router serial number from $ROUTER_IP..."
    read -p "   Router admin username: " ROUTER_ADMIN
    read -s -p "   Router admin password: " ROUTER_ADMIN_PASS
    echo ""
    
    ROUTER_SERIAL=$(ssh -o StrictHostKeyChecking=no -o ConnectTimeout=5 "$ROUTER_ADMIN@$ROUTER_IP" \
        '/system/routerboard/serial-number print' 2>/dev/null | grep -o '[A-Z0-9]\{8,\}' | head -1 || echo "UNKNOWN")
    
    if [[ "$ROUTER_SERIAL" == "UNKNOWN" ]]; then
        echo "   ⚠️  Could not fetch serial, using timestamp binding"
        ROUTER_SERIAL=$(date +%s)
    else
        echo "   ✓ Router serial: $ROUTER_SERIAL"
    fi
fi

# Create encryption key from passphrase + serial
ENCRYPTION_KEY=$(echo -n "${DEPLOY_PASSPHRASE}${ROUTER_SERIAL}" | sha256sum | cut -d' ' -f1)

# Encrypt the env file
openssl enc -aes-256-cbc -salt -in "$ENV_TEMP" -out "$OUTPUT_DIR/$DEPLOYMENT_NAME/container.env.enc" -k "$ENCRYPTION_KEY" -pbkdf2 2>/dev/null

rm -f "$ENV_TEMP"

echo "   ✓ Secrets encrypted"
echo ""

# Step 5: Generate RouterOS install script
echo "📝 Step 5: Generating RouterOS install script..."

cat > "$OUTPUT_DIR/$DEPLOYMENT_NAME/install.rsc" <> 'EOF'
# MikroClaw Secure Installation Script
# Deployment: DEPLOYMENT_PLACEHOLDER
# Generated: DATE_PLACEHOLDER

# Configuration
:local deploymentName "DEPLOYMENT_PLACEHOLDER"
:local containerName "mikroclaw"
:local containerDir "disk1/mikroclaw"
:local ramMax "288M"

# Create container directory
/container/config/set ram-high=256M ram-max=288M

# Remove existing container if present
/container/remove [find name=\$containerName]

# Create environment variables (decrypted at runtime)
/container/envs/add name="mikroclaw-env" \
  key="DEPLOYMENT" value="DEPLOYMENT_PLACEHOLDER"

# Import container
/container/add \
  name=\$containerName \
  file=\$containerDir/mikroclaw \
  envlist=mikroclaw-env \
  interface=veth1 \
  start-on-boot=yes \
  logging=yes

# Start container
/container/start \$containerName

# Verify
:delay 3
:put "Container status:"
/container/print where name=\$containerName
EOF

# Substitute placeholders
sed -i "s/DEPLOYMENT_PLACEHOLDER/$DEPLOYMENT_NAME/g" "$OUTPUT_DIR/$DEPLOYMENT_NAME/install.rsc"
sed -i "s/DATE_PLACEHOLDER/$(date -Iseconds)/g" "$OUTPUT_DIR/$DEPLOYMENT_NAME/install.rsc"

echo "   ✓ Install script created"
echo ""

# Step 6: Create deployment README
echo "📄 Step 6: Creating deployment documentation..."

cat > "$OUTPUT_DIR/$DEPLOYMENT_NAME/README.deploy" << EOF
MikroClaw Deployment Package
=============================
Deployment: $DEPLOYMENT_NAME
Router IP: $ROUTER_IP
Router Serial: $ROUTER_SERIAL
Created: $(date -Iseconds)

Files:
  - mikroclaw: Binary executable
  - container.env.enc: Encrypted environment variables
  - install.rsc: RouterOS installation script
  - verify.sha256: Checksum for verification

Deployment Passphrase (SAVE THIS SECURELY):
$DEPLOY_PASSPHRASE

Installation Instructions:
1. Copy files to RouterOS:
   scp -r $OUTPUT_DIR/$DEPLOYMENT_NAME/* admin@$ROUTER_IP:/disk1/

2. Run install script:
   ssh admin@$ROUTER_IP '/import install.rsc'

3. Decrypt and load secrets (run on router):
   /container/envs/add name=mikroclaw-env key=BOT_TOKEN value="..."
   (or use keyman tool for automated injection)

4. Verify:
   /container/print where name=mikroclaw

Security Notes:
- This package is bound to router serial: $ROUTER_SERIAL
- Secrets are encrypted with AES-256-CBC
- Decryption requires the deployment passphrase
- Store passphrase securely (password manager)

Rollback:
/container/stop mikroclaw
/container/remove mikroclaw
/container/envs/remove [find name~"mikroclaw"]
EOF

echo "   ✓ Documentation created"
echo ""

# Step 7: Create tarball
echo "📦 Step 7: Creating deployment tarball..."
cd "$OUTPUT_DIR"
tar czf "$DEPLOYMENT_NAME.tar.gz" "$DEPLOYMENT_NAME/"

echo "   ✓ Package created: $OUTPUT_DIR/$DEPLOYMENT_NAME.tar.gz"
echo ""

# Summary
echo "========================================"
echo "✅ Build Complete!"
echo "========================================"
echo ""
echo "Deployment Package: $OUTPUT_DIR/$DEPLOYMENT_NAME.tar.gz"
echo ""
echo "Next Steps:"
echo "1. Securely store the deployment passphrase"
echo "2. Copy package to router:"
echo "   scp $OUTPUT_DIR/$DEPLOYMENT_NAME.tar.gz admin@$ROUTER_IP:/disk1/"
echo ""
echo "3. Extract and install:"
echo "   ssh admin@$ROUTER_IP 'cd /disk1 && tar xzf $DEPLOYMENT_NAME.tar.gz && /import $DEPLOYMENT_NAME/install.rsc'"
echo ""
echo "⚠️  IMPORTANT: Save this passphrase securely!"
echo "   $DEPLOY_PASSPHRASE"
echo ""
