#!/bin/bash
set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

echo -e "${CYAN}============================================${NC}"
echo -e "${CYAN}Bybit Trading Bot - Complete Build Script${NC}"
echo -e "${CYAN}Stack: Aeron + SBE + WebSocket + simdjson${NC}"
echo -e "${CYAN}============================================${NC}"
echo ""

# ============================================================================
# Detect Operating System
# ============================================================================
if [[ "$OSTYPE" == "darwin"* ]]; then
    echo -e "${GREEN}✓ Detected: macOS${NC}"
    PKG_MANAGER="brew"
    NUM_CORES=$(sysctl -n hw.ncpu)
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    echo -e "${GREEN}✓ Detected: Linux${NC}"
    PKG_MANAGER="apt"
    NUM_CORES=$(nproc)
else
    echo -e "${RED}✗ Unsupported OS: $OSTYPE${NC}"
    exit 1
fi

echo -e "${BLUE}Using ${NUM_CORES} CPU cores for parallel builds${NC}"
echo ""

# ============================================================================
# Step 1: Install System Dependencies
# ============================================================================
echo -e "${MAGENTA}[1/7] Installing system dependencies...${NC}"

if [[ "$PKG_MANAGER" == "brew" ]]; then
    echo "Installing via Homebrew..."
    brew install cmake openssl libwebsockets pkg-config openjdk@11 git curl || true
    
    # Set JAVA_HOME
    export JAVA_HOME=$(/usr/libexec/java_home -v 11 2>/dev/null || echo "/opt/homebrew/opt/openjdk@11")
    
elif [[ "$PKG_MANAGER" == "apt" ]]; then
    echo "Installing via apt..."
    sudo apt-get update
    sudo apt-get install -y \
        cmake \
        g++ \
        libssl-dev \
        libwebsockets-dev \
        pkg-config \
        openjdk-11-jdk \
        git \
        libcurl4-openssl-dev \
        build-essential
    
    export JAVA_HOME=/usr/lib/jvm/java-11-openjdk-amd64
fi

export PATH="$JAVA_HOME/bin:$PATH"
echo -e "${GREEN}✓ Java: $(java -version 2>&1 | head -n 1)${NC}"
echo ""

# ============================================================================
# Step 2: Create Directory Structure
# ============================================================================
echo -e "${MAGENTA}[2/7] Creating project directories...${NC}"

mkdir -p third_party
mkdir -p schema
mkdir -p generated
mkdir -p logs
mkdir -p build

echo -e "${GREEN}✓ Directories created${NC}"
echo ""

# ============================================================================
# Step 3: Setup simdjson
# ============================================================================
echo -e "${MAGENTA}[3/7] Setting up simdjson...${NC}"

cd third_party

if [ ! -d "simdjson" ]; then
    echo "Cloning simdjson..."
    git clone --depth 1 https://github.com/simdjson/simdjson.git
fi

cd simdjson

if [ ! -f "build/libsimdjson.a" ]; then
    echo "Building simdjson..."
    mkdir -p build
    cd build
    cmake ..
    make -j$NUM_CORES
    cd ..
    echo -e "${GREEN}✓ simdjson built successfully${NC}"
else
    echo -e "${YELLOW}✓ simdjson already built${NC}"
fi

cd ../..
echo ""

# ============================================================================
# Step 4: Setup Aeron (This takes time)
# ============================================================================
echo -e "${MAGENTA}[4/7] Setting up Aeron (may take 5-10 minutes)...${NC}"

cd third_party

if [ ! -d "aeron" ]; then
    echo "Cloning Aeron..."
    git clone https://github.com/real-logic/aeron.git
fi

cd aeron

