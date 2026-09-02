
#include "common.h"
#include "dmx_hardware.h"
#include <esp_system.h>
#include "view_receiver.h"
#include "view_sender.h"
#include <Preferences.h>

QueueHandle_t queue;

enum scene_mode_t {
    mode_startup,
    mode_select,
    mode_receiver,
    mode_sender,
};

static scene_mode_t scene_mode = mode_startup;
static ui_button_t btns[2]     = {{0, 200, 120, 40, "Receiver"},
                                  {200, 200, 120, 40, "Sender"}};
static view_receiver_t view_receiver;
static view_sender_t view_sender;


// ============================================================
// DISPLAY / STARTUP STATE
// ============================================================

// User-adjustable display settings.
// These globals are also used by view_sender.h.
uint8_t g_display_brightness_level = 3;   // 1..5
uint8_t g_screen_timeout_minutes = 3;     // 0=always on, otherwise 1..4
uint8_t g_ui_volume_level = 2;            // 0..4, 0=MUTE

uint8_t g_display_normal_brightness = 128;
uint32_t g_display_timeout_ms = 3UL * 60UL * 1000UL;
uint8_t g_ui_speaker_volume = 40;

// Battery warnings ignore the normal UI volume and always use 80%.
static constexpr uint8_t BATTERY_WARNING_VOLUME = 204;  // 80% of 255
static bool battery_warning_volume_override = false;


// Convert the user-facing 1..5 brightness scale to LCD backlight PWM.
static uint8_t brightnessFromLevel(uint8_t level) {
    static constexpr uint8_t levels[5] = {
        32,   // 1
        64,   // 2
        128,  // 3
        192,  // 4
        255   // 5
    };

    if (level < 1) level = 1;
    if (level > 5) level = 5;

    return levels[level - 1];
}


// Convert the user-facing 0..4 sound scale to M5 speaker volume.
// Five positions total; 0 is a true mute.
// Level 2 intentionally keeps the previous app default volume of 40.
static uint8_t speakerVolumeFromLevel(uint8_t level) {
    static constexpr uint8_t levels[5] = {
        0,    // 0 = MUTE
        20,   // 1
        40,   // 2  (previous app default)
        100,  // 3
        200   // 4
    };

    if (level > 4) level = 4;

    return levels[level];
}


static void saveUiSettings(void) {
    Preferences prefs;

    if (prefs.begin("dmxui", false)) {
        prefs.putUChar(
            "bright",
            g_display_brightness_level
        );

        prefs.putUChar(
            "timeout",
            g_screen_timeout_minutes
        );

        prefs.putUChar(
            "volume",
            g_ui_volume_level
        );

        prefs.end();
    }
}


static void loadUiSettings(void) {
    Preferences prefs;

    if (prefs.begin("dmxui", true)) {
        g_display_brightness_level =
            prefs.getUChar("bright", 3);

        g_screen_timeout_minutes =
            prefs.getUChar("timeout", 3);

        g_ui_volume_level =
            prefs.getUChar("volume", 2);

        prefs.end();
    }

    if (g_display_brightness_level < 1 ||
        g_display_brightness_level > 5) {
        g_display_brightness_level = 3;
    }

    // 0 = always on, otherwise 1..4 minutes.
    if (g_screen_timeout_minutes > 4) {
        g_screen_timeout_minutes = 3;
    }

    // Older firmware could save level 5. Migrate it to the new
    // maximum level 4 rather than unexpectedly reducing the volume.
    if (g_ui_volume_level > 4) {
        g_ui_volume_level = 4;
    }

    g_display_normal_brightness =
        brightnessFromLevel(
            g_display_brightness_level
        );

    g_ui_speaker_volume =
        speakerVolumeFromLevel(
            g_ui_volume_level
        );

    if (g_screen_timeout_minutes == 0) {
        g_display_timeout_ms = 0;
    } else {
        g_display_timeout_ms =
            static_cast<uint32_t>(
                g_screen_timeout_minutes
            ) *
            60UL *
            1000UL;
    }
}


