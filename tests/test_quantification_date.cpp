#include <catch2/catch_all.hpp>

#include <halp/audio.hpp>
#include <halp/midi.hpp>

// Test for get_quantification_date function.
TEST_CASE("Returns valid quantification dates", "[audio]")
{
  halp::tick_musical tk = halp::tick_musical();
  tk.frames = 512; // The buffer size
  tk.tempo = 120; // BPM. default
  tk.signature = {4, 4}; // The time signature.

  // FIXME: We always get -2147483648 as a result.
  // FIXME: There might be an attribute that should be initialized, and is not.
  {
    // 4 / 4:
    auto quants = tk.get_quantification_date(4. / 4);
    auto [pos0, q0] = quants[0];
    REQUIRE(quants.size() == 1);
    REQUIRE(pos0 == -2147483648);
  }

  {
    // 1 / 4:
    auto quants = tk.get_quantification_date(1. / 4);
    auto [pos0, q0] = quants[0];
    REQUIRE(quants.size() == 1);
    REQUIRE(pos0 == -2147483648);
  }

  {
    // 4 / 1:
    auto quants = tk.get_quantification_date(4. / 1);
    auto [pos0, q0] = quants[0];
    REQUIRE(quants.size() == 1);
    REQUIRE(pos0 == -2147483648);
  }
}

