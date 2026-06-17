#!/usr/bin/env python3
# Generate the compile-time benchmark translation units:
#   cases/     - one "exercise" TU per real example (instantiates the introspection
#                a binding walks: input/output/messages introspection + per-port
#                iteration + metadata). Built from exercise.tmpl.
#   scaling/   - synthetic flat-N and rec-N objects to characterise scaling.
#   micro/     - pure mechanism micro-benchmarks (std::tuple_cat vs std::tie vs
#                reflection feed) isolating the root cause of the flattening cost.
#
# Usage: python3 generate_cases.py            (writes into ./cases ./scaling ./micro)
import os

HERE = os.path.dirname(os.path.abspath(__file__))

# ---------------------------------------------------------------- real examples
EXAMPLES = [
    ("minimal",       "Raw/Minimal.hpp",                          "examples::Minimal"),
    ("addition",      "Raw/Addition.hpp",                         "examples::Addition"),
    ("lowpass",       "Raw/Lowpass.hpp",                          "examples::Lowpass"),
    ("trivialfilter", "Tutorial/TrivialFilterExample.hpp",        "examples::TrivialFilterExample"),
    ("audioeffect",   "Tutorial/AudioEffectExample.hpp",          "examples::AudioEffectExample"),
    ("distortion",    "Tutorial/Distortion.hpp",                  "examples::Distortion"),
    ("saccfilter",    "Tutorial/SampleAccurateFilterExample.hpp", "examples::SampleAccurateFilterExample"),
    ("hlowpass",      "Helpers/Lowpass.hpp",                      "examples::helpers::Lowpass"),
    ("peak",          "Helpers/Peak.hpp",                          "examples::helpers::Peak"),
    ("controls",      "Helpers/Controls.hpp",                      "examples::helpers::Controls"),
    # "kabang" (large, recursive ~220 ports): copy tests/test_introspection_many.cpp
]

def gen_examples():
    os.makedirs(f"{HERE}/cases", exist_ok=True)
    tmpl = open(f"{HERE}/exercise.tmpl").read()
    for name, header, typ in EXAMPLES:
        out = tmpl.replace("__HEADER__", header).replace("__TYPE__", typ)
        open(f"{HERE}/cases/{name}.cpp", "w").write(out)
    # large recursive object
    import shutil
    shutil.copy(f"{HERE}/../../tests/test_introspection_many.cpp", f"{HERE}/cases/kabang.cpp")

# ----------------------------------------------------------------- scaling series
SCALE_HEAD = '''#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <cstddef>
struct Big {
  halp_meta(name, "Big")
  halp_meta(c_name, "big")
  struct ins {
'''
SCALE_TAIL = '''  } inputs;
  struct { } outputs;
};
static Big g{};
int main() {
  using IN = avnd::input_introspection<Big>;
  volatile auto s = IN::size;
  IN::for_all(avnd::get_inputs(g), [](auto& p){ if constexpr(requires{p.value;}){ volatile auto v=p.value;(void)v; } });
  (void)s;
}
'''
def gen_scaling():
    os.makedirs(f"{HERE}/scaling", exist_ok=True)
    for N in (16, 64, 128, 256):  # flat: N direct ports
        body = "".join(f'    halp::knob_f32<"p{i}"> p{i};\n' for i in range(N))
        open(f"{HERE}/scaling/flat{N}.cpp", "w").write(SCALE_HEAD + body + SCALE_TAIL)
    grp = ('struct G { halp_flag(recursive_group); '
           + " ".join(f'halp::knob_f32<"g{i}"> g{i};' for i in range(8)) + ' };\n')
    for N in (64, 128, 256):  # recursive: N leaves via 8-wide groups
        M = N // 8
        head = ('#include <halp/controls.hpp>\n#include <halp/meta.hpp>\n'
                '#include <avnd/introspection/input.hpp>\n#include <cstddef>\n'
                + grp + 'struct Big { halp_meta(name,"B") struct ins {\n')
        body = "".join(f'    G grp{i};\n' for i in range(M))
        tail = ('  } inputs; struct{} outputs; };\nstatic Big g{};\n'
                'int main(){ using IN=avnd::input_introspection<Big>; volatile auto s=IN::size;'
                ' IN::for_all(avnd::get_inputs(g),[](auto&p){if constexpr(requires{p.value;})'
                '{volatile auto v=p.value;(void)v;}}); (void)s; }\n')
        open(f"{HERE}/scaling/rec{N}.cpp", "w").write(head + body + tail)

