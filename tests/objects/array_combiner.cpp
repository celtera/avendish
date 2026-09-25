#include <catch2/catch_all.hpp>
#include <examples/Advanced/Utilities/ArrayBest.hpp>
#include <examples/Advanced/Utilities/ArrayCombiner.hpp>

#include <cmath>

using M = ao::ArrayCombinerMode;
using V = std::vector<float>;

namespace
{
V combine(M mode, std::vector<V> inputs)
{
  ao::ArrayCombiner c;
  c.inputs.in_i.ports.resize(inputs.size());
  for(std::size_t i = 0; i < inputs.size(); i++)
    c.inputs.in_i.ports[i].value = std::move(inputs[i]);
  c.inputs.mode.value = mode;
  c();
  return c.outputs.out.value;
}

bool near(const V& a, const V& b, float eps = 1e-5f)
{
  if(a.size() != b.size())
    return false;
  for(std::size_t i = 0; i < a.size(); i++)
    if(std::abs(a[i] - b[i]) > eps)
      return false;
  return true;
}
}

TEST_CASE("Array combiner: existing modes", "[array_combiner]")
{
  CHECK(combine(M::Sum, {{1, 2, 3}, {10, 20}}) == V{11, 22, 3});
  CHECK(combine(M::Product, {{1, 2, 3}, {10, 20}}) == V{10, 40, 3});
  CHECK(combine(M::Append, {{1, 2}, {3}}) == V{1, 2, 3});
  CHECK(combine(M::Intersperse, {{1, 2}, {3, 4}}) == V{1, 3, 2, 4});
  CHECK(combine(M::Min, {{1, 5, 3}, {4, 2}}) == V{1, 2, 3});
  CHECK(combine(M::Max, {{1, 5, 3}, {4, 2}}) == V{4, 5, 3});
  // One input passes through.
  CHECK(combine(M::Sum, {{1, 2}}) == V{1, 2});
}

TEST_CASE("Array combiner: element-wise arithmetic", "[array_combiner]")
{
  // Mean over the inputs that have the element.
  CHECK(combine(M::Mean, {{2, 4, 6}, {4, 8}}) == V{3, 6, 6});
  CHECK(combine(M::Subtract, {{10, 10, 10}, {1, 2}, {3}}) == V{6, 8, 10});
  CHECK(combine(M::Subtract, {{1}, {1, 2}}) == V{0, -2});
  CHECK(combine(M::AbsDifference, {{1, 5}, {4, 2}}) == V{3, 3});
  // A division by 0 (or by a missing element) gives 0.
  CHECK(combine(M::Divide, {{6, 6, 6}, {2, 0}}) == V{3, 0, 0});
  CHECK(combine(M::Divide, {{8, 9}, {2, 3}, {2, 3}}) == V{2, 1});
  CHECK(combine(M::Median, {{1, 9}, {5, 1}, {3, 5}}) == V{3, 5});
  CHECK(combine(M::Median, {{1, 1}, {3}}) == V{2, 1});
  // Clamp: input 1 between input 2 (low) and input 3 (high).
  CHECK(combine(M::Clamp, {{-1, 0.5, 3}, {0, 0, 0}, {1, 1, 1}}) == V{0, 0.5, 1});
  CHECK(combine(M::Clamp, {{-1, 5}, {0}}) == V{0, 5});
}

TEST_CASE("Array combiner: input 1 against each other input", "[array_combiner]")
{
  const V a{1, 0, 0};
  CHECK(near(combine(M::CosineSimilarity, {a, {2, 0, 0}, {0, 3, 0}, {-1, 0, 0}}), {1, 0, -1}));
  CHECK(near(combine(M::CosineSimilarity, {{1, 1}, {1, 0}}), {1.f / std::sqrt(2.f)}));
  // A zero vector has no direction: 0, not NaN.
  CHECK(combine(M::CosineSimilarity, {a, {0, 0, 0}}) == V{0});
  CHECK(combine(M::DotProduct, {{1, 2, 3}, {4, 5, 6}, {1}}) == V{32, 1});
  CHECK(near(combine(M::EuclideanDistance, {{0, 0}, {3, 4}, {0, 0}}), {5, 0}));
  CHECK(combine(M::ManhattanDistance, {{0, 0}, {3, -4}}) == V{7});
  // Over the common length.
  CHECK(combine(M::DotProduct, {{1, 1, 1}, {2, 2}}) == V{4});
  // Nothing to compare with: nothing out, not input 1.
  CHECK(combine(M::CosineSimilarity, {a}).empty());
}

TEST_CASE("Array combiner: comparisons", "[array_combiner]")
{
  CHECK(combine(M::Greater, {{1, 5, 3}, {2, 2, 3}}) == V{0, 1, 0});
  CHECK(combine(M::GreaterEqual, {{1, 5, 3}, {2, 2, 3}}) == V{0, 1, 1});
  CHECK(combine(M::Less, {{1, 5, 3}, {2, 2, 3}}) == V{1, 0, 0});
  CHECK(combine(M::LessEqual, {{1, 5, 3}, {2, 2, 3}}) == V{1, 0, 1});
  // Chained: 3 > 2 > 1 holds, 3 > 2 > 2 does not.
  CHECK(combine(M::Greater, {{3, 3}, {2, 2}, {1, 2}}) == V{1, 0});
  // A missing element fails.
  CHECK(combine(M::Greater, {{3, 3}, {1}}) == V{1, 0});
}

TEST_CASE("Array best match", "[array_best]")
{
  ao::ArrayBest b;
  b.inputs.in.value = {0.2f, 0.9f, 0.5f};
  b.inputs.scale.value = 0.f;
  b();
  CHECK(b.outputs.index.value == 1);
  CHECK(b.outputs.value.value == 0.9f);
  // Scale 0: every element equally likely.
  CHECK(near(b.outputs.probabilities.value, {1.f / 3, 1.f / 3, 1.f / 3}));

  // CLIP's scale: the best label takes nearly all.
  b.inputs.scale.value = 100.f;
  b();
  CHECK(b.outputs.probabilities.value[1] > 0.99f);
  float sum = 0.f;
  for(float p : b.outputs.probabilities.value)
    sum += p;
  CHECK(std::abs(sum - 1.f) < 1e-5f);

  // Lowest, for distances.
  b.inputs.mode.value = ao::ArrayBestMode::Lowest;
  b.inputs.scale.value = 1.f;
  b.inputs.in.value = {3.f, 1.f, 2.f};
  b();
  CHECK(b.outputs.index.value == 1);
  CHECK(b.outputs.value.value == 1.f);
  CHECK(b.outputs.probabilities.value[1] > b.outputs.probabilities.value[2]);
  CHECK(b.outputs.probabilities.value[2] > b.outputs.probabilities.value[0]);

  b.inputs.in.value.clear();
  b();
  CHECK(b.outputs.index.value == -1);
  CHECK(b.outputs.probabilities.value.empty());
}
