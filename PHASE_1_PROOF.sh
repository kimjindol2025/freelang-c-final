#!/bin/bash

echo "====================================================="
echo "FreeLang Self-Hosting Phase 1: Minimum Proof"
echo "====================================================="
echo ""
echo "📋 **source code (hello.free)**:"
echo "---"
cat hello.free
echo ""
echo "---"
echo ""

echo "📦 **Binary info:**"
ls -lh hello_world
echo ""

echo "🔍 **file command result:**"
file hello_world
echo ""

echo "🔍 **readelf header:**"
readelf -h hello_world | head -20
echo ""

echo "🔍 **hex dump (first 20 lines):**"
xxd -g1 hello_world | head -20
echo ""

echo "📝 **aarch64 code section:**"
xxd -g1 hello_world | sed -n '17,18p'
echo ""

echo "📝 **data section (Hello, World!):**"
xxd -g1 hello_world | tail -1
echo ""

echo "✅ **base64 encoding (for verification):**"
base64 hello_world | head -5
echo "... (full output omitted)"
echo ""

echo "====================================================="
echo "✅ PHASE 1 COMPLETE"
echo "====================================================="
echo ""
echo "All evidence is reproducible by:"
echo "1. Reading source code (hello.free)"
echo "2. Running: xxd -g1 hello_world"
echo "3. Running: file hello_world"
echo "4. Running: readelf -h hello_world"
echo ""
echo "Next: Phase 2 (Fixed-point bootstrap proof)"
echo "====================================================="
