#include <avnd/binding/liblo/receive.hpp>
#include <avnd/binding/liblo/send.hpp>
#include <examples/Helpers/Controls.hpp>
#include <examples/Raw/AllPortsTypes.hpp>

#include <cassert>
#include <chrono>
#include <thread>

void test0()
{
  using T = examples::helpers::Controls;
  auto obj1 = std::make_shared<T>();
  auto server = osc::liblo_recv_binding<T>(obj1, "5677", "/foo");

  auto addr = lo_address_new("127.0.0.1", "5677");
  auto sender = osc::liblo_send_binding{addr, "/foo"};

  // Send some dumb values over OSC
  auto obj2 = std::make_shared<T>();
  obj2->inputs.a.value = 123;
  obj2->inputs.b.value = -4560;
  obj2->inputs.c.value = false;
  obj2->inputs.d.value = "hello world";
  obj2->inputs.e.value = true;
  obj2->inputs.f.value = decltype(obj2->inputs.f.value)::Foo;
  sender.push(obj2->inputs.a);
  sender.push(obj2->inputs.b);
  sender.push(obj2->inputs.c);
  sender.push(obj2->inputs.d);
  sender.push(obj2->inputs.e);
  sender.push(obj2->inputs.f);

  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  // The server applied clamping given the parameter values
  assert(obj1->inputs.a.value == 1.);
  assert(obj1->inputs.b.value == -1000);
  assert(obj1->inputs.c.value == false);
  assert(obj1->inputs.d.value == "hello world");
  assert(obj1->inputs.e.value == true);
  assert(obj1->inputs.f.value == decltype(obj2->inputs.f.value)::Foo);

  lo_address_free(addr);
}

void test1()
{
  using T = examples::AllPortsTypes;
  auto obj1 = std::make_shared<T>();
  auto server = osc::liblo_recv_binding<T>(obj1, "5677", "/foo");

  auto addr = lo_address_new("127.0.0.1", "5677");
  auto sender = osc::liblo_send_binding{addr, "/foo"};

  // Send some dumb values over OSC
  auto obj2 = std::make_shared<T>();

  avnd::input_introspection<T>::for_all(
      avnd::get_inputs(*obj2), [&]<typename F>(F& field) { sender.push(field); });

  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  // The server applied clamping given the parameter values

  lo_address_free(addr);
}

int main()
{
  test0();
  test1();
}
