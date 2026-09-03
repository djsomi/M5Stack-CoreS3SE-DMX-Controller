// VERIFIED PRESET-8 BUILD - 2026-08-07
#pragma once

#include "common.h"
#include "logo_sender.h"
#include "preset_store.h"
#include <Preferences.h>

// Display settings are owned by src.cpp because the global safe-wake /
// auto-off logic lives there.
extern uint8_t g_display_brightness_level;
extern uint8_t g_screen_timeout_minutes;
extern uint8_t g_ui_volume_level;
extern uint8_t g_ui_speaker_volume;
extern uint8_t g_display_normal_brightness;
extern uint32_t g_display_timeout_ms;

void applyDisplayBrightnessLevel(uint8_t level);
void setScreenTimeoutMinutes(uint8_t minutes);
void applyUiVolumeLevel(uint8_t level);
void playBatteryWarningTone(uint16_t frequency, uint16_t duration_ms);

void controllerRestart(void);
void controllerPowerOff(void);


class view_sender_t : public view_t {
   public:
    void setup(void) override {
#if ESP_DMX_VERSION == 1
        dmx_set_mode(dmxPort, DMX_MODE_TX);
#else
        dmx_set_mode(dmxPort, DMX_MODE_WRITE);
#endif
        memset(data, 0, sizeof(data));
        dmx_write_packet(dmxPort, data[0], DMX_MAX_PACKET_SIZE);

        M5.Display.setFont(&fonts::Font0);
        M5.Display.setTextDatum(textdatum_t::top_left);
        M5.Display.setTextWrap(false);
        M5.Display.fillScreen(TFT_BLACK);

        enable_channel_count = 512;
        for (int i = 0; i < enable_channel_count; ++i) {
            int ch             = i + 1;
            enable_channels[i] = ch;
            visible[ch]        = 1;
            // data[0][ch] = rand();
            // data[1][ch] = data[0][ch];
        }
        target_channel = enable_channels[target_channel_index];

        updateDisplay(true);
        hideUIValueSet();
    }

    bool loop(void) override {
#if ESP_DMX_VERSION == 1
        if (ESP_ERR_TIMEOUT != dmx_wait_tx_done(dmxPort, 0)) {
            dmx_tx_packet(dmxPort);
            dmx_last_activity_ms = millis();
        }
#else
        if (ESP_ERR_TIMEOUT != dmx_wait_send_done(dmxPort, 0)) {
            dmx_send_packet(dmxPort, DMX_MAX_PACKET_SIZE);
            dmx_last_activity_ms = millis();
        }
#endif

        // Keep battery ETA/warnings active in every Sender UI screen,
        // including the full-screen preset manager.
        updateBatteryMonitor();

        bool modified     = false;
        bool btnB_Clicked = M5.BtnB.wasClicked();

        auto tp = M5.Touch.getDetail();

        // Full-screen SETTINGS has exclusive touch control.
        // DMX transmission at the top of loop() continues normally.
        if (settings_screen) {
            handleSettingsTouch(tp);

            // Continue a flick after finger release, just like the main
            // DMX channel list, with gradual deceleration.
            serviceSettingsKineticScroll(tp.state);

            // Expire RESTART / POWER OFF first-tap protection after 500 ms.
            updateSettingsPowerArmState();

            // If a Settings frame was deferred to protect an active tone,
            // render it immediately after the speaker becomes idle.
            if (settings_redraw_pending &&
                !M5.Speaker.isPlaying()) {

                drawSettingsScreen();
            }

            // Keep the complete battery indicator live on SETTINGS too.
            // Skip the direct LCD overlay while a full Settings frame is
            // waiting, otherwise it could briefly appear over an old page.
            if (!settings_redraw_pending) {
                drawBatteryStatus();
            }

            delay(1);
            return true;
        }


        // Full-screen preset manager has exclusive touch control.
        // DMX transmission at the top of loop() continues normally.
        if (preset_screen) {

            // --------------------------------------------------
            // PRESET OPTIONS MODAL
            // --------------------------------------------------
            if (preset_options_screen) {

                // Consume the release of the long-press that opened
                // the modal so it cannot accidentally press an option.
                if (preset_options_consume_release) {
                    if (!tp.state) {
                        preset_options_consume_release = false;
                    }

                    delay(1);
                    return true;
                }

                // CLEAR is protected by a 1-second hold.
                if (handlePresetClearHold(tp)) {
                    modified = true;
                }

                // COPY / CLEAR tap hint / BACK.
                if (preset_options_screen &&
                    tp.state &&
                    tp.wasClicked()) {

                    handlePresetOptionsTouch(
                        tp.base_x,
                        tp.base_y
                    );

                    modified = true;
                }

                if (!modified) {
                    delay(1);
                }

                return true;
            }


            // --------------------------------------------------
            // NORMAL PRESET GRID
            // --------------------------------------------------

            // Long-pressing an occupied P1-P8 opens COPY/CLEAR options.
            if (!preset_copy_mode &&
                handlePresetSlotLongPress(tp)) {
                modified = true;
            }

            // SAVE uses a protected ~1 second hold gesture.
            if (handlePresetSaveHold(tp)) {
                modified = true;
            }

            // Normal taps still handle P1-P8, LOAD and BACK.
            // SAVE itself is intentionally ignored here and can only
            // be activated by the hold handler above.
            if (!preset_options_screen &&
                tp.state &&
                tp.wasClicked()) {

                handlePresetScreenTouch(
                    tp.base_x,
                    tp.base_y
                );

                modified = true;
            }

            if (!modified) {
                delay(1);
            }

            return true;
        }

        if (tp.state) {
            if (tp.base_y < scroll_height &&
                tp.base_x <
                    scroll_width) {  /// スクロールエリア内でのタッチ操作の処理;
                auto dy      = tp.deltaY();
                scroll_y_add = -dy;
                if (dy && ui_mode == ui_mode_t::mode_value_setting) {
                    setUIMode(ui_mode_t::mode_channel_select);
                }

                if (tp.wasClicked()) {  /// タップ操作でチャンネル選択;
                    int new_channel_index =
                        (tp.x / channel_item_width) +
                        ((tp.y + scroll_y) / channel_item_height) *
                            channel_item_cols;

                    if (new_channel_index < 0) {
                        new_channel_index = 0;
                    } else if (new_channel_index >= enable_channel_count) {
                        new_channel_index = enable_channel_count - 1;
                    } else if (target_channel_index != new_channel_index) {
                        target_channel_index = new_channel_index;
                        target_channel = enable_channels[new_channel_index];
                        target_value   = data[data_idx][target_channel];
                        modified       = true;
                        ui_mode        = ui_mode_t::mode_channel_select;
                        setUIMode(ui_mode_t::mode_value_setting);
                    } else {
                        setUIMode(ui_mode == ui_mode_t::mode_channel_select
                                      ? ui_mode_t::mode_value_setting
                                      : ui_mode_t::mode_channel_select);
                    }
                    updateDisplay(true);
                }
            } else if (tp.base_x >= scroll_width &&
                       tp.base_y < M5.Display.height()) {
                if (ui_mode == ui_mode_t::mode_channel_select) {
                    if (tp.wasClicked()) {

                        // SETTINGS
                        if (tp.base_x >= settings_open_x &&
                            tp.base_x < settings_open_x + settings_open_w &&
                            tp.base_y >= settings_open_y &&
                            tp.base_y < settings_open_y + settings_open_h) {

                            openSettingsScreen();
                            modified = true;
                        }

                        // BLACKOUT - double-tap protected.
                        else if (tp.base_x >= blackout_open_x &&
                                 tp.base_x < blackout_open_x + blackout_open_w &&
                                 tp.base_y >= blackout_open_y &&
                                 tp.base_y < blackout_open_y + blackout_open_h) {

                            handleBlackoutTap();
                            modified = true;
                        }

                        // PRESET
                        else if (tp.base_x >= preset_open_x &&
                                 tp.base_x < preset_open_x + preset_open_w &&
                                 tp.base_y >= preset_open_y &&
                                 tp.base_y < preset_open_y + preset_open_h) {

                            openPresetScreen();
                            modified = true;
                        }
                    }
                } else if (ui_mode == ui_mode_t::mode_value_setting) {
                    int new_value = target_value;
                    if (tp.base_y >= slider_y && tp.base_y - 128 < slider_y) {
                        new_value = 255 - ((tp.y - slider_y) << 1);
                    } else if (tp.wasPressed() || tp.isHolding()) {
                        new_value += (tp.base_y < slider_y) ? 1 : -1;
                    }

                    if (new_value < 0) {
                        new_value = 0;
                    } else if (new_value > 255) {
                        new_value = 255;
                    }
                    if (target_value != new_value) {
                        drawUIValueSet(new_value);
                    }
                }
            }
        }

        switch (ui_mode) {
            case mode_channel_select: {
                int new_channel_index = target_channel_index;
                if (M5.BtnA.wasPressed() || M5.BtnA.isHolding()) {
                    --new_channel_index;
                }
                if (M5.BtnC.wasPressed() || M5.BtnC.isHolding()) {
                    ++new_channel_index;
                }
                if (new_channel_index != target_channel_index) {
                    if (new_channel_index < 0) {
                        new_channel_index = 0;
                    } else if (new_channel_index >= enable_channel_count) {
                        new_channel_index = enable_channel_count - 1;
                    }

                    target_channel_index = new_channel_index;
                    int new_channel      = enable_channels[new_channel_index];
                    if (target_channel != new_channel) {
                        target_channel = new_channel;
                        target_value   = data[data_idx][new_channel];
                        if (!scrollToTargetItem()) {
                            updateDisplay(true);
                        }
                    }
                    modified = true;
                }
                if (btnB_Clicked) {
                    setUIMode(ui_mode_t::mode_value_setting);
                    modified = true;
                }
            } break;

            case mode_value_setting:

            {
                int new_value = target_value;
                if (M5.BtnA.wasPressed() || M5.BtnA.isHolding()) {
                    --new_value;
                }
                if (M5.BtnC.wasPressed() || M5.BtnC.isHolding()) {
                    ++new_value;
                }
                if (new_value != target_value) {
                    if (new_value <= 0) {
                        new_value = 0;
                    } else if (new_value > 255) {
                        new_value = 255;
                    }
                    drawUIValueSet(new_value);
                }
                if (btnB_Clicked) {
                    setUIMode(ui_mode_t::mode_channel_select);
                }
            }
        }

        if (target_value != data[data_idx][target_channel]) {
            data_idx                       = 1 - data_idx;
            data[data_idx][target_channel] = target_value;
            writeCurrentDmxOutput();
            updateDisplay();
            modified                           = true;
            data[1 - data_idx][target_channel] = target_value;
        }

        if (scroll_y_add) {
            int new_y = scroll_y + scroll_y_add;
            scroll_y_add += (scroll_y_add < 0) ? 1 : -1;
            if (new_y < 0) {
                new_y = 0;
            }
            static constexpr int scroll_liimt =
                channel_item_height *
                    ((511 + channel_item_cols) / channel_item_cols) -
                scroll_height;
            if (new_y > scroll_liimt) {
                new_y = scroll_liimt;
            }
            if (scroll_y != new_y) {
                scroll_y = new_y;
                updateDisplay(true);
            }
        }

        // Refresh battery indicator only on the normal channel-select screen.
        // The preset screen returns earlier, and value-setting mode uses this
        // right-hand area for the slider.
        if (!settings_screen &&
            !preset_screen &&
            ui_mode == ui_mode_t::mode_channel_select) {

            drawBatteryStatus();
            updateBlackoutArmState();
            drawDmxActivityIndicator();
        }

        if (!modified) {
            delay(1);
        }

        return true;
    }

    void close(void) override {
        for (auto& tmp : canvas) {
            tmp.deleteSprite();
        }
    }


   private:
    static constexpr int BAR_HEIGHT = 64;
    static constexpr int CONSOLE_Y  = 88;

    // ==========================
    // PRESET STORAGE
    // ==========================
    PresetStore presetStore;

    bool preset_screen      = false;
    uint8_t selected_preset = 0;
    char preset_status[32]  = "";

    size_t data_idx = 0;
    M5Canvas canvas[2];
    bool canvas_flip;

    // Full-screen off-screen buffer for SETTINGS.
    // The complete frame is composed here first, then pushed to the LCD
    // in one operation. This prevents visible black/redraw flashing.
    M5Canvas settings_canvas;
    bool settings_canvas_ready = false;
    bool settings_redraw_pending = false;

    uint16_t enable_channels[DMX_MAX_PACKET_SIZE];
    uint16_t enable_channel_count;

