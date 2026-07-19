#!/bin/bash
#
# libtfs Backend Performance Benchmark
# =====================================
#
# Quick benchmark comparing ZIP vs DwarFS backend performance.
# Compatible with bash 3+ (macOS default)
#
# Usage: ./benchmarks/quick_benchmark.sh
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/../build"
RESULTS_DIR="${SCRIPT_DIR}/results"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
BOLD='\033[1m'
NC='\033[0m' # No Color

# Number of iterations
ITERATIONS=10
WARMUP=2

# ============================================================================
# Helper Functions
# ============================================================================

format_ms() {
    local ms=$1
    if (( $(echo "$ms < 1" | bc -l 2>/dev/null || echo 0) )); then
        printf "%.2f µs" "$(echo "$ms * 1000" | bc -l)"
    elif (( $(echo "$ms < 1000" | bc -l 2>/dev/null || echo 0) )); then
        printf "%.2f ms" "$ms"
    else
        printf "%.2f s" "$(echo "$ms / 1000" | bc -l)"
    fi
}

run_benchmark() {
    local binary=$1
    local filter=$2
    local iterations=$3

    local values=""
    local sum=0
    local count=0

    # Warmup runs
    for _ in $(seq 1 "$WARMUP"); do
        "$binary" --gtest_filter="$filter" > /dev/null 2>&1 || true
    done

    # Measured runs
    for _ in $(seq 1 "$iterations"); do
        start=$(python3 -c "import time; print(time.perf_counter() * 1000)")
        "$binary" --gtest_filter="$filter" > /dev/null 2>&1
        end=$(python3 -c "import time; print(time.perf_counter() * 1000)")

        duration=$(echo "$end - $start" | bc -l)
        if [ -z "$values" ]; then
            values="$duration"
        else
            values="$values $duration"
        fi
        sum=$(echo "$sum + $duration" | bc -l)
        ((count++))
    done

    # Calculate statistics
    local mean
    mean=$(echo "scale=2; $sum / $count" | bc -l)

    # Sort values for median
    local sorted
    sorted=$(echo "$values" | tr ' ' '\n' | sort -n)
    local -a sorted_arr=()
    while IFS= read -r line; do
        sorted_arr+=("$line")
    done <<< "$sorted"
    local median_idx=$((count / 2))
    local median=${sorted_arr[$median_idx]}

    # Calculate min/max
    local min=${sorted_arr[0]}
    local max=${sorted_arr[$((count - 1))]}

    echo "$mean:$median:$min:$max"
}

compare_results() {
    local zip_val=$1
    local dwarfs_val=$2

    local ratio
    ratio=$(echo "scale=2; $dwarfs_val / $zip_val" | bc -l)

    if (( $(echo "$ratio < 1" | bc -l) )); then
        local improvement
        improvement=$(echo "scale=1; (1 - $ratio) * 100" | bc -l)
        echo -e "${GREEN}🏆 DwarFS is ${improvement}% faster${NC}"
        echo "dwarfs"
    elif (( $(echo "$ratio > 1" | bc -l) )); then
        local improvement
        improvement=$(echo "scale=1; ($ratio - 1) * 100" | bc -l)
        echo -e "${GREEN}🏆 ZIP is ${improvement}% faster${NC}"
        echo "zip"
    else
        echo -e "${YELLOW}⚖️ It's a tie!${NC}"
        echo "tie"
    fi
}

# ============================================================================
# Main
# ============================================================================

echo -e "${BOLD}============================================================${NC}"
echo -e "${BOLD}         libtfs Backend Performance Benchmark${NC}"
echo -e "${BOLD}============================================================${NC}"
echo ""
echo "Build directory: $BUILD_DIR"
echo "Iterations: $ITERATIONS"
echo ""

# Check if binaries exist
ZIP_TEST="$BUILD_DIR/test_zip_backend"
DWARFS_TEST="$BUILD_DIR/test_dwarfs_backend"

if [[ ! -x "$ZIP_TEST" ]]; then
    echo -e "${RED}Error: ZIP test binary not found: $ZIP_TEST${NC}"
    echo "Please build the project first."
    exit 1
fi

if [[ ! -x "$DWARFS_TEST" ]]; then
    echo -e "${RED}Error: DwarFS test binary not found: $DWARFS_TEST${NC}"
    echo "Please build the project first."
    exit 1
fi

mkdir -p "$RESULTS_DIR"