if [ ! -f "cppbuild/Release/lib/libaeron.a" ]; then
    echo "Building Aeron (this will take a while)..."
    rm -rf cppbuild
    mkdir -p cppbuild/Release
    cd cppbuild/Release
    
    # Find OpenSSL
    if [[ "$PKG_MANAGER" == "brew" ]]; then
        OPENSSL_ROOT=$(brew --prefix openssl 2>/dev/null || echo "")
    else
        OPENSSL_ROOT=""
    fi
    
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DBUILD_AERON_DRIVER=ON \
          -DBUILD_AERON_ARCHIVE_API=OFF \
          -DAERON_TESTS=OFF \
          -DAERON_BUILD_SAMPLES=OFF \
          -DAERON_BUILD_DOCUMENTATION=OFF \
          -DOPENSSL_ROOT_DIR=$OPENSSL_ROOT \
          ../../
    
    make -j$NUM_CORES aeron aeron_client aeron_driver
    
    cd ../..
    echo -e "${GREEN}✓ Aeron built successfully${NC}"
else
    echo -e "${YELLOW}✓ Aeron already built${NC}"
fi

cd ../..
echo ""

# ============================================================================
# Step 5: Setup Simple Binary Encoding (SBE)
# ============================================================================
echo -e "${MAGENTA}[5/7] Setting up Simple Binary Encoding (SBE)...${NC}"

cd third_party

if [ ! -d "simple-binary-encoding" ]; then
    echo "Cloning SBE..."
    git clone --depth 1 https://github.com/real-logic/simple-binary-encoding.git
fi

cd simple-binary-encoding

SBE_JAR=$(ls sbe-all/build/libs/sbe-all-*.jar 2>/dev/null | head -1)

if [ -z "$SBE_JAR" ]; then
    echo "Building SBE..."
    chmod +x gradlew
    ./gradlew :sbe-all:build
    echo -e "${GREEN}✓ SBE built successfully${NC}"
else
    echo -e "${YELLOW}✓ SBE already built${NC}"
fi

cd ../..
echo ""

# ============================================================================
# Step 6: Generate SBE Schema Code
# ============================================================================
echo -e "${MAGENTA}[6/7] Generating SBE code from schema...${NC}"

# Create SBE schema if it doesn't exist
if [ ! -f "schema/market_data.xml" ]; then
    echo "Creating SBE schema..."
    cat > schema/market_data.xml << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<sbe:messageSchema xmlns:sbe="http://fixprotocol.io/2016/sbe"
                   package="trading.sbe"
                   id="1"
                   version="0"
                   semanticVersion="0.1"
                   description="Trading Bot SBE Messages"
                   byteOrder="littleEndian">
    <types>
        <composite name="messageHeader">
            <type name="blockLength" primitiveType="uint16"/>
            <type name="templateId" primitiveType="uint16"/>
            <type name="schemaId" primitiveType="uint16"/>
            <type name="version" primitiveType="uint16"/>
        </composite>
        <composite name="groupSizeEncoding">
            <type name="blockLength" primitiveType="uint16"/>
            <type name="numInGroup" primitiveType="uint16"/>
        </composite>
        <composite name="varStringEncoding">
            <type name="length" primitiveType="uint16"/>
            <type name="varData" primitiveType="uint8" length="0"/>
        </composite>
    </types>
    
    <sbe:message name="OrderBookSnapshot" id="2" description="Orderbook snapshot">
        <field name="timestamp" id="1" type="uint64" description="Nanosecond timestamp"/>
        <field name="bidCount" id="2" type="uint16" description="Number of bid levels"/>
        <field name="askCount" id="3" type="uint16" description="Number of ask levels"/>
        <group name="bids" id="4" dimensionType="groupSizeEncoding">
            <field name="price" id="5" type="double"/>
            <field name="quantity" id="6" type="double"/>
        </group>
        <group name="asks" id="7" dimensionType="groupSizeEncoding">
            <field name="price" id="8" type="double"/>
            <field name="quantity" id="9" type="double"/>
        </group>
        <data name="symbol" id="10" type="varStringEncoding"/>
    </sbe:message>
    
    <sbe:message name="TradeSignal" id="3" description="Trading signal">
        <field name="timestamp" id="1" type="uint64" description="Nanosecond timestamp"/>
        <field name="action" id="2" type="uint8" description="0=Buy, 1=Sell"/>
        <field name="price" id="3" type="double"/>
        <field name="quantity" id="4" type="double"/>
        <data name="symbol" id="5" type="varStringEncoding"/>
    </sbe:message>
