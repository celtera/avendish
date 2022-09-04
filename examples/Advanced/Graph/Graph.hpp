#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/meta.hpp>

namespace grph
{

template<typename T = float>
struct ugen
{
  struct
  {
  } inputs;
  struct
  {
    halp::val_port<"Out", T> o;
  } outputs;
};
template<typename T = float>
struct unode
{
  struct
  {
    halp::val_port<"In", T> i;
  } inputs;
  struct
  {
    halp::val_port<"Out", T> o;
  } outputs;
};

template<typename T = float>
struct binode
{
  struct
  {
    halp::val_port<"A", T> a;
    halp::val_port<"B", T> b;
  } inputs;
  struct
  {
    halp::val_port<"Out", T> o;
  } outputs;
};
// either an address or a constant
using relation_value = std::variant<std::string, ossia::value>;
struct relation
{
    struct
    {
      halp::val_port<"A", relation_value> a;
      halp::val_port<"B", relation_value> b;
    } inputs;
    struct
    {
      halp::val_port<"Out", relation_value> o;
    } outputs;
};

struct expression_graph
{
    // Relation is a boolean-yielding expression
    struct relation
    {

    };

    // Expression is a boolean tree of relations
    struct constant : ugen<float> { void operator()() { outputs.o = 123; }};
    struct boolean_not : unode<bool>{ void operator()() { outputs.o = !inputs.i; } };
    struct boolean_and : binode<bool>{ void operator()() { outputs.o = inputs.a && inputs.b; } };
    struct boolean_or : binode<bool>{ void operator()() { outputs.o = inputs.a || inputs.b; } };
};

struct my_graph
{
  struct constant : ugen<> { void operator()() { outputs.o = 123; }};
  struct square_root : unode<> { void operator()() { outputs.o = sqrt(inputs.i); } };
  struct add : binode<> { void operator()() { outputs.o = inputs.a + inputs.b; } };
  struct mul : binode<> { void operator()() { outputs.o = inputs.a * inputs.b; } };

  // We need to: list the potential node types
  // Those can be avendish nodes huehue

  using node = std::variant<constant, square_root, add, mul>;

  struct edge {
    int source_port;
    int sink_port;
    int source_node;
    int sink_node;
  };

  struct
  {
    // guaranteed to be in topological order
    std::vector<node> nodes;

    // guaranteed to be sorted relative to the topo order
    std::vector<edge> edges;
  } graph;

  static constexpr uint32_t map(int a, int b) { return ((uint32_t)a) << 16 | (uint32_t)b; }
  void read_inputs(node& src, node& sink, edge e)
  {
      std::visit([=] <typename Source, typename Sink> (Source& a, Sink& b) {
          using outs = avnd::output_introspection<Source>;
          using ins = avnd::input_introspection<Sink>;
          switch(map(e.source_port, e.sink_port))
          {
#define MAP(IA, IB)       \
          case map(IA, IB): \
              if constexpr(outs::size > IA && ins::size > IB) \
                  avnd::pfr::get<IB>(b.inputs).value = avnd::pfr::get<IA>(a.outputs).value; \
              break;
          MAP(0, 0);
          MAP(0, 1);
          MAP(0, 2);
          MAP(0, 3);
          MAP(1, 0);
          MAP(1, 1);
          MAP(1, 2);
          MAP(1, 3);
          MAP(2, 0);
          MAP(2, 1);
          MAP(2, 2);
          MAP(2, 3);
          MAP(3, 0);
          MAP(3, 1);
          MAP(3, 2);
          MAP(3, 3);
          }
      }, src, sink);
  }

  void read_inputs(int sink_idx)
  {
      node& sink = graph.nodes[sink_idx];
      for(auto edge : graph.edges)
      {
          if(edge.sink_node == sink_idx)
          {
              node& source = graph.nodes[edge.source_node];
              read_inputs(source, sink, edge);
          }
      }
  }

  float process() {
      for(int i = 0; i < graph.nodes.size(); i++)
      {
          // Copy inputs
          read_inputs(i);

          // Process
          std::visit([=] (auto& a) { a(); }, graph.nodes[i]);

      }
      return 0.f;
  }
};

struct Graph
{
public:
  halp_meta(name, "Graph")
  halp_meta(c_name, "avnd_graph")
  halp_meta(category, "Script")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(uuid, "42050fb9-2983-4982-afd1-f94bdf0f2ba1")

  my_graph graph;
  struct
  {
  } inputs;

  struct
  {
    struct
    {
      float value;
    } main;
  } outputs;

  void operator()() { outputs.main.value = graph.process(); }
};
}
