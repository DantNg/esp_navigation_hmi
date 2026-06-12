#!/usr/bin/env python3
"""UI code generator: ui/ui_schema.json -> src/ui/gen/UiGen.h.

Runs automatically as a PlatformIO pre-build extra_script, or standalone:
    python tools/gen_ui.py

The generated header contains:
  - constexpr layout constants  (ui::gen::kTopBarH, ...)
  - constexpr palette colors    (ui::gen::kColBg, ...)
  - struct Widgets              (one lv_obj_t* per element with an "id")
  - inline void build(lv_obj_t* screen, Widgets& w)

Behavior (event callbacks, dynamic text/colors) stays in C++; the schema
only describes the static look/layout.
"""
import json
import os
import re
import sys

SCHEMA_REL = os.path.join("ui", "ui_schema.json")
OUTPUT_REL = os.path.join("src", "ui", "gen", "UiGen.h")

CREATE_FN = {
    "panel":   "lv_obj_create",
    "flexrow": "lv_obj_create",
    "spacer":  "lv_obj_create",
    "label":   "lv_label_create",
    "bar":     "lv_bar_create",
    "switch":  "lv_switch_create",
}

FLEX_ALIGN = {
    "start": "LV_FLEX_ALIGN_START", "end": "LV_FLEX_ALIGN_END",
    "center": "LV_FLEX_ALIGN_CENTER",
    "space_between": "LV_FLEX_ALIGN_SPACE_BETWEEN",
    "space_around": "LV_FLEX_ALIGN_SPACE_AROUND",
    "space_evenly": "LV_FLEX_ALIGN_SPACE_EVENLY",
}

TEXT_ALIGN = {"left": "LV_TEXT_ALIGN_LEFT", "center": "LV_TEXT_ALIGN_CENTER",
              "right": "LV_TEXT_ALIGN_RIGHT"}

LONG_MODE = {"wrap": "LV_LABEL_LONG_WRAP", "clip": "LV_LABEL_LONG_CLIP",
             "dot": "LV_LABEL_LONG_DOT", "scroll": "LV_LABEL_LONG_SCROLL"}

EVENT_TRIGGER = {
    "clicked": "LV_EVENT_CLICKED",
    "pressed": "LV_EVENT_PRESSED",
    "released": "LV_EVENT_RELEASED",
    "long_pressed": "LV_EVENT_LONG_PRESSED",
    "value_changed": "LV_EVENT_VALUE_CHANGED",
    "focused": "LV_EVENT_FOCUSED",
    "defocused": "LV_EVENT_DEFOCUSED",
}

# style key -> lv_obj_set_style_<key>; value kind: color | opa | int
STYLE_KEYS = {
    "bg_color": "color", "border_color": "color", "text_color": "color",
    "bg_opa": "opa", "border_opa": "opa",
    "border_width": "int", "radius": "int",
    "pad_all": "int", "pad_hor": "int", "pad_ver": "int",
    "pad_left": "int", "pad_right": "int", "pad_top": "int",
    "pad_bottom": "int", "pad_row": "int", "pad_column": "int",
}


def camel(name):
    return "".join(p[0].upper() + p[1:] for p in re.split(r"[_\-]", name) if p)