    uint8_t visible[DMX_MAX_PACKET_SIZE];
    uint8_t data[2][DMX_MAX_PACKET_SIZE];
    uint16_t target_channel_index = 0;
    uint16_t target_channel;
    int16_t target_value                       = 0;
    int16_t scroll_y                           = 0;
    int16_t scroll_y_add                       = 0;
    static constexpr uint16_t scroll_width     = 250;
    static constexpr uint8_t scroll_height     = 240;
    static constexpr uint8_t channel_item_cols = 5;
    static constexpr uint8_t channel_item_width =
        scroll_width / channel_item_cols;
    static constexpr uint8_t channel_item_height = 54;
    static constexpr uint16_t slider_x           = scroll_width + 28;
    static constexpr uint8_t slider_w            = 16;
    static constexpr uint8_t slider_y            = 72;  // 40;
    static constexpr uint8_t slider_btn_height   = 32;

    // ==========================
    // PRESET UI
    // ==========================
    // Right-side control stack on the normal DMX channel screen.
    //
    // Battery
    // SETTINGS
    // BLACKOUT
    // PRESET
    // DMX heartbeat
    static constexpr int side_button_x = scroll_width + 5;
    static constexpr int side_button_w = 320 - scroll_width - 10;
    static constexpr int side_button_h = 50;

    static constexpr int settings_open_x = side_button_x;
    static constexpr int settings_open_y = 70;
    static constexpr int settings_open_w = side_button_w;
    static constexpr int settings_open_h = side_button_h;

    static constexpr int blackout_open_x = side_button_x;
    static constexpr int blackout_open_y = 126;
    static constexpr int blackout_open_w = side_button_w;
    static constexpr int blackout_open_h = side_button_h;

    static constexpr int preset_open_x = side_button_x;
    static constexpr int preset_open_y = 182;
    static constexpr int preset_open_w = side_button_w;
    static constexpr int preset_open_h = side_button_h;

    // Full-screen P1-P8 grid.
    static constexpr int preset_grid_x = 8;
    static constexpr int preset_grid_y = 38;
    static constexpr int preset_slot_w = 72;
    static constexpr int preset_slot_h = 46;
    static constexpr int preset_slot_gap_x = 5;
    static constexpr int preset_slot_gap_y = 9;

    // Bottom action buttons.
    static constexpr int preset_action_y = 184;
    static constexpr int preset_action_h = 46;
    static constexpr int preset_action_w = 98;
    static constexpr int preset_action_gap = 5;
    static constexpr int preset_action_x0 = 8;

    // ==========================
    // BLACKOUT
    // ==========================

    static constexpr uint32_t blackout_double_tap_ms = 500;

    bool blackout_active = false;
    bool blackout_tap_armed = false;
    uint32_t blackout_first_tap_ms = 0;

    uint8_t blackout_packet[DMX_MAX_PACKET_SIZE] = {};
    uint8_t blackout_restore[DMX_MAX_PACKET_SIZE] = {};


    // ==========================
    // SETTINGS SCREEN
    // ==========================

    bool settings_screen = false;

    // Settings are drawn in a vertically scrollable content area.
    // More settings can simply be added lower down later.
    static constexpr int settings_content_h = 300;
    static constexpr int settings_view_h = 240;
    static constexpr int settings_scroll_max =
        settings_content_h - settings_view_h;

    int settings_scroll_y = 0;

    // Momentum/inertia value. Same lightweight concept as scroll_y_add
    // on the main DMX channel page.
    int16_t settings_scroll_add = 0;

    enum settings_drag_t {
        settings_drag_none,

        // A touch started on a slider, but we wait briefly to determine
        // whether the user means horizontal adjustment or vertical scroll.
        settings_drag_pending_brightness,
        settings_drag_pending_timeout,
        settings_drag_pending_volume,

        settings_drag_scroll,
        settings_drag_brightness,
        settings_drag_timeout,
        settings_drag_volume,
    };

    settings_drag_t settings_drag_mode =
        settings_drag_none;

    // Sliders stay in the left/main control area. The top-right corner
    // is reserved for the fixed close button.
    static constexpr int settings_slider_x = 22;
    static constexpr int settings_slider_w = 220;
    static constexpr int settings_content_center_x =
        settings_slider_x + (settings_slider_w >> 1);

    static constexpr int settings_slider_touch_x0 = 8;
    static constexpr int settings_slider_touch_x1 = 258;

    // Gesture direction threshold.
    static constexpr int settings_gesture_threshold = 9;

    // Fixed BACK button: exactly the same size/alignment as the
    // PRESET button on the main Sender screen.
    static constexpr int settings_back_x = preset_open_x;
    static constexpr int settings_back_y = preset_open_y;
    static constexpr int settings_back_w = preset_open_w;
    static constexpr int settings_back_h = preset_open_h;

    // Fixed software power controls above BACK.
    static constexpr int settings_restart_x = settings_back_x;
    static constexpr int settings_restart_y = settings_back_y - 112;
    static constexpr int settings_restart_w = settings_back_w;
    static constexpr int settings_restart_h = settings_back_h;

    static constexpr int settings_poweroff_x = settings_back_x;
    static constexpr int settings_poweroff_y = settings_back_y - 56;
    static constexpr int settings_poweroff_w = settings_back_w;
    static constexpr int settings_poweroff_h = settings_back_h;

    static constexpr uint32_t settings_power_double_tap_ms = 500;

    bool settings_restart_tap_armed = false;
    uint32_t settings_restart_first_tap_ms = 0;

    bool settings_poweroff_tap_armed = false;
    uint32_t settings_poweroff_first_tap_ms = 0;

    // Content-space Y coordinates.
    static constexpr int brightness_label_y = 50;
    static constexpr int brightness_slider_y = 78;

    static constexpr int timeout_label_y = 128;
    static constexpr int timeout_slider_y = 156;

    static constexpr int volume_label_y = 206;
    static constexpr int volume_slider_y = 234;

    // Automatic firmware build timestamp at the end of the scrollable page.
    static constexpr int build_timestamp_y = 282;

    static constexpr int settings_slider_touch_h = 34;


    // ==========================
    // BATTERY UI / MONITORING
    // ==========================

    // Screen update interval. 500 ms also gives a clear low-battery flash.
    static constexpr uint32_t battery_refresh_ms = 500;

    // Runtime estimator:
    // - sample about once per minute
    // - do not show an estimate until at least 10 minutes have elapsed
    // - require at least 2% observed discharge
    // - restart the measurement window every 20 minutes
    static constexpr uint32_t runtime_sample_ms = 60UL * 1000UL;
    static constexpr uint32_t runtime_min_observation_ms =
        10UL * 60UL * 1000UL;
    static constexpr uint32_t runtime_window_ms =
        20UL * 60UL * 1000UL;

    uint32_t last_battery_update_ms = 0;
    uint32_t last_battery_monitor_ms = 0;

    int battery_level_now = -1;
    bool battery_charging_now = false;
    bool battery_external_power_now = false;

    int last_battery_level = -999;
    bool last_battery_charging = false;
    bool last_battery_external_power = false;
    bool last_battery_flash_on = true;
    bool battery_state_valid = false;

    // Runtime estimation state.
    bool runtime_window_active = false;
    uint32_t runtime_window_start_ms = 0;
    uint32_t last_runtime_sample_ms = 0;
    int runtime_window_start_level = -1;
    int runtime_estimate_minutes = -1;

    // Low-battery warning latches. They reset when external power/charging
    // is detected, so each new discharge cycle can warn once again.
    bool warning_20_sent = false;
    bool warning_10_sent = false;
    int last_warning_battery_level = -1;

    // ==========================
    // DMX ACTIVITY INDICATOR
    // ==========================
    // Blink every 250 ms while frames are flowing.
    // If no completed/new frame is seen for 500 ms, show red.
    static constexpr uint32_t dmx_indicator_refresh_ms = 100;
    static constexpr uint32_t dmx_active_timeout_ms = 500;

    uint32_t dmx_last_activity_ms = 0;
    uint32_t dmx_indicator_last_draw_ms = 0;
    bool dmx_indicator_last_on = false;
    bool dmx_indicator_last_active = false;
    bool dmx_indicator_state_valid = false;

    // ==========================
    // PRESET SAVE PROTECTION
    // ==========================
    static constexpr uint32_t preset_save_hold_ms = 1000;

    bool preset_save_holding = false;
    bool preset_save_completed = false;
    uint32_t preset_save_hold_start_ms = 0;
    int preset_save_last_progress = -1;

    // ==========================
    // PRESET COPY / CLEAR
    // ==========================

    // Long-press an occupied preset slot to open its options.
    static constexpr uint32_t preset_slot_long_press_ms = 800;

    bool preset_slot_holding = false;
    uint8_t preset_slot_hold_slot = 0;
    uint32_t preset_slot_hold_start_ms = 0;

    // Options modal state.
    bool preset_options_screen = false;
    bool preset_options_consume_release = false;
    uint8_t preset_options_slot = 0;

    // COPY workflow: after choosing COPY, tap an empty target slot.
    bool preset_copy_mode = false;
    uint8_t preset_copy_source = 0;

    // CLEAR is destructive, so require a protected 1-second hold.
    static constexpr uint32_t preset_clear_hold_ms = 1000;

    bool preset_clear_holding = false;
    bool preset_clear_completed = false;
    uint32_t preset_clear_hold_start_ms = 0;
    int preset_clear_last_progress = -1;

    // Persistent metadata mask for slots that the UI treats as cleared.
    // This keeps compatibility with the existing PresetStore implementation.
    // Saving/copying into a cleared slot removes its bit again.
    bool preset_meta_loaded = false;
    uint8_t preset_cleared_mask = 0;

    // Options-screen button geometry.
    static constexpr int preset_option_y = 166;
    static constexpr int preset_option_h = 54;
    static constexpr int preset_option_w = 98;
    static constexpr int preset_option_gap = 5;
    static constexpr int preset_option_x0 = 8;

    int16_t circle_ui_x  = 160;
    int16_t circle_ui_y  = 160;
    int16_t circle_ui_r0 = 40;
    int16_t circle_ui_r1 = 72;

    enum ui_mode_t {
        mode_channel_select,
        mode_value_setting,
    };

    ui_mode_t ui_mode = mode_channel_select;

// ==========================
// PRESET FUNCTIONS
// ==========================

void loadPresetMeta(void) {
    if (preset_meta_loaded) {
        return;
    }

    Preferences prefs;

    if (prefs.begin("dmxmeta", true)) {
        preset_cleared_mask =
            prefs.getUChar("cleared", 0);

        prefs.end();
    } else {
        preset_cleared_mask = 0;
    }

    preset_meta_loaded = true;
}


void savePresetMeta(void) {
    Preferences prefs;

    if (prefs.begin("dmxmeta", false)) {
        prefs.putUChar(
            "cleared",
            preset_cleared_mask
        );

        prefs.end();
    }
}


bool presetExists(uint8_t slot) {
    if (slot >= PresetStore::PRESET_COUNT) {
        return false;
    }

    loadPresetMeta();

    uint8_t bit =
        static_cast<uint8_t>(1U << slot);

    if (preset_cleared_mask & bit) {
        return false;
    }

    return presetStore.exists(slot);
}


void markPresetCleared(
    uint8_t slot,
    bool cleared
) {
    if (slot >= PresetStore::PRESET_COUNT) {
        return;
    }

    loadPresetMeta();

    uint8_t bit =
        static_cast<uint8_t>(1U << slot);

    uint8_t old_mask = preset_cleared_mask;

    if (cleared) {
        preset_cleared_mask |= bit;
    } else {
        preset_cleared_mask &= ~bit;
    }

    if (preset_cleared_mask != old_mask) {
        savePresetMeta();
    }
}


bool clearPreset(uint8_t slot) {
    if (slot >= PresetStore::PRESET_COUNT) {
        return false;
    }

    if (!presetExists(slot)) {
        snprintf(
            preset_status,
            sizeof(preset_status),
            "P%u IS ALREADY EMPTY",
            static_cast<unsigned int>(slot + 1)
        );

        return false;
    }

    // Persistently mark this slot as empty.
    // The existing preset bytes are left untouched for compatibility,
    // but load/exists ignore them until this slot is saved again.
    markPresetCleared(slot, true);

    snprintf(
        preset_status,
        sizeof(preset_status),
        "PRESET P%u CLEARED",
        static_cast<unsigned int>(slot + 1)
    );

    return true;
}


bool copyPreset(
    uint8_t source,
    uint8_t target
) {
    if (source >= PresetStore::PRESET_COUNT ||
        target >= PresetStore::PRESET_COUNT) {
        return false;
    }

    if (!presetExists(source)) {
        snprintf(
            preset_status,
            sizeof(preset_status),
            "P%u IS EMPTY",
            static_cast<unsigned int>(source + 1)
        );

        return false;
    }

    if (source == target) {
        snprintf(
            preset_status,
            sizeof(preset_status),
            "CHOOSE ANOTHER SLOT"
        );

        return false;
    }

    // Deliberately do not overwrite an occupied preset via COPY.
    if (presetExists(target)) {
        snprintf(
            preset_status,
            sizeof(preset_status),
            "P%u OCCUPIED - CLEAR FIRST",
            static_cast<unsigned int>(target + 1)
        );

        return false;
    }

    uint8_t packet[DMX_MAX_PACKET_SIZE] = {};

    if (!presetStore.load(source, packet)) {
        snprintf(
            preset_status,
            sizeof(preset_status),
            "COPY SOURCE READ FAILED"
        );

        return false;
    }

    packet[0] = 0;

    bool success =
        presetStore.save(target, packet);

    if (success) {
        // Reactivate the target in case it had previously been cleared.
        markPresetCleared(target, false);

        snprintf(
            preset_status,
            sizeof(preset_status),
            "P%u COPIED TO P%u",
            static_cast<unsigned int>(source + 1),
            static_cast<unsigned int>(target + 1)
        );
    } else {
        snprintf(
            preset_status,
            sizeof(preset_status),
            "COPY FAILED"
        );
    }

    return success;
}


bool savePreset(uint8_t slot) {
    if (slot >= PresetStore::PRESET_COUNT) {
        return false;
    }

    bool success = presetStore.save(slot, data[data_idx]);

    if (success) {
        // Saving into a previously cleared slot reactivates it.
        markPresetCleared(slot, false);
    }

    snprintf(
        preset_status,
        sizeof(preset_status),
        success ? "PRESET P%u SAVED" : "SAVE FAILED",
        static_cast<unsigned int>(slot + 1)
    );

    return success;
}


bool loadPreset(uint8_t slot) {
    if (slot >= PresetStore::PRESET_COUNT) {
        return false;
    }

    uint8_t restoredPacket[DMX_MAX_PACKET_SIZE] = {};

    if (!presetExists(slot) ||
        !presetStore.load(slot, restoredPacket)) {
        snprintf(
            preset_status,
            sizeof(preset_status),
            "PRESET P%u IS EMPTY",
            static_cast<unsigned int>(slot + 1)
        );

        return false;
    }

    // DMX start code
    restoredPacket[0] = 0;

    // Copy loaded preset into both working buffers
    memcpy(
        data[0],
        restoredPacket,
        DMX_MAX_PACKET_SIZE
    );

    memcpy(
        data[1],
        restoredPacket,
        DMX_MAX_PACKET_SIZE
    );

    data_idx = 0;

    // Update selected channel value
    target_value =
        data[data_idx][target_channel];

    // Send loaded preset to DMX driver.
    // While BLACKOUT is active, the output remains zero.
    esp_err_t result =
        writeCurrentDmxOutput();

    if (result != ESP_OK) {
        snprintf(
            preset_status,
            sizeof(preset_status),
            "DMX WRITE FAILED"
        );

        return false;
    }

    snprintf(
        preset_status,
        sizeof(preset_status),
        "PRESET P%u LOADED",
        static_cast<unsigned int>(slot + 1)
    );

    return true;
}


