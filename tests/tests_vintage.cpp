#include <avnd/binding/vintage/audio_effect.hpp>

struct float_arg
{
  static constexpr auto name() { return ""; }
  struct {

  } inputs;
  struct {

  } outputs;
  void operator()(float** in, float** out, int n)
  {

  }
};
static_assert(avnd::float_processor<float_arg>);

struct double_arg
{
  static constexpr auto name() { return ""; }
  struct {

  } inputs;
  struct {

  } outputs;
  void operator()(double** in, double** out, int n)
  {

  }
};
static_assert(avnd::double_processor<double_arg>);

struct any_arg
{
  static constexpr auto name() { return ""; }
  struct {

  } inputs;
  struct {

  } outputs;
  void operator()(std::floating_point auto** in, std::floating_point auto** out, int n)
  {

  }
};

struct float_port
{
  static constexpr auto name() { return ""; }
  struct {
    struct {
      float** samples;
      int channels;
    } audio;

  } inputs;
  struct {
    struct {
      float** samples;
      int channels;
    } audio;

  } outputs;
  void operator()(int n)
  {

  }
};
static_assert(avnd::float_processor<float_port>);
static_assert(avnd::polyphonic_audio_processor<float_port>);
static_assert(avnd::poly_array_port_based<float, float_port>);

struct double_port
{
  static constexpr auto name() { return ""; }
  struct {
    struct {
      double** samples;
      int channels;
    } audio;

  } inputs;
  struct {
    struct {
      double** samples;
      int channels;
    } audio;

  } outputs;
  void operator()(double** in, double** out, int n)
  {

  }
};
static_assert(avnd::double_processor<double_port>);
static_assert(avnd::polyphonic_audio_processor<double_port>);
static_assert(avnd::poly_array_port_based<double, double_port>);

template<std::floating_point T>
struct any_port
{
  static constexpr auto name() { return ""; }
  struct {
    struct {
      T** samples;
      int channels;
    } audio;

  } inputs;
  struct {
    struct {
      T** samples;
      int channels;
    } audio;

  } outputs;
  void operator()(int n)
  {

  }
};

static_assert(avnd::float_processor<any_port<float>>);
static_assert(avnd::double_processor<any_port<double>>);

intptr_t host_callback(
    vintage::Effect* effect,
    int32_t opcode,
    int32_t index,
    intptr_t value,
    void* ptr,
    float opt)
{
  return 0;
}

int main()
{
  using namespace avnd;
  avnd::process_adapter<float_arg> t;
  { vintage::SimpleAudioEffect<float_arg> fx{host_callback}; }
  { vintage::SimpleAudioEffect<double_arg> fx{host_callback}; }
  { vintage::SimpleAudioEffect<any_arg> fx{host_callback}; }
  { vintage::SimpleAudioEffect<any_port<float>> fx{host_callback}; }
  { vintage::SimpleAudioEffect<any_port<double>> fx{host_callback}; }

}
