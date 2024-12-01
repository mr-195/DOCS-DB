#!/bin/bash

# Base directory for storing benchmark results
BASE_DIR="./benchmark"

# Function to run the benchmark and save results
run_benchmark() {
  local request_count=$1
  local parallel_connections=$2
  local with_dpdkt=$3
  local result_file=$4

  # DPDK check - if "with_dpdkt" is 1, modify Redis settings (this is just a placeholder for actual setup)
  if [ "$with_dpdkt" -eq 1 ]; then
    # Placeholder for starting Redis with DPDK
    # e.g., service redis-dpdk start
    echo "Running benchmark with DPDK..."
  else
    # Placeholder for standard Redis setup
    echo "Running benchmark without DPDK..."
  fi

  # Run the redis-benchmark command
  redis-benchmark -h 127.0.0.1 -p 6379 -n "$request_count" -c "$parallel_connections" -t set,get > "$result_file"
  
  # Print the result for verification
  echo "Results saved to $result_file"
}

# Create necessary directories
create_directories() {
  mkdir -p "$BASE_DIR/10000-requests/10-concurrent-with-dpdk"
  mkdir -p "$BASE_DIR/10000-requests/10-concurrent-without-dpdk"
  mkdir -p "$BASE_DIR/100000-requests/100-concurrent-with-dpdk"
  mkdir -p "$BASE_DIR/100000-requests/100-concurrent-without-dpdk"
  mkdir -p "$BASE_DIR/1000000-requests/1000-concurrent-with-dpdk"
  mkdir -p "$BASE_DIR/1000000-requests/1000-concurrent-without-dpdk"
}

# Run benchmarks for different combinations of requests and connections
run_all_benchmarks() {
  # 10,000 requests
  run_benchmark 10000 10 1 "$BASE_DIR/10000-requests/10-concurrent-with-dpdk/results.txt"
  run_benchmark 10000 10 0 "$BASE_DIR/10000-requests/10-concurrent-without-dpdk/results.txt"
  
  # 100,000 requests
  run_benchmark 100000 100 1 "$BASE_DIR/100000-requests/100-concurrent-with-dpdk/results.txt"
  run_benchmark 100000 100 0 "$BASE_DIR/100000-requests/100-concurrent-without-dpdk/results.txt"
  
  # 1,000,000 requests
  run_benchmark 1000000 1000 1 "$BASE_DIR/1000000-requests/1000-concurrent-with-dpdk/results.txt"
  run_benchmark 1000000 1000 0 "$BASE_DIR/1000000-requests/1000-concurrent-without-dpdk/results.txt"
}

# Main execution
create_directories
run_all_benchmarks

echo "All benchmarks completed."