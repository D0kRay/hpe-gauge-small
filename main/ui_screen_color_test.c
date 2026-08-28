#include "ui_screen_color_test.h"

#include "esp_check.h"
#include "hpe_fonts.h"

typedef struct {
    int step;
    lv_obj_t *sample;
    lv_obj_t *sample_label;
    lv_obj_t *title;
    lv_obj_t *hint;
    ui_screen_color_test_done_cb_t done_cb;
    void *done_ctx;
} color_test_state_t;

typedef struct {
    uint32_t color;
    uint32_t text_color;
    const char *name;
} color_test_step_t;

static const color_test_step_t s_test_steps[] = {
    {.color = 0x000000, .text_color = 0xffffff, .name = "BLACK"},
    {.color = 0xffffff, .text_color = 0x000000, .name = "WHITE"},
    {.color = 0xff0000, .text_color = 0xffffff, .name = "RED"},
    {.color = 0x00ff00, .text_color = 0x000000, .name = "GREEN"},
    {.color = 0x0000ff, .text_color = 0xffffff, .name = "BLUE"},
    {.color = 0x00ffff, .text_color = 0x000000, .name = "CYAN"},
    {.color = 0xff00ff, .text_color = 0xffffff, .name = "MAGENTA"},
    {.color = 0xffff00, .text_color = 0x000000, .name = "YELLOW"},
};

static void color_test_apply_step(color_test_state_t *state)
{
    if (!state || !state->sample || !state->sample_label || !state->title || !state->hint) {
        return;
    }

    const color_test_step_t *step = &s_test_steps[state->step];
    lv_obj_set_style_bg_color(state->sample, lv_color_hex(step->color), 0);
    lv_obj_set_style_text_color(state->sample_label, lv_color_hex(step->text_color), 0);
    lv_label_set_text(state->sample_label, step->name);

    char title_text[48];
    lv_snprintf(title_text, sizeof(title_text), "Color Test %d/%d", state->step + 1,
                (int)(sizeof(s_test_steps) / sizeof(s_test_steps[0])));
    lv_label_set_text(state->title, title_text);
    lv_label_set_text(state->hint, "Tap to advance color");
}

static void color_test_tap_cb(lv_event_t *e)
{
    color_test_state_t *state = (color_test_state_t *)lv_event_get_user_data(e);
    if (!state) {
        return;
    }

    lv_event_stop_bubbling(e);

    state->step++;
    if (state->step < (int)(sizeof(s_test_steps) / sizeof(s_test_steps[0]))) {
        color_test_apply_step(state);
        return;
    }

    if (state->done_cb) {
        state->done_cb(state->done_ctx);
    }
}

static void color_test_delete_cb(lv_event_t *e)
{
    color_test_state_t *state = (color_test_state_t *)lv_event_get_user_data(e);
    if (state) {
        lv_free(state);
    }
}

static lv_obj_t *create_gradient_bar(lv_obj_t *parent, int y, lv_color_t from, lv_color_t to, const char *label_text)
{
    lv_obj_t *bar = lv_obj_create(parent);
    if (!bar) {
        return NULL;
    }

    lv_obj_set_size(bar, 208, 12);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_set_style_radius(bar, 2, 0);
    lv_obj_set_style_bg_color(bar, from, 0);
    lv_obj_set_style_bg_grad_color(bar, to, 0);
    lv_obj_set_style_bg_grad_dir(bar, LV_GRAD_DIR_HOR, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, 1, 0);
    lv_obj_set_style_border_color(bar, lv_color_hex(0x2d3640), 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *label = lv_label_create(bar);
    if (!label) {
        return NULL;
    }
    lv_label_set_text(label, label_text);
    lv_obj_set_style_text_font(label, &lv_font_ddin_regular_14, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xdce4ec), 0);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 6, 0);

    return bar;
}

