#pragma once

#include "EQDiagram.hpp"
#include "ParametricEqControls.hpp"

#include <DspFilters/Butterworth.h>
#include <DspFilters/ChebyshevI.h>
#include <DspFilters/Filter.h>
#include <halp/custom_widgets.hpp>
#include <halp/layout.hpp>

#include <iostream>
#include <map>
#include <string>
#include <tuple>
#include <vector>

#include <string_view>

namespace examples
{
// ********************************************************************************
//                                  Actual EQ
// ********************************************************************************
struct ParametricEq
{
  static constexpr auto name() { return "Parametric Equalizer"; }
  static constexpr auto c_name() { return "ngry_parameq"; }
  static constexpr auto author() { return "Nicolas Gry"; }
  static constexpr auto category() { return "Demo"; }
  static constexpr auto description()
  {
    return "Parametric Equalizer, using Butterworth equations for all filters except "
           "the bandshelf filter (which is computed with Chebyshev Type I equations).";
  }
  static constexpr auto uuid() { return "ec972d12-edba-4a9c-9736-9176efad17e3"; }

  static constexpr int bandsNb = 8;  // EQ Bands number
  static constexpr int maxOrder = 7; //Filters maximum order
  struct ins
  {
    struct
    {
      static constexpr auto name() { return "Input"; }
      const double** samples;
      int channels;
    } audio;

    EQBand band1, band2, band3, band4, band5, band6, band7, band8;

    // Common filters properties
    filterToggle toggle1, toggle2, toggle3, toggle4, toggle5, toggle6, toggle7, toggle8;
    order<maxOrder> order1, order2, order3, order4, order5, order6, order7, order8;

    // Specific filters properties (not all filters require them)
    cutoffFrequency cutoffFreq1, cutoffFreq2, cutoffFreq3, cutoffFreq4, cutoffFreq5,
        cutoffFreq6, cutoffFreq7, cutoffFreq8;
    centerFrequency centerFreq1, centerFreq2, centerFreq3, centerFreq4, centerFreq5,
        centerFreq6, centerFreq7, centerFreq8;
    gain gain1, gain2, gain3, gain4, gain5, gain6, gain7, gain8;
    frequencyBandWidth freqBandWidth1, freqBandWidth2, freqBandWidth3, freqBandWidth4,
        freqBandWidth5, freqBandWidth6, freqBandWidth7, freqBandWidth8;
    passbandRipple passbandRipple1, passbandRipple2, passbandRipple3, passbandRipple4,
        passbandRipple5, passbandRipple6, passbandRipple7, passbandRipple8;

  } inputs;

  //                                      Available filters
  static constexpr auto chans = 2;
  Dsp::FilterDesign<Dsp::Butterworth::Design::LowPass<maxOrder>, chans>   lowpassFilter  ; // crashes when cutoffFreq = 0 (should never happen)
  Dsp::FilterDesign<Dsp::Butterworth::Design::HighPass<maxOrder>, chans>  highpassFilter ;
  Dsp::FilterDesign<Dsp::Butterworth::Design::BandPass<maxOrder>, chans>  bandpassFilter ;
  Dsp::FilterDesign<Dsp::Butterworth::Design::BandStop<maxOrder>, chans>  bandstopFilter ;
  Dsp::FilterDesign<Dsp::Butterworth::Design::LowShelf<maxOrder>, chans>  lowshelfFilter ;
  Dsp::FilterDesign<Dsp::Butterworth::Design::HighShelf<maxOrder>, chans> highshelfFilter;
  Dsp::FilterDesign<Dsp::ChebyshevI::Design::BandShelf<maxOrder>, chans>  bandshelfFilter; // crashes when gain = 0;
  //Dsp::Filter * lowpassFilter, * highpassFilter, * bandpassFilter, * bandstopFilter, * lowshelfFilter, * highshelfFilter, * bandshelfFilter;

