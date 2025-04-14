#pragma once
#include <cmath>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/file_port.hpp>
#include <halp/meta.hpp>
#include <halp/sample_accurate_controls.hpp>
#include <halp/texture.hpp>

#include <QImage>

/** 
 * An example of how to make an object that reads a sprite from a sprite sheet.
 * Uses Qt's QImage for image processing but any pull request using a simpler image 
 * reader such as stb_image will be very welcome!
 */
namespace examples::helpers
{
struct SpriteReader
{
  halp_meta(name, "Sprite reader");
  halp_meta(c_name, "sprite_reader");
  halp_meta(category, "Visuals");
  halp_meta(author, "Jean-Michaël Celerier");
  halp_meta(description, "Loads an image file into a texture");
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/sprite-reader.html")
  halp_meta(uuid, "b28b0cf5-164b-4aa0-8edd-8830ade702e5");

  struct
  {
    struct : halp::file_port<"Image", halp::mmap_file_view>
    {
      void update(SpriteReader& r) { r.on_image_changed(); }
    } img;

    halp::xy_spinboxes_i32<"Sprite size", halp::range{1, 1024, 32}> sz;
    halp::hslider_f32<"Image count"> percentage;
  } inputs;

  struct
  {
    halp::texture_output<"Out"> tex;
  } outputs;

  QImage image_data;
  QImage sprite;

  void on_image_changed()
  {
    image_data = QImage(QString::fromUtf8(this->inputs.img.file.filename))
                     .convertedTo(QImage::Format_RGBA8888);
  }

  void operator()()
  {
    if(image_data.isNull())
      return;

    const int sprite_w = inputs.sz.value.x;
    const int sprite_h = inputs.sz.value.y;
    const QSize img_size = image_data.size();
    const int columns = std::floor(img_size.width() / float(sprite_w));
    const int rows = std::floor(img_size.height() / float(sprite_h));
    if(columns <= 0 || rows <= 0)
      return;

    const int N = columns * rows;
    const int cur = std::floor(inputs.percentage * N);
    const int cur_row = cur / columns;
    const int cur_col = cur % columns;
    const QRect rect(cur_col * sprite_w, cur_row * sprite_h, sprite_w, sprite_h);
    sprite = image_data.copy(rect);
    outputs.tex.texture.update(sprite.bits(), inputs.sz.value.x, inputs.sz.value.y);
  }
};
}
