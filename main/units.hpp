#pragma once

constexpr int CelsiusToFahrenheit(int c) {
    return c * 9 / 5 + 32;
}

constexpr int FahrenheitToCelsius(int f) {
    return (f - 32) * 5 / 9;
}