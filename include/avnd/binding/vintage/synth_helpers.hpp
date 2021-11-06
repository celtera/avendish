#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

namespace vintage
{
template <typename T>
struct SynthControls : Controls<T>
{
  std::atomic<float> unison_voices;
  std::atomic<float> unison_detune;
  std::atomic<float> unison_volume;

  template <typename Effect_T>
  void init(Effect_T& effect)
  {
    effect.Effect::setParameter
        = [](Effect* effect, int32_t index, float parameter) noexcept
    {
      if (index < Controls<T>::parameter_count + 3)
      {
        auto& self = *static_cast<Effect_T*>(effect);

        switch (index)
        {
          default:
            self.controls.parameters[index].store(
                parameter, std::memory_order_release);
            break;
          case Controls<T>::parameter_count:
            self.controls.unison_voices.store(
                parameter, std::memory_order_release);
            break;
          case Controls<T>::parameter_count + 1:
            self.controls.unison_detune.store(
                parameter, std::memory_order_release);
            break;
          case Controls<T>::parameter_count + 2:
            self.controls.unison_volume.store(
                parameter, std::memory_order_release);
            break;
        }
      }
    };

    effect.Effect::getParameter = [](Effect* effect, int32_t index) noexcept
    {
      if (index < Controls<T>::parameter_count + 3)
      {
        auto& self = *static_cast<Effect_T*>(effect);

        switch (index)
        {
          default:
            return self.controls.parameters[index].load(
                std::memory_order_acquire);
          case Controls<T>::parameter_count:
            return self.controls.unison_voices.load(std::memory_order_acquire);
          case Controls<T>::parameter_count + 1:
            return self.controls.unison_detune.load(std::memory_order_acquire);
          case Controls<T>::parameter_count + 2:
            return self.controls.unison_volume.load(std::memory_order_acquire);
        }
      }

      return 0.f;
    };
  }

  template <typename Effect_T>
  void label(Effect_T& effect, int index, void* ptr)
  {
    auto& self = effect.implementation;
    switch (index)
    {
      default:
        Controls<T>::label(effect, index, ptr);
        break;
      case Controls<T>::parameter_count:
        vintage::label{"Unison voices"}.copy_to(ptr);
        break;
      case Controls<T>::parameter_count + 1:
        vintage::label{"Unison detune"}.copy_to(ptr);
        break;
      case Controls<T>::parameter_count + 2:
        vintage::label{"Unison volume"}.copy_to(ptr);
        break;
    }
  }
  template <typename Effect_T>
  void name(Effect_T& effect, int index, void* ptr)
  {
    auto& self = effect.implementation;
    switch (index)
    {
      default:
        Controls<T>::name(effect, index, ptr);
        break;
      case Controls<T>::parameter_count:
        vintage::label{"Unison voices"}.copy_to(ptr);
        break;
      case Controls<T>::parameter_count + 1:
        vintage::label{"Unison detune"}.copy_to(ptr);
        break;
      case Controls<T>::parameter_count + 2:
        vintage::label{"Unison volume"}.copy_to(ptr);
        break;
    }
  }
  template <typename Effect_T>
  void display(Effect_T& effect, int index, void* ptr)
  {
    auto& self = effect.implementation;
    switch (index)
    {
      default:
        Controls<T>::display(effect, index, ptr);
        break;
      case Controls<T>::parameter_count:
        snprintf(
            reinterpret_cast<char*>(ptr),
            vintage::Constants::ParamStrLen,
            "%d",
            int(effect.controls.unison_voices.load() * 20));
        break;
      case Controls<T>::parameter_count + 1:
        snprintf(
            reinterpret_cast<char*>(ptr),
            vintage::Constants::ParamStrLen,
            "%.2f",
            effect.controls.unison_detune.load());
        break;
      case Controls<T>::parameter_count + 2:
        snprintf(
            reinterpret_cast<char*>(ptr),
            vintage::Constants::ParamStrLen,
            "%.2f",
            effect.controls.unison_volume.load());
        break;
    }
  }
};
}