# ------------------------------------------------------------------ micro-benches
META = '#if __has_include(<meta>)\n#include <meta>\n#else\n#include <experimental/meta>\n#endif\n'
HELP = ('namespace sm=std::meta;\n'
        'template<typename T> consteval std::size_t mcount(){ return sm::nonstatic_data_members_of(^^T, sm::access_context::unchecked()).size(); }\n'
        'template<typename T> consteval std::array<sm::info,mcount<T>()> members(){ std::array<sm::info,mcount<T>()> a{}; std::size_t i=0; for(sm::info m: sm::nonstatic_data_members_of(^^T, sm::access_context::unchecked())) a[i++]=m; return a; }\n')
def gen_micro():
    os.makedirs(f"{HERE}/micro", exist_ok=True)
    for N in (32, 64, 128, 256):
        decl = "static int a[%d];\n" % N
        cat = " ,\n    ".join("std::tie(a[%d])" % i for i in range(N))
        open(f"{HERE}/micro/purecat_{N}.cpp", "w").write(
            "#include <tuple>\n" + decl +
            f"auto build() {{ return std::tuple_cat(\n    {cat}); }}\n"
            f"int main() {{ auto t = build(); std::get<{N-1}>(t) = 1; return std::get<0>(t); }}\n")
        tie = ", ".join("a[%d]" % i for i in range(N))
        open(f"{HERE}/micro/puretie_{N}.cpp", "w").write(
            "#include <tuple>\n" + decl +
            f"auto build() {{ return std::tie({tie}); }}\n"
            f"int main() {{ auto t = build(); std::get<{N-1}>(t) = 1; return std::get<0>(t); }}\n")
        members = "".join("  int m%d;\n" % i for i in range(N))
        base = "#include <tuple>\n#include <array>\n#include <utility>\n" + META + f"struct S {{\n{members}}};\nstatic S s{{}};\n" + HELP
        open(f"{HERE}/micro/refl_cat_{N}.cpp", "w").write(base +
            "template<sm::info... Ms,typename T> auto f(T& v){ return std::tuple_cat(std::tie(v.[:Ms:])...); }\n"
            "template<typename T,std::size_t... I> auto idx(T& v,std::index_sequence<I...>){ return f<members<T>()[I]...>(v); }\n"
            f"int main(){{ auto t=idx(s,std::make_index_sequence<mcount<S>()>{{}}); std::get<{N-1}>(t)=1; return std::get<0>(t); }}\n")
        open(f"{HERE}/micro/refl_tie_{N}.cpp", "w").write(base +
            "template<sm::info... Ms,typename T> auto f(T& v){ return std::tie(v.[:Ms:]...); }\n"
            "template<typename T,std::size_t... I> auto idx(T& v,std::index_sequence<I...>){ return f<members<T>()[I]...>(v); }\n"
            f"int main(){{ auto t=idx(s,std::make_index_sequence<mcount<S>()>{{}}); std::get<{N-1}>(t)=1; return std::get<0>(t); }}\n")
        open(f"{HERE}/micro/refl_enum_{N}.cpp", "w").write(base +
            "template<sm::info... Ms,typename T> std::size_t f(T&){ return sizeof...(Ms); }\n"
            "template<typename T,std::size_t... I> std::size_t idx(T& v,std::index_sequence<I...>){ return f<members<T>()[I]...>(v); }\n"
            f"int main(){{ volatile auto n=idx(s,std::make_index_sequence<mcount<S>()>{{}}); return (int)n; }}\n")

if __name__ == "__main__":
    gen_examples(); gen_scaling(); gen_micro()
    print("generated cases/, scaling/, micro/")
