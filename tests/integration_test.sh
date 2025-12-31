#!/bin/bash
set -e

echo "=== Integration Test Suite ==="
cd /Users/mulgogi/src/tamatebako/libdwarfs/build

# Test 1: Basic mount/read/unmount
echo "Test 1: Basic Workflow"
./test_c_api --gtest_filter=CApiTest.Integration_FullWorkflow

# Test 2: Concurrent operations  
echo "Test 2: Concurrent Access"
./test_zip_backend --gtest_filter=ZipBackendTest.*Concurrent*

# Test 3: Memory mounting
echo "Test 3: Memory Mounting"
./test_c_api --gtest_filter=CApiTest.InitFromMemory_*

# Test 4: Multiple archives
echo "Test 4: Multi-Archive"
./test_zip_integration --gtest_filter=*MultipleZip*

# Test 5: Error handling
echo "Test 5: Error Scenarios"
./test_c_api --gtest_filter=*Invalid*

echo "=== All Integration Tests PASSED ==="