    void setUIMode(ui_mode_t new_mode) {
        if (ui_mode == new_mode) {
            return;
        }
        ui_mode = new_mode;
        switch (new_mode) {
            case ui_mode_t::mode_channel_select:
                // M5.Speaker.tone(660, 80);
                hideUIValueSet();
                break;

            case ui_mode_t::mode_value_setting:
                // M5.Speaker.tone(880, 80);
                target_value = data[data_idx][target_channel];
                drawUIValueSet(target_value, true);
                scrollToTargetItem();
                break;

            default:
                break;
        }
        updateDisplay(true);
    }

    bool scrollToTargetItem(void) {
        int new_y =
            (target_channel_index / channel_item_cols) * channel_item_height;
        if (scroll_y > new_y) {
            scroll_y_add = 0;
            int target   = scroll_y - new_y;
            while (0 < (target += --scroll_y_add))
                ;
            return true;
        } else if (scroll_y + scroll_height - channel_item_height < new_y) {
            scroll_y_add = 0;
            int target =
                new_y - (scroll_y + scroll_height - channel_item_height);
            while (0 < (target -= ++scroll_y_add))
                ;
            return true;
        }
        return false;
    }

    void drawFocusBox(LovyanGFX* gfx, int x, int y, int w, int h, int fw) {
        int horizon  = w >> 2;
        int vertical = h >> 3;
        gfx->fillRect(x, y, horizon, fw);
        gfx->fillRect(x + w - horizon, y, horizon, fw);
        gfx->fillRect(x, y + h - fw, horizon, fw);
        gfx->fillRect(x + w - horizon, y + h - fw, horizon, fw);
        gfx->fillRect(x, y + fw, fw, vertical);
        gfx->fillRect(x + w - fw, y + fw, fw, vertical);
        gfx->fillRect(x, y + h - fw - vertical, fw, vertical);
        gfx->fillRect(x + w - fw, y + h - fw - vertical, fw, vertical);
    }

    // ========================================================
    // DMX OUTPUT / BLACKOUT
    // ========================================================

    esp_err_t writeCurrentDmxOutput(void) {
        if (blackout_active) {
            return dmx_write_packet(
                dmxPort,
                blackout_packet,
                DMX_MAX_PACKET_SIZE
            );
        }

        return dmx_write_packet(
            dmxPort,
            data[data_idx],
            DMX_MAX_PACKET_SIZE
        );
    }


    void drawSettingsButton(void) {
        const int center_x =
            settings_open_x +
            (settings_open_w >> 1);

        const int center_y =
            settings_open_y +
            (settings_open_h >> 1);

        M5.Display.drawRoundRect(
            settings_open_x,
            settings_open_y,
            settings_open_w,
            settings_open_h,
            6,
            TFT_WHITE
        );

        M5.Display.setTextDatum(
            textdatum_t::middle_center
        );

        M5.Display.setTextColor(
            TFT_WHITE,
            TFT_BLACK
        );

        M5.Display.setFont(&fonts::Font0);

        M5.Display.drawString(
            "SETTINGS",
            center_x,
            center_y
        );
    }


    void drawBlackoutButton(void) {
        const int center_x =
            blackout_open_x +
            (blackout_open_w >> 1);

        const int center_y =
            blackout_open_y +
            (blackout_open_h >> 1);

        // Clear only the button area before redrawing.
        M5.Display.fillRect(
            blackout_open_x,
            blackout_open_y,
            blackout_open_w,
            blackout_open_h,
            TFT_BLACK
        );

        M5.Display.setTextDatum(
            textdatum_t::middle_center
        );

        M5.Display.setFont(&fonts::Font0);

        if (blackout_active) {
            // Active BLACKOUT is unmistakably solid red.
            M5.Display.fillRoundRect(
                blackout_open_x,
                blackout_open_y,
                blackout_open_w,
                blackout_open_h,
                6,
                TFT_RED
            );

            M5.Display.setTextColor(
                TFT_WHITE,
                TFT_RED
            );

            M5.Display.drawString(
                "BLACKOUT",
                center_x,
                center_y
            );
        } else {
            M5.Display.drawRoundRect(
                blackout_open_x,
                blackout_open_y,
                blackout_open_w,
                blackout_open_h,
                6,
                TFT_RED
            );

            M5.Display.setTextColor(
                TFT_RED,
                TFT_BLACK
            );

            M5.Display.drawString(
                blackout_tap_armed
                    ? "TAP AGAIN"
                    : "BLACKOUT",
                center_x,
                center_y
            );
        }

        M5.Display.setTextColor(
            TFT_WHITE,
            TFT_BLACK
        );
    }


    void updateBlackoutArmState(void) {
        if (!blackout_tap_armed) {
            return;
        }

        uint32_t now = millis();

        if ((uint32_t)(
                now - blackout_first_tap_ms
            ) > blackout_double_tap_ms) {

            blackout_tap_armed = false;
            drawBlackoutButton();
        }
    }


    void toggleBlackout(void) {
        if (!blackout_active) {
            // Snapshot the exact current universe before forcing zero.
            memcpy(
                blackout_restore,
                data[data_idx],
                DMX_MAX_PACKET_SIZE
            );

            memset(
                blackout_packet,
                0,
                sizeof(blackout_packet)
            );

            blackout_packet[0] = 0;

            blackout_active = true;

            dmx_write_packet(
                dmxPort,
                blackout_packet,
                DMX_MAX_PACKET_SIZE
            );

            if (!M5.Speaker.isPlaying()) {
                M5.Speaker.tone(700, 180);
            }
        } else {
            // Restore the exact pre-blackout universe.
            memcpy(
                data[0],
                blackout_restore,
                DMX_MAX_PACKET_SIZE
            );

            memcpy(
                data[1],
                blackout_restore,
                DMX_MAX_PACKET_SIZE
            );

            data_idx = 0;

            target_value =
                data[data_idx][target_channel];

            blackout_active = false;

            dmx_write_packet(
                dmxPort,
                data[data_idx],
                DMX_MAX_PACKET_SIZE
            );

            updateDisplay(true);

            if (!M5.Speaker.isPlaying()) {
                M5.Speaker.tone(1800, 100);
            }
        }

        blackout_tap_armed = false;
        blackout_first_tap_ms = 0;

        drawBlackoutButton();
    }


    void handleBlackoutTap(void) {
        uint32_t now = millis();

        if (blackout_tap_armed &&
            (uint32_t)(
                now - blackout_first_tap_ms
            ) <= blackout_double_tap_ms) {

            toggleBlackout();
            return;
        }

        // First tap only arms the action.
        blackout_tap_armed = true;
        blackout_first_tap_ms = now;

        drawBlackoutButton();
    }


    // ========================================================
    // SETTINGS
    // ========================================================

    int sliderValueFromX(int x) {
        if (x < settings_slider_x) {
            x = settings_slider_x;
        }

        if (x >
            settings_slider_x +
                settings_slider_w) {

            x =
                settings_slider_x +
                settings_slider_w;
        }

        // Five equally spaced discrete positions.
        int step =
            settings_slider_w / 4;

        int relative =
            x - settings_slider_x;

        int value =
            ((relative + (step >> 1)) /
             step) +
            1;

        if (value < 1) value = 1;
        if (value > 5) value = 5;

        return value;
    }


    int volumeSliderValueFromX(int x) {
        if (x < settings_slider_x) {
            x = settings_slider_x;
        }

        if (x >
            settings_slider_x +
                settings_slider_w) {

            x =
                settings_slider_x +
                settings_slider_w;
        }

        // Five positions: 0,1,2,3,4.
        int step =
            settings_slider_w / 4;

        int relative =
            x - settings_slider_x;

        int value =
            (relative + (step >> 1)) /
            step;

        if (value < 0) value = 0;
        if (value > 4) value = 4;

        return value;
    }


    void drawVolumeSlider(
        LovyanGFX* gfx,
        int y,
        int value
    ) {
        if (value < 0) value = 0;
        if (value > 4) value = 4;

        int step =
            settings_slider_w / 4;

        gfx->fillRect(
            settings_slider_x,
            y - 2,
            settings_slider_w,
            4,
            TFT_DARKGRAY
        );

        int knob_x =
            settings_slider_x +
            value * step;

        if (knob_x > settings_slider_x) {
            gfx->fillRect(
                settings_slider_x,
                y - 2,
                knob_x - settings_slider_x,
                4,
                TFT_GREEN
            );
        }

        for (int i = 0; i < 5; ++i) {
            int x =
                settings_slider_x +
                i * step;

            uint16_t tick_color =
                (i <= value)
                    ? TFT_GREEN
                    : TFT_DARKGRAY;

            gfx->fillCircle(
                x,
                y,
                4,
                tick_color
            );

            gfx->setFont(
                &fonts::Font0
            );

            gfx->setTextDatum(
                textdatum_t::top_center
            );

            gfx->setTextColor(
                TFT_DARKGRAY,
                TFT_BLACK
            );

            gfx->drawNumber(
                i,
                x,
                y + 10
            );
        }

        gfx->fillCircle(
            knob_x,
            y,
            8,
            TFT_WHITE
        );

        gfx->fillCircle(
            knob_x,
            y,
            5,
            value == 0
                ? TFT_RED
                : TFT_GREEN
        );

        gfx->setTextDatum(
            textdatum_t::middle_center
        );
    }