  // EQ Bode diagram calculator
  static constexpr auto                     plotPoints = 5000;
  EQDiagramCalculator<plotPoints, bandsNb>  diagramCalculator{};
  bool                                      toRecompute = false;
  struct processor_to_ui
  {
    std::vector<float> xInput {};
    std::vector<float> yInput {};
  };
  std::function<void(processor_to_ui)> send_message;

  struct
  {
    struct
    {
      static constexpr auto name() { return "Output"; }
      double** samples;
      int channels;
    } audio;
  } outputs;
  struct setup
  {
    float rate{};
  };

  void prepare(setup s)
  {
    diagramCalculator.sampleRate = s.rate;
    inputs.band1.id = 0;
    inputs.band2.id = 1;
    inputs.band3.id = 2;
    inputs.band4.id = 3;
    inputs.band5.id = 4;
    inputs.band6.id = 5;
    inputs.band7.id = 6;
    inputs.band8.id = 7;

    Dsp::Params params; 

    // This feels kinda messy because each filter type has its own parameters order;
    // the two first parameters are always sample rate and filter order;
    // for the other parameters, you need to check the TypeBase[I-IV] class of the desired filter (Butterworth, Chebyshev, etc).
    params[0] = s.rate;   // sample rate
    params[1] = 1;        // filter order
    params[2] = 10.;     // cutoff frequency / center frequency

    lowpassFilter.setParams(params);
    highpassFilter.setParams(params);

    params[3] = .5;     // frequency band width

    bandpassFilter.setParams(params);
    bandstopFilter.setParams(params);

    params[3] = -6.;     // shelf gain

    lowshelfFilter.setParams(params);
    highshelfFilter.setParams(params);

    params[3] = .5;     // frequency band width
    params[4] = -6.;      // shelf gain
    params[5] = .5;       // ripple

    bandshelfFilter.setParams(params);
  }

