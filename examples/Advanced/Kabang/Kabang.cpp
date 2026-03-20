#include "Kabang.hpp"

#include "Minibang.hpp"

namespace kbng
{

voice* DrumChannel::trigger(int ts, int vel_byte)
{
  double rate = this->sample.soundfile.rate;
  int channels = this->sample.soundfile.channels;

  if(channels <= 0)
    return nullptr;
  if(rate <= 0)
    return nullptr;

  voice& v = this->sample.trigger();
  double vel = vel_byte / 127.;
  v.ampl_track = vel * this->amp_envelop;
  v.filt_track = vel * this->filt_envelop;
  v.pitch_track = vel * this->pitch_envelop;

  v.amp_envelope = this->amp_env_enable;
  v.flt_envelope = this->filt_env_enable;
  v.pitch_envelope = this->pitch_env_enable;

  v.pitch = this->pitch;
  v.position = -ts;
  v.lpCutoff = this->lp_cutoff.value;
  v.lpRes = this->lp_res.value;
  v.hpCutoff = this->hp_cutoff.value;
  v.hpRes = this->hp_res.value;

  //qDebug() << "Pitch amplitude: " << pitch_envelop.value * v.velocity;
  //qDebug() << "Filt amplitude: " << filt_envelop.value * v.velocity;

  v.pitch_adsr.reset();
  v.pitch_adsr.set_sample_rate(rate);
  v.pitch_adsr.attack(this->pitch_attack);
  v.pitch_adsr.decay(this->pitch_decay);
  v.pitch_adsr.sustain(this->pitch_sustain);
  v.pitch_adsr.release(this->pitch_release);

  v.filt_adsr.reset();
  v.filt_adsr.set_sample_rate(rate);
  v.filt_adsr.attack(this->filt_attack);
  v.filt_adsr.decay(this->filt_decay);
  v.filt_adsr.sustain(this->filt_sustain);
  v.filt_adsr.release(this->filt_release);

  v.amp_adsr.reset();
  v.amp_adsr.set_sample_rate(rate);
  v.amp_adsr.attack(this->amp_attack);
  v.amp_adsr.decay(this->amp_decay);
  v.amp_adsr.sustain(this->amp_sustain);
  v.amp_adsr.release(this->amp_release);

  return &v;
}

voice* DrumChannel::trigger(int ts, float pitch_ratio, int vel_byte)
{
  auto v = trigger(ts, vel_byte);
  v->pitch *= pitch_ratio;
  return v;
}

void DrumChannel::run(int frames, int channels, double** out, double volume)
{
  this->sample.volume = this->vol * volume;
  sample.sample(frames, channels, out);
}

void Kabang::operator()(halp::tick t)
{
  for(auto& msg : inputs.midi)
  {
    auto m = libremidi::message{{msg.bytes[0], msg.bytes[1], msg.bytes[2]}};
    if(m.get_message_type() == libremidi::message_type::NOTE_ON)
    {
      for_each_channel([&msg](DrumChannel& channel) {
        if(msg.bytes[1] == channel.midi_key)
          channel.trigger(msg.timestamp, msg.bytes[2]);
      });
    }
    else if(m.get_message_type() == libremidi::message_type::NOTE_OFF)
    {
      for_each_channel([&msg](DrumChannel& channel) {
        if(msg.bytes[1] == channel.midi_key)
        {
          for(auto& voice : channel.sample.voices)
          {
            voice->amp_adsr.release();
          }
        }
      });
    }
  }
  for_each_channel([&](DrumChannel& channel) {
    channel.run(t.frames, 2, outputs.audio.samples, inputs.volume);
  });
}

void Minibang::operator()(halp::tick t)
{
  for(auto& msg : inputs.midi)
  {
    auto m = libremidi::message{{msg.bytes[0], msg.bytes[1], msg.bytes[2]}};
    if(m.get_message_type() == libremidi::message_type::NOTE_ON)
    {
      float pitch_ratio = float(msg.bytes[1]) / this->inputs.s1.midi_key.value;
      for_each_channel([&msg, pitch_ratio](DrumChannel& channel) {
        channel.trigger(msg.timestamp, pitch_ratio, msg.bytes[2]);
      });
    }
    else if(m.get_message_type() == libremidi::message_type::NOTE_OFF)
    {
      for_each_channel([&msg](DrumChannel& channel) {
        {
          for(auto& voice : channel.sample.voices)
          {
            voice->amp_adsr.release();
          }
        }
      });
    }
  }
  for_each_channel([&](DrumChannel& channel) {
    channel.run(t.frames, 2, outputs.audio.samples, inputs.volume);
  });
}
}