// Called from the Settings screen in view_sender.h.
void applyDisplayBrightnessLevel(uint8_t level) {
    if (level < 1) level = 1;
    if (level > 5) level = 5;

    if (g_display_brightness_level == level) {
        return;
    }

    g_display_brightness_level = level;

    g_display_normal_brightness =
        brightnessFromLevel(level);

    M5.Display.setBrightness(
        g_display_normal_brightness
    );

    saveUiSettings();
}


// Called from the Settings screen in view_sender.h.
void setScreenTimeoutMinutes(uint8_t minutes) {
    // 0 = always on, otherwise 1..4 minutes.
    if (minutes > 4) {
        minutes = 4;
    }

    if (g_screen_timeout_minutes == minutes) {
        return;
    }

    g_screen_timeout_minutes = minutes;

    if (minutes == 0) {
        g_display_timeout_ms = 0;
    } else {
        g_display_timeout_ms =
            static_cast<uint32_t>(minutes) *
            60UL *
            1000UL;
    }

    saveUiSettings();
}


// Called from the Settings screen in view_sender.h.
void applyUiVolumeLevel(uint8_t level) {
    if (level > 4) level = 4;

    if (g_ui_volume_level == level) {
        return;
    }

    g_ui_volume_level = level;
    g_ui_speaker_volume =
        speakerVolumeFromLevel(level);

    // Do not reduce an active 80% battery warning mid-tone.
    if (!battery_warning_volume_override) {
        M5.Speaker.setVolume(
            g_ui_speaker_volume
        );
    }

    saveUiSettings();
}


// Battery-warning tones deliberately bypass the user's UI volume.
void playBatteryWarningTone(
    uint16_t frequency,
    uint16_t duration_ms
) {
    if (M5.Speaker.isPlaying()) {
        return;
    }

    battery_warning_volume_override = true;

    M5.Speaker.setVolume(
        BATTERY_WARNING_VOLUME
    );

    M5.Speaker.tone(
        frequency,
        duration_ms
    );
}


// Restore the normal user-selected volume immediately after the warning ends.
static void serviceSpeakerVolumeOverride(void) {
    if (battery_warning_volume_override &&
        !M5.Speaker.isPlaying()) {

        M5.Speaker.setVolume(
            g_ui_speaker_volume
        );

        battery_warning_volume_override = false;
    }
}


// Startup screen duration.
static constexpr int STARTUP_DURATION_SECONDS = 3;
static constexpr uint32_t STARTUP_DURATION_MS =
    STARTUP_DURATION_SECONDS * 1000UL;

static uint32_t last_display_activity_ms = 0;
static bool display_sleeping = false;

// When the sleeping display is touched, consume the complete
// wake-up gesture so it cannot accidentally operate a UI control.
static bool consume_wake_touch = false;

// Startup state.
static uint32_t startup_started_ms = 0;
static int startup_last_second = -1;
static int startup_last_progress_w = -1;
static int startup_last_battery = -999;
static bool startup_last_charging = false;
static bool startup_last_external_power = false;

// Short non-blocking rising boot chime.
static uint32_t startup_sound_started_ms = 0;
static uint8_t startup_sound_step = 0;


// Keep transmitting the already-buffered DMX frame while the wake
// gesture is being consumed. This prevents a long touch from
// interrupting the DMX stream.
static void serviceDmxDuringWakeTouch(void) {
    if (scene_mode != mode_sender) {
        return;
    }

#if ESP_DMX_VERSION == 1
    if (ESP_ERR_TIMEOUT != dmx_wait_tx_done(dmxPort, 0)) {
        dmx_tx_packet(dmxPort);
    }
#else
    if (ESP_ERR_TIMEOUT != dmx_wait_send_done(dmxPort, 0)) {
        dmx_send_packet(dmxPort, DMX_MAX_PACKET_SIZE);
    }
#endif
}



// ============================================================
// STARTUP SOUND
// ============================================================

