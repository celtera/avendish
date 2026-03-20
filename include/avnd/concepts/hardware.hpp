#pragma once
#include <avnd/concepts/generic.hpp>

namespace avnd
{
type_or_value_qualification(gpio)
type_or_value_qualification(pwm)
type_or_value_qualification(adc)
type_or_value_qualification(dac)
type_or_value_qualification(i2c)
type_or_value_qualification(spi)
type_or_value_qualification(uart)
type_or_value_qualification(rtc)
type_or_value_qualification(watchdog)

type_or_value_qualification(hardware)
type_or_value_reflection(hardware)

template <typename T>
concept gpio_port = has_gpio<T> && requires(T t) { bool(t.state); };

template <typename T>
concept pwm_port = has_pwm<T> && requires(T t) {
  t.duty_cycle;
  t.frequency;
};

template <typename T>
concept adc_port = has_adc<T> && requires(T t) { t.value; };

template <typename T>
concept dac_port = has_dac<T> && requires(T t) { t.value; };

// All below this : TODO, names reserved
template <typename T>
concept i2c_port = has_i2c<T> && requires(T t) {
  t.sda;
  t.scl;
};

template <typename T>
concept spi_port = has_spi<T> && requires(T t) {
  t.mosi;
  t.miso;
  t.sclk;
};

template <typename T>
concept uart_port = has_uart<T> && requires(T t) {
  t.rx;
  t.tx;
};

template <typename T>
concept rtc_port = has_rtc<T> && requires(T t) { t.rtc; };

template <typename T>
concept watchdog_port = has_watchdog<T> && requires(T t) { t.watchdog; };
}