    void drawSettingsSlider(
        LovyanGFX* gfx,
        int y,
        int value,
        bool timeout_slider = false
    ) {
        if (value < 1) value = 1;
        if (value > 5) value = 5;

        int step =
            settings_slider_w / 4;

        // Track.
        gfx->fillRect(
            settings_slider_x,
            y - 2,
            settings_slider_w,
            4,
            TFT_DARKGRAY
        );

        // Active part.
        int knob_x =
            settings_slider_x +
            (value - 1) * step;

        gfx->fillRect(
            settings_slider_x,
            y - 2,
            knob_x - settings_slider_x,
            4,
            TFT_GREEN
        );

        // Five discrete positions.
        for (int i = 0; i < 5; ++i) {
            int x =
                settings_slider_x +
                i * step;

            uint16_t tick_color =
                (i + 1 <= value)
                    ? TFT_GREEN
                    : TFT_DARKGRAY;

            gfx->fillCircle(
                x,
                y,
                4,
                tick_color
            );

            gfx->setFont(
                &fonts::Font0
            );

            gfx->setTextDatum(
                textdatum_t::top_center
            );

            gfx->setTextColor(
                TFT_DARKGRAY,
                TFT_BLACK
            );

            if (timeout_slider && i == 4) {
                gfx->drawString(
                    "ON",
                    x,
                    y + 10
                );
            } else {
                gfx->drawNumber(
                    i + 1,
                    x,
                    y + 10
                );
            }
        }

        // Knob.
        gfx->fillCircle(
            knob_x,
            y,
            8,
            TFT_WHITE
        );

        gfx->fillCircle(
            knob_x,
            y,
            5,
            TFT_GREEN
        );

        // Slider tick labels use top-center. Restore the normal settings
        // alignment so following labels/buttons stay correctly centered.
        gfx->setTextDatum(
            textdatum_t::middle_center
        );
    }


    void serviceSettingsKineticScroll(
        bool touch_active
    ) {
        // Finger still down: direct touch handling owns movement.
        if (touch_active) {
            return;
        }

        if (settings_scroll_add == 0) {
            return;
        }

        // Full-screen canvas transfer is relatively heavy. Do not push a
        // Settings frame while the speaker is actively generating a tone;
        // this avoids chopped/distorted short UI sounds.
        if (M5.Speaker.isPlaying()) {
            return;
        }

        int new_scroll =
            settings_scroll_y +
            settings_scroll_add;

        // Linear deceleration, matching the lightweight feel of the
        // main channel list.
        settings_scroll_add +=
            (settings_scroll_add < 0)
                ? 1
                : -1;

        if (new_scroll < 0) {
            new_scroll = 0;
            settings_scroll_add = 0;
        }

        if (new_scroll > settings_scroll_max) {
            new_scroll = settings_scroll_max;
            settings_scroll_add = 0;
        }

        if (new_scroll != settings_scroll_y) {
            settings_scroll_y = new_scroll;
            drawSettingsScreen();
        }
    }




    void drawBuildTimestamp(
        LovyanGFX* gfx,
        int screen_y
    ) {
        // Compiler-generated firmware build timestamp.
        //
        // __DATE__ = "Aug 13 2026"
        // __TIME__ = "16:19:42"
        //
        // Displayed as one Font0 footer line:
        // Build Date: 2026.08.13 - 16:19

        const char* build_date = __DATE__;
        const char* build_time = __TIME__;

        uint8_t month = 0;

        static const char* months[12] = {
            "Jan", "Feb", "Mar", "Apr",
            "May", "Jun", "Jul", "Aug",
            "Sep", "Oct", "Nov", "Dec"
        };

        for (uint8_t i = 0; i < 12; ++i) {
            if (strncmp(build_date, months[i], 3) == 0) {
                month = i + 1;
                break;
            }
        }

        int day =
            (build_date[4] == ' ')
                ? (build_date[5] - '0')
                : ((build_date[4] - '0') * 10 +
                   (build_date[5] - '0'));

        int year =
            (build_date[7] - '0') * 1000 +
            (build_date[8] - '0') * 100 +
            (build_date[9] - '0') * 10 +
            (build_date[10] - '0');

        char build_text[40];

        snprintf(
            build_text,
            sizeof(build_text),
            "Build Date: %04d.%02u.%02d - %.5s",
            year,
            static_cast<unsigned int>(month),
            day,
            build_time
        );

        gfx->setFont(&fonts::Font0);
        gfx->setTextDatum(
            textdatum_t::middle_center
        );

        gfx->setTextColor(
            TFT_DARKGRAY,
            TFT_BLACK
        );

        gfx->drawString(
            build_text,
            settings_content_center_x,
            screen_y
        );

        gfx->setTextColor(
            TFT_WHITE,
            TFT_BLACK
        );
    }


    void sendDmxZeroBeforePowerAction(void) {
        // Send several all-zero DMX frames before restart/power-off so
        // fixtures are not intentionally left at the last active values.
        uint8_t zero_packet[DMX_MAX_PACKET_SIZE] = {};
        zero_packet[0] = 0;

        for (int i = 0; i < 3; ++i) {
            dmx_write_packet(
                dmxPort,
                zero_packet,
                DMX_MAX_PACKET_SIZE
            );

#if ESP_DMX_VERSION == 1
            dmx_wait_tx_done(
                dmxPort,
                50
            );
#else
            dmx_wait_send_done(
                dmxPort,
                50
            );
#endif

            delay(10);
        }
    }


    void updateSettingsPowerArmState(void) {
        uint32_t now = millis();
        bool changed = false;

        if (settings_restart_tap_armed &&
            (uint32_t)(
                now - settings_restart_first_tap_ms
            ) > settings_power_double_tap_ms) {

            settings_restart_tap_armed = false;
            settings_restart_first_tap_ms = 0;
            changed = true;
        }

        if (settings_poweroff_tap_armed &&
            (uint32_t)(
                now - settings_poweroff_first_tap_ms
            ) > settings_power_double_tap_ms) {

            settings_poweroff_tap_armed = false;
            settings_poweroff_first_tap_ms = 0;
            changed = true;
        }

        if (changed) {
            drawSettingsScreen();
        }
    }


    void handleSettingsRestartTap(void) {
        uint32_t now = millis();

        if (settings_restart_tap_armed &&
            (uint32_t)(
                now - settings_restart_first_tap_ms
            ) <= settings_power_double_tap_ms) {

            settings_restart_tap_armed = false;
            settings_restart_first_tap_ms = 0;

            // Cancel the other armed action as well.
            settings_poweroff_tap_armed = false;
            settings_poweroff_first_tap_ms = 0;

            sendDmxZeroBeforePowerAction();
            controllerRestart();
            return;
        }

        settings_restart_tap_armed = true;
        settings_restart_first_tap_ms = now;

        settings_poweroff_tap_armed = false;
        settings_poweroff_first_tap_ms = 0;

        drawSettingsScreen();
    }


    void handleSettingsPowerOffTap(void) {
        uint32_t now = millis();

        if (settings_poweroff_tap_armed &&
            (uint32_t)(
                now - settings_poweroff_first_tap_ms
            ) <= settings_power_double_tap_ms) {

            settings_poweroff_tap_armed = false;
            settings_poweroff_first_tap_ms = 0;

            // Cancel the other armed action as well.
            settings_restart_tap_armed = false;
            settings_restart_first_tap_ms = 0;

            sendDmxZeroBeforePowerAction();
            controllerPowerOff();
            return;
        }

        settings_poweroff_tap_armed = true;
        settings_poweroff_first_tap_ms = now;

        settings_restart_tap_armed = false;
        settings_restart_first_tap_ms = 0;

        drawSettingsScreen();
    }


    void drawSettingsScreen(void) {
        // Protect short speaker tones from the relatively heavy full-screen
        // canvas transfer. The next UI loop will redraw once audio finishes.
        if (M5.Speaker.isPlaying()) {
            settings_redraw_pending = true;
            return;
        }

        // Allocate the Settings framebuffer once and reuse it.
        // 320x240 at RGB565 is about 150 KB and avoids repeated LCD clears.
        if (!settings_canvas_ready) {
            settings_canvas.setColorDepth(16);
            settings_canvas.createSprite(
                M5.Display.width(),
                M5.Display.height()
            );
            settings_canvas_ready = true;
        }

        auto& c = settings_canvas;

        // IMPORTANT: this clear happens OFF-SCREEN.
        // The physical LCD keeps showing the previous complete frame.
        c.fillScreen(TFT_BLACK);

        c.setTextDatum(
            textdatum_t::middle_center
        );

        c.setTextColor(
            TFT_WHITE,
            TFT_BLACK
        );

        // Helper converts content-space Y to visible screen-space Y.
        auto sy = [this](int content_y) {
            return content_y - settings_scroll_y;
        };

        // ----------------------------------------------------
        // TITLE
        // ----------------------------------------------------
        int title_y = sy(18);

        if (title_y > -20 && title_y < 260) {
            c.setFont(
                &fonts::AsciiFont8x16
            );

            c.drawString(
                "SETTINGS",
                settings_content_center_x,
                title_y
            );
        }

        c.setFont(&fonts::Font0);

        // ----------------------------------------------------
        // BRIGHTNESS
        // ----------------------------------------------------
        int bright_label_screen_y =
            sy(brightness_label_y);

        int bright_slider_screen_y =
            sy(brightness_slider_y);

        if (bright_label_screen_y > -20 &&
            bright_label_screen_y < 260) {

            char brightness_text[32];

            snprintf(
                brightness_text,
                sizeof(brightness_text),
                "BRIGHTNESS  %u / 5",
                static_cast<unsigned int>(
                    g_display_brightness_level
                )
            );

            c.drawString(
                brightness_text,
                settings_content_center_x,
                bright_label_screen_y
            );
        }

        if (bright_slider_screen_y > -30 &&
            bright_slider_screen_y < 250) {

            drawSettingsSlider(
                &c,
                bright_slider_screen_y,
                g_display_brightness_level,
                false
            );
        }

        // ----------------------------------------------------
        // SCREEN OFF
        // ----------------------------------------------------
        int timeout_label_screen_y =
            sy(timeout_label_y);

        int timeout_slider_screen_y =
            sy(timeout_slider_y);

        if (timeout_label_screen_y > -20 &&
            timeout_label_screen_y < 260) {

            char timeout_text[32];

            if (g_screen_timeout_minutes == 0) {
                snprintf(
                    timeout_text,
                    sizeof(timeout_text),
                    "SCREEN OFF  ALWAYS ON"
                );
            } else {
                snprintf(
                    timeout_text,
                    sizeof(timeout_text),
                    "SCREEN OFF  %u MIN",
                    static_cast<unsigned int>(
                        g_screen_timeout_minutes
                    )
                );
            }

            c.drawString(
                timeout_text,
                settings_content_center_x,
                timeout_label_screen_y
            );
        }

        int timeout_slider_value =
            (g_screen_timeout_minutes == 0)
                ? 5
                : g_screen_timeout_minutes;

        if (timeout_slider_screen_y > -30 &&
            timeout_slider_screen_y < 250) {

            drawSettingsSlider(
                &c,
                timeout_slider_screen_y,
                timeout_slider_value,
                true
            );
        }

        // ----------------------------------------------------
        // VOLUME
        // ----------------------------------------------------
        int volume_label_screen_y =
            sy(volume_label_y);

        int volume_slider_screen_y =
            sy(volume_slider_y);

        if (volume_label_screen_y > -20 &&
            volume_label_screen_y < 260) {

            char volume_text[32];

            snprintf(
                volume_text,
                sizeof(volume_text),
                "VOLUME  %u / 4",
                static_cast<unsigned int>(
                    g_ui_volume_level
                )
            );

            c.drawString(
                volume_text,
                settings_content_center_x,
                volume_label_screen_y
            );
        }

        if (volume_slider_screen_y > -30 &&
            volume_slider_screen_y < 250) {

            drawVolumeSlider(
                &c,
                volume_slider_screen_y,
                g_ui_volume_level
            );
        }

        // ----------------------------------------------------
        // BUILD DATE FOOTER
        // ----------------------------------------------------
        int build_timestamp_screen_y =
            sy(build_timestamp_y);

        if (build_timestamp_screen_y > -20 &&
            build_timestamp_screen_y < 260) {

            drawBuildTimestamp(
                &c,
                build_timestamp_screen_y
            );
        }

        // Battery is a fixed status element and must remain completely
        // identical to the normal Sender screen. Force a redraw here because
        // fillScreen() above erased it even if the battery state did not change.
        drawBatteryStatusTo(
            &c,
            true
        );


        // ----------------------------------------------------
        // FIXED POWER CONTROL BUTTONS
        // ----------------------------------------------------

        // RESTART
        c.fillRoundRect(
            settings_restart_x - 2,
            settings_restart_y - 2,
            settings_restart_w + 4,
            settings_restart_h + 4,
            7,
            TFT_BLACK
        );

        c.drawRoundRect(
            settings_restart_x,
            settings_restart_y,
            settings_restart_w,
            settings_restart_h,
            6,
            TFT_YELLOW
        );

        c.setFont(&fonts::Font0);
        c.setTextDatum(textdatum_t::middle_center);
        c.setTextColor(TFT_YELLOW, TFT_BLACK);

        c.drawString(
            settings_restart_tap_armed
                ? "TAP AGAIN"
                : "RESTART",
            settings_restart_x +
                (settings_restart_w >> 1),
            settings_restart_y +
                (settings_restart_h >> 1) - 1
        );


        // POWER OFF
        c.fillRoundRect(
            settings_poweroff_x - 2,
            settings_poweroff_y - 2,
            settings_poweroff_w + 4,
            settings_poweroff_h + 4,
            7,
            TFT_BLACK
        );

        c.drawRoundRect(
            settings_poweroff_x,
            settings_poweroff_y,
            settings_poweroff_w,
            settings_poweroff_h,
            6,
            TFT_RED
        );

        c.setTextColor(TFT_RED, TFT_BLACK);

        c.drawString(
            settings_poweroff_tap_armed
                ? "TAP AGAIN"
                : "POWER OFF",
            settings_poweroff_x +
                (settings_poweroff_w >> 1),
            settings_poweroff_y +
                (settings_poweroff_h >> 1) - 1
        );

        c.setTextColor(TFT_WHITE, TFT_BLACK);


        // ----------------------------------------------------
        // FIXED BACK BUTTON
        // ----------------------------------------------------
        //
        // Always visible in the bottom-right while settings scroll.
        // Clear behind it first so scrolling content never shows through.
        c.fillRoundRect(
            settings_back_x - 2,
            settings_back_y - 2,
            settings_back_w + 4,
            settings_back_h + 4,
            7,
            TFT_BLACK
        );

        c.drawRoundRect(
            settings_back_x,
            settings_back_y,
            settings_back_w,
            settings_back_h,
            6,
            TFT_WHITE
        );

        c.setFont(&fonts::Font0);
        c.setTextDatum(
            textdatum_t::middle_center
        );
        c.setTextColor(
            TFT_WHITE,
            TFT_BLACK
        );

        c.drawString(
            "BACK",
            settings_back_x +
                (settings_back_w >> 1),
            settings_back_y +
                (settings_back_h >> 1) - 1
        );

        // One completed-frame transfer to the LCD.
        // No physical fillScreen() occurs between scroll frames.
        M5.Display.startWrite();
        c.pushSprite(
            &M5.Display,
            0,
            0
        );
        M5.Display.endWrite();

        settings_redraw_pending = false;
    }


