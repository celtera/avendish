# Custom items

One can also define and use custom items. This is however very experimental and only support in the ossia bindings so far :-)

## Non-interactive items
Here is a non-interactive item:

```cpp
struct custom_anim
{
  // Static item metadatas: mandatory
  static constexpr double width() { return 200.; }
  static constexpr double height() { return 200.; }
  static constexpr double layout() { enum { custom } d{}; return d; }

  // In practice with the helpers, we use a type with the mandatory parts
  // already defined and just focus on our item's specificities ; this is 
  // enabled by this typedef.
  using item_type = custom_anim;

  // Item properties: those are mandatory
  double x = 0.0;
  double y = 0.0;
  double scale = 1.0;

  // Our paint method. avnd::painter is a concept which maps to the most usual 
  // canvas-like APIs. It is not necessary to indicate it - it just will give better
  // error messages in case of mistake, and code completion (yes) in IDEs such as QtCreator
  void paint(avnd::painter auto ctx)
  {
    constexpr double cx = 30., cy = 30.;
    constexpr double side = 40.;
  
    ctx.set_stroke_color({.r = 92, .g = 53, .b = 102, .a = 255});
    ctx.set_fill_color({173, 127, 168, 255});

    ctx.translate(100, 100);
    ctx.rotate(rot += 0.1);
  
    for(int i = 0; i < 10; i++)
    {
      ctx.translate(10, 10);
      ctx.rotate(5.+ 0.1 * rot);
      ctx.scale(0.8, 0.8);
      ctx.begin_path();
  
      ctx.draw_rect(-side / 2., -side / 2., side, side);
      ctx.fill();
      ctx.stroke();
    }
  
    ctx.update();
  }

  double rot{};
};
```

This produces the small squares animation here: 

![Basic UI](images/ui-image.gif)

## Interactive items for controlling single ports
This is *even more* experimental :)

Here is what I believe to be the first entirely UI-library-independent UI slider defined in C++.

```cpp
// This type allows to define a sequence of operations which will modify a value, 
// in order to allow handling undo-redo properly. 
// The std::function members are filled by the bindings.
template<typename T>
struct transaction
{
  std::function<void()> start;
  std::function<void(const T&)> update;
  std::function<void()> commit;
  std::function<void()> rollback;
};

// look ma, no inheritance
struct custom_slider
{
  // Same as above
  static constexpr double width() { return 100.; }
  static constexpr double height() { return 20.; }

  // Needed for changing the ui. It's the type above - it's already defined as-is 
  // in the helpers library.
  halp::transaction<double> transaction;

  // Called when the value changes from the host software.
  void set_value(const auto& control, double value)
  {
    this->value = avnd::map_control_to_01(control, value);
  }

  // When transaction.update() is called, this converts the value in the slider 
  // into one fit for the control definition passed as argument.
  static auto value_to_control(auto& control, double value)
  {
    return avnd::map_control_from_01(control, value);
  }

  // Paint method: same as above
  void paint(avnd::painter auto ctx)
  {
    ctx.set_stroke_color({200, 200, 200, 255});
    ctx.set_stroke_width(2.);
    ctx.set_fill_color({120, 120, 120, 255});
    ctx.begin_path();
    ctx.draw_rect(0., 0., width(), height());
    ctx.fill();
    ctx.stroke();

    ctx.begin_path();
    ctx.set_fill_color({90, 90, 90, 255});
    ctx.draw_rect(2., 2., (width() - 4) * value, (height() - 4));
    ctx.fill();
  }

  // Return true to handle the event. x, y, are the positions of the item in local coordinates.
  bool mouse_press(double x, double y)
  {
    transaction.start();
    mouse_move(x, y);
    return true;
  }

  // Obvious :-)
  void mouse_move(double x, double y)
  {
    const double res = std::clamp(x / width(), 0., 1.);
    transaction.update(res);
  }

  // Same
  void mouse_release(double x, double y)
  {
    mouse_move(x, y);
    transaction.commit();
  }
  
  double value{};
};

// This wraps a custom widget in all the data which is mandatory to have so that we do not have to repeat it.
// This is also already provided in the helper library ; using it looks like: 
// 
// halp::custom_item<custom_slider, &inputs::my_control>
template<typename T, auto F>
struct custom_item
{
  static constexpr double layout() { enum { custom } d{}; return d; }
  using item_type = T;

  double x = 0.0;
  double y = 0.0;
  double scale = 1.0;
  decltype(F) control = F;
};
```

## Painter API
Here is the complete supported API so far:

```cpp
{{ #include ../../../include/avnd/concepts/painter.hpp }}
```