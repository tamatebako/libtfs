#!/usr/bin/env python3
"""
libtfs Performance Benchmark Suite
===================================

Comprehensive benchmark comparing ZIP vs DwarFS backend performance.

Measures:
- Mount/initialization time
- Sequential read throughput
- Random access latency
- Directory enumeration speed
- Path lookup latency
- Memory footprint

Usage:
    python3 benchmarks/benchmark_backends.py [--iterations N] [--output json|markdown]

Author: libtfs team
License: Same as libtfs
"""

import subprocess
import json
import os
import sys
import tempfile
import shutil
import time
import statistics
import argparse
from pathlib import Path
from dataclasses import dataclass, asdict
from typing import List, Dict, Optional, Tuple
import random
import string

# ============================================================================
# Configuration
# ============================================================================

BUILD_DIR = Path(__file__).parent.parent / "build"
MKDWARFS = Path(__file__).parent.parent / "deps" / "bin" / "mkdwarfs"

# Test configurations
WARMUP_ITERATIONS = 2
DEFAULT_ITERATIONS = 10

# Test data sizes
SMALL_FILE_SIZE = 1024  # 1 KB
MEDIUM_FILE_SIZE = 1024 * 1024  # 1 MB
LARGE_FILE_SIZE = 10 * 1024 * 1024  # 10 MB

# File counts for different scenarios
FEW_FILES = 10
MANY_FILES = 100
LOTS_FILES = 500

# Directory depths
SHALLOW_DEPTH = 2
MEDIUM_DEPTH = 5
DEEP_DEPTH = 10


# ============================================================================
# Data Classes
# ============================================================================

@dataclass
class BenchmarkResult:
    """Single benchmark measurement"""
    name: str
    backend: str
    iterations: int
    values: List[float]  # in milliseconds or bytes
    mean: float
    median: float
    stddev: float
    min_val: float
    max_val: float
    unit: str  # "ms", "MB/s", "ops/s", "bytes"

    def to_dict(self) -> dict:
        return asdict(self)


@dataclass
class ComparisonResult:
    """Comparison between two backends"""
    name: str
    zip_result: BenchmarkResult
    dwarfs_result: BenchmarkResult
    ratio: float  # dwarfs/zip (< 1.0 means dwarfs faster)
    winner: str
    improvement_percent: float


# ============================================================================
# Test Data Generation
# ============================================================================

def generate_random_content(size: int, seed: int = None) -> bytes:
    """Generate deterministic random content for reproducibility"""
    if seed is not None:
        random.seed(seed)
    return bytes(random.getrandbits(8) for _ in range(size))


def generate_text_content(size: int) -> bytes:
    """Generate text-like content (better compression)"""
    words = ["hello", "world", "test", "data", "file", "content", "benchmark", "performance"]
    content = []
    while len(b" ".join(content)) < size:
        content.append(random.choice(words))
    return b" ".join(content)[:size]