static void serviceStartupSound(uint32_t now) {
    if (startup_sound_step >= 4) {
        return;
    }

    uint32_t elapsed =
        now - startup_sound_started_ms;

    // Rising four-note "device boot" chime.
    // Each note is intentionally short and spaced so the speaker task
    // can finish naturally without blocking the application.
    static constexpr uint16_t note_hz[4] = {
        900,
        1350,
        1900,
        2700
    };

    static constexpr uint16_t note_ms[4] = {
        70,
        70,
        80,
        120
    };

    static constexpr uint16_t note_at_ms[4] = {
        0,
        95,
        190,
        295
    };

    if (elapsed >= note_at_ms[startup_sound_step] &&
        !M5.Speaker.isPlaying()) {

        M5.Speaker.tone(
            note_hz[startup_sound_step],
            note_ms[startup_sound_step]
        );

        ++startup_sound_step;
    }
}


// Common startup exit path.
// consume_touch=true is used when the user taps the startup screen,
// ensuring that same press/release gesture cannot operate the Sender UI.
static void enterSenderFromStartup(bool consume_touch) {
    scene_mode = mode_sender;
    view_sender.setup();

    last_display_activity_ms = millis();
    display_sleeping = false;

    consume_wake_touch = consume_touch;
}


// ============================================================
// STARTUP SCREEN
// ============================================================

static void drawStartupBattery(bool force_redraw = false) {
    int battery_level = M5.Power.getBatteryLevel();
    bool charging = M5.Power.isCharging();

    int vbus_mv = M5.Power.getVBUSVoltage();
    bool external_power = (vbus_mv >= 4500);

    if (battery_level > 100) {
        battery_level = 100;
    }

    if (!force_redraw &&
        battery_level == startup_last_battery &&
        charging == startup_last_charging &&
        external_power == startup_last_external_power) {
        return;
    }

    startup_last_battery = battery_level;
    startup_last_charging = charging;
    startup_last_external_power = external_power;

    // Clear battery/status area.
    M5.Display.fillRect(62, 101, 210, 60, TFT_BLACK);

    static constexpr int bat_x = 86;
    static constexpr int bat_y = 111;
    static constexpr int bat_w = 58;
    static constexpr int bat_h = 22;

    M5.Display.drawRoundRect(bat_x, bat_y, bat_w, bat_h, 4, TFT_WHITE);
    M5.Display.fillRect(bat_x + bat_w, bat_y + 6, 5, 10, TFT_WHITE);

    if (battery_level >= 0) {
        int level = battery_level;

        if (level < 0) level = 0;
        if (level > 100) level = 100;

        int inner_w = bat_w - 6;
        int fill_w = (inner_w * level) / 100;

        uint16_t fill_color = TFT_GREEN;

        if (level <= 20) {
            fill_color = TFT_RED;
        } else if (level <= 35) {
            fill_color = TFT_YELLOW;
        }

        if (charging) {
            fill_color = TFT_GREEN;
        }

        if (fill_w > 0) {
            M5.Display.fillRect(
                bat_x + 3,
                bat_y + 3,
                fill_w,
                bat_h - 6,
                fill_color
            );
        }
    }

    M5.Display.setTextDatum(textdatum_t::middle_left);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setFont(&fonts::AsciiFont8x16);

    char battery_text[12];

    if (battery_level >= 0) {
        snprintf(battery_text, sizeof(battery_text), "%d%%", battery_level);
    } else {
        snprintf(battery_text, sizeof(battery_text), "--%%");
    }

    M5.Display.drawString(battery_text, 164, 122);

    M5.Display.setFont(&fonts::Font0);

    if (charging) {
        M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
        M5.Display.drawString("CHARGING", 164, 145);
    } else if (external_power) {
        M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
        M5.Display.drawString("EXTERNAL POWER", 164, 145);
    } else {
        M5.Display.setTextColor(TFT_DARKGRAY, TFT_BLACK);
        M5.Display.drawString("BATTERY", 164, 145);
    }

    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
}