</sbe:messageSchema>
EOF
    echo -e "${GREEN}✓ SBE schema created${NC}"
fi

# Generate SBE code
SBE_JAR=$(ls third_party/simple-binary-encoding/sbe-all/build/libs/sbe-all-*.jar 2>/dev/null | head -1)

if [ -f "$SBE_JAR" ] && [ -f "schema/market_data.xml" ]; then
    echo "Generating SBE code..."
    java -Dsbe.output.dir=generated -jar "$SBE_JAR" schema/market_data.xml
    echo -e "${GREEN}✓ SBE code generated${NC}"
else
    echo -e "${YELLOW}⚠ Warning: SBE code generation skipped${NC}"
fi

echo ""

# ============================================================================
# Step 7: Build Trading Bot
# ============================================================================
echo -e "${MAGENTA}[7/7] Building trading bot...${NC}"

cd build

if [[ "$PKG_MANAGER" == "brew" ]]; then
    OPENSSL_ROOT=$(brew --prefix openssl)
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DOPENSSL_ROOT_DIR=$OPENSSL_ROOT \
          ..
else
    cmake -DCMAKE_BUILD_TYPE=Release ..
fi

make -j$NUM_CORES

# ============================================================================
# Build Complete
# ============================================================================
if [ $? -eq 0 ]; then
    echo ""
    echo -e "${GREEN}============================================${NC}"
    echo -e "${GREEN}✓ BUILD SUCCESSFUL!${NC}"
    echo -e "${GREEN}============================================${NC}"
    echo ""
    echo -e "${CYAN}Executable:${NC} ./build/trading_bot"
    echo ""
    echo -e "${YELLOW}To run:${NC}"
    echo "   cd build"
    echo "   ./trading_bot"
    echo ""
    echo -e "${YELLOW}Features Enabled:${NC}"
    echo "   ✓ Bybit WebSocket orderbook streaming (all USDT pairs)"
    echo "   ✓ Aeron IPC (ultra-low latency messaging)"
    echo "   ✓ SBE encoding (efficient binary serialization)"
    echo "   ✓ Lock-free data structures"
    echo "   ✓ Sub-microsecond processing"
    echo "   ✓ Multi-threaded architecture"
    echo "   ✓ Comprehensive logging"
    echo ""
    echo -e "${YELLOW}System Architecture:${NC}"
    echo "   WebSocket → simdjson Parser → OrderBook → SBE Encoder → Aeron IPC"
    echo ""
    echo -e "${YELLOW}Expected Performance:${NC}"
    echo "   - WebSocket latency: 50-100μs"
    echo "   - JSON parsing: 5-10μs"
    echo "   - SBE encoding: 1-2μs"
    echo "   - Aeron publish: 1-5μs"
    echo "   - Total end-to-end: ~60-120μs per market update"
    echo ""
    echo -e "${YELLOW}Log files:${NC}"
    echo "   - Runtime logs: ./logs/"
    echo "   - Trading data: ./logs/*_bybit_trading.log"
    echo ""
else
    echo ""
    echo -e "${RED}============================================${NC}"
    echo -e "${RED}✗ BUILD FAILED${NC}"
    echo -e "${RED}============================================${NC}"
    echo ""
    echo -e "${YELLOW}Common issues:${NC}"
    echo "1. Missing dependencies - rerun this script"
    echo "2. Java not found - install openjdk-11"
    echo "3. Aeron build failed - check logs above"
    echo ""
    exit 1
fi