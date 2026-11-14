#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph/graph_utils.hpp>
#include <ossia/dataflow/graph_edge.hpp>

#include <avnd/binding/ossia/mono_audio_node.hpp>
#include <avnd/binding/ossia/ossia_audio_node.hpp>
#include <catch2/catch_test_macros.hpp>
#include <halp/audio.hpp>
#include <halp/controls.hpp>

#include <memory>
#include <vector>

// The nodes are driven the way libossia's graph drives them: each tick
// init_node() copies the upstream outlets into the inlets, run() processes,
// teardown_node() empties the inlets again. An inlet thus has no channel at
// all on every tick without a live source.
namespace
{
//! Set while the node runs, i.e. on the audio thread.
bool ticking = false;

struct BusAccumulator
{
  struct ins
  {
    halp::dynamic_audio_bus<"Input", double> audio;
  } inputs;
  struct
  {
    halp::mimic_audio_bus<"Output", &ins::audio> audio;
  } outputs;

  std::vector<double> sums;
  int prepared_channels = -1;
  int prepares_while_ticking = 0;
  void prepare(halp::setup setup)
  {
    prepared_channels = setup.input_channels;
    prepares_while_ticking += ticking;
    sums.assign(setup.input_channels, 3.);
  }
  void operator()(int frames)
  {
    for(int c = 0; c < outputs.audio.channels; ++c)
      for(int i = 0; i < frames; ++i)
        outputs.audio.samples[c][i]
            = c < int(sums.size()) ? sums[c] += inputs.audio.samples[c][i] : 0.;
  }
};

struct TwoBuses
{
  struct ins
  {
    halp::dynamic_audio_bus<"A", double> a;
    halp::dynamic_audio_bus<"B", double> b;
  } inputs;
  struct
  {
    halp::mimic_audio_bus<"Out A", &ins::a> a;
    halp::mimic_audio_bus<"Out B", &ins::b> b;
  } outputs;

  int prepared_a = -1, prepared_b = -1;
  int prepares_while_ticking = 0;
  void prepare(halp::setup)
  {
    prepared_a = inputs.a.channels;
    prepared_b = inputs.b.channels;
    prepares_while_ticking += ticking;
  }
  void operator()(int frames)
  {
    for(int c = 0; c < outputs.a.channels; ++c)
      for(int i = 0; i < frames; ++i)
        outputs.a.samples[c][i] = inputs.a.samples[c][i];
    for(int c = 0; c < outputs.b.channels; ++c)
      for(int i = 0; i < frames; ++i)
        outputs.b.samples[c][i] = inputs.b.samples[c][i];
  }
};

struct MonoGain
{
  struct
  {
    halp::hslider_f32<"Gain", halp::range{.min = 0., .max = 2., .init = 1.}> gain;
  } inputs;

  float operator()(float in) { return in * inputs.gain; }
};

//! An upstream box: what it writes in its outlet reaches the cable.
struct source_node final : ossia::nonowning_graph_node
{
  ossia::audio_outlet out;
  source_node() { m_outlets.push_back(&out); }
  std::string label() const noexcept override { return "source"; }

  //! Channel c carries c + 1 on every frame.
  void write(int channels, int frames)
  {
    out->set_channels(channels);
    for(int c = 0; c < channels; ++c)
      out->channel(c).assign(frames, double(c + 1));
  }
};

struct harness
{
  ossia::execution_state state;
  ossia::token_request token;
  std::vector<std::shared_ptr<source_node>> sources;
  std::vector<std::shared_ptr<ossia::graph_edge>> edges;

  harness()
  {
    state.bufferSize = 64;
    state.sampleRate = 48000.;
    state.modelToSamplesRatio = 1.;
    state.samplesToModelRatio = 1.;
    token.prev_date = ossia::time_value{11};
    token.date = ossia::time_value{28};
    token.start_sample = 11;
    token.length_sample = 17;
  }
  ~harness()
  {
    for(auto& e : edges)
      e->clear();
  }

  //! Set up as the score executor does, on the main thread.
  template <typename T>
  std::shared_ptr<oscr::safe_node<T>> make()
  {
    auto node = std::make_shared<oscr::safe_node<T>>(64, 48000., 1);
    node->prepare(state);
    node->finish_init();
    node->audio_configuration_changed({&state});
    return node;
  }

  //! A cable from a new upstream box into the inlet.
  void connect(const std::shared_ptr<ossia::graph_node>& node, int inlet)
  {
    auto src = std::make_shared<source_node>();
    auto edge = std::make_shared<ossia::graph_edge>(
        ossia::immediate_glutton_connection{}, &src->out,
        node->root_inputs()[inlet], src, node);
    edge->init();
    sources.push_back(src);
    edges.push_back(edge);
  }

  //! One tick, with the upstream box k writing channels[k] channels, or
  //! disabled (not running) for a negative count.
  void tick(ossia::graph_node& node, std::vector<int> channels = {})
  {
    for(std::size_t k = 0; k < sources.size(); ++k)
    {
      sources[k]->disable();
      if(channels[k] >= 0)
      {
        sources[k]->request(token);
        sources[k]->write(channels[k], state.bufferSize);
      }
    }
    ossia::graph_util::init_node(node, state);
    ticking = true;
    node.run(token, ossia::exec_state_facade{&state});
    ticking = false;
    ossia::graph_util::teardown_node(node, state);
  }
};
}

