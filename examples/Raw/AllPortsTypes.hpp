#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <string>
#include <array>
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <list>
#include <deque>
#include <unordered_set>
#include <optional>
#include <bitset>
#include <utility>
#include <tuple>
#include <variant>
#include <boost/container/vector.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/container/static_vector.hpp>

namespace examples
{
enum Foo { A, B, C };
struct Aggregate {
  int sub1;
  std::string sub2;
  struct Sub {
    float sub3;
    std::vector<float> sub4;
  };

  friend bool operator==(const Aggregate& lhs, const Aggregate& rhs) noexcept = default;
  friend bool operator<(const Aggregate& lhs, const Aggregate& rhs) noexcept { return lhs.sub1 < rhs.sub1; }
};
}
namespace std
{
template<>
struct hash <examples::Aggregate>
{
   std::size_t operator()(const examples::Aggregate& a) const noexcept
   {
     return a.sub1;
   }
};
}
namespace examples
{
struct AllPortsTypes
{
  static consteval auto name() { return "AllPortsTypes"; }
  static consteval auto c_name() { return "avnd_all_ports_types"; }
  static consteval auto uuid() { return "7713f267-4ced-4bf0-87d6-f56e368c2be8"; }

  // clang-format off
  struct inputs
  {
#if 1
    struct { static constexpr auto name() { return "float"; } float value; } p_float;
    struct { static constexpr auto name() { return "double"; } double value; } p_double;
    struct { static constexpr auto name() { return "int"; } int value; } p_int;
    struct { static constexpr auto name() { return "string"; } std::string value; } p_string;
    struct { static constexpr auto name() { return "enum"; } Foo value; } p_enum;
    struct { static constexpr auto name() { return "agg"; } Aggregate value; } p_agg;
    struct { static constexpr auto name() { return "bitset"; } std::bitset<53> value; } p_bitset;
    struct { static constexpr auto name() { return "pair_int_int"; } std::pair<int, int> value; } p_pair_int_int;
    struct { static constexpr auto name() { return "pair_float_string"; } std::pair<float, std::string> value; } p_pair_float_string;
    struct { static constexpr auto name() { return "tuple_int_int"; } std::tuple<int, int> value; } p_tuple_int_int;
    struct { static constexpr auto name() { return "tuple_float_string"; } std::tuple<float, std::string> value; } p_tuple_float_string;
    struct { static constexpr auto name() { return "variant"; } std::variant<int, float, std::string> value; } p_variant;

    struct { static constexpr auto name() { return "optional_float"; } std::optional<float>value; } p_optional_float;
    struct { static constexpr auto name() { return "optional_double"; } std::optional<double>value; } p_optional_double;
    struct { static constexpr auto name() { return "optional_int"; } std::optional<int>value; } p_optional_int;
    struct { static constexpr auto name() { return "optional_string"; } std::optional<std::string> value; } p_optional_string;
    struct { static constexpr auto name() { return "optional_enum"; } std::optional<Foo> value; } p_optional_enum;
    struct { static constexpr auto name() { return "optional_agg"; } std::optional<Aggregate> value; } p_optional_agg;

    struct { static constexpr auto name() { return "vector_float"; } std::vector<float>value; } p_vector_float;
    struct { static constexpr auto name() { return "vector_double"; } std::vector<double>value; } p_vector_double;
    struct { static constexpr auto name() { return "vector_int"; } std::vector<int>value; } p_vector_int;
    struct { static constexpr auto name() { return "vector_string"; } std::vector<std::string> value; } p_vector_string;
    struct { static constexpr auto name() { return "vector_enum"; } std::vector<Foo> value; } p_vector_enum;
    struct { static constexpr auto name() { return "vector_agg"; } std::vector<Aggregate> value; } p_vector_agg;

    struct { static constexpr auto name() { return "list_float"; } std::list<float>value; } p_list_float;
    struct { static constexpr auto name() { return "list_double"; } std::list<double>value; } p_list_double;
    struct { static constexpr auto name() { return "list_int"; } std::list<int>value; } p_list_int;
    struct { static constexpr auto name() { return "list_string"; } std::list<std::string> value; } p_list_string;
    struct { static constexpr auto name() { return "list_enum"; } std::list<Foo> value; } p_list_enum;
    struct { static constexpr auto name() { return "list_agg"; } std::list<Aggregate> value; } p_list_agg;