static void drawStartupCountdown(
    int seconds_left,
    bool force_redraw = false
) {
    static constexpr int text_y = 187;

    static constexpr int bar_x = 45;
    static constexpr int bar_y = 208;
    static constexpr int bar_w = 230;
    static constexpr int bar_h = 10;

    static constexpr int inner_x = bar_x + 2;
    static constexpr int inner_y = bar_y + 2;
    static constexpr int inner_w = bar_w - 4;
    static constexpr int inner_h = bar_h - 4;

    // --------------------------------------------------------
    // COUNTDOWN TEXT
    // --------------------------------------------------------
    //
    // Previously the complete 320x60 startup area was erased on every
    // loop iteration. Now the text row is touched only when the displayed
    // second actually changes.
    if (force_redraw ||
        seconds_left != startup_last_second) {

        M5.Display.fillRect(
            0,
            174,
            320,
            27,
            TFT_BLACK
        );

        M5.Display.setTextDatum(
            textdatum_t::middle_center
        );

        M5.Display.setTextColor(
            TFT_WHITE,
            TFT_BLACK
        );

        M5.Display.setFont(
            &fonts::Font0
        );

        char startup_text[32];

        snprintf(
            startup_text,
            sizeof(startup_text),
            "Starting Sender in %d...",
            seconds_left
        );

        M5.Display.drawString(
            startup_text,
            160,
            text_y
        );

        startup_last_second =
            seconds_left;
    }


    // --------------------------------------------------------
    // PROGRESS BAR
    // --------------------------------------------------------
    uint32_t elapsed =
        millis() - startup_started_ms;

    if (elapsed > STARTUP_DURATION_MS) {
        elapsed = STARTUP_DURATION_MS;
    }

    int fill_w =
        (inner_w *
         static_cast<int>(elapsed)) /
        static_cast<int>(
            STARTUP_DURATION_MS
        );

    if (fill_w < 0) {
        fill_w = 0;
    }

    if (fill_w > inner_w) {
        fill_w = inner_w;
    }


    // Initial draw/reset: outline and empty interior only once.
    if (force_redraw ||
        startup_last_progress_w < 0 ||
        fill_w < startup_last_progress_w) {

        M5.Display.drawRoundRect(
            bar_x,
            bar_y,
            bar_w,
            bar_h,
            4,
            TFT_DARKGRAY
        );

        M5.Display.fillRect(
            inner_x,
            inner_y,
            inner_w,
            inner_h,
            TFT_BLACK
        );

        if (fill_w > 0) {
            M5.Display.fillRect(
                inner_x,
                inner_y,
                fill_w,
                inner_h,
                TFT_GREEN
            );
        }

        startup_last_progress_w =
            fill_w;

        return;
    }


    // Normal startup progress: paint ONLY the newly added green pixels.
    // Nothing is erased from the physical LCD between frames.
    if (fill_w >
        startup_last_progress_w) {

        int delta_w =
            fill_w -
            startup_last_progress_w;

        M5.Display.fillRect(
            inner_x +
                startup_last_progress_w,
            inner_y,
            delta_w,
            inner_h,
            TFT_GREEN
        );

        startup_last_progress_w =
            fill_w;
    }
}


static void drawStartupScreen(void) {
    scene_mode = mode_startup;

    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextDatum(textdatum_t::middle_center);

    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setFont(&fonts::Font4);
    M5.Display.drawString("DMX CONTROLLER", 160, 47);

    M5.Display.setFont(&fonts::Font0);
    M5.Display.setTextColor(TFT_DARKGRAY, TFT_BLACK);
#if defined(DMX_HARDWARE_UNIT)
    M5.Display.drawString("UNIT DMX", 160, 78);
#elif defined(DMX_HARDWARE_BASE)
    M5.Display.drawString("DMX BASE", 160, 78);
#endif

    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);

    startup_last_second = -1;
    startup_last_progress_w = -1;
    startup_last_battery = -999;
    startup_last_charging = false;
    startup_last_external_power = false;

    drawStartupBattery(true);
    drawStartupCountdown(
        STARTUP_DURATION_SECONDS,
        true
    );
}


static void handleStartupScreen(uint32_t now) {
    uint32_t elapsed = now - startup_started_ms;

    drawStartupBattery();

    int seconds_left =
        STARTUP_DURATION_SECONDS -
        static_cast<int>(elapsed / 1000);
    if (seconds_left < 1) {
        seconds_left = 1;
    }

    // Countdown redraws only when its number changes.
    // Progress updates continuously, but only newly added pixels are written.
    drawStartupCountdown(seconds_left);

    if (elapsed >= STARTUP_DURATION_MS) {
        enterSenderFromStartup(false);
    }
}


