// VERIFIED PRESET-8 BUILD - 2026-08-07
#pragma once

#include "common.h"
#include "device_settings.h"
#include "logo_sender.h"
#include "dmx_controller.h"
#include "preset_double_tap.h"
#include "command_dispatcher.h"
#include "dmx_transport.h"
#include "hardware_diagnostics.h"
#include "network_service.h"
#include "fullscreen_canvas_memory.h"
#include <soc/soc_memory_types.h>

// Display settings are owned by src.cpp because the global safe-wake /
// auto-off logic lives there.
DeviceSettings& deviceSettings();
void persistDeviceSettings();
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
    view_sender_t(DmxController& controller, CommandDispatcher& dispatcher, DmxTransport& transport)
        : controller(controller), dispatcher(dispatcher), transport(transport) {
        controller.setObserver(onControllerChange, this);
        dispatcher.setPublisher(publishDmx, this);
        dispatcher.setAdmission(admitDmx, this);
    }

    bool reserveNetworkCanvas() {
        wifi_startup_trace::capture("fullscreen.before", 0, false, nullptr, 0, true);
        network_canvas_reserved = getFullscreenCanvas(active_fullscreen_page, "NETWORK RESERVE") != nullptr;
        wifi_startup_trace::capture("fullscreen.after", 0, false, fullscreen_canvas_buffer, network_canvas_reserved);
        for (auto& item : canvas) {
            if (!item.createSprite(channel_item_width, channel_item_height)) network_canvas_reserved = false;
        }
        return network_canvas_reserved;
    }

    void setup(void) override {
        transport.requestSender(true);
        dispatcher.executeLocal(ControllerCommand::resetSenderCommand());
        controller_redraw_pending = false;
        memset(rendered_values, 0, sizeof(rendered_values));
        transport.stage(controller.outputPacket(), DMX_MAX_PACKET_SIZE);

        M5.Display.setFont(&fonts::Font0);
        M5.Display.setTextDatum(textdatum_t::top_left);
        M5.Display.setTextWrap(false);
        M5.Display.fillScreen(TFT_BLACK);

        enable_channel_count = 512;
        for (int i = 0; i < enable_channel_count; ++i) {
            int ch             = i + 1;
            enable_channels[i] = ch;
            visible[ch]        = 1;
        }
        target_channel = enable_channels[target_channel_index];

        updateDisplay(true);
        hideUIValueSet();
#if DMX_HW_DIAGNOSTICS
        recordHardwareDiagnostics("sender_enter");
#endif
    }

    void serviceControllerChanges() {
        if (!controller_redraw_pending || M5.Speaker.isPlaying()) return;
        controller_redraw_pending = false;
        if (settings_screen) return; // Return-to-Main already performs a full redraw.
        if (precision_editor_screen) {
            drawPrecisionEditor(); // Keeps keypad draft and existing owner checks.
        } else if (preset_screen) {
            if (!preset_options_screen) drawPresetScreen();
        } else {
            updateDisplay(true);
            if (ui_mode == ui_mode_t::mode_value_setting) drawUIValueSet(target_value, true);
            else drawBlackoutButton();
        }
    }

    bool loop(void) override {
        serviceDmxOutput();

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
            if (!tp.state) persistDeviceSettings();
            const auto settings_revision = deviceSettings().snapshot().revision;
            if (settings_revision != settings_device_revision) {
                settings_device_revision = settings_revision; settings_redraw_pending = true;
            }
            // Stage 5C Settings network refresh begin.
            serviceSettingsNetwork();
            // Stage 5C Settings network refresh end.

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

        // Full-screen precision channel editor has exclusive touch control.
        // DMX transmission at the top of loop() continues normally.
        if (precision_editor_screen) {
            handlePrecisionEditorTouch(tp);

            if (precision_redraw_pending &&
                !M5.Speaker.isPlaying()) {

                drawPrecisionEditor();
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
                preset_double_tap.reset();

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

            if (preset_copy_mode) preset_double_tap.reset();
            const int tapped_slot = getPresetSlotAt(tp.x, tp.y);
            const bool double_load = !preset_copy_mode && preset_double_tap.sample(
                tapped_slot, tp.wasPressed(), tp.isPressed(), tp.wasClicked(),
                tp.x, tp.y, millis());

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
                    tp.base_y,
                    double_load
                );

                modified = true;
            }

            if (!modified) {
                delay(1);
            }

            return true;
        }

        if (main_touch_consume_release) {
            if (!tp.state) {
                main_touch_consume_release = false;
            }

            delay(1);
            return true;
        }

        if (!tp.state) {
            main_value_slider_dragging = false;
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
                        target_value   = controller.channel(target_channel);
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

                    if (tp.wasClicked() &&
                        tp.base_x >= value_edit_button_x &&
                        tp.base_x < value_edit_button_x + value_edit_button_w &&
                        tp.base_y >= value_edit_button_y &&
                        tp.base_y < value_edit_button_y + value_edit_button_h) {

                        main_value_slider_dragging = false;
                        openPrecisionEditor();
                        modified = true;
                    } else {
                        bool slider_hit =
                            pointInRect(
                                tp.base_x,
                                tp.base_y,
                                value_slider_touch_x,
                                value_slider_touch_y,
                                value_slider_touch_w,
                                value_slider_touch_h
                            );

                        if (tp.wasPressed()) {
                            main_value_slider_dragging = slider_hit;
                        }
                    }

                    if (main_value_slider_dragging) {
                        new_value = 255 - ((tp.y - slider_y) << 1);
                    }

                    if (new_value < 0) {
                        new_value = 0;
                    } else if (new_value > 255) {
                        new_value = 255;
                    }
                    if (target_value != new_value) {
                        modified |= applyMainCommand(ControllerCommand::channelCommand(
                            CommandType::set_channel, target_channel, new_value));
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
                        target_value   = controller.channel(new_channel);
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
                int delta = 0;
                if (M5.BtnA.wasPressed() || M5.BtnA.isHolding()) {
                    --delta;
                }
                if (M5.BtnC.wasPressed() || M5.BtnC.isHolding()) {
                    ++delta;
                }
                if (delta != 0) {
                    modified |= applyMainCommand(ControllerCommand::channelCommand(
                        CommandType::adjust_channel, target_channel, delta));
                }
                if (btnB_Clicked) {
                    setUIMode(ui_mode_t::mode_channel_select);
                }
            }
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
        transport.requestSender(false);
        // Keep the existing UI buffers reserved across mode switches while Wi-Fi exists.
        if (!network_canvas_reserved) {
            for (auto& tmp : canvas) {
                tmp.deleteSprite();
            }
            fullscreen_canvas.deleteSprite();
            fullscreen_canvas_ready = false;
            fullscreen_canvas_buffer = nullptr;
        }
    }

    void serviceDmxOutput() {
        if (transport.serviceTx(esp_timer_get_time())) dmx_last_activity_ms = millis();
    }


   private:
    static constexpr int BAR_HEIGHT = 64;
    static constexpr int CONSOLE_Y  = 88;

    // ==========================
    // CONTROLLER AND PRESET UI
    // ==========================
    DmxController& controller;
    CommandDispatcher& dispatcher;
    DmxTransport& transport;
    bool controller_redraw_pending = false;

    static int publishDmx(void* context, const ControllerCommand& command) {
        return static_cast<view_sender_t*>(context)->writeCurrentDmxOutput(
            command.type == CommandType::set_blackout);
    }

    static bool admitDmx(void* context, const ControllerCommand& command) {
        auto& view = *static_cast<view_sender_t*>(context);
        const bool edge = command.type == CommandType::set_blackout &&
            (command.payload.blackout != 0) != view.controller.blackoutActive();
        switch (command.type) {
            case CommandType::set_blackout:
            case CommandType::set_channel:
            case CommandType::adjust_channel:
            case CommandType::set_channels:
            case CommandType::load_preset:
                return view.transport.canStage(edge);
            default: return true;
        }
    }

    static void onControllerChange(void* context, const DmxController::Change&) {
        auto& view = *static_cast<view_sender_t*>(context);
        view.target_value = view.controller.channel(view.target_channel);
        if (!view.dispatcher.executingLocal()) view.controller_redraw_pending = true;
    }

    bool applyMainCommand(const ControllerCommand& command) {
        const int previous = target_value;
        const auto revision = controller.revision();
        dispatcher.executeLocal(command);
        const int committed = controller.channel(target_channel);
        // Incremental slider painting needs the old display value, never a write draft.
        target_value = previous;
        if (previous != committed) drawUIValueSet(committed);
        target_value = committed;
        if (controller.revision() == revision) return false;
        updateDisplay();
        return true;
    }

    bool preset_screen      = false;
    uint8_t selected_preset = 0;
    char preset_status[32]  = "";
    PresetDoubleTap preset_double_tap;

    M5Canvas canvas[2];
    bool canvas_flip;

    enum fullscreen_page_t {
        fullscreen_page_none,
        fullscreen_page_settings,
        fullscreen_page_precision,
        fullscreen_page_keypad,
        fullscreen_page_preset
    };

    fullscreen_page_t active_fullscreen_page = fullscreen_page_none;

    // Shared full-screen off-screen buffer for mutually exclusive pages.
    // 320x240 RGB565 is about 153,600 bytes, so Settings and Precision/Keypad
    // must not keep separate persistent sprites.
    M5Canvas fullscreen_canvas;
    bool fullscreen_canvas_ready = false;
    bool fullscreen_canvas_allocation_attempted = false;
    bool network_canvas_reserved = false;
    void* fullscreen_canvas_buffer = nullptr;
    bool settings_redraw_pending = false;
    // Stage 5C Settings network fields begin.
    uint32_t settings_network_revision = UINT32_MAX;
    uint64_t settings_device_revision = UINT64_MAX;
    bool wifi_reset_active = false, wifi_reset_canceled = false, wifi_reset_completed = false;
    bool wifi_reset_failed = false;
    uint32_t wifi_reset_started_ms = 0;
    // Stage 5C Settings network fields end.

    bool precision_editor_screen = false;
    bool precision_keypad_screen = false;
    bool precision_redraw_pending = false;
    bool precision_consume_release = false;
    bool main_touch_consume_release = false;
    bool main_value_slider_dragging = false;
    bool precision_slider_dragging = false;
    int precision_step = 1;
    int precision_repeat_dir = 0;
    uint32_t precision_repeat_started_ms = 0;
    uint32_t precision_repeat_last_ms = 0;
    char keypad_value[4] = "";
    uint8_t keypad_len = 0;
    bool keypad_invalid = false;

    uint16_t enable_channels[DMX_MAX_PACKET_SIZE];
    uint16_t enable_channel_count;

    uint8_t visible[DMX_MAX_PACKET_SIZE];
    // Last values painted into the grid; never an authoritative universe.
    uint8_t rendered_values[DMX_MAX_PACKET_SIZE] = {};
    uint16_t target_channel_index = 0;
    uint16_t target_channel = 1;
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
    static constexpr uint8_t slider_y            = 42;
    static constexpr uint8_t slider_btn_height   = 32;

    static constexpr int value_slider_touch_x = slider_x - 12;
    static constexpr int value_slider_touch_y = slider_y - 8;
    static constexpr int value_slider_touch_w = slider_w + 24;
    static constexpr int value_slider_touch_h = 144;

    static constexpr int value_edit_button_x = scroll_width + 5;
    static constexpr int value_edit_button_y = 182;
    static constexpr int value_edit_button_w = 320 - scroll_width - 10;
    static constexpr int value_edit_button_h = 50;

    static constexpr int precision_back_x = scroll_width + 5;
    static constexpr int precision_back_y = 182;
    static constexpr int precision_back_w = 320 - scroll_width - 10;
    static constexpr int precision_back_h = 50;

    static constexpr int precision_minus_x = 16;
    static constexpr int precision_minus_y = 104;
    static constexpr int precision_button_w = 98;
    static constexpr int precision_button_h = 45;
    static constexpr int precision_plus_x = 136;
    static constexpr int precision_plus_y = precision_minus_y;

    static constexpr int precision_value_x = 57;
    static constexpr int precision_value_y = 36;
    static constexpr int precision_value_w = 136;
    static constexpr int precision_value_h = 54;

    static constexpr int precision_slider_x = slider_x;
    static constexpr int precision_slider_y = slider_y;
    static constexpr int precision_slider_w = slider_w;
    static constexpr int precision_slider_h = 128;

    static constexpr int precision_step_y = precision_back_y;
    static constexpr int precision_step_w = 54;
    static constexpr int precision_step_h = 36;
    static constexpr int precision_step_gap = 6;
    static constexpr int precision_step_x0 = 10;

    static constexpr int keypad_digit_x0 = 12;
    static constexpr int keypad_digit_y0 = 78;
    static constexpr int keypad_digit_w = 66;
    static constexpr int keypad_digit_h = 30;
    static constexpr int keypad_digit_gap_x = 8;
    static constexpr int keypad_digit_gap_y = 8;

    static constexpr int keypad_action_x = precision_back_x;
    static constexpr int keypad_action_w = precision_back_w;
    static constexpr int keypad_action_h = precision_back_h;
    static constexpr int keypad_clr_y = 54;
    static constexpr int keypad_ok_y = 118;

    static constexpr uint32_t precision_hold_start_ms = 350;
    static constexpr uint32_t precision_repeat_slow_ms = 180;
    static constexpr uint32_t precision_repeat_fast_ms = 65;

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

    bool blackout_tap_armed = false;
    uint32_t blackout_first_tap_ms = 0;

    // ==========================
    // SETTINGS SCREEN
    // ==========================

    bool settings_screen = false;

    // Settings are drawn in a vertically scrollable content area.
    // More settings can simply be added lower down later.
    static constexpr int settings_content_h = 540;
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

