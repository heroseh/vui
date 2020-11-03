#ifndef VUI_H
#define VUI_H

#ifndef DAS_H
#error "VUI depends on DAS, make sure das.h is included before vui.h"
#endif

#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <ctype.h>

//
// =============================================
//
// general
//
// =============================================
//

typedef uint8_t VuiBool;
#define vui_false 0
#define vui_true 1
#define vui_ensure(expr) if (!(expr)) return 0;
#define vui_epsilon 0.001
#define vui_approx_eq(a, b) (fabsf((a) - (b)) < vui_epsilon)
#define vui_assert(cond, message, ...) \
	if (!(cond)) { fprintf(stderr, message"\n", ##__VA_ARGS__); abort(); }

#ifdef WAL_DEBUG_ASSERTIONS
#define vui_debug_assert vui_assert
#else
#define vui_debug_assert(cond, message, ...) (void)(cond)
#endif

#define vui_assert_finite_len(len, var_name) \
	vui_assert(isfinite(len), "expected %s parameter to be a finite length but got %f", var_name, length);

#define vui_assert_finite_size(size, var_name) \
	vui_assert(isfinite(size.x) && isfinite(size.y), "expected %s parameter to be a finite size but got x: %f, y: %f", var_name, size.x, size.y);

#define vui_auto_len INFINITY
#define vui_fill_len -INFINITY

#define vui_min(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

#define vui_max(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#define vui_clamp(v, min, max) \
   ({ __typeof__ (min) _min = (min); \
       __typeof__ (max) _max = (max); \
       __typeof__ (v) _v = (v); \
     (_v > _max) ? _max : (_v < _min) ? _min : v; })

typedef struct {
    float x;
    float y;
} VuiVec2;
typedef_DasStk(VuiVec2);

#define VuiVec2_init(x, y) (VuiVec2){ x, y }
#define VuiVec2_auto (VuiVec2){ vui_auto_len, vui_auto_len }
#define VuiVec2_fill (VuiVec2){ vui_fill_len, vui_fill_len }
#define VuiVec2_zero (VuiVec2){0}
VuiBool VuiVec2_has_auto(VuiVec2 v);
VuiBool VuiVec2_has_fill(VuiVec2 v);
VuiVec2 VuiVec2_add(VuiVec2 a, VuiVec2 b);
VuiVec2 VuiVec2_sub(VuiVec2 a, VuiVec2 b);
VuiVec2 VuiVec2_mul(VuiVec2 a, VuiVec2 b);
VuiVec2 VuiVec2_div(VuiVec2 a, VuiVec2 b);
float VuiVec2_len(VuiVec2 v);
VuiVec2 VuiVec2_scale(VuiVec2 v, float by);
VuiVec2 VuiVec2_norm(VuiVec2 v);
VuiVec2 VuiVec2_perp_left(VuiVec2 v);
VuiVec2 VuiVec2_perp_right(VuiVec2 v);

VuiVec2 VuiVec2_min(VuiVec2 a, VuiVec2 b);
VuiVec2 VuiVec2_max(VuiVec2 a, VuiVec2 b);
VuiVec2 VuiVec2_clamp(VuiVec2 v, VuiVec2 min, VuiVec2 max);

typedef struct {
    VuiVec2 top_left;
    VuiVec2 bottom_right;
} VuiRect;

#define VuiRect_init(x, y, ex, ey) (VuiRect){ x, y, ex, ey }
#define VuiRect_init_with_wh(x, y, w, h) (VuiRect){ x, y, (x) + (w), (x) + (h) }
#define VuiRect_init_with_vecs(top_left, bottom_right) (VuiRect){ top_left, bottom_right }
#define VuiRect_zero (VuiRect){ VuiVec2_zero, VuiVec2_zero }

VuiVec2 VuiRect_bottom_left(VuiRect rect);
VuiVec2 VuiRect_top_right(VuiRect rect);
VuiVec2 VuiRect_size(VuiRect rect);
VuiBool VuiRect_is_zero(VuiRect rect);
VuiRect VuiRect_clip(VuiRect a, VuiRect b);
VuiVec2 VuiRect_clip_pt(VuiRect rect, VuiVec2 pt);
VuiBool VuiRect_intersects(VuiRect a, VuiRect b);
VuiBool VuiRect_intersects_pt(VuiRect r, VuiVec2 pt);

//
// =============================================
//
// input
//
// =============================================
//

typedef enum {
    VuiMouseButtons_none = 0x0,
    VuiMouseButtons_left = 0x1,
    VuiMouseButtons_middle = 0x2,
    VuiMouseButtons_right = 0x4,
} VuiMouseButtons;

typedef enum {
    VuiInputActions_none = 0x0,
    //
    // the single key actions are the only ones you need
    // to fill in from your applications input system
    //
    VuiInputActions_ctrl_pressed = 0x1,
    VuiInputActions_shift_pressed = 0x2,
    VuiInputActions_left = 0x4,
    VuiInputActions_right = 0x8,
    VuiInputActions_down = 0x10,
    VuiInputActions_up = 0x20,
    VuiInputActions_enter = 0x40,
    VuiInputActions_space = 0x80,
    VuiInputActions_backspace = 0x100,
    VuiInputActions_delete = 0x200,
    VuiInputActions_home = 0x400,
    VuiInputActions_end = 0x800,
    VuiInputActions_page_up = 0x1000,
    VuiInputActions_page_down = 0x2000,
    VuiInputActions_focus_next = 0x4000, // TAB
    VuiInputActions_focus_pressed = 0x8000,
    VuiInputActions_focus_held = 0x10000,
    VuiInputActions_focus_released = 0x20000,
    //
    // these are the only two key actions that will have to be set manually
    //
    VuiInputActions_undo = 0x40001, // CTRL + Z
    VuiInputActions_cut = 0x80001, // CTRL + X
    VuiInputActions_copy = 0x100001, // CTRL + C
    VuiInputActions_paste = 0x200001, // CTRL + V
    VuiInputActions_select_all = 0x400001, // CTRL + A
    //
    // from here are no longer single keys and are
    // automatically worked out based on the combinations
    //
    VuiInputActions_redo = 0x8003, // CTRL + SHIFT + Z
    VuiInputActions_delete_to_start_of_word = 0x101, // CTRL + BACKSPACE
    VuiInputActions_delete_to_end_of_word = 0x201, // CTRL + DELETE
    VuiInputActions_word_start = 0x5, // CTRL + LEFT
    VuiInputActions_word_end = 0x9, // CTRL + RIGHT
    VuiInputActions_document_home = 0x401, // CTRL + HOME
    VuiInputActions_document_end = 0x801, // CTRL + END
    VuiInputActions_page_prev = 0x1001, // CTRL + PAGEUP
    VuiInputActions_page_next = 0x2001, // CTRL + PAGEDOWN
    VuiInputActions_select_left = 0x6, // SHIFT + LEFT
    VuiInputActions_select_right = 0xa, // SHIFT + RIGHT
    VuiInputActions_select_down = 0x12, // SHIFT + DOWN
    VuiInputActions_select_up = 0x22, // SHIFT + UP
    VuiInputActions_select_word_start = 0x7, // CTRL + SHIFT + LEFT
    VuiInputActions_select_word_end = 0xb, // CTRL + SHIFT + RIGHT
    VuiInputActions_select_home = 0x402, // SHIFT + HOME
    VuiInputActions_select_end = 0x802, // SHIFT + END
    VuiInputActions_select_page_up = 0x1002, // SHIFT + PAGEUP
    VuiInputActions_select_page_down = 0x2002, // SHIFT + PAGEDOWN
    VuiInputActions_select_to_document_home = 0x403, // CTRL + SHIFT + HOME
    VuiInputActions_select_to_document_end = 0x803, // CTRL + SHIFT + HOME
    VuiInputActions_focus_prev = 0x4002, // SHIFT + TAB
} VuiInputActions;

void vui_input_set_mouse_pos(float x, float y);
void vui_input_set_mouse_wheel_offset(float wheel_offset_x, float wheel_offset_y);
void vui_input_set_mouse_button_pressed(VuiMouseButtons buttons);
void vui_input_set_mouse_button_released(VuiMouseButtons buttons);
void vui_input_add_actions(VuiInputActions actions);
void vui_input_add_text(char* string, uint32_t string_length);

typedef uint32_t VuiCtrlIdHash;

typedef enum {
    VuiFocusState_none,
    // signals that the ctrl is mouse focused
    VuiFocusState_focused,
    // signals has been pressed this frame
    VuiFocusState_pressed,
    // signals has been double pressed this frame
    VuiFocusState_double_pressed,
    // signals is being held this frame
    VuiFocusState_held,
    // signals has been released this frame
    VuiFocusState_released,
} VuiFocusState;

VuiBool vui_has_mouse_over_ctrl();
VuiBool vui_has_scroll_mouse_focused_ctrl();
VuiBool vui_has_mouse_focused_ctrl();
VuiBool vui_ctrl_is_mouse_focused(VuiCtrlIdHash id_hash);
VuiBool vui_ctrl_is_focused(VuiCtrlIdHash id_hash);
VuiBool vui_ctrl_is_scroll_mouse_focused(VuiCtrlIdHash id_hash);
void vui_ctrl_set_focused(VuiCtrlIdHash id_hash);
VuiFocusState vui_ctrl_focus_state(VuiCtrlIdHash id_hash);

VuiVec2 vui_ctrl_get_last_top_left();
VuiVec2 vui_ctrl_get_last_size();

//
// =============================================
//
// resources
//
// =============================================
//

typedef uint32_t VuiUserFontId;
typedef uint32_t VuiImageId;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} VuiColor;
#define VuiColor_transparent (VuiColor){0}
#define VuiColor_black (VuiColor){0, 0, 0, 0xff}
#define VuiColor_white (VuiColor){0xff, 0xff, 0xff, 0xff}
#define VuiColor_init(r, g, b, a) (VuiColor){ r, g, b, a }

typedef struct {
    float line_height;
    VuiUserFontId user_id;
} VuiFont;

typedef struct {
    float width;
    VuiColor color;
} VuiBorder;
#define VuiBorder_init(width, color) (VuiBorder){ width, color }

typedef struct {
    float left;
    float right;
    float bottom;
    float top;
} VuiThickness;
#define VuiThickness_zero (VuiThickness){0}
#define VuiThickness_init(left, right, bottom, top) (VuiThickness) { left, right, bottom, top }
#define VuiThickness_init_hv(h, v) (VuiThickness) { h, h, v, v }
#define VuiThickness_init_even(len) (VuiThickness) { len, len, len, len }

float VuiThickness_horizontal(VuiThickness thickness);
float VuiThickness_vertical(VuiThickness thickness);
VuiVec2 VuiThickness_size(VuiThickness thickness);

//
// =============================================
//
// render
//
// =============================================
//

#ifndef VuiTextureId
#define VuiTextureId uint32_t
#endif

typedef struct {
	VuiVec2 pos;
	VuiVec2 uv;
	VuiColor color;
} VuiRenderVert;
typedef_DasStk(VuiRenderVert);

typedef struct {
	VuiTextureId texture_id;
	uint32_t verts_start_idx;
	uint32_t indices_start_idx;
	uint32_t indices_count;
} VuiRenderCmd;
typedef_DasStk(VuiRenderCmd);

#ifndef VuiRenderIdxT
#define VuiRenderIdxT uint16_t
#endif
typedef VuiRenderIdxT VuiRenderIdx;
typedef_DasStk(VuiRenderIdx);

typedef struct {
	DasStk(VuiRenderCmd) cmds;
    DasStk(VuiRenderVert) verts;
	DasStk(VuiRenderIdx) indices;
} VuiRenderLayer;
typedef_DasStk(VuiRenderLayer);

typedef struct {
    DasStk(VuiRenderLayer) layers;
    uint64_t hash;
} VuiRenderWindow;

void vui_render_line(VuiVec2 start_pos, VuiVec2 end_pos, VuiColor color, float width);
void vui_render_rect(VuiRect rect, VuiColor color, float radius);
void vui_render_rect_border(VuiRect rect, VuiColor color, float radius, float width);
void vui_render_triangle(VuiVec2 a, VuiVec2 b, VuiVec2 c, VuiColor color);
void vui_render_triangle_border(VuiVec2 a, VuiVec2 b, VuiVec2 c, VuiColor color, float width);
void vui_render_circle(VuiVec2 pos, float radius, VuiColor color);
void vui_render_circle_border(VuiVec2 pos, float radius, VuiColor color, float width);
void vui_render_text(VuiVec2 pos, VuiFont* font, char* text, uint32_t text_byte_count, VuiColor color, float wrap_at_width);
void vui_render_polyline(VuiVec2* points, uint32_t points_count, VuiColor color, float width, VuiBool connect_first_and_last);
void vui_render_convex_polygon(VuiVec2* points, uint32_t points_count, VuiColor color);
void vui_render_bezier_curve(VuiVec2 start_pos, VuiVec2 end_pos, VuiVec2 start_anchor_pos, VuiVec2 end_anchor_pos, VuiColor color, float width);

void vui_render_image(VuiTextureId texture_id, VuiRect rect, VuiRect uv_rect, VuiColor color);
void vui_render_image_(VuiTextureId texture_id, VuiRect rect, VuiRect uv_rect, VuiColor color, VuiBool is_glyph);

void vui_path_reset();
void vui_path_plot_point(VuiVec2 pt);
void vui_path_plot_arc(VuiVec2 pt, float radius, float angle_start, float angle_end, uint32_t segments_count);
void vui_path_plot_bezier_curve(VuiVec2 start_control_pt, VuiVec2 end_control_pt, VuiVec2 end_pt);
void vui_path_plot_rect(VuiRect rect, float radius);
void vui_path_plot_circle(VuiVec2 pt, float radius, VuiBool is_border);
void vui_render_path_stroked(VuiColor color, float width, VuiBool connect_first_and_last);
void vui_render_path_filled_convex(VuiColor color);

typedef struct {
	VuiRenderVert* verts;
	VuiRenderIdx* indices;
	uint32_t verts_start_idx;
} VuiRenderWriter;

VuiRenderWriter vui_render_get_writer(VuiTextureId texture_id, uint32_t verts_count, uint32_t indices_count);
void vui_render_inc_layer();
void vui_render_dec_layer();

//
// =============================================
//
// control styles
//
// =============================================
//

typedef struct {
    VuiThickness margin;
    VuiThickness padding;
} VuiCtrlStyle;
extern VuiCtrlStyle VuiCtrlStyle_zero;

typedef struct {
	VuiCtrlStyle ctrl;
	float spacing;
	VuiBool wrap;
	float wrap_spacing;
} VuiRowColumnLayoutStyle;
extern VuiRowColumnLayoutStyle VuiRowColumnLayoutStyle_zero;

typedef struct {
    VuiCtrlStyle ctrl;
    VuiColor bg_color;
    VuiBorder border;
    float radius;
} VuiBoxStyle;

typedef struct {
	VuiCtrlStyle ctrl;
	VuiColor color;
	float width;
	float radius;
} VuiSeparatorStyle;

typedef struct {
    VuiCtrlStyle ctrl;
    VuiColor color;
	VuiFont font;
} VuiTextStyle;

typedef struct {
    VuiCtrlStyle ctrl;
    float radius;
    float border_width;
    VuiColor inactive_bg_color;
    VuiColor inactive_border_color;
    VuiColor focused_bg_color;
    VuiColor focused_border_color;
    VuiColor text_color;
	VuiFont text_font;
    VuiColor selection_color;
    float selection_radius;
	VuiColor cursor_color;
	float cursor_width;
	float cursor_radius;
} VuiTextBoxStyle;

typedef struct {
    VuiCtrlStyle ctrl;
    float radius;
    float border_width;
    VuiColor inactive_bg_color;
    VuiColor inactive_border_color;
    VuiColor pressed_bg_color;
    VuiColor pressed_border_color;
    VuiColor focused_bg_color;
    VuiColor focused_border_color;
} VuiButtonStyle;

typedef struct {
    VuiButtonStyle button;
    VuiColor inactive_color;
    VuiColor pressed_color;
    VuiColor focused_color;
	VuiFont font;
} VuiTextButtonStyle;

typedef struct {
    VuiCtrlStyle ctrl;
    float radius;
    float border_width;
    VuiColor inactive_bg_color;
    VuiColor inactive_border_color;
    VuiColor focused_bg_color;
    VuiColor focused_border_color;
    VuiColor checked_color;
	float size;
} VuiCheckBoxStyle;

typedef struct {
    VuiCheckBoxStyle check_box;
    VuiColor color;
	VuiFont font;
} VuiTextCheckBoxStyle;

typedef struct {
    VuiCtrlStyle ctrl;
    float border_width;
    VuiColor inactive_bg_color;
    VuiColor inactive_border_color;
    VuiColor focused_bg_color;
    VuiColor focused_border_color;
    VuiColor selected_color;
	float size;
	float selected_size;
} VuiRadioButtonStyle;

typedef struct {
    VuiRadioButtonStyle radio_button;
    VuiColor color;
	VuiFont font;
} VuiTextRadioButtonStyle;

typedef struct {
    VuiBoxStyle box;
    VuiColor bar_color;
} VuiProgressBarStyle;

typedef struct {
	VuiBoxStyle box;
    float width;
	VuiButtonStyle slider;
} VuiScrollBarStyle;

typedef struct {
    VuiBoxStyle box;
    VuiScrollBarStyle scroll_bar;
} VuiScrollViewStyle;

//
// =============================================
//
// controls
//
// =============================================
//

//
// this must be an enum.
// the enum count must be passed into the vui_init
typedef uint16_t VuiWindowId;
typedef uint32_t VuiCtrlId;
typedef void (*VuiRenderCtrlFn)();

typedef uint8_t VuiLayoutType;
#define VuiLayoutType_container 0
#define VuiLayoutType_stack 1
#define VuiLayoutType_row 2
#define VuiLayoutType_column 3
extern char* VuiLayoutType_strings[];

typedef uint8_t VuiCheckState;
#define VuiCheckState_not_checked 0
#define VuiCheckState_checked 1
#define VuiCheckState_partially_checked 2

typedef uint8_t VuiAlign;
#define VuiAlign_top_left 0
#define VuiAlign_top_center 1
#define VuiAlign_top_right 2
#define VuiAlign_center_left 3
#define VuiAlign_center 4
#define VuiAlign_center_right 5
#define VuiAlign_bottom_left 6
#define VuiAlign_bottom_center 7
#define VuiAlign_bottom_right 8

typedef uint8_t VuiScrollViewFlags;
#define VuiScrollViewFlags_none 0x0
#define VuiScrollViewFlags_resizable 0x1
#define VuiScrollViewFlags_vertical_scroll 0x2
#define VuiScrollViewFlags_horizontal_scroll 0x4
#define VuiScrollViewFlags_always_show_vertical_bar 0xa // 0x8 + vertical_scroll
#define VuiScrollViewFlags_always_show_horizontal_bar 0x14 // 0x10 + horizontal_scroll

typedef union {
    VuiLayoutType type;
	struct {
		VuiLayoutType type;
		VuiBool has_child;
	} container;
	struct {
		VuiLayoutType type;
		VuiVec2 offset;
		uint16_t stack_child_start_idx;
		VuiAlign align;
	} stack;
	struct {
		VuiLayoutType type;
		VuiBool wrap;
		VuiVec2 next_ctrl_top_left;
		float max_pt; // used by row and column layouts
		float spacing;
		float wrap_spacing;
	} row_column;
} _VuiLayout;

void vui_container_layout_start(VuiCtrlId id, VuiVec2 size, VuiCtrlStyle* style);
void vui_container_layout_end();

void vui_stack_layout_set_next_pos(VuiAlign align, VuiVec2 offset);
void vui_stack_layout_start(VuiCtrlId id, VuiVec2 size, VuiCtrlStyle* style);
void vui_stack_layout_end(VuiVec2 auto_size_extension);

void vui_column_layout_start(VuiCtrlId id, VuiVec2 size, VuiRowColumnLayoutStyle* style);
void vui_column_layout_end();

void vui_row_layout_start(VuiCtrlId id, VuiVec2 size, VuiRowColumnLayoutStyle* style);
void vui_row_layout_end();

// HACK: hacky things that need finalizing
float vui_row_column_remaining_length();
VuiVec2 vui_row_column_next_ctrl_top_left();

VuiCtrlIdHash vui_ctrl_gen_id_hash(VuiCtrlId id);
void vui_ctrl_start(VuiCtrlId id, VuiVec2 size, VuiRenderCtrlFn render_ctrl_fn, VuiCtrlStyle* style);
void vui_ctrl_end();

void vui_box_start(VuiCtrlId id, VuiVec2 size, VuiBoxStyle* style);
void vui_box_end();

VuiVec2 vui_get_text_size(char* text, uint32_t text_length, float wrap_at_width, VuiTextStyle* style);

// wrap_at_width = 0.0 to not have word wrapping
#define vui_text(text, wrap_at_width, style) vui_text_(text, strlen(text), wrap_at_width, style)
void vui_text_(char* text, uint32_t text_length, float wrap_at_width, VuiTextStyle* style);
void vui_text_with_size(VuiVec2 size, char* text, uint32_t text_length, float wrap_at_width, VuiTextStyle* style);

void vui_image(VuiVec2 size, VuiTextureId texture_id, VuiRect uv_rect, VuiColor color, VuiCtrlStyle* style);

void vui_spacing(VuiVec2 size);
void vui_separator(VuiSeparatorStyle* style);

void vui_button_core_start(VuiCtrlId id, VuiVec2 size, VuiBool pressed, VuiButtonStyle* style);
void vui_button_core_end();

// returns true when released
VuiBool vui_button_start(VuiCtrlId id, VuiVec2 size, VuiButtonStyle* style);
void vui_button_end();
#define vui_text_button(id, text, style) vui_text_button_(id, text, strlen(text), style)
VuiBool vui_text_button_(VuiCtrlId id, char* text, uint32_t text_length, VuiTextButtonStyle* style);
VuiBool vui_image_button(VuiCtrlId id, VuiVec2 size, VuiTextureId texture_id, VuiRect uv_rect, VuiColor color, VuiButtonStyle* style);

// returns true when pressed or held
VuiBool vui_press_button_start(VuiCtrlId id, VuiVec2 size, VuiButtonStyle* style);
void vui_press_button_end();

VuiBool vui_toggle_button_start(VuiCtrlId id, VuiVec2 size, VuiBool* pressed, VuiButtonStyle* style);
// returns *pressed, that is toggled when this has been pressed on this frame
void vui_toggle_button_end();
#define vui_text_toggle_button(id, pressed, text, style) vui_text_toggle_button_(id, pressed, text, strlen(text), style)
VuiBool vui_text_toggle_button_(VuiCtrlId id, VuiBool* pressed, char* text, uint32_t text_length, VuiTextButtonStyle* style);
VuiBool vui_image_toggle_button(VuiCtrlId id, VuiBool* pressed, VuiVec2 size, VuiTextureId texture_id, VuiRect uv_rect, VuiColor color, VuiButtonStyle* style);

VuiBool vui_select_button_start(VuiCtrlId id, VuiVec2 size, VuiCtrlId* selected_id, VuiButtonStyle* style);
void vui_select_button_end();
#define vui_text_select_button(id, selected_id, text, style) vui_text_select_button_(id, selected_id, text, strlen(text), style)
VuiBool vui_text_select_button_(VuiCtrlId id, VuiCtrlId* selected_id, char* text, uint32_t text_length, VuiTextButtonStyle* style);
VuiBool vui_image_select_button(VuiCtrlId id, VuiCtrlId* selected_id, VuiVec2 size, VuiTextureId texture_id, VuiRect uv_rect, VuiColor color, VuiButtonStyle* style);
#define vui_image_text_select_button(id, selected_id, text, image_size, texture_id, uv_rect, color, style) \
	vui_image_text_select_button_(id, selected_id, text, strlen(text), image_size, texture_id, uv_rect, color, style)
VuiBool vui_image_text_select_button_(VuiCtrlId id, VuiCtrlId* selected_id, char* text, uint32_t text_length, VuiVec2 image_size, VuiTextureId texture_id, VuiRect uv_rect, VuiColor color, VuiTextButtonStyle* style);

VuiCheckState vui_check_box(VuiCtrlId id, VuiCheckState* check_state, VuiCheckBoxStyle* style);
#define vui_text_check_box(id, check_state, text, style) vui_text_check_box_(id, check_state, text, strlen(text), style)
VuiCheckState vui_text_check_box_(VuiCtrlId id, VuiCheckState* check_state, char* text, uint32_t text_length, VuiTextCheckBoxStyle* style);
VuiCheckState vui_image_check_box(VuiCtrlId id, VuiCheckState* check_state, VuiVec2 image_size, VuiTextureId texture_id, VuiRect uv_rect, VuiColor color, VuiCheckBoxStyle* style);

VuiBool vui_radio_button(VuiCtrlId id, VuiCtrlId* selected_id, VuiRadioButtonStyle* style);
#define vui_text_radio_button(id, selected_id, text, style) vui_text_radio_button_(id, selected_id, text, strlen(text), style)
VuiBool vui_text_radio_button_(VuiCtrlId id, VuiCtrlId* selected_id, char* text, uint32_t text_length, VuiTextRadioButtonStyle* style);
VuiBool vui_image_radio_button(VuiCtrlId id, VuiCtrlId* selected_id, VuiVec2 image_size, VuiTextureId texture_id, VuiRect uv_rect, VuiColor color, VuiRadioButtonStyle* style);

void vui_progress_bar(VuiVec2 size, float value, float min, float max, VuiProgressBarStyle* style);

void vui_scroll_bar(VuiCtrlId id, float length, float* content_offset, float content_length, VuiBool is_horizontal, VuiScrollBarStyle* style);
void vui_scroll_view_start(VuiCtrlId id, VuiVec2* size, VuiVec2* content_offset, VuiScrollViewFlags flags, VuiScrollViewStyle* style);
void vui_scroll_view_end();

VuiBool vui_text_box(VuiCtrlId id, float width, DasStk(char)* string, VuiTextBoxStyle* style);
VuiBool vui_input_box_uint(VuiCtrlId id, float width, uint32_t* value, VuiTextBoxStyle* style);
VuiBool vui_input_box_sint(VuiCtrlId id, float width, int32_t* value, VuiTextBoxStyle* style);
VuiBool vui_input_box_float(VuiCtrlId id, float width, float* value, VuiTextBoxStyle* style);

/*

VuiBool vui_drag_button(Vui* vui, VuiCtrlId id, VuiVec2 size, VuiButtonStyle* style);
void vui_drag_button_start(Vui* vui, VuiCtrlId id, VuiVec2 size, VuiButtonStyle* style);
// returns true when being held or has been pressed on this frame
VuiBool vui_drag_button_end(Vui* vui);
*/


//
// =============================================
//
// vui
//
// =============================================
//



typedef uint32_t VuiCtrlId;
#define VuiCtrlId_mask 0x3fffffff
#define VuiCtrlId_focusable_mask 0x80000000
#define VuiCtrlId_scrollable_mask 0x40000000
typedef struct VuiCtrl VuiCtrl;
struct VuiCtrl {
	uint32_t parent_idx;
	VuiCtrlIdHash id_hash;
	VuiCtrlStyle* style;
    VuiVec2 max_bottom_right;
	VuiRect rect;
	_VuiLayout layout;
	VuiRenderCtrlFn render_fn;
};
typedef_DasStk(VuiCtrl);

typedef uint16_t VuiViewportId;

typedef void (*VuiRenderGlyphFn)(VuiTextureId glyph_texture_id, VuiRect rect, VuiRect uv_rect);
typedef VuiVec2 (*VuiPositionTextFn)(void* userdata, VuiUserFontId user_font_id, char* text, uint32_t text_length, VuiVec2 top_left, VuiRenderGlyphFn render_glyph_fn);
typedef void (*VuiTextBoxFocusChange)(VuiBool focused);

typedef struct {
	VuiPositionTextFn position_text_fn;
	void* position_text_userdata;
	VuiTextBoxFocusChange text_box_focus_change_fn;
	uint16_t windows_count;
} VuiSetup;

VuiBool vui_init(VuiSetup* setup);
VuiCtrl* vui_get_render_ctrl();
VuiCtrlIdHash vui_get_ctrl_id_hash();
VuiVec2 vui_get_content_size();
VuiCtrl* vui_get_ctrl();
VuiVec2 vui_next_ctrl_top_left();
void vui_frame_start();
void vui_frame_end();
void vui_window_start(VuiWindowId id, VuiVec2 size);
void vui_window_end();

VuiRenderWindow* vui_window_build_render(VuiWindowId id, float scale_factor, VuiBool pixel_snapping);
void vui_window_set_mouse_focused(VuiWindowId id);
void vui_window_set_focused(VuiWindowId id);

void vui_window_dump_render(VuiWindowId id, FILE* file);

// allocate zeroed memory that is cleared at the end of the frame.
// useful if you need to make things like styles for child controls.
#define vui_frame_data_alloc_elmt(T) (T*)vui_frame_data_alloc(sizeof(T), alignof(T));
void* vui_frame_data_alloc(uint32_t size, uint32_t align);

#endif // VUI_H

