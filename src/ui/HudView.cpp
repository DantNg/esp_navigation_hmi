#include "ui/HudView.h"

#include <cmath>
#include <cstdio>

#include "ui/gen/UiGen.h"

namespace ui {

/* All colors/scales come from ui/ui_schema.json ("custom.hud" section). */
using namespace gen::hud;

namespace {
constexpr float kR2D = 57.29578f;
constexpr float kD2R = 0.01745329f;

void line(lv_draw_ctx_t* ctx, lv_draw_line_dsc_t* d,
          float x1, float y1, float x2, float y2) {
    lv_point_t a = {(lv_coord_t)x1, (lv_coord_t)y1};
    lv_point_t b = {(lv_coord_t)x2, (lv_coord_t)y2};
    lv_draw_line(ctx, d, &a, &b);
}

void fillRect(lv_draw_ctx_t* ctx, lv_coord_t x1, lv_coord_t y1,
              lv_coord_t x2, lv_coord_t y2, uint32_t color, lv_opa_t opa,
              lv_coord_t radius = 0) {
    lv_draw_rect_dsc_t r;
    lv_draw_rect_dsc_init(&r);
    r.bg_color = lv_color_hex(color);
    r.bg_opa   = opa;
    r.radius   = radius;
    lv_area_t a = {x1, y1, x2, y2};
    lv_draw_rect(ctx, &r, &a);
}

void text(lv_draw_ctx_t* ctx, lv_coord_t x, lv_coord_t y, const char* txt,
          const lv_font_t* font, uint32_t color,
          lv_text_align_t align = LV_TEXT_ALIGN_LEFT, lv_coord_t w = 60) {
    lv_draw_label_dsc_t l;
    lv_draw_label_dsc_init(&l);
    l.color = lv_color_hex(color);
    l.font  = font;
    l.align = align;
    lv_area_t a = {x, y, (lv_coord_t)(x + w), (lv_coord_t)(y + 20)};
    lv_draw_label(ctx, &l, &a, txt, nullptr);
}

/* Boxed readout with a small side pointer (EFIS style). */
void valueBox(lv_draw_ctx_t* ctx, lv_coord_t x1, lv_coord_t y1,
              lv_coord_t x2, lv_coord_t y2, const char* txt,
              const lv_font_t* font) {
    lv_draw_rect_dsc_t r;
    lv_draw_rect_dsc_init(&r);
    r.bg_color     = lv_color_hex(kBoxBg);
    r.bg_opa       = LV_OPA_COVER;
    r.border_color = lv_color_hex(kLine);
    r.border_width = 1;
    r.border_opa   = LV_OPA_COVER;
    r.radius       = 3;
    lv_area_t a = {x1, y1, x2, y2};
    lv_draw_rect(ctx, &r, &a);

    lv_draw_label_dsc_t l;
    lv_draw_label_dsc_init(&l);
    l.color = lv_color_hex(kText);
    l.font  = font;
    l.align = LV_TEXT_ALIGN_CENTER;
    lv_area_t t = {x1, (lv_coord_t)((y1 + y2) / 2 - 8), x2, y2};
    lv_draw_label(ctx, &l, &t, txt, nullptr);
}
}  // namespace

void HudView::build(lv_obj_t* parent, lv_coord_t, lv_coord_t) {
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
    const bool full = (w >= kTapeMinW);   /* tapes only on the big view */
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
    const float D = (w + h) * 2.0f;            /* far depth */
    const float band = 45.0f * kPxPerDeg;      /* near-horizon color band */

    auto quad = [&](float off1, float off2, uint32_t color) {
        lv_draw_rect_dsc_t g;
        lv_draw_rect_dsc_init(&g);
        g.bg_color = lv_color_hex(color);
        g.bg_opa   = LV_OPA_COVER;
        lv_point_t pts[4] = {
            {(lv_coord_t)(ccx - ux * L + nx * off1), (lv_coord_t)(ccy - uy * L + ny * off1)},
            {(lv_coord_t)(ccx + ux * L + nx * off1), (lv_coord_t)(ccy + uy * L + ny * off1)},
            {(lv_coord_t)(ccx + ux * L + nx * off2), (lv_coord_t)(ccy + uy * L + ny * off2)},
            {(lv_coord_t)(ccx - ux * L + nx * off2), (lv_coord_t)(ccy - uy * L + ny * off2)},
        };
        lv_draw_polygon(ctx, &g, pts, 4);
    };

    /* --- Sky / ground, two-tone for a subtle depth gradient --- */
    fillRect(ctx, area.x1, area.y1, area.x2, area.y2, kSkyHigh, LV_OPA_COVER);
    quad(-band, 0, kSky);          /* near-horizon sky */
    quad(0, D, kGroundLow);        /* deep ground */
    quad(0, band, kGround);        /* near-horizon ground */

    /* --- Horizon line --- */
    lv_draw_line_dsc_t ld;
    lv_draw_line_dsc_init(&ld);
    ld.color = lv_color_hex(kLine);
    ld.width = 2;
    line(ctx, &ld, ccx - ux * L, ccy - uy * L, ccx + ux * L, ccy + uy * L);

    /* --- Pitch ladder: every 5°, long marks at 10s, dashed below horizon --- */
    {
        lv_draw_label_dsc_t pl;
        lv_draw_label_dsc_init(&pl);
        pl.color = lv_color_hex(kLine);
        pl.font  = &lv_font_montserrat_12;
        for (int m = -20; m <= 20; m += 5) {
            if (m == 0) continue;
            const float off = -(float)m * kPxPerDeg;   /* +deg is up (-n) */
            const float mx = ccx + nx * off;
            const float my = ccy + ny * off;
            const bool major = (m % 10 == 0);
            const float half = major ? (float)kLadderHalf10 : (float)kLadderHalf5;
            const float gap  = (float)kLadderGap;

            lv_draw_line_dsc_t pd;
            lv_draw_line_dsc_init(&pd);
            pd.color = lv_color_hex(kLine);
            pd.width = major ? 2 : 1;
            if (m < 0) { pd.dash_width = 6; pd.dash_gap = 5; }  /* below horizon */

            /* two segments with a center gap (classic EFIS ladder) */
            line(ctx, &pd, mx - ux * half, my - uy * half, mx - ux * gap, my - uy * gap);
            line(ctx, &pd, mx + ux * gap,  my + uy * gap,  mx + ux * half, my + uy * half);

            if (major) {
                char b[6];
                snprintf(b, sizeof(b), "%d", m > 0 ? m : -m);
                lv_area_t la = {(lv_coord_t)(mx + ux * half + 4), (lv_coord_t)(my - 7),
                                (lv_coord_t)(mx + ux * half + 26), (lv_coord_t)(my + 8)};
                lv_draw_label(ctx, &pl, &la, b, nullptr);
                lv_area_t lb = {(lv_coord_t)(mx - ux * half - 26), (lv_coord_t)(my - 7),
                                (lv_coord_t)(mx - ux * half - 4),  (lv_coord_t)(my + 8)};
                lv_draw_label(ctx, &pl, &lb, b, nullptr);
            }
        }
    }

    /* --- Roll arc: ticks, fixed reference triangle, moving bank pointer --- */
    const float r = h * 0.40f;
    {
        lv_draw_line_dsc_t td;
        lv_draw_line_dsc_init(&td);
        td.color = lv_color_hex(kLine);
        td.width = 1;
        const int ticks[11] = {-60, -45, -30, -20, -10, 0, 10, 20, 30, 45, 60};
        for (int i = 0; i < 11; i++) {
            const float ang = (-90.0f + ticks[i]) * kD2R;
            const float co = cosf(ang), si = sinf(ang);
            const float len = (ticks[i] % 30 == 0) ? 9.0f : 5.0f;
            line(ctx, &td, cx + co * r, cy + si * r,
                           cx + co * (r + len), cy + si * (r + len));
        }
        /* fixed reference triangle at zero bank, pointing inward */
        lv_draw_rect_dsc_t fd;
        lv_draw_rect_dsc_init(&fd);
        fd.bg_color = lv_color_hex(kLine);
        fd.bg_opa   = LV_OPA_COVER;
        lv_point_t ref[3] = {
            {(lv_coord_t)cx,       (lv_coord_t)(cy - r + 2)},
            {(lv_coord_t)(cx - 6), (lv_coord_t)(cy - r - 8)},
            {(lv_coord_t)(cx + 6), (lv_coord_t)(cy - r - 8)},
        };
        lv_draw_polygon(ctx, &fd, ref, 3);

        /* moving bank pointer (accent), pointing outward toward the arc */
        const float ang = (-90.0f + rollRad_ * kR2D) * kD2R;
        const float co = cosf(ang), si = sinf(ang);
        const float bx = cx + co * (r - 3), by = cy + si * (r - 3);
        const float px = -si, py = co;
        lv_draw_rect_dsc_t pdsc;
        lv_draw_rect_dsc_init(&pdsc);
        pdsc.bg_color = lv_color_hex(kAccent);
        pdsc.bg_opa   = LV_OPA_COVER;
        lv_point_t tri[3] = {
            {(lv_coord_t)bx,                  (lv_coord_t)by},
            {(lv_coord_t)(cx + co * (r - 14) + px * 5), (lv_coord_t)(cy + si * (r - 14) + py * 5)},
            {(lv_coord_t)(cx + co * (r - 14) - px * 5), (lv_coord_t)(cy + si * (r - 14) - py * 5)},
        };
        lv_draw_polygon(ctx, &pdsc, tri, 3);
    }

    /* --- Fixed aircraft symbol: wings + center dot --- */
    {
        lv_draw_line_dsc_t ad;
        lv_draw_line_dsc_init(&ad);
        ad.color = lv_color_hex(kAccent);
        ad.width = 4;
        line(ctx, &ad, cx - 46, cy, cx - 16, cy);
        line(ctx, &ad, cx - 16, cy, cx - 16, cy + 8);
        line(ctx, &ad, cx + 46, cy, cx + 16, cy);
        line(ctx, &ad, cx + 16, cy, cx + 16, cy + 8);
        fillRect(ctx, (lv_coord_t)(cx - 3), (lv_coord_t)(cy - 3),
                 (lv_coord_t)(cx + 3), (lv_coord_t)(cy + 3),
                 kAccent, LV_OPA_COVER, LV_RADIUS_CIRCLE);
    }

    char buf[16];
    if (full) {
        /* ============ Airspeed tape (left) ============ */
        const lv_coord_t tapeTop = area.y1 + kRibbonH + 10;
        const lv_coord_t tapeBot = area.y2 - 14;
        const float tcy = (tapeTop + tapeBot) / 2.0f;
        {
            const lv_coord_t x1 = area.x1, x2 = area.x1 + kTapeW;
            fillRect(ctx, x1, tapeTop, x2, tapeBot, kTapeBg, LV_OPA_60);
            lv_draw_line_dsc_t td;
            lv_draw_line_dsc_init(&td);
            td.color = lv_color_hex(kTapeTick);
            td.width = 1;
            const float range = (tapeBot - tapeTop) / 2.0f / kSpdPxPerUnit;
            const int v0 = (int)floorf((airspeed_ - range) / 5.0f) * 5;
            const int v1 = (int)ceilf((airspeed_ + range) / 5.0f) * 5;
            for (int v = v0; v <= v1; v += 5) {
                if (v < 0) continue;
                const float y = tcy - (v - airspeed_) * kSpdPxPerUnit;
                if (y < tapeTop + 4 || y > tapeBot - 4) continue;
                const bool major = (v % 10 == 0);
                line(ctx, &td, x2 - (major ? 12 : 7), y, x2 - 2, y);
                if (major) {
                    snprintf(buf, sizeof(buf), "%d", v);
                    text(ctx, x1 + 4, (lv_coord_t)(y - 7), buf,
                         &lv_font_montserrat_12, kTapeTick);
                }
            }
            snprintf(buf, sizeof(buf), "%.0f", airspeed_);
            valueBox(ctx, x1, (lv_coord_t)(tcy - 13), (lv_coord_t)(x2 + 14),
                     (lv_coord_t)(tcy + 13), buf, &lv_font_montserrat_14);
        }

        /* ============ Altitude tape (right) ============ */
        {
            const lv_coord_t x1 = area.x2 - kTapeW, x2 = area.x2;
            fillRect(ctx, x1, tapeTop, x2, tapeBot, kTapeBg, LV_OPA_60);
            lv_draw_line_dsc_t td;
            lv_draw_line_dsc_init(&td);
            td.color = lv_color_hex(kTapeTick);
            td.width = 1;
            const float range = (tapeBot - tapeTop) / 2.0f / kAltPxPerUnit;
            const int v0 = (int)floorf((altM_ - range) / 10.0f) * 10;
            const int v1 = (int)ceilf((altM_ + range) / 10.0f) * 10;
            for (int v = v0; v <= v1; v += 10) {
                const float y = tcy - (v - altM_) * kAltPxPerUnit;
                if (y < tapeTop + 4 || y > tapeBot - 4) continue;
                const bool major = (v % 50 == 0);
                line(ctx, &td, x1 + 2, y, x1 + (major ? 12 : 7), y);
                if (major) {
                    snprintf(buf, sizeof(buf), "%d", v);
                    text(ctx, (lv_coord_t)(x1 + 16), (lv_coord_t)(y - 7), buf,
                         &lv_font_montserrat_12, kTapeTick);
                }
            }
            snprintf(buf, sizeof(buf), "%.0fm", altM_);
            valueBox(ctx, (lv_coord_t)(x1 - 14), (lv_coord_t)(tcy - 13), x2,
                     (lv_coord_t)(tcy + 13), buf, &lv_font_montserrat_14);
            /* climb rate under the box */
            snprintf(buf, sizeof(buf), "%+.1f", climb_);
            text(ctx, (lv_coord_t)(x1 - 14), (lv_coord_t)(tcy + 18), buf,
                 &lv_font_montserrat_12,
                 climb_ >= 0 ? kText : kAccent, LV_TEXT_ALIGN_CENTER, kTapeW + 14);
        }

        /* ============ Heading ribbon (top) ============ */
        {
            const lv_coord_t y1 = area.y1, y2 = area.y1 + kRibbonH;
            fillRect(ctx, area.x1, y1, area.x2, y2, kTapeBg, LV_OPA_60);
            lv_draw_line_dsc_t td;
            lv_draw_line_dsc_init(&td);
            td.color = lv_color_hex(kTapeTick);
            td.width = 1;
            const float range = w / 2.0f / kHdgPxPerDeg;
            const int h0 = (int)floorf((headingDeg_ - range) / 10.0f) * 10;
            const int h1 = (int)ceilf((headingDeg_ + range) / 10.0f) * 10;
            for (int d = h0; d <= h1; d += 10) {
                const float x = cx + (d - headingDeg_) * kHdgPxPerDeg;
                if (x < area.x1 + 4 || x > area.x2 - 4) continue;
                const int dn = ((d % 360) + 360) % 360;
                const bool major = (dn % 30 == 0);
                line(ctx, &td, x, y2 - (major ? 10 : 6), x, y2 - 2);
                if (major) {
                    const char* card = dn == 0 ? "N" : dn == 90 ? "E"
                                     : dn == 180 ? "S" : dn == 270 ? "W" : nullptr;
                    if (card) snprintf(buf, sizeof(buf), "%s", card);
                    else      snprintf(buf, sizeof(buf), "%d", dn / 10);
                    text(ctx, (lv_coord_t)(x - 12), y1 + 1, buf,
                         &lv_font_montserrat_12,
                         card ? kText : kTapeTick, LV_TEXT_ALIGN_CENTER, 24);
                }
            }
            snprintf(buf, sizeof(buf), "%03d", ((int)lroundf(headingDeg_)) % 360);
            valueBox(ctx, (lv_coord_t)(cx - 24), y1, (lv_coord_t)(cx + 24),
                     (lv_coord_t)(y2 + 6), buf, &lv_font_montserrat_14);
        }
    } else {
        /* Compact readouts for the thumbnail view */
        snprintf(buf, sizeof(buf), "%.0f", airspeed_);
        valueBox(ctx, area.x1 + 3, (lv_coord_t)(cy - 10), area.x1 + 43,
                 (lv_coord_t)(cy + 10), buf, &lv_font_montserrat_14);
        snprintf(buf, sizeof(buf), "%.0fm", altM_);
        valueBox(ctx, (lv_coord_t)(area.x2 - 47), (lv_coord_t)(cy - 10),
                 (lv_coord_t)(area.x2 - 3), (lv_coord_t)(cy + 10), buf,
                 &lv_font_montserrat_14);
        snprintf(buf, sizeof(buf), "%03d", ((int)lroundf(headingDeg_)) % 360);
        valueBox(ctx, (lv_coord_t)(cx - 20), area.y1 + 3, (lv_coord_t)(cx + 20),
                 area.y1 + 23, buf, &lv_font_montserrat_14);
    }

    ctx->clip_area = &clipOrig;  /* restore */
}

}  // namespace ui
