#include "ui/HudView.h"

#include <cmath>
#include <cstdio>

namespace ui {

namespace {
constexpr float kR2D = 57.29578f;
constexpr float kD2R = 0.01745329f;

constexpr uint32_t kSky    = 0x2b8cd4;
constexpr uint32_t kGround = 0x8a5a32;
constexpr uint32_t kLine   = 0xffffff;
constexpr uint32_t kAircraft = 0xffd400;
constexpr uint32_t kBoxBg  = 0x000000;
constexpr uint32_t kText   = 0xffffff;

constexpr float kPxPerDeg = 2.2f;   /* pitch ladder scale */

void line(lv_draw_ctx_t* ctx, lv_draw_line_dsc_t* d,
          lv_coord_t x1, lv_coord_t y1, lv_coord_t x2, lv_coord_t y2) {
    lv_point_t a = {x1, y1};
    lv_point_t b = {x2, y2};
    lv_draw_line(ctx, d, &a, &b);
}

void textBox(lv_draw_ctx_t* ctx, lv_coord_t x, lv_coord_t y, lv_coord_t w,
             lv_coord_t h, const char* txt, const lv_font_t* font) {
    lv_draw_rect_dsc_t r;
    lv_draw_rect_dsc_init(&r);
    r.bg_color = lv_color_hex(kBoxBg);
    r.bg_opa   = LV_OPA_50;
    r.radius   = 2;
    lv_area_t box = {x, y, (lv_coord_t)(x + w), (lv_coord_t)(y + h)};
    lv_draw_rect(ctx, &r, &box);

    lv_draw_label_dsc_t l;
    lv_draw_label_dsc_init(&l);
    l.color = lv_color_hex(kText);
    l.font  = font;
    lv_area_t tarea = {(lv_coord_t)(x + 3), (lv_coord_t)(y + 1),
                       (lv_coord_t)(x + w - 1), (lv_coord_t)(y + h - 1)};
    lv_draw_label(ctx, &l, &tarea, txt, nullptr);
}
}  // namespace

void HudView::build(lv_obj_t* parent, lv_coord_t width, lv_coord_t height) {
    obj_ = lv_obj_create(parent);
    /* Fill parent so the HUD auto-resizes when the parent panel is swapped. */
    lv_obj_set_size(obj_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(obj_, LV_OPA_TRANSP, 0);  /* we paint everything */
    lv_obj_set_style_border_width(obj_, 1, 0);
    lv_obj_set_style_border_color(obj_, lv_color_hex(0x30363d), 0);
    lv_obj_set_style_radius(obj_, 4, 0);
    lv_obj_set_style_pad_all(obj_, 0, 0);
    lv_obj_clear_flag(obj_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(obj_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(obj_, drawEvent, LV_EVENT_DRAW_MAIN, this);
}

void HudView::update(const telemetry::TelemetrySnapshot& s) {
    rollRad_    = s.attitude.roll;
    pitchRad_   = s.attitude.pitch;
    headingDeg_ = s.position.headingDeg;
    altM_       = s.position.altRel;
    airspeed_   = s.vfr.airspeed;
    climb_      = s.vfr.climb;
    valid_      = true;
    if (obj_) lv_obj_invalidate(obj_);
}

void HudView::drawEvent(lv_event_t* e) {
    static_cast<HudView*>(lv_event_get_user_data(e))->draw(e);
}

void HudView::draw(lv_event_t* e) {
    lv_draw_ctx_t* ctx = lv_event_get_draw_ctx(e);
    lv_obj_t* obj = lv_event_get_target(e);

    lv_area_t area;
    lv_obj_get_coords(obj, &area);

    /* Restrict all drawing to the HUD rectangle. */
    const lv_area_t clipOrig = *ctx->clip_area;
    lv_area_t clip;
    if (!_lv_area_intersect(&clip, &clipOrig, &area)) return;
    ctx->clip_area = &clip;

    const lv_coord_t w  = lv_area_get_width(&area);
    const lv_coord_t h  = lv_area_get_height(&area);
    const float cx = area.x1 + w / 2.0f;
    const float cy = area.y1 + h / 2.0f;

    const float a = -rollRad_;                 /* horizon rotation on screen */
    const float ca = cosf(a), sa = sinf(a);
    const float ux = ca,  uy = sa;             /* along horizon */
    const float nx = -sa, ny = ca;             /* perpendicular, points to ground */
    const float pitchDeg = pitchRad_ * kR2D;
    const float ccx = cx + nx * pitchDeg * kPxPerDeg;  /* horizon center */
    const float ccy = cy + ny * pitchDeg * kPxPerDeg;

    const float L = (w + h) * 1.6f;            /* horizon half-length */
    const float D = (w + h) * 2.0f;            /* ground depth */

    /* --- Sky background --- */
    {
        lv_draw_rect_dsc_t r;
        lv_draw_rect_dsc_init(&r);
        r.bg_color = lv_color_hex(kSky);
        r.bg_opa   = LV_OPA_COVER;
        lv_draw_rect(ctx, &r, &area);
    }

    /* --- Ground (quad on the +n side of the horizon) --- */
    {
        lv_draw_rect_dsc_t g;
        lv_draw_rect_dsc_init(&g);
        g.bg_color = lv_color_hex(kGround);
        g.bg_opa   = LV_OPA_COVER;
        lv_point_t pts[4] = {
            {(lv_coord_t)(ccx - ux * L),        (lv_coord_t)(ccy - uy * L)},
            {(lv_coord_t)(ccx + ux * L),        (lv_coord_t)(ccy + uy * L)},
            {(lv_coord_t)(ccx + ux * L + nx * D), (lv_coord_t)(ccy + uy * L + ny * D)},
            {(lv_coord_t)(ccx - ux * L + nx * D), (lv_coord_t)(ccy - uy * L + ny * D)},
        };
        lv_draw_polygon(ctx, &g, pts, 4);
    }

    /* --- Horizon line --- */
    lv_draw_line_dsc_t ld;
    lv_draw_line_dsc_init(&ld);
    ld.color = lv_color_hex(kLine);
    ld.width = 2;
    line(ctx, &ld, (lv_coord_t)(ccx - ux * L), (lv_coord_t)(ccy - uy * L),
                   (lv_coord_t)(ccx + ux * L), (lv_coord_t)(ccy + uy * L));

    /* --- Pitch ladder (+/-10, +/-20) --- */
    lv_draw_line_dsc_t pd;
    lv_draw_line_dsc_init(&pd);
    pd.color = lv_color_hex(kLine);
    pd.width = 1;
    lv_draw_label_dsc_t pl;
    lv_draw_label_dsc_init(&pl);
    pl.color = lv_color_hex(kLine);
    pl.font  = &lv_font_montserrat_12;
    const int marks[4] = {-20, -10, 10, 20};
    for (int i = 0; i < 4; i++) {
        const float off = -(float)marks[i] * kPxPerDeg;  /* +deg is up (-n) */
        const float mx = ccx + nx * off;
        const float my = ccy + ny * off;
        const float halfLen = (marks[i] % 20 == 0) ? 26.0f : 16.0f;
        line(ctx, &pd, (lv_coord_t)(mx - ux * halfLen), (lv_coord_t)(my - uy * halfLen),
                       (lv_coord_t)(mx + ux * halfLen), (lv_coord_t)(my + uy * halfLen));
        char b[6];
        snprintf(b, sizeof(b), "%d", marks[i] > 0 ? marks[i] : -marks[i]);
        lv_area_t la = {(lv_coord_t)(mx + ux * halfLen + 2), (lv_coord_t)(my - 6),
                        (lv_coord_t)(mx + ux * halfLen + 22), (lv_coord_t)(my + 8)};
        lv_draw_label(ctx, &pl, &la, b, nullptr);
    }

    /* --- Roll arc (fixed) + bank pointer (moves with roll) --- */
    const float r = h * 0.42f;
    lv_draw_line_dsc_t td;
    lv_draw_line_dsc_init(&td);
    td.color = lv_color_hex(kLine);
    td.width = 1;
    const int allTicks[11] = {-60, -45, -30, -20, -10, 0, 10, 20, 30, 45, 60};
    for (int i = 0; i < 11; i++) {
        const float ang = (-90.0f + allTicks[i]) * kD2R;
        const float co = cosf(ang), si = sinf(ang);
        const float len = (allTicks[i] % 30 == 0) ? 7.0f : 4.0f;
        line(ctx, &td, (lv_coord_t)(cx + co * r),       (lv_coord_t)(cy + si * r),
                       (lv_coord_t)(cx + co * (r - len)), (lv_coord_t)(cy + si * (r - len)));
    }
    {  /* bank pointer */
        const float ang = (-90.0f + rollRad_ * kR2D) * kD2R;
        const float co = cosf(ang), si = sinf(ang);
        const float tip = r - 9.0f;
        lv_draw_rect_dsc_t pdsc;
        lv_draw_rect_dsc_init(&pdsc);
        pdsc.bg_color = lv_color_hex(kAircraft);
        pdsc.bg_opa   = LV_OPA_COVER;
        /* small triangle pointing inward */
        const float bx = cx + co * r, by = cy + si * r;
        const float perpx = -si, perpy = co;
        lv_point_t tri[3] = {
            {(lv_coord_t)(cx + co * tip),            (lv_coord_t)(cy + si * tip)},
            {(lv_coord_t)(bx + perpx * 4),           (lv_coord_t)(by + perpy * 4)},
            {(lv_coord_t)(bx - perpx * 4),           (lv_coord_t)(by - perpy * 4)},
        };
        lv_draw_polygon(ctx, &pdsc, tri, 3);
    }

    /* --- Fixed aircraft symbol --- */
    lv_draw_line_dsc_t ad;
    lv_draw_line_dsc_init(&ad);
    ad.color = lv_color_hex(kAircraft);
    ad.width = 3;
    line(ctx, &ad, (lv_coord_t)(cx - 34), (lv_coord_t)cy, (lv_coord_t)(cx - 12), (lv_coord_t)cy);
    line(ctx, &ad, (lv_coord_t)(cx - 12), (lv_coord_t)cy, (lv_coord_t)(cx - 12), (lv_coord_t)(cy + 6));
    line(ctx, &ad, (lv_coord_t)(cx + 34), (lv_coord_t)cy, (lv_coord_t)(cx + 12), (lv_coord_t)cy);
    line(ctx, &ad, (lv_coord_t)(cx + 12), (lv_coord_t)cy, (lv_coord_t)(cx + 12), (lv_coord_t)(cy + 6));
    {
        lv_draw_rect_dsc_t dot;
        lv_draw_rect_dsc_init(&dot);
        dot.bg_color = lv_color_hex(kAircraft);
        dot.bg_opa   = LV_OPA_COVER;
        dot.radius   = LV_RADIUS_CIRCLE;
        lv_area_t d = {(lv_coord_t)(cx - 2), (lv_coord_t)(cy - 2),
                       (lv_coord_t)(cx + 2), (lv_coord_t)(cy + 2)};
        lv_draw_rect(ctx, &dot, &d);
    }

    /* --- Readouts: airspeed (L), altitude (R), heading (top) --- */
    char buf[16];
    snprintf(buf, sizeof(buf), "%.0f", airspeed_);
    textBox(ctx, area.x1 + 3, (lv_coord_t)(cy - 9), 40, 18, buf, &lv_font_montserrat_14);

    snprintf(buf, sizeof(buf), "%.0fm", altM_);
    textBox(ctx, (lv_coord_t)(area.x2 - 47), (lv_coord_t)(cy - 9), 44, 18, buf, &lv_font_montserrat_14);

    snprintf(buf, sizeof(buf), "%03d", ((int)lroundf(headingDeg_)) % 360);
    textBox(ctx, (lv_coord_t)(cx - 18), area.y1 + 3, 36, 18, buf, &lv_font_montserrat_14);

    ctx->clip_area = &clipOrig;  /* restore */
}

}  // namespace ui
