#pragma once
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>
#include <boost/variant2.hpp>
#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>
#include <ossia/detail/flat_map.hpp>
#include <ossia/detail/flat_set.hpp>
#include <ossia/network/value/value.hpp>
namespace co
{

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

struct zone_display
{
  static constexpr double width() { return 100.; }
  static constexpr double height() { return 100.; }

  void paint(auto ctx)
  {
    ctx.set_fill_color({255, 255, 255, 255});
    for(const auto& zone : zones)
    {
      ctx.begin_path();
      const double* pos_xy = reinterpret_cast<const double*>(zone.data());
      ctx.draw_polygon(pos_xy, zone.size());
      ctx.fill();
    }

    ctx.set_stroke_color({255, 255, 255, 255});
    ctx.begin_path();
    ctx.draw_line(0, lat * height(), width(), lat * height());
    ctx.draw_line(lon * width(), 0, lon * width(), height());
    ctx.stroke();
  }

  std::function<void()> update;

  float lat{};
  float lon{};
  std::vector<std::vector<geom_point>> zones;
};

struct GeoZones
{
  halp_meta(name, "Geo Zones")
  halp_meta(category, "Spatial")
  halp_meta(c_name, "geozones")
  halp_meta(author, "Jean-Michaël Celerier, Brice Ammar-Khodja")
  halp_meta(uuid, "b5690418-5832-4038-9549-5cc69b77008c")

  struct ins
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

  geom_point m_bounding0, m_bounding1;
  ossia::flat_set<std::string> m_attributes;
  std::vector<zone> m_zones;
  output m_outputs;

  struct pos_message
  {
    halp_flag(relocatable);
    float lat;
    float lon;
  };
  struct shape_message
  {
    halp_flag(relocatable);
    geom_point b0;
    geom_point b1;
    std::vector<std::vector<geom_point>> z;
  };

  struct ui_message
  {
    halp_flag(relocatable);
    boost::variant2::variant<pos_message, shape_message> msg;
  };

  std::function<void(ui_message)> send_message;

  struct ui
  {
    halp_meta(name, "Main")
    halp_meta(layout, halp::layouts::vbox)
    halp_meta(width, 100)
    halp_meta(height, 100)

    halp::item<&ins::zones> z;
    halp::item<&ins::latitude> lat;
    halp::item<&ins::longitude> lon;
    halp::item<&ins::normalize> nrm;
    halp::item<&ins::blur> bl;
    halp::custom_actions_item<zone_display> anim;

    struct bus
    {
      void init(ui& ui) { }

      // 4. Receive a message on the UI thread from the processing thread
      static void process(ui& self, pos_message&& msg)
      {
        self.anim.lat = msg.lat;
        self.anim.lon = msg.lon;
        self.anim.update();
      }

      static void process(ui& self, shape_message&& msg)
      {
        self.anim.zones = msg.z;

        auto& b0 = msg.b0;
        auto& b1 = msg.b1;

        for(auto& zone : self.anim.zones)
        {
          for(geom_point& pos : zone)
          {
            pos.x(100. * (pos.x() - b0.x()) / (b1.x() - b0.x()));
            pos.y(100. * (pos.y() - b0.y()) / (b1.y() - b0.y()));
          }
        }
        self.anim.update();
      }

      static void process_message(ui& self, ui_message&& msg)
      {
        boost::variant2::visit(
            [&self](auto&& msg) { process(self, std::move(msg)); }, std::move(msg.msg));
      }
    };
  };
};
}
