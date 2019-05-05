#pragma once

#include <math.h>

constexpr int CelsiusToFahrenheit(int c) {
    return round(c * 9.0 / 5.0 + 32);
}

constexpr int FahrenheitToCelsius(int f) {
    return round((f - 32) * 5.0 / 9.0);
}