#pragma once
#include <cmath>
#include <numeric>
#include <algorithm>

namespace avnd::blocks
{
struct Base {
  static consteval auto category() { return "Basic blocks"; }
  static consteval auto author() { return "ossia score"; }
};

struct Zerop : Base {
  struct
  {
  } inputs;

  struct
  {
    struct { static consteval auto name() { return "Output"; } double value{}; } a;
  } outputs;
};
struct Unop : Base {
  struct
  {
    struct { static consteval auto name() { return "Input"; } double value{}; } a;
  } inputs;

  struct
  {
    struct { static consteval auto name() { return "Output"; } double value{}; } a;
  } outputs;
};

struct Binop : Base {
    struct
    {
      struct { static consteval auto name() { return "Input 1"; } double value{}; } a;
      struct { static consteval auto name() { return "Input 2"; } double value{}; } b;
    } inputs;

    struct
    {
      struct { static consteval auto name() { return "Output 1"; } double value{}; } a;
    } outputs;
};

struct Triop : Base {
  struct
  {
    struct { static consteval auto name() { return "Input 1"; } double value{}; } a;
    struct { static consteval auto name() { return "Input 2"; } double value{}; } b;
    struct { static consteval auto name() { return "Input 3"; } double value{}; } c;
  } inputs;

  struct
  {
    struct { static consteval auto name() { return "Output 1"; } double value{}; } a;
  } outputs;
};


struct Constant : Zerop
{
  static consteval auto name() { return "Absolute value"; }
  static consteval auto c_name() { return "abs"; }
  static consteval auto uuid() { return "a31307d8-ae17-4e27-9628-bb75b618c702"; }

