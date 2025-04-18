#ifndef BENCHMARK_HPP
#define BENCHMARK_HPP

// Simple Abstraction 
#include <cmath>
#include <utility>
#include <vector>

// Template Abstraction
#include <tuple> 
#include <functional>
#include <any>
#include <iterator>
#include <type_traits>
#include <typeindex>

// Timings
#include <chrono>

// IO
#include <iostream>
#include <cstring>
#include <iomanip>

// Complex separation of types to delineate benchmark classes 
// Defines all problematic arguments that might be included in template  
// Checks if implements begin, end, and size (which would mean its a container)
template<typename T>
concept Container = requires(T t)
{
  std::begin(t);
  std::end(t);
  std::size(t);
};

// Checks if value is pointer
template<typename T>
concept Pointer = std::is_pointer_v<T>;

// Checks if a list of arguments has a container in it 
template<typename... Args>
concept HasContainer = (Container<Args> || ...);

// Same but for pointers
template<typename... Args>
concept HasPointer = (Pointer<Args> || ...);

// Checks if a value is neither a pointer or container (simple!)
template<typename T>
concept Simple = !Pointer<T> && !Container<T>;

// Checks if a list of arguments is simple 
template<typename... Args>
concept AllSimple = (Simple<Args> && ...);

// Checks if its any integer type (int8_t to size_t; etc)
template<typename T>
concept Integer = std::is_integral_v<T>;

// Checks if the two types are a pair of some pointer and its size
template<typename T, typename U>
concept PointerSizePair = Pointer<T> && Integer<U>;

template<typename T>
concept Constant = std::is_const_v<T>;

// Root class which all benchmarks inherit from 
template<typename... Args> 
class BenchmarkRoot
{
public:
  // Unique indentifying data for all benchmarked functions
  struct Unique
  {
    std::string id; 
    double runtime;
    float speedup;
  };
  
  BenchmarkRoot(size_t iter, Args... args) : 
    iter_(iter),
    args_(std::forward<Args>(args)...)
  {}

  // Guaranteed Members
  size_t iter_;
  size_t to_benchmark_{0};
  bool has_ran{false};
  // Can be handled by shared root class 
  std::tuple<Args...> args_;
  std::tuple<Args...> copied_args_;
  std::vector<size_t> pointer_sizes_;
  std::vector<std::unique_ptr<void, std::function<void(void*)>>> copied_ptrs_;
  bool needs_copies_;

  template<size_t I>
  auto process_argument(auto&& arg)
  {
    using ArgType = std::decay_t<decltype(arg)>;

    if constexpr (Simple<ArgType>)
    {
      // Return argument unchanged 
      return std::forward<decltype(arg)>(arg);
    }
    else if constexpr (Container<ArgType>)
    {
      // Copy the container 
      return ArgType(arg);
    }
    else if constexpr (Pointer<ArgType>)
    {

    // Argument must be a pointer thus a deep copy should be enacted 
    using pointer_type = std::remove_pointer_t<ArgType>;

    size_t size = pointer_sizes_[I];

    auto* ptr_copy = new pointer_type[size];
    std::memcpy(ptr_copy, arg, size * sizeof(pointer_type));

    // Push into vector the new unique pointer and a delete method
    copied_ptrs_.push_back(
      std::unique_ptr<void, std::function<void(void*)>>(
        ptr_copy,
        [](void* ptr) { delete[] static_cast<pointer_type*>(ptr); }
      )
    );
    return ptr_copy;
    }
    else
    {
      // Catch all for references that are still simple types 
      return std::forward<decltype(arg)>(arg);
    }
  }

  // Check for raw pointer and its number of elements proceeding 
  template<size_t I, size_t J, typename... Ts>
  void pointer_size_pair(Ts... args)
  {
    if constexpr (J < sizeof...(Ts) && I < sizeof...(Ts))
    {
      using first_arg  = std::decay_t<std::tuple_element_t<I, std::tuple<Ts...>>>;
      using second_arg = std::decay_t<std::tuple_element_t<J, std::tuple<Ts...>>>;

      // Check for raw pointer array size pair to set the size at index
      if constexpr (PointerSizePair<first_arg, second_arg>)
      {
        pointer_sizes_[I] = std::get<J>(std::make_tuple(args...));
      }
      else 
      {
        // Set size to 1 for the pointer to simplfy how much information should be copied 
        // Assume that a single raw pointer with no proceeding size is not an array but should 
        //    still be copied
        pointer_sizes_[I] = 1;
      }
    }
  }