    void openSettingsScreen(void) {
        settings_screen = true;
        settings_scroll_y = 0;
        settings_scroll_add = 0;
        settings_redraw_pending = false;
        settings_drag_mode = settings_drag_none;

        settings_restart_tap_armed = false;
        settings_restart_first_tap_ms = 0;
        settings_poweroff_tap_armed = false;
        settings_poweroff_first_tap_ms = 0;

        blackout_tap_armed = false;

        drawSettingsScreen();
    }


    void closeSettingsScreen(void) {
        settings_screen = false;
        settings_scroll_add = 0;
        settings_redraw_pending = false;
        settings_drag_mode = settings_drag_none;

        settings_restart_tap_armed = false;
        settings_restart_first_tap_ms = 0;
        settings_poweroff_tap_armed = false;
        settings_poweroff_first_tap_ms = 0;

        M5.Display.fillScreen(TFT_BLACK);

        updateDisplay(true);
        hideUIValueSet();
    }


    void handleSettingsTouch(
        const decltype(M5.Touch.getDetail())& tp
    ) {
        int touch_x = tp.x;
        int touch_y = tp.y;
        int content_y =
            touch_y + settings_scroll_y;

        // ----------------------------------------------------
        // FIXED POWER CONTROL BUTTONS
        // ----------------------------------------------------
        if (tp.wasClicked() &&
            tp.base_x >= settings_restart_x &&
            tp.base_x <
                settings_restart_x + settings_restart_w &&
            tp.base_y >= settings_restart_y &&
            tp.base_y <
                settings_restart_y + settings_restart_h) {

            handleSettingsRestartTap();
            return;
        }

        if (tp.wasClicked() &&
            tp.base_x >= settings_poweroff_x &&
            tp.base_x <
                settings_poweroff_x + settings_poweroff_w &&
            tp.base_y >= settings_poweroff_y &&
            tp.base_y <
                settings_poweroff_y + settings_poweroff_h) {

            handleSettingsPowerOffTap();
            return;
        }


        // ----------------------------------------------------
        // FIXED BACK BUTTON
        // ----------------------------------------------------
        if (tp.wasClicked() &&
            tp.base_x >= settings_back_x &&
            tp.base_x <
                settings_back_x + settings_back_w &&
            tp.base_y >= settings_back_y &&
            tp.base_y <
                settings_back_y + settings_back_h) {

            closeSettingsScreen();
            return;
        }


        // ----------------------------------------------------
        // PRESS START
        // ----------------------------------------------------
        if (tp.wasPressed()) {
            // New touch immediately cancels old momentum.
            settings_scroll_add = 0;

            bool in_slider_x =
                touch_x >= settings_slider_touch_x0 &&
                touch_x <= settings_slider_touch_x1;

            if (in_slider_x &&
                content_y >=
                    brightness_slider_y -
                        (settings_slider_touch_h >> 1) &&
                content_y <=
                    brightness_slider_y +
                        (settings_slider_touch_h >> 1)) {

                settings_drag_mode =
                    settings_drag_pending_brightness;
            }
            else if (in_slider_x &&
                     content_y >=
                         timeout_slider_y -
                             (settings_slider_touch_h >> 1) &&
                     content_y <=
                         timeout_slider_y +
                             (settings_slider_touch_h >> 1)) {

                settings_drag_mode =
                    settings_drag_pending_timeout;
            }
            else if (in_slider_x &&
                     content_y >=
                         volume_slider_y -
                             (settings_slider_touch_h >> 1) &&
                     content_y <=
                         volume_slider_y +
                             (settings_slider_touch_h >> 1)) {

                settings_drag_mode =
                    settings_drag_pending_volume;
            }
            else {
                settings_drag_mode =
                    settings_drag_scroll;
            }
        }


        // ----------------------------------------------------
        // GESTURE DIRECTION LOCK
        // ----------------------------------------------------
        bool pending_slider =
            settings_drag_mode ==
                settings_drag_pending_brightness ||
            settings_drag_mode ==
                settings_drag_pending_timeout ||
            settings_drag_mode ==
                settings_drag_pending_volume;

        if (tp.state && pending_slider) {
            int dx =
                touch_x - tp.base_x;

            int dy =
                touch_y - tp.base_y;

            int abs_dx =
                (dx < 0) ? -dx : dx;

            int abs_dy =
                (dy < 0) ? -dy : dy;

            // Vertical intent wins -> turn it into page scrolling.
            if (abs_dy >= settings_gesture_threshold &&
                abs_dy > abs_dx) {

                settings_drag_mode =
                    settings_drag_scroll;
            }
            // Horizontal intent -> lock to slider.
            else if (
                abs_dx >= settings_gesture_threshold &&
                abs_dx >= abs_dy) {

                if (settings_drag_mode ==
                    settings_drag_pending_brightness) {

                    settings_drag_mode =
                        settings_drag_brightness;
                }
                else if (settings_drag_mode ==
                         settings_drag_pending_timeout) {

                    settings_drag_mode =
                        settings_drag_timeout;
                }
                else {
                    settings_drag_mode =
                        settings_drag_volume;
                }
            }
        }


        // ----------------------------------------------------
        // ACTIVE SLIDERS
        // ----------------------------------------------------
        if (tp.state &&
            settings_drag_mode ==
                settings_drag_brightness) {

            int value =
                sliderValueFromX(touch_x);

            if (value !=
                g_display_brightness_level) {

                applyDisplayBrightnessLevel(
                    static_cast<uint8_t>(value)
                );

                drawSettingsScreen();
            }

            return;
        }


        if (tp.state &&
            settings_drag_mode ==
                settings_drag_timeout) {

            int slider_value =
                sliderValueFromX(touch_x);

            uint8_t timeout_minutes =
                (slider_value == 5)
                    ? 0
                    : static_cast<uint8_t>(
                        slider_value
                    );

            if (timeout_minutes !=
                g_screen_timeout_minutes) {

                setScreenTimeoutMinutes(
                    timeout_minutes
                );

                drawSettingsScreen();
            }

            return;
        }


        if (tp.state &&
            settings_drag_mode ==
                settings_drag_volume) {

            int value =
                volumeSliderValueFromX(
                    touch_x
                );

            if (value !=
                g_ui_volume_level) {

                applyUiVolumeLevel(
                    static_cast<uint8_t>(value)
                );

                // Preview only if not muted.
                bool played_preview = false;

                if (value > 0 &&
                    !M5.Speaker.isPlaying()) {

                    M5.Speaker.tone(
                        2600,
                        35
                    );

                    played_preview = true;
                }

                // If a preview tone just started, skip the heavy full-screen
                // push this loop. The next loop will redraw after the tone.
                if (!played_preview) {
                    drawSettingsScreen();
                } else {
                    settings_redraw_pending = true;
                }
            }

            return;
        }


        // ----------------------------------------------------
        // DIRECT VERTICAL SCROLL + CAPTURE MOMENTUM
        // ----------------------------------------------------
        if (tp.state &&
            settings_drag_mode ==
                settings_drag_scroll) {

            int dy =
                tp.deltaY();

            if (dy != 0) {
                // Same sign convention as the main DMX channel list.
                settings_scroll_add = -dy;

                int new_scroll =
                    settings_scroll_y +
                    settings_scroll_add;

                if (new_scroll < 0) {
                    new_scroll = 0;
                    settings_scroll_add = 0;
                }

                if (new_scroll >
                    settings_scroll_max) {

                    new_scroll =
                        settings_scroll_max;

                    settings_scroll_add = 0;
                }

                if (new_scroll !=
                    settings_scroll_y) {

                    settings_scroll_y =
                        new_scroll;

                    drawSettingsScreen();
                }
            }
        }


        // ----------------------------------------------------
        // TAP WITHOUT DRAG ON A SLIDER
        // ----------------------------------------------------
        if (tp.wasClicked()) {

            if (settings_drag_mode ==
                settings_drag_pending_brightness) {

                int value =
                    sliderValueFromX(
                        tp.base_x
                    );

                if (value !=
                    g_display_brightness_level) {

                    applyDisplayBrightnessLevel(
                        static_cast<uint8_t>(value)
                    );

                    drawSettingsScreen();
                }

                settings_drag_mode =
                    settings_drag_none;

                return;
            }


            if (settings_drag_mode ==
                settings_drag_pending_timeout) {

                int slider_value =
                    sliderValueFromX(
                        tp.base_x
                    );

                uint8_t timeout_minutes =
                    (slider_value == 5)
                        ? 0
                        : static_cast<uint8_t>(
                            slider_value
                        );

                if (timeout_minutes !=
                    g_screen_timeout_minutes) {

                    setScreenTimeoutMinutes(
                        timeout_minutes
                    );

                    drawSettingsScreen();
                }

                settings_drag_mode =
                    settings_drag_none;

                return;
            }


            if (settings_drag_mode ==
                settings_drag_pending_volume) {

                int value =
                    volumeSliderValueFromX(
                        tp.base_x
                    );

                if (value !=
                    g_ui_volume_level) {

                    applyUiVolumeLevel(
                        static_cast<uint8_t>(value)
                    );

                    if (value > 0 &&
                        !M5.Speaker.isPlaying()) {

                        M5.Speaker.tone(
                            2600,
                            35
                        );
                    }

                    drawSettingsScreen();
                }

                settings_drag_mode =
                    settings_drag_none;

                return;
            }
        }


        // Release leaves settings_scroll_add intact so kinetic scrolling
        // can continue in serviceSettingsKineticScroll().
        if (!tp.state) {
            settings_drag_mode =
                settings_drag_none;
        }
    }