  void init(double cst) { constant = cst; }
  double constant{};
  void operator()()
  { outputs.a.value = constant; }
};


struct Abs : Unop
{
  static consteval auto name() { return "Absolute value"; }
  static consteval auto c_name() { return "abs"; }
  static consteval auto uuid() { return "a31307d8-ae17-4e27-9628-bb75b618c702"; }
  void operator()()
  { outputs.a.value = std::abs(inputs.a.value); }
};

struct Sqrt : Unop
{
  static consteval auto name() { return "Square root"; }
  static consteval auto c_name() { return "sqrt"; }
  static consteval auto uuid() { return "23171629-046a-4b56-bb62-ea896332772e"; }
  void operator()()
  { outputs.a.value = std::sqrt(inputs.a.value); }
};
struct Cbrt : Unop
{
  static consteval auto name() { return "Cubic root"; }
  static consteval auto c_name() { return "cbrt"; }
  static consteval auto uuid() { return "fe2ab5fb-4e10-4785-8f43-f6a19a33d8bc"; }
  void operator()()
  { outputs.a.value = std::cbrt(inputs.a.value); }
};

struct Cos : Unop
{
  static consteval auto name() { return "Cosinus"; }
  static consteval auto c_name() { return "cos"; }
  static consteval auto uuid() { return "31a8da98-ffbf-42e6-a289-41478a5ed9cf"; }
  void operator()()
  { outputs.a.value = std::cos(inputs.a.value); }
};
struct Sin : Unop
{
  static consteval auto name() { return "Sinus"; }
  static consteval auto c_name() { return "sin"; }
  static consteval auto uuid() { return "27556edd-d4f4-4816-baf2-d606776be45f"; }
  void operator()()
  { outputs.a.value = std::sin(inputs.a.value); }
};
struct Tan : Unop
{
  static consteval auto name() { return "Tangent"; }
  static consteval auto c_name() { return "tan"; }
  static consteval auto uuid() { return "72bd1410-ef79-402f-881b-9b0fe4ace47a"; }
  void operator()()
  { outputs.a.value = std::tan(inputs.a.value); }
};
struct ACos : Unop
{
  static consteval auto name() { return "Arc Cosinus"; }
  static consteval auto c_name() { return "acos"; }
  static consteval auto uuid() { return "fc0077fd-5d2e-4c2a-8455-1cbb636a44b2"; }
  void operator()()
  { outputs.a.value = std::acos(inputs.a.value); }
};
struct ASin : Unop
{
  static consteval auto name() { return "Arc Sinus"; }
  static consteval auto c_name() { return "asin"; }
  static consteval auto uuid() { return "934e22d8-518c-4fd0-8eb6-88c8909802fe"; }
  void operator()()
  { outputs.a.value = std::asin(inputs.a.value); }
};
struct ATan : Unop
{
  static consteval auto name() { return "Arc Tangent"; }
  static consteval auto c_name() { return "atan"; }
  static consteval auto uuid() { return "6ca40be7-7ef5-46ec-a03c-57ee8aa3415a"; }
  void operator()()
  { outputs.a.value = std::atan(inputs.a.value); }
};
struct Cosh : Unop
{
  static consteval auto name() { return "Cosinus Hyperbolic"; }
  static consteval auto c_name() { return "cosh"; }
  static consteval auto uuid() { return "370bf947-f864-4020-b487-a946b26a4b0f"; }
  void operator()()
  { outputs.a.value = std::cosh(inputs.a.value); }
};
struct Sinh : Unop
{
  static consteval auto name() { return "Sinus Hyperbolic"; }
  static consteval auto c_name() { return "sinh"; }
  static consteval auto uuid() { return "89a44619-55fa-46de-b3ff-7b506afb1fc6"; }
  void operator()()
  { outputs.a.value = std::sinh(inputs.a.value); }
};
struct Tanh : Unop
{
  static consteval auto name() { return "Tangent Hyperbolic"; }
  static consteval auto c_name() { return "tanh"; }
  static consteval auto uuid() { return "9b65aeb3-b8b0-4fce-b742-32b4622d071b"; }
  void operator()()
  { outputs.a.value = std::tanh(inputs.a.value); }
};
struct ACosh : Unop
{
  static consteval auto name() { return "Arc Cosinus Hyperbolic"; }
  static consteval auto c_name() { return "acosh"; }
  static consteval auto uuid() { return "0d9ec920-551c-4798-b0a8-a1b6cc490fcf"; }
  void operator()()
  { outputs.a.value = std::acosh(inputs.a.value); }
};
struct ASinh : Unop
{
  static consteval auto name() { return "Arc Sinus Hyperbolic"; }
  static consteval auto c_name() { return "asinh"; }
  static consteval auto uuid() { return "e4d2f9ec-6bf4-4131-a949-83aea0f45f9a"; }
  void operator()()
  { outputs.a.value = std::asinh(inputs.a.value); }
};
struct ATanh : Unop
{
  static consteval auto name() { return "Arc Tangent Hyperbolic"; }
  static consteval auto c_name() { return "atanh"; }
  static consteval auto uuid() { return "40f35abf-29ea-4101-8f7e-ee65e38cf1c5"; }
  void operator()()
  { outputs.a.value = std::atanh(inputs.a.value); }
};

struct Exp : Unop
{
  static consteval auto name() { return "Exponential"; }
  static consteval auto c_name() { return "exp"; }
  static consteval auto uuid() { return "65c8e1a3-26ff-48a6-aeec-c3bc41741613"; }
  void operator()()
  { outputs.a.value = std::exp(inputs.a.value); }
};
struct Log : Unop
{
  static consteval auto name() { return "Log (base e)"; }
  static consteval auto c_name() { return "ln"; }
  static consteval auto uuid() { return "0ecb3522-c97d-4b55-9d16-83a393db1cf1"; }
  void operator()()
  { outputs.a.value = std::log(inputs.a.value); }
};
struct Log10 : Unop
{
  static consteval auto name() { return "Log (base 10)"; }
  static consteval auto c_name() { return "log10"; }
  static consteval auto uuid() { return "96e043d0-887e-489b-848f-7c31a54d928d"; }
  void operator()()
  { outputs.a.value = std::log10(inputs.a.value); }
};
struct Log2 : Unop
{
  static consteval auto name() { return "Log (base 2)"; }
  static consteval auto c_name() { return "log2"; }
  static consteval auto uuid() { return "c33ecc95-3de0-4ad9-8e39-b8a1a6788c11"; }
  void operator()()
  { outputs.a.value = std::log2(inputs.a.value); }
};

struct Ceil : Unop
{
  static consteval auto name() { return "Ceil"; }
  static consteval auto c_name() { return "ceil"; }
  static consteval auto uuid() { return "40a2b83e-b28c-40c7-9f41-d2e060ba336a"; }
  void operator()()
  { outputs.a.value = std::ceil(inputs.a.value); }
};
struct Floor : Unop
{
  static consteval auto name() { return "Floor"; }
  static consteval auto c_name() { return "floor"; }
  static consteval auto uuid() { return "c192b98a-b482-41a5-a547-cb9f6c8b5405"; }
  void operator()()
  { outputs.a.value = std::ceil(inputs.a.value); }
};
struct Trunc : Unop
{
  static consteval auto name() { return "Trunc"; }
  static consteval auto c_name() { return "trunc"; }
  static consteval auto uuid() { return "8e7ffbae-38bc-41b7-8f4a-c3ff6162ee8b"; }
  void operator()()
  { outputs.a.value = std::ceil(inputs.a.value); }
};
struct Round : Unop
{
  static consteval auto name() { return "Round"; }
  static consteval auto c_name() { return "round"; }
  static consteval auto uuid() { return "5c7ee251-3a60-48e1-b549-052e9196e952"; }
  void operator()()
  { outputs.a.value = std::ceil(inputs.a.value); }
};


struct Addition : Binop
{
  static consteval auto name() { return "Addition"; }
  static consteval auto c_name() { return "add"; }
  static consteval auto uuid() { return "a3b03041-a7c7-405e-9603-a547cebdccd0"; }
  void operator()()
  { outputs.a.value = inputs.a.value + inputs.b.value; }
};
struct Substraction : Binop
{
  static consteval auto name() { return "Substraction"; }
  static consteval auto c_name() { return "sub"; }
  static consteval auto uuid() { return "22d3cb6a-86ae-4402-8d87-3a0decad7ec1"; }
  void operator()()
  { outputs.a.value = inputs.a.value - inputs.b.value; }
};
struct Multiplication : Binop
{
  static consteval auto name() { return "Multiplication"; }
  static consteval auto c_name() { return "mul"; }
  static consteval auto uuid() { return "67702877-773c-4667-9861-dda20eaa4ca4"; }
  void operator()()
  { outputs.a.value = inputs.a.value * inputs.b.value; }
};
struct Division : Binop
{
  static consteval auto name() { return "Division"; }
  static consteval auto c_name() { return "div"; }
  static consteval auto uuid() { return "adb988d2-822c-4dff-9cea-621b8b7b1bfd"; }
  void operator()()
  { outputs.a.value = inputs.a.value / inputs.b.value; }
};
struct Modulo : Binop
{
  static consteval auto name() { return "Modulo"; }
  static consteval auto c_name() { return "fmod"; }
  static consteval auto uuid() { return "a7ffb126-c173-4560-b892-038cd3d4b2d2"; }
  void operator()()
  { outputs.a.value = std::fmod(inputs.a.value, inputs.b.value); }
};


struct Greater : Binop
{
  static consteval auto name() { return "Greater"; }
  static consteval auto c_name() { return "greater"; }
  static consteval auto uuid() { return "c1bb054f-ca4c-4b7d-87b4-fafe4bd0c073"; }
  void operator()()
  { outputs.a.value = inputs.a.value > inputs.b.value ? 1. : 0.; }
};
struct GreaterEqual : Binop
{
  static consteval auto name() { return "Greater Equal"; }
  static consteval auto c_name() { return "greater_eq"; }
  static consteval auto uuid() { return "c44e1a72-75f1-4b62-943a-0ace9364ac6c"; }
  void operator()()
  { outputs.a.value = inputs.a.value >= inputs.b.value ? 1. : 0.; }
};
struct Lesser : Binop
{
  static consteval auto name() { return "Lesser"; }
  static consteval auto c_name() { return "lesser"; }
  static consteval auto uuid() { return "e3c935ed-d2d9-4978-925a-e7a81e9a82f7"; }
  void operator()()
  { outputs.a.value = inputs.a.value > inputs.b.value ? 1. : 0.; }
};
struct LesserEqual : Binop
{
  static consteval auto name() { return "Lesser Equal"; }
  static consteval auto c_name() { return "lesser_eq"; }
  static consteval auto uuid() { return "e19431ac-d03f-4f73-b3e1-b67d7946a7e3"; }
  void operator()()
  { outputs.a.value = inputs.a.value >= inputs.b.value ? 1. : 0.; }
};
struct Equal : Binop
{
  static consteval auto name() { return "Equal"; }
  static consteval auto c_name() { return "eq"; }
  static consteval auto uuid() { return "c8dbe96b-290f-40af-a459-b2408c20d424"; }
  void operator()()
  { outputs.a.value = inputs.a.value == inputs.b.value ? 1. : 0.; }
};
struct Different : Binop
{
  static consteval auto name() { return "Not equal"; }
  static consteval auto c_name() { return "neq"; }
  static consteval auto uuid() { return "4d59767f-3e04-4f67-b301-5c1ce5836ebd"; }
  void operator()()
  { outputs.a.value = inputs.a.value != inputs.b.value ? 1. : 0.; }
};

struct Power : Binop
{
  static consteval auto name() { return "Power"; }
  static consteval auto c_name() { return "pow"; }
  static consteval auto uuid() { return "41cba2c2-a14c-42c0-90c3-a39a91b3c905"; }
  void operator()()
  { outputs.a.value = std::pow(inputs.a.value, inputs.b.value); }
};

struct Min : Binop
{
  static consteval auto name() { return "Min"; }
  static consteval auto c_name() { return "min"; }
  static consteval auto uuid() { return "4598d024-7bed-4b9f-86b8-27a687cf73ad"; }
  void operator()()
  { outputs.a.value = std::min(inputs.a.value, inputs.b.value); }
};
struct Max : Binop
{
  static consteval auto name() { return "Max"; }
  static consteval auto c_name() { return "max"; }
  static consteval auto uuid() { return "b6200175-8002-4191-92e6-a32256c71f9f"; }
  void operator()()
  { outputs.a.value = std::max(inputs.a.value, inputs.b.value); }
};

struct ATan2 : Binop
{
  static consteval auto name() { return "Arc Tangent 2"; }
  static consteval auto c_name() { return "atan2"; }
  static consteval auto uuid() { return "9299c2d9-14b8-4156-99e7-a22693633d7e"; }
  void operator()()
  { outputs.a.value = std::atan2(inputs.a.value, inputs.b.value); }
};


struct Lerp : Triop
{
  static consteval auto name() { return "Lerp"; }
  static consteval auto c_name() { return "lerp"; }
  static consteval auto uuid() { return "bcd9b929-9d28-479c-b9f3-a6a3ae05fb20"; }
  void operator()()
  { outputs.a.value = std::lerp(inputs.a.value, inputs.b.value, inputs.c.value); }
};
struct Clamp : Triop
{
  static consteval auto name() { return "Clamp"; }
  static consteval auto c_name() { return "clamp"; }
  static consteval auto uuid() { return "fe2d8cfe-05c4-4e7a-9ad1-29eaf161359d"; }
  void operator()()
  { outputs.a.value = std::clamp(inputs.a.value, inputs.b.value, inputs.c.value); }
};

struct Copy : Unop {
  static consteval auto name() { return "Copy"; }
  static consteval auto c_name() { return "copy"; }
  static consteval auto uuid() { return "53640965-92d1-4627-a5a4-bcd503a41f91"; }

  void operator()()
  { outputs.a.value = inputs.a.value; }
};
}