  // Copies arguments in various manners depending on whether they contain pointers, containers, or neither 
  template<size_t... Is>
  void prepare_args(std::index_sequence<Is...>, Args... args)
  {
    args_ = std::make_tuple(args...);

    pointer_sizes_.resize(sizeof...(Args), 1);
    
    (pointer_size_pair<Is, Is+1>(args...), ...);

    copied_args_ = std::make_tuple(
      process_argument<Is>(std::get<Is>(args_))...
    );
  }

  // Simpler copy once we already extract the sizes of any potential pointers in original arguments 
  template<size_t... Is>
  std::tuple<Args...> simple_arg_copy(std::index_sequence<Is...>)
  {
    // unique ptrs are automatically destroyed when out of scope
    copied_ptrs_.clear();

    // recopy from original arguments
    return std::make_tuple(
      process_argument<Is>(std::get<Is>(args_))...
    );
  }

  // Virtual methods that will allow abstract sort to be implemented regardless of template
  virtual size_t get_count() const = 0; 
  virtual Unique& get_struct(size_t index) const = 0;
  virtual void swap(size_t first, size_t second) = 0;

  // Sorts functions by runtime as defined by virtual implementation 
  void sort()
  {
     for (size_t i = 1; i < get_count(); i++)
     {
       for (size_t j = 1; j < get_count(); j++)
       {

         if (i == j) { continue; }
          
         if (get_struct(i).runtime > get_struct(j).runtime)
         {
           swap(i, j);
         }
       }
     }
  }

  std::string format_runtime_string(double runtime)
  {
    std::ostringstream ss_result;

    // Check for empty runtime if user prints table without running 
    if (runtime == 0.0)
    {
      ss_result << "0.0000 s";
      return ss_result.str();
    }

    // Get power of ten -> order of magnitude 
    int order = static_cast<int>(std::floor(std::log10(std::abs(runtime))));
    int prefix_idx = 0;

    // Table of order prefix pairs 
    const std::pair<int, const char*> prefix_table[] = 
    {
      {0, " ns"},
      {1, " us"},
      {2, " ms"},
      {3, " s"},
      {-1, " ps"}
    };
  
    int group = order / 3;
    for (int i = 0; i < 5; i++)
    {
      if (group <= prefix_table[i].first)
      {
        prefix_idx = i;
        break;
      }
    }

    runtime *= std::pow(10.0, -prefix_table[prefix_idx].first * 3);

    ss_result << std::fixed << std::setprecision(4) << runtime << prefix_table[prefix_idx].second;

    return ss_result.str();
  }
};

// Simple Error, Simple Return, Arguments are not considered as this is handled by inheriting 
template<typename Error, typename Return, typename... Args>
class BenchmarkSimple : public BenchmarkRoot<Args...>
{
public:
  // Capture Function Pointer for error defn
  using fn_error = std::function<Error(Return, Return)>;
  using Unique = typename BenchmarkRoot<Args...>::Unique;
protected:

  // Define Result struct for Case
  struct Result
  {
    Unique data_;
    Return result;
    Error error;
  };

  // 'Private' error function and results vector 
  fn_error error_function_;
  std::vector<Result> results_;

  BenchmarkSimple(fn_error err, size_t iter) :
    BenchmarkRoot<Args...>(iter),
    error_function_(err)
  {
    results_.clear();
  }

  // Implementation of virtual methods to be inherited by Benchmark
  // For sort 
  size_t get_count() const override
  {
    return results_.size();
  }

  Unique& get_struct(size_t index) const override
  {
    return const_cast<Unique&>(results_[index].data_); 
  }