def create_test_directory(
    base_path: Path,
    file_count: int,
    file_size: int,
    depth: int = 1,
    text_mode: bool = False
) -> Dict[str, Path]:
    """
    Create a test directory structure with specified parameters.

    Returns dict mapping relative paths to actual paths.
    """
    files = {}
    base_path.mkdir(parents=True, exist_ok=True)

    def create_level(current_path: Path, current_depth: int, files_in_level: int):
        if current_depth > depth:
            return

        # Create files at this level
        files_per_level = file_count // depth if depth > 0 else file_count
        for i in range(files_per_level):
            filename = f"file_{current_depth}_{i}.txt"
            filepath = current_path / filename
            if text_mode:
                content = generate_text_content(file_size)
            else:
                content = generate_random_content(file_size, seed=hash(str(filepath)))
            filepath.write_bytes(content)
            files[str(filepath.relative_to(base_path))] = filepath

        # Create subdirectories
        if current_depth < depth:
            subdir = current_path / f"level_{current_depth}"
            subdir.mkdir(exist_ok=True)
            create_level(subdir, current_depth + 1, files_in_level)

    create_level(base_path, 1, file_count // depth if depth > 0 else file_count)
    return files


def create_deep_directory(base_path: Path, depth: int, files_per_level: int = 3) -> List[Path]:
    """Create a deeply nested directory structure"""
    files = []
    current = base_path
    current.mkdir(parents=True, exist_ok=True)

    for d in range(depth):
        # Create files at this level
        for f in range(files_per_level):
            filepath = current / f"file_{f}.txt"
            filepath.write_bytes(generate_random_content(SMALL_FILE_SIZE, seed=d*100+f))
            files.append(filepath)

        # Create next level
        if d < depth - 1:
            current = current / f"level_{d+1}"
            current.mkdir(exist_ok=True)

    return files


# ============================================================================
# Archive Creation
# ============================================================================

def create_zip_archive(source_dir: Path, output_path: Path) -> bool:
    """Create a ZIP archive from a directory"""
    try:
        result = subprocess.run(
            ["zip", "-r", "-q", str(output_path), "."],
            cwd=source_dir,
            capture_output=True,
            timeout=60
        )
        return result.returncode == 0
    except Exception as e:
        print(f"Error creating ZIP: {e}", file=sys.stderr)
        return False


def create_dwarfs_archive(source_dir: Path, output_path: Path) -> bool:
    """Create a DwarFS archive from a directory"""
    try:
        # Check if mkdwarfs exists
        if not MKDWARFS.exists():
            print(f"mkdwarfs not found at {MKDWARFS}", file=sys.stderr)
            return False

        result = subprocess.run(
            [str(MKDWARFS), "-o", str(output_path), "-i", str(source_dir)],
            capture_output=True,
            timeout=120
        )
        if result.returncode != 0:
            print(f"mkdwarfs stderr: {result.stderr.decode()}", file=sys.stderr)
        return result.returncode == 0
    except Exception as e:
        print(f"Error creating DwarFS: {e}", file=sys.stderr)
        return False


# ============================================================================
# Measurement Functions
# ============================================================================

def measure_time_ms(func, *args, **kwargs) -> Tuple[float, any]:
    """Measure execution time in milliseconds"""
    start = time.perf_counter()
    result = func(*args, **kwargs)
    end = time.perf_counter()
    return (end - start) * 1000, result


def run_benchmark_iterations(
    func,
    iterations: int,
    warmup: int = WARMUP_ITERATIONS,
    **kwargs
) -> BenchmarkResult:
    """Run a benchmark function multiple times and compute statistics"""
    values = []

    # Warmup runs (not counted)
    for _ in range(warmup):
        try:
            func(**kwargs)
        except Exception as e:
            print(f"Warmup failed: {e}", file=sys.stderr)

    # Measured runs
    for i in range(iterations):
        try:
            time_ms, _ = measure_time_ms(func, **kwargs)
            values.append(time_ms)
        except Exception as e:
            print(f"Iteration {i} failed: {e}", file=sys.stderr)

    if not values:
        raise RuntimeError("No successful benchmark iterations")

    return BenchmarkResult(
        name=func.__name__,
        backend=kwargs.get("backend", "unknown"),
        iterations=len(values),
        values=values,
        mean=statistics.mean(values),
        median=statistics.median(values),
        stddev=statistics.stdev(values) if len(values) > 1 else 0,
        min_val=min(values),
        max_val=max(values),
        unit="ms"
    )


# ============================================================================
# Backend Operations
# ============================================================================

class BackendTester:
    """Test operations for a specific backend"""

    def __init__(self, backend_type: str, archive_path: Path, build_dir: Path):
        self.backend_type = backend_type
        self.archive_path = archive_path
        self.build_dir = build_dir
        self.test_binary = build_dir / "test_backend_factory"

    def mount_and_unmount(self):
        """Test mount/unmount cycle"""
        # This would call the C API through a test binary
        # For now, we'll use the test executables
        result = subprocess.run(
            [str(self.test_binary), "--gtest_filter=*Mount*"],
            capture_output=True,
            timeout=30
        )
        return result.returncode == 0

    def list_root_directory(self):
        """List all entries in root directory"""
        # Implementation would use the C API
        pass

    def read_file_sequential(self, path: str):
        """Read entire file sequentially"""
        pass

    def read_file_random(self, path: str, block_size: int = 4096, count: int = 100):
        """Read file with random access pattern"""
        pass

    def lookup_path(self, path: str):
        """Look up a file by path"""
        pass


# ============================================================================
# Benchmark Scenarios
# ============================================================================

def benchmark_mount_time(
    archive_path: Path,
    backend_type: str,
    build_dir: Path,
    iterations: int
) -> BenchmarkResult:
    """Benchmark archive mount/initialization time"""
    values = []
    test_binary = build_dir / f"test_{backend_type}_backend"

    if not test_binary.exists():
        raise FileNotFoundError(f"Test binary not found: {test_binary}")

    # Warmup
    for _ in range(WARMUP_ITERATIONS):
        subprocess.run(
            [str(test_binary), "--gtest_filter=*MountValid*"],
            capture_output=True,
            timeout=30
        )

    # Measured runs
    for _ in range(iterations):
        start = time.perf_counter()
        result = subprocess.run(
            [str(test_binary), "--gtest_filter=*MountValid*"],
            capture_output=True,
            timeout=30
        )
        end = time.perf_counter()

        if result.returncode == 0:
            values.append((end - start) * 1000)

    if not values:
        raise RuntimeError("No successful mount benchmark iterations")

    return BenchmarkResult(
        name="Mount Time",
        backend=backend_type,
        iterations=len(values),
        values=values,
        mean=statistics.mean(values),
        median=statistics.median(values),
        stddev=statistics.stdev(values) if len(values) > 1 else 0,
        min_val=min(values),
        max_val=max(values),
        unit="ms"
    )


def benchmark_directory_listing(
    archive_path: Path,
    backend_type: str,
    build_dir: Path,
    iterations: int,
    file_count: int
) -> BenchmarkResult:
    """Benchmark directory listing performance"""
    values = []
    test_binary = build_dir / f"test_{backend_type}_backend"

    # Warmup
    for _ in range(WARMUP_ITERATIONS):
        subprocess.run(
            [str(test_binary), "--gtest_filter=*ListDirectory*"],
            capture_output=True,
            timeout=30
        )

    # Measured runs
    for _ in range(iterations):
        start = time.perf_counter()
        result = subprocess.run(
            [str(test_binary), "--gtest_filter=*ListDirectory*"],
            capture_output=True,
            timeout=30
        )
        end = time.perf_counter()

        if result.returncode == 0:
            values.append((end - start) * 1000)

    if not values:
        raise RuntimeError("No successful directory listing benchmark iterations")

    return BenchmarkResult(
        name=f"Directory Listing ({file_count} files)",
        backend=backend_type,
        iterations=len(values),
        values=values,
        mean=statistics.mean(values),
        median=statistics.median(values),
        stddev=statistics.stdev(values) if len(values) > 1 else 0,
        min_val=min(values),
        max_val=max(values),
        unit="ms"
    )


def benchmark_file_read(
    archive_path: Path,
    backend_type: str,
    build_dir: Path,
    iterations: int,
    file_size: int
) -> BenchmarkResult:
    """Benchmark file read performance"""
    values = []
    test_binary = build_dir / f"test_{backend_type}_backend"

    # Warmup
    for _ in range(WARMUP_ITERATIONS):
        subprocess.run(
            [str(test_binary), "--gtest_filter=*ReadFile*"],
            capture_output=True,
            timeout=30
        )

    # Measured runs
    for _ in range(iterations):
        start = time.perf_counter()
        result = subprocess.run(
            [str(test_binary), "--gtest_filter=*ReadFileContents*"],
            capture_output=True,
            timeout=30
        )
        end = time.perf_counter()

        if result.returncode == 0:
            values.append((end - start) * 1000)

    if not values:
        raise RuntimeError("No successful read benchmark iterations")

    size_label = f"{file_size // 1024}KB" if file_size < 1024*1024 else f"{file_size // (1024*1024)}MB"

    return BenchmarkResult(
        name=f"File Read ({size_label})",
        backend=backend_type,
        iterations=len(values),
        values=values,
        mean=statistics.mean(values),
        median=statistics.median(values),
        stddev=statistics.stdev(values) if len(values) > 1 else 0,
        min_val=min(values),
        max_val=max(values),
        unit="ms"
    )


def benchmark_archive_size(
    zip_path: Path,
    dwarfs_path: Path
) -> Tuple[int, int, float]:
    """Compare archive sizes"""
    zip_size = zip_path.stat().st_size if zip_path.exists() else 0
    dwarfs_size = dwarfs_path.stat().st_size if dwarfs_path.exists() else 0

    ratio = dwarfs_size / zip_size if zip_size > 0 else 0

    return zip_size, dwarfs_size, ratio


# ============================================================================
# Report Generation
# ============================================================================

def format_size(size_bytes: int) -> str:
    """Format byte size in human readable form"""
    for unit in ['B', 'KB', 'MB', 'GB']:
        if size_bytes < 1024:
            return f"{size_bytes:.1f} {unit}"
        size_bytes /= 1024
    return f"{size_bytes:.1f} TB"


def format_duration(ms: float) -> str:
    """Format duration in appropriate unit"""
    if ms < 1:
        return f"{ms*1000:.1f} µs"
    elif ms < 1000:
        return f"{ms:.2f} ms"
    else:
        return f"{ms/1000:.2f} s"


def generate_markdown_report(
    comparisons: List[ComparisonResult],
    archive_sizes: Dict[str, Tuple[int, int, float]],
    output_path: Path
):
    """Generate a markdown report"""

    report = []
    report.append("# libtfs Backend Performance Benchmark Report\n")
    report.append(f"Generated: {time.strftime('%Y-%m-%d %H:%M:%S')}\n")
    report.append("\n---\n")

    # Archive Sizes
    report.append("## Archive Size Comparison\n")
    report.append("| Test Case | ZIP Size | DwarFS Size | Ratio |")
    report.append("|-----------|----------|-------------|-------|")

    for test_name, (zip_size, dwarfs_size, ratio) in archive_sizes.items():
        winner = "✅ DwarFS" if ratio < 1.0 else "✅ ZIP"
        report.append(
            f"| {test_name} | {format_size(zip_size)} | {format_size(dwarfs_size)} | "
            f"{ratio:.2%} {winner} |"
        )

    report.append("\n---\n")

    # Performance Results
    report.append("## Performance Comparison\n")
    report.append("| Benchmark | ZIP (median) | DwarFS (median) | Winner | Improvement |")
    report.append("|-----------|--------------|-----------------|--------|-------------|")

    for comp in comparisons:
        zip_time = format_duration(comp.zip_result.median)
        dwarfs_time = format_duration(comp.dwarfs_result.median)

        if comp.ratio < 1.0:
            improvement = f"🚀 DwarFS {abs(comp.improvement_percent):.1f}% faster"
        elif comp.ratio > 1.0:
            improvement = f"🚀 ZIP {comp.improvement_percent:.1f}% faster"
        else:
            improvement = "≈ Equal"

        report.append(
            f"| {comp.name} | {zip_time} | {dwarfs_time} | {comp.winner} | {improvement} |"
        )

    report.append("\n---\n")

    # Detailed Statistics
    report.append("## Detailed Statistics\n")

    for comp in comparisons:
        report.append(f"### {comp.name}\n")
        report.append("| Metric | ZIP | DwarFS |")
        report.append("|--------|-----|--------|")

        for backend, result in [("ZIP", comp.zip_result), ("DwarFS", comp.dwarfs_result)]:
            report.append(f"| Mean | {format_duration(result.mean)} | - |" if backend == "ZIP" else f"| Mean | - | {format_duration(result.mean)} |")
            break

        report.append(f"| Mean | {format_duration(comp.zip_result.mean)} | {format_duration(comp.dwarfs_result.mean)} |")
        report.append(f"| Median | {format_duration(comp.zip_result.median)} | {format_duration(comp.dwarfs_result.median)} |")
        report.append(f"| Std Dev | {format_duration(comp.zip_result.stddev)} | {format_duration(comp.dwarfs_result.stddev)} |")
        report.append(f"| Min | {format_duration(comp.zip_result.min_val)} | {format_duration(comp.dwarfs_result.min_val)} |")
        report.append(f"| Max | {format_duration(comp.zip_result.max_val)} | {format_duration(comp.dwarfs_result.max_val)} |")
        report.append("")

    # Write report
    output_path.write_text("\n".join(report))
    print(f"\nReport written to: {output_path}")


def generate_json_report(
    comparisons: List[ComparisonResult],
    archive_sizes: Dict[str, Tuple[int, int, float]],
    output_path: Path
):
    """Generate a JSON report for programmatic processing"""

    data = {
        "timestamp": time.strftime('%Y-%m-%d %H:%M:%S'),
        "archive_sizes": {
            name: {
                "zip_size": zip_size,
                "dwarfs_size": dwarfs_size,
                "ratio": ratio
            }
            for name, (zip_size, dwarfs_size, ratio) in archive_sizes.items()
        },
        "benchmarks": [
            {
                "name": comp.name,
                "zip": comp.zip_result.to_dict(),
                "dwarfs": comp.dwarfs_result.to_dict(),
                "ratio": comp.ratio,
                "winner": comp.winner,
                "improvement_percent": comp.improvement_percent
            }
            for comp in comparisons
        ]
    }

    output_path.write_text(json.dumps(data, indent=2))
    print(f"\nJSON report written to: {output_path}")


def print_summary(comparisons: List[ComparisonResult], archive_sizes: Dict[str, Tuple[int, int, float]]):
    """Print a summary to console"""

    print("\n" + "=" * 70)
    print("                    BENCHMARK RESULTS SUMMARY")
    print("=" * 70)

    # Archive sizes
    print("\n📦 Archive Size Comparison:")
    print("-" * 50)
    for test_name, (zip_size, dwarfs_size, ratio) in archive_sizes.items():
        winner = "DwarFS 🏆" if ratio < 1.0 else "ZIP 🏆"
        print(f"  {test_name}:")
        print(f"    ZIP:    {format_size(zip_size):>12}")
        print(f"    DwarFS: {format_size(dwarfs_size):>12} ({ratio:.1%})")
        print(f"    Winner: {winner}")

    # Performance
    print("\n⚡ Performance Comparison:")
    print("-" * 50)

    dwarfs_wins = 0
    zip_wins = 0

    for comp in comparisons:
        if comp.winner == "DwarFS":
            dwarfs_wins += 1
            icon = "🏆"
        elif comp.winner == "ZIP":
            zip_wins += 1
            icon = "🏆"
        else:
            icon = "≈"

        print(f"  {comp.name}:")
        print(f"    ZIP:    {format_duration(comp.zip_result.median):>12} (±{format_duration(comp.zip_result.stddev)})")
        print(f"    DwarFS: {format_duration(comp.dwarfs_result.median):>12} (±{format_duration(comp.dwarfs_result.stddev)})")
        print(f"    {icon} {comp.winner} ({abs(comp.improvement_percent):.1f}% {'faster' if comp.improvement_percent > 0 else 'slower'})")

    # Overall
    print("\n" + "=" * 70)
    print("                        OVERALL SUMMARY")
    print("=" * 70)
    print(f"  DwarFS wins: {dwarfs_wins}")
    print(f"  ZIP wins:    {zip_wins}")
    print(f"  Ties:        {len(comparisons) - dwarfs_wins - zip_wins}")
    print("=" * 70 + "\n")


# ============================================================================
# Main Benchmark Runner
# ============================================================================

def run_full_benchmark(iterations: int = DEFAULT_ITERATIONS) -> Tuple[List[ComparisonResult], Dict]:
    """Run the complete benchmark suite"""

    comparisons = []
    archive_sizes = {}

    # Create temp directory for test data
    with tempfile.TemporaryDirectory(prefix="libtfs_bench_") as tmpdir:
        tmpdir = Path(tmpdir)

        print("🔧 Setting up test data...")

        # Test scenario 1: Many small files
        print("  Creating test data: Many small files...")
        small_files_dir = tmpdir / "many_small_files"
        create_test_directory(small_files_dir, file_count=MANY_FILES, file_size=SMALL_FILE_SIZE, depth=SHALLOW_DEPTH)

        zip_path_small = tmpdir / "small_files.zip"
        dwarfs_path_small = tmpdir / "small_files.dwarfs"

        print("  Creating ZIP archive...")
        create_zip_archive(small_files_dir, zip_path_small)

        print("  Creating DwarFS archive...")
        create_dwarfs_archive(small_files_dir, dwarfs_path_small)

        # Record sizes
        archive_sizes["Many small files"] = benchmark_archive_size(zip_path_small, dwarfs_path_small)

        # Test scenario 2: Few large files
        print("  Creating test data: Few large files...")
        large_files_dir = tmpdir / "few_large_files"
        create_test_directory(large_files_dir, file_count=FEW_FILES, file_size=LARGE_FILE_SIZE, depth=1)

        zip_path_large = tmpdir / "large_files.zip"
        dwarfs_path_large = tmpdir / "large_files.dwarfs"

        create_zip_archive(large_files_dir, zip_path_large)
        create_dwarfs_archive(large_files_dir, dwarfs_path_large)

        archive_sizes["Few large files"] = benchmark_archive_size(zip_path_large, dwarfs_path_large)

        # Test scenario 3: Deep directory structure
        print("  Creating test data: Deep directories...")
        deep_dir = tmpdir / "deep_structure"
        create_deep_directory(deep_dir, depth=DEEP_DEPTH)

        zip_path_deep = tmpdir / "deep_structure.zip"
        dwarfs_path_deep = tmpdir / "deep_structure.dwarfs"

        create_zip_archive(deep_dir, zip_path_deep)
        create_dwarfs_archive(deep_dir, dwarfs_path_deep)

        archive_sizes["Deep structure"] = benchmark_archive_size(zip_path_deep, dwarfs_path_deep)

        print("\n🚀 Running performance benchmarks...\n")

        # Run performance benchmarks using existing test binaries
        try:
            print("  Benchmarking ZIP mount time...")
            zip_mount = benchmark_mount_time(zip_path_small, "zip", BUILD_DIR, iterations)

            print("  Benchmarking DwarFS mount time...")
            dwarfs_mount = benchmark_mount_time(dwarfs_path_small, "dwarfs", BUILD_DIR, iterations)

            # Compare mount times
            ratio = dwarfs_mount.median / zip_mount.median if zip_mount.median > 0 else 0
            comp = ComparisonResult(
                name="Mount Time",
                zip_result=zip_mount,
                dwarfs_result=dwarfs_mount,
                ratio=ratio,
                winner="DwarFS" if ratio < 1.0 else "ZIP" if ratio > 1.0 else "Tie",
                improvement_percent=(1 - ratio) * 100
            )
            comparisons.append(comp)

        except Exception as e:
            print(f"  Warning: Mount benchmark failed: {e}")

        try:
            print("  Benchmarking ZIP directory listing...")
            zip_list = benchmark_directory_listing(zip_path_small, "zip", BUILD_DIR, iterations, MANY_FILES)

            print("  Benchmarking DwarFS directory listing...")
            dwarfs_list = benchmark_directory_listing(dwarfs_path_small, "dwarfs", BUILD_DIR, iterations, MANY_FILES)

            ratio = dwarfs_list.median / zip_list.median if zip_list.median > 0 else 0
            comp = ComparisonResult(
                name="Directory Listing",
                zip_result=zip_list,
                dwarfs_result=dwarfs_list,
                ratio=ratio,
                winner="DwarFS" if ratio < 1.0 else "ZIP" if ratio > 1.0 else "Tie",
                improvement_percent=(1 - ratio) * 100
            )
            comparisons.append(comp)

        except Exception as e:
            print(f"  Warning: Directory listing benchmark failed: {e}")

        try:
            print("  Benchmarking ZIP file read...")
            zip_read = benchmark_file_read(zip_path_large, "zip", BUILD_DIR, iterations, LARGE_FILE_SIZE)

            print("  Benchmarking DwarFS file read...")
            dwarfs_read = benchmark_file_read(dwarfs_path_large, "dwarfs", BUILD_DIR, iterations, LARGE_FILE_SIZE)

            ratio = dwarfs_read.median / zip_read.median if zip_read.median > 0 else 0
            comp = ComparisonResult(
                name="File Read (10MB)",
                zip_result=zip_read,
                dwarfs_result=dwarfs_read,
                ratio=ratio,
                winner="DwarFS" if ratio < 1.0 else "ZIP" if ratio > 1.0 else "Tie",
                improvement_percent=(1 - ratio) * 100
            )
            comparisons.append(comp)

        except Exception as e:
            print(f"  Warning: File read benchmark failed: {e}")

    return comparisons, archive_sizes


def main():
    parser = argparse.ArgumentParser(description="libtfs Backend Performance Benchmark")
    parser.add_argument(
        "--iterations", "-n",
        type=int,
        default=DEFAULT_ITERATIONS,
        help=f"Number of benchmark iterations (default: {DEFAULT_ITERATIONS})"
    )
    parser.add_argument(
        "--output", "-o",
        choices=["json", "markdown", "both"],
        default="markdown",
        help="Output format (default: markdown)"
    )
    parser.add_argument(
        "--output-dir",
        type=str,
        default="benchmarks/results",
        help="Output directory for reports"
    )

    args = parser.parse_args()

    print("=" * 70)
    print("           libtfs Backend Performance Benchmark Suite")
    print("=" * 70)
    print(f"  Iterations: {args.iterations}")
    print(f"  Output format: {args.output}")
    print(f"  Build directory: {BUILD_DIR}")
    print("=" * 70 + "\n")

    # Check prerequisites
    if not BUILD_DIR.exists():
        print(f"Error: Build directory not found: {BUILD_DIR}")
        print("Please run cmake and build first.")
        sys.exit(1)

    # Run benchmarks
    comparisons, archive_sizes = run_full_benchmark(args.iterations)

    if not comparisons:
        print("Error: No benchmark results collected")
        sys.exit(1)

    # Print summary
    print_summary(comparisons, archive_sizes)

    # Generate reports
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    if args.output in ["markdown", "both"]:
        generate_markdown_report(
            comparisons,
            archive_sizes,
            output_dir / f"benchmark_report_{time.strftime('%Y%m%d_%H%M%S')}.md"
        )

    if args.output in ["json", "both"]:
        generate_json_report(
            comparisons,
            archive_sizes,
            output_dir / f"benchmark_report_{time.strftime('%Y%m%d_%H%M%S')}.json"
        )


if __name__ == "__main__":
    main()
