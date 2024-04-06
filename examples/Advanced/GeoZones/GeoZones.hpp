#pragma once
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <ossia/detail/flat_set.hpp>

namespace co
{
struct GeoZones
{
  halp_meta(name, "Geo Zones")
  halp_meta(category, "Spatial")
  halp_meta(c_name, "geozones")
  halp_meta(author, "Jean-Michaël Celerier, Brice Ammar-Khodja")
  halp_meta(uuid, "b5690418-5832-4038-9549-5cc69b77008c")

  struct
  {
    struct : halp::lineedit<"Program", "">
    {
      halp_meta(language, "json")
      void update(GeoZones& self) { self.loadZones(); }
    } zones;

    halp::hslider_f32<"Latitude", halp::range{0, 1, 0.5}> latitude;
    halp::hslider_f32<"Longitude", halp::range{0, 1, 0.5}> longitude;
    halp::toggle<"Normalize"> normalize;
    halp::knob_f32<"Blur", halp::range{0, 1, 0.5}> blur;
  } inputs;

  struct
  {
    halp::val_port<"Out", ossia::value> zones;
  } outputs;

  void operator()();
  void loadZones();

  using geom_point = boost::geometry::model::d2::point_xy<double>;
  using geom_polygon = boost::geometry::model::polygon<geom_point>;
  struct zone
  {
    std::vector<geom_point> positions;
    geom_polygon polygon;
    geom_point center{};
    boost::container::flat_map<std::string, float> attributes;
  };

  struct result
  {
    float to_side;
    float to_center;
    float influence;
  };

  struct output
  {
    halp_field_names(inside, closest, per_zone, attributes);
    result inside;
    result closest;
    std::vector<result> per_zone;
    boost::container::flat_map<std::string, float> attributes;
  };

  geom_point m_bounding0, m_bounding1;
  ossia::flat_set<std::string> m_attributes;
  std::vector<zone> m_zones;
  output m_outputs;
};
}