void drawSelectSetup(void) {
    scene_mode = mode_select;
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextDatum(textdatum_t::baseline_center);
    M5.Display.setFont(&fonts::Font4);
    M5.Display.drawString("DMX512Tools for Unit", M5.Display.width() >> 1,
                          M5.Display.height() >> 2);

    M5.Display.setFont(&fonts::AsciiFont8x16);
    M5.Display.drawString("Select mode", M5.Display.width() >> 1,
                          M5.Display.height() * 12 >> 4);
    for (size_t i = 0; i < 2; ++i) {
        btns[i].draw(&M5.Display, false, false, true);
    }
}


// ============================================================
// SOFTWARE RESTART / POWER OFF
// ============================================================

void controllerRestart(void) {
    // Short visual/audio delay is intentionally avoided here.
    // Restart immediately once the Settings double-tap is confirmed.
    esp_restart();
}


void controllerPowerOff(void) {
    M5.Power.powerOff();
}


void setup(void) {
    auto cfg = M5.config();
    cfg.output_power = false;
    M5.begin(cfg);

#if defined(DMX_HARDWARE_UNIT)
    if (M5.Power.getType() == m5::Power_Class::pmic_t::pmic_axp2101 &&
        M5.Power.Axp2101.getBatState()) {
        M5.Power.setExtOutput(true);
    }
#endif

    // Restore brightness and screen-off time from NVS.
    loadUiSettings();

    // Apply the saved normal UI sound volume.
    M5.Speaker.setVolume(
        g_ui_speaker_volume
    );

    // Display starts at the saved brightness.
    M5.Display.setBrightness(g_display_normal_brightness);
    last_display_activity_ms = millis();

    /* Configure the DMX hardware to the default DMX settings and tell the DMX
      driver which hardware pins we are using. */
    dmx_config_t dmxConfig = DMX_DEFAULT_CONFIG;
    dmx_param_config(dmxPort, &dmxConfig);

    /// For M5Stack Core2/Tough pin setting: TX:19  RX:35  EN:27

int transmitPin = GPIO_NUM_19;
int receivePin  = GPIO_NUM_35;
int enablePin   = GPIO_NUM_27;

if (M5.getBoard() == m5::board_t::board_M5StackCoreS3 ||
    M5.getBoard() == m5::board_t::board_M5StackCoreS3SE) {
    transmitPin = DMX_TX_PIN;
    receivePin  = DMX_RX_PIN;
    enablePin   = DMX_EN_PIN;
}
else if (M5.getBoard() == m5::board_t::board_M5Stack) {
    /// M5Stack(BASIC/GRAY/GO/FIRE) pin setting: TX:13  RX:35  EN:12
    transmitPin = GPIO_NUM_13;
    receivePin  = GPIO_NUM_35;
    enablePin   = GPIO_NUM_12;
}

dmx_set_pin(
    dmxPort,
    transmitPin,
    receivePin,
    enablePin
);

    dmx_driver_install(dmxPort, DMX_MAX_PACKET_SIZE, dmxQueueSize, &queue,
                       dmxInterruptPriority);

    // Show startup information, then automatically enter Sender mode.
    startup_started_ms = millis();

    // Start the non-blocking boot chime at the same time.
    startup_sound_started_ms = startup_started_ms;
    startup_sound_step = 0;

    drawStartupScreen();

    // Play the first note immediately instead of waiting for loop().
    serviceStartupSound(millis());
}

int getBtnIndex(int x, int y) {
    for (int i = 0; i < 2; ++i) {
        if (btns[i].contain(x, y)) {
            return i;
        }
    }
    return -1;
}

int focus_idx = -1;

