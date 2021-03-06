// Copyright 2012 Cloudera Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <iomanip>
#include <iostream>
#include <sstream>

#include "util/benchmark.h"
#include "util/cpu-info.h"
#include "util/stopwatch.h"

using namespace std;

namespace impala {
  
double Benchmark::Measure(BenchmarkFunction function, void* args,
    int max_time, int batch_size) {
  int64_t target_cycles = CpuInfo::cycles_per_ms() * max_time;
  int64_t iters = 0;
  
  // Run it with the default batch size to roughly estimate how many iterations
  // it will take
  StopWatch sw;
  sw.Start();
  function(batch_size, args);
  sw.Stop();
  iters += batch_size;

  if (sw.ElapsedTime() < target_cycles) {
    int64_t iters_guess = (target_cycles / sw.ElapsedTime()) * batch_size;
    // Shoot for 110% of the guess. Going a little over is not a big deal.
    iters_guess *= 1.1;
    // Modify the batch size based on the guess.  We ran the function a small number
    // of times to estimate how fast the function is.  Run the remaining iterations at
    // in 20% increments.
    // TODO: we can make this more sophisticated if need to be dynamically ramp up and
    // ramp down the sizes.
    batch_size = (iters_guess - iters) / 5;
  }

  while (sw.ElapsedTime() < target_cycles) {
    sw.Start();
    function(batch_size, args);
    sw.Stop();
    iters += batch_size;
  }

  double ms_elapsed = sw.ElapsedTime() / CpuInfo::cycles_per_ms();
  return iters / ms_elapsed;
}

Benchmark::Benchmark(const string& name) : name_(name) {}

void Benchmark::AddBenchmark(const string& name, BenchmarkFunction fn, void* args) {
  BenchmarkResult benchmark;
  benchmark.name = name;
  benchmark.fn = fn;
  benchmark.args = args;
  benchmarks_.push_back(benchmark);
}

string Benchmark::Measure() {
  if (benchmarks_.empty()) return "";

  // Run a warmup to iterate through the data
  benchmarks_[0].fn(10, benchmarks_[0].args);

  stringstream ss;
  for (int i = 0; i < benchmarks_.size(); ++i) {
    benchmarks_[i].rate = Measure(benchmarks_[i].fn, benchmarks_[i].args);
  }

  int function_out_width = 30;
  int rate_out_width = 20;
  int comparison_out_width = 20;
  int padding = 0;
  int total_width = function_out_width + rate_out_width + comparison_out_width + padding;

  double base_line = benchmarks_[0].rate;
  
  ss << name_ << ":"
     << setw(function_out_width - name_.size() - 1) << "Function" 
     << setw(rate_out_width) << "Rate" 
     << setw(comparison_out_width) << "Comparison" << endl;
  for (int i = 0; i < total_width; ++i) {
    ss << '-';
  }
  ss << endl;

  for (int i = 0; i < benchmarks_.size(); ++i) {
    ss << setw(function_out_width) << benchmarks_[i].name 
       << setw(rate_out_width) << setprecision(4) << benchmarks_[i].rate 
       << setw(comparison_out_width - 1) << setprecision(4) 
       << (benchmarks_[i].rate / base_line) << "X" << endl;
  }
  
  return ss.str();
}

}
