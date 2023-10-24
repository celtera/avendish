#include <inja/inja.hpp>
#include <nlohmann/json.hpp>

static std::string match_type(std::string_view tp)
{
  if(tp == "float" || tp == "double")
    return "float";
  if(tp == "int" || tp == "long" || tp == "long long")
    return "long";
  if(tp == "const char*" || tp == "std::string" || tp == "std::string_view")
    return "symbol";
  if(tp == "char")
    return "char";
  if(tp == "bool")
    return "boolean";
  return "atom";
}

int main(int argc, char** argv)
{
  using namespace inja;
  Environment env;

  env.set_trim_blocks(true);
  env.set_lstrip_blocks(true);

  env.add_callback("tag", [](inja::Arguments& arg) -> bool {
    const nlohmann::json& port = *arg.at(0);
    auto tags = port.find("tags");
    if(tags == port.end())
      return false;

    const auto& str = arg.at(1)->get<std::string>();
    return tags->contains(str);
  });

  env.add_callback("port_name", [](inja::Arguments& arg) -> std::string {
    const nlohmann::json& a = *arg.at(0);
    if(auto it = a.find("name"); it != a.cend())
      return it->get<std::string>();
    return "<unnamed port>";
  });

  env.add_callback("arg_type", [](inja::Arguments& arg) -> std::string {
    const nlohmann::json& a = *arg.at(0);
    return match_type(a.get<std::string>());
    ;
  });
  env.add_callback("port_type", [](inja::Arguments& arg) -> std::string {
    // signal
    // signal/float
    // symbol
    // float64
    // long
    // signal/integer
    // char
    // message
    // list
    const nlohmann::json& a = *arg.at(0);
    if(auto p_it = a.find("parameter"); p_it != a.end())
    {
      if(auto v_it = p_it->find("value_type"); v_it != p_it->end())
      {
        auto tp = v_it->get<std::string>();
        return match_type(tp);
      }
    }
    else if(auto p_it = a.find("audio"); p_it != a.end())
    {
      return "signal";
    }
    return "atom"; // any?
  });

  env.add_callback("make_digest", [](inja::Arguments& arg) -> std::string {
    const nlohmann::json* a = arg.at(0);
    if(!a)
      return "";
    auto& o = *a;
    if(auto it = o.find("digest"); it != o.end())
      return it->get<std::string>();
    if(auto it = o.find("short_description"); it != o.end())
      return it->get<std::string>();
    if(auto it = o.find("description"); it != o.end())
      return it->get<std::string>();
    return "";
  });
  env.add_callback("make_description", [](inja::Arguments& arg) -> std::string {
    const nlohmann::json* a = arg.at(0);
    if(!a)
      return "";
    auto& o = *a;
    if(auto it = o.find("long_description"); it != o.end())
      return it->get<std::string>();
    if(auto it = o.find("description"); it != o.end())
      return it->get<std::string>();
    if(auto it = o.find("short_description"); it != o.end())
      return it->get<std::string>();
    if(auto it = o.find("digest"); it != o.end())
      return it->get<std::string>();
    return "";
  });


  if(argc == 3)
  {
    std::cout << env.render_file_with_json_file(argv[1], argv[2])
              << std::endl;
  }
  else
    if(argc == 4)
  {
    env.write_with_json_file(argv[1], argv[2], argv[3]);
  }
}