echo -e "${BOLD}============================================================${NC}"
echo -e "${BOLD}                    RUNNING BENCHMARKS${NC}"
echo -e "${BOLD}============================================================${NC}"
echo ""

# Track wins
ZIP_WINS=0
DWARFS_WINS=0

# ============================================================================
# Benchmark 1: Mount Time
# ============================================================================

echo -e "${BLUE}Benchmark 1: Mount/Initialization Time${NC}"
echo "  Running ZIP mount test..."

result=$(run_benchmark "$ZIP_TEST" "*MountValid*" "$ITERATIONS")
zip_mount_mean=$(echo "$result" | cut -d: -f1)
zip_mount_median=$(echo "$result" | cut -d: -f2)

echo "  ZIP mount time: $(format_ms "$zip_mount_median") (mean: $(format_ms "$zip_mount_mean"))"

echo "  Running DwarFS mount test..."
result=$(run_benchmark "$DWARFS_TEST" "*MountValid*" "$ITERATIONS")
dwarfs_mount_mean=$(echo "$result" | cut -d: -f1)
dwarfs_mount_median=$(echo "$result" | cut -d: -f2)

echo "  DwarFS mount time: $(format_ms "$dwarfs_mount_median") (mean: $(format_ms "$dwarfs_mount_mean"))"

echo -n "  "
winner=$(compare_results "$zip_mount_median" "$dwarfs_mount_median" | tail -1)
if [ "$winner" = "dwarfs" ]; then
    ((DWARFS_WINS++))
elif [ "$winner" = "zip" ]; then
    ((ZIP_WINS++))
fi
echo ""

# ============================================================================
# Benchmark 2: Directory Listing
# ============================================================================

echo -e "${BLUE}Benchmark 2: Directory Listing${NC}"
echo "  Running ZIP directory listing test..."

result=$(run_benchmark "$ZIP_TEST" "*ListDirectory*" "$ITERATIONS")
zip_list_mean=$(echo "$result" | cut -d: -f1)
zip_list_median=$(echo "$result" | cut -d: -f2)

echo "  ZIP list time: $(format_ms "$zip_list_median") (mean: $(format_ms "$zip_list_mean"))"

echo "  Running DwarFS directory listing test..."
result=$(run_benchmark "$DWARFS_TEST" "*ListDirectory*" "$ITERATIONS")
dwarfs_list_mean=$(echo "$result" | cut -d: -f1)
dwarfs_list_median=$(echo "$result" | cut -d: -f2)

echo "  DwarFS list time: $(format_ms "$dwarfs_list_median") (mean: $(format_ms "$dwarfs_list_mean"))"

echo -n "  "
winner=$(compare_results "$zip_list_median" "$dwarfs_list_median" | tail -1)
if [ "$winner" = "dwarfs" ]; then
    ((DWARFS_WINS++))
elif [ "$winner" = "zip" ]; then
    ((ZIP_WINS++))
fi
echo ""

# ============================================================================
# Benchmark 3: File Read
# ============================================================================

echo -e "${BLUE}Benchmark 3: File Read${NC}"
echo "  Running ZIP file read test..."

result=$(run_benchmark "$ZIP_TEST" "*ReadFile*" "$ITERATIONS")
zip_read_mean=$(echo "$result" | cut -d: -f1)
zip_read_median=$(echo "$result" | cut -d: -f2)

echo "  ZIP read time: $(format_ms "$zip_read_median") (mean: $(format_ms "$zip_read_mean"))"

echo "  Running DwarFS file read test..."
result=$(run_benchmark "$DWARFS_TEST" "*ReadFile*" "$ITERATIONS")
dwarfs_read_mean=$(echo "$result" | cut -d: -f1)
dwarfs_read_median=$(echo "$result" | cut -d: -f2)

echo "  DwarFS read time: $(format_ms "$dwarfs_read_median") (mean: $(format_ms "$dwarfs_read_mean"))"

echo -n "  "
winner=$(compare_results "$zip_read_median" "$dwarfs_read_median" | tail -1)
if [ "$winner" = "dwarfs" ]; then
    ((DWARFS_WINS++))
elif [ "$winner" = "zip" ]; then
    ((ZIP_WINS++))
fi
echo ""

# ============================================================================
# Benchmark 4: Full Test Suite Time
# ============================================================================

echo -e "${BLUE}Benchmark 4: Full Test Suite (single run)${NC}"
echo "  Running ZIP full test suite..."

result=$(run_benchmark "$ZIP_TEST" "*" 1)
zip_full_median=$(echo "$result" | cut -d: -f2)