  void swap(size_t first, size_t second) override 
  {
    std::swap(results_[first], results_[second]);
  }

public:
  // Prints all results for benchmark matching template<E,R,Args>
  void print()
  {
    // Sort before display
    this->sort();

    // Header
    std::cout << ">> Iterations: " << this->iter_ << '\n';
    std::cout << std::left << std::setw(32) << "ID"
              << std::setw(16) << "Runtime"
              << std::setw(16) << "Speedup"
              << std::setw(16) << "Result"
              << std::setw(16) << "Error"
              << '\n';
    std::cout << "----------------------------------------------------------------------------------------------"
              << '\n';

    for (size_t i = 0; i < results_.size(); i++)
    {
      std::cout << std::left << std::setw(32) << results_[i].data_.id;
      
      std::string runtime_str = format_runtime_string(results_[i].data_.runtime);
      std::cout << std::left << std::setw(16) << runtime_str;
      
      // Speedup column (with "x fast" as part of the formatted string)
      std::ostringstream speedup_str;
      speedup_str << std::fixed << std::setprecision(6) << results_[i].data_.speedup << "x fast";
      std::cout << std::left << std::setw(16) << speedup_str.str();
      
      // Result column
      std::cout << std::setw(16) << std::fixed << std::setprecision(6) << results_[i].result;

      // Error column
      std::cout << std::setw(16) << std::fixed << std::setprecision(6) << results_[i].error;
      
      std::cout << '\n';
    }
  }

};

// Simple case of simple error that uses return and non-void return type 
template<typename Error, typename Return,  typename... Args>
class Benchmark : public BenchmarkSimple<Error, Return>
{
public: 
  // Function pointer for benchmarked function 
  using fn_benchmark = std::function<Return(Args...)>;
  // Local aliasing from inherited classes
  using typename BenchmarkSimple<Error, Return>::fn_error; 
  using typename BenchmarkSimple<Error, Return>::Result;
  using Unique = typename BenchmarkRoot<Args...>::Unique;

  Benchmark(fn_error err, fn_benchmark bench, size_t iter, Args... args) : 
    BenchmarkSimple<Error, Return>(err, iter)
  {
    functions_.clear();
    functions_.push_back(bench);

    this->BenchmarkRoot<Args...>::prepare_args(std::make_index_sequence<sizeof...(Args)>{}, std::forward<Args>(args)...);
    this->needs_copies_ = !this->pointer_sizes_.empty() || (Container<Args> || ...);

    init_baseline();
  }

  void insert(fn_benchmark function, const std::string& id)
  {
    // Set benchmark flags 
    if (this->has_ran) this->has_ran = false;
    this->to_benchmark_ = (this->to_benchmark_ == 0) 
      ? functions_.size() 
      : this->to_benchmark_;

    functions_.push_back(function);

    Result result;
    result.data_.id = id;
    result.data_.runtime = 0.0;
    result.data_.speedup = 1.0;
    result.result = Return();
    result.error  = Error();
    this->results_.push_back(result);
  }

