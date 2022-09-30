#pragma once
#include <halp/file_port.hpp>

namespace examples::helpers
{
struct CSVFileReader
{
  halp_meta(name, "CSV reader")
  halp_meta(c_name, "avnd_csv_reader")
  halp_meta(uuid, "2a0ca4bb-a3ee-47f2-9830-195b35663c0e")

  struct
  {
    struct : halp::file_port<"CSV file">
    {
      halp_flag(file_watch);
      halp_meta(filters, "CSV file (*.csv)");

      void update(CSVFileReader& reader) { reader.position = 0; }
    } file;
  } inputs;

  struct
  {
    halp::val_port<"Element", std::optional<std::string>> element;
  } outputs;

  std::size_t position = 0;
  void operator()()
  {
    auto& f = inputs.file.file.bytes;

    if(position < f.size())
    {
      auto end = f.find_first_of(",\n", position);
      if(end != std::string_view::npos)
      {
        outputs.element.value = f.substr(position, end - position);
        position = end + 1;
      }
    }
  }
};
}
