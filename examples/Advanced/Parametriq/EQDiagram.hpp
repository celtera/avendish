#pragma once

#include "ParametricEqControls.hpp"
#include "Response.hpp"
#include <avnd/concepts/painter.hpp>
#include <halp/custom_widgets.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <optional>
#include <tuple>
#include <utility>


namespace examples::parametriq
{
struct EQDiagram
{
  static constexpr double width() { return 560.; }
  static constexpr double height() { return 210.; }
  static constexpr int points = response_points;
  static constexpr double left = 34., top = 18., plot_width = 510., plot_height = 168.;

  using Values = decltype(std::tuple_cat(
      BandValues{}, BandValues{}, BandValues{}, BandValues{}, BandValues{},
      BandValues{}, BandValues{}, BandValues{}, std::tuple<int, float>{}));
  template<typename... T>
  static auto transaction_types(const std::tuple<T...>&)
      -> std::tuple<halp::transaction<T>...>;
  template<typename... T>
  static auto execution_types(const std::tuple<T...>&)
      -> std::tuple<std::optional<T>...>;
  // values are what the user edits; execution_values what execution gives
  // the ports, while it runs. Handles show the former, the curve the latter.
  Values values{};
  decltype(execution_types(values)) execution_values{};
  decltype(transaction_types(values)) transactions;
  static constexpr auto selection() { return "eq-band"; }
  std::function<void(int)> select;
  std::function<void()> update;
  // The native host starts a GUI-only timer on demand and stops when poll()
  // returns false. Neither callback is ever passed to the response worker.
  std::function<void()> request_poll;
  // The host gives its audio device's rate here; the processor's messages
  // replace it with the rate it actually runs at.
  double sample_rate = 48000.;
  double cached_rate = 0.;
  Settings cached{};
  ResponseService response;
  ResponseStatus status = ResponseStatus::Pending;
  std::uint64_t revision{};
  bool needs_response = false;
  bool has_curve = false;
  std::array<double, points> ordinate{};
  // Where the curve is interpolated across a measurement that did not settle:
  // drawn dashed.
  std::array<bool, points> estimated{};
  int selected = -1;
  bool dragging = false, editing = false;
  // What a handle drag edits: the frequency along x; along y the gain of a band
  // that has one, else the parameter shaping it (the Q of a low-pass...), which
  // a drag with alt edits alone on any band. On a band without gain the handle
  // follows that parameter, through the scale it had when the drag started.
  Field drag_shape = field_count;
  bool drag_frequency = true, drag_mapped = false;
  int wheel_edit = -1;
  ConfigurationValues wheel_configuration{};
  double press_x = 0., press_y = 0., drag_x = 0., drag_y = 0., drag_value = 0., drag_level = 0.;