  void applyBand(
      int N,
      EQBand& bd,
      filterToggle& tog,
      order<maxOrder>& ord,
      cutoffFrequency& cutoffFreq,
      centerFrequency& centerFreq,
      gain& gn,
      frequencyBandWidth& freqBdWidth,
      passbandRipple& ripple)
  {
    
    if (tog.value)
    {
    // Fortunately one would say, here we don't have to remember the parameters order as the DspFilters lib provides ParamID.
    // BUT access with ID is non-constant (see the implementation for yourself) while setParam isConstant. So for efficiency, we WILl be using parameters index rather than id.
      auto& out = outputs.audio;
      Dsp::Params previousParams;
      switch (bd.value)
      {
        case filters::lowpass:
          previousParams = lowpassFilter.getParams();
          lowpassFilter.setParam(1, ord.value);
          lowpassFilter.setParam(2, cutoffFreq.value);
          toRecompute |= previousParams[1] != ord.value || previousParams[2] != cutoffFreq.value;
          lowpassFilter.process(N, out.samples);
          diagramCalculator.addFilter(bd.id, &lowpassFilter);
          break;
        case filters::highpass:
          previousParams = highpassFilter.getParams();
          highpassFilter.setParam(1, ord.value);
          highpassFilter.setParam(2, cutoffFreq.value);
          toRecompute |= previousParams[1] != ord.value || previousParams[2] != cutoffFreq.value;
          highpassFilter.process(N, out.samples);
          diagramCalculator.addFilter(bd.id, &highpassFilter);
          break;
        case filters::bandpass:
          previousParams = bandpassFilter.getParams();
          bandpassFilter.setParam(1, ord.value);
          bandpassFilter.setParam(2, centerFreq.value);
          bandpassFilter.setParam(3, freqBdWidth.value);
          toRecompute |= previousParams[1] != ord.value || previousParams[2] != centerFreq.value || previousParams[3] != freqBdWidth.value;
          bandpassFilter.process(N, out.samples);
          diagramCalculator.addFilter(bd.id, &bandpassFilter);
          break;
        case filters::bandstop:
          previousParams = bandstopFilter.getParams();
          bandstopFilter.setParam(1, ord.value);
          bandstopFilter.setParam(2, centerFreq.value);
          bandstopFilter.setParam(3, freqBdWidth.value);
          toRecompute |= previousParams[1] != ord.value || previousParams[2] != centerFreq.value || previousParams[3] != freqBdWidth.value;
          bandstopFilter.process(N, out.samples);
          diagramCalculator.addFilter(bd.id, &bandstopFilter);
          break;
        case filters::lowshelf:
          previousParams = lowshelfFilter.getParams();
          lowshelfFilter.setParam(1, ord.value);
          lowshelfFilter.setParam(2, cutoffFreq.value);
          lowshelfFilter.setParam(3, gn.value);
          toRecompute |= previousParams[1] != ord.value || previousParams[2] != cutoffFreq.value || previousParams[3] != gn.value;
          lowshelfFilter.process(N, out.samples);
          diagramCalculator.addFilter(bd.id, &lowshelfFilter);
          break;
        case filters::highshelf:
          previousParams = highshelfFilter.getParams();
          highshelfFilter.setParam(1, ord.value);
          highshelfFilter.setParam(2, cutoffFreq.value);
          highshelfFilter.setParam(3, gn.value);
          toRecompute |= previousParams[1] != ord.value || previousParams[2] != cutoffFreq.value || previousParams[3] != gn.value;
          highshelfFilter.process(N, out.samples);
          diagramCalculator.addFilter(bd.id, &highshelfFilter);
          break;
        case filters::bandshelf:
          previousParams = bandshelfFilter.getParams();
          bandshelfFilter.setParam(1, ord.value);
          bandshelfFilter.setParam(2, centerFreq.value);
          bandshelfFilter.setParam(3, freqBdWidth.value);
          bandshelfFilter.setParam(4, gn.value);
          bandshelfFilter.setParam(5, ripple.value);
          toRecompute |= previousParams[1] != ord.value || previousParams[2] != centerFreq.value || previousParams[3] != freqBdWidth.value || previousParams[4] != gn.value || previousParams[5] != ripple.value;
          bandshelfFilter.process(N, out.samples);
          diagramCalculator.addFilter(bd.id, &bandshelfFilter);
          break;
        default:
          break;
      }
    }
    else
    {
      toRecompute |= diagramCalculator.removeFilter(bd.id);
    }
  }