void loop(void) {
    M5.update();

    // Restore normal sound volume as soon as an 80% battery warning finishes.
    serviceSpeakerVolumeOverride();

    auto touch = M5.Touch.getDetail();
    uint32_t now = millis();

    // Continue the short boot chime without blocking UI/DMX processing.
    serviceStartupSound(now);


    // --------------------------------------------------------
    // SAFE WAKE-UP TOUCH HANDLING
    // --------------------------------------------------------
    //
    // If the display was sleeping, the first touch only wakes it.
    // The whole press/release gesture is consumed, so the same touch
    // cannot select a channel, move a slider, or activate a preset.
    if (display_sleeping && touch.wasPressed()) {
        M5.Display.setBrightness(g_display_normal_brightness);

        display_sleeping = false;
        consume_wake_touch = true;
        last_display_activity_ms = now;

        serviceDmxDuringWakeTouch();
        delay(1);
        return;
    }


    // Continue consuming the wake-up gesture until the finger has
    // completely left the touchscreen. DMX transmission is serviced
    // manually while the normal sender UI loop is temporarily skipped.
    if (consume_wake_touch) {
        serviceDmxDuringWakeTouch();

        if (!touch.state) {
            consume_wake_touch = false;
            last_display_activity_ms = now;
        }

        delay(1);
        return;
    }


    // --------------------------------------------------------
    // STARTUP TAP-TO-SKIP
    // --------------------------------------------------------
    //
    // Any touch immediately enters Sender. The complete touch gesture
    // is then consumed by the existing safe-touch mechanism, so the
    // same finger press cannot accidentally change a DMX control.
    if (scene_mode == mode_startup &&
        touch.wasPressed()) {

        enterSenderFromStartup(true);

        delay(1);
        return;
    }


    // --------------------------------------------------------
    // NORMAL TOUCH FEEDBACK / ACTIVITY TIMER
    // --------------------------------------------------------

    if (touch.wasPressed()) {
        last_display_activity_ms = now;

        // Existing short touchscreen click.
        // Do not chop a boot/warning/confirmation sound already playing.
        if (!M5.Speaker.isPlaying()) {
            M5.Speaker.tone(4000, 20);
        }
    }


    // --------------------------------------------------------
    // DISPLAY AUTO-OFF
    // --------------------------------------------------------
    //
    // Only the backlight is disabled. The application, battery
    // monitoring and DMX processing keep running.
    if (!display_sleeping &&
        g_display_timeout_ms > 0 &&
        (uint32_t)(now - last_display_activity_ms) >= g_display_timeout_ms) {

        M5.Display.setBrightness(0);
        display_sleeping = true;
    }


    switch (scene_mode) {
        case mode_startup:
            handleStartupScreen(now);
            delay(10);
            return;

        case mode_receiver:
            if (!view_receiver.loop()) {
                view_receiver.close();
                drawSelectSetup();
            }
            return;

        case mode_sender:
            if (!view_sender.loop()) {
                view_sender.close();
                drawSelectSetup();
            }
            return;

        default:
            break;
    }

    int clicked_idx = -1;

    auto tp = M5.Touch.getDetail();
    if (tp.wasPressed()) {
        focus_idx = getBtnIndex(tp.base_x, tp.base_y);
    }
    if (tp.wasClicked()) {
        clicked_idx = getBtnIndex(tp.base_x, tp.base_y);
    }

    if (M5.BtnA.wasPressed()) {
        focus_idx = 0;
    } else if (M5.BtnA.wasReleased() && focus_idx == 0) {
        focus_idx = -1;
    }
    if (M5.BtnC.wasPressed()) {
        focus_idx = 1;
    } else if (M5.BtnC.wasReleased() && focus_idx == 1) {
        focus_idx = -1;
    }
    if (M5.BtnA.wasClicked()) {
        clicked_idx = 0;
    }
    if (M5.BtnC.wasClicked()) {
        clicked_idx = 1;
    }
    for (size_t i = 0; i < 2; ++i) {
        btns[i].draw(&M5.Display, i == focus_idx, i == clicked_idx);
    }
    delay(10);

    if (clicked_idx >= 0) {
        scene_mode = (clicked_idx == 0) ? mode_receiver : mode_sender;
        switch (scene_mode) {
            case mode_receiver:
                view_receiver.setup();
                break;

            case mode_sender:
                view_sender.setup();
                break;

            default:
                break;
        }
    }
}
