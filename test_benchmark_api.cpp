#include "benchmark.hpp"
#include <cmath>
#include <vector>
#include <string>

__attribute__((optnone))
static float naive_square_root(float x)
{
  float guess = x / 2.0;
  for (int i = 0; i < 10; i++)
  {
    guess = 0.5 * (guess + x / guess);
  }
  return guess;
}

static float sqrt_wrapper(float x)
{
  return std::sqrt(x);
}

int main(void)
{
  auto error_function = [](float a, float b) { return a - b; };
  Benchmark<float, float, float> benchmark(error_function, naive_square_root, 10000, 144.0);

  benchmark.insert(sqrt_wrapper, "Standard Libary sqrt()");

  benchmark.run();

  benchmark.get_results();

  return 0;
}
