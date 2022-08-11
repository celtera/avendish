#pragma once

#include <DspFilters/Cascade.h>
#include <DspFilters/Filter.h>
#include <avnd/concepts/painter.hpp>
#include <cmath>
#include <fmt/format.h>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace utilities
{
std::string to_string(float val, int precision)
{
  return fmt::format("{:.{}f}", val, precision);
}
}

/*  The following object handles the computation of the values to plot the Parametric EQ (Bode) diagram.
    It needs two values to properly run:
        - The number of plot values to compute (N) which is passed as a template argument;
        - The sample rate (sampleRate), needed to normalize frequencies when computing a filter's response to a certain frequency, which should be set before use of the object.
*/
template <int N, int bandNb>
struct EQDiagramCalculator
{
  EQDiagramCalculator()
      : sampleRate{44100.f}
  {
    auto eps = (range::max - range::min) / (N - 1);
    for(auto i = 0; i < N; ++i)
      frequencies.push_back(range::min + eps * i);
    filters.resize(bandNb);
  }

  struct range
  {
    static constexpr int plotAmount = N;
    static constexpr double min = 20.f;
    static constexpr double max = 20000.f;
  };

  float sampleRate;

  std::vector<float> frequencies{}; //abscissa values
  std::vector<float> responses{};   //ordinate values

  using FilterHolder = Dsp::Filter;
  std::vector<const FilterHolder*> filters{};

  void addFilter(int band, const FilterHolder* filter)
  {
    if(filters[band] != filter)
    {
      filters[band] = filter;
    }
  }

  bool removeFilter(int band)
  {
    return std::exchange(filters[band], nullptr) != nullptr;
  }
  /*
        We consider each filter to be connected in series. In this case, the resulting transfer function is the 
        multiplication of each filter's transfer function and the gain response in the sum of the norms of 
        each filter's complex responses.
    */
  void computeDiagram()
  {
    // we erase previous values contained in responses as there might be new filters added / removed filters since last computation
    responses.clear();
    for(auto& w : frequencies)
    {
      float res{};
      for(auto& f : filters)
      {
        if(f)
        {
          auto resp_n = std::norm(f->response(w / sampleRate));
          if(resp_n > 0)
            res += std::log10(resp_n);
        }
      }
      responses.push_back(20. * res);
    }
  }
};

/* 
    UI Object to display values computed by an EQDiagramCalculator. When initialized with halp::custom_item_base<EQDiagram>, xInput, yInput and valuesNb should be specified
    to match an EQDiagramCalculator's frequencies, responses and plotAmount value.
*/
template <int w, int h, int N>
struct EQDiagram
{
  EQDiagram()
      : valuesNb(N)
  {
    //maybe should fill with coords to draw flatline at start
    abscissa.resize(N);
    ordinate.resize(N);
  }
  static constexpr double width() { return w; }
  static constexpr double height() { return h; }
  static constexpr double layout()
  {
    enum
    {
      custom
    } d{};
    return d;
  }

  // double x = 0.0;
  // double y = 0.0;

  int valuesNb;

  // the followings will hold the coords of plotted points
  std::vector<float> abscissa{};
  std::vector<float> ordinate{};

  using axis_ticks = std::pair<float, std::string_view>;
  static constexpr axis_ticks xticks[]{{20, "20"},    {500, "500"}, {1000, "1k"},
                                       {2000, "2k"},  {5000, "5k"}, {10000, "10k"},
                                       {20000, "20k"}};
  std::vector<std::pair<float, float>> yticks{};

  // basically construct a log scale for x values and compute coords to correctly plot values in the UI
  float xScale(float val)
  {
    auto xMin = std::log10(20);
    auto xMax = std::log10(20000);
    auto xscale = width() / (xMax - xMin);
    auto xdiff = -xscale * xMin;
    return xscale * std::log10(val) + xdiff;
  }

  void update(const float* xInput, const float* yInput)
  {
    //updating plot values
    std::cout << "updating" << std::endl;
    auto yMin = *std::min_element(yInput, yInput + valuesNb - 1);
    auto yMax = *std::max_element(yInput, yInput + valuesNb - 1);
    auto rng = std::abs(yMax - yMin);
    if(rng == 0)
      return;
    auto yscale = height() / rng;
    auto ydiff = -yscale * yMax;
    for(auto i = 0; i < valuesNb; ++i)
    {
      abscissa[i] = xScale(*(xInput + i));
      ordinate[i] = -(yscale * (*(yInput + i)) + ydiff);
    }

    // updating y axis scale labels
    constexpr auto ticksNb = 10;
    auto step = (yMax - yMin) / (ticksNb - 1);
    for(auto i = 0; i < ticksNb; ++i)
    {
      yticks.push_back({yMin + i * step, -(yscale * (yMin + i * step) + ydiff)});
    }

    std::cout << "update done" << std::endl;
  }

  void paint(avnd::painter auto ctx)
  {
    // THE AXES
    // x axis
    ctx.set_stroke_color({.r = 103, .g = 103, .b = 103, .a = 255});
    ctx.set_fill_color({255, 255, 255, 255});
    ctx.begin_path();
    ctx.draw_line(0, height() - 1, width(), height() - 1);
    ctx.draw_line(0, height(), width(), height());
    ctx.fill();
    ctx.stroke();
    for(auto i = 0; i < 7; ++i)
    {
      // scale labels
      auto atX = xScale(xticks[i].first);
      ctx.begin_path();
      ctx.draw_text(atX, height() - 10., xticks[i].second);
      ctx.fill();

      // vertical lines
      ctx.begin_path();
      ctx.draw_line(atX, 0, atX, height());
      ctx.fill();
      ctx.stroke();
    }

    // y axis
    for(auto i = 0; i < yticks.size(); ++i)
    {
      // scale labels
      // somewhat buggy for now -> it seems like the canvas is cleared before we rewrite the ticks on the y-axis...
      // ctx.begin_path();
      // std::string grad = utilities::to_string(yticks[i].first, 2);
      // ctx.draw_text(0, yticks[i].second, grad);
      // ctx.fill();

      // horizontal lines
      ctx.begin_path();
      ctx.draw_line(0, yticks[i].second, width(), yticks[i].second);
      ctx.fill();
      ctx.stroke();
    }

    // THE GRAPH
    ctx.set_stroke_color(
        {.r = 118, .g = 37, .b = 190, .a = 255}); // gotta love some purple <3
    ctx.set_fill_color({255, 255, 255, 255});

    for(auto i = 1; i < valuesNb; ++i)
    {
      ctx.begin_path();
      ctx.draw_line(abscissa[i - 1], ordinate[i - 1], abscissa[i], ordinate[i]);
      ctx.fill();
      ctx.stroke();
    }

    ctx.update();
  }
};