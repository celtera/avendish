#include <avnd/wrappers/concepts.hpp>
#include <avnd/wrappers/prepare.hpp>


template<typename T>
struct test_mono_audio_effect
{
  void operator()(T* in, T* out, int n);
};
template<typename T>
struct test_poly_audio_effect
{
  void operator()(T** in, T** out, int n);
};

static_assert(avnd::monophonic_arg_audio_effect<float, test_mono_audio_effect<float>>);
static_assert(!avnd::monophonic_arg_audio_effect<float, test_mono_audio_effect<double>>);
static_assert(!avnd::monophonic_arg_audio_effect<float, test_poly_audio_effect<double>>);
static_assert(avnd::polyphonic_arg_audio_effect<double, test_poly_audio_effect<double>>);
static_assert(!avnd::polyphonic_arg_audio_effect<double, test_poly_audio_effect<float>>);
static_assert(!avnd::polyphonic_arg_audio_effect<double, test_mono_audio_effect<float>>);

static_assert(avnd::monophonic_audio_processor<test_mono_audio_effect<float>>);
static_assert(!avnd::polyphonic_audio_processor<test_mono_audio_effect<float>>);
static_assert(avnd::float_processor<test_mono_audio_effect<float>>);
static_assert(!avnd::double_processor<test_mono_audio_effect<float>>);

static_assert(avnd::monophonic_audio_processor<test_mono_audio_effect<double>>);
static_assert(!avnd::polyphonic_audio_processor<test_mono_audio_effect<double>>);
static_assert(!avnd::float_processor<test_mono_audio_effect<double>>);
static_assert(avnd::double_processor<test_mono_audio_effect<double>>);

static_assert(!avnd::monophonic_audio_processor<test_poly_audio_effect<float>>);
static_assert(avnd::polyphonic_audio_processor<test_poly_audio_effect<float>>);
static_assert(avnd::float_processor<test_poly_audio_effect<float>>);
static_assert(!avnd::double_processor<test_poly_audio_effect<float>>);

static_assert(!avnd::monophonic_audio_processor<test_poly_audio_effect<double>>);
static_assert(avnd::polyphonic_audio_processor<test_poly_audio_effect<double>>);
static_assert(!avnd::float_processor<test_poly_audio_effect<double>>);
static_assert(avnd::double_processor<test_poly_audio_effect<double>>);

template<typename T>
struct test_port_mono_audio_effect
{
  struct inputs {
    struct audio_t {
      T* channel{};
    } audio;
  };

  struct outputs {
    struct {
      T* channel{};
    } audio;
  };

  void operator()(int n);
};

template<typename T>
struct test_port_poly_audio_effect
{
  struct inputs {
    struct {
      T** samples{};
    } audio;
  };

  struct outputs {
    struct {
      T** samples{};
    } audio;
  };

  void operator()(int n);
};

static_assert(!avnd::poly_array_sample_port<float, typename test_port_mono_audio_effect<float>::inputs::audio_t>);
static_assert(!avnd::poly_array_sample_port<double, typename test_port_mono_audio_effect<float>::inputs::audio_t>);


static_assert(avnd::monophonic_port_audio_effect<float, test_port_mono_audio_effect<float>>);
static_assert(!avnd::monophonic_port_audio_effect<float, test_port_mono_audio_effect<double>>);
static_assert(!avnd::monophonic_port_audio_effect<float, test_port_poly_audio_effect<double>>);
static_assert(avnd::polyphonic_port_audio_effect<double, test_port_poly_audio_effect<double>>);
static_assert(!avnd::polyphonic_port_audio_effect<double, test_port_poly_audio_effect<float>>);
static_assert(!avnd::polyphonic_port_audio_effect<double, test_port_mono_audio_effect<float>>);
static_assert(avnd::monophonic_audio_processor<test_port_mono_audio_effect<float>>);
static_assert(avnd::poly_sample_array_input_port_count<float, test_port_mono_audio_effect<float>> == 0);
static_assert(avnd::poly_sample_array_output_port_count<float, test_port_mono_audio_effect<float>> == 0);
static_assert(!avnd::polyphonic_arg_audio_effect<float, test_port_mono_audio_effect<float>>);
static_assert(!avnd::poly_array_port_based<float, test_port_mono_audio_effect<float>>);
static_assert(!avnd::polyphonic_processor<float, test_port_mono_audio_effect<float>>);
static_assert(!avnd::polyphonic_audio_processor<test_port_mono_audio_effect<float>>);
static_assert(avnd::float_processor<test_port_mono_audio_effect<float>>);

static_assert(avnd::monophonic_processor<float, test_port_mono_audio_effect<float>>);
static_assert(!avnd::monophonic_arg_audio_effect<double, test_port_mono_audio_effect<float>>);
static_assert(avnd::mono_sample_array_input_port_count<double, test_port_mono_audio_effect<float>> == 0);
static_assert(avnd::mono_sample_array_output_port_count<double, test_port_mono_audio_effect<float>> == 0);
static_assert(!avnd::mono_array_port_based<double, test_port_mono_audio_effect<float>>);
static_assert(!avnd::mono_per_sample_arg_processor<double, test_port_mono_audio_effect<float>>);
static_assert(!avnd::mono_per_sample_port_processor<double, test_port_mono_audio_effect<float>>);
static_assert(!avnd::monophonic_processor<double, test_port_mono_audio_effect<float>>);
static_assert(!avnd::polyphonic_processor<float, test_port_mono_audio_effect<float>>);
static_assert(!avnd::polyphonic_processor<double, test_port_mono_audio_effect<float>>);
static_assert(!avnd::double_processor<test_port_mono_audio_effect<float>>);