  bool run()
  {
    // Check if any functions should be benchmarked
    if (this->to_benchmark_ == 0) { return false; }
    
    const size_t n_functions = functions_.size();
    if (n_functions == 1) { return false; }
    
    const size_t run_count = n_functions - this->to_benchmark_;

    // Allocate arrays to hold results 
    auto* average_runtime  = new double[run_count]();
    auto* function_results = new Return[run_count];
    auto* errors           = new Error[run_count];

    // Run the benchmark for each function that hasn't been ran
    for (size_t i = 0; i < this->iter_; i++)
    {
      // Run for each function that hasn't been benchmarked 
      for (size_t j = this->to_benchmark_; j < n_functions; j++)
      {
        // Check if we even need to recopy 
        if (this->needs_copies_)
        {
          // Recopy arguments to original per function to benchmark
          this->copied_args_ = this->BenchmarkRoot<Args...>::simple_arg_copy(std::make_index_sequence<sizeof...(Args)>{});
        }
        const size_t current_index = j - this->to_benchmark_;
        auto start = std::chrono::high_resolution_clock::now();
        function_results[current_index] = std::apply(functions_[j], this->BenchmarkRoot<Args...>::copied_args_);
        auto end = std::chrono::high_resolution_clock::now();

        auto runtime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        average_runtime[current_index] += static_cast<double>(runtime.count());
      }
    }

    // Collect runtime and custom error for each iteration 
    for (size_t j = this->to_benchmark_; j < n_functions; j++)
    {
      const size_t current_index = j - this->to_benchmark_;
      // Take average
      average_runtime[current_index] /= this->iter_;
      float speedup = this->results_[0].data_.runtime / average_runtime[current_index];

      // Collect error 
      errors[current_index] = this->error_function_(this->results_[0].result, function_results[current_index]);
      // Push results into public vector 
      this->results_[j].data_.runtime = average_runtime[current_index];
      this->results_[j].data_.speedup = speedup;
      this->results_[j].result        = function_results[current_index];
      this->results_[j].error         = errors[current_index];
    }

    // Free memory
    delete[] average_runtime;
    delete[] function_results;
    delete[] errors;

    // Reset to_benchmark_ to zero 
    this->to_benchmark_ = 0;
    this->has_ran = true;
    return true;
  }

private:
  std::vector<fn_benchmark> functions_;
  // Sets the 0th result etc 
  void init_baseline()
  {
    Return baseline_result = Return();
    double average_runtime = 0.0;

    // Run for preset number of iterations 
    for (size_t i = 0; i < this->iter_; i++)
    {
      // Check if we even need to recopy 
      if (this->needs_copies_)
      {
        // Recopy arguments to original
        this->copied_args_ = simple_arg_copy(std::make_index_sequence<sizeof...(Args)>{});
      }
      auto start = std::chrono::high_resolution_clock::now(); 
      auto result = std::apply(functions_[0], this->copied_args_); 
      // Collect result from first iteration 
      if (i == 0)
      {
        baseline_result = result;
      }
      auto end   = std::chrono::high_resolution_clock::now();
      
      auto runtime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
      // Accumulate runtime
      average_runtime += static_cast<double>(runtime.count());
    }

    // Take average 
    average_runtime /= this->iter_;

    Result result;
    result.data_.id = "Baseline";
    result.data_.runtime = average_runtime;
    result.data_.speedup = 1.0;
    result.result = baseline_result;
    result.error  = Error();

    this->results_.push_back(result);
  }
};

/*
 * Scenario where some complex/abstract error is needed when the function 
 * doesn't actually return anything
 *
 * Example:
 * Function takes hashtable and array. Hashes all values but doesn't return anything
 * The error might compare where hashed values end up 
 *
 * This of all templates needs some check for it pointers are immutable
 */

template<typename Error, typename... Args>
class BenchmarkComplexError : public BenchmarkRoot<Args...>
{
public:
  using fn_error = std::function<Error(void*, void*)>;
  using Unique = typename BenchmarkComplexError<Args...>::Unique;

  // Prints all results for benchmark matching template<E,R,Args>
  void print()
  {
    // Sort before display
    this->sort();

    // Header
    std::cout << ">> Iterations: " << this->iter_ << '\n';
    std::cout << std::left << std::setw(32) << "ID"
              << std::setw(16) << "Runtime"
              << std::setw(16) << "Speedup"
              << std::setw(16) << "Result"
              << std::setw(16) << "Error"
              << '\n';
    std::cout << "----------------------------------------------------------------------------------------------"
              << '\n';

    for (size_t i = 0; i < results_.size(); i++)
    {
      std::cout << std::left << std::setw(32) << results_[i].data_.id;
      
      std::string runtime_str = this->BenchmarkRoot<Args...>::format_runtime_string(results_[i].data_.runtime);
      std::cout << std::left << std::setw(16) << runtime_str;
      
      // Speedup column (with "x fast" as part of the formatted string)
      std::ostringstream speedup_str;
      speedup_str << std::fixed << std::setprecision(6) << results_[i].data_.speedup << "x fast";
      std::cout << std::left << std::setw(16) << speedup_str.str();

      // Error column
      std::cout << std::setw(16) << std::fixed << std::setprecision(6) << results_[i].error;
      
      std::cout << '\n';
    }
  }

protected:
  // Define empty error function 
  // User must define the cast to struct properly in their error function! (Not my problem)
  fn_error error_function;

  struct Result 
  {
    Unique data_;
    Error error; 
  };

  BenchmarkComplexError(fn_error err, size_t iter, Args... args) : 
    BenchmarkRoot<Args...>(iter, args...),
    error_function(err)
  {
    results_.clear();
  }

  // The error function must be independently set from constructor after constructor is ran 
  // Called by constructor. Requires the fn_error to have some template for the 
  // type of what is encapsulated by it 

  std::vector<Result> results_;

  // Implementation of virtual methods to be inherited by Benchmark
  // For sort 
  size_t get_count() const override
  {
    return results_.size();
  }