  void operator()(int N)
  {
    auto& in = inputs.audio;
    auto& out = outputs.audio;

    auto& channels = in.channels;

    //  Since DSPFilters functions modify the samples in-place,
    //  we first copy the input into the output and then process the output samples directly.

    for (auto i = 0; i < channels; ++i)
    {
      auto& in_samp = in.samples[i];
      auto& out_samp = out.samples[i];
      for (auto j = 0; j < N; ++j)
      {
        out_samp[j] = in_samp[j];
      }
    }
    std::apply(
        [&](auto&&... args)
        {
          (applyBand(
               N,
               std::get<0>(args),
               std::get<1>(args),
               std::get<2>(args),
               std::get<3>(args),
               std::get<4>(args),
               std::get<5>(args),
               std::get<6>(args),
               std::get<7>(args)),
           ...);
        },
        std::make_tuple(
            std::tie(
                inputs.band1,
                inputs.toggle1,
                inputs.order1,
                inputs.cutoffFreq1,
                inputs.centerFreq1,
                inputs.gain1,
                inputs.freqBandWidth1,
                inputs.passbandRipple1),
            std::tie(
                inputs.band2,
                inputs.toggle2,
                inputs.order2,
                inputs.cutoffFreq2,
                inputs.centerFreq2,
                inputs.gain2,
                inputs.freqBandWidth2,
                inputs.passbandRipple2),
            std::tie(
                inputs.band3,
                inputs.toggle3,
                inputs.order3,
                inputs.cutoffFreq3,
                inputs.centerFreq3,
                inputs.gain3,
                inputs.freqBandWidth3,
                inputs.passbandRipple3),
            std::tie(
                inputs.band4,
                inputs.toggle4,
                inputs.order4,
                inputs.cutoffFreq4,
                inputs.centerFreq4,
                inputs.gain4,
                inputs.freqBandWidth4,
                inputs.passbandRipple4),
            std::tie(
                inputs.band5,
                inputs.toggle5,
                inputs.order5,
                inputs.cutoffFreq5,
                inputs.centerFreq5,
                inputs.gain5,
                inputs.freqBandWidth5,
                inputs.passbandRipple5),
            std::tie(
                inputs.band6,
                inputs.toggle6,
                inputs.order6,
                inputs.cutoffFreq6,
                inputs.centerFreq6,
                inputs.gain6,
                inputs.freqBandWidth6,
                inputs.passbandRipple6),
            std::tie(
                inputs.band7,
                inputs.toggle7,
                inputs.order7,
                inputs.cutoffFreq7,
                inputs.centerFreq7,
                inputs.gain7,
                inputs.freqBandWidth7,
                inputs.passbandRipple7),
            std::tie(
                inputs.band8,
                inputs.toggle8,
                inputs.order8,
                inputs.cutoffFreq8,
                inputs.centerFreq8,
                inputs.gain8,
                inputs.freqBandWidth8,
                inputs.passbandRipple8)));
    if(toRecompute)
    {
      // computes the new bode diagram for the equalizer
      diagramCalculator.computeDiagram();
      // sends a message to the ui to update the graph
      processor_to_ui msg { .xInput = diagramCalculator.frequencies, .yInput = diagramCalculator.responses};
      send_message(std::move(msg));
      toRecompute = false;
    }
  }

  // ********************************************************************************
  //                              UI Part of the object
  // ********************************************************************************

  struct ui // defines the main layout
  {
    // Object size parameters
    static constexpr int mainWidth = 1300;
    static constexpr int mainHeight = 1000;

    // Filters controls size parameters
    static constexpr int controlsWidth = mainWidth;
    static constexpr int controlsHeight = mainHeight / 2 - 100;

    //Display size parameters

    static constexpr auto displayBorders = .9;
    static constexpr int displayWidth = mainWidth * .9;
    static constexpr int displayHeight = (mainHeight - controlsHeight) * .9;

    static constexpr auto filtersNb = 8.5;

    static constexpr auto name() { return "Main"; }
    static constexpr auto width() { return mainWidth; }
    static constexpr auto height() { return mainHeight; }

    static constexpr auto layout()
    {
      enum
      {
        vbox //each sub-layout will be placed vertically in the main layout
      } d{};
      return d;
    }

    static constexpr auto background() //defines color of the background
    {
      enum
      {
        mid
      } d{};
      return d;
    }
    halp::custom_actions_item<EQDiagram<displayWidth, displayHeight, plotPoints>>
        diagram{
            .x = (1 - displayBorders) / 2 * mainWidth,
            .y = (1 - displayBorders) / 2 * mainHeight};
    struct bus
    {
      static void process_message(ui& self, processor_to_ui msg)
      {
        self.diagram.update(msg.xInput.data(), msg.yInput.data());
      }
    };

    struct
    {
      static constexpr auto name() { return "Filters Controls"; }
      static constexpr auto layout()
      {
        enum
        {
          hbox //each sub-layout will be placed horizontally in the main layout
        } d{};
        return d;
      }

      static constexpr auto background() //defines color of the background
      {
        enum
        {
          darker
        } d{};
        return d;
      }