    struct { static constexpr auto name() { return "deque_float"; } std::deque<float>value; } p_deque_float;
    struct { static constexpr auto name() { return "deque_double"; } std::deque<double>value; } p_deque_double;
    struct { static constexpr auto name() { return "deque_int"; } std::deque<int>value; } p_deque_int;
    struct { static constexpr auto name() { return "deque_string"; } std::deque<std::string> value; } p_deque_string;
    struct { static constexpr auto name() { return "deque_enum"; } std::deque<Foo> value; } p_deque_enum;
    struct { static constexpr auto name() { return "deque_agg"; } std::deque<Aggregate> value; } p_deque_agg;

    struct { static constexpr auto name() { return "bvector_float"; } boost::container::vector<float>value; } p_bvector_float;
    struct { static constexpr auto name() { return "bvector_double"; } boost::container::vector<double>value; } p_bvector_double;
    struct { static constexpr auto name() { return "bvector_int"; } boost::container::vector<int>value; } p_bvector_int;
    struct { static constexpr auto name() { return "bvector_string"; } boost::container::vector<std::string> value; } p_bvector_string;
    struct { static constexpr auto name() { return "bvector_enum"; } boost::container::vector<Foo> value; } p_bvector_enum;
    struct { static constexpr auto name() { return "bvector_agg"; } boost::container::vector<Aggregate> value; } p_bvector_agg;

    struct { static constexpr auto name() { return "array_float"; } std::array<float, 5 >value; } p_array_float;
    struct { static constexpr auto name() { return "array_double"; } std::array<double, 5 >value; } p_array_double;
    struct { static constexpr auto name() { return "array_int"; } std::array<int, 5 >value; } p_array_int;
    struct { static constexpr auto name() { return "array_string"; } std::array<std::string, 5> value; } p_array_string;
    struct { static constexpr auto name() { return "array_enum"; } std::array<Foo, 5> value; } p_array_enum;
    struct { static constexpr auto name() { return "array_agg"; } std::array<Aggregate, 5> value; } p_array_agg;

    struct { static constexpr auto name() { return "smallvec_float"; } boost::container::small_vector<float, 5 >value; } p_smallvec_float;
    struct { static constexpr auto name() { return "smallvec_double"; } boost::container::small_vector<double, 5 >value; } p_smallvec_double;
    struct { static constexpr auto name() { return "smallvec_int"; } boost::container::small_vector<int, 5 >value; } p_smallvec_int;
    struct { static constexpr auto name() { return "smallvec_string"; } boost::container::small_vector<std::string, 5> value; } p_smallvec_string;
    struct { static constexpr auto name() { return "smallvec_enum"; } boost::container::small_vector<Foo, 5> value; } p_smallvec_enum;
    struct { static constexpr auto name() { return "smallvec_agg"; } boost::container::small_vector<Aggregate, 5> value; } p_smallvec_agg;

    struct { static constexpr auto name() { return "staticvec_float"; } boost::container::static_vector<float, 5 >value; } p_staticvec_float;
    struct { static constexpr auto name() { return "staticvec_double"; } boost::container::static_vector<double, 5 >value; } p_staticvec_double;
    struct { static constexpr auto name() { return "staticvec_int"; } boost::container::static_vector<int, 5 >value; } p_staticvec_int;
    struct { static constexpr auto name() { return "staticvec_string"; } boost::container::static_vector<std::string, 5> value; } p_staticvec_string;
    struct { static constexpr auto name() { return "staticvec_enum"; } boost::container::static_vector<Foo, 5> value; } p_staticvec_enum;
    struct { static constexpr auto name() { return "staticvec_agg"; } boost::container::static_vector<Aggregate, 5> value; } p_staticvec_agg;

    struct { static constexpr auto name() { return "set_float"; } std::set<float>value; } p_set_float;
    struct { static constexpr auto name() { return "set_double"; } std::set<double>value; } p_set_double;
    struct { static constexpr auto name() { return "set_int"; } std::set<int>value; } p_set_int;
    struct { static constexpr auto name() { return "set_string"; } std::set<std::string> value; } p_set_string;
    struct { static constexpr auto name() { return "set_enum"; } std::set<Foo> value; } p_set_enum;
    struct { static constexpr auto name() { return "set_agg"; } std::set<Aggregate> value; } p_set_agg;

