#include <avnd/common/aggregates.hpp>
#include <avnd/introspection/input.hpp>
#include <boost/pfr.hpp>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/mappers.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <tuplet/tuple.hpp>

namespace test_1
{
template <halp::static_string lit, auto setup>
struct log_pot : halp::knob_f32<lit, setup>
{
  using mapper = halp::log_mapper<std::ratio<85, 100>>;
};

struct DrumChannel
{
  halp_flag(recursive_group);

  halp::soundfile_port<"Drum"> sample;

  halp::knob_f32<"Volume", halp::range{0.0, 2., 1.}> vol;
  halp::knob_f32<"Pitch", halp::range{0.01, 10, 1.}> pitch;
  halp::spinbox_i32<"Input", halp::range{0, 127, 38}> midi_key;

  log_pot<"HP Cutoff", halp::range{20., 20000., 30.}> hp_cutoff;
  halp::knob_f32<"HP Reso", halp::range{0.001, 1., 0.5}> hp_res;

  log_pot<"LP Cutoff", halp::range{20., 20000., 20000.}> lp_cutoff;
  halp::knob_f32<"LP Reso", halp::range{0.001, 1., 0.5}> lp_res;

  log_pot<"P. Attack", halp::range{0.0001, 5., 0.01}> pitch_attack;
  log_pot<"P. Decay", halp::range{0.0001, 5., 0.05}> pitch_decay;
  halp::knob_f32<"P. Sustain", halp::range{0., 1., 1.}> pitch_sustain;
  log_pot<"P. Release", halp::range{0.0001, 100., 0.25}> pitch_release;
  halp::knob_f32<"Vel->Pitch", halp::range{-1., 1., 0.}> pitch_envelop;

  log_pot<"F. Attack", halp::range{0.0001, 5., 0.01}> filt_attack;
  log_pot<"F. Decay", halp::range{0.0001, 5., 0.05}> filt_decay;
  halp::knob_f32<"F. Sustain", halp::range{0., 1., 1.}> filt_sustain;
  log_pot<"F. Release", halp::range{0.0001, 100., 0.25}> filt_release;
  halp::knob_f32<"Vel->Filt", halp::range{-1., 1., 0.}> filt_envelop;

  log_pot<"Attack", halp::range{0.0001, 5., 0.01}> amp_attack;
  log_pot<"Decay", halp::range{0.0001, 5., 0.05}> amp_decay;
  halp::knob_f32<"Sustain", halp::range{0., 1., 1.}> amp_sustain;
  log_pot<"Release", halp::range{0.0001, 100., 0.25}> amp_release;

  halp::knob_f32<"Vel->Amp", halp::range{-1., 1., 0.}> amp_envelop;
};

class Kabang
{
public:
  halp_meta(name, "Kabang")
  halp_meta(category, "Audio/Synth")
  halp_meta(c_name, "kabang")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Basic MIDI drum sampler")
  halp_meta(uuid, "d6cd303e-c851-4655-806e-6c344cade2ae")

  struct ins
  {
    halp::midi_bus<"Input"> midi;

    DrumChannel s1;
    DrumChannel s2;
    DrumChannel s3;
    DrumChannel s4;
    DrumChannel s5;
    DrumChannel s6;
    DrumChannel s7;
    DrumChannel s8;

    halp::knob_f32<"Volume"> volume;
  } inputs;

  struct
  {
    halp::fixed_audio_bus<"Output", double, 2> audio;
  } outputs;
};

}

template <typename Functor, typename T = test_1::Kabang>
void process_inputs_impl(Functor& f, auto& in)
{
  using info = avnd::input_introspection<T>;
  [&]<typename K, K... Index>(std::integer_sequence<K, Index...>) {
    using namespace tpl;
    (f(avnd::pfr::get<Index>(in), avnd::field_index<Index>{}), ...);
      }(typename info::indices_n{});
}

test_1::Kabang g;
#include <iostream>
int main()
{
  auto ftor = [](auto& port, auto field) {
    if constexpr(requires { port.value; })
      std::cout << "port: _Z" << typeid(port).name() << " : " << port.value << "\n";
  };
  process_inputs_impl(ftor, g.inputs);
}
