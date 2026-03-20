#pragma once
#include <avnd/common/struct_reflection.hpp>
#include <avnd/concepts/hardware.hpp>
#include <avnd/introspection/generic.hpp>

namespace avnd
{
generate_predicate_introspection(gpio_port);
generate_member_introspection(gpio_port, hardware);

generate_predicate_introspection(pwm_port);
generate_member_introspection(pwm_port, hardware);

generate_predicate_introspection(adc_port);
generate_member_introspection(adc_port, hardware);

generate_predicate_introspection(dac_port);
generate_member_introspection(dac_port, hardware);
}
