#pragma once
#include <halp/modules.hpp>

#include <functional>
#include <string>
#include <vector>
HALP_MODULE_EXPORT
namespace halp
{

template <typename T>
struct transaction
{
  std::function<void()> start;
  std::function<void(const T&)> update;
  std::function<void()> commit;
  std::function<void()> rollback;
};

// For halp::custom_multi_control: one gesture, one edit. Values normalized to [0, 1].
struct multi_transaction
{
  std::function<void()> start;
  std::function<void(int index, double normalized)> update;
  std::function<void()> commit;
  std::function<void()> rollback;
};

/**
 * What a custom item hands the host when a drag starts from it, from
 *
 *   halp::drag_payload drag_payload(double x, double y);
 *
 * which the host calls on the UI thread once the mouse has moved far enough
 * with a button held. Empty: no drag. Files are absolute paths; the host
 * offers them as file URLs, which every drop target that takes files reads.
 */
struct drag_payload
{
  std::vector<std::string> files;
  //! Optional data of a type of its own, e.g. "audio/midi".
  std::string mime;
  std::string data;

  bool empty() const noexcept { return files.empty() && data.empty(); }
};

/**
 * Files a custom item asks the host to place in the timeline next to the
 * process, through a member
 *
 *   std::function<void(halp::timeline_clip)> send_to_timeline;
 *
 * which the host sets when it can place clips, and leaves empty otherwise.
 * The files go together in one new box, each made the way a drop of it
 * would, in a slot of its own.
 */
struct timeline_clip
{
  std::vector<std::string> files;
  //! Where the clip starts, in seconds from the start of the process.
  double date{};
  //! Name of the box; the first file's name when empty.
  std::string name;
};
}
