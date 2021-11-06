#include <avnd/common/function_reflection.hpp>
#include <avnd/concepts/callback.hpp>
#include <avnd/helpers/callback.hpp>

using namespace avnd;

void f();
void g(int);
void h(int, float);

struct x {
  void f_simple();
  void f_c() const;
  void f_v() volatile;
  void f_cv() const volatile;
  void f_ref() &;
  void f_c_ref() const &;
  void f_v_ref() volatile &;
  void f_cv_ref() const volatile &;
  void f_rref() &&;
  void f_c_rref() const &&;
  void f_v_rref() volatile &&;
  void f_cv_rref() const volatile &&;

  int* example(const char*, x&);
};

static_assert(function_reflection<f>::count == 0);
static_assert(function_reflection<g>::count == 1);
static_assert(function_reflection<h>::count == 2);
static_assert(std::is_same_v<boost::mp11::mp_first<function_reflection<g>::arguments>, int>);
static_assert(std::is_same_v<boost::mp11::mp_second<function_reflection<h>::arguments>, float>);

static_assert(!function_reflection<&x::f_simple>::is_const);

static_assert(function_reflection<&x::f_c>::is_const);
static_assert(!function_reflection<&x::f_c>::is_volatile);
static_assert(!function_reflection<&x::f_c>::is_reference);
static_assert(!function_reflection<&x::f_c>::is_rvalue_reference);

static_assert(!function_reflection<&x::f_v>::is_const);
static_assert(function_reflection<&x::f_v>::is_volatile);
static_assert(!function_reflection<&x::f_v>::is_reference);
static_assert(!function_reflection<&x::f_v>::is_rvalue_reference);

static_assert(function_reflection<&x::f_cv_rref>::is_const);
static_assert(function_reflection<&x::f_cv_rref>::is_volatile);
static_assert(!function_reflection<&x::f_cv_rref>::is_reference);
static_assert(function_reflection<&x::f_cv_rref>::is_rvalue_reference);

static_assert(std::is_same_v<function_reflection<&x::example>::return_type, int*>);
static_assert(std::is_same_v<function_reflection<&x::example>::arguments, boost::mp11::mp_list<const char*, x&>>);



static_assert(function_ish<std::function<void(float, float)>>);
static_assert(function_ish<basic_callback<void(float, float)>>);
using a_function_t = void(*)(float);
static_assert(std::is_function_v<std::remove_pointer_t<a_function_t>>);
static_assert(function<decltype(basic_callback<void(float, float)>::function)>);
static_assert(pointer<decltype(basic_callback<void(float, float)>::context)>);
static_assert(function_view_ish<basic_callback<void(float, float)>>);


int main() { }