bool presetExists(uint8_t slot) {
    return controller.presetExists(slot);
}

const char* presetError(DmxController::Result result) {
    switch (result) {
        case DmxController::Result::corrupt: return "PRESET CORRUPT";
        case DmxController::Result::metadata_failed: return "PRESET METADATA FAILED";
        case DmxController::Result::read_failed: return "PRESET READ FAILED";
        default: return "SAVE FAILED";
    }
}

bool commandSucceeded(const CommandDispatcher::Result& result) {
    return result.status == CommandStatus::applied || result.status == CommandStatus::no_change;
}

const char* commandError(const CommandDispatcher::Result& result) {
    if (result.status == CommandStatus::busy) return "COMMAND BUSY";
    if (result.status == CommandStatus::unsupported_mode) return "SENDER MODE REQUIRED";
    if (result.status == CommandStatus::transport_error) return "DMX WRITE FAILED";
    if (result.status == CommandStatus::invalid) return "INVALID COMMAND";
    return presetError(result.domain);
}

bool clearPreset(uint8_t slot) {
    if (slot >= DmxController::preset_count) return false;
    const auto outcome = dispatcher.executeLocal(ControllerCommand::presetCommand(CommandType::clear_preset, slot));
    const auto result = outcome.domain;
    if (result == DmxController::Result::empty) {
        snprintf(preset_status, sizeof(preset_status), "P%u IS ALREADY EMPTY",
                 static_cast<unsigned>(slot + 1));
    } else if (commandSucceeded(outcome)) {
        snprintf(preset_status, sizeof(preset_status), "PRESET P%u CLEARED",
                 static_cast<unsigned>(slot + 1));
    } else {
        snprintf(preset_status, sizeof(preset_status), "%s", commandError(outcome));
    }
    return commandSucceeded(outcome);
}

