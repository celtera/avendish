#pragma once
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <rnd/random.hpp>

#include <numbers>
#include <random>

namespace ao
{
struct LFO
{
  halp_meta(name, "LFO X")
  halp_meta(c_name, "lfo")
  halp_meta(category, "Control/Generators")
  halp_meta(description, "Low-frequency oscillator")
  halp_meta(author, "ossia score")
  halp_meta(uuid, "1e17e479-3513-44c8-a8a7-017be9f6ac8b")

  struct Inputs
  {
    halp::knob_f32<"Freq.", halp::range{0.01f, 100.f, 1.f}> freq;
    halp::knob_f32<"Ampl.", halp::range{0.01f, 100.f, 1.f}> ampl;
    halp::knob_f32<"Offset", halp::range{0.01f, 100.f, 1.f}> offset;
    halp::knob_f32<"Jitter", halp::range{0.01f, 100.f, 1.f}> jitter;
    halp::knob_f32<"Phase", halp::range{0.01f, 100.f, 1.f}> phase;

    struct Waveform
    {
      halp__enum(
          "Waveform", Sin, Sin, Triangle, Saw, Square, SampleAndHold, Noise1, Noise2,
          Noise3)
    } waveform;
  } inputs;
  struct
  {
    halp::val_port<"Out", std::optional<float>> out;
  } outputs;

  static inline std::random_device& random_source()
  {
    static thread_local std::random_device d;
    return d;
  }

  double phase{};
  rnd::pcg rd{random_source()};

  halp::setup setup;
  void prepare(halp::setup s) { this->setup = s; }

  void operator()(int frames)
  {
    float freq = inputs.freq;
    float ampl = inputs.ampl;
    float offset = inputs.offset;
    float jitter = inputs.jitter;
    float custom_phase = inputs.phase;
    //const std::string& type = "sin";
    float quantif = 0.;
    //           ossia::value_port& out;
    //           ossia::token_request tk; ossia::exec_state_facade st; State& s;

    static constexpr auto pi = std::numbers::pi_v<double>;
    static constexpr auto half_pi = pi / 2.;
    const double sine_ratio
        = pi * 2. / this->setup.rate; // ossia::flicks_per_second<double>;
    // const auto& waveform_map = Control::Widgets::waveformMap();
    const auto elapsed = frames; // tk.model_read_duration().impl;

    // out.type = ossia::val_type::FLOAT;
    // out.domain = ossia::domain_base<float>{0., 1.};

    if(quantif)
    {
      // Determine the frequency with the quantification
      // if(tk.unexpected_bar_change())
      // {
      //   s.phase = 0;
      // }

      // If quantif == 1, we quantize to the bar
      //   => f = 0.5 hz
      // If quantif == 1/4, we quantize to the quarter
      //   => f = 2hz
      // -> sin(elapsed * freq * 2 * pi / fps)
      // -> sin(elapsed * 4 * 2 * pi / fps)
      freq = 1. / (2. * quantif);
    }

    const auto ph_delta = elapsed * freq * sine_ratio;

    //if(const auto it = waveform_map.find_key(type))
    {
      auto ph = this->phase;
      if(jitter > 0)
      {
        ph += std::normal_distribution<float>(0., 0.25)(this->rd) * jitter;
      }
      // using namespace Control::Widgets;

      const auto add_val = [&](auto new_val) {
        outputs.out.value = ampl * new_val + offset;
        // const auto [tick_start, d] = st.timings(tk);
        // out.write_value(ampl * new_val + offset, tick_start);
      };

      using waveform_type = decltype(inputs.waveform.value);
      switch(inputs.waveform.value)
      {
        case waveform_type::Sin:
          add_val(std::sin(custom_phase + ph));
          break;
        case waveform_type::Triangle:
          add_val(std::asin(std::sin(custom_phase + ph)) / half_pi);
          break;
        case waveform_type::Saw:
          add_val(std::atan(std::tan(custom_phase + ph)) / half_pi);
          break;
        case waveform_type::Square:
          add_val((std::sin(custom_phase + ph) > 0.f) ? 1.f : -1.f);
          break;
        case waveform_type::SampleAndHold: {
          const auto start_s = std::sin(custom_phase + ph);
          const auto end_s = std::sin(custom_phase + ph + ph_delta);
          if((start_s > 0 && end_s <= 0) || (start_s <= 0 && end_s > 0))
          {
            add_val(std::uniform_real_distribution<float>(-1.f, 1.f)(this->rd));
          }
          break;
        }
        case waveform_type::Noise1:
          add_val(std::uniform_real_distribution<float>(-1.f, 1.f)(this->rd));
          break;
        case waveform_type::Noise2:
          add_val(std::normal_distribution<float>(0.f, 1.f)(this->rd));
          break;
        case waveform_type::Noise3:
          add_val(
              std::clamp(std::cauchy_distribution<float>(0.f, 1.f)(this->rd), 0.f, 1.f));
          break;
      }
    }

    this->phase += ph_delta;
  }
};
}