      struct UIBand
      {
        constexpr UIBand(
            decltype(&ins::band1) bd,
            decltype(&ins::toggle1) tog,
            decltype(&ins::order1) ord,
            decltype(&ins::cutoffFreq1) cutoffFreq,
            decltype(&ins::centerFreq1) centerFreq,
            decltype(&ins::freqBandWidth1) freqBdWidth,
            decltype(&ins::gain1) gn,
            decltype(&ins::passbandRipple1) ripple)
            : bandWidget{bd}
            , toggleWidget{tog}
            , orderWidget{ord}
            , cutofffreqWidget{cutoffFreq}
            , centerfreqWidget{centerFreq}
            , bandWidthWidget{freqBdWidth}
            , gainWidget{gn}
            , rippleWidget{ripple}
        {
        }

        static constexpr auto width() { return controlsWidth / filtersNb; }
        static constexpr auto height() { return controlsHeight; }

        decltype(&ins::band1) bandWidget;

        static constexpr auto layout()
        {
          enum
          {
            vbox
          } d{};
          return d;
        }
        static constexpr auto background()
        {
          enum
          {
            dark
          } d{};
          return d;
        }

        decltype(&ins::toggle1) toggleWidget;
        decltype(&ins::order1) orderWidget;
        decltype(&ins::cutoffFreq1) cutofffreqWidget;
        decltype(&ins::centerFreq1) centerfreqWidget;
        decltype(&ins::freqBandWidth1) bandWidthWidget;
        decltype(&ins::gain1) gainWidget;
        decltype(&ins::passbandRipple1) rippleWidget;
      } band1{
          &ins::band1,
          &ins::toggle1,
          &ins::order1,
          &ins::cutoffFreq1,
          &ins::centerFreq1,
          &ins::freqBandWidth1,
          &ins::gain1,
          &ins::passbandRipple1},
          band2{
              &ins::band2,
              &ins::toggle2,
              &ins::order2,
              &ins::cutoffFreq2,
              &ins::centerFreq2,
              &ins::freqBandWidth2,
              &ins::gain2,
              &ins::passbandRipple2},
          band3{
              &ins::band3,
              &ins::toggle3,
              &ins::order3,
              &ins::cutoffFreq3,
              &ins::centerFreq3,
              &ins::freqBandWidth3,
              &ins::gain3,
              &ins::passbandRipple3},
          band4{
              &ins::band4,
              &ins::toggle4,
              &ins::order4,
              &ins::cutoffFreq4,
              &ins::centerFreq4,
              &ins::freqBandWidth4,
              &ins::gain4,
              &ins::passbandRipple4},
          band5{
              &ins::band5,
              &ins::toggle5,
              &ins::order5,
              &ins::cutoffFreq5,
              &ins::centerFreq5,
              &ins::freqBandWidth5,
              &ins::gain5,
              &ins::passbandRipple5},
          band6{
              &ins::band6,
              &ins::toggle6,
              &ins::order6,
              &ins::cutoffFreq6,
              &ins::centerFreq6,
              &ins::freqBandWidth6,
              &ins::gain6,
              &ins::passbandRipple6},
          band7{
              &ins::band7,
              &ins::toggle7,
              &ins::order7,
              &ins::cutoffFreq7,
              &ins::centerFreq7,
              &ins::freqBandWidth7,
              &ins::gain7,
              &ins::passbandRipple7},
          band8{
              &ins::band8,
              &ins::toggle8,
              &ins::order8,
              &ins::cutoffFreq8,
              &ins::centerFreq8,
              &ins::freqBandWidth8,
              &ins::gain8,
              &ins::passbandRipple8};

    } filters;
  };
};
}

/*

Problems:
- should the uis of each band display all controls even they're not necessary for the per-say filter? 
    -> wouldn't have to delete unneeded controls / create neeeded ones each time we change the filter of the band
    -> keeps unnecessary controls in memory
- if not, how do we design the structures of the band in the ui part to display only needed controls? 


Further work:
 - allow an arbitrary number of channels for audio input
 - port ALL filters from DSP
    -> could be done by using pointers instead of direct objects?
*/