    void updateBatteryMonitor(bool force_update = false) {
        uint32_t now = millis();

        if (!force_update &&
            (uint32_t)(now - last_battery_monitor_ms) < battery_refresh_ms) {
            return;
        }

        last_battery_monitor_ms = now;

        int level = M5.Power.getBatteryLevel();
        bool charging = M5.Power.isCharging();
        int vbus_mv = M5.Power.getVBUSVoltage();
        bool external_power = (vbus_mv >= 4500);

        if (level > 100) {
            level = 100;
        }

        battery_level_now = level;
        battery_charging_now = charging;
        battery_external_power_now = external_power;

        // ------------------------------------------------------
        // EXTERNAL POWER / CHARGING
        // ------------------------------------------------------
        if (charging || external_power || level < 0) {
            // Runtime ETA is meaningful only while actually discharging.
            runtime_window_active = false;
            runtime_window_start_level = -1;
            runtime_estimate_minutes = -1;

            // A new charge/power cycle arms the warnings again.
            if (charging || external_power) {
                warning_20_sent = false;
                warning_10_sent = false;
                last_warning_battery_level = -1;
            } else if (level < 0) {
                last_warning_battery_level = -1;
            }

            return;
        }

        // ------------------------------------------------------
        // LOW-BATTERY WARNINGS
        // ------------------------------------------------------
        //
        // 20%: short warning
        // 10%: stronger/longer warning
        //
        // Do not interrupt an existing speaker sound. If the speaker is
        // busy, this check will retry on the next monitor update.
        bool warning_pending = false;

        if (last_warning_battery_level >= 0 &&
            last_warning_battery_level > 10 &&
            level <= 10) {
            if (!warning_10_sent && !M5.Speaker.isPlaying()) {
                // Critical warning always plays at 80%, independent of
                // the normal UI volume slider.
                playBatteryWarningTone(900, 350);
                warning_10_sent = true;
                warning_20_sent = true;
            } else if (!warning_10_sent) {
                warning_pending = true;
            }
        } else if (last_warning_battery_level >= 0 &&
                   last_warning_battery_level > 20 &&
                   level <= 20) {
            if (!warning_20_sent && !M5.Speaker.isPlaying()) {
                // Low-battery warning always plays at 80%.
                playBatteryWarningTone(1800, 140);
                warning_20_sent = true;
            } else if (!warning_20_sent) {
                warning_pending = true;
            }
        }

        if (!warning_pending) {
            last_warning_battery_level = level;
        }

        // ------------------------------------------------------
        // RUNTIME ESTIMATOR
        // ------------------------------------------------------

        if (!runtime_window_active) {
            runtime_window_active = true;
            runtime_window_start_ms = now;
            last_runtime_sample_ms = now;
            runtime_window_start_level = level;
            return;
        }

        // Calculate only once per minute.
        if ((uint32_t)(now - last_runtime_sample_ms) < runtime_sample_ms) {
            return;
        }

        last_runtime_sample_ms = now;

        uint32_t elapsed = now - runtime_window_start_ms;
        int drop = runtime_window_start_level - level;

        // Battery percentage can move slightly upward due to load/voltage
        // variation. A large upward change means the measurement window is
        // no longer useful, so start a fresh window.
        if (level > runtime_window_start_level + 2) {
            runtime_window_start_ms = now;
            runtime_window_start_level = level;
            return;
        }

        // Wait until we have enough time and enough actual percentage drop
        // to avoid a wildly unstable estimate.
        if (elapsed >= runtime_min_observation_ms && drop >= 2) {
            float elapsed_minutes =
                static_cast<float>(elapsed) / 60000.0f;

            // If D percent were consumed in T minutes, the estimated
            // remaining time at the same average load is:
            //
            // remaining_minutes = current_percent * T / D
            float raw_minutes =
                (static_cast<float>(level) * elapsed_minutes) /
                static_cast<float>(drop);

            // Reject implausible values caused by battery-level quantization.
            if (raw_minutes >= 1.0f && raw_minutes <= 1440.0f) {
                int raw = static_cast<int>(raw_minutes + 0.5f);

                if (runtime_estimate_minutes < 0) {
                    runtime_estimate_minutes = raw;
                } else {
                    // Smooth the displayed ETA:
                    // 70% previous estimate + 30% newest measurement.
                    runtime_estimate_minutes =
                        (runtime_estimate_minutes * 7 + raw * 3) / 10;
                }
            }
        }

        // Use rolling observation windows so the ETA can adapt to changes
        // such as the display spending more time on/off.
        if (elapsed >= runtime_window_ms) {
            runtime_window_start_ms = now;
            runtime_window_start_level = level;
        }
    }


    void formatBatteryRuntime(char* out, size_t out_size) {
        if (out == nullptr || out_size == 0) {
            return;
        }

        if (runtime_estimate_minutes < 0) {
            snprintf(out, out_size, "CALCULATE");
            return;
        }

        int minutes = runtime_estimate_minutes;

        if (minutes < 60) {
            snprintf(out, out_size, "~%dM", minutes);
        } else {
            int hours = minutes / 60;
            int mins = minutes % 60;

            if (hours < 10) {
                snprintf(out, out_size, "~%dH%02dM", hours, mins);
            } else {
                snprintf(out, out_size, "~%dH", hours);
            }
        }
    }


    void drawBatteryStatusTo(
        LovyanGFX* gfx,
        bool force_redraw = false
    ) {
        uint32_t now = millis();

        // Make sure cached battery data is current.
        updateBatteryMonitor(force_redraw);

        if (!force_redraw &&
            (uint32_t)(now - last_battery_update_ms) < battery_refresh_ms) {
            return;
        }

        last_battery_update_ms = now;

        int battery_level = battery_level_now;
        bool charging = battery_charging_now;
        bool external_power = battery_external_power_now;

        bool low_battery =
            (battery_level >= 0 &&
             battery_level <= 20 &&
             !charging &&
             !external_power);

        // Flash the battery icon twice per second when <=20%.
        bool flash_on =
            !low_battery ||
            (((now / 500UL) & 1UL) == 0UL);

        // Normally redraw only when something changed. During low battery,
        // the changing flash phase also forces a redraw.
        if (!force_redraw &&
            battery_state_valid &&
            battery_level == last_battery_level &&
            charging == last_battery_charging &&
            external_power == last_battery_external_power &&
            flash_on == last_battery_flash_on) {
            return;
        }

        last_battery_level = battery_level;
        last_battery_charging = charging;
        last_battery_external_power = external_power;
        last_battery_flash_on = flash_on;
        battery_state_valid = true;

        // Compact battery area at the top of the right-side control stack.
        static constexpr int area_x = scroll_width + 1;
        static constexpr int area_y = 2;
        // Leave the far top-right corner untouched for the DMX heartbeat.
        static constexpr int area_w = 320 - scroll_width - 16;
        static constexpr int area_h = 50;

        gfx->fillRect(
            area_x,
            area_y,
            area_w,
            area_h,
            TFT_BLACK
        );

        // Battery icon.
        // Battery icon centered in the right-side column.
        static constexpr int bat_w = 36;
        static constexpr int bat_h = 14;
        static constexpr int bat_x =
            scroll_width +
            ((320 - scroll_width - bat_w - 4) >> 1);
        static constexpr int bat_y = 4;

        if (flash_on) {
            gfx->drawRoundRect(
                bat_x,
                bat_y,
                bat_w,
                bat_h,
                3,
                TFT_WHITE
            );

            // Battery terminal.
            gfx->fillRect(
                bat_x + bat_w,
                bat_y + 4,
                4,
                6,
                TFT_WHITE
            );

            if (battery_level >= 0) {
                int level = battery_level;

                if (level < 0) {
                    level = 0;
                }

                if (level > 100) {
                    level = 100;
                }

                int inner_w = bat_w - 6;
                int fill_w = (inner_w * level) / 100;

                // Battery level colors:
                //   36-100% = green
                //   21-35%  = yellow
                //   0-20%   = red
                uint16_t fill_color = TFT_GREEN;

                if (level <= 20) {
                    fill_color = TFT_RED;
                } else if (level <= 35) {
                    fill_color = TFT_YELLOW;
                }

                // Charging always shows green.
                if (charging) {
                    fill_color = TFT_GREEN;
                }

                if (fill_w > 0) {
                    gfx->fillRect(
                        bat_x + 3,
                        bat_y + 3,
                        fill_w,
                        bat_h - 6,
                        fill_color
                    );
                }
            }
        }

        // Percentage.
        gfx->setTextDatum(textdatum_t::middle_center);
        gfx->setTextColor(TFT_WHITE, TFT_BLACK);
        gfx->setFont(&fonts::Font0);

        char battery_text[8];

        if (battery_level >= 0) {
            snprintf(
                battery_text,
                sizeof(battery_text),
                "%d%%",
                battery_level
            );
        } else {
            snprintf(
                battery_text,
                sizeof(battery_text),
                "--%%"
            );
        }

        int center_x =
            (scroll_width + gfx->width()) >> 1;

        gfx->drawString(
            battery_text,
            center_x,
            28
        );

        // Charging / external-power / runtime status.
        gfx->setFont(&fonts::Font0);

        // Clear the entire status-text row before drawing the new state.
        // CALCULATE is wider than CHARGE/PWR, so without this cleanup
        // characters at the far right could remain visible.
        gfx->fillRect(
            scroll_width + 1,
            35,
            320 - scroll_width - 1,
            18,
            TFT_BLACK
        );

        if (charging) {
            gfx->setTextColor(TFT_GREEN, TFT_BLACK);
            gfx->drawString("CHARGE", center_x, 44);
        } else if (external_power) {
            gfx->setTextColor(TFT_WHITE, TFT_BLACK);
            gfx->drawString("PWR", center_x, 44);
        } else {
            // Runtime estimate while operating on battery.
            char runtime_text[16];
            formatBatteryRuntime(runtime_text, sizeof(runtime_text));

            gfx->setTextColor(
                low_battery ? TFT_RED : TFT_DARKGRAY,
                TFT_BLACK
            );

            gfx->drawString(
                runtime_text,
                center_x,
                44
            );
        }

        // Restore standard text color for the rest of the UI.
        gfx->setTextColor(TFT_WHITE, TFT_BLACK);
    }


    // Normal Sender-screen wrapper.
    void drawBatteryStatus(bool force_redraw = false) {
        drawBatteryStatusTo(
            &M5.Display,
            force_redraw
        );
    }


    void drawDmxActivityIndicator(bool force_redraw = false) {
        // The heartbeat belongs only to the normal Sender screen.
        // A full-screen SETTINGS/PRESET page can be opened in the middle
        // of the current loop iteration, so guard here as well.
        if (settings_screen || preset_screen) {
            return;
        }

        uint32_t now = millis();

        if (!force_redraw &&
            (uint32_t)(now - dmx_indicator_last_draw_ms) <
                dmx_indicator_refresh_ms) {
            return;
        }

        dmx_indicator_last_draw_ms = now;

        bool active =
            dmx_last_activity_ms != 0 &&
            (uint32_t)(now - dmx_last_activity_ms) <
                dmx_active_timeout_ms;

        // 2 Hz-ish visual heartbeat: 250 ms ON / 250 ms OFF.
        bool blink_on =
            active &&
            (((now / 250UL) & 1UL) == 0UL);

        if (!force_redraw &&
            dmx_indicator_state_valid &&
            active == dmx_indicator_last_active &&
            blink_on == dmx_indicator_last_on) {
            return;
        }

        dmx_indicator_last_active = active;
        dmx_indicator_last_on = blink_on;
        dmx_indicator_state_valid = true;

        // Top-right corner of the right-side status area.
        static constexpr int dot_x = 312;
        static constexpr int dot_y = 8;
        static constexpr int dot_r = 4;

        // Clear only the small heartbeat area.
        M5.Display.fillRect(304, 0, 16, 16, TFT_BLACK);

        uint16_t dot_color;

        if (!active) {
            // No recent completed/scheduled DMX frames.
            dot_color = TFT_RED;
        } else if (blink_on) {
            dot_color = TFT_GREEN;
        } else {
            // Dark phase of the heartbeat.
            dot_color = TFT_DARKGRAY;
        }

        M5.Display.fillCircle(
            dot_x,
            dot_y,
            dot_r,
            dot_color
        );
    }


    void drawPresetOpenButton(void) {
        const int center_x =
            preset_open_x +
            (preset_open_w >> 1);

        const int center_y =
            preset_open_y +
            (preset_open_h >> 1);

        // Clear the complete right-side control area.
        M5.Display.fillRect(
            scroll_width,
            0,
            320 - scroll_width,
            M5.Display.height(),
            TFT_BLACK
        );

        M5.Display.setTextDatum(
            textdatum_t::middle_center
        );

        M5.Display.setTextColor(
            TFT_WHITE,
            TFT_BLACK
        );

        M5.Display.setFont(&fonts::Font0);

        // Battery.
        drawBatteryStatus(true);

        // SETTINGS.
        drawSettingsButton();

        // BLACKOUT.
        drawBlackoutButton();

        // PRESET.
        M5.Display.drawRoundRect(
            preset_open_x,
            preset_open_y,
            preset_open_w,
            preset_open_h,
            6,
            TFT_WHITE
        );

        M5.Display.setTextColor(
            TFT_WHITE,
            TFT_BLACK
        );

        M5.Display.drawString(
            "PRESET",
            center_x,
            center_y
        );

        // DMX heartbeat in the top-right status corner.
        drawDmxActivityIndicator(true);
    }

    void openPresetScreen(void) {
        preset_screen = true;
        scroll_y_add = 0;
        preset_status[0] = '\0';

        preset_save_holding = false;
        preset_save_completed = false;
        preset_save_hold_start_ms = 0;
        preset_save_last_progress = -1;

        preset_slot_holding = false;
        preset_slot_hold_start_ms = 0;

        preset_options_screen = false;
        preset_options_consume_release = false;

        preset_copy_mode = false;

        preset_clear_holding = false;
        preset_clear_completed = false;
        preset_clear_hold_start_ms = 0;
        preset_clear_last_progress = -1;

        loadPresetMeta();

        drawPresetScreen();
    }

