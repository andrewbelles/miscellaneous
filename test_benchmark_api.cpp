#include "benchmark.hpp"
#include <cmath>
#include <random>
#include <vector>
#include <string>

// Returns some random value in large range to operate on
static float random_float()
{
  unsigned int seed = time(NULL);
  std::uniform_real_distribution<> d { 0.0, 1000000.0 };
  std::default_random_engine rng {seed};

  return d(rng);
}

__attribute__((optnone))
static float naive_square_root(float x)
{
  float guess = x / 2.0;
  for (int i = 0; i < 25; i++)
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
  float input = random_float();

  auto error_function = [](float a, float b) { return a - b; };
  Benchmark<float, float, float> benchmark(error_function, sqrt_wrapper, 1000000, input);
  benchmark.insert(naive_square_root, "Newton's Method");
  benchmark.run();
  benchmark.get_results();

  return 0;
}
