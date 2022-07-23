#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/ui/nuklear/nuklear.hpp>
#include <avnd/common/for_nth.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/introspection/layout.hpp>
#include <avnd/introspection/messages.hpp>
#include <avnd/wrappers/metadatas.hpp>

#include <chrono>
#include <thread>

namespace nkl
{
constexpr const char* c_str(std::string_view s)
{
  return s.data();
}

template <typename T>
struct layout_ui_base
{
  using inputs_type = typename avnd::input_introspection<T>::type;
  using outputs_type = typename avnd::output_introspection<T>::type;

  avnd::effect_container<T>& implementation;


  static constexpr const int row_height = 40;
  nk_context* ctx{};

  template<typename Item>
  void forEachUnsafe(const Item& item, auto&& f)
  {
    using namespace boost::pfr;
    using namespace boost::pfr::detail;
    constexpr int N = avnd::fields_count_unsafe<Item>();
    auto t = boost::pfr::detail::tie_as_tuple(item, size_t_<N>{});
    [&]<std::size_t... I>(std::index_sequence<I...>)
    {
      (f(sequence_tuple::get<I>(t)), ...);
    }
    (make_index_sequence<N>{});
  }

  void recurseItem(const auto& item)
  {
    forEachUnsafe(item, [this](auto& child) { this->createItem(child); });
  }

  template <avnd::float_control C>
  void create(C& control, int idx)
  {
    constexpr auto rng = avnd::get_range<C>();
    nk_layout_row_dynamic(ctx, row_height, 2);
    nk_label(ctx, avnd::get_name<C>().data(), NK_TEXT_LEFT);
    nk_slider_float(ctx, rng.min, &control.value, rng.max, 0.1);
  }

  template <avnd::int_control C>
  void create(C& control, int idx)
  {
    constexpr auto rng = avnd::get_range<C>();
    nk_layout_row_dynamic(ctx, row_height, 2);
    nk_label(ctx, avnd::get_name<C>().data(), NK_TEXT_LEFT);
    nk_slider_int(ctx, rng.min, &control.value, rng.max, 1);
  }

  void createWidget(const auto& item)
  {
    if constexpr (requires {
                    {
                      item
                      } -> std::convertible_to<std::string_view>;
                  })
    {
      nk_label(ctx, item, NK_TEXT_LEFT);
    }
    else if constexpr (requires { ((inputs_type{}).*item); })
    {
      auto& ins = avnd::get_inputs<T>(this->implementation);
      auto& control = ins.*item;
      create(control, avnd::index_in_struct(ins, item));
    }
    else if constexpr (requires { ((outputs_type{}).*item); })
    {
      auto& ins = avnd::get_outputs<T>(this->implementation);
      auto& control = ins.*item;
      create(control, avnd::index_in_struct(ins, item));
    }
  }

  template <typename Item>
  void createTabs(const Item& item)
  {
    constexpr int child_count = avnd::pfr::tuple_size_v<Item>;
    static int tab_state = 0;
    nk_layout_row_begin(ctx, NK_STATIC, row_height, child_count);

    int k = 0;
    forEachUnsafe(
        item,
        [&]<typename TT>(const TT& child)
        {
          if (nk_tab(ctx, c_str(TT::name()), tab_state == k))
          {
            tab_state = k;
          }
          k++;
        });
    k = 0;
    forEachUnsafe(
        item,
        [&]<typename TT>(const TT& child)
        {
          if (k == tab_state)
          {
            createItem(child);
          }
          k++;
        });
  }

  template <typename Item>
  void createItem(const Item& item)
  {
    if constexpr (avnd::spacing_layout<Item>)
    {
      nk_label(ctx, " ", NK_TEXT_LEFT);
    }
    else if constexpr (avnd::container_layout<Item> || avnd::hbox_layout<Item> || avnd::group_layout<Item> || avnd::grid_layout<Item>)
    {
      constexpr int child_count = avnd::pfr::tuple_size_v<Item>;
      nk_layout_row_dynamic(ctx, row_height, child_count);
      recurseItem(item);
    }
    else if constexpr (avnd::vbox_layout<Item>)
    {
      recurseItem(item);
    }
    else if constexpr (avnd::split_layout<Item>)
    {
      if (nk_tree_push(ctx, NK_TREE_TAB, "Split", NK_MINIMIZED))
      {
        recurseItem(item);
        nk_tree_pop(ctx);
      }
    }
    else if constexpr (avnd::group_layout<Item>)
    {
      if (nk_tree_push(ctx, NK_TREE_TAB, c_str(Item::name()), NK_MINIMIZED))
      {
        recurseItem(item);
        nk_tree_pop(ctx);
      }
    }
    else if constexpr (avnd::tab_layout<Item>)
    {
      createTabs(item);
    }
    else if constexpr (avnd::control_layout<Item>)
    {
      // Normal widget
      createWidget(item);
    }
    else if constexpr (avnd::custom_layout<Item>)
    {
    }
    else if constexpr (avnd::has_layout<Item>)
    {
      recurseItem(item);
    }
    else
    {
      // Normal widget
      createWidget(item);
    }
  }