  Unique& get_struct(size_t index) const override
  {
    return const_cast<Unique&>(results_[index].data_); 
  }

  void swap(size_t first, size_t second) override 
  {
    std::swap(results_[first], results_[second]);
  }
};

template<typename Error, typename... Args>
class Benchmark<Error, void, Args...> : public BenchmarkComplexError<Error, Args...>
{
public:
  using fn_benchmark = std::function<void(Args...)>;
  using fn_error = typename BenchmarkComplexError<Args...>::fn_error;
  using Result = typename BenchmarkComplexError<Args...>::Result;
  using Unique = typename BenchmarkComplexError<Args...>::Unique;

  // This specific template's constructor takes an additional template for the type to peel from copied args
  template<typename S>
  Benchmark(fn_error err, fn_benchmark bench, size_t iter, Args... args) : 
    captured_type_(typeid(S)),
    BenchmarkComplexError<Args...>(err, iter, args...) 
  {
    functions_.clear();
    functions_.push_back(bench);

    this->BenchmarkRoot<Args...>::prepare_args(std::make_index_sequence<sizeof...(Args)>{}, std::forward<Args>(args)...);
    this->needs_copies_ = !this->pointer_sizes_.empty() || (Container<Args> || ...);

    init_baseline<S>();
  }

  void insert(fn_benchmark function, const std::string& id)
  {
    // Set benchmark flags 
    if (this->has_ran) this->has_ran = false;
    this->to_benchmark_ = (this->to_benchmark_ == 0) 
      ? functions_.size() 
      : this->to_benchmark_;

    functions_.push_back(function);

    Result result;
    result.data_.id = id;
    result.data_.runtime = 0.0;
    result.data_.speedup = 1.0;
    result.error  = Error();

    this->results_.push_back(result);
  }

  bool run()
  {
    // Check if any functions should be benchmarked
    if (this->to_benchmark_ == 0) { return false; }
    
    const size_t n_functions = functions_.size();
    if (n_functions == 1) { return false; }
    
    const size_t run_count = n_functions - this->to_benchmark_;

    // Allocate arrays to hold results 
    auto* average_runtime  = new double[run_count]();
    std::vector<std::any> error_arguments(run_count); 

    // Run the benchmark for each function that hasn't been ran
    for (size_t i = 0; i < this->iter_; i++)
    {
      // Run for each function that hasn't been benchmarked 
      for (size_t j = this->to_benchmark_; j < n_functions; j++)
      {
        // Check if we even need to recopy 
        if (this->needs_copies_)
        {
          // Recopy arguments to original per function to benchmark
          this->copied_args_ = simple_arg_copy(std::make_index_sequence<sizeof...(Args)>{});
        }
        const size_t current_index = j - this->to_benchmark_;
        auto start = std::chrono::high_resolution_clock::now();
        std::apply(functions_[j], this->copied_args_);
        auto end = std::chrono::high_resolution_clock::now();

        auto runtime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        average_runtime[current_index] += static_cast<double>(runtime.count());

        // Peel argument matching captured_type_ into error_arguments
        if (i == this->iter_ - 1)
        {
          // NOTE: THIS FUNCTION IS DESIGNATED TO USE COPIED_ARGS_ GUARANTEED
          error_arguments[current_index] = std::forward<captured_type_>(
            get_first_of_type<captured_type_>(
              std::make_index_sequence<sizeof...(Args)>{}
            ));
        }
      }
    }

    // Collect runtime and custom error for each iteration 
    for (size_t j = this->to_benchmark_; j < n_functions; j++)
    {
      const size_t current_index = j - this->to_benchmark_;
      // Take average
      average_runtime[current_index] /= this->iter_;
      float speedup = this->results_[0].data_.runtime / average_runtime[current_index];

      // Compute error for every collected value  
      auto baseline = std::any_cast<captured_type_>(base_error_argument_);
      auto current  = std::any_cast<captured_type_>(error_arguments[current_index]);
      this->results_[j].error = this->error_function(&baseline, &current);

      // Push results into public vector 
      this->results_[j].data_.runtime = average_runtime[current_index];
      this->results_[j].data_.speedup = speedup;
    }

    // Free memory
    delete[] average_runtime;

    // Reset to_benchmark_ to zero 
    this->to_benchmark_ = 0;
    this->has_ran = true;
    return true;
  }

private:

