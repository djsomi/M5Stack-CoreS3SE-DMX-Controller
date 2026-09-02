#pragma once

#include <driver/gpio.h>

#if defined(DMX_HARDWARE_UNIT) && defined(DMX_HARDWARE_BASE)
#error "Select only one DMX hardware profile"
#endif

#if !defined(DMX_HARDWARE_UNIT) && !defined(DMX_HARDWARE_BASE)
#error "Select a DMX hardware profile"
#endif

#if defined(DMX_HARDWARE_UNIT)

static constexpr int DMX_TX_PIN = GPIO_NUM_2;
static constexpr int DMX_RX_PIN = GPIO_NUM_1;
static constexpr int DMX_EN_PIN = -1;

#elif defined(DMX_HARDWARE_BASE)

static constexpr int DMX_TX_PIN = GPIO_NUM_7;
static constexpr int DMX_RX_PIN = GPIO_NUM_10;
static constexpr int DMX_EN_PIN = GPIO_NUM_6;

#endif