    struct { static constexpr auto name() { return "uset_float"; } std::unordered_set<float>value; } p_uset_float;
    struct { static constexpr auto name() { return "uset_double"; } std::unordered_set<double>value; } p_uset_double;
    struct { static constexpr auto name() { return "uset_int"; } std::unordered_set<int>value; } p_uset_int;
    struct { static constexpr auto name() { return "uset_string"; } std::unordered_set<std::string> value; } p_uset_string;
    struct { static constexpr auto name() { return "uset_enum"; } std::unordered_set<Foo> value; } p_uset_enum;
    struct { static constexpr auto name() { return "uset_agg"; } std::unordered_set<Aggregate> value; } p_uset_agg;

    struct { static constexpr auto name() { return "map_string_float"; } std::map<std::string, float>value; } p_map_string_float;
    struct { static constexpr auto name() { return "map_string_double"; } std::map<std::string, double>value; } p_map_string_double;
    struct { static constexpr auto name() { return "map_string_int"; } std::map<std::string, int>value; } p_map_string_int;
    struct { static constexpr auto name() { return "map_string_string"; } std::map<std::string, std::string> value; } p_map_string_string;
    struct { static constexpr auto name() { return "map_string_enum"; } std::map<std::string, Foo> value; } p_map_string_enum;
    struct { static constexpr auto name() { return "map_string_agg"; } std::map<std::string, Aggregate> value; } p_map_string_agg;

    struct { static constexpr auto name() { return "map_int_float"; } std::map<int, float>value; } p_map_int_float;
    struct { static constexpr auto name() { return "map_int_double"; } std::map<int, double>value; } p_map_int_double;
    struct { static constexpr auto name() { return "map_int_int"; } std::map<int, int>value; } p_map_int_int;
    struct { static constexpr auto name() { return "map_int_string"; } std::map<int, std::string> value; } p_map_int_string;
    struct { static constexpr auto name() { return "map_int_enum"; } std::map<int, Foo> value; } p_map_int_enum;
    struct { static constexpr auto name() { return "map_int_agg"; } std::map<int, Aggregate> value; } p_map_int_agg;

    struct { static constexpr auto name() { return "umap_string_float"; } std::unordered_map<std::string, float>value; } p_umap_string_float;
    struct { static constexpr auto name() { return "umap_string_double"; } std::unordered_map<std::string, double>value; } p_umap_string_double;
    struct { static constexpr auto name() { return "umap_string_int"; } std::unordered_map<std::string, int>value; } p_umap_string_int;
    struct { static constexpr auto name() { return "umap_string_string"; } std::unordered_map<std::string, std::string> value; } p_umap_string_string;
    struct { static constexpr auto name() { return "umap_string_enum"; } std::unordered_map<std::string, Foo> value; } p_umap_string_enum;
    struct { static constexpr auto name() { return "umap_string_agg"; } std::unordered_map<std::string, Aggregate> value; } p_umap_string_agg;

