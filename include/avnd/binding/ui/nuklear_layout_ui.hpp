#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/ui/nuklear/nuklear.hpp>
#include <avnd/common/for_nth.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/layout.hpp>
#include <avnd/introspection/messages.hpp>
#include <avnd/wrappers/metadatas.hpp>
namespace nkl
{
constexpr const char* c_str(std::string_view s)
{
  return s.data();
}
template <typename T>
class layout_ui
{
public:
  static constexpr const int row_height = 40;
  avnd::effect_container<T>& implementation;
  GLFWwindow* win;
  int width = 0, height = 0;
  nk_context* ctx;
  nk_colorf bg;
  struct nk_image img;

  explicit layout_ui(avnd::effect_container<T>& impl)
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

    ctx = nk_glfw3_init(
        win, NK_GLFW3_INSTALL_CALLBACKS, MAX_VERTEX_BUFFER, MAX_ELEMENT_BUFFER);
    {
      struct nk_font_atlas* atlas;
      nk_glfw3_font_stash_begin(&atlas);
      nk_glfw3_font_stash_end();
    }

    bg.r = 0.10f, bg.g = 0.18f, bg.b = 0.24f, bg.a = 1.0f;
  }

  ~layout_ui()
  {
    nk_glfw3_shutdown();
    glfwTerminate();
  }

  void recurseItem(const auto& item)
  {
    avnd::for_each_field_ref(item, [this](auto& child) { this->createItem(child); });
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
    else if constexpr (requires { (avnd::get_inputs<T>(this->implementation).*item); })
    {
      auto& ins = avnd::get_inputs<T>(this->implementation);
      auto& control = ins.*item;
      create(control, avnd::index_in_struct(ins, item));
    }
  }

  template <typename Item>
  void createTabs(const Item& item)
  {
    constexpr int child_count = boost::pfr::tuple_size_v<Item>;
    static int tab_state = 0;
    nk_layout_row_begin(ctx, NK_STATIC, row_height, child_count);

    int k = 0;
    avnd::for_each_field_ref(
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
    avnd::for_each_field_ref(
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
    constexpr int child_count = boost::pfr::tuple_size_v<Item>;
    if constexpr (requires { item.spacing; })
    {
      nk_label(ctx, " ", NK_TEXT_LEFT);
    }
    else if constexpr (requires { item.hbox; })
    {
      nk_layout_row_dynamic(ctx, row_height, child_count);
      recurseItem(item);
    }
    else if constexpr (requires { item.vbox; })
    {
      recurseItem(item);
    }
    else if constexpr (requires { item.split; })
    {
      if (nk_tree_push(ctx, NK_TREE_TAB, "Split", NK_MINIMIZED))
      {
        recurseItem(item);
        nk_tree_pop(ctx);
      }
    }
    else if constexpr (requires { item.group; })
    {
      if (nk_tree_push(ctx, NK_TREE_TAB, c_str(Item::name()), NK_MINIMIZED))
      {
        recurseItem(item);
        nk_tree_pop(ctx);
      }
    }
    else if constexpr (requires { item.tabs; })
    {
      createTabs(item);
    }
    else
    {
      // Normal widget
      createWidget(item);
    }
  }

  void createLayout()
  {
    using type = typename avnd::layout_type<T>::type;
    constexpr type layout;
    createItem(layout);
  }

  void render()
  {
    while (!glfwWindowShouldClose(win))
    {
      glfwPollEvents();
      nk_glfw3_new_frame();

      glfwGetWindowSize(win, &width, &height);

      if (nk_begin(ctx, "Demo", nk_rect(0, 0, width, height), 0))
        createLayout();
      nk_end(ctx);

      glViewport(0, 0, width, height);
      glClear(GL_COLOR_BUFFER_BIT);
      glClearColor(bg.r, bg.g, bg.b, bg.a);
      nk_glfw3_render(NK_ANTI_ALIASING_ON);
      glfwSwapBuffers(win);
    }
  }
};
}
