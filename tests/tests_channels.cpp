#include <avnd/wrappers/channels_introspection.hpp>

using namespace avnd;
namespace test_1
{
struct object {
  static constexpr int channels = 2;
};

static_assert(input_channels<object>() == 2);
static_assert(output_channels<object>() == 2);
}

namespace test_2
{
struct object {
  static constexpr int input_channels = 3;
  static constexpr int output_channels = 5;

};

static_assert(input_channels<object>() == 3);
static_assert(output_channels<object>() == 5);
}

namespace test_3
{
struct object {
  struct {
    struct {
      float sample;
    } a;
    struct {
      float sample;
    } b;
  } inputs;
  struct {
    struct {
      float sample;
    } c;
  } outputs;
};

static_assert(input_channels<object>() == 2);
static_assert(output_channels<object>() == 1);
}


namespace test_mono_arrays
{
struct object {
  struct {
    struct {
      float* channel;
    } a;
    struct {
      float* channel;
    } b;
    struct {
      float* channel;
    } c;
  } inputs;
  struct {
    struct {
      float* channel;
    } c;
  } outputs;
};

static_assert(input_channels<object>() == 3);
static_assert(output_channels<object>() == 1);
}

namespace test_poly_arrays_undef
{
struct object {
  struct {
    struct {
      float** samples;
    } a;
    struct {
      float** samples;
    } b;
    struct {
      float** samples;
    } c;
  } inputs;
  struct {
    struct {
      float** samples;
    } c;
  } outputs;
};

static_assert(input_channels<object>() == avnd::undefined_channels);
static_assert(output_channels<object>() == avnd::undefined_channels);
}
namespace test_poly_arrays_nonconst
{
struct object {
  struct {
    struct {
      float** samples;
      int channels;
    } a;
    struct {
      float** samples;
      int channels;
    } b;
    struct {
      float** samples;
      int channels;
    } c;
  } inputs;
  struct {
    struct {
      float** samples;
      int channels;
    } c;
  } outputs;
};

static_assert(input_channels<object>() == avnd::undefined_channels);
static_assert(output_channels<object>() == avnd::undefined_channels);
}

namespace test_poly_arrays_const
{
struct object {
  struct {
    struct a_ {
      float** samples;
      static inline consteval int channels() { return 2; }
    } a;
    struct b_ {
      float** samples;
      static inline consteval int channels() { return 1; }
    } b;
    struct c_ {
      float** samples;
      static inline consteval int channels() { return 3; }
    } c;
  } inputs;
  struct {
    struct {
      float** samples;
      static inline consteval int channels() { return 4; }
    } c;
  } outputs;
};
static_assert(std::is_same_v<audio_bus_input_introspection<object>::indices_n, std::integer_sequence<int, 0, 1, 2>>);
static_assert(std::is_same_v<audio_bus_output_introspection<object>::indices_n, std::integer_sequence<int, 0>>);
static_assert(count_input_channels_in_busses<object>() == 6);
static_assert(count_output_channels_in_busses<object>() == 4);
static_assert(implicit_io_busses<object>);
static_assert(input_channels<object>() == 6);
static_assert(output_channels<object>() == 4);
}

int main()
{

}
