#include "GeoZones.hpp"

#include <avnd/binding/ossia/from_value.hpp>
#include <avnd/binding/ossia/to_value.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/multi_polygon.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>
#include <ossia/detail/fmt.hpp>
#include <ossia/detail/json.hpp>
namespace co
{
static result pip(const zone& z, double latitude, double longitude, double blur)
{
  auto pt = geom_point{latitude, longitude};
  bool within = boost::geometry::within(pt, z.polygon);
  double distance_side = std::abs(boost::geometry::distance(pt, z.polygon));
  double distance_center = std::abs(boost::geometry::distance(pt, z.center));
  double influence
      = within ? 1.f
               : std::clamp(
                   std::pow(1. / (1.0 + distance_center), 1e5 * (1. - blur)), 0., 1.);

  return result{
      .to_side = (float)distance_side,
      .to_center = (float)distance_center,
      .influence = (float)influence};
}

void GeoZones::operator()()
{
  m_outputs.inside = {};
  m_outputs.closest = {};
  m_outputs.attributes.clear();
  m_outputs.per_zone.clear();

  if(m_zones.empty())
  {
    return;
  }

  auto in_fence = -1;
  auto closest_fence = -1;

  double min_distance = 1e100;
  m_outputs.per_zone.clear();
  m_outputs.per_zone.resize(m_zones.size());
  const auto [lat, lon] = [&]() -> std::pair<double, double> {
    if(inputs.normalize)
    {
      const double w = m_bounding1.x() - m_bounding0.x();
      const double h = m_bounding1.y() - m_bounding0.y();
      const double lat = inputs.latitude * w + m_bounding0.x();
      const double lon = inputs.longitude * h + m_bounding0.y();
      return {lat, lon};
    }
    else
    {
      return {inputs.latitude, inputs.longitude};
    }
  }();
  for(int i = 0, N = m_zones.size(); i < N; i++)
  {
    auto& z = m_outputs.per_zone[i];
    z = pip(m_zones[i], lat, lon, inputs.blur);
    if(z.influence >= 1.)
    {
      in_fence = i;
    }

    if(z.to_center < min_distance)
    {
      closest_fence = i;
      min_distance = z.to_center;
    }
  }

  if(closest_fence >= 0)
    m_outputs.closest = m_outputs.per_zone[closest_fence];
  else
    m_outputs.closest = {};
  if(in_fence >= 0)
    m_outputs.inside = m_outputs.per_zone[in_fence];
  else
    m_outputs.inside = {};

  for(const auto& k : m_attributes)
    m_outputs.attributes[k] = 0.;
  if(in_fence != -1)
  {
    for(const auto& [k, v] : m_zones[in_fence].attributes)
      m_outputs.attributes[k] = v;
  }
  else
  {
    for(int i = 0, N = m_zones.size(); i < N; i++)
    {
      for(const auto& [k, v] : m_zones[i].attributes)
        m_outputs.attributes[k] += v * m_outputs.per_zone[i].influence;
    }
    //     for(const auto& [k, v] : m_outputs.attributes)
    //     {
    //       m_outputs.attributes[k] += v * m_outputs.per_zone[i].influence;
    //     }
  }

  outputs.zones.value = oscr::to_ossia_value(m_outputs);
  send_message({pos_message{inputs.latitude, inputs.longitude}});
}

void GeoZones::loadZones()
{
  m_attributes.clear();
  m_bounding0 = {};
  m_bounding1 = {};
  m_zones.clear();

  try
  {
    rapidjson::Document doc;
    doc.Parse(inputs.zones.value);
    if(doc.HasParseError())
      return;
    if(!doc.IsArray())
      return;

    boost::geometry::model::multi_polygon<geom_polygon> polys;
    for(auto& obj : doc.GetArray())
    {
      zone z;

      if(!obj.IsObject())
        continue;

      auto positions_it = obj.FindMember("polygon");
      if(positions_it == obj.MemberEnd())
        continue;
      if(!positions_it->value.IsArray())
        continue;

      for(const auto& pos : positions_it->value.GetArray())
      {
        if(pos[0].IsNumber() && pos[1].IsNumber())
        {
          z.positions.emplace_back(pos[0].GetDouble(), pos[1].GetDouble());
        }
      }
      boost::geometry::assign_points(z.polygon, z.positions);
      polys.push_back(z.polygon);
      z.center = boost::geometry::return_centroid<geom_point>(z.polygon);

      for(const auto& mem : obj.GetObject())
      {
        if(mem.value.GetType() == rapidjson::kNumberType)
        {
          m_attributes.insert(mem.name.GetString());

          z.attributes[mem.name.GetString()] = mem.value.GetDouble();
        }
        else if(mem.value.GetType() == rapidjson::kStringType)
        {
          auto enum_str = mem.value.GetString();
          std::vector<std::string> result;
          boost::split(result, enum_str, boost::is_any_of("|"));

          for(auto& an_enum : result)
          {
            auto str = fmt::format("{}_{}", mem.name.GetString(), an_enum);
            m_attributes.insert(str);

            z.attributes[str] = 1.;
          }
        }
      }
      m_zones.push_back(std::move(z));
    }

    boost::geometry::model::box<geom_point> box;
    boost::geometry::envelope(polys, box);
    m_bounding0 = box.min_corner();
    m_bounding1 = box.max_corner();

    std::vector<std::vector<geom_point>> ptx;
    ptx.reserve(m_zones.size());
    for(auto& zz : m_zones)
      ptx.push_back(zz.positions);
    send_message({shape_message{m_bounding0, m_bounding1, std::move(ptx)}});
  }
  catch(...)
  {
  }
}
}
