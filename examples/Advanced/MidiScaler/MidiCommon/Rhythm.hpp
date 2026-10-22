#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

namespace mtk
{
//! Whether step `i` of `steps` is a hit when `pulses` of them are spread as
//! evenly as possible, the rhythm turned `rotation` steps later.
inline bool euclid_hit(int i, int steps, int pulses, int rotation) noexcept
{
  if(steps <= 0 || pulses <= 0)
    return false;
  if(pulses >= steps)
    return true;
  const int j = (((i - rotation) % steps) + steps) % steps;
  return (j * pulses) % steps < pulses;
}

/**
 * @brief Delay of a step under swing, as a fraction of the step length.
 *
 * Steps go by pairs and the second of each pair is late by `amount * 2 / 3`
 * of a step: 0 is straight, 0.5 puts the off-beat on the triplet (2/3 of the
 * pair), 1 at 5/6 of the pair.
 */
inline double swing_delay(long long step, double amount) noexcept
{
  if((step & 1) == 0 || amount <= 0.)
    return 0.;
  if(amount > 1.)
    amount = 1.;
  return amount * 2. / 3.;
}
}