  void repaint()
  {
    if(update)
      update();
  }
  void on_selection_changed(int index)
  {
    if(index < 0 || index >= band_count)
      return;
    if(index != selected)
      finish_wheel();
    if(index != selected && dragging)
    {
      dragging = false;
      commit();
    }
    selected = index;
    repaint();
  }
  void select_band(int index)
  {
    on_selection_changed(index);
    if(select)
      select(index);
  }
  template <typename F>
  static void visit_band(int index, F&& f)
  {
    switch(index)
    {
      case 0: f.template operator()<0>(); break;
      case 1: f.template operator()<1>(); break;
      case 2: f.template operator()<2>(); break;
      case 3: f.template operator()<3>(); break;
      case 4: f.template operator()<4>(); break;
      case 5: f.template operator()<5>(); break;
      case 6: f.template operator()<6>(); break;
      case 7: f.template operator()<7>(); break;
    }
  }
  template <std::size_t I, typename T>
  void set(T value)
  {
    using V = std::tuple_element_t<I, Values>;
    const V next = static_cast<V>(value);
    auto& transaction = std::get<I>(transactions);
    if(std::get<I>(values) == next || !transaction.update)
      return;
    // All raw control transactions share one host gesture. Start lazily:
    // selecting a handle without moving it is not an undoable edit.
    if(!editing)
    {
      std::get<0>(transactions).start();
      editing = true;
    }
    transaction.update(next);
  }
  void commit()
  {
    if(editing)
      std::get<0>(transactions).commit();
    editing = false;
  }
  template <std::size_t Band>
  Parameters band() const
  {
    return [&]<std::size_t... I>(std::index_sequence<I...>) {
      return parameters(BandValues{std::get<Band * field_count + I>(values)...});
    }(std::make_index_sequence<field_count>{});
  }
  template <typename T>
  static Settings settings(const T& v)
  {
    const auto band = [&]<std::size_t Band>() {
      return [&]<std::size_t... I>(std::index_sequence<I...>) {
        return parameters(BandValues{std::get<Band * field_count + I>(v)...});
      }(std::make_index_sequence<field_count>{});
    };
    return {{band.template operator()<0>(), band.template operator()<1>(),
             band.template operator()<2>(), band.template operator()<3>(),
             band.template operator()<4>(), band.template operator()<5>(),
             band.template operator()<6>(), band.template operator()<7>()},
            std::get<band_count * field_count>(v),
            std::get<band_count * field_count + 1>(v)};
  }
  Settings settings() const { return settings(values); }
  // What sounds: the executed value of each port where there is one.
  Settings executed() const
  {
    const auto v = [&]<std::size_t... I>(std::index_sequence<I...>) {
      return Values{std::get<I>(execution_values).value_or(std::get<I>(values))...};
    }(std::make_index_sequence<std::tuple_size_v<Values>>{});
    return settings(v);
  }
  double upper_frequency() const { return std::min(20000., parametriq::sample_rate(sample_rate) * 0.49); }
  double x(double frequency) const
  {
    return left + plot_width * std::log(frequency / 20.) / std::log(upper_frequency() / 20.);
  }
  static double y(double db)
  {
    return top + plot_height * (36. - std::clamp(db, -36., 36.)) / 72.;
  }
  double frequency_at(double px) const
  {
    return 20. * std::exp(std::clamp((px - left) / plot_width, 0., 1.)
                         * std::log(upper_frequency() / 20.));
  }
  static double gain_at(double py)
  {
    return std::clamp(36. - (py - top) * 72. / plot_height, -24., 24.);
  }
  // Where a band's handle sits: at its gain when it has one. A band without
  // gain sits where its own response is at its frequency, as the curve shows
  // it when that band shapes it alone: a flat low-pass at its -3 dB, a resonant
  // one up on its peak. Its shape parameter moves it there, so that a vertical
  // drag maps the level the mouse points at back to the parameter. The levels
  // are made to rise with the parameter, at least `min_slope` dB over its
  // range, so that every value of it can be reached: where the band's own
  // level falls (a vintage ladder past self-oscillation) or stays put (a
  // band-pass normalized to 0 dB, a notch, an all-pass), the handle leaves the
  // curve, and with less than `min_span` dB of change it shows the parameter
  // along `direct_low` to `direct_high` instead.
  static constexpr double level_low = -30., level_high = 30.;
  static constexpr double min_slope = 6., min_span = 6.;
  static constexpr double direct_low = -24., direct_high = 24.;
  struct HandleScale
  {
    bool direct = true;
    std::array<double, level_points> db{};
    double level(double position) const noexcept
    {
      if(direct)
        return direct_low + (direct_high - direct_low) * std::clamp(position, 0., 1.);
      for(int k = 1; k < level_points; ++k)
      {
        if(position <= level_position(k) || k == level_points - 1)
        {
          const double from = level_position(k - 1), to = level_position(k);
          const double t = std::clamp((position - from) / (to - from), 0., 1.);
          return db[k - 1] + t * (db[k] - db[k - 1]);
        }
      }
      return db.back();
    }
    // The inverse of level(), clamped to the parameter's range.
    double position(double level) const noexcept
    {
      if(direct)
        return std::clamp((level - direct_low) / (direct_high - direct_low), 0., 1.);
      if(level <= db.front())
        return 0.;
      for(int k = 1; k < level_points; ++k)
      {
        if(level <= db[k])
        {
          const double from = level_position(k - 1), to = level_position(k);
          return from + (to - from) * (level - db[k - 1]) / (db[k] - db[k - 1]);
        }
      }
      return 1.;
    }
  };
  // Levels measured along with the curve, for nonlinear bands.
  std::array<BandLevels, band_count> measured_levels{};
  HandleScale drag_scale{};
  static bool same_configuration(const Parameters& a, const Parameters& b) noexcept
  {
    return a.model == b.model && a.response == b.response && a.slope == b.slope
           && a.character == b.character && a.submodel == b.submodel;
  }
  // `p` is the band's sanitized edited settings.
  HandleScale handle_scale(int band, const Parameters& p) const
  {
    HandleScale scale;
    BandLevels analytic;
    const BandLevels* levels = nullptr;
    auto edited = sanitize(settings(), sample_rate);
    edited.bands[band] = p;
    if(analytic_levels(edited, band, parametriq::sample_rate(sample_rate), analytic))
      levels = &analytic;
    // A measurement made for other settings of the same filter, while a new
    // one is under way: close enough to keep the handle where it was.
    else if(measured_levels[band].valid && same_configuration(measured_levels[band].band, p))
      levels = &measured_levels[band];
    if(!levels)
      return scale;
    for(int k = 0; k < level_points; ++k)
      scale.db[k] = std::clamp(levels->db[k], level_low, level_high);
    const bool shaped = shape_field(p) != field_count;
    const auto [lo, hi] = std::minmax_element(scale.db.begin(), scale.db.end());
    if(shaped && *hi - *lo < min_span)
      return scale;
    scale.direct = false;
    if(shaped)
      for(int k = 1; k < level_points; ++k)
        scale.db[k] = std::max(scale.db[k], scale.db[k - 1]
            + min_slope * (level_position(k) - level_position(k - 1)));
    return scale;
  }
  double handle_level(int band, const Parameters& p) const
  {
    if(applicable(p, Gain))
      return p.gain;
    if(dragging && band == selected && drag_mapped)
      return drag_scale.level(shape_position(p, drag_shape));
    return handle_scale(band, p).level(shape_position(p, shape_field(p)));
  }
  template<std::size_t Band>
  void set_shape(Parameters p, Field field, double value)
  {
    if(!p.enabled || !applicable(p, field))
      return;
    switch(field)
    {
      case Q:
        p.q = value;
        set<Band * field_count + Q>(sanitize(p, sample_rate).q);
        break;
      case Bandwidth:
        p.bandwidth = value;
        set<Band * field_count + Bandwidth>(sanitize(p, sample_rate).bandwidth);
        break;
      case Resonance:
        p.resonance = value;
        set<Band * field_count + Resonance>(sanitize(p, sample_rate).resonance);
        break;
      case ShelfSlope:
        p.shelf_slope = value;
        set<Band * field_count + ShelfSlope>(sanitize(p, sample_rate).shelf_slope);
        break;
      default: break;
    }
  }
  static bool inside(double px, double py)
  {
    return px >= left && px <= left + plot_width
           && py >= top && py <= top + plot_height;
  }
  int hit(double px, double py) const
  {
    const auto current = settings();
    int found = -1;
    double distance = 100.;
    for(int i = 0; i < band_count; ++i)
    {
      const auto p = sanitize(current.bands[i], sample_rate);
      if(!p.enabled)
        continue;
      const double dx = px - x(p.frequency), dy = py - y(handle_level(i, p));
      const double d = dx * dx + dy * dy;
      if(d < distance || (d == distance && i == selected))
      {
        distance = d;
        found = i;
      }
    }
    return found;
  }
  bool mouse_press(auto event)
  {
    finish_wheel();
    if(event.button != event.left || !inside(event.x, event.y))
      return false;
    const int index = hit(event.x, event.y);
    if(index < 0)
      return !(event.modifiers & event.alt);
    const auto p = sanitize(settings().bands[index], sample_rate);
    const bool shape_only = event.modifiers & event.alt;
    drag_shape = (shape_only || !applicable(p, Gain)) ? shape_field(p) : field_count;
    if(shape_only && drag_shape == field_count)
      return false;
    drag_frequency = !shape_only;
    drag_mapped = drag_shape != field_count && !applicable(p, Gain);
    if(drag_mapped)
      drag_scale = handle_scale(index, p);
    select_band(index);
    drag_level = handle_level(index, p);
    dragging = true;
    press_x = event.x;
    press_y = event.y;
    drag_x = x(p.frequency);
    drag_y = y(drag_level);
    drag_value = shape_value(p, drag_shape);
    return true;
  }
  void mouse_move(auto event)
  {
    if(!dragging || (!editing && event.x == press_x && event.y == press_y))
      return;
    visit_band(selected, [&]<std::size_t Band> {
      auto p = sanitize(band<Band>(), sample_rate);
      if(!p.enabled)
        return;
      if(drag_frequency)
      {
        // The limits of the other parameters follow the frequency.
        p.frequency = frequency_at(drag_x + event.x - press_x);
        p = sanitize(p, sample_rate);
        set<Band * field_count + Frequency>(p.frequency);
      }
      if(drag_mapped)
      {
        // The handle goes where the mouse goes, within the parameter's range.
        const double level = drag_level + (press_y - event.y) * 72. / plot_height;
        set_shape<Band>(p, drag_shape, shape_at(drag_shape, drag_scale.position(level)));
      }
      else if(drag_shape != field_count)
      {
        const double amount = std::clamp((press_y - event.y) * 0.02, -20., 20.);
        const double value = drag_shape == Resonance ? drag_value + amount * 0.1
            : drag_value * std::exp(drag_shape == Bandwidth ? -amount : amount);
        set_shape<Band>(p, drag_shape, value);
      }
      else if(applicable(p, Gain))
      {
        p.gain = gain_at(drag_y + event.y - press_y);
        set<Band * field_count + Gain>(sanitize(p, sample_rate).gain);
      }
    });
  }
  void mouse_release(auto event)
  {
    if(event.button != event.left || !dragging)
      return;
    mouse_move(event);
    dragging = false;
    commit();
  }
  // Opt into the native wheel gesture lifecycle. The key separates bands and
  // parameter meanings; the host supplies scroll-end / idle / interruption.
  int wheel_target(double px, double py) const
  {
    if(dragging || !inside(px, py))
      return -1;
    const int index = hit(px, py);
    if(index < 0)
      return -1;
    const auto field = shape_field(sanitize(settings().bands[index], sample_rate));
    return field == field_count ? -1 : index * field_count + field;
  }
  void finish_wheel()
  {
    if(wheel_edit < 0)
      return;
    wheel_edit = -1;
    commit();
  }
  bool wheel(double px, double py, double delta)
  {
    const int target = wheel_target(px, py);
    if(target < 0 || delta == 0.)
      return false;
    const int index = target / field_count;
    const auto field = static_cast<Field>(target % field_count);
    const auto p = sanitize(settings().bands[index], sample_rate);
    const auto configuration = configuration_values(p);
    if(target != wheel_edit || configuration != wheel_configuration)
      finish_wheel();
    select_band(index);
    wheel_edit = target;
    wheel_configuration = configuration;
    const double amount = std::clamp(delta / 480., -20., 20.);
    const double value = field == Resonance ? p.resonance + amount * 0.1
        : shape_value(p, field) * std::exp2(field == Bandwidth ? -amount : amount);
    visit_band(index, [&]<std::size_t Band> { set_shape<Band>(p, field, value); });
    return true;
  }
  template<std::size_t Band>
  void create_band(const Parameters& p)
  {
    const auto fields = std::tuple{
        p.enabled, p.model, p.response, p.slope, p.character, p.submodel,
        p.order, p.frequency, p.q, p.bandwidth, p.gain, p.shelf_slope,
        p.ripple, p.resonance, p.drive, p.trim, p.extra, p.correction, p.sigma};
    // Enable last so dependent native controls see a fully initialized band.
    [&]<std::size_t... I>(std::index_sequence<I...>) {
      (set<Band * field_count + I + 1>(std::get<I + 1>(fields)), ...);
    }(std::make_index_sequence<field_count - 1>{});
    set<Band * field_count + Enabled>(true);
  }
  bool mouse_double_click(auto event)
  {
    finish_wheel();
    if(event.button != event.left || !inside(event.x, event.y))
      return false;
    if(const int index = hit(event.x, event.y); index >= 0)
    {
      select_band(index);
      return true;
    }
    const auto current = settings();
    for(int i = 0; i < band_count; ++i)
    {
      if(current.bands[i].enabled)
        continue;
      commit();
      dragging = false;
      select_band(i);
      Parameters p;
      p.enabled = true;
      p.model = RBJ;
      p.response = event.x <= x(50.) ? HighPass
                   : event.x >= x(10000.) ? LowPass : Bell;
      p.frequency = frequency_at(event.x);
      p.gain = p.response == Bell ? gain_at(event.y) : 0.;
      p = sanitize(p, sample_rate);
      visit_band(i, [&]<std::size_t Band> { create_band<Band>(p); });
      commit();
      return true;
    }
    return true;
  }
  // Whether the curve is out of date; cheap enough for every paint.
  bool stale() const
  {
    const double next_rate = parametriq::sample_rate(sample_rate);
    return next_rate != cached_rate || sanitize(executed(), next_rate) != cached;
  }
  // Called from poll(), never from paint: it may finish a wheel gesture,
  // which pushes an undo command.
  void refresh()
  {
    const double next_rate = parametriq::sample_rate(sample_rate);
    if(wheel_edit >= 0)
    {
      const auto p = sanitize(settings().bands[wheel_edit / field_count], next_rate);
      if(!p.enabled || configuration_values(p) != wheel_configuration)
        finish_wheel();
    }
    const auto next = sanitize(executed(), next_rate);
    if(next == cached && next_rate == cached_rate)
      return;
    if(next_rate != cached_rate || response_kind(next) == ResponseKind::Unavailable)
      has_curve = false;
    cached = next;
    cached_rate = next_rate;
    status = ResponseStatus::Pending;
    needs_response = true;
    if(request_poll)
      request_poll();
  }
  bool poll()
  {
    refresh();
    // Coalesce edits into the native display timer, not a trailing debounce.
    // Analytic requests finish here; nonlinear measurements stay on the worker.
    if(needs_response)
    {
      needs_response = false;
      revision = response.request(cached, cached_rate);
    }
    const auto show = [&](const ResponseResult& result) {
      for(int i = 0; i < points; ++i)
        ordinate[i] = y(result.db[i]);
      estimated = result.estimated;
      repaint();
    };
    // Levels are keyed by the settings they were measured for: those of an
    // earlier request still place handles until the latest ones come.
    const auto keep_levels = [&](const ResponseResult& result) {
      for(int i = 0; i < band_count; ++i)
        if(result.levels[i].valid)
        {
          measured_levels[i] = result.levels[i];
          repaint();
        }
    };
    ResponseResult result;
    const bool taken = response.take(result);
    if(taken)
      keep_levels(result);
    if(taken && result.revision == revision)
    {
      status = result.status;
      has_curve = status == ResponseStatus::Analytic || status == ResponseStatus::Reference;
      if(has_curve)
        show(result);
      else
        repaint();
    }
    else if(status == ResponseStatus::Pending && response.take_preview(result))
    {
      keep_levels(result);
      has_curve = true;
      show(result);
    }
    return status == ResponseStatus::Pending;
  }
  void paint(avnd::painter auto ctx)
  {
    // Edits and execution values repaint the item: the next poll follows them.
    if(request_poll && (wheel_edit >= 0 || stale()))
      request_poll();
    const auto edited = sanitize(settings(), sample_rate);
    auto color = [&](halp::colors role, std::array<unsigned char, 4> fallback) {
      if constexpr(requires { ctx.to_rgba(role); })
      {
        const auto c = ctx.to_rgba(role);
        return std::array<unsigned char, 4>{c.r, c.g, c.b, c.a};
      }
      return fallback;
    };
    const auto grid = color(halp::colors::mid, {90, 90, 90, 255});
    const auto text = color(halp::colors::lighter, {200, 200, 200, 255});
    const auto curve = color(halp::colors::runtime_value_mid, {230, 180, 40, 255});
    ctx.set_stroke_color({grid[0], grid[1], grid[2], static_cast<unsigned char>(grid[3] / 4)});
    ctx.set_fill_color({text[0], text[1], text[2], text[3]});
    ctx.set_stroke_width(1.);
    ctx.set_font_size(10.);
    ctx.begin_path();
    for(double db : {-36., -24., -12., 0., 12., 24., 36.})
      ctx.draw_line(left, y(db), left + plot_width, y(db));
    struct Tick { double hz; const char* label; };
    static constexpr Tick ticks[]{
        {20., "20"}, {50., "50"}, {100., "100"}, {200., "200"}, {500., "500"},
        {1000., "1k"}, {2000., "2k"}, {5000., "5k"}, {10000., "10k"}, {20000., "20k"}};
    for(const auto& tick : ticks)
      if(tick.hz <= upper_frequency())
        ctx.draw_line(x(tick.hz), top, x(tick.hz), top + plot_height);
    ctx.stroke();
    ctx.begin_path();
    for(int db : {-36, -24, -12, 0, 12, 24, 36})
    {
      char text[8];
      std::snprintf(text, sizeof(text), "%+d", db);
      ctx.draw_text(2., y(db) + 3., text);
    }
    for(const auto& tick : ticks)
      if(tick.hz <= upper_frequency())
        ctx.draw_text(x(tick.hz) - 6., top + plot_height + 16., tick.label);
    ctx.fill();
    ctx.set_stroke_color({grid[0], grid[1], grid[2], static_cast<unsigned char>(grid[3] / 2)});
    ctx.begin_path();
    ctx.draw_line(left, y(0.), left + plot_width, y(0.));
    ctx.stroke();
    if(has_curve)
    {
      ctx.set_stroke_color({curve[0], curve[1], curve[2], curve[3]});
      ctx.set_stroke_width(2.);
      ctx.begin_path();
      bool segment = false, dashed = false;
      for(int i = 0; i < points; ++i)
      {
        if(!std::isfinite(ordinate[i]))
        {
          segment = false;
          continue;
        }
        const double px = left + plot_width * i / (points - 1);
        const bool estimate = i > 0 && (estimated[i - 1] || estimated[i]);
        dashed |= estimate;
        if(segment && !(estimate && (i / 4) % 2))
          ctx.line_to(px, ordinate[i]);
        else
          ctx.move_to(px, ordinate[i]);
        segment = true;
      }
      ctx.stroke();
      if(dashed)
      {
        ctx.set_fill_color({text[0], text[1], text[2], text[3]});
        ctx.begin_path();
        ctx.draw_text(left + plot_width - 196., top - 6., "Dashed where the response does not settle");
        ctx.fill();
      }
    }
    // Handles represent editable parameters, not the sum of all filters.
    ctx.set_font_size(10.);
    for(int i = 0; i < band_count; ++i)
    {
      const auto& p = edited.bands[i];
      if(!p.enabled)
        continue;
      const double px = x(p.frequency), py = y(handle_level(i, p));
      const auto& c = i == selected ? curve : text;
      ctx.set_fill_color({c[0], c[1], c[2], c[3]});
      ctx.set_stroke_color({c[0], c[1], c[2], c[3]});
      ctx.set_stroke_width(i == selected ? 2. : 1.);
      ctx.begin_path();
      ctx.draw_circle(px, py, i == selected ? 7. : 5.);
      ctx.fill();
      if(i == selected)
      {
        ctx.begin_path();
        ctx.draw_circle(px, py, 10.);
        ctx.stroke();
      }
      char number[4];
      std::snprintf(number, sizeof(number), "%d", i + 1);
      ctx.begin_path();
      ctx.draw_text(px - 3., py - 12., number);
      ctx.fill();
    }
  }
};
}
