/* Round gauge UI: AKTUALIZACE, from the menu where the board has the Wi-Fi
 * updater (rnd_update_available(); the 1.85": ../../notstock-updater185, the
 * 1.75" AMOLED: ../../notstock-updater175, RND_UPD_SSID its network).
 * What happens and what to do on the phone, and SPUSTIT, which restarts
 * the gauge into the updater. A long press goes back to the menu.
 */
#include "ui_round_int.h"

static lv_obj_t *upd_scr;

static void start_cb(lv_event_t *e)
{
    (void)e;
    rnd_update_start();
}

static void back_cb(lv_event_t *e)
{
    (void)e;
    rnd_menu_open();
}

void rnd_update_open(void)
{
    lv_scr_load(upd_scr);
}

void rnd_update_create(void)
{
    if (upd_scr) lv_obj_del(upd_scr);         /* rebuilt for a new language */
    upd_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(upd_scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(upd_scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(upd_scr, LV_OBJ_FLAG_SCROLLABLE);
    rnd_on_long(upd_scr, back_cb);

    lv_obj_t *l = rnd_label(upd_scr, &rnd_18, C_GREY, 64);
    lv_obj_set_style_text_letter_space(l, 4, 0);
    lv_label_set_text(l, TR("UPDATE", "AKTUALIZACE"));

    l = rnd_label(upd_scr, &rnd_18, C_W, 104);
    lv_label_set_text(l, TR("THE GAUGE RESTARTS INTO", "BUDÍK SE PŘEPNE DO"));
    l = rnd_label(upd_scr, &rnd_18, C_W, 130);
    lv_label_set_text(l, TR("THE UPDATER. ON THE PHONE:", "AKTUALIZACE. V MOBILU:"));

    l = rnd_label(upd_scr, &rnd_26, C_REGEN, 168);
#ifndef RND_UPD_SSID
#define RND_UPD_SSID "NOTSTOCK-185"
#endif
    lv_label_set_text(l, "WIFI " RND_UPD_SSID);
    l = rnd_label(upd_scr, &rnd_26, C_W, 204);
    lv_label_set_text(l, TR("PASSWORD notstock", "HESLO notstock"));
    l = rnd_label(upd_scr, &rnd_26, C_W, 240);
    lv_label_set_text(l, "192.168.4.1");
    l = rnd_label(upd_scr, &rnd_18, C_GREY, 282);
    lv_label_set_text(l, TR("THERE, UPLOAD THE .BIN", "TAM NAHRAJ SOUBOR .BIN"));

    lv_obj_t *b = lv_obj_create(upd_scr);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, 260, 58);
    lv_obj_align(b, LV_ALIGN_TOP_MID, 0, 324);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(b, C_PANEL, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(b, C_REGEN, 0);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_bg_color(b, C_EDGE, LV_STATE_PRESSED);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(b, start_cb, LV_EVENT_CLICKED, NULL);
    l = lv_label_create(b);
    lv_obj_set_style_text_font(l, &rnd_26, 0);
    lv_obj_set_style_text_color(l, C_W, 0);
    lv_obj_set_style_text_letter_space(l, 2, 0);
    lv_label_set_text(l, TR("START", "SPUSTIT"));
    lv_obj_center(l);

    l = rnd_label(upd_scr, &rnd_18, C_DIM, 404);
    lv_label_set_text(l, TR("LONG PRESS: BACK", "PODRŽ: ZPĚT"));
}
