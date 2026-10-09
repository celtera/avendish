#include "../examples/Tests/TestTick.hpp"
#include "../examples/Tests/TestTickFlicks.hpp"
#include "../examples/Tests/TestTickMusical.hpp"

#include <avnd/wrappers/effect_container.hpp>
#include <avnd/wrappers/process_adapter.hpp>

struct FrameCountEffect
{
  int processed{};
  void operator()(int frames) { processed += frames; }
};

int main()
{
  avnd::span<float*> empty{};

  avnd::effect_container<examples::tests::TestTick> basic{};
  avnd::process_adapter<examples::tests::TestTick> basic_processor;
  basic_processor.process(basic, empty, empty, 128);
  if(basic.effect.outputs.frames.value != 128)
    return 1;

  avnd::effect_container<examples::tests::TestTickMusical> musical{};
  avnd::process_adapter<examples::tests::TestTickMusical> musical_processor;
  halp::tick_musical transport{};
  transport.frames = 256;
  transport.tempo = 96.;
  musical_processor.process(musical, empty, empty, transport);
  if(musical.effect.outputs.frames.value != 256
     || musical.effect.outputs.tempo.value != 96.f)
    return 2;

  avnd::effect_container<examples::tests::TestTickFlicks> flicks{};
  avnd::process_adapter<examples::tests::TestTickFlicks> flicks_processor;
  halp::tick_flicks position{};
  position.frames = 512;
  position.relative_position = 0.25;
  flicks_processor.process(flicks, empty, empty, position);
  if(flicks.effect.outputs.position.value != 0.25f)
    return 3;

  avnd::effect_container<FrameCountEffect> frames{};
  avnd::process_adapter<FrameCountEffect> frames_processor;
  frames_processor.process(frames, empty, empty, 64);
  frames_processor.process(frames, empty, empty, transport);
  return frames.effect.processed == 320 ? 0 : 4;
}