    struct { static constexpr auto name() { return "umap_int_float"; } std::unordered_map<int, float>value; } p_umap_int_float;
    struct { static constexpr auto name() { return "umap_int_double"; } std::unordered_map<int, double>value; } p_umap_int_double;
    struct { static constexpr auto name() { return "umap_int_int"; } std::unordered_map<int, int>value; } p_umap_int_int;
    struct { static constexpr auto name() { return "umap_int_string"; } std::unordered_map<int, std::string> value; } p_umap_int_string;
    struct { static constexpr auto name() { return "umap_int_enum"; } std::unordered_map<int, Foo> value; } p_umap_int_enum;
    struct { static constexpr auto name() { return "umap_int_agg"; } std::unordered_map<int, Aggregate> value; } p_umap_int_agg;
    #endif
    
#if 0
    struct {enum { class_attribute }; static constexpr auto symbol() { return "float"; } static constexpr auto name() { return "float"; } float value; } p_symbol_float;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "double"; } static constexpr auto name() { return "double"; } double value; } p_symbol_double;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "int"; } static constexpr auto name() { return "int"; } int value; } p_symbol_int;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "string"; } static constexpr auto name() { return "string"; } std::string value; } p_symbol_string;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "enum"; } static constexpr auto name() { return "enum"; } Foo value; } p_symbol_enum;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "agg"; } static constexpr auto name() { return "agg"; } Aggregate value; } p_symbol_agg;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "bitset"; } static constexpr auto name() { return "bitset"; } std::bitset<53> value; } p_symbol_bitset;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "pair_int_int"; } static constexpr auto name() { return "pair_int_int"; } std::pair<int, int> value; } p_symbol_pair_int_int;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "pair_float_string"; } static constexpr auto name() { return "pair_float_string"; } std::pair<float, std::string> value; } p_symbol_pair_float_string;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "tuple_int_int"; } static constexpr auto name() { return "tuple_int_int"; } std::tuple<int, int> value; } p_symbol_tuple_int_int;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "tuple_float_string"; } static constexpr auto name() { return "tuple_float_string"; } std::tuple<float, std::string> value; } p_symbol_tuple_float_string;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "variant"; } static constexpr auto name() { return "variant"; } std::variant<int, float, std::string> value; } p_symbol_variant;
    
    struct {enum { class_attribute }; static constexpr auto symbol() { return "optional_float"; } static constexpr auto name() { return "optional_float"; } std::optional<float>value; } p_symbol_optional_float;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "optional_double"; } static constexpr auto name() { return "optional_double"; } std::optional<double>value; } p_symbol_optional_double;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "optional_int"; } static constexpr auto name() { return "optional_int"; } std::optional<int>value; } p_symbol_optional_int;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "optional_string"; } static constexpr auto name() { return "optional_string"; } std::optional<std::string> value; } p_symbol_optional_string;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "optional_enum"; } static constexpr auto name() { return "optional_enum"; } std::optional<Foo> value; } p_symbol_optional_enum;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "optional_agg"; } static constexpr auto name() { return "optional_agg"; } std::optional<Aggregate> value; } p_symbol_optional_agg;
    
    struct {enum { class_attribute }; static constexpr auto symbol() { return "vector_float"; } static constexpr auto name() { return "vector_float"; } std::vector<float>value; } p_symbol_vector_float;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "vector_double"; } static constexpr auto name() { return "vector_double"; } std::vector<double>value; } p_symbol_vector_double;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "vector_int"; } static constexpr auto name() { return "vector_int"; } std::vector<int>value; } p_symbol_vector_int;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "vector_string"; } static constexpr auto name() { return "vector_string"; } std::vector<std::string> value; } p_symbol_vector_string;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "vector_enum"; } static constexpr auto name() { return "vector_enum"; } std::vector<Foo> value; } p_symbol_vector_enum;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "vector_agg"; } static constexpr auto name() { return "vector_agg"; } std::vector<Aggregate> value; } p_symbol_vector_agg;
    
    struct {enum { class_attribute }; static constexpr auto symbol() { return "list_float"; } static constexpr auto name() { return "list_float"; } std::list<float>value; } p_symbol_list_float;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "list_double"; } static constexpr auto name() { return "list_double"; } std::list<double>value; } p_symbol_list_double;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "list_int"; } static constexpr auto name() { return "list_int"; } std::list<int>value; } p_symbol_list_int;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "list_string"; } static constexpr auto name() { return "list_string"; } std::list<std::string> value; } p_symbol_list_string;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "list_enum"; } static constexpr auto name() { return "list_enum"; } std::list<Foo> value; } p_symbol_list_enum;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "list_agg"; } static constexpr auto name() { return "list_agg"; } std::list<Aggregate> value; } p_symbol_list_agg;
    
    struct {enum { class_attribute }; static constexpr auto symbol() { return "deque_float"; } static constexpr auto name() { return "deque_float"; } std::deque<float>value; } p_symbol_deque_float;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "deque_double"; } static constexpr auto name() { return "deque_double"; } std::deque<double>value; } p_symbol_deque_double;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "deque_int"; } static constexpr auto name() { return "deque_int"; } std::deque<int>value; } p_symbol_deque_int;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "deque_string"; } static constexpr auto name() { return "deque_string"; } std::deque<std::string> value; } p_symbol_deque_string;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "deque_enum"; } static constexpr auto name() { return "deque_enum"; } std::deque<Foo> value; } p_symbol_deque_enum;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "deque_agg"; } static constexpr auto name() { return "deque_agg"; } std::deque<Aggregate> value; } p_symbol_deque_agg;
    
    struct {enum { class_attribute }; static constexpr auto symbol() { return "bvector_float"; } static constexpr auto name() { return "bvector_float"; } boost::container::vector<float>value; } p_symbol_bvector_float;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "bvector_double"; } static constexpr auto name() { return "bvector_double"; } boost::container::vector<double>value; } p_symbol_bvector_double;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "bvector_int"; } static constexpr auto name() { return "bvector_int"; } boost::container::vector<int>value; } p_symbol_bvector_int;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "bvector_string"; } static constexpr auto name() { return "bvector_string"; } boost::container::vector<std::string> value; } p_symbol_bvector_string;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "bvector_enum"; } static constexpr auto name() { return "bvector_enum"; } boost::container::vector<Foo> value; } p_symbol_bvector_enum;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "bvector_agg"; } static constexpr auto name() { return "bvector_agg"; } boost::container::vector<Aggregate> value; } p_symbol_bvector_agg;
    
    struct {enum { class_attribute }; static constexpr auto symbol() { return "array_float"; } static constexpr auto name() { return "array_float"; } std::array<float, 5 >value; } p_symbol_array_float;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "array_double"; } static constexpr auto name() { return "array_double"; } std::array<double, 5 >value; } p_symbol_array_double;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "array_int"; } static constexpr auto name() { return "array_int"; } std::array<int, 5 >value; } p_symbol_array_int;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "array_string"; } static constexpr auto name() { return "array_string"; } std::array<std::string, 5> value; } p_symbol_array_string;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "array_enum"; } static constexpr auto name() { return "array_enum"; } std::array<Foo, 5> value; } p_symbol_array_enum;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "array_agg"; } static constexpr auto name() { return "array_agg"; } std::array<Aggregate, 5> value; } p_symbol_array_agg;
    
    struct {enum { class_attribute }; static constexpr auto symbol() { return "smallvec_float"; } static constexpr auto name() { return "smallvec_float"; } boost::container::small_vector<float, 5 >value; } p_symbol_smallvec_float;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "smallvec_double"; } static constexpr auto name() { return "smallvec_double"; } boost::container::small_vector<double, 5 >value; } p_symbol_smallvec_double;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "smallvec_int"; } static constexpr auto name() { return "smallvec_int"; } boost::container::small_vector<int, 5 >value; } p_symbol_smallvec_int;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "smallvec_string"; } static constexpr auto name() { return "smallvec_string"; } boost::container::small_vector<std::string, 5> value; } p_symbol_smallvec_string;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "smallvec_enum"; } static constexpr auto name() { return "smallvec_enum"; } boost::container::small_vector<Foo, 5> value; } p_symbol_smallvec_enum;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "smallvec_agg"; } static constexpr auto name() { return "smallvec_agg"; } boost::container::small_vector<Aggregate, 5> value; } p_symbol_smallvec_agg;
    
    struct {enum { class_attribute }; static constexpr auto symbol() { return "staticvec_float"; } static constexpr auto name() { return "staticvec_float"; } boost::container::static_vector<float, 5 >value; } p_symbol_staticvec_float;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "staticvec_double"; } static constexpr auto name() { return "staticvec_double"; } boost::container::static_vector<double, 5 >value; } p_symbol_staticvec_double;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "staticvec_int"; } static constexpr auto name() { return "staticvec_int"; } boost::container::static_vector<int, 5 >value; } p_symbol_staticvec_int;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "staticvec_string"; } static constexpr auto name() { return "staticvec_string"; } boost::container::static_vector<std::string, 5> value; } p_symbol_staticvec_string;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "staticvec_enum"; } static constexpr auto name() { return "staticvec_enum"; } boost::container::static_vector<Foo, 5> value; } p_symbol_staticvec_enum;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "staticvec_agg"; } static constexpr auto name() { return "staticvec_agg"; } boost::container::static_vector<Aggregate, 5> value; } p_symbol_staticvec_agg;
    
    struct {enum { class_attribute }; static constexpr auto symbol() { return "set_float"; } static constexpr auto name() { return "set_float"; } std::set<float>value; } p_symbol_set_float;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "set_double"; } static constexpr auto name() { return "set_double"; } std::set<double>value; } p_symbol_set_double;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "set_int"; } static constexpr auto name() { return "set_int"; } std::set<int>value; } p_symbol_set_int;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "set_string"; } static constexpr auto name() { return "set_string"; } std::set<std::string> value; } p_symbol_set_string;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "set_enum"; } static constexpr auto name() { return "set_enum"; } std::set<Foo> value; } p_symbol_set_enum;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "set_agg"; } static constexpr auto name() { return "set_agg"; } std::set<Aggregate> value; } p_symbol_set_agg;
    
    struct {enum { class_attribute }; static constexpr auto symbol() { return "uset_float"; } static constexpr auto name() { return "uset_float"; } std::unordered_set<float>value; } p_symbol_uset_float;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "uset_double"; } static constexpr auto name() { return "uset_double"; } std::unordered_set<double>value; } p_symbol_uset_double;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "uset_int"; } static constexpr auto name() { return "uset_int"; } std::unordered_set<int>value; } p_symbol_uset_int;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "uset_string"; } static constexpr auto name() { return "uset_string"; } std::unordered_set<std::string> value; } p_symbol_uset_string;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "uset_enum"; } static constexpr auto name() { return "uset_enum"; } std::unordered_set<Foo> value; } p_symbol_uset_enum;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "uset_agg"; } static constexpr auto name() { return "uset_agg"; } std::unordered_set<Aggregate> value; } p_symbol_uset_agg;
    
    struct {enum { class_attribute }; static constexpr auto symbol() { return "map_string_float"; } static constexpr auto name() { return "map_string_float"; } std::map<std::string, float>value; } p_symbol_map_string_float;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "map_string_double"; } static constexpr auto name() { return "map_string_double"; } std::map<std::string, double>value; } p_symbol_map_string_double;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "map_string_int"; } static constexpr auto name() { return "map_string_int"; } std::map<std::string, int>value; } p_symbol_map_string_int;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "map_string_string"; } static constexpr auto name() { return "map_string_string"; } std::map<std::string, std::string> value; } p_symbol_map_string_string;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "map_string_enum"; } static constexpr auto name() { return "map_string_enum"; } std::map<std::string, Foo> value; } p_symbol_map_string_enum;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "map_string_agg"; } static constexpr auto name() { return "map_string_agg"; } std::map<std::string, Aggregate> value; } p_symbol_map_string_agg;
    
    struct {enum { class_attribute }; static constexpr auto symbol() { return "map_int_float"; } static constexpr auto name() { return "map_int_float"; } std::map<int, float>value; } p_symbol_map_int_float;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "map_int_double"; } static constexpr auto name() { return "map_int_double"; } std::map<int, double>value; } p_symbol_map_int_double;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "map_int_int"; } static constexpr auto name() { return "map_int_int"; } std::map<int, int>value; } p_symbol_map_int_int;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "map_int_string"; } static constexpr auto name() { return "map_int_string"; } std::map<int, std::string> value; } p_symbol_map_int_string;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "map_int_enum"; } static constexpr auto name() { return "map_int_enum"; } std::map<int, Foo> value; } p_symbol_map_int_enum;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "map_int_agg"; } static constexpr auto name() { return "map_int_agg"; } std::map<int, Aggregate> value; } p_symbol_map_int_agg;
    
    struct {enum { class_attribute }; static constexpr auto symbol() { return "umap_string_float"; } static constexpr auto name() { return "umap_string_float"; } std::unordered_map<std::string, float>value; } p_symbol_umap_string_float;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "umap_string_double"; } static constexpr auto name() { return "umap_string_double"; } std::unordered_map<std::string, double>value; } p_symbol_umap_string_double;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "umap_string_int"; } static constexpr auto name() { return "umap_string_int"; } std::unordered_map<std::string, int>value; } p_symbol_umap_string_int;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "umap_string_string"; } static constexpr auto name() { return "umap_string_string"; } std::unordered_map<std::string, std::string> value; } p_symbol_umap_string_string;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "umap_string_enum"; } static constexpr auto name() { return "umap_string_enum"; } std::unordered_map<std::string, Foo> value; } p_symbol_umap_string_enum;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "umap_string_agg"; } static constexpr auto name() { return "umap_string_agg"; } std::unordered_map<std::string, Aggregate> value; } p_symbol_umap_string_agg;
    
    struct {enum { class_attribute }; static constexpr auto symbol() { return "umap_int_float"; } static constexpr auto name() { return "umap_int_float"; } std::unordered_map<int, float>value; } p_symbol_umap_int_float;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "umap_int_double"; } static constexpr auto name() { return "umap_int_double"; } std::unordered_map<int, double>value; } p_symbol_umap_int_double;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "umap_int_int"; } static constexpr auto name() { return "umap_int_int"; } std::unordered_map<int, int>value; } p_symbol_umap_int_int;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "umap_int_string"; } static constexpr auto name() { return "umap_int_string"; } std::unordered_map<int, std::string> value; } p_symbol_umap_int_string;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "umap_int_enum"; } static constexpr auto name() { return "umap_int_enum"; } std::unordered_map<int, Foo> value; } p_symbol_umap_int_enum;
    struct {enum { class_attribute }; static constexpr auto symbol() { return "umap_int_agg"; } static constexpr auto name() { return "umap_int_agg"; } std::unordered_map<int, Aggregate> value; } p_symbol_umap_int_agg;
#endif
    // clang-format on
  } inputs;

  struct inputs outputs;

  void operator()() { }
};
}
