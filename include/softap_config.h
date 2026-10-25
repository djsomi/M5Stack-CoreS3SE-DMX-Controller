#pragma once
#ifndef DMX_WIFI_MAX_STATIONS
#define DMX_WIFI_MAX_STATIONS 4
#endif
// Hardware supports 10, but the installed DHCP server has eight lease slots.
static_assert(DMX_WIFI_MAX_STATIONS >= 1 && DMX_WIFI_MAX_STATIONS <= 8,
              "DMX_WIFI_MAX_STATIONS must fit the installed DHCP lease capacity (1..8)");