static_assert(avnd::monophonic_audio_processor<test_port_mono_audio_effect<double>>);
static_assert(!avnd::polyphonic_audio_processor<test_port_mono_audio_effect<double>>);
static_assert(!avnd::float_processor<test_port_mono_audio_effect<double>>);
static_assert(avnd::double_processor<test_port_mono_audio_effect<double>>);

static_assert(!avnd::monophonic_audio_processor<test_port_poly_audio_effect<float>>);
static_assert(avnd::polyphonic_audio_processor<test_port_poly_audio_effect<float>>);
static_assert(avnd::float_processor<test_port_poly_audio_effect<float>>);
static_assert(!avnd::double_processor<test_port_poly_audio_effect<float>>);

static_assert(!avnd::monophonic_audio_processor<test_port_poly_audio_effect<double>>);
static_assert(avnd::polyphonic_audio_processor<test_port_poly_audio_effect<double>>);
static_assert(!avnd::float_processor<test_port_poly_audio_effect<double>>);
static_assert(avnd::double_processor<test_port_poly_audio_effect<double>>);

template<typename T>
struct test_port_sample_audio_effect_value
{
  struct {
    struct audio_t {
      T sample{};
    } audio;
  } inputs;

  struct {
    struct {
      T sample{};
    } audio;
  } outputs ;

  void operator()(int n);
};
template<typename T>
struct test_port_mono_audio_effect_value
{
  struct {
    struct audio_t {
      T* channel{};
    } audio;
  } inputs;

  struct {
    struct {
      T* channel{};
    } audio;
  } outputs ;

  void operator()(int n);
};

template<typename T>
struct test_port_poly_audio_effect_value
{
  struct {
    struct {
      T** samples{};
    } audio;
  } inputs;

  struct {
    struct {
      T** samples{};
    } audio;
  } outputs;

  void operator()(int n);
};

static_assert(avnd::sample_input_port_count<float, test_port_sample_audio_effect_value<float>> == 1);
static_assert(avnd::sample_output_port_count<float, test_port_sample_audio_effect_value<float>> == 1);
static_assert(avnd::sample_port_based<float, test_port_sample_audio_effect_value<float>>);

static_assert(avnd::mono_sample_array_input_port_count<float, test_port_mono_audio_effect_value<float>> == 1);
static_assert(avnd::mono_sample_array_output_port_count<float, test_port_mono_audio_effect_value<float>> == 1);
static_assert(avnd::mono_array_port_based<float, test_port_mono_audio_effect_value<float>>);

static_assert(avnd::poly_sample_array_input_port_count<float, test_port_poly_audio_effect_value<float>> == 1);
static_assert(avnd::poly_sample_array_output_port_count<float, test_port_poly_audio_effect_value<float>> == 1);
static_assert(avnd::poly_array_port_based<float, test_port_poly_audio_effect_value<float>>);

static_assert(avnd::monophonic_port_audio_effect<float,   test_port_mono_audio_effect_value<float>>);
static_assert(!avnd::monophonic_port_audio_effect<float,  test_port_mono_audio_effect_value<double>>);
static_assert(!avnd::monophonic_port_audio_effect<float,  test_port_poly_audio_effect_value<double>>);
static_assert(avnd::polyphonic_port_audio_effect<double,  test_port_poly_audio_effect_value<double>>);
static_assert(!avnd::polyphonic_port_audio_effect<double, test_port_poly_audio_effect_value<float>>);
static_assert(!avnd::polyphonic_port_audio_effect<double, test_port_mono_audio_effect_value<float>>);

static_assert(avnd::monophonic_audio_processor<test_port_mono_audio_effect_value<float>>);
static_assert(!avnd::polyphonic_audio_processor<test_port_mono_audio_effect_value<float>>);
static_assert(avnd::float_processor<test_port_mono_audio_effect_value<float>>);
static_assert(!avnd::double_processor<test_port_mono_audio_effect_value<float>>);

static_assert(avnd::monophonic_audio_processor<test_port_mono_audio_effect_value<double>>);
static_assert(!avnd::polyphonic_audio_processor<test_port_mono_audio_effect_value<double>>);
static_assert(!avnd::float_processor<test_port_mono_audio_effect_value<double>>);
static_assert(avnd::double_processor<test_port_mono_audio_effect_value<double>>);

static_assert(!avnd::monophonic_audio_processor<test_port_poly_audio_effect_value<float>>);
static_assert(avnd::polyphonic_audio_processor<test_port_poly_audio_effect_value<float>>);
static_assert(avnd::float_processor<test_port_poly_audio_effect_value<float>>);
static_assert(!avnd::double_processor<test_port_poly_audio_effect_value<float>>);

static_assert(!avnd::monophonic_audio_processor<test_port_poly_audio_effect_value<double>>);
static_assert(avnd::polyphonic_audio_processor<test_port_poly_audio_effect_value<double>>);
static_assert(!avnd::float_processor<test_port_poly_audio_effect_value<double>>);
static_assert(avnd::double_processor<test_port_poly_audio_effect_value<double>>);


template<typename T>
struct test_per_sample_processor
{
  struct inputs {
  };
  struct outputs {
  };

  T operator()(T sample);
};


static_assert(avnd::mono_per_sample_arg_processor<double, test_per_sample_processor<double>>);
static_assert(!avnd::mono_per_sample_arg_processor<double, test_per_sample_processor<float>>);


/// Prepare ///
struct has_prepare {
struct setup {
  int channels{};
  float rate{};
};

void prepare(setup info)
{
}
};

static_assert(avnd::can_prepare<has_prepare>);
