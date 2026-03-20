#include <catch2/catch_all.hpp>
#include <examples/Advanced/Audio/Gain.hpp>
TEST_CASE("Gain test", "[gain]")
{
  ao::Gain gain;
  GIVEN("Correct input")
  {
    ao::Gain::inputs inputs;
    inputs.gain.value = 0.5f;
    THEN("Correct output")
    {
        double res = gain(1.5, inputs);
        REQUIRE(res == 1.5 * 0.5);
    }
  }
}