class Generator:
    def __init__(self, schema):
        self.schema = schema
        self.consts = self._resolve_constants(schema.get("constants", {}))
        self.palette = schema.get("palette", {})
        self.fonts = schema.get("fonts", {})
        self.ids = []
        self.handlers = []  # (handler_name, trigger) in declaration order
        self.lines = []
        self.var_n = 0

    # ---- value resolution ------------------------------------------------
    def _resolve_constants(self, raw):
        out = {}
        pending = dict(raw)
        for _ in range(len(pending) + 1):
            for k, v in list(pending.items()):
                try:
                    out[k] = self._eval(v, out)
                    del pending[k]
                except KeyError:
                    continue
        if pending:
            raise ValueError("unresolvable constants: %s" % list(pending))
        return out

    @staticmethod
    def _eval(v, consts):
        if isinstance(v, (int, float)):
            return int(v)
        expr = re.sub(r"\$(\w+)", lambda m: str(consts[m.group(1)]), str(v))
        return int(eval(expr, {"__builtins__": {}}, {}))

    def coord(self, v):
        """x/y/w/h value -> C expression."""
        if isinstance(v, str):
            if v == "content":
                return "LV_SIZE_CONTENT"
            m = re.fullmatch(r"(\d+)%", v)
            if m:
                return "LV_PCT(%s)" % m.group(1)
        return str(self._eval(v, self.consts))

    def color(self, v):
        if isinstance(v, str) and v.startswith("$"):
            key = v[1:]
            if key not in self.palette:
                raise ValueError("unknown palette color: " + v)
            return "lv_color_hex(kCol%s)" % camel(key)
        return "lv_color_hex(%s)" % v

    @staticmethod
    def opa(v):
        if v == "transp":
            return "LV_OPA_TRANSP"
        if v == "cover":
            return "LV_OPA_COVER"
        n = int(v)
        if n % 10 == 0 and 0 <= n <= 100:
            return "LV_OPA_%d" % n
        return str(round(255 * n / 100))

    def font(self, name):
        if name not in self.fonts:
            raise ValueError("unknown font: " + name)
        return "&" + self.fonts[name]

    # ---- emission ----------------------------------------------------------
    def emit(self, s):
        self.lines.append("    " + s)

    def gen_element(self, el, parent_var):
        t = el.get("type", "panel")
        if t not in CREATE_FN:
            raise ValueError("unknown element type: " + t)
        var = "e%d" % self.var_n
        self.var_n += 1
        eid = el.get("id")
        self.emit("")
        self.emit("/* %s */" % (eid or t))
        self.emit("lv_obj_t* %s = %s(%s);" % (var, CREATE_FN[t], parent_var))
        if eid:
            self.ids.append(eid)
            self.emit("w.%s = %s;" % (eid, var))

        # geometry
        w, h = el.get("w"), el.get("h")
        if w is not None and h is not None:
            self.emit("lv_obj_set_size(%s, %s, %s);" % (var, self.coord(w), self.coord(h)))
        elif w is not None:
            self.emit("lv_obj_set_width(%s, %s);" % (var, self.coord(w)))
        elif h is not None:
            self.emit("lv_obj_set_height(%s, %s);" % (var, self.coord(h)))
        if "x" in el or "y" in el:
            self.emit("lv_obj_set_pos(%s, %s, %s);"
                      % (var, self.coord(el.get("x", 0)), self.coord(el.get("y", 0))))

        # built-in defaults for convenience types
        if t == "spacer":
            self.emit("lv_obj_set_height(%s, 1);" % var)
            self.emit("lv_obj_set_style_bg_opa(%s, LV_OPA_TRANSP, 0);" % var)
            self.emit("lv_obj_set_style_border_width(%s, 0, 0);" % var)
            self.emit("lv_obj_set_flex_grow(%s, 1);" % var)
        if t == "flexrow":
            self.emit("lv_obj_set_width(%s, LV_PCT(100));" % var)
            self.emit("lv_obj_set_height(%s, LV_SIZE_CONTENT);" % var)
            self.emit("lv_obj_set_style_bg_opa(%s, LV_OPA_TRANSP, 0);" % var)
            self.emit("lv_obj_set_style_border_width(%s, 0, 0);" % var)
            self.emit("lv_obj_set_style_pad_all(%s, 0, 0);" % var)
            self.emit("lv_obj_clear_flag(%s, LV_OBJ_FLAG_SCROLLABLE);" % var)
            self.emit("lv_obj_set_flex_flow(%s, LV_FLEX_FLOW_ROW);" % var)
            self.emit("lv_obj_set_flex_align(%s, LV_FLEX_ALIGN_START, "
                      "LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);" % var)

        # label content
        if t == "label":
            self.emit("lv_label_set_text(%s, %s);" % (var, json.dumps(el.get("text", ""))))
            if "long_mode" in el:
                self.emit("lv_label_set_long_mode(%s, %s);" % (var, LONG_MODE[el["long_mode"]]))
        if "font" in el:
            self.emit("lv_obj_set_style_text_font(%s, %s, 0);" % (var, self.font(el["font"])))

        # bar
        if t == "bar":
            rng = el.get("range", [0, 100])
            self.emit("lv_bar_set_range(%s, %d, %d);" % (var, rng[0], rng[1]))
            self.emit("lv_bar_set_value(%s, %d, LV_ANIM_OFF);" % (var, el.get("value", 0)))
            if "indicator_color" in el:
                self.emit("lv_obj_set_style_bg_color(%s, %s, LV_PART_INDICATOR);"
                          % (var, self.color(el["indicator_color"])))

        # styles
        for key, val in el.get("style", {}).items():
            kind = STYLE_KEYS.get(key)
            if key == "text_align":
                self.emit("lv_obj_set_style_text_align(%s, %s, 0);" % (var, TEXT_ALIGN[val]))
            elif kind == "color":
                self.emit("lv_obj_set_style_%s(%s, %s, 0);" % (key, var, self.color(val)))
            elif kind == "opa":
                self.emit("lv_obj_set_style_%s(%s, %s, 0);" % (key, var, self.opa(val)))
            elif kind == "int":
                self.emit("lv_obj_set_style_%s(%s, %d, 0);" % (key, var, int(val)))
            else:
                raise ValueError("unknown style key: " + key)

        # flags
        for f in el.get("flags", []):
            if f == "no_scroll":
                self.emit("lv_obj_clear_flag(%s, LV_OBJ_FLAG_SCROLLABLE);" % var)
            elif f == "clickable":
                self.emit("lv_obj_add_flag(%s, LV_OBJ_FLAG_CLICKABLE);" % var)
            elif f == "hidden":
                self.emit("lv_obj_add_flag(%s, LV_OBJ_FLAG_HIDDEN);" % var)
            else:
                raise ValueError("unknown flag: " + f)

        # flex container
        flex = el.get("flex")
        if flex:
            flow = "LV_FLEX_FLOW_" + flex.get("flow", "row").upper()
            self.emit("lv_obj_set_flex_flow(%s, %s);" % (var, flow))
            if any(k in flex for k in ("main", "cross", "track")):
                self.emit("lv_obj_set_flex_align(%s, %s, %s, %s);" % (
                    var,
                    FLEX_ALIGN[flex.get("main", "start")],
                    FLEX_ALIGN[flex.get("cross", "start")],
                    FLEX_ALIGN[flex.get("track", "start")]))
        if "grow" in el:
            self.emit("lv_obj_set_flex_grow(%s, %d);" % (var, el["grow"]))
        if el.get("align") == "center":
            self.emit("lv_obj_center(%s);" % var)

        # events -> dispatch into Handlers struct members
        events = el.get("event")
        if events:
            if isinstance(events, dict):
                events = [events]
            for ev in events:
                handler = ev["handler"]
                trigger = EVENT_TRIGGER[ev["trigger"]]
                if handler not in [h for h, _ in self.handlers]:
                    self.handlers.append((handler, ev["trigger"]))
                self.emit("lv_obj_add_event_cb(%s, [](lv_event_t* e) {" % var)
                self.emit("    auto* hs = static_cast<Handlers*>(lv_event_get_user_data(e));")
                self.emit("    if (hs && hs->%s) hs->%s(e);" % (handler, handler))
                self.emit("}, %s, h);" % trigger)

        for child in el.get("children", []):
            self.gen_element(child, var)

    def generate(self):
        # screen-level styles
        scr = self.schema.get("screen", {})
        for key, val in scr.get("style", {}).items():
            kind = STYLE_KEYS.get(key)
            if kind == "color":
                self.emit("lv_obj_set_style_%s(screen, %s, 0);" % (key, self.color(val)))
            elif kind == "opa":
                self.emit("lv_obj_set_style_%s(screen, %s, 0);" % (key, self.opa(val)))
            elif kind == "int":
                self.emit("lv_obj_set_style_%s(screen, %d, 0);" % (key, int(val)))
        if "no_scroll" in scr.get("flags", []):
            self.emit("lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);")

        for el in self.schema.get("elements", []):
            self.gen_element(el, "screen")

        out = []
        out.append("/* AUTO-GENERATED from ui/ui_schema.json by tools/gen_ui.py — DO NOT EDIT.")
        out.append(" * To change the UI, edit ui/ui_schema.json and rebuild. */")
        out.append("#pragma once")
        out.append("")
        out.append("#include <lvgl.h>")
        out.append("")
        out.append("#include <functional>")
        out.append("")
        out.append("namespace ui {")
        out.append("namespace gen {")
        out.append("")
        out.append("/* ---- Layout constants ---- */")
        for k, v in self.consts.items():
            out.append("constexpr lv_coord_t k%s = %d;" % (camel(k), v))
        out.append("")
        out.append("/* ---- Palette ---- */")
        for k, v in self.palette.items():
            out.append("constexpr uint32_t kCol%s = %s;" % (camel(k), v))
        out.append("")
        out.append("/* ---- Named widgets ---- */")
        out.append("struct Widgets {")
        for eid in self.ids:
            out.append("    lv_obj_t* %s = nullptr;" % eid)
        out.append("};")
        out.append("")
        out.append("/* ---- Event handlers declared in the schema ----")
        out.append(" * Assign these (e.g. in View::build) before or after gen::build();")
        out.append(" * the Handlers object must outlive the widgets. */")
        out.append("struct Handlers {")
        for handler, trigger in self.handlers:
            out.append("    std::function<void(lv_event_t*)> %s;  /* %s */" % (handler, trigger))
        out.append("};")
        out.append("")
        out.append("/* Builds the whole UI tree on `screen`, fills `w`, wires events to `h`. */")
        out.append("inline void build(lv_obj_t* screen, Widgets& w, Handlers* h = nullptr) {")
        out.append("    (void)h;")
        out.extend(self.lines)
        out.append("}")
        out.append("")
        out.append("}  // namespace gen")
        out.append("}  // namespace ui")
        out.append("")
        return "\n".join(out)


def run(project_dir):
    schema_path = os.path.join(project_dir, SCHEMA_REL)
    out_path = os.path.join(project_dir, OUTPUT_REL)
    with open(schema_path, "r", encoding="utf-8") as f:
        schema = json.load(f)
    content = Generator(schema).generate()
    # Skip the write if unchanged so incremental builds stay incremental.
    if os.path.exists(out_path):
        with open(out_path, "r", encoding="utf-8") as f:
            if f.read() == content:
                return
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(content)
    print("gen_ui: regenerated %s" % OUTPUT_REL)


try:
    Import("env")  # noqa: F821  (PlatformIO/SCons context)
    run(env["PROJECT_DIR"])  # noqa: F821
except NameError:
    run(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
