#include <avnd/binding/ossia/data_node.hpp>
#include <catch2/catch_test_macros.hpp>
#include <halp/midi.hpp>
#include <ossia/dataflow/graph/graph_utils.hpp>

namespace
{
struct value_metadata_off
{
  static constexpr bool local_midi_tick_batch = false;
};
struct value_metadata_on
{
  static constexpr bool local_midi_tick_batch = true;
};
static_assert(!oscr::use_local_midi_tick_batch<value_metadata_off>);
static_assert(oscr::use_local_midi_tick_batch<value_metadata_on>);

template <typename Message>
struct raw_midi_port
{
  static constexpr auto name() { return "Raw MIDI"; }
  Message* midi_messages{};
  int size{};
};

struct fixed_message
{
  unsigned char bytes[3]{};
  int64_t timestamp{};
};

template <bool Batch>
struct midi_effect
{
  static constexpr auto name() { return "MIDI binding regression"; }
  static constexpr bool local_midi_tick_batch() { return Batch; }
  struct
  {
    raw_midi_port<libremidi::message> raw;
    halp::midi_bus<"MIDI 1", libremidi::message> midi1;
    halp::midi_bus<"UMP", libremidi::ump> ump;
    halp::midi_bus<"Generic"> generic;
  } inputs;
  struct
  {
    raw_midi_port<libremidi::message> raw;
    struct
    {
      int value{};
    } non_midi;
    halp::midi_bus<"MIDI 1", libremidi::message> midi1;
    halp::midi_bus<"UMP", libremidi::ump> ump;
    halp::midi_bus<"Generic"> generic;
    raw_midi_port<fixed_message> fixed;
    raw_midi_port<libremidi::ump> raw_ump;
  } outputs;
  bool emit{true};
  int calls{};

  void operator()(int frames)
  {
    ++calls;
    if(!emit || frames == 0)
      return;
    libremidi::message message;
    message.bytes = {0x90, 60, 100};
    message.timestamp = frames - 1;
    outputs.raw.midi_messages[0] = message;
    outputs.raw.size = 1;
    outputs.midi1.midi_messages.push_back(message);
    auto ump = libremidi::ump_from_midi1(message);
    outputs.ump.midi_messages.push_back(ump);
    outputs.raw_ump.midi_messages[0] = ump;
    outputs.raw_ump.size = 1;
    outputs.generic.midi_messages.push_back({{0x90, 60, 100}, frames - 1});
    outputs.fixed.midi_messages[0] = {{0x90, 60, 100}, frames - 1};
    outputs.fixed.size = 1;
  }
};

ossia::token_request slice(int start, int frames)
{
  ossia::token_request token;
  token.prev_date = ossia::time_value{start};
  token.date = ossia::time_value{start + frames};
  token.start_sample = start;
  token.length_sample = frames;
  return token;
}

template <typename Node>
void initialize(Node& node, ossia::execution_state& state)
{
  node.finish_init();
  node.audio_configuration_changed({&state});
  node.prepare(state);
}

template <bool Batch>
struct moving_node : oscr::safe_node<midi_effect<Batch>>
{
  using base = oscr::safe_node<midi_effect<Batch>>;
  using base::base;
  bool move_first{};
  void
  run(const ossia::token_request& token,
      ossia::exec_state_facade state) noexcept override
  {
    base::run(token, state);
    if(move_first && token.start_sample == 0)
    {
      for(auto* port : this->root_outputs())
      {
        if(auto* midi = port->template target<ossia::midi_port>())
        {
          auto discarded = std::move(midi->messages);
          midi->messages.clear();
        }
      }
    }
  }
};

void require_output(ossia::graph_node& node, std::initializer_list<int64_t> timestamps)
{
  for(auto* port : node.root_outputs())
  {
    if(auto* midi = port->target<ossia::midi_port>())
    {
      REQUIRE(midi->messages.size() == timestamps.size());
      auto expected = timestamps.begin();
      for(const auto& packet : midi->messages)
      {
        REQUIRE(packet.timestamp == *expected++);
        const auto message = libremidi::midi1_from_ump(packet);
        REQUIRE(message.bytes == libremidi::midi_bytes{0x90, 60, 100});
      }
    }
  }
}
}

