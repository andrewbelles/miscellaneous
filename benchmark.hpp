#ifndef BENCHMARK_HPP
#define BENCHMARK_HPP

#include <vector>
#include <tuple> 
#include <functional>
#include <chrono>
#include <iostream>
#include <cstring>

template <typename Error, typename Return, typename... Args>
class Benchmark 
{
public:
  // Define struct to hold results of benchmark
  struct Result
  {
    std::string id;
    double runtime;
    Error result;
    float speedup;
  };

  // Define function pointers. Public function types
  using error_function_ptr     = std::function<Error(Return, Return)>;
  using benchmark_function_ptr = std::function<Return(Args...)>; 

  // Construct benchmark object 
  Benchmark(
    benchmark_function_ptr base,
    error_function_ptr efunc,
    size_t iter,
    Args... args) 
  :
    error_function_(efunc),
    iter_(iter),
    args_(std::forward<Args>(args)...),
    to_benchmark_(0),
    has_ran(false)
  { 
    functions_.clear();
    functions_.push_back(base);

    // Set the baseline value with baseline function
    results_.clear();
    init_baseline();
  }

  // Insert a new function to be benchmarked
  void insert(benchmark_function_ptr function, const char* id)
  {
    // Check if any other functions should be benchmarked, keep start index if yes 
    has_ran = (has_ran = true) ? false : true;
    to_benchmark_ = (to_benchmark_ == 0) ? functions_.size() : to_benchmark_;
    functions_.push_back(function);
    
    // Initialize the runtime to 0 ns and result to baseline 
    results_.push_back(
      (Result)
      { 
        .id = id,
        .runtime = 0.0,
        .result = results_[0].result 
      }
    );
  }

  // Returns whether benchamrks where completed 
  bool run()
  {
    // Check if any functions should be benchmarked
    if (to_benchmark_ == 0)
      return false;
    
    const size_t n_functions = functions_.size();
    if (n_functions == 1)
      return false;

    // Allocate arrays to hold results 
    double* average_runtime  = new double[n_functions - to_benchmark_]();
    Return* function_results = new Return[n_functions - to_benchmark_];
    Error*  errors           = new Error[n_functions - to_benchmark_];

    // Run the benchmark for each function that hasn't been ran
    for (size_t i = 0; i < iter_; i++)
    {
      // Run for each function that hasn't been benchmarked 
      for (size_t j = to_benchmark_; j < n_functions; j++)
      {
        auto start = std::chrono::high_resolution_clock::now();
        function_results[j - to_benchmark_] = std::apply(functions_[j], args_);
        auto end = std::chrono::high_resolution_clock::now();

        auto runtime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        average_runtime[j - to_benchmark_] += static_cast<double>(runtime.count());
      }
    }

    // Collect runtime and custom error for each iteration 
    for (size_t j = to_benchmark_; j < n_functions; j++)
    {
      // Take average
      average_runtime[j - to_benchmark_] /= iter_;
      float speedup = results_[0].runtime / average_runtime[j - to_benchmark_];

      // Collect error 
      errors[j - to_benchmark_] = error_function_(
          results_[0].result, function_results[j - to_benchmark_]
      );
      // Push results into public vector 
      results_[j].runtime = average_runtime[j - to_benchmark_], 
      results_[j].result  = errors[j - to_benchmark_];
      results_[j].speedup = speedup;
    }

    // Free memory
    delete[] average_runtime;
    delete[] function_results;
    delete[] errors;

    // Reset to_benchmark_ to zero 
    to_benchmark_ = 0;
    has_ran = true;
    return true;
  }

  // Prints all results stored in class
  void get_results()
  {
    // Sort before display
    sort_results();

    for (size_t i = 0; i < functions_.size(); i++)
    {
      std::cout << results_[i].id << '\n'; 
      std::cout << "  Runtime: " << results_[i].runtime << " ns\n";
      std::cout << "  Error: " << results_[i].result << '\n';
      if (results_[i].id != "Baseline")
        std::cout << "  " << results_[i].speedup << "x fast\n";
    }
  }

private:
  // Functions  
  std::vector<benchmark_function_ptr> functions_;
  error_function_ptr error_function_;
  size_t iter_;
  std::tuple<Args...> args_;
  size_t to_benchmark_; // Index of nth function to start benchmark from 
  std::vector<Result> results_; 
  bool has_ran;

  // Set the 0th value of function and baseline result  
  void init_baseline() {
    double average_runtime = 0.0;
    // Initialize to default value 
    Error result_value = Error();

    for (size_t i = 0; i < iter_; i++)
    {
      auto start = std::chrono::high_resolution_clock::now();
      auto result = std::apply(functions_[0], args_);
      // If first iter set the baseline result to compare to 
      if (i == 0) {
        result_value = result;
      }
      auto end = std::chrono::high_resolution_clock::now();

      auto runtime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
      // Cast to double and sum
      average_runtime += static_cast<double>(runtime.count());
    }

    // Take average of runtime
    average_runtime /= iter_;
    results_.push_back(
      (Result) 
      {
        .id = "Baseline",
        .runtime = average_runtime,
        .result = result_value
      }
    );
  }
  
  // Selection sort since element count is low
  void sort_results()
  {
    // Skip baseline 
    for (size_t i = 1; i < results_.size(); i++)
    {
      for (size_t j = 1; j < results_.size(); j++)
      {
        // Check if self 
        if (i == j) continue;
        // Check if runtime is higher 
        if (results_[i].runtime > results_[j].runtime) {
          // Swap
          Result temp = results_[i];
          results_[i] = results_[j];
          results_[j] = temp;
        }
      }
    }
  }
};

#endif // BENCHMARK_HPP
