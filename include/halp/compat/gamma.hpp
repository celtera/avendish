#pragma once

namespace halp::compat
{

/**
 * @brief Used to easily connect objects to Lance Putnam's Gamma library
 */
struct gamma_domain
{
  void set_sample_rate(double v)
  {
    sample_rate = v;
    onDomainChange(v);
  }

  double spu() const noexcept
  {
    return sample_rate;
  }

  double ups() const noexcept
  {
    return 1. / sample_rate;
  }

  static constexpr auto domain() noexcept
  {
      struct Dom {
          struct Impl {
          static constexpr bool hasBeenSet() { return true; }
          } impl;
          auto operator->() const noexcept { return &impl; }

          constexpr operator bool() { return true; }
      } dom;
      return dom;
  }

  virtual ~gamma_domain() = default;
  virtual void onDomainChange(double r) { }

private:
  double sample_rate{};
};

}