bool copyPreset(uint8_t source, uint8_t target) {
    if (source >= DmxController::preset_count || target >= DmxController::preset_count) return false;
    const auto outcome = dispatcher.executeLocal(ControllerCommand::presetCommand(CommandType::copy_preset, source, target));
    const auto result = outcome.domain;
    if (!commandSucceeded(outcome) && result == DmxController::Result::ok) {
        snprintf(preset_status, sizeof(preset_status), "%s", commandError(outcome));
        return false;
    }
    switch (result) {
        case DmxController::Result::ok:
            snprintf(preset_status, sizeof(preset_status), "P%u COPIED TO P%u",
                     static_cast<unsigned>(source + 1), static_cast<unsigned>(target + 1));
            break;
        case DmxController::Result::empty:
            snprintf(preset_status, sizeof(preset_status), "P%u IS EMPTY",
                     static_cast<unsigned>(source + 1));
            break;
        case DmxController::Result::same_slot:
            snprintf(preset_status, sizeof(preset_status), "CHOOSE ANOTHER SLOT");
            break;
        case DmxController::Result::occupied:
            snprintf(preset_status, sizeof(preset_status), "P%u OCCUPIED - CLEAR FIRST",
                     static_cast<unsigned>(target + 1));
            break;
        case DmxController::Result::write_failed:
            snprintf(preset_status, sizeof(preset_status), "COPY FAILED");
            break;
        default:
            snprintf(preset_status, sizeof(preset_status), "%s", commandError(outcome));
            break;
    }
    return commandSucceeded(outcome);
}

bool savePreset(uint8_t slot) {
    if (slot >= DmxController::preset_count) return false;
    const auto outcome = dispatcher.executeLocal(ControllerCommand::presetCommand(CommandType::save_preset, slot));
    const bool success = commandSucceeded(outcome);
    if (success) {
        snprintf(preset_status, sizeof(preset_status), "PRESET P%u SAVED",
                 static_cast<unsigned>(slot + 1));
    } else {
        snprintf(preset_status, sizeof(preset_status), "%s", commandError(outcome));
    }
#if DMX_HW_DIAGNOSTICS
    recordHardwareDiagnostics("preset_save", slot + 1, success ? ESP_OK : ESP_FAIL);
#endif
    return success;
}