echo "  ZIP full suite: $(format_ms "$zip_full_median")"

echo "  Running DwarFS full test suite..."
result=$(run_benchmark "$DWARFS_TEST" "*" 1)
dwarfs_full_median=$(echo "$result" | cut -d: -f2)

echo "  DwarFS full suite: $(format_ms "$dwarfs_full_median")"

echo -n "  "
winner=$(compare_results "$zip_full_median" "$dwarfs_full_median" | tail -1)
if [ "$winner" = "dwarfs" ]; then
    ((DWARFS_WINS++))
elif [ "$winner" = "zip" ]; then
    ((ZIP_WINS++))
fi
echo ""

# ============================================================================
# Summary
# ============================================================================

echo -e "${BOLD}============================================================${NC}"
echo -e "${BOLD}                       SUMMARY${NC}"
echo -e "${BOLD}============================================================${NC}"
echo ""

# Determine mount winner
mount_winner="ZIP 🏆"
if (( $(echo "$dwarfs_mount_median < $zip_mount_median" | bc -l) )); then
    mount_winner="DwarFS 🏆"
fi

# Determine list winner
list_winner="ZIP 🏆"
if (( $(echo "$dwarfs_list_median < $zip_list_median" | bc -l) )); then
    list_winner="DwarFS 🏆"
fi

# Determine read winner
read_winner="ZIP 🏆"
if (( $(echo "$dwarfs_read_median < $zip_read_median" | bc -l) )); then
    read_winner="DwarFS 🏆"
fi

# Determine full suite winner
full_winner="ZIP 🏆"
if (( $(echo "$dwarfs_full_median < $zip_full_median" | bc -l) )); then
    full_winner="DwarFS 🏆"
fi

echo -e "| Benchmark          | ZIP (median)      | DwarFS (median)   | Winner        |"
echo -e "|--------------------|-------------------|-------------------|---------------|"
printf "| %-18s | %-17s | %-17s | %-13s |\n" "Mount Time" "$(format_ms "$zip_mount_median")" "$(format_ms "$dwarfs_mount_median")" "$mount_winner"
printf "| %-18s | %-17s | %-17s | %-13s |\n" "Directory Listing" "$(format_ms "$zip_list_median")" "$(format_ms "$dwarfs_list_median")" "$list_winner"
printf "| %-18s | %-17s | %-17s | %-13s |\n" "File Read" "$(format_ms "$zip_read_median")" "$(format_ms "$dwarfs_read_median")" "$read_winner"
printf "| %-18s | %-17s | %-17s | %-13s |\n" "Full Suite" "$(format_ms "$zip_full_median")" "$(format_ms "$dwarfs_full_median")" "$full_winner"
echo ""

echo -e "${BOLD}Overall Results:${NC}"
echo -e "  ZIP wins:    $ZIP_WINS"
echo -e "  DwarFS wins: $DWARFS_WINS"
echo ""

if (( DWARFS_WINS > ZIP_WINS )); then
    echo -e "${GREEN}${BOLD}🏆 DwarFS wins overall!${NC}"
elif (( ZIP_WINS > DWARFS_WINS )); then
    echo -e "${GREEN}${BOLD}🏆 ZIP wins overall!${NC}"
else
    echo -e "${YELLOW}${BOLD}⚖️ It's a tie!${NC}"
fi

echo ""
echo -e "${BOLD}============================================================${NC}"
echo -e "Results saved to: ${RESULTS_DIR}"
echo -e "${BOLD}============================================================${NC}"

# Save results to file
cat > "$RESULTS_DIR/benchmark_results.txt" << EOF
libtfs Backend Benchmark Results
================================
Date: $(date)
Iterations: $ITERATIONS

Results:
--------
Mount Time:
  ZIP: $(format_ms "$zip_mount_median")
  DwarFS: $(format_ms "$dwarfs_mount_median")
  Winner: $mount_winner

Directory Listing:
  ZIP: $(format_ms "$zip_list_median")
  DwarFS: $(format_ms "$dwarfs_list_median")
  Winner: $list_winner

File Read:
  ZIP: $(format_ms "$zip_read_median")
  DwarFS: $(format_ms "$dwarfs_read_median")
  Winner: $read_winner

Full Suite:
  ZIP: $(format_ms "$zip_full_median")
  DwarFS: $(format_ms "$dwarfs_full_median")
  Winner: $full_winner

Overall:
  ZIP wins: $ZIP_WINS
  DwarFS wins: $DWARFS_WINS
EOF

echo "Done!"