TEST_CASE("ossia MIDI aggregates real graph slices and resets repeated callbacks")
{
  ossia::execution_state state;
  moving_node<true> node{64, 44100., 1};
  initialize(node, state);
  node.move_first = true;
  node.request(slice(0, 8));
  node.request(slice(8, 8));
  node.request(slice(16, 0));
  ossia::graph_util::exec_node(node, state);
  require_output(node, {7, 15});

  node.requested_tokens.clear();
  node.impl.effect.emit = false;
  node.request(slice(0, 32));
  ossia::graph_util::exec_node(node, state);
  require_output(node, {});

  node.requested_tokens.clear();
  node.move_first = false;
  node.impl.effect.emit = true;
  node.request(slice(3, 5));
  ossia::graph_util::exec_node(node, state);
  require_output(node, {7});
  REQUIRE(node.impl.effect.outputs.raw.midi_messages[0].timestamp == 4);
  REQUIRE(node.impl.effect.outputs.midi1.midi_messages[0].timestamp == 4);
  REQUIRE(node.impl.effect.outputs.ump.midi_messages[0].timestamp == 4);
  REQUIRE(node.impl.effect.outputs.generic.midi_messages[0].timestamp == 4);
}

TEST_CASE("ossia MIDI explicit false preserves per-slice replacement without mutation")
{
  ossia::execution_state state;
  moving_node<false> node{64, 44100., 1};
  initialize(node, state);
  node.request(slice(0, 8));
  node.request(slice(8, 8));
  ossia::graph_util::exec_node(node, state);
  require_output(node, {15});
  REQUIRE(node.impl.effect.outputs.raw.midi_messages[0].timestamp == 7);
  REQUIRE(node.impl.effect.outputs.midi1.midi_messages[0].timestamp == 7);

  node.finish_run();
  require_output(node, {15});
  node.requested_tokens.clear();
  node.request(slice(0, 8));
  node.request(slice(8, 0));
  ossia::graph_util::exec_node(node, state);
  require_output(node, {});
}

TEST_CASE("ossia raw and dynamic MIDI inputs own slice-local copies")
{
  ossia::execution_state state;
  oscr::safe_node<midi_effect<false>> node{2, 44100., 1};
  initialize(node, state);
  auto& effect = node.impl.effect;
  ossia::midi_inlet input;
  for(int timestamp : {2, 4, 5, 6, 7, 8})
  {
    libremidi::message message;
    message.bytes = {0x90, 60, 100};
    message.timestamp = timestamp;
    input.data.messages.push_back(libremidi::ump_from_midi1(message));
  }
  using node_type = decltype(node);
  oscr::process_before_run<node_type, midi_effect<false>> before{node, effect, 4, 4};
  before(effect.inputs.raw, input, avnd::field_index<0>{});
  before(effect.inputs.midi1, input, avnd::field_index<1>{});
  before(effect.inputs.ump, input, avnd::field_index<2>{});
  before(effect.inputs.generic, input, avnd::field_index<3>{});
  input.data.messages.clear();
  REQUIRE(effect.inputs.raw.size == 4);
  REQUIRE(effect.inputs.midi1.size() == 4);
  REQUIRE(effect.inputs.ump.size() == 4);
  REQUIRE(effect.inputs.generic.size() == 4);
  for(int i = 0; i < 4; ++i)
  {
    REQUIRE(effect.inputs.raw.midi_messages[i].timestamp == i);
    REQUIRE(effect.inputs.midi1.midi_messages[i].timestamp == i);
    REQUIRE(effect.inputs.ump.midi_messages[i].timestamp == i);
    REQUIRE(effect.inputs.generic.midi_messages[i].timestamp == i);
    REQUIRE(effect.inputs.raw.midi_messages[i].bytes[1] == 60);
  }
}

TEST_CASE("ossia effects without MIDI do not require output storage")
{
  struct no_midi
  {
    static constexpr auto name() { return "No MIDI"; }
    int frames{};
    void operator()(int n) { frames += n; }
  };
  ossia::execution_state state;
  oscr::safe_node<no_midi> node{64, 44100., 1};
  initialize(node, state);
  node.request(slice(0, 4));
  node.request(slice(4, 12));
  ossia::graph_util::exec_node(node, state);
  REQUIRE(node.impl.effect.frames == 16);
}

TEST_CASE("ossia MIDI publishes bank and RPN controllers without deferred conversion")
{
  ossia::execution_state state;
  oscr::safe_node<midi_effect<false>> node{64, 44100., 1};
  initialize(node, state);
  node.start_frame_for_this_tick = 4;
  auto& messages = node.impl.effect.outputs.midi1.midi_messages;
  for(int controller : {0, 32, 101, 100, 6, 38})
  {
    libremidi::message message;
    message.bytes = {0xb0, static_cast<unsigned char>(controller), 127};
    message.timestamp = 3;
    messages = {message};
    node.finish_run();
    const auto& output = tuplet::get<2>(node.ossia_outlets.ports).data.messages;
    REQUIRE(output.size() == 1);
    REQUIRE(output.front().timestamp == 7);
    REQUIRE(libremidi::midi1_from_ump(output.front()).bytes == message.bytes);
  }
}