TEST_CASE("an inlet without a cable keeps its channels, silent", "[ossia][audio]")
{
  harness h;
  auto node = h.make<BusAccumulator>();
  auto& fx = node->impl.effect;
  REQUIRE(fx.prepared_channels == 2);

  auto& output = node->root_outputs()[0]->cast<ossia::audio_port>();
  for(int tick = 0; tick < 6; ++tick)
  {
    CAPTURE(tick);
    h.tick(*node);
    REQUIRE(output.channels() == 2);
    for(int c = 0; c < 2; ++c)
      REQUIRE(output.channel(c)[11 + 16] == 3.);
  }
  REQUIRE(fx.prepares_while_ticking == 0);
  REQUIRE(fx.prepared_channels == 2);
}

TEST_CASE("a source that stops and starts again keeps the node's state", "[ossia][audio]")
{
  harness h;
  auto node = h.make<BusAccumulator>();
  h.connect(node, 0);
  auto& fx = node->impl.effect;
  auto& output = node->root_outputs()[0]->cast<ossia::audio_port>();

  // A disabled box upstream, or one that writes no channel, leaves the inlet
  // empty.
  int active = 0;
  for(int channels : {2, 2, -1, -1, 2, 0, 2})
  {
    CAPTURE(channels);
    h.tick(*node, {channels});
    active += channels > 0;
    REQUIRE(output.channels() == 2);
    for(int c = 0; c < 2; ++c)
    {
      REQUIRE(fx.sums[c] == 3. + active * 17 * (c + 1.));
      REQUIRE(output.channel(c)[11 + 16] == fx.sums[c]);
    }
  }
  REQUIRE(fx.prepares_while_ticking == 0);
  REQUIRE(fx.prepared_channels == 2);
}

TEST_CASE("another channel count sets the node up again in its tick", "[ossia][audio]")
{
  harness h;
  auto node = h.make<BusAccumulator>();
  h.connect(node, 0);
  auto& fx = node->impl.effect;
  auto& output = node->root_outputs()[0]->cast<ossia::audio_port>();

  struct step
  {
    int channels, prepares, layout;
  };
  // prepare() runs inside run(), on the audio thread, and starts every
  // channel over.
  int ticks_since_prepare = 0;
  for(auto [channels, prepares, layout] : {
          step{2, 0, 2},
          step{1, 1, 1},
          step{2, 2, 2},
          step{5, 3, 5},
          step{-1, 3, 5},
          step{5, 3, 5},
          step{3, 4, 3},
      })
  {
    CAPTURE(channels);
    if(prepares != fx.prepares_while_ticking)
      ticks_since_prepare = 0;
    h.tick(*node, {channels});
    ticks_since_prepare += channels > 0;
    REQUIRE(fx.prepares_while_ticking == prepares);
    REQUIRE(fx.prepared_channels == layout);
    REQUIRE(output.channels() == layout);
    if(prepares > 0)
      for(int c = 0; c < layout; ++c)
        REQUIRE(fx.sums[c] == 3. + ticks_since_prepare * 17 * (c + 1.));
  }
}

TEST_CASE("each bus has its own layout", "[ossia][audio]")
{
  harness h;
  auto node = h.make<TwoBuses>();
  h.connect(node, 0);
  h.connect(node, 1);
  auto& fx = node->impl.effect;
  REQUIRE(fx.prepared_a == 2);
  REQUIRE(fx.prepared_b == 2);

  auto& out_a = node->root_outputs()[0]->cast<ossia::audio_port>();
  auto& out_b = node->root_outputs()[1]->cast<ossia::audio_port>();

  h.tick(*node, {2, 1});
  REQUIRE(fx.prepares_while_ticking == 1);
  REQUIRE(fx.prepared_a == 2);
  REQUIRE(fx.prepared_b == 1);
  // As many channels in all as 2 + 1.
  h.tick(*node, {1, 2});
  REQUIRE(fx.prepares_while_ticking == 2);
  REQUIRE(fx.prepared_a == 1);
  REQUIRE(fx.prepared_b == 2);
  REQUIRE(out_a.channels() == 1);
  REQUIRE(out_b.channels() == 2);
  for(int c = 0; c < 2; ++c)
    REQUIRE(out_b.channel(c)[11] == c + 1.);
  h.tick(*node, {1, 2});
  h.tick(*node, {-1, 2});
  REQUIRE(fx.prepares_while_ticking == 2);
  REQUIRE(out_a.channels() == 1);
  REQUIRE(out_a.channel(0)[11] == 0.);
}

TEST_CASE("a per-channel processor keeps its controls without input", "[ossia][audio]")
{
  harness h;
  auto node = h.make<MonoGain>();
  h.connect(node, 0);
  node->control_updated_from_ui<float, 0>(0.5f);

  auto& output = node->root_outputs()[0]->cast<ossia::audio_port>();
  for(int channels : {3, -1, 0, 3})
  {
    CAPTURE(channels);
    h.tick(*node, {channels});
    REQUIRE(output.channels() == 3);
    for(int c = 0; c < 3; ++c)
      REQUIRE(output.channel(c)[11] == (channels > 0 ? 0.5 * (c + 1) : 0.));
  }
  int instances = 0;
  for(auto state : node->impl.full_state())
  {
    REQUIRE(state.inputs.gain.value == 0.5f);
    ++instances;
  }
  REQUIRE(instances == 3);
}