  std::type_index captured_type_;
  std::any base_error_argument_;
  std::vector<fn_benchmark> functions_;

  // Run as normal. Peel type from copied_args_ at end.
  void init_baseline()
  {
    double average_runtime = 0.0;

    // Run for preset number of iterations 
    for (size_t i = 0; i < this->iter_; i++)
    {
      // Check if we even need to recopy 
      if (this->needs_copies_)
      {
        // Recopy arguments to original
        this->copied_args_ = simple_arg_copy(std::make_index_sequence<sizeof...(Args)>{});
      }
      auto start = std::chrono::high_resolution_clock::now(); 
      std::apply(functions_[0], this->copied_args_);          // Returns void  
      // Collect result from first iteration 
      auto end   = std::chrono::high_resolution_clock::now();
      
      auto runtime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
      // Accumulate runtime
      average_runtime += static_cast<double>(runtime.count());
    }

    // Take average 
    average_runtime /= this->iter_;

    // Peel type from modified arguments copied_args_
    // Since error_argument is unknown until the user calls constructor<S> we have to do it like this  
    base_error_argument_ = std::forward<captured_type_>(get_first_of_type<captured_type_>(std::make_index_sequence<sizeof...(Args)>{}));

    // Copy the argument locally to pass for comparison (For baseline) 
    auto a = std::any_cast<captured_type_>(base_error_argument_);
    auto b = std::any_cast<captured_type_>(base_error_argument_);

    // Compute error 
    Error base_error = this->error_function(&a, &b);

    Result result;
    result.data_.id = "Baseline";
    result.data_.runtime = 0.0;
    result.data_.speedup = 1.0;
    result.error  = base_error;

    this->results_.push_back(result);
  }

  // Checks all elements of tuple Args... against the template type
  template<typename C, size_t... Is>
  C& get_first_of_type(std::index_sequence<Is...>)
  {
    C* result = nullptr; 
    
    // Lambda get for all elements of Args... if they match type C and if so sets C to that value of matching type
    [&]<size_t... Js>(std::index_sequence<Js...>) {
      ((result = std::is_same_v<C, std::tuple_element_t<Js, std::tuple<Args...>>> ?
        &std::get<Js>(this->copied_args_) : result), ...);
    }(std::make_index_sequence<sizeof...(Args)>{});

    // Error for if type is not contained within tuple
    if (result != nullptr) {
      throw std::runtime_error("Type not found within arguments!");
    } 
    return *result;
  }

  // Helper function that returns some boolean if it can find a matching type or not 
  template<typename C, size_t I>
  bool get_element(C*& result)
  {
    // Compare types of each element within the tuple 
    if constexpr (std::is_same_v<std::remove_reference_t<C>,
        std::remove_reference_t<std::tuple_element_t<I, std::tuple<Args...>>>>) {
      result = &std::get<I>(this->copied_args_);
      return true;
    }
    return false;
  }
};

// Instance where no error is recorded and fn ptr follows void function(args)
template<typename... Args>
class Benchmark<void, void, Args...> : public BenchmarkRoot<Args...>
{
public: 
  // Void function 
  using fn_benchmark = std::function<void(Args...)>;
  using Unique = BenchmarkRoot<Args...>::Unique;

  Benchmark(fn_benchmark bench, size_t iter, Args... args) :
    BenchmarkRoot<Args...>(iter, args...)
  {
    functions_.clear();
    functions_.push_back(bench);

    this->BenchmarkRoot<Args...>::prepare_args(std::make_index_sequence<sizeof...(Args)>{}, std::forward<Args>(args)...);
    this->needs_copies_ = !this->pointer_sizes_.empty() || (Container<Args> || ...);

    init_baseline();
  }
  
  void insert(fn_benchmark function, const std::string& id)
  {
    if (this->has_ran) this->has_ran = false;
    this->to_benchmark_ = (this->to_benchmark_ == 0) 
      ? functions_.size() 
      : this->to_benchmark_;

    functions_.push_back(function);

    Unique unique;
    unique.id = id; 
    unique.runtime = 0.0;
    unique.speedup = 1.0;
    this->data_.push_back(unique);
  }