  void createLayout()
  {
    using type = typename avnd::ui_type<T>::type;
    constexpr type layout;
    createItem(layout);
  }
};

struct XWindow
{
  explicit XWindow(void* handle = nullptr)
  {
    dpy = XOpenDisplay(NULL);
    if (!dpy)
    {
      assert(0);
      return;
    }

    root = DefaultRootWindow(dpy);
    screen = XDefaultScreen(dpy);
    vis = XDefaultVisual(dpy, screen);
    cmap = XCreateColormap(dpy,root,vis,AllocNone);

    swa.colormap = cmap;
    swa.event_mask =
        ExposureMask | KeyPressMask | KeyReleaseMask |
        ButtonPress | ButtonReleaseMask| ButtonMotionMask |
        Button1MotionMask | Button3MotionMask | Button4MotionMask | Button5MotionMask|
        PointerMotionMask | KeymapStateMask;

    owned = (handle == nullptr);
    if(!owned)
    {
      win = (Window)handle;
    }
    else
    {
      win = XCreateWindow(dpy, root, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 0,
                         XDefaultDepth(dpy, screen), InputOutput,
                         vis, CWEventMask | CWColormap, &swa);
    }

    XStoreName(dpy, win, "X11");
    XMapWindow(dpy, win);
    wm_delete_window = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &wm_delete_window, 1);
    XGetWindowAttributes(dpy, win, &attr);

    /* GUI */
    font = nk_xfont_create(dpy, "fixed");
  }

  ~XWindow()
  {
    nk_xfont_del(dpy, font);

    //nk_xlib_shutdown();

    XUnmapWindow(dpy, win);
    XFreeColormap(dpy, cmap);
    if(owned)
      XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
  }

  Display *dpy{};
  Window root{};
  Visual *vis{};
  Colormap cmap{};
  XWindowAttributes attr{};
  XSetWindowAttributes swa{};
  Window win{};
  int screen{};
  XFont *font{};
  Atom wm_delete_window{};
  bool owned = false;
};

template <typename T>
class layout_ui_xlib : layout_ui_base<T>
{
public:
  XWindow xw;
  nk_colorf bg;
  struct nk_image img;

  int running = 1;

  explicit layout_ui_xlib(avnd::effect_container<T>& impl, void* handle = nullptr)
      : layout_ui_base<T>{impl}
      , xw{handle}
  {
    this->ctx = nk_xlib_init(xw.font, xw.dpy, xw.screen, xw.win, xw.attr.width, xw.attr.height);

    bg.r = 0.10f, bg.g = 0.18f, bg.b = 0.24f, bg.a = 1.0f;
  }

  ~layout_ui_xlib()
  {
    nk_xlib_shutdown();
  }
  auto get_size()
  {
    struct sz { int w, h; };
    return sz{xw.attr.width, xw.attr.height};
  }

  void run_one()
  {
    // Input
    XEvent evt;

    nk_input_begin(this->ctx);
    while (XPending(xw.dpy))
    {
      XNextEvent(xw.dpy, &evt);
      if (evt.type == ClientMessage)
      {
        running = 0;
        nk_input_end(this->ctx);
        return;
      }

      if (XFilterEvent(&evt, xw.win))
        continue;

      nk_xlib_handle_event(xw.dpy, xw.screen, xw.win, &evt);
    }
    nk_input_end(this->ctx);

    // Render
    XGetWindowAttributes(xw.dpy, xw.win, &xw.attr);
    if (nk_begin(this->ctx, "Demo", nk_rect(0, 0, xw.attr.width, xw.attr.height), 0))
      this->createLayout();
    nk_end(this->ctx);

    // Flush
    XClearWindow(xw.dpy, xw.win);
    nk_xlib_render(xw.win, nk_rgb(30,30,30));
    XFlush(xw.dpy);
  }

  void render()
  {
    while (running)
    {
      run_one();
      std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
  }
};






/*
template <typename T>
class layout_ui_glfw : layout_ui_base<T>
{
public:
  avnd::effect_container<T>& implementation;
  GLFWwindow* win;
  int width = 0, height = 0;
  nk_colorf bg;
  struct nk_image img;

  explicit layout_ui_glfw(avnd::effect_container<T>& impl)
      : implementation{impl}
  {
    glfwSetErrorCallback([](int e, const char* d) { printf("Error %d: %s\n", e, d); });
    if (!glfwInit())
    {
      fprintf(stdout, "[GFLW] failed to init!\n");
      exit(1);
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    win = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Demo", nullptr, nullptr);
    glfwMakeContextCurrent(win);
    glfwGetWindowSize(win, &width, &height);

    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    glewExperimental = 1;
    if (glewInit() != GLEW_OK)
    {
      fprintf(stderr, "Failed to setup GLEW\n");
      exit(1);
    }

    this->ctx = nk_glfw3_init(
        win, NK_GLFW3_INSTALL_CALLBACKS, MAX_VERTEX_BUFFER, MAX_ELEMENT_BUFFER);
    {
      struct nk_font_atlas* atlas;
      nk_glfw3_font_stash_begin(&atlas);
      nk_glfw3_font_stash_end();
    }

    bg.r = 0.10f, bg.g = 0.18f, bg.b = 0.24f, bg.a = 1.0f;
  }

  ~layout_ui_glfw()
  {
    nk_glfw3_shutdown();
    glfwTerminate();
  }

  void render_one()
  {
    glfwPollEvents();
    nk_glfw3_new_frame();

    glfwGetWindowSize(win, &width, &height);

    if (nk_begin(this->ctx, "Demo", nk_rect(0, 0, width, height), 0))
      this->createLayout();
    nk_end(this->ctx);

    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(bg.r, bg.g, bg.b, bg.a);
    nk_glfw3_render(NK_ANTI_ALIASING_ON);
    glfwSwapBuffers(win);
  }

  void render()
  {
    while (!glfwWindowShouldClose(win))
      render_one();
  }
};
*/
template<typename T>
using layout_ui = layout_ui_xlib<T>;
}

#undef CursorShape
#undef Ok
#undef Status
#undef Bool
#undef None
#undef True
#undef False
#undef Expose
#undef FontChange
#undef FocusIn
#undef FocusOut
#undef KeyPress
#undef KeyRelease
