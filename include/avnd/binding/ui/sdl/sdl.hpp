#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_video.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_syswm.h>

#include <set>

namespace avnd
{

struct sdl_mouse_event
{
  int x = 0, y = 0;
  int button = 0;
};

struct sdl_widget
{
  int x = 0, y = 0, z = 0, w = 0, h = 0;

  bool contains(int parent_x, int parent_y)
  {
    return parent_x >= x && parent_y >= y
        && parent_x < x + w && parent_y < y + h;
  }

  sdl_widget() = default;
  sdl_widget(const sdl_widget&) = delete;
  sdl_widget(sdl_widget&&) = delete;
  sdl_widget& operator=(const sdl_widget&) = delete;
  sdl_widget& operator=(sdl_widget&&) = delete;
  virtual ~sdl_widget();
  virtual void paint(SDL_Renderer& r) = 0;
  virtual void mouse_press(sdl_mouse_event, SDL_Renderer& r) = 0;
  virtual void mouse_move(sdl_mouse_event, SDL_Renderer& r) = 0;
  virtual void mouse_release(sdl_mouse_event, SDL_Renderer& r) = 0;
};

struct sdl_gui
{
  struct z_comp {
    bool operator()(sdl_widget* lhs, sdl_widget* rhs) const noexcept
    {
      return lhs->z < rhs->z;
    }
  };


  std::set<sdl_widget*, z_comp> widgets;
  sdl_widget* grab{};
  int pressIndex = 0;

  ~sdl_gui()
  {
    for(auto w : widgets) delete w;
  }

  void add_widget(sdl_widget* w)
  {
    widgets.insert(w);
  }

  sdl_widget* find(int x, int y)
  {
    sdl_widget* widg{};
    for(auto w : widgets)
    {
      if(w->contains(x, y))
      {
        if(!widg)
        {
          widg = w;
          continue;
        }
        else
        {
          if(w->z > widg->z)
            widg = w;
        }
      }
    }
    return widg;
  }

  sdl_mouse_event map_to(sdl_widget& widg, SDL_MouseButtonEvent& ev)
  {
    sdl_mouse_event e;
    e.x = ev.x - widg.x;
    e.y = ev.y - widg.y;
    if(ev.button == 0)
    {
      e.button = 0;
    }
    else
      e.button = 1;
    return e;
  }

  void mouse_press(SDL_MouseButtonEvent& ev, SDL_Renderer& renderer)
  {
    if(!grab)
      grab = find(ev.x, ev.y);

    if(grab)
      grab->mouse_press(map_to(*grab, ev), renderer);

    pressIndex++;
  }
  void mouse_move(SDL_MouseButtonEvent& ev, SDL_Renderer& renderer)
  {
    if(grab)
      grab->mouse_move(map_to(*grab, ev), renderer);
  }
  void mouse_release(SDL_MouseButtonEvent& ev, SDL_Renderer& renderer)
  {
    if(grab)
      grab->mouse_release(map_to(*grab, ev), renderer);

    pressIndex--;
    if(pressIndex == 0)
      grab = nullptr;
  }

  void update(SDL_Renderer* renderer)
  {
    SDL_Event event;
    while(SDL_PollEvent(&event))
    {
      switch(event.type)
      {
        case SDL_MOUSEBUTTONDOWN:
          mouse_press(event.button, *renderer);
          break;
        case SDL_MOUSEBUTTONUP:
          mouse_release(event.button, *renderer);
          break;
        case SDL_MOUSEMOTION:
          mouse_move(event.button, *renderer);
          break;
        case SDL_KEYDOWN:
          switch(event.key.keysym.sym)
          {
            case SDLK_ESCAPE:
              break;

            case SDLK_LEFT:
              break;
            case SDLK_RIGHT:
              break;
            case SDLK_UP:
              break;
            case SDLK_DOWN:
              break;

            default:
              break;
          }
          break;

        case SDL_WINDOWEVENT:
          switch(event.window.event)
          {
            case SDL_WINDOWEVENT_RESIZED:
              SDL_SetRenderDrawColor(renderer, rand(), 0, 0, 255);
              SDL_RenderFillRect(renderer, NULL);
              break;

            default:
              break;
          }
          break;
      }
    }

    // Paint

    SDL_SetRenderDrawColor(renderer, rand(), 0, 0, 255);
    SDL_RenderFillRect(renderer, NULL);
    SDL_RenderPresent(renderer);

  }


};



class sdl_ui
{
  SDL_Window* screen{};
  SDL_Renderer* renderer{};
  sdl_gui gui;
public:
  explicit sdl_ui(void* handle)
  {
    //  Prevent SDL from setting SIGINT handler on Posix Systems
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    if (int ret = SDL_Init(SDL_INIT_VIDEO); ret < 0)
    {
      return;
    }
    screen = SDL_CreateWindowFrom(handle);
    SDL_SetWindowSize(screen, 640, 480);

    renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_SOFTWARE);
    if(!renderer)
    {
      fprintf(stderr, "SDL: could not create renderer - exiting\n");
      return;
    }
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer, NULL);
    SDL_RenderPresent(renderer);
  }

  auto get_size()
  {
    struct sz { int w, h; };
    return sz{640, 480};
  }

  void run_one()
  {
    gui.update(renderer);
  }

private:

};


}
