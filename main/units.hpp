#pragma once

constexpr float CelsiusToFahrenheit(float c) {
    return c * 9.0 / 5.0 + 32;
}

constexpr float FahrenheitToCelsius(float f) {
    return (f - 32) * 5.0 / 9.0;
}