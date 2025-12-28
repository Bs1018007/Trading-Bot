#!/bin/bash

# 1. Load variables from .env (ignoring comments)
export $(grep -v '^#' .env | xargs)

# 2. Check if keys loaded successfully
if [ -z "$BYBIT_API_KEY" ]; then
    echo "❌ ERROR: API Keys not loaded! Check your .env file."
    exit 1
fi

# 3. Run the bot
./build/trading_bot