    void closePresetScreen(void) {
        preset_screen = false;

        preset_save_holding = false;
        preset_save_completed = false;
        preset_save_hold_start_ms = 0;
        preset_save_last_progress = -1;

        preset_slot_holding = false;
        preset_slot_hold_start_ms = 0;

        preset_options_screen = false;
        preset_options_consume_release = false;
        preset_copy_mode = false;

        preset_clear_holding = false;
        preset_clear_completed = false;
        preset_clear_hold_start_ms = 0;
        preset_clear_last_progress = -1;

        M5.Display.fillScreen(TFT_BLACK);

        // Restore normal DMX channel-select interface.
        ui_mode = ui_mode_t::mode_channel_select;
        target_value = data[data_idx][target_channel];

        updateDisplay(true);
        hideUIValueSet();
    }

    void drawPresetScreen(void) {
        M5.Display.fillScreen(TFT_BLACK);

        M5.Display.setTextDatum(textdatum_t::middle_center);
        M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
        M5.Display.setFont(&fonts::AsciiFont8x16);

        if (preset_copy_mode) {
            M5.Display.drawString("COPY PRESET", 160, 17);
        } else {
            M5.Display.drawString("DMX PRESETS", 160, 17);
        }

        // P1-P8, arranged as 4 columns x 2 rows.
        for (uint8_t slot = 0; slot < PresetStore::PRESET_COUNT; ++slot) {
            int col = slot % 4;
            int row = slot / 4;

            int x = preset_grid_x +
                    col * (preset_slot_w + preset_slot_gap_x);

            int y = preset_grid_y +
                    row * (preset_slot_h + preset_slot_gap_y);

            bool selected = (slot == selected_preset);
            bool occupied = presetExists(slot);

            // Slot border.
            M5.Display.drawRoundRect(x, y,
                                     preset_slot_w, preset_slot_h,
                                     6,
                                     selected ? TFT_WHITE : TFT_DARKGRAY);

            // Selected slot gets a second border.
            if (selected) {
                M5.Display.drawRoundRect(x + 2, y + 2,
                                         preset_slot_w - 4,
                                         preset_slot_h - 4,
                                         5,
                                         TFT_WHITE);
            }

            char label[4];
            snprintf(label, sizeof(label),
                     "P%u",
                     static_cast<unsigned int>(slot + 1));

            M5.Display.setFont(&fonts::AsciiFont8x16);
            M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);

            M5.Display.drawString(
                label,
                x + (preset_slot_w >> 1),
                y + (preset_slot_h >> 1)
            );

            // Green square = preset contains stored DMX data.
            if (occupied) {
                M5.Display.fillRect(
                    x + preset_slot_w - 12,
                    y + 7,
                    6,
                    6,
                    TFT_GREEN
                );
            }
        }