bool loadPreset(uint8_t slot) {
    if (slot >= DmxController::preset_count) return false;
    const auto outcome = dispatcher.executeLocal(ControllerCommand::presetCommand(CommandType::load_preset, slot));
    if (!commandSucceeded(outcome)) {
        if (outcome.domain == DmxController::Result::empty) {
            snprintf(preset_status, sizeof(preset_status), "PRESET P%u IS EMPTY",
                     static_cast<unsigned>(slot + 1));
        } else {
            snprintf(preset_status, sizeof(preset_status), "%s", commandError(outcome));
        }
#if DMX_HW_DIAGNOSTICS
        recordHardwareDiagnostics("preset_load", slot + 1,
                                  outcome.transport_result ? outcome.transport_result : ESP_FAIL);
#endif
        return false;
    }

    snprintf(preset_status, sizeof(preset_status), "PRESET P%u LOADED",
             static_cast<unsigned>(slot + 1));
#if DMX_HW_DIAGNOSTICS
    recordHardwareDiagnostics("preset_load", slot + 1);
#endif
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
                target_value = controller.channel(target_channel);
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

    esp_err_t writeCurrentDmxOutput(bool barrier) {
        return transport.stage(controller.outputPacket(), DMX_MAX_PACKET_SIZE, barrier)
            ? ESP_OK : ESP_ERR_INVALID_STATE;
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

        if (controller.blackoutActive()) {
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
        if (!controller.blackoutActive()) {
            dispatcher.executeLocal(ControllerCommand::blackoutCommand(true));

            if (!M5.Speaker.isPlaying()) {
                M5.Speaker.tone(700, 180);
            }
        } else {
            dispatcher.executeLocal(ControllerCommand::blackoutCommand(false));

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
        // Legacy staging/wait sequence, NOT a guarantee of three zero frames.
        // Do not add new starts or change power-action timing in this milestone.
        uint8_t zero_packet[DMX_MAX_PACKET_SIZE] = {};
        zero_packet[0] = 0;

        for (int i = 0; i < 3; ++i) {
            transport.stage(zero_packet, DMX_MAX_PACKET_SIZE);

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
        if (active_fullscreen_page != fullscreen_page_settings) {
            settings_redraw_pending = false;
            return;
        }

        // Protect short speaker tones from the relatively heavy full-screen
        // canvas transfer. The next UI loop will redraw once audio finishes.
        if (M5.Speaker.isPlaying()) {
            settings_redraw_pending = true;
            return;
        }

        M5Canvas* frame = getFullscreenCanvas(
            fullscreen_page_settings,
            "SETTINGS"
        );

        if (frame == nullptr) {
            settings_redraw_pending = true;
            return;
        }

        auto& c = *frame;

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
                    deviceSettings().value(DeviceSetting::brightness)
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
                deviceSettings().value(DeviceSetting::brightness),
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

            if (deviceSettings().value(DeviceSetting::timeout) == 0) {
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
                        deviceSettings().value(DeviceSetting::timeout)
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
            (deviceSettings().value(DeviceSetting::timeout) == 0)
                ? 5
                : deviceSettings().value(DeviceSetting::timeout);

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
                    deviceSettings().value(DeviceSetting::volume)
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
                deviceSettings().value(DeviceSetting::volume)
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

        // Stage 5 local network information begin (no HTTP/log credentials).
        const auto network = network_service::status();
        c.setFont(&fonts::Font0);
        c.setTextDatum(textdatum_t::middle_center);
        c.setTextColor(TFT_WHITE, TFT_BLACK);
        char network_line[64];
        auto networkText = [&](const char* text, int y) {
            const int screen_y = sy(y);
            if (screen_y > -10 && screen_y < 250) c.drawString(text, settings_content_center_x, screen_y);
        };
        settings_network_revision = network.ui_revision;
        if (network.phase == network_service::Phase::ready)
            snprintf(network_line, sizeof(network_line), "WiFi %s - Clients %lu/%lu", network_service::phaseName(network.phase), (unsigned long)network.clients, (unsigned long)network.max_clients);
        else
            snprintf(network_line, sizeof(network_line), "WiFi %s (%d)", network_service::phaseName(network.phase), network.error);
        networkText(network_line, 316);
        networkText(network_service::ssid(), 334);
        snprintf(network_line, sizeof(network_line), "KEY %s", network_service::password());
        networkText(network_line, 352);
        snprintf(network_line, sizeof(network_line), "http://%s/", network.ip[0] ? network.ip : "not-ready");
        networkText(network_line, 370);
        networkText(!strcmp(network_service::credentialMode(), "LEGACY") ? "LEGACY KEY: RESET WIFI TO CHANGE" : "DEFAULT KEY IS PREDICTABLE", 388);
        snprintf(network_line, sizeof(network_line), "INT %lu DMA %lu", (unsigned long)network.current.internal, (unsigned long)network.current.dma);
        networkText(network_line, 406);
        networkText("/status FOR MEMORY HISTORY", 424);
        const int reset_y = sy(450);
        c.drawRoundRect(24, reset_y, 216, 28, 5, wifi_reset_active ? TFT_GREEN : TFT_WHITE);
        networkText(wifi_reset_active && !wifi_reset_canceled && !wifi_reset_completed ? "KEEP HOLDING..." : "HOLD RESET WIFI 2s", 464);
        networkText(network.reset_pending ? "RESET SAVED: RESTART TO APPLY" : (wifi_reset_failed ? "RESET FAILED - TRY AGAIN" : "WIFI RESET APPLIES NEXT BOOT"), 492);
        networkText("WIFI ONLY - PRESETS ARE KEPT", 510);
        // Stage 5 local network information end.

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
        pushFullscreenCanvas(
            fullscreen_page_settings,
            "SETTINGS"
        );

        settings_device_revision = deviceSettings().snapshot().revision;
        settings_redraw_pending = false;
    }


    void openSettingsScreen(void) {
        resetPrecisionState();
        setActiveFullscreenPage(fullscreen_page_settings);
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
        setActiveFullscreenPage(fullscreen_page_none);
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
#if DMX_HW_DIAGNOSTICS
        recordHardwareDiagnostics("return_main_settings");
#endif
    }


    // Stage 5C Settings network methods begin.
    void serviceSettingsNetwork() {
        const uint32_t revision = network_service::uiRevision();
        if (revision == settings_network_revision) return;
        settings_network_revision = revision;
        const int status_y = 316 - settings_scroll_y;
        if (status_y >= 0 && status_y < 240) settings_redraw_pending = true;
    }

    bool handleWifiResetTouch(const decltype(M5.Touch.getDetail())& tp, int content_y) {
        const bool inside = tp.x >= 24 && tp.x < 240 && content_y >= 450 && content_y < 478;
        if (tp.wasPressed() && inside) {
            wifi_reset_active = true; wifi_reset_canceled = wifi_reset_completed = wifi_reset_failed = false;
            wifi_reset_started_ms = millis();
            settings_scroll_add = 0; settings_drag_mode = settings_drag_none;
            drawSettingsScreen();
        }
        if (!wifi_reset_active) return false;
        if (tp.state && (!inside || abs(tp.x - tp.base_x) >= settings_gesture_threshold || abs(tp.y - tp.base_y) >= settings_gesture_threshold))
            wifi_reset_canceled = true;
        if (tp.state && !wifi_reset_canceled && !wifi_reset_completed && uint32_t(millis() - wifi_reset_started_ms) >= 2000) {
            wifi_reset_completed = true;
            wifi_reset_failed = !network_service::requestFactoryReset();
            drawSettingsScreen();
        }
        if (!tp.state) { wifi_reset_active = false; drawSettingsScreen(); }
        return true; // Consume press/hold/release, including canceled gestures.
    }
    // Stage 5C Settings network methods end.

    void handleSettingsTouch(
        const decltype(M5.Touch.getDetail())& tp
    ) {
        int touch_x = tp.x;
        int touch_y = tp.y;
        int content_y =
            touch_y + settings_scroll_y;

        // Stage 5C Settings network touch begin.
        if (handleWifiResetTouch(tp, content_y)) return;
        // Stage 5C Settings network touch end.

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
                deviceSettings().value(DeviceSetting::brightness)) {

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
                deviceSettings().value(DeviceSetting::timeout)) {

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
                deviceSettings().value(DeviceSetting::volume)) {

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
                    deviceSettings().value(DeviceSetting::brightness)) {

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
                    deviceSettings().value(DeviceSetting::timeout)) {

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
                    deviceSettings().value(DeviceSetting::volume)) {

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
        preset_double_tap.reset();
        resetPrecisionState();
        setActiveFullscreenPage(fullscreen_page_preset);
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

        drawPresetScreen();
    }

    void closePresetScreen(void) {
        preset_double_tap.reset();
        preset_screen = false;
        setActiveFullscreenPage(fullscreen_page_none);

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
        target_value = controller.channel(target_channel);

        updateDisplay(true);
        hideUIValueSet();
#if DMX_HW_DIAGNOSTICS
        recordHardwareDiagnostics("return_main_presets");
#endif
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
        for (uint8_t slot = 0; slot < DmxController::preset_count; ++slot) {
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

            // Loaded provenance is independent of the white selection outline.
            if (controller.loadedPreset() == slot + 1) {
                M5.Display.setFont(&fonts::Font0);
                M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
                M5.Display.drawString(controller.loadedPresetModified() ? "MODIFIED" : "LOADED",
                    x + (preset_slot_w >> 1), y + preset_slot_h - 7);
            }

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
             slot < DmxController::preset_count;
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

                const auto state = controller.presetState(selected_preset);
                if (state != DmxPresetStorage::SlotState::valid) {
                    if (state == DmxPresetStorage::SlotState::missing) {
                        snprintf(preset_status, sizeof(preset_status), "P%u IS EMPTY",
                                 static_cast<unsigned>(selected_preset + 1));
                    } else {
                        snprintf(preset_status, sizeof(preset_status), "%s",
                                 state == DmxPresetStorage::SlotState::corrupt
                                     ? "PRESET CORRUPT" : "PRESET READ FAILED");
                    }

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

#if !DMX_HW_DIAGNOSTICS
                Serial.println(preset_status);
#endif

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
#if !DMX_HW_DIAGNOSTICS
                Serial.println(preset_status);
#endif

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


    void handlePresetScreenTouch(int touch_x, int touch_y, bool double_load) {
        // ------------------------------------------------------
        // Select P1-P8
        // ------------------------------------------------------
        for (uint8_t slot = 0; slot < DmxController::preset_count; ++slot) {
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
                if (double_load && presetExists(slot)) loadPreset(slot);
                else preset_status[0] = '\0';

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

#if !DMX_HW_DIAGNOSTICS
                Serial.println(preset_status);
#endif

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

    int clampDmxValue(int value) {
        if (value < 0) {
            return 0;
        }

        if (value > 255) {
            return 255;
        }

        return value;
    }

    bool pointInRect(int x, int y, int rx, int ry, int rw, int rh) {
        return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
    }

    void setActiveFullscreenPage(fullscreen_page_t page) {
        active_fullscreen_page = page;

        if (page != fullscreen_page_settings) {
            settings_redraw_pending = false;
        }

        if (page != fullscreen_page_precision &&
            page != fullscreen_page_keypad) {

            precision_redraw_pending = false;
        }
    }

#if DMX_HW_DIAGNOSTICS
    bool diagnostics_settings_seen = false;
    bool diagnostics_precision_seen = false;
    bool diagnostics_keypad_seen = false;
    bool diagnostics_allocation_failure_seen = false;

    void recordHardwareDiagnostics(const char* event, uint8_t preset = 0,
                                   int result = ESP_OK) {
        const char* owner = "none";
        switch (active_fullscreen_page) {
            case fullscreen_page_settings: owner = "settings"; break;
            case fullscreen_page_precision: owner = "precision"; break;
            case fullscreen_page_keypad: owner = "keypad"; break;
            case fullscreen_page_preset: owner = "presets"; break;
            default: break;
        }
        const char* screen = owner;
        if (active_fullscreen_page == fullscreen_page_none) {
            screen = ui_mode == ui_mode_t::mode_value_setting ? "main_value" : "main";
        } else if (preset_options_screen) {
            screen = "preset_options";
        }
        hardware_diagnostics::capture(event, screen, owner, fullscreen_canvas_ready,
                                      fullscreen_canvas_buffer, preset, result,
                                      controller.blackoutActive());
    }
#endif

    M5Canvas* getFullscreenCanvas(
        fullscreen_page_t owner,
        const char* label
    ) {
        if (active_fullscreen_page != owner) {
            return nullptr;
        }

        if (!fullscreen_canvas_ready && !fullscreen_canvas_allocation_attempted) {
#if !DMX_HW_DIAGNOSTICS
            uint32_t heap_before = ESP.getFreeHeap();
            uint32_t psram_before = ESP.getFreePsram();
#endif

            fullscreen_canvas_buffer = fullscreen_canvas_memory::allocateOnce(
                fullscreen_canvas,
                fullscreen_canvas_allocation_attempted,
                M5.Display.width(),
                M5.Display.height(),
                esp_ptr_external_ram
            );
            fullscreen_canvas_ready = fullscreen_canvas_buffer != nullptr;

#if !DMX_HW_DIAGNOSTICS
            Serial.printf(
                "fullscreen_canvas create for %s: buffer=%p heap %lu->%lu psram %lu->%lu active=%d settings=%d precision=%d keypad=%d preset=%d\n",
                label,
                fullscreen_canvas_buffer,
                static_cast<unsigned long>(heap_before),
                static_cast<unsigned long>(ESP.getFreeHeap()),
                static_cast<unsigned long>(psram_before),
                static_cast<unsigned long>(ESP.getFreePsram()),
                static_cast<int>(active_fullscreen_page),
                settings_screen ? 1 : 0,
                precision_editor_screen ? 1 : 0,
                precision_keypad_screen ? 1 : 0,
                preset_screen ? 1 : 0
            );
#endif
        }

        if (!fullscreen_canvas_ready) {
#if DMX_HW_DIAGNOSTICS
            if (!diagnostics_allocation_failure_seen) {
                diagnostics_allocation_failure_seen = true;
                recordHardwareDiagnostics("canvas_alloc_failed", 0, ESP_ERR_NO_MEM);
            }
#endif
            return nullptr;
        }

        return &fullscreen_canvas;
    }

    void pushFullscreenCanvas(
        fullscreen_page_t owner,
        const char* label
    ) {
        if (active_fullscreen_page != owner) {
            return;
        }

#if !DMX_HW_DIAGNOSTICS
        Serial.printf(
            "fullscreen_canvas push %s: buffer=%p active=%d settings=%d precision=%d keypad=%d preset=%d\n",
            label,
            fullscreen_canvas_buffer,
            static_cast<int>(active_fullscreen_page),
            settings_screen ? 1 : 0,
            precision_editor_screen ? 1 : 0,
            precision_keypad_screen ? 1 : 0,
            preset_screen ? 1 : 0
        );
#endif

        M5.Display.startWrite();
        fullscreen_canvas.pushSprite(&M5.Display, 0, 0);
        M5.Display.endWrite();
#if DMX_HW_DIAGNOSTICS
        if (owner == fullscreen_page_settings && !diagnostics_settings_seen) {
            diagnostics_settings_seen = true;
            recordHardwareDiagnostics("settings_first");
        } else if (owner == fullscreen_page_precision && !diagnostics_precision_seen) {
            diagnostics_precision_seen = true;
            recordHardwareDiagnostics("precision_first");
        } else if (owner == fullscreen_page_keypad && !diagnostics_keypad_seen) {
            diagnostics_keypad_seen = true;
            recordHardwareDiagnostics("keypad_first");
        }
#endif
    }

    void applyLiveDmxValue(int value) {
        int new_value = clampDmxValue(value);

        if (new_value == target_value &&
            controller.channel(target_channel) == new_value) {
            return;
        }

        dispatcher.executeLocal(ControllerCommand::channelCommand(
            CommandType::set_channel, target_channel, new_value));
        target_value = controller.channel(target_channel);

        visible[target_channel] = true;
    }

    int precisionValueFromY(int y) {
        if (y < precision_slider_y) {
            y = precision_slider_y;
        }

        if (y > precision_slider_y + precision_slider_h) {
            y = precision_slider_y + precision_slider_h;
        }

        return clampDmxValue(
            255 - ((y - precision_slider_y) << 1)
        );
    }

    void drawPrecisionButton(
        LovyanGFX* gfx,
        int x,
        int y,
        int w,
        int h,
        const char* label,
        bool active = false,
        bool large_label = false
    ) {
        uint16_t rim = active ? TFT_WHITE : TFT_DARKGRAY;
        uint16_t bg = active ? TFT_WHITE : TFT_BLACK;
        uint16_t fg = active ? TFT_BLACK : TFT_WHITE;

        gfx->fillRoundRect(x, y, w, h, 5, bg);
        gfx->drawRoundRect(x, y, w, h, 5, rim);
        gfx->setTextDatum(textdatum_t::middle_center);
        gfx->setTextColor(fg, bg);
        if (large_label) {
            gfx->setFont(&fonts::Font4);
        } else {
            gfx->setFont(&fonts::Font0);
        }
        gfx->drawString(label, x + (w >> 1), y + (h >> 1));
    }

    void drawValueSliderFull(
        LovyanGFX* gfx,
        int value,
        size_t ch
    ) {
        int y = ((256 - clampDmxValue(value)) >> 1) + slider_y;
        int bottom_y = slider_y + 128;

        gfx->setColor(TFT_WHITE);
        drawFocusBox(
            gfx,
            slider_x - 8,
            slider_y - 5,
            slider_w + 16,
            128 + 10,
            2
        );

        fillBar(
            gfx,
            slider_x,
            slider_y,
            slider_w,
            y - slider_y,
            ch
        );

        fillBar(
            gfx,
            slider_x,
            bottom_y,
            slider_w,
            y - bottom_y,
            ch
        );
    }

    void resetPrecisionState(void) {
        precision_editor_screen = false;
        precision_keypad_screen = false;
        precision_redraw_pending = false;
        precision_consume_release = false;
        precision_slider_dragging = false;
        precision_repeat_dir = 0;
        keypad_value[0] = '\0';
        keypad_len = 0;
        keypad_invalid = false;

        if (active_fullscreen_page == fullscreen_page_precision ||
            active_fullscreen_page == fullscreen_page_keypad) {

            setActiveFullscreenPage(fullscreen_page_none);
        }
    }

    void drawPrecisionKeypad(M5Canvas& c) {
        c.fillScreen(TFT_BLACK);
        c.setTextDatum(textdatum_t::middle_center);
        c.setTextColor(TFT_WHITE, TFT_BLACK);
        c.setFont(&fonts::Font2);
        c.drawString("ENTER DMX VALUE", 125, 18);

        c.setFont(&fonts::Font4);
        c.drawRoundRect(
            57,
            36,
            136,
            36,
            5,
            keypad_invalid ? TFT_RED : TFT_DARKGRAY
        );
        c.setTextColor(
            keypad_invalid ? TFT_RED : TFT_WHITE,
            TFT_BLACK
        );
        c.drawString(keypad_len ? keypad_value : "---", 125, 54);
        c.setTextColor(TFT_WHITE, TFT_BLACK);

        if (keypad_invalid) {
            c.setFont(&fonts::Font0);
            c.drawString("MAX 255", 125, 72);
        }

        static constexpr const char* labels[10] = {
            "1", "2", "3",
            "4", "5", "6",
            "7", "8", "9",
            "0"
        };

        for (int i = 0; i < 10; ++i) {
            int col = (i == 9) ? 1 : i % 3;
            int row = (i == 9) ? 3 : i / 3;
            int x = keypad_digit_x0 +
                    col * (keypad_digit_w + keypad_digit_gap_x);
            int y = keypad_digit_y0 +
                    row * (keypad_digit_h + keypad_digit_gap_y);
            drawPrecisionButton(
                &c,
                x,
                y,
                keypad_digit_w,
                keypad_digit_h,
                labels[i],
                false
            );
        }

        drawPrecisionButton(
            &c,
            keypad_action_x,
            keypad_clr_y,
            keypad_action_w,
            keypad_action_h,
            "CLR"
        );
        drawPrecisionButton(
            &c,
            keypad_action_x,
            keypad_ok_y,
            keypad_action_w,
            keypad_action_h,
            "OK",
            true
        );
        drawPrecisionButton(
            &c,
            precision_back_x,
            precision_back_y,
            precision_back_w,
            precision_back_h,
            "BACK"
        );
    }

    void drawPrecisionEditor(void) {
        fullscreen_page_t owner =
            precision_keypad_screen
                ? fullscreen_page_keypad
                : fullscreen_page_precision;

        if (!precision_editor_screen ||
            active_fullscreen_page != owner) {

            precision_redraw_pending = false;
            return;
        }

        if (M5.Speaker.isPlaying()) {
            precision_redraw_pending = true;
            return;
        }

        M5Canvas* frame = getFullscreenCanvas(owner, "PRECISION");

        if (frame == nullptr) {
            precision_redraw_pending = true;
            return;
        }

        auto& c = *frame;

        if (precision_keypad_screen) {
            drawPrecisionKeypad(c);
            pushFullscreenCanvas(owner, "KEYPAD");
            precision_redraw_pending = false;
            return;
        }

        c.fillScreen(TFT_BLACK);
        c.setTextDatum(textdatum_t::middle_center);
        c.setTextColor(TFT_WHITE, TFT_BLACK);

        char text[20];
        c.setFont(&fonts::Font2);
        snprintf(text, sizeof(text), "CH %u", target_channel);
        c.drawString(
            text,
            precision_back_x + (precision_back_w >> 1),
            18
        );

        c.setFont(&fonts::Font7);
        snprintf(text, sizeof(text), "%d", target_value);
        c.drawRoundRect(
            precision_value_x,
            precision_value_y,
            precision_value_w,
            precision_value_h,
            5,
            TFT_DARKGRAY
        );
        c.drawString(
            text,
            precision_value_x + (precision_value_w >> 1),
            precision_value_y + (precision_value_h >> 1)
        );

        drawPrecisionButton(
            &c,
            precision_minus_x,
            precision_minus_y,
            precision_button_w,
            precision_button_h,
            "-",
            false,
            true
        );
        drawPrecisionButton(
            &c,
            precision_plus_x,
            precision_plus_y,
            precision_button_w,
            precision_button_h,
            "+",
            false,
            true
        );

        static constexpr int steps[4] = {1, 5, 10, 25};
        c.setFont(&fonts::Font0);
        for (int i = 0; i < 4; ++i) {
            int x =
                precision_step_x0 +
                i * (precision_step_w + precision_step_gap);
            char step_text[4];
            snprintf(step_text, sizeof(step_text), "%d", steps[i]);
            drawPrecisionButton(
                &c,
                x,
                precision_step_y,
                precision_step_w,
                precision_step_h,
                step_text,
                precision_step == steps[i]
            );
        }

        drawValueSliderFull(&c, target_value, target_channel);

        drawPrecisionButton(
            &c,
            precision_back_x,
            precision_back_y,
            precision_back_w,
            precision_back_h,
            "BACK"
        );

        pushFullscreenCanvas(owner, "PRECISION");
        precision_redraw_pending = false;
    }

    void openPrecisionEditor(void) {
        resetPrecisionState();
        setActiveFullscreenPage(fullscreen_page_precision);
        precision_editor_screen = true;
        precision_consume_release = true;
        target_value = controller.channel(target_channel);
        drawPrecisionEditor();
    }

    void closePrecisionEditor(void) {
        resetPrecisionState();
        setActiveFullscreenPage(fullscreen_page_none);
        main_touch_consume_release = true;
        M5.Display.fillScreen(TFT_BLACK);
        updateDisplay(true);
        drawUIValueSet(target_value, true);
#if DMX_HW_DIAGNOSTICS
        recordHardwareDiagnostics("return_main_precision");
#endif
    }

    void openPrecisionKeypad(void) {
        setActiveFullscreenPage(fullscreen_page_keypad);
        precision_keypad_screen = true;
        precision_consume_release = true;
        keypad_value[0] = '\0';
        keypad_len = 0;
        keypad_invalid = false;
        drawPrecisionEditor();
    }

    void closePrecisionKeypad(bool apply_value) {
        if (apply_value && keypad_len > 0) {
            int value = atoi(keypad_value);
            if (value <= 255) {
                applyLiveDmxValue(value);
            } else {
                keypad_invalid = true;
                drawPrecisionEditor();
                return;
            }
        }

        precision_keypad_screen = false;
        setActiveFullscreenPage(fullscreen_page_precision);
        precision_consume_release = true;
        drawPrecisionEditor();
    }

    void appendKeypadDigit(char digit) {
        if (keypad_len >= 3) {
            return;
        }

        keypad_invalid = false;
        keypad_value[keypad_len++] = digit;
        keypad_value[keypad_len] = '\0';
        drawPrecisionEditor();
    }

    void handlePrecisionKeypadTouch(int x, int y) {
        if (pointInRect(
                x,
                y,
                precision_back_x,
                precision_back_y,
                precision_back_w,
                precision_back_h)) {

            closePrecisionKeypad(false);
            return;
        }

        if (pointInRect(
                x,
                y,
                keypad_action_x,
                keypad_clr_y,
                keypad_action_w,
                keypad_action_h)) {

            keypad_len = 0;
            keypad_value[0] = '\0';
            keypad_invalid = false;
            drawPrecisionEditor();
            return;
        }

        if (pointInRect(
                x,
                y,
                keypad_action_x,
                keypad_ok_y,
                keypad_action_w,
                keypad_action_h)) {

            closePrecisionKeypad(true);
            return;
        }

        static constexpr const char keys[10] = {
            '1', '2', '3',
            '4', '5', '6',
            '7', '8', '9',
            '0'
        };

        for (int i = 0; i < 10; ++i) {
            int col = (i == 9) ? 1 : i % 3;
            int row = (i == 9) ? 3 : i / 3;
            int bx = keypad_digit_x0 +
                     col * (keypad_digit_w + keypad_digit_gap_x);
            int by = keypad_digit_y0 +
                     row * (keypad_digit_h + keypad_digit_gap_y);

            if (!pointInRect(
                    x,
                    y,
                    bx,
                    by,
                    keypad_digit_w,
                    keypad_digit_h)) {
                continue;
            }

            char key = keys[i];
            appendKeypadDigit(key);
            return;
        }
    }

    void applyPrecisionStep(int dir) {
        dispatcher.executeLocal(ControllerCommand::channelCommand(
            CommandType::adjust_channel, target_channel, dir * precision_step));
        target_value = controller.channel(target_channel);
        visible[target_channel] = true;
        drawPrecisionEditor();
    }

    void handlePrecisionRepeat(const m5::touch_detail_t& tp) {
        int dir = 0;

        if (pointInRect(
                tp.x,
                tp.y,
                precision_minus_x,
                precision_minus_y,
                precision_button_w,
                precision_button_h)) {

            dir = -1;
        } else if (pointInRect(
                       tp.x,
                       tp.y,
                       precision_plus_x,
                       precision_plus_y,
                       precision_button_w,
                       precision_button_h)) {

            dir = 1;
        }

        uint32_t now = millis();

        if (!tp.state || dir == 0) {
            precision_repeat_dir = 0;
            return;
        }

        if (tp.wasPressed() || precision_repeat_dir != dir) {
            precision_repeat_dir = dir;
            precision_repeat_started_ms = now;
            precision_repeat_last_ms = now;
            applyPrecisionStep(dir);
            return;
        }

        if (precision_repeat_dir == 0) {
            return;
        }

        uint32_t held = now - precision_repeat_started_ms;
        if (held < precision_hold_start_ms) {
            return;
        }

        uint32_t interval = precision_repeat_slow_ms;
        if (precision_step <= 5 && held > 1400) {
            interval = precision_repeat_fast_ms;
        }

        if ((uint32_t)(now - precision_repeat_last_ms) >= interval) {
            precision_repeat_last_ms = now;
            applyPrecisionStep(dir);
        }
    }

    void handlePrecisionEditorTouch(const m5::touch_detail_t& tp) {
        if (precision_consume_release) {
            if (!tp.state) {
                precision_consume_release = false;
            }
            return;
        }

        if (precision_keypad_screen) {
            if (tp.state && tp.wasClicked()) {
                handlePrecisionKeypadTouch(tp.base_x, tp.base_y);
            }
            return;
        }

        handlePrecisionRepeat(tp);

        if (!tp.state) {
            precision_slider_dragging = false;
            return;
        }

        if (tp.wasClicked()) {
            if (pointInRect(
                    tp.base_x,
                    tp.base_y,
                    precision_back_x,
                    precision_back_y,
                    precision_back_w,
                    precision_back_h)) {

                closePrecisionEditor();
                return;
            }

            if (pointInRect(
                    tp.base_x,
                    tp.base_y,
                    precision_value_x,
                    precision_value_y,
                    precision_value_w,
                    precision_value_h)) {

                openPrecisionKeypad();
                return;
            }

            static constexpr int steps[4] = {1, 5, 10, 25};
            for (int i = 0; i < 4; ++i) {
                int x =
                    precision_step_x0 +
                    i * (precision_step_w + precision_step_gap);

                if (pointInRect(
                        tp.base_x,
                        tp.base_y,
                        x,
                        precision_step_y,
                        precision_step_w,
                        precision_step_h)) {

                    precision_step = steps[i];
                    drawPrecisionEditor();
                    return;
                }
            }
        }

        bool slider_hit =
            pointInRect(
                tp.base_x,
                tp.base_y,
                precision_slider_x - 12,
                precision_slider_y - 8,
                precision_slider_w + 24,
                precision_slider_h + 16
            );

        if (tp.wasPressed() && slider_hit) {
            precision_slider_dragging = true;
        }

        if (precision_slider_dragging) {
            int value = precisionValueFromY(tp.y);
            if (value != target_value) {
                applyLiveDmxValue(value);
                drawPrecisionEditor();
            }
        }
    }

    void hideUIValueSet(void) {
        // Channel-select mode shows a PRESET button on the right.
        drawPresetOpenButton();
    }

    void drawUIValueUpDown(int sign = 0) {
        (void)sign;

        M5.Display.fillRoundRect(
            value_edit_button_x,
            value_edit_button_y,
            value_edit_button_w,
            value_edit_button_h,
            6,
            TFT_BLACK
        );
        M5.Display.drawRoundRect(
            value_edit_button_x,
            value_edit_button_y,
            value_edit_button_w,
            value_edit_button_h,
            6,
            TFT_WHITE
        );
        M5.Display.setTextDatum(textdatum_t::middle_center);
        M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
        M5.Display.setFont(&fonts::Font0);
        M5.Display.drawString(
            "EDIT",
            value_edit_button_x + (value_edit_button_w >> 1),
            value_edit_button_y + (value_edit_button_h >> 1)
        );
    }

    void drawUIValueSet(uint16_t new_value, bool force_redraw = false) {
        M5.Display.setTextDatum(textdatum_t::middle_center);
        M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
        M5.Display.setFont(&fonts::AsciiFont8x16);
        char str[8];

        int y  = ((256 - new_value) >> 1) + slider_y;
        int py = ((256 - target_value) >> 1) + slider_y;

        int text_x = (scroll_width + M5.Display.width()) >> 1;
        int text_y = 18;
        if (force_redraw) {
            M5.Display.fillRect(scroll_width + 1, 0, 320 - scroll_width,
                                M5.Display.height(), TFT_BLACK);
            py = 128 + slider_y;

            drawValueSliderFull(
                &M5.Display,
                new_value,
                target_channel
            );
            drawUIValueUpDown();
        }
        if (force_redraw || target_value != new_value) {
            M5.Display.fillRect(scroll_width + 2, 0,
                                320 - scroll_width - 4, 34, TFT_BLACK);
            M5.Display.setFont(&fonts::Font2);
            snprintf(str, sizeof(str), "CH %u", target_channel);
            M5.Display.drawString(str, text_x, text_y);
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
                if (controller.channel(ch) == rendered_values[ch]) {
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
                current_data = controller.channel(ch);
                prev_data    = rendered_values[ch];
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
                    if (ch < DMX_MAX_PACKET_SIZE) {
                        rendered_values[ch] = static_cast<uint8_t>(current_data);
                    }
                }
            }
            ++idx;
        }
        M5.Display.clearClipRect();
        M5.Display.endWrite();
    }
};
