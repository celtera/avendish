#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/concepts/diagnostics.hpp>

#include <string>

namespace touchdesigner
{

/**
 * Bridges an object's halp::diagnostics onto TouchDesigner's node state.
 *
 * TD polls getErrorString / getWarningString / getInfoPopupString after the
 * cook and puts the node into the corresponding state, so the strings are
 * rendered once per cook and cached here rather than rebuilt per call. Building
 * them on this side keeps the object's storage fixed-size and allocation-free.
 *
 * A processor uses it as:
 *   diag.begin(implementation);   // before invoking the object
 *   ...cook...
 *   diag.end(implementation);     // renders the three channels
 */
struct diagnostics_state
{
  std::string error;
  std::string warning;
  std::string info;

  template <typename T>
  void begin(T& implementation)
  {
    if constexpr(avnd::has_diagnostics<T>)
      implementation.diagnostics.clear();
  }

  template <typename T>
  void end(T& implementation)
  {
    if constexpr(avnd::has_diagnostics<T>)
    {
      error.clear();
      warning.clear();
      info.clear();

      auto& d = implementation.diagnostics;
      for(unsigned i = 0; i < d.count; ++i)
      {
        const auto& e = d.entries[i];
        std::string* dst = nullptr;
        switch(e.severity)
        {
          case avnd::diagnostic_severity::info:
            dst = &info;
            break;
          case avnd::diagnostic_severity::warning:
            dst = &warning;
            break;
          // TD has no separate fatal channel.
          case avnd::diagnostic_severity::error:
          case avnd::diagnostic_severity::fatal:
            dst = &error;
            break;
        }
        if(!dst)
          continue;
        if(!dst->empty())
          *dst += '\n';
        dst->append(e.message());
      }

      if(d.dropped > 0)
      {
        // Report the overflow at the severity of what was actually lost:
        // always using the error channel would turn a node red for dropped
        // informational messages.
        std::string* dst = &info;
        switch(d.dropped_severity)
        {
          case avnd::diagnostic_severity::warning:
            dst = &warning;
            break;
          case avnd::diagnostic_severity::error:
          case avnd::diagnostic_severity::fatal:
            dst = &error;
            break;
          case avnd::diagnostic_severity::info:
            break;
        }
        if(!dst->empty())
          *dst += '\n';
        *dst += "... and " + std::to_string(d.dropped) + " more";
      }
    }
  }
};

// TD only reads the pointer during the call, so a c_str() into our cache is fine.
inline void set_string(TD::OP_String* out, const std::string& s)
{
  if(out && !s.empty())
    out->setString(s.c_str());
}

}