static lv_obj_t *create_swatch(lv_obj_t *parent, int x, int y, lv_color_t color, const char *text, lv_color_t text_color)
{
    lv_obj_t *swatch = lv_obj_create(parent);
    if (!swatch) {
        return NULL;
    }

    lv_obj_set_size(swatch, 64, 30);
    lv_obj_align(swatch, LV_ALIGN_TOP_LEFT, x, y);
    lv_obj_set_style_radius(swatch, 6, 0);
    lv_obj_set_style_bg_color(swatch, color, 0);
    lv_obj_set_style_bg_opa(swatch, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(swatch, 1, 0);
    lv_obj_set_style_border_color(swatch, lv_color_hex(0x2f3843), 0);
    lv_obj_set_style_pad_all(swatch, 0, 0);
    lv_obj_clear_flag(swatch, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *label = lv_label_create(swatch);
    if (!label) {
        return NULL;
    }
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_ddin_regular_14, 0);
    lv_obj_set_style_text_color(label, text_color, 0);
    lv_obj_center(label);

    return swatch;
}

static esp_err_t create_gray_ramp(lv_obj_t *parent)
{
    const uint32_t grays[6] = {
        0x000000, 0x333333, 0x666666, 0x999999, 0xcccccc, 0xffffff,
    };

    for (int i = 0; i < 6; ++i) {
        lv_obj_t *cell = lv_obj_create(parent);
        ESP_RETURN_ON_FALSE(cell != NULL, ESP_ERR_NO_MEM, "ui_color_test", "gray cell create failed");
        lv_obj_set_size(cell, 30, 16);
        lv_obj_align(cell, LV_ALIGN_TOP_LEFT, 13 + (i * 36), 211);
        lv_obj_set_style_bg_color(cell, lv_color_hex(grays[i]), 0);
        lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(cell, 1, 0);
        lv_obj_set_style_border_color(cell, lv_color_hex(0x2f3843), 0);
        lv_obj_set_style_radius(cell, 2, 0);
        lv_obj_set_style_pad_all(cell, 0, 0);
        lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
    }

    return ESP_OK;
}

esp_err_t ui_screen_color_test_init(ui_screen_color_test_t *screen,
                                    lv_obj_t *parent,
                                    ui_screen_color_test_done_cb_t done_cb,
                                    void *done_ctx)
{
    ESP_RETURN_ON_FALSE(screen && parent, ESP_ERR_INVALID_ARG, "ui_color_test", "invalid args");

    color_test_state_t *state = lv_malloc(sizeof(color_test_state_t));
    ESP_RETURN_ON_FALSE(state != NULL, ESP_ERR_NO_MEM, "ui_color_test", "state alloc failed");
    state->step = 0;
    state->sample = NULL;
    state->sample_label = NULL;
    state->title = NULL;
    state->hint = NULL;
    state->done_cb = done_cb;
    state->done_ctx = done_ctx;

    screen->root = parent;
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x050608), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_add_flag(parent, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(parent, color_test_tap_cb, LV_EVENT_CLICKED, state);
    lv_obj_add_event_cb(parent, color_test_delete_cb, LV_EVENT_DELETE, state);

    lv_obj_t *title = lv_label_create(parent);
    ESP_RETURN_ON_FALSE(title != NULL, ESP_ERR_NO_MEM, "ui_color_test", "title create failed");
    lv_label_set_text(title, "Color Test");
    lv_obj_set_style_text_font(title, &lv_font_ddin_regular_16, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xe8f2fb), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);
    state->title = title;

    ESP_RETURN_ON_FALSE(create_gradient_bar(parent, 28, lv_color_hex(0x000000), lv_color_hex(0xff0000), "R") != NULL,
                        ESP_ERR_NO_MEM, "ui_color_test", "red gradient create failed");
    ESP_RETURN_ON_FALSE(create_gradient_bar(parent, 44, lv_color_hex(0x000000), lv_color_hex(0x00ff00), "G") != NULL,
                        ESP_ERR_NO_MEM, "ui_color_test", "green gradient create failed");
    ESP_RETURN_ON_FALSE(create_gradient_bar(parent, 60, lv_color_hex(0x000000), lv_color_hex(0x0000ff), "B") != NULL,
                        ESP_ERR_NO_MEM, "ui_color_test", "blue gradient create failed");

    ESP_RETURN_ON_FALSE(create_swatch(parent, 12, 82, lv_color_hex(0xff0000), "RED", lv_color_hex(0xffffff)) != NULL,
                        ESP_ERR_NO_MEM, "ui_color_test", "swatch red failed");
    ESP_RETURN_ON_FALSE(create_swatch(parent, 88, 82, lv_color_hex(0x00ff00), "GREEN", lv_color_hex(0x000000)) != NULL,
                        ESP_ERR_NO_MEM, "ui_color_test", "swatch green failed");
    ESP_RETURN_ON_FALSE(create_swatch(parent, 164, 82, lv_color_hex(0x0000ff), "BLUE", lv_color_hex(0xffffff)) != NULL,
                        ESP_ERR_NO_MEM, "ui_color_test", "swatch blue failed");

    ESP_RETURN_ON_FALSE(create_swatch(parent, 12, 118, lv_color_hex(0x00ffff), "CYAN", lv_color_hex(0x000000)) != NULL,
                        ESP_ERR_NO_MEM, "ui_color_test", "swatch cyan failed");
    ESP_RETURN_ON_FALSE(create_swatch(parent, 88, 118, lv_color_hex(0xff00ff), "MAG", lv_color_hex(0xffffff)) != NULL,
                        ESP_ERR_NO_MEM, "ui_color_test", "swatch magenta failed");
    ESP_RETURN_ON_FALSE(create_swatch(parent, 164, 118, lv_color_hex(0xffff00), "YEL", lv_color_hex(0x000000)) != NULL,
                        ESP_ERR_NO_MEM, "ui_color_test", "swatch yellow failed");

    ESP_RETURN_ON_FALSE(create_swatch(parent, 12, 154, lv_color_hex(0x000000), "BLACK", lv_color_hex(0xffffff)) != NULL,
                        ESP_ERR_NO_MEM, "ui_color_test", "swatch black failed");
    ESP_RETURN_ON_FALSE(create_swatch(parent, 88, 154, lv_color_hex(0x808080), "GRAY", lv_color_hex(0xffffff)) != NULL,
                        ESP_ERR_NO_MEM, "ui_color_test", "swatch gray failed");
    ESP_RETURN_ON_FALSE(create_swatch(parent, 164, 154, lv_color_hex(0xffffff), "WHITE", lv_color_hex(0x000000)) != NULL,
                        ESP_ERR_NO_MEM, "ui_color_test", "swatch white failed");

    lv_obj_t *hint = lv_label_create(parent);
    ESP_RETURN_ON_FALSE(hint != NULL, ESP_ERR_NO_MEM, "ui_color_test", "hint create failed");
    lv_label_set_text(hint, "Gray ramp 0% -> 100%");
    lv_obj_set_style_text_font(hint, &lv_font_ddin_regular_14, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0xaab6c4), 0);
    lv_obj_align(hint, LV_ALIGN_TOP_LEFT, 12, 192);
    state->hint = hint;

    ESP_RETURN_ON_ERROR(create_gray_ramp(parent), "ui_color_test", "gray ramp create failed");

    state->sample = lv_obj_create(parent);
    ESP_RETURN_ON_FALSE(state->sample != NULL, ESP_ERR_NO_MEM, "ui_color_test", "sample create failed");
    lv_obj_set_size(state->sample, 120, 26);
    lv_obj_align(state->sample, LV_ALIGN_TOP_MID, 0, 182);
    lv_obj_set_style_radius(state->sample, 4, 0);
    lv_obj_set_style_bg_opa(state->sample, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(state->sample, 1, 0);
    lv_obj_set_style_border_color(state->sample, lv_color_hex(0x2f3843), 0);
    lv_obj_set_style_pad_all(state->sample, 0, 0);
    lv_obj_clear_flag(state->sample, LV_OBJ_FLAG_SCROLLABLE);

    state->sample_label = lv_label_create(state->sample);
    ESP_RETURN_ON_FALSE(state->sample_label != NULL, ESP_ERR_NO_MEM, "ui_color_test", "sample label create failed");
    lv_obj_set_style_text_font(state->sample_label, &lv_font_ddin_regular_14, 0);
    lv_obj_center(state->sample_label);

    color_test_apply_step(state);

    return ESP_OK;
}
