#pragma once

// Ported from https://github.com/v7b1/vb-objects/blob/main/source/projects/vb.fourses_tilde/vb.fourses_tilde.c

/*
    a chaotic oscillator network
    based on descriptions of the 'fourses system' by ciat-lonbarde
    www.ciat-lonbarde.net

    07.april 2013, volker böhm
*/

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/log.hpp>
#include <halp/meta.hpp>

#include <avnd/concepts/message.hpp>
#include <cmath>

namespace vb_ports
{

#define NUMFOURSES 4
struct horse
{
    // this is a horse... basically a ramp generator
    double		val = 0.;
    double		inc = 0.01;
    double		dec = -0.01;
    double		adder = inc;
    double		incy{}, incym1{};		// used for smoothing
    double		decy{}, decym1{};		// used for smoothing
};

template <typename C>
requires halp::has_logger<C>
struct fourses_tilde
{
  halp_meta(name, "vb.fourses~");
  halp_meta(c_name, "avnd_vb_fourses_tilde");
  halp_meta(category, "Ports");
  halp_meta(author, "Volker Böhm");
  halp_meta(description, "vb.fourses~ by volker böhm");
  halp_meta(uuid, "9db0af3c-8573-4541-95d4-cf7902cdbedb");

  // For this object all the inputs are messages
  struct inputs { };

  // Four single-channel audio outputs
  struct
  {
    halp::audio_channel<"osc1", double, "(signal) signal out osc1"> osc1;
    halp::audio_channel<"osc2", double, "(signal) signal out osc2"> osc2;
    halp::audio_channel<"osc3", double, "(signal) signal out osc3"> osc3;
    halp::audio_channel<"osc4", double, "(signal) signal out osc4"> osc4;
  } outputs;

  // Messages which will change the configuration of the object
  struct messages {
    struct X {
        halp_meta(name, "smooth")
        void operator()(fourses_tilde& x, double input) {
            input = std::clamp(input, 0., 1.);
            x.smoother = 0.01 - std::pow(input,0.2)*0.01;
        }
    } smooth;

    struct {
        halp_meta(name, "hilim")
        void operator()(fourses_tilde& x, double input) {
            x.fourses[0].val = input;
        }
    } hilim;

    struct {
        halp_meta(name, "lolim")
        void operator()(fourses_tilde& x, double input) {
            x.fourses[5].val = input;
        }
    } lolim;

    struct {
      halp_meta(name, "upfreq")
      void operator()(fourses_tilde& x, double freq1, double freq2, double freq3, double freq4) {
          x.fourses[1].inc = fabs(freq1) * 4 * x.r_sr;
          x.fourses[2].inc = fabs(freq2) * 4 * x.r_sr;
          x.fourses[3].inc = fabs(freq3) * 4 * x.r_sr;
          x.fourses[4].inc = fabs(freq4) * 4 * x.r_sr;
      }
    } upfreq;

    struct {
        halp_meta(name, "downfreq")
        void operator()(fourses_tilde& x, double freq1, double freq2, double freq3, double freq4) {
            x.fourses[1].inc = fabs(freq1) * -4 * x.r_sr;
            x.fourses[2].inc = fabs(freq2) * -4 * x.r_sr;
            x.fourses[3].inc = fabs(freq3) * -4 * x.r_sr;
            x.fourses[4].inc = fabs(freq4) * -4 * x.r_sr;
        }
    } downfreq;
    struct {
        halp_meta(name, "info")
        void operator()(fourses_tilde& x) {
            // only fourses 1 to 4 are used
            x.logger.info("----- fourses.info -------");
            for(int i=1; i<=NUMFOURSES; i++) {
                x.logger.info("fourses[{}].val = {}", i, x.fourses[i].val);
                x.logger.info("fourses[{}].inc = {}", i, x.fourses[i].inc);
                x.logger.info("fourses[{}].dec = {}", i, x.fourses[i].dec);
                x.logger.info("fourses[{}].adder = {}", i, x.fourses[i].adder);
            }
            x.logger.info("------ end -------");
        }
    } info;
  };

  void prepare(halp::setup info)
  {
    if(info.rate <= 0) r_sr = 1.0 / 44100.0;
    else r_sr = 1.0 / info.rate;
  }

  void operator()(int vs)
  {
      // This is handled upwards in the stack
      // if (x->x_obj.z_disabled)
      //     return;

      double c = smoother;
      double hilim = fourses[0].val;
      double lolim = fourses[5].val;

      double* output[] = { outputs.osc1.channel, outputs.osc2.channel, outputs.osc3.channel, outputs.osc4.channel };

      for(int i = 0; i < vs; i++)
      {
         for(int n = 1; n <= NUMFOURSES; n++) {
             // smoother
             fourses[n].incy = fourses[n].inc*c + fourses[n].incym1*(1-c);
             fourses[n].incym1 = fourses[n].incy;

             fourses[n].decy = fourses[n].dec*c + fourses[n].decym1*(1-c);
             fourses[n].decym1 = fourses[n].decy;

             double val = fourses[n].val;
             val += fourses[n].adder;

             if(val <= fourses[n+1].val || val <= lolim ) {
                 fourses[n].adder = fourses[n].incy;
             }
             else if( val >= fourses[n-1].val || val >= hilim ) {
                 fourses[n].adder = fourses[n].decy;
             }

             output[n-1][i] = val;

             fourses[n].val = val;
         }
      }
  }

  [[no_unique_address]] typename C::logger_type logger;
  double r_sr = 1. / 44100.;
  horse fourses[NUMFOURSES+2]{ {.val = 1.}, {}, {}, {}, {.val = -1.} };	// four horses make a fourse...
  double smoother = 0.01;
};
}
