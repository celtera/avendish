#pragma once
#include <fmt/format.h>
#include <halp/audio.hpp>
#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <ossia/network/common/complex_type.hpp>
#include <ossia/network/context.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/protocols/osc/osc_factory.hpp>
#include <readerwritercircularbuffer.h>
#include <samplerate.h>

#include <memory>
#include <thread>

namespace ad
{
struct resampler
{
  explicit resampler() noexcept
      : src{src_new(SRC_SINC_BEST_QUALITY, 1, nullptr)}
      , data(1024)
  {
  }
  resampler(resampler&&) noexcept = default;
  resampler& operator=(resampler&&) noexcept = default;
  resampler(const resampler&) noexcept = delete;
  resampler& operator=(const resampler&) noexcept = delete;

  ~resampler() { }

  using src_handle
      = std::unique_ptr<SRC_STATE, decltype([](SRC_STATE* p) { src_delete(p); })>;
  src_handle src;
  moodycamel::BlockingReaderWriterCircularBuffer<float> data;
};

struct osc_thread
{
  explicit osc_thread(
      std::string host, uint16_t port, std::span<std::shared_ptr<resampler>> resamplers)
      : m_config{.mode = conf::MIRROR, .version = conf::OSC1_1, .transport = ossia::net::udp_configuration{{.local = std::nullopt, .remote = ossia::net::send_socket_configuration{{host, port}}}}}
      , m_device{ossia::net::make_osc_protocol(m_ctx, m_config), "P"}
      , m_resamplers{resamplers.begin(), resamplers.end()}
      , m_sema{0}
  {
    auto& root = m_device.get_root_node();
    for(int i = 0; i < resamplers.size(); i++)
    {
      auto channel
          = ossia::create_parameter(root, fmt::format("/channel.{}", i), "float");
      m_channels.push_back(channel);
    }

    m_thread = std::thread([this] {
      while(!m_stop_flag.test(std::memory_order_acquire))
      {
        for(int k = 0; k < m_resamplers.size(); k++)
        {
          auto& queue = m_resamplers[k]->data;
          auto& osc = *m_channels[k];
          float sample{};
          if(queue.try_dequeue(sample))
          {
            osc.push_value(sample);
          }
        }
      }
      m_sema.release();
    });
  }

  ~osc_thread()
  {
    m_stop_flag.test_and_set(std::memory_order_release);
    m_sema.acquire();
    m_thread.join();
  }

  using conf = ossia::net::osc_protocol_configuration;
  conf m_config;

  std::shared_ptr<ossia::net::network_context> m_ctx
      = std::make_shared<ossia::net::network_context>();
  ossia::net::generic_device m_device{};
  std::vector<ossia::net::parameter_base*> m_channels;

  std::vector<std::shared_ptr<resampler>> m_resamplers;

  std::thread m_thread;
  std::atomic_flag m_stop_flag = ATOMIC_FLAG_INIT;
  std::binary_semaphore m_sema;
};

// The core audio object
struct AudioToOSC
{
public:
  halp_meta(name, "Audio to OSC")
  halp_meta(c_name, "audio_to_osc")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(category, "Audio/Effects")
  halp_meta(description, "Convert audio to OSC at a specific rate")
  halp_meta(uuid, "e525a387-b2ce-43b1-83c6-e308a4d7fd2f")

  struct
  {
    halp::dynamic_audio_bus<"In", float> audio;
    halp::knob_f32<"Frequency", halp::range{1., 1000., 401.2}> frequency;
  } inputs;

  struct outputs
  {

  } outputs;

  halp::setup s;
  void prepare(halp::setup info) noexcept
  {
    s = info;

    m_osc.reset();
    m_resamplers.clear();
    for(int i = 0; i < info.input_channels; i++)
      m_resamplers.push_back(std::make_shared<resampler>());
    m_osc = std::make_shared<osc_thread>("127.0.0.1", 1234, m_resamplers);
  }

  void operator()(int frames) noexcept
  {
    if(s.rate <= 0.)
      return;

    m_buffer.resize(4 * frames, boost::container::default_init);

    for(std::size_t i = 0; i < s.input_channels; ++i)
    {
      SRC_DATA data;
      auto& src = this->m_resamplers[i]->src;
      data.data_in = inputs.audio.samples[i];
      data.data_out = m_buffer.data();
      data.input_frames = frames;
      data.output_frames = m_buffer.size();
      data.input_frames_used = 0;
      data.output_frames_gen = 0;
      data.src_ratio = inputs.frequency.value / s.rate;
      data.end_of_input = 0;

      // Resample
      src_process(src.get(), &data);

      // Put in ring buffer
      auto& queue = m_resamplers[i]->data;
      for(auto sample : std::span(m_buffer.data(), data.output_frames_gen))
        queue.try_enqueue(sample);
    }
  }

private:
  boost::container::vector<float> m_buffer;
  std::vector<std::shared_ptr<resampler>> m_resamplers;
  std::shared_ptr<osc_thread> m_osc;
};
}