  bool run()
  {
    // Check if any functions should be benchmarked
    if (this->to_benchmark_ == 0) { return false; }
    
    const size_t n_functions = functions_.size();
    if (n_functions == 1) { return false; }
    
    const size_t run_count = n_functions - this->to_benchmark_;

    // Allocate arrays to hold results 
    auto* average_runtime  = new double[run_count]();

    // Run the benchmark for each function that hasn't been ran
    for (size_t i = 0; i < this->iter_; i++)
    {
      // Run for each function that hasn't been benchmarked 
      for (size_t j = this->to_benchmark_; j < n_functions; j++)
      {
        // Check if we even need to recopy 
        if (this->needs_copies_)
        {
          // Recopy arguments to original per function to benchmark
          this->copied_args_ = simple_arg_copy(std::make_index_sequence<sizeof...(Args)>{});
        }
        const size_t current_index = j - this->to_benchmark_;
        auto start = std::chrono::high_resolution_clock::now();
        std::apply(functions_[j], this->copied_args_);
        auto end = std::chrono::high_resolution_clock::now();

        auto runtime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        average_runtime[current_index] += static_cast<double>(runtime.count());
      }
    }

    // Collect runtime and custom error for each iteration 
    for (size_t j = this->to_benchmark_; j < n_functions; j++)
    {
      const size_t current_index = j - this->to_benchmark_;
      // Take average
      average_runtime[current_index] /= this->iter_;
      float speedup = this->results_[0].data_.runtime / average_runtime[current_index];

      // Collect error 
      // Push results into public vector 
      this->results_[j].data_.runtime = average_runtime[current_index];
      this->results_[j].data_.speedup = speedup;
    }

    // Free memory
    delete[] average_runtime;

    // Reset to_benchmark_ to zero 
    this->to_benchmark_ = 0;
    this->has_ran = true;
    return true;
  }

  void print()
  {
    // Sort before display
    this->sort();

    // Header
    std::cout << ">> Iterations: " << this->iter_ << '\n';
    std::cout << std::left << std::setw(32) << "ID"
              << std::setw(16) << "Runtime"
              << std::setw(16) << "Speedup"
              << '\n';
    std::cout << "--------------------------------------------------------------------------"
              << '\n';

    for (size_t i = 0; i < data_.size(); i++)
    {
      std::cout << std::left << std::setw(32) << data_[i].id;
      
      std::string runtime_str = format_runtime_string(data_[i].runtime);
      std::cout << std::left << std::setw(16) << runtime_str;
      
      // Speedup column (with "x fast" as part of the formatted string)
      std::ostringstream speedup_str;
      speedup_str << std::fixed << std::setprecision(6) << data_[i].speedup << "x fast";
      std::cout << std::left << std::setw(16) << speedup_str.str();
      
      std::cout << '\n';
    }
  }

private:
  std::vector<fn_benchmark> functions_;
  std::vector<Unique> data_;

  // No return value. No error to store as baseline 
  void init_baseline()
  {
    double average_runtime = 0.0;

    // Run for preset number of iterations 
    for (size_t i = 0; i < this->iter_; i++)
    {
      // Check if we even need to recopy 
      if (this->needs_copies_)
      {
        // Recopy arguments to original
        this->copied_args_ = simple_arg_copy(std::make_index_sequence<sizeof...(Args)>{});
      }
      auto start = std::chrono::high_resolution_clock::now(); 
      std::apply(functions_[0], this->copied_args_);          // Returns void  
      // Collect result from first iteration 
      auto end   = std::chrono::high_resolution_clock::now();
      
      auto runtime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
      // Accumulate runtime
      average_runtime += static_cast<double>(runtime.count());
    }

    // Take average 
    average_runtime /= this->iter_;

    Unique unique;
    unique.id = "Baseline"; 
    unique.runtime = 0.0;
    unique.speedup = 1.0;
    this->data_.push_back(unique);
  }


// Virtual overrides
protected:
  size_t get_count() const override
  {
    return data_.size();
  }

  Unique& get_struct(size_t index) const override
  {
    return const_cast<Unique&>(data_[index]); 
  }

  void swap(size_t first, size_t second) override 
  {
    std::swap(data_[first], data_[second]);
  }

};

#endif // BENCHMARK_HPP