        // Status / selected preset line.
        M5.Display.setFont(&fonts::Font0);
        M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);

        if (preset_status[0] != '\0') {
            M5.Display.drawString(preset_status, 160, 158);
        } else if (preset_copy_mode) {
            char copy_text[32];

            snprintf(
                copy_text,
                sizeof(copy_text),
                "COPY P%u -> TAP EMPTY SLOT",
                static_cast<unsigned int>(preset_copy_source + 1)
            );

            M5.Display.drawString(copy_text, 160, 158);
        } else {
            char selected_text[24];

            snprintf(
                selected_text,
                sizeof(selected_text),
                "SELECTED: P%u",
                static_cast<unsigned int>(selected_preset + 1)
            );

            M5.Display.drawString(selected_text, 160, 158);
        }

        // Bottom action buttons.
        static const char* action_labels[3] = {
            "LOAD", "SAVE", "BACK"
        };

        for (int i = 0; i < 3; ++i) {
            int x = preset_action_x0 +
                    i * (preset_action_w + preset_action_gap);

            M5.Display.drawRoundRect(
                x,
                preset_action_y,
                preset_action_w,
                preset_action_h,
                6,
                TFT_WHITE
            );

            // SAVE button gets a hold-progress fill.
            if (i == 1 && preset_save_holding && !preset_save_completed) {
                uint32_t elapsed = millis() - preset_save_hold_start_ms;

                if (elapsed > preset_save_hold_ms) {
                    elapsed = preset_save_hold_ms;
                }

                int progress_w =
                    ((preset_action_w - 6) * static_cast<int>(elapsed)) /
                    static_cast<int>(preset_save_hold_ms);

                if (progress_w > 0) {
                    M5.Display.fillRect(
                        x + 3,
                        preset_action_y + preset_action_h - 8,
                        progress_w,
                        4,
                        TFT_YELLOW
                    );
                }
            }

            M5.Display.drawString(
                action_labels[i],
                x + (preset_action_w >> 1),
                preset_action_y + (preset_action_h >> 1)
            );
        }
    }

    int getPresetSlotAt(int touch_x, int touch_y) {
        for (uint8_t slot = 0;
             slot < PresetStore::PRESET_COUNT;
             ++slot) {

            int col = slot % 4;
            int row = slot / 4;

            int x =
                preset_grid_x +
                col *
                    (preset_slot_w + preset_slot_gap_x);

            int y =
                preset_grid_y +
                row *
                    (preset_slot_h + preset_slot_gap_y);

            if (touch_x >= x &&
                touch_x < x + preset_slot_w &&
                touch_y >= y &&
                touch_y < y + preset_slot_h) {

                return slot;
            }
        }

        return -1;
    }


    void drawPresetOptionsScreen(void) {
        M5.Display.fillScreen(TFT_BLACK);

        M5.Display.setTextDatum(
            textdatum_t::middle_center
        );

        M5.Display.setTextColor(
            TFT_WHITE,
            TFT_BLACK
        );

        M5.Display.setFont(
            &fonts::AsciiFont8x16
        );

        char title[24];

        snprintf(
            title,
            sizeof(title),
            "P%u OPTIONS",
            static_cast<unsigned int>(
                preset_options_slot + 1
            )
        );

        M5.Display.drawString(
            title,
            160,
            28
        );

        M5.Display.setFont(&fonts::Font0);
        M5.Display.setTextColor(
            TFT_DARKGRAY,
            TFT_BLACK
        );

        M5.Display.drawString(
            "COPY DUPLICATES TO AN EMPTY SLOT",
            160,
            70
        );

        M5.Display.drawString(
            "HOLD CLEAR FOR 1 SECOND",
            160,
            92
        );

        M5.Display.setTextColor(
            TFT_WHITE,
            TFT_BLACK
        );

        static const char* labels[3] = {
            "COPY",
            "CLEAR",
            "BACK"
        };

        for (int i = 0; i < 3; ++i) {
            int x =
                preset_option_x0 +
                i *
                    (preset_option_w +
                     preset_option_gap);

            M5.Display.drawRoundRect(
                x,
                preset_option_y,
                preset_option_w,
                preset_option_h,
                6,
                TFT_WHITE
            );

            // CLEAR gets the same protected-hold progress treatment.
            if (i == 1 &&
                preset_clear_holding &&
                !preset_clear_completed) {

                uint32_t elapsed =
                    millis() -
                    preset_clear_hold_start_ms;

                if (elapsed > preset_clear_hold_ms) {
                    elapsed = preset_clear_hold_ms;
                }

                int progress_w =
                    ((preset_option_w - 6) *
                     static_cast<int>(elapsed)) /
                    static_cast<int>(
                        preset_clear_hold_ms
                    );

                if (progress_w > 0) {
                    M5.Display.fillRect(
                        x + 3,
                        preset_option_y +
                            preset_option_h - 8,
                        progress_w,
                        4,
                        TFT_RED
                    );
                }
            }

            M5.Display.drawString(
                labels[i],
                x + (preset_option_w >> 1),
                preset_option_y +
                    (preset_option_h >> 1)
            );
        }
    }


    void closePresetOptionsScreen(void) {
        preset_options_screen = false;

        preset_clear_holding = false;
        preset_clear_completed = false;
        preset_clear_hold_start_ms = 0;
        preset_clear_last_progress = -1;

        drawPresetScreen();
    }


    bool handlePresetSlotLongPress(
        const decltype(M5.Touch.getDetail())& tp
    ) {
        uint32_t now = millis();
        int slot =
            getPresetSlotAt(
                tp.base_x,
                tp.base_y
            );

        // Start tracking a press on one of the P1-P8 slots.
        if (tp.wasPressed() && slot >= 0) {
            preset_slot_holding = true;
            preset_slot_hold_slot =
                static_cast<uint8_t>(slot);

            preset_slot_hold_start_ms = now;

            return false;
        }

        if (!preset_slot_holding) {
            return false;
        }

        // Finger moved away from the original slot.
        if (tp.state &&
            slot != preset_slot_hold_slot) {

            preset_slot_holding = false;
            preset_slot_hold_start_ms = 0;

            return false;
        }

        if (tp.state &&
            slot == preset_slot_hold_slot) {

            uint32_t elapsed =
                now - preset_slot_hold_start_ms;

            if (elapsed >=
                preset_slot_long_press_ms) {

                preset_slot_holding = false;
                preset_slot_hold_start_ms = 0;

                selected_preset =
                    preset_slot_hold_slot;

                if (!presetExists(selected_preset)) {
                    snprintf(
                        preset_status,
                        sizeof(preset_status),
                        "P%u IS EMPTY",
                        static_cast<unsigned int>(
                            selected_preset + 1
                        )
                    );

                    drawPresetScreen();
                    return true;
                }

                preset_options_slot =
                    selected_preset;

                preset_options_screen = true;

                // Consume the release of this same long-press.
                preset_options_consume_release = true;

                preset_clear_holding = false;
                preset_clear_completed = false;
                preset_clear_hold_start_ms = 0;
                preset_clear_last_progress = -1;

                if (!M5.Speaker.isPlaying()) {
                    M5.Speaker.tone(2200, 70);
                }

                drawPresetOptionsScreen();
                return true;
            }
        }

        // Released before reaching the long-press threshold:
        // let the existing normal-tap handler process it.
        if (!tp.state) {
            preset_slot_holding = false;
            preset_slot_hold_start_ms = 0;
        }

        return false;
    }


    bool handlePresetClearHold(
        const decltype(M5.Touch.getDetail())& tp
    ) {
        int clear_x =
            preset_option_x0 +
            (preset_option_w +
             preset_option_gap);

        bool inside_clear =
            tp.base_x >= clear_x &&
            tp.base_x <
                clear_x + preset_option_w &&
            tp.base_y >= preset_option_y &&
            tp.base_y <
                preset_option_y +
                preset_option_h;

        uint32_t now = millis();

        if (tp.wasPressed() && inside_clear) {
            preset_clear_holding = true;
            preset_clear_completed = false;
            preset_clear_hold_start_ms = now;
            preset_clear_last_progress = 0;

            drawPresetOptionsScreen();
            return true;
        }

        if (!preset_clear_holding) {
            return false;
        }

        // Sliding away cancels the destructive action.
        if (tp.state && !inside_clear) {
            preset_clear_holding = false;
            preset_clear_completed = false;
            preset_clear_hold_start_ms = 0;
            preset_clear_last_progress = -1;

            drawPresetOptionsScreen();
            return true;
        }

        if (tp.state &&
            inside_clear &&
            !preset_clear_completed) {

            uint32_t elapsed =
                now -
                preset_clear_hold_start_ms;

            int progress =
                static_cast<int>(
                    (elapsed * 10UL) /
                    preset_clear_hold_ms
                );

            if (progress > 10) {
                progress = 10;
            }

            if (progress !=
                preset_clear_last_progress) {

                preset_clear_last_progress =
                    progress;

                drawPresetOptionsScreen();
            }

            if (elapsed >= preset_clear_hold_ms) {
                preset_clear_completed = true;

                uint8_t cleared_slot =
                    preset_options_slot;

                bool success =
                    clearPreset(cleared_slot);

                Serial.println(preset_status);

                if (success &&
                    !M5.Speaker.isPlaying()) {

                    M5.Speaker.tone(1000, 180);
                }

                // Return to grid immediately after successful clear.
                preset_options_screen = false;

                preset_clear_holding = false;
                preset_clear_completed = false;
                preset_clear_hold_start_ms = 0;
                preset_clear_last_progress = -1;

                drawPresetScreen();
            }

            return true;
        }

        if (!tp.state) {
            preset_clear_holding = false;
            preset_clear_completed = false;
            preset_clear_hold_start_ms = 0;
            preset_clear_last_progress = -1;

            drawPresetOptionsScreen();
            return true;
        }

        return false;
    }


    void handlePresetOptionsTouch(
        int touch_x,
        int touch_y
    ) {
        if (touch_y < preset_option_y ||
            touch_y >=
                preset_option_y +
                preset_option_h) {

            return;
        }

        int copy_x =
            preset_option_x0;

        int clear_x =
            preset_option_x0 +
            (preset_option_w +
             preset_option_gap);

        int back_x =
            preset_option_x0 +
            2 *
                (preset_option_w +
                 preset_option_gap);

        // COPY
        if (touch_x >= copy_x &&
            touch_x <
                copy_x + preset_option_w) {

            preset_copy_source =
                preset_options_slot;

            preset_copy_mode = true;
            preset_options_screen = false;

            snprintf(
                preset_status,
                sizeof(preset_status),
                "COPY P%u -> TAP EMPTY SLOT",
                static_cast<unsigned int>(
                    preset_copy_source + 1
                )
            );

            drawPresetScreen();
            return;
        }

        // CLEAR: normal taps never clear.
        if (touch_x >= clear_x &&
            touch_x <
                clear_x + preset_option_w) {

            // The instruction is already on-screen.
            // Actual CLEAR is handled by the hold handler.
            return;
        }

        // BACK
        if (touch_x >= back_x &&
            touch_x <
                back_x + preset_option_w) {

            closePresetOptionsScreen();
            return;
        }
    }


    bool handlePresetSaveHold(const decltype(M5.Touch.getDetail())& tp) {
        int save_x =
            preset_action_x0 +
            (preset_action_w + preset_action_gap);

        bool inside_save =
            tp.base_x >= save_x &&
            tp.base_x < save_x + preset_action_w &&
            tp.base_y >= preset_action_y &&
            tp.base_y < preset_action_y + preset_action_h;

        uint32_t now = millis();

        // Start protected SAVE hold.
        if (tp.wasPressed() && inside_save) {
            preset_save_holding = true;
            preset_save_completed = false;
            preset_save_hold_start_ms = now;
            preset_save_last_progress = 0;

            snprintf(
                preset_status,
                sizeof(preset_status),
                "HOLD SAVE..."
            );

            drawPresetScreen();
            return true;
        }

        if (!preset_save_holding) {
            return false;
        }

        // Moving the finger away cancels the pending save.
        if (tp.state && !inside_save) {
            preset_save_holding = false;
            preset_save_completed = false;
            preset_save_last_progress = -1;

            snprintf(
                preset_status,
                sizeof(preset_status),
                "SAVE CANCELLED"
            );

            drawPresetScreen();
            return true;
        }

        // Finger still held on SAVE: update progress and save at 1 second.
        if (tp.state && inside_save && !preset_save_completed) {
            uint32_t elapsed = now - preset_save_hold_start_ms;

            int progress =
                static_cast<int>(
                    (elapsed * 10UL) / preset_save_hold_ms
                );

            if (progress > 10) {
                progress = 10;
            }

            // Redraw only when progress advances by 10%.
            if (progress != preset_save_last_progress) {
                preset_save_last_progress = progress;
                drawPresetScreen();
            }

            if (elapsed >= preset_save_hold_ms) {
                preset_save_completed = true;

                savePreset(selected_preset);
                Serial.println(preset_status);

                // Confirmation sound, but do not cut an existing sound.
                if (!M5.Speaker.isPlaying()) {
                    M5.Speaker.tone(2400, 90);
                }

                drawPresetScreen();
            }

            return true;
        }

        // Finger released before the hold completed: cancel.
        if (!tp.state) {
            if (!preset_save_completed) {
                snprintf(
                    preset_status,
                    sizeof(preset_status),
                    "HOLD 1 SEC TO SAVE"
                );
            }

            preset_save_holding = false;
            preset_save_completed = false;
            preset_save_hold_start_ms = 0;
            preset_save_last_progress = -1;

            drawPresetScreen();
            return true;
        }

        return false;
    }


    void handlePresetScreenTouch(int touch_x, int touch_y) {
        // ------------------------------------------------------
        // Select P1-P8
        // ------------------------------------------------------
        for (uint8_t slot = 0; slot < PresetStore::PRESET_COUNT; ++slot) {
            int col = slot % 4;
            int row = slot / 4;

            int x = preset_grid_x +
                    col * (preset_slot_w + preset_slot_gap_x);

            int y = preset_grid_y +
                    row * (preset_slot_h + preset_slot_gap_y);

            if (touch_x >= x &&
                touch_x < x + preset_slot_w &&
                touch_y >= y &&
                touch_y < y + preset_slot_h) {

                // COPY workflow: tap an empty destination slot.
                if (preset_copy_mode) {
                    bool success =
                        copyPreset(
                            preset_copy_source,
                            slot
                        );

                    if (success) {
                        preset_copy_mode = false;
                        selected_preset = slot;

                        if (!M5.Speaker.isPlaying()) {
                            M5.Speaker.tone(2400, 90);
                        }
                    }

                    drawPresetScreen();
                    return;
                }

                selected_preset = slot;
                preset_status[0] = '\0';

                drawPresetScreen();
                return;
            }
        }

        // ------------------------------------------------------
        // LOAD / SAVE / BACK
        // ------------------------------------------------------
        if (touch_y >= preset_action_y &&
            touch_y < preset_action_y + preset_action_h) {

            int load_x = preset_action_x0;

            int save_x =
                preset_action_x0 +
                (preset_action_w + preset_action_gap);

            int back_x =
                preset_action_x0 +
                2 * (preset_action_w + preset_action_gap);

            // LOAD
            if (touch_x >= load_x &&
                touch_x < load_x + preset_action_w) {

                loadPreset(selected_preset);

                Serial.println(preset_status);

                drawPresetScreen();
                return;
            }

            // SAVE
            // Protected: a normal tap must never overwrite a preset.
            // Actual saving is handled by handlePresetSaveHold().
            if (touch_x >= save_x &&
                touch_x < save_x + preset_action_w) {

                snprintf(
                    preset_status,
                    sizeof(preset_status),
                    "HOLD 1 SEC TO SAVE"
                );

                drawPresetScreen();
                return;
            }

            // BACK
            if (touch_x >= back_x &&
                touch_x < back_x + preset_action_w) {

                if (preset_copy_mode) {
                    preset_copy_mode = false;

                    snprintf(
                        preset_status,
                        sizeof(preset_status),
                        "COPY CANCELLED"
                    );

                    drawPresetScreen();
                    return;
                }

                closePresetScreen();
                return;
            }
        }
    }

    void hideUIValueSet(void) {
        // Channel-select mode shows a PRESET button on the right.
        drawPresetOpenButton();
    }

    void drawUIValueUpDown(int sign = 0) {
        int x = scroll_width + 10;
        int w = 320 - scroll_width - 20;
        // M5.Display.drawRoundRect(x,              2, w, slider_btn_height - 4,
        // 4, sign > 0 ? TFT_WHITE : TFT_DARKGRAY);
        M5.Display.drawRoundRect(x, slider_y - 10 - (slider_btn_height - 4), w,
                                 slider_btn_height - 4, 4,
                                 sign > 0 ? TFT_WHITE : TFT_DARKGRAY);
        M5.Display.drawRoundRect(x, slider_y + 128 + 10, w,
                                 slider_btn_height - 4, 4,
                                 sign < 0 ? TFT_WHITE : TFT_DARKGRAY);
    }

    void drawUIValueSet(uint16_t new_value, bool force_redraw = false) {
        M5.Display.setTextDatum(textdatum_t::middle_center);
        M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
        M5.Display.setFont(&fonts::AsciiFont8x16);
        char str[8];
        uint32_t color = bar_color_table[target_channel & 15];

        int y  = ((256 - new_value) >> 1) + slider_y;
        int py = ((256 - target_value) >> 1) + slider_y;

        int text_x = (scroll_width + M5.Display.width()) >> 1;
        // int text_y = ((slider_y * 2 + 128)+M5.Display.height()) >> 1;
        int text_y = 16;
        if (force_redraw) {
            M5.Display.fillRect(scroll_width + 1, 0, 320 - scroll_width,
                                M5.Display.height(), TFT_BLACK);
            py = 128 + slider_y;

            M5.Display.setColor(TFT_WHITE);
            drawFocusBox(&M5.Display, slider_x - 8, slider_y - 5, slider_w + 16,
                         128 + 10, 2);

            fillBar(&M5.Display, slider_x, slider_y, slider_w, y - slider_y,
                    target_channel);

            int text_x = (scroll_width + M5.Display.width()) >> 1;
            M5.Display.drawString("+", text_x,
                                  slider_y - 8 - (slider_btn_height >> 1));
            M5.Display.drawString(
                "-", text_x, slider_y + 128 + 8 + (slider_btn_height >> 1));
            snprintf(str, sizeof(str), "%3dCH", target_channel);
            M5.Display.drawString(str, text_x, text_y - 8);
            drawUIValueUpDown();
        }
        if (force_redraw || target_value != new_value) {
            snprintf(str, sizeof(str), "%3d", new_value);
            M5.Display.drawString(str, text_x, text_y + 8);
            target_value = new_value;
        }

        fillBar(&M5.Display, slider_x, py, slider_w, y - py, target_channel);
    }

    uint32_t getBarColor(int32_t y) {
        int32_t v  = 63 - (y >> 1);
        int32_t v2 = 63 - ((y + 1) >> 1);
        if ((v >> 4) != (v2 >> 4)) {
            return 0x887766u;
        }
        return m5gfx::color888(v + 2, v, v + 6);
    }

    void fillBar(LovyanGFX* gfx, int32_t x, int32_t y, int32_t w, int32_t h,
                 size_t ch = 0) {
        uint32_t color_add = 0;
        if (h < 0) {
            y += h;
            h         = -h;
            color_add = (bar_color_table[ch & 15] >> 1) & 0x7F7F7Fu;
        }

        gfx->setAddrWindow(x, y, w, h);
        uint32_t prev_color =
            (color_add + getBarColor(y - slider_y)) & 0xF8FCF8u;
        uint32_t py = y;
        while (h--) {
            uint32_t color =
                (color_add + getBarColor(++y - slider_y)) & 0xF8FCF8u;
            if (prev_color != color || h == 0) {
                gfx->pushBlock(prev_color, w * (y - py));
                prev_color = color;
                py         = y;
            }
        }
    }

    void updateDisplay(bool full_redraw = false) {
        int idx     = 0;
        bool drawed = false;

        size_t drawcount = 0;
        for (int ch = 1; ch < DMX_MAX_PACKET_SIZE; ++ch) {
            if (!visible[ch]) {
                if (data[0][ch] == data[1][ch]) {
                    continue;
                }
                visible[ch] = true;
                full_redraw = true;
            }
            ++drawcount;
        }

        M5.Display.setClipRect(0, 0, 320, scroll_height);
        M5.Display.startWrite();

        static constexpr int circle_x = channel_item_width >> 1;
        static constexpr int circle_y = (channel_item_height - 8) >> 1;

        for (int ch = 1; ch < 512 + channel_item_cols; ++ch) {
            uint32_t current_data = 0;
            uint32_t prev_data    = 0;
            if (ch < DMX_MAX_PACKET_SIZE) {
                current_data = data[data_idx][ch];
                prev_data    = data[!data_idx][ch];
            }
            if (full_redraw || current_data != prev_data) {
                int y = (idx / channel_item_cols) * channel_item_height;
                y -= scroll_y;
                if (y >= scroll_height) {
                    break;
                }
                if (y + channel_item_height > 0) {
                    int x = (idx % channel_item_cols) * channel_item_width;

                    auto& c     = canvas[canvas_flip];
                    canvas_flip = !canvas_flip;
                    c.createSprite(channel_item_width, channel_item_height);

                    if (ch < DMX_MAX_PACKET_SIZE) {
                        if (ch == target_channel) {
                            int fw = (ui_mode == ui_mode_t::mode_channel_select)
                                         ? 2
                                         : 1;
                            c.setColor(0xFFFFFFu);
                            drawFocusBox(&c, 0, 0, channel_item_width,
                                         channel_item_height, fw);
                        }
                        bool dimmer =
                            (ui_mode != ui_mode_t::mode_channel_select) &&
                            (ch != target_channel);

                        c.setTextDatum(textdatum_t::middle_center);
                        c.setTextColor(dimmer ? TFT_DARKGRAY : TFT_WHITE);
                        c.setFont(&fonts::Font2);
                        c.drawNumber(ch, circle_x + 1, circle_y);
                        c.setFont(&fonts::Font0);
                        c.setTextDatum(textdatum_t::top_center);
                        c.drawNumber(current_data, circle_x + 1, circle_y + 20);
                        float angle = current_data * 360.0f / 255;

                        c.fillArc(circle_x, circle_y, 17, 15, 270 + angle, 269,
                                  dimmer ? 0x131313u : 0x333333u);
                        uint32_t color = bar_color_table[ch & 15];
                        if (dimmer) {
                            color = (0x3F3F3F & (color >> 2)) + 0x2F2F2Fu;
                        }
                        c.fillArc(circle_x, circle_y, 19, 13, 270, 270 + angle,
                                  color);
                    }
                    c.pushSprite(&M5.Display, x, y);
                }
            }
            ++idx;
        }
        M5.Display.clearClipRect();
        M5.Display.endWrite();
    }
};
