#ifndef VUI_H
#define VUI_H

#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <ctype.h>

// ===========================================================================================
//
//
// Configuration - compile time flags and constants
//
//
// ===========================================================================================

#define VUI_DEBUG_ASSERTIONS 0

//
// you can redefine these macros to supply your own custom memory allocation
#if !defined(vui_mem_alloc)
#include <stdlib.h> //malloc, etc.
#define vui_mem_alloc(allocator, size, align) malloc(size)
#define vui_mem_realloc(allocator, ptr, size, new_size, align) realloc(ptr, new_size)
#define vui_mem_dealloc(allocator, ptr, size, align) free(ptr)
#endif

#define vui_mem_alloc_array(T, allocator, count) vui_mem_alloc(allocator, sizeof(T) * (count), alignof(T))
#define vui_mem_realloc_array(T, allocator, ptr, count, new_count) vui_mem_realloc(allocator, ptr, sizeof(T) * (count), sizeof(T) * (new_count), alignof(T))
#define vui_mem_dealloc_array(T, allocator, ptr, count) vui_mem_dealloc(allocator, ptr, sizeof(T) * (count), alignof(T))

#ifndef noreturn
#define noreturn _Noreturn
#endif

#ifndef alignof
#define alignof _Alignof
#endif

// ===========================================================================================
//
//
// Misc Helpers
//
//
// ===========================================================================================

typedef uint8_t VuiBool;
#define vui_false 0
#define vui_true 1
#define vui_ensure(expr) if (!(expr)) return 0;
#define vui_epsilon 0.001
#define vui_approx_eq(a, b) (fabsf((a) - (b)) < vui_epsilon)

#define vui_ensure_alloc_ok(ptr, ...) if (ptr == NULL) { _vui.flags |= _VuiFlags_out_of_memory; return __VA_ARGS__; }

noreturn void _vui_abort(const char* file, int line, const char* func, char* assert_test, char* message_fmt, ...);
#define vui_abort(message_fmt, ...) \
	_vui_abort(__FILE__, __LINE__, __func__, NULL, message_fmt, ##__VA_ARGS__);

#define vui_assert(cond, message_fmt, ...) \
	if (!(cond)) _vui_abort(__FILE__, __LINE__, __func__, #cond, message_fmt, ##__VA_ARGS__);

#define vui_assert_loc(file, line, func, cond, message_fmt, ...) \
	if (!(cond)) _vui_abort(file, line, func, #cond, message_fmt, ##__VA_ARGS__);

#if VUI_DEBUG_ASSERTIONS
#define vui_debug_assert vui_assert
#else
#define vui_debug_assert(cond, message_fmt, ...) (void)(cond)
#endif

#define vui_auto_len 0
#define vui_fill_len INFINITY
#define vui_sib_id __LINE__

static inline float vui_min(float a, float b) { return a < b ? a : b; }
static inline float vui_max(float a, float b) { return a > b ? a : b; }
static inline float vui_clamp(float v, float min, float max) { return v < min ? min : (v > max ? max : v); }
static inline float vui_lerp(float a, float b, float t) { return (b - a) * t + a; }

#define vui_is_power_of_two(v) ((v) != 0) && (((v) & ((v) - 1)) == 0)

static inline void* vui_ptr_round_up_align(void* ptr, uintptr_t align) {
	vui_debug_assert(vui_is_power_of_two(align), "align must be a power of two but got: %zu", align);
	return (void*)(((uintptr_t)ptr + (align - 1)) & ~(align - 1));
}

static inline void* vui_ptr_round_down_align(void* ptr, uintptr_t align) {
	vui_debug_assert(vui_is_power_of_two(align), "align must be a power of two but got: %zu", align);
	return (void*)((uintptr_t)ptr & ~(align - 1));
}

#define vui_ptr_add(ptr, by) (void*)((uintptr_t)(ptr) + (uintptr_t)(by))
#define vui_ptr_sub(ptr, by) (void*)((uintptr_t)(ptr) - (uintptr_t)(by))
#define vui_ptr_diff(to, from) ((char*)(to) - (char*)(from))

#define _vui_defer_loop(start_expr, end_expr) for (int _i_ = (start_expr, 0); _i_ < 1; _i_ += 1, end_expr)

// ===========================================================================================
//
//
// Stack - LIFO (inspired by stb stretchy buffer)
// will not work for a type with an alignment greater than alignof(void*)
//
//
// ===========================================================================================

#define VuiStk(T) T*

typedef struct _VuiStkHeader _VuiStkHeader;
struct _VuiStkHeader {
    uint32_t cap;
    uint32_t count;
};

//
// only get the typeof if c++ as c will implicitly cast to and from a void*
#ifdef __cplusplus
#define _vui_typeof(type) decltype(type)
#else
#define _vui_typeof(type) void*
#endif

//
// these are the functions that the macro versions call, refer to the comments of the macro.
void* _VuiStk_resize_cap(void* stk, uint32_t new_cap, uint32_t elmt_size);
void* _VuiStk_push_many(void** stk_ptr, uint32_t elmts_count, uint32_t elmt_size);
void* _VuiStk_insert_many(void** stk_ptr, uint32_t idx, uint32_t elmts_count, uint32_t elmt_size);
void _VuiStk_remove_range_shift(void* stk, uint32_t start_idx, uint32_t end_idx, uint32_t elmt_size);

//
// the number of elements the stacks will initially be able to hold
#define vui_stk_init_cap 16

// get the header of the stack that sits directly before the pointer you have.
#define _VuiStk_header(stk) ((_VuiStkHeader*)((char*)(stk) - sizeof(_VuiStkHeader)))

// gets the size of all the elements in bytes, returns zero if stk is NULL
#define VuiStk_size(stk) ((stk) ? (uintptr_t)_VuiStk_header(stk)->count * sizeof(*(stk)) : 0)

// removes all the elements from the stack by setting the count to 0
#define VuiStk_clear(stk) ((stk) ? _VuiStk_header(stk)->count = 0 : 0)

// gets the number of elements in the stack, returns zero if stk is NULL
#define VuiStk_count(stk) ((stk) ? _VuiStk_header(stk)->count : 0)

// gets the capacity of the stack, returns zero if stk is NULL
#define VuiStk_cap(stk) ((stk) ? _VuiStk_header(stk)->cap : 0)

// returns the last element, you can also take the address of this
#define VuiStk_last(stk) ((stk)[_VuiStk_header(stk)->count - 1])

// returns true if the stack has reached its maximum capacity, if stk is null returns true
#define VuiStk_is_full(stk) (VuiStk_count(stk) == VuiStk_cap(stk))

//
// reallocates the stack to hold @param(new_cap) number of elements.
//
// @param stk: the stack of type VuiStk(T)
//
// @param new_cap: the number of elements you want to be able to hold in the stack before you need to reallocate again
//
// @return: on allocation failure NULL is returned, otherwise returns the newly allocated stack of type VuiStk(T)
//
// @example:
// stk = VuiStk_resize_cap(stk, 24);
//
#define VuiStk_resize_cap(stk, new_cap) (_vui_typeof(stk))_VuiStk_resize_cap(stk, new_cap, sizeof(*(stk)))

// pushes an uninitialized element on the stack and returns a pointer to it.
// NULL is returned on allocation failure.
//
// @example:
// uin32_t* elmt = VuiStk_push(&stk, 24);
#define VuiStk_push(stk_ptr) (_vui_typeof(*(stk_ptr)))VuiStk_push_many(stk_ptr, 1)

// pushes @param(elmts_count) number of uninitialized elements on the stack and returns a pointer to the first one.
// NULL is returned on allocation failure.
#define VuiStk_push_many(stk_ptr, elmts_count) (_vui_typeof(*(stk_ptr)))_VuiStk_push_many((void**)stk_ptr, elmts_count, sizeof(**(stk_ptr)))

// inserts an uninitialized element into the stack at @param(idx) and returns a pointer to it.
// NULL is returned on allocation failure.
//
// @example:
// uin32_t* elmt = VuiStk_insert(&stk, 4, 24);
#define VuiStk_insert(stk_ptr, idx) (_vui_typeof(*(stk_ptr)))VuiStk_insert_many(stk_ptr, idx, 1)

// inserts @param(elmts_count) number of uninitialized elements on the stack and returns a pointer to the first one.
// NULL is returned on allocation failure.
#define VuiStk_insert_many(stk_ptr, idx, elmts_count) (_vui_typeof(*(stk_ptr)))_VuiStk_insert_many((void**)stk_ptr, idx, elmts_count, sizeof(**(stk_ptr)))

//
// removes a range of elements by shifting the ones to it's right to the left.
// this retains the order of elements but comes at a cost of copying all the elements to it's right.
//
// @param stk: the stack of type VuiStk(T)
//
// @param start_idx: the index of the first element you wish to remove
//
// @param end_idx: the index after the last element you wish to remove
//                 this must be greater than @param(start_idx)
//
// @example:
// VuiStk_remove_range_shift(stk, 12, 18);
//
#define VuiStk_remove_range_shift(stk, start_idx, end_idx) _VuiStk_remove_range_shift(stk, start_idx, end_idx)

//
// deallocates the stack and returns NULL.
//
// @example:
// stk = VuiStk_deinit(stk);
//
#define VuiStk_deinit(stk) ((stk) ? vui_mem_dealloc(_VuiStk_header(stk), VuiStk_size(stk) + sizeof(_VuiStkHeader), alignof(void*)), NULL : NULL)

// ===========================================================================================
//
//
// Common Types
//
//
// ===========================================================================================

typedef struct {
    float x;
    float y;
} VuiVec2;

#define VuiVec2_init(x, y) (VuiVec2){ x, y }
#define VuiVec2_zero (VuiVec2){0}
static inline VuiVec2 VuiVec2_add(VuiVec2 a, VuiVec2 b) { return VuiVec2_init(a.x + b.x, a.y + b.y); }
static inline VuiVec2 VuiVec2_sub(VuiVec2 a, VuiVec2 b) { return VuiVec2_init(a.x - b.x, a.y - b.y); }
static inline VuiVec2 VuiVec2_mul(VuiVec2 a, VuiVec2 b) { return VuiVec2_init(a.x * b.x, a.y * b.y); }
static inline VuiVec2 VuiVec2_div(VuiVec2 a, VuiVec2 b) { return VuiVec2_init(a.x / b.x, a.y / b.y); }
static inline float VuiVec2_len(VuiVec2 v) { return sqrtf((v.x * v.x) + (v.y * v.y)); }
static inline VuiVec2 VuiVec2_scale(VuiVec2 v, float by) { return VuiVec2_init(v.x * by, v.y * by); }
static inline VuiVec2 VuiVec2_norm(VuiVec2 v) {
	float k = 1.0 / sqrtf((v.x * v.x) + (v.y * v.y));
	return VuiVec2_init(v.x * k, v.y * k);
}
static inline VuiVec2 VuiVec2_perp_left(VuiVec2 v) { return VuiVec2_init(v.y, -v.x); }
static inline VuiVec2 VuiVec2_perp_right(VuiVec2 v) { return VuiVec2_init(-v.y, v.x); }

static inline VuiVec2 VuiVec2_min(VuiVec2 a, VuiVec2 b) { return VuiVec2_init(vui_min(a.x, b.x), vui_min(a.y, b.y)); }
static inline VuiVec2 VuiVec2_max(VuiVec2 a, VuiVec2 b) { return VuiVec2_init(vui_max(a.x, b.x), vui_max(a.y, b.y)); }
static inline VuiVec2 VuiVec2_clamp(VuiVec2 v, VuiVec2 min, VuiVec2 max) { return VuiVec2_init(vui_clamp(v.x, min.x, max.x), vui_clamp(v.y, min.y, max.y)); }

typedef struct VuiColor VuiColor;
struct VuiColor {
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t a;
};
#define VuiColor_init(r, g, b, a) ((VuiColor){r,g,b,a})
#define VuiColor_white ((VuiColor){1.f, 1.f, 1.f, 1.f})
#define VuiColor_black ((VuiColor){0.f, 0.f, 0.f, 1.f})
#define VuiColor_transparent ((VuiColor){0})

typedef union VuiVec4 VuiVec4;
typedef VuiVec4 VuiRect;
typedef VuiVec4 VuiThickness;
union VuiVec4 {
	// thickness: margin, padding
	struct {
		VuiVec2 left_top;
		VuiVec2 right_bottom;
	};
	struct {
		float left;
		float top;
		float right;
		float bottom;
	};
	// rectangle
	struct {
		float x;
		float y;
		union {
			struct {
				float ex;
				float ey;
			};
			struct {
				float z;
				float w;
			};
		};
	};
	float array[4];
};

#define VuiVec4_init(x_, y_, z_, w_) ((VuiVec4){.x=x_, .y=y_, .z=z_, .w=w_})
#define VuiThickness_zero ((VuiVec4){0})
#define VuiThickness_init(left, top, right, bottom) ((VuiVec4){.left = left_, .top = top_, .right = right_, .bottom = bottom_})
#define VuiThickness_init_even(size) ((VuiVec4){.left=size, .top=size, .right=size, .bottom=size})
#define VuiThickness_hv_init(horizontal, vertical) ((VuiVec4){.left=horizontal, .top=vertical, .right=horizontal, .bottom=vertical})
#define VuiThickness_horizontal(thickness) ((thickness).left + (thickness).right)
#define VuiThickness_vertical(thickness) ((thickness).top + (thickness).bottom)
#define VuiThickness_hv(thickness) VuiVec2_init(((thickness).left + (thickness).right), (thickness).top + (thickness).bottom)
#define VuiRect_zero ((VuiVec4){0})
#define VuiRect_init(left_, top_, right_, bottom_) ((VuiVec4){.left = left_, .top = top_, .right = right_, .bottom = bottom_})
#define VuiRect_init_wh(left_, top_, width, height) ((VuiVec4){.left = left_, .top = top_, .right = (left_) + (width), .bottom = (top_) + (height)})
#define VuiRect_init_v2(left_top_, right_bottom_) ((VuiVec4){.left_top = left_top_, .right_bottom = right_bottom_})

#define VuiRect_left_bottom(rect) VuiVec2_init((rect).left, (rect).bottom)
#define VuiRect_right_top(rect) VuiVec2_init((rect).right, (rect).top)
#define VuiRect_size(rect) VuiVec2_init((rect).left + (rect).right, (rect).top + (rect).bottom)
static inline float VuiRect_width(const VuiRect* rect) { return rect->right - rect->left; }
static inline float VuiRect_neg_width(const VuiRect* rect) { return rect->left - rect->right; }
static inline float VuiRect_height(const VuiRect* rect) { return rect->bottom - rect->top; }
VuiRect VuiRect_clip(const VuiRect* a, const VuiRect* b);
VuiVec2 VuiRect_clip_pt(const VuiRect* rect, VuiVec2 pt);
VuiBool VuiRect_intersects(const VuiRect* a, const VuiRect* b);
VuiBool VuiRect_intersects_pt(const VuiRect* r, VuiVec2 pt);

typedef uint32_t VuiFontId;
typedef uint32_t VuiImageId;
typedef uint32_t VuiCtrlHash;
typedef uint32_t VuiCtrlId;
typedef uint32_t VuiCtrlSibId;

typedef enum {
    VuiFocusState_none,
    // signals that the ctrl is mouse or keyboard focused
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

// ===========================================================================================
//
//
// Input
//
//
// ===========================================================================================

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

VuiBool vui_input_is_mouse_over_ctrl();
VuiBool vui_input_is_mouse_scroll_focused_ctrl();
VuiBool vui_input_is_mouse_focused_ctrl();

VuiBool vui_ctrl_is_mouse_focused(VuiCtrlHash id_hash);
VuiBool vui_ctrl_is_focused(VuiCtrlHash id_hash);
VuiBool vui_ctrl_is_mouse_scroll_focused(VuiCtrlHash id_hash);
void vui_ctrl_set_focused(VuiCtrlHash id_hash);

// ===========================================================================================
//
//
// Control & Style Types & Funcions
//
//
// ===========================================================================================

typedef uint8_t VuiAlign;
enum {
	VuiAlign_left_top,
	VuiAlign_left_center,
	VuiAlign_left_bottom,
	VuiAlign_center_top,
	VuiAlign_center,
	VuiAlign_center_bottom,
	VuiAlign_right_top,
	VuiAlign_right_center,
	VuiAlign_right_bottom,
};

typedef uint8_t VuiImageScaleMode;
enum {
	VuiImageScaleMode_stretch, // image is scaled to fit but will not maintain the aspect ratio
	VuiImageScaleMode_uniform, // image is scaled to fit and will maintain the aspect ratio
	VuiImageScaleMode_uniform_crop, // image is scaled to fit on a single side and crop the other, this will maintain the aspect ratio
	VuiImageScaleMode_none, // image is not scaled, the original size is used.
};

typedef union VuiCtrlAttrValue VuiCtrlAttrValue;
union VuiCtrlAttrValue {
	float float_;
	VuiVec2 vec2;
	VuiThickness thickness;
	VuiColor color;
	VuiFontId font_id;
	VuiAlign align;
	VuiBool bool_;
	VuiImageScaleMode image_scale_mode;
};

typedef uint64_t VuiCtrlAttrFlags; // VuiCtrlAttr -> VuiCtrlAttrFlags = (1 << VuiCtrlAttr_*)
typedef uint8_t VuiCtrlAttr;
enum {
    VuiCtrlAttr_margin, // VuiVec4.thickness
    VuiCtrlAttr_padding, // VuiVec4.thickness
	VuiCtrlAttr_width, // float
	VuiCtrlAttr_width_min, // float
	VuiCtrlAttr_width_max, // float
	VuiCtrlAttr_height, // float
	VuiCtrlAttr_height_min, // float
	VuiCtrlAttr_height_max, // float
	VuiCtrlAttr_offset, // VuiVec2
	VuiCtrlAttr_align, // VuiAlign
	VuiCtrlAttr_layout_spacing, // float
	VuiCtrlAttr_layout_wrap_spacing, // float
	VuiCtrlAttr_layout_wrap, // VuiBool
    VuiCtrlAttr_bg_color, // VuiColor
    VuiCtrlAttr_border_width, // VuiVec4.float
    VuiCtrlAttr_border_color, // VuiColor
	VuiCtrlAttr_radius, // VuiVec4.float
    VuiCtrlAttr_text_color, // VuiColor
    VuiCtrlAttr_check_color, // VuiColor
    VuiCtrlAttr_check_size, // float
    VuiCtrlAttr_separator_size, // float
    VuiCtrlAttr_image_scale_mode, // VuiImageScaleMode
	VuiCtrlAttr_text_font_id,
    VuiCtrlAttr_text_selection_color, // VuiColor
    VuiCtrlAttr_text_selection_radius, // VuiVec4.radius
    VuiCtrlAttr_text_cursor_color, // VuiColor
    VuiCtrlAttr_text_cursor_width, // float
    VuiCtrlAttr_text_cursor_radius, // VuiVec4.radius
    VuiCtrlAttr_COUNT,
};

typedef uint8_t VuiCtrlStateFlags;
enum {
	VuiCtrlStateFlags_disabled = 0x1,
	VuiCtrlStateFlags_active = 0x2,
	VuiCtrlStateFlags_mouse_focused = 0x4,
	VuiCtrlStateFlags_focused = 0x8,
	VuiCtrlStateFlags_default = 0x10,
};
typedef uint8_t VuiCtrlState;
enum {
	VuiCtrlState_disabled,
	VuiCtrlState_active,
	VuiCtrlState_mouse_focused,
	VuiCtrlState_focused,
	VuiCtrlState_default,
	VuiCtrlState_COUNT,
};

typedef struct VuiStyle VuiStyle;
struct VuiStyle {
	// used to tell if a state has a perticular style attribute set.
	VuiCtrlAttrFlags state_attr_flags[VuiCtrlState_COUNT];
	VuiCtrlAttrValue state_attr_values[VuiCtrlState_COUNT][VuiCtrlAttr_COUNT];
};

//
// push and pop the global style.
extern void vui_push_style(VuiStyle* style);
extern void vui_pop_style();
#define vui_scope_style(style) _vui_defer_loop(vui_push_style(style), vui_pop_style())

//
// set and unset attribute of a control state in a style
void _VuiStyle_set_attr(VuiStyle* style, VuiCtrlAttr attr, VuiCtrlState ctrl_state, VuiCtrlAttrValue value);
void _VuiStyle_unset_attr(VuiStyle* style, VuiCtrlAttr attr, VuiCtrlState ctrl_state);

#define VuiStyle_set_bg_color(style, ctrl_state, value) _VuiStyle_set_attr(style, VuiCtrlAttr_bg_color, ctrl_state, (VuiCtrlAttrValue) { .color = value })
#define VuiStyle_unset_bg_color(style, ctrl_state) _VuiStyle_unset_attr(style, VuiCtrlAttr_bg_color, ctrl_state)

#define VuiStyle_set_border_width(style, ctrl_state, value) _VuiStyle_set_attr(style, VuiCtrlAttr_border_width, ctrl_state, (VuiCtrlAttrValue) { .float_ = value })
#define VuiStyle_unset_border_width(style, ctrl_state) _VuiStyle_unset_attr(style, VuiCtrlAttr_border_width, ctrl_state)

#define VuiStyle_set_border_color(style, ctrl_state, value) _VuiStyle_set_attr(style, VuiCtrlAttr_border_color, ctrl_state, (VuiCtrlAttrValue) { .color = value })
#define VuiStyle_unset_border_color(style, ctrl_state) _VuiStyle_unset_attr(style, VuiCtrlAttr_border_color, ctrl_state)

#define VuiStyle_set_radius(style, ctrl_state, value) _VuiStyle_set_attr(style, VuiCtrlAttr_radius, ctrl_state, (VuiCtrlAttrValue) { .float_ = value })
#define VuiStyle_unset_radius(style, ctrl_state) _VuiStyle_unset_attr(style, VuiCtrlAttr_radius, ctrl_state)

#define VuiStyle_set_text_color(style, ctrl_state, value) _VuiStyle_set_attr(style, VuiCtrlAttr_text_color, ctrl_state, (VuiCtrlAttrValue) { .color = value })
#define VuiStyle_unset_text_color(style, ctrl_state) _VuiStyle_unset_attr(style, VuiCtrlAttr_text_color, ctrl_state)

#define VuiStyle_set_check_color(style, ctrl_state, value) _VuiStyle_set_attr(style, VuiCtrlAttr_check_color, ctrl_state, (VuiCtrlAttrValue) { .color = value })
#define VuiStyle_unset_check_color(style, ctrl_state) _VuiStyle_unset_attr(style, VuiCtrlAttr_check_color, ctrl_state)

#define VuiStyle_set_check_size(style, ctrl_state, value) _VuiStyle_set_attr(style, VuiCtrlAttr_check_size, ctrl_state, (VuiCtrlAttrValue) { .float_ = value })
#define VuiStyle_unset_check_size(style, ctrl_state) _VuiStyle_unset_attr(style, VuiCtrlAttr_check_size, ctrl_state)

#define VuiStyle_set_separator_size(style, ctrl_state, value) _VuiStyle_set_attr(style, VuiCtrlAttr_separator_size, ctrl_state, (VuiCtrlAttrValue) { .float_ = value })
#define VuiStyle_unset_separator_size(style, ctrl_state) _VuiStyle_unset_attr(style, VuiCtrlAttr_separator_size, ctrl_state)

#define VuiStyle_set_image_scale_mode(style, ctrl_state, value) _VuiStyle_set_attr(style, VuiCtrlAttr_image_scale_mode, ctrl_state, (VuiCtrlAttrValue) { .image_scale_mode = value })
#define VuiStyle_unset_image_scale_mode(style, ctrl_state) _VuiStyle_unset_attr(style, VuiCtrlAttr_image_scale_mode, ctrl_state)

#define VuiStyle_set_text_font_id(style, ctrl_state, value) _VuiStyle_set_attr(style, VuiCtrlAttr_text_font_id, ctrl_state, (VuiCtrlAttrValue) { .font_id = value })
#define VuiStyle_unset_text_font_id(style, ctrl_state) _VuiStyle_unset_attr(style, VuiCtrlAttr_text_font_id, ctrl_state)

#define VuiStyle_set_text_selection_color(style, ctrl_state, value) _VuiStyle_set_attr(style, VuiCtrlAttr_text_selection_color, ctrl_state, (VuiCtrlAttrValue) { .color = value })
#define VuiStyle_unset_text_selection_color(style, ctrl_state) _VuiStyle_unset_attr(style, VuiCtrlAttr_text_selection_color, ctrl_state)

#define VuiStyle_set_text_selection_radius(style, ctrl_state, value) _VuiStyle_set_attr(style, VuiCtrlAttr_text_selection_radius, ctrl_state, (VuiCtrlAttrValue) { .float_ = value })
#define VuiStyle_unset_text_selection_radius(style, ctrl_state) _VuiStyle_unset_attr(style, VuiCtrlAttr_text_selection_radius, ctrl_state)

#define VuiStyle_set_text_cursor_color(style, ctrl_state, value) _VuiStyle_set_attr(style, VuiCtrlAttr_text_cursor_color, ctrl_state, (VuiCtrlAttrValue) { .color = value })
#define VuiStyle_unset_text_cursor_color(style, ctrl_state) _VuiStyle_unset_attr(style, VuiCtrlAttr_text_cursor_color, ctrl_state)

#define VuiStyle_set_text_cursor_width(style, ctrl_state, value) _VuiStyle_set_attr(style, VuiCtrlAttr_text_cursor_width, ctrl_state, (VuiCtrlAttrValue) { .float_ = value })
#define VuiStyle_unset_text_cursor_width(style, ctrl_state) _VuiStyle_unset_attr(style, VuiCtrlAttr_text_cursor_width, ctrl_state)

#define VuiStyle_set_text_cursor_radius(style, ctrl_state, value) _VuiStyle_set_attr(style, VuiCtrlAttr_text_cursor_radius, ctrl_state, (VuiCtrlAttrValue) { .float_ = value })
#define VuiStyle_unset_text_cursor_radius(style, ctrl_state) _VuiStyle_unset_attr(style, VuiCtrlAttr_text_cursor_radius, ctrl_state)

//
// push and pop attributes for a given control state that override the global style
extern void _vui_push_ctrl_attr(VuiCtrlState ctrl_state, VuiCtrlAttr attr, VuiCtrlAttrValue value);
extern void _vui_pop_ctrl_attr(VuiCtrlState ctrl_state, VuiCtrlAttr attr);

#define vui_push_margin(ctrl_state, value) _vui_push_ctrl_attr(ctrl_state, VuiCtrlAttr_margin, (VuiCtrlAttrValue) { .thickness = value })
#define vui_pop_margin(ctrl_state) _vui_pop_ctrl_attr(ctrl_state, VuiCtrlAttr_margin)
#define vui_scope_margin(ctrl_state, value) _vui_defer_loop(vui_push_margin(ctrl_state, value), vui_pop_margin(ctrl_state))

#define vui_push_padding(ctrl_state, value) _vui_push_ctrl_attr(ctrl_state, VuiCtrlAttr_padding, (VuiCtrlAttrValue) { .thickness = value })
#define vui_pop_padding(ctrl_state) _vui_pop_ctrl_attr(ctrl_state, VuiCtrlAttr_padding)
#define vui_scope_padding(ctrl_state, value) _vui_defer_loop(vui_push_padding(ctrl_state, value), vui_pop_padding(ctrl_state))

#define vui_push_width(ctrl_state, value) _vui_push_ctrl_attr(ctrl_state, VuiCtrlAttr_width, (VuiCtrlAttrValue) { .float_ = value })
#define vui_pop_width(ctrl_state) _vui_pop_ctrl_attr(ctrl_state, VuiCtrlAttr_width)
#define vui_scope_width(ctrl_state, value) _vui_defer_loop(vui_push_width(ctrl_state, value), vui_pop_width(ctrl_state))
#define vui_push_width_ratio(ctrl_state, value) vui_push_width(ctrl_state, -(value))
#define vui_pop_width_ratio(ctrl_state) vui_pop_width(ctrl_state, )
#define vui_scope_width_ratio(ctrl_state, value) _vui_defer_loop(vui_push_width_ratio(ctrl_state, value), vui_pop_width(ctrl_state))

#define vui_push_width_min(ctrl_state, value) _vui_push_ctrl_attr(ctrl_state, VuiCtrlAttr_width_min, (VuiCtrlAttrValue) { .float_ = value })
#define vui_pop_width_min(ctrl_state) _vui_pop_ctrl_attr(ctrl_state, VuiCtrlAttr_width_min)
#define vui_scope_width_min(ctrl_state, value) _vui_defer_loop(vui_push_width_min(ctrl_state, value), vui_pop_width_min(ctrl_state))
#define vui_push_width_min_ratio(ctrl_state, value) vui_push_width_min(ctrl_state, -(value))
#define vui_pop_width_min_ratio(ctrl_state) vui_pop_width_min(ctrl_state, )
#define vui_scope_width_min_ratio(ctrl_state, value) _vui_defer_loop(vui_push_width_min_ratio(ctrl_state, value), vui_pop_width_min(ctrl_state))

#define vui_push_width_max(ctrl_state, value) _vui_push_ctrl_attr(ctrl_state, VuiCtrlAttr_width_max, (VuiCtrlAttrValue) { .float_ = value })
#define vui_pop_width_max(ctrl_state) _vui_pop_ctrl_attr(ctrl_state, VuiCtrlAttr_width_max)
#define vui_scope_width_max(ctrl_state, value) _vui_defer_loop(vui_push_width_max(ctrl_state, value), vui_pop_width_max(ctrl_state))
#define vui_push_width_max_ratio(ctrl_state, value) vui_push_width_max(ctrl_state, -(value))
#define vui_pop_width_max_ratio(ctrl_state) vui_pop_width_max(ctrl_state, )
#define vui_scope_width_max_ratio(ctrl_state, value) _vui_defer_loop(vui_push_width_max_ratio(ctrl_state, value), vui_pop_width_max(ctrl_state))

#define vui_push_height(ctrl_state, value) _vui_push_ctrl_attr(ctrl_state, VuiCtrlAttr_height, (VuiCtrlAttrValue) { .float_ = value })
#define vui_pop_height(ctrl_state) _vui_pop_ctrl_attr(ctrl_state, VuiCtrlAttr_height)
#define vui_scope_height(ctrl_state, value) _vui_defer_loop(vui_push_height(ctrl_state, value), vui_pop_height(ctrl_state))
#define vui_push_height_ratio(ctrl_state, value) vui_push_height(ctrl_state, -(value))
#define vui_pop_height_ratio(ctrl_state) vui_pop_height(ctrl_state, )
#define vui_scope_height_ratio(ctrl_state, value) _vui_defer_loop(vui_push_height_ratio(ctrl_state, value), vui_pop_height(ctrl_state))

#define vui_push_height_min(ctrl_state, value) _vui_push_ctrl_attr(ctrl_state, VuiCtrlAttr_height_min, (VuiCtrlAttrValue) { .float_ = value })
#define vui_pop_height_min(ctrl_state) _vui_pop_ctrl_attr(ctrl_state, VuiCtrlAttr_height_min)
#define vui_scope_height_min(ctrl_state, value) _vui_defer_loop(vui_push_height_min(ctrl_state, value), vui_pop_height_min(ctrl_state))
#define vui_push_height_min_ratio(ctrl_state, value) vui_push_height_min(ctrl_state, -(value))
#define vui_pop_height_min_ratio(ctrl_state) vui_pop_height_min(ctrl_state, )
#define vui_scope_height_min_ratio(ctrl_state, value) _vui_defer_loop(vui_push_height_min_ratio(ctrl_state, value), vui_pop_height_min(ctrl_state))

#define vui_push_height_max(ctrl_state, value) _vui_push_ctrl_attr(ctrl_state, VuiCtrlAttr_height_max, (VuiCtrlAttrValue) { .float_ = value })
#define vui_pop_height_max(ctrl_state) _vui_pop_ctrl_attr(ctrl_state, VuiCtrlAttr_height_max)
#define vui_scope_height_max(ctrl_state, value) _vui_defer_loop(vui_push_height_max(ctrl_state, value), vui_pop_height_max(ctrl_state))
#define vui_push_height_max_ratio(ctrl_state, value) vui_push_height_max(ctrl_state, -(value))
#define vui_pop_height_max_ratio(ctrl_state) vui_pop_height_max(ctrl_state, )
#define vui_scope_height_max_ratio(ctrl_state, value) _vui_defer_loop(vui_push_height_max_ratio(ctrl_state, value), vui_pop_height_max(ctrl_state))

#define vui_push_offset(ctrl_state, x, y) _vui_push_ctrl_attr(ctrl_state, VuiCtrlAttr_offset, (VuiCtrlAttrValue) { .vec2 = VuiVec2_init(x, y) })
#define vui_pop_offset(ctrl_state) _vui_pop_ctrl_attr(ctrl_state, VuiCtrlAttr_offset)
#define vui_scope_offset(ctrl_state, x, y) _vui_defer_loop(vui_push_offset(ctrl_state, x, y), vui_pop_offset(ctrl_state))

#define vui_push_align(ctrl_state, value) _vui_push_ctrl_attr(ctrl_state, VuiCtrlAttr_align, (VuiCtrlAttrValue) { .align = value })
#define vui_pop_align(ctrl_state) _vui_pop_ctrl_attr(ctrl_state, VuiCtrlAttr_align)
#define vui_scope_align(ctrl_state, value) _vui_defer_loop(vui_push_align(ctrl_state, value), vui_pop_align(ctrl_state))

#define vui_push_layout_spacing(ctrl_state, value) _vui_push_ctrl_attr(ctrl_state, VuiCtrlAttr_layout_spacing, (VuiCtrlAttrValue) { .float_ = value })
#define vui_pop_layout_spacing(ctrl_state) _vui_pop_ctrl_attr(ctrl_state, VuiCtrlAttr_layout_spacing)
#define vui_scope_layout_spacing(ctrl_state, value) _vui_defer_loop(vui_push_layout_spacing(ctrl_state, value), vui_pop_layout_spacing(ctrl_state))

#define vui_push_layout_wrap_spacing(ctrl_state, value) _vui_push_ctrl_attr(ctrl_state, VuiCtrlAttr_layout_wrap_spacing, (VuiCtrlAttrValue) { .float_ = value })
#define vui_pop_layout_wrap_spacing(ctrl_state) _vui_pop_ctrl_attr(ctrl_state, VuiCtrlAttr_layout_wrap_spacing)
#define vui_scope_layout_wrap_spacing(ctrl_state, value) _vui_defer_loop(vui_push_layout_wrap_spacing(ctrl_state, value), vui_pop_layout_wrap_spacing(ctrl_state))

#define vui_push_layout_wrap(ctrl_state, value) _vui_push_ctrl_attr(ctrl_state, VuiCtrlAttr_layout_wrap, (VuiCtrlAttrValue) { .bool_ = value })
#define vui_pop_layout_wrap(ctrl_state) _vui_pop_ctrl_attr(ctrl_state, VuiCtrlAttr_layout_wrap)
#define vui_scope_layout_wrap(ctrl_state, value) _vui_defer_loop(vui_push_layout_wrap(ctrl_state, value), vui_pop_layout_wrap(ctrl_state))

#define vui_push_image_scale_mode(ctrl_state, value) _vui_push_ctrl_attr(ctrl_state, VuiCtrlAttr_image_scale_mode, (VuiCtrlAttrValue) { .image_scale_mode = value })
#define vui_pop_image_scale_mode(ctrl_state) _vui_pop_ctrl_attr(ctrl_state, VuiCtrlAttr_image_scale_mode)
#define vui_scope_image_scale_mode(ctrl_state, value) _vui_defer_loop(vui_push_image_scale_mode(ctrl_state, value), vui_pop_image_scale_mode(ctrl_state))

typedef uint64_t VuiCtrlFlags;
enum {
	VuiCtrlFlags_focusable = 0x1,
	VuiCtrlFlags_scrollable = 0x2,
	VuiCtrlFlags_background = 0x4,
	VuiCtrlFlags_border = 0x8,
	VuiCtrlFlags_pressable = 0x10,
	VuiCtrlFlags_toggleable = 0x20,
	_VuiCtrlFlags_image = 0x40,
	_VuiCtrlFlags_text = 0x80,
};

typedef uint8_t VuiLayoutType;
enum {
	VuiLayoutType_container,
	VuiLayoutType_stack,
	VuiLayoutType_row,
	VuiLayoutType_column,
};
extern char* VuiLayoutType_strings[];

typedef void (*VuiCtrlRenderFn)();
typedef struct VuiCtrl VuiCtrl;
struct VuiCtrl {
	VuiCtrlId id;
	VuiCtrlId parent_id;
	VuiCtrlId child_first_id;
	VuiCtrlId child_last_id;
	VuiCtrlId sibling_prev_id;
	VuiCtrlId sibling_next_id;
	VuiCtrlHash hash;
	uint32_t last_frame_idx;
	VuiRect rect;
	VuiCtrlStateFlags state_flags;
    VuiLayoutType layout_type;
	VuiFocusState focus_state;
	VuiCtrlFlags flags;
	VuiCtrlRenderFn render_fn;
	union {
		struct {
			VuiImageId image_id;
			VuiColor image_tint;
		};
		struct {
			uint32_t text_start_idx;
			uint32_t text_length;
			uint32_t text_wrap_width;
		};
	};
	VuiCtrlAttrValue attributes[VuiCtrlAttr_COUNT];
};

#define vui_color_white VuiColor_init(0xfa, 0xfa, 0xfa, 0xff)
#define vui_color_gray VuiColor_init(0xbd, 0xbd, 0xbd, 0xff)
#define vui_color_dark_gray VuiColor_init(0x46, 0x46, 0x46, 0xff)
#define vui_color_very_dark_grayish_blue VuiColor_init(0x32, 0x39, 0x3d, 0xff)
#define vui_color_very_dark_gray VuiColor_init(0x37, 0x37, 0x37, 0xff)
#define vui_color_black VuiColor_init(0x2d, 0x2d, 0x2d, 0xff)

#define vui_color_alizarin VuiColor_init(0xe7, 0x4c, 0x3c, 0xff)
#define vui_color_pomegranate VuiColor_init(0xc0, 0x39, 0x2b, 0xff)
#define vui_color_carrot VuiColor_init(0xe6, 0x7e, 0x22, 0xff)
#define vui_color_pumpkin VuiColor_init(0xd3, 0x54, 0x00, 0xff)
#define vui_color_sun_flower VuiColor_init(0xf1, 0xc4, 0x0f, 0xff)
#define vui_color_orange VuiColor_init(0xf3, 0x9c, 0x12, 0xff)
#define vui_color_emerald VuiColor_init(0x2e, 0xcc, 0x71, 0xff)
#define vui_color_nephritis VuiColor_init(0x27, 0xae, 0x60, 0xff)
#define vui_color_turquoise VuiColor_init(0x1a, 0xbc, 0x9c , 0xff)
#define vui_color_green_sea VuiColor_init(0x16, 0xa0, 0x85, 0xff)
#define vui_color_peter_river VuiColor_init(0x34, 0x98, 0xdb, 0xff)
#define vui_color_belize_hole VuiColor_init(0x29, 0x80, 0xb9, 0xff)
#define vui_color_amethyst VuiColor_init(0x9b, 0x59, 0xb6, 0xff)
#define vui_color_wisteria VuiColor_init(0x8e, 0x44, 0xad, 0xff)
#define vui_color_clouds VuiColor_init(0xec, 0xf0, 0xf1, 0xff)
#define vui_color_silver VuiColor_init(0xbd, 0xc3, 0xc7, 0xff)
#define vui_color_concrete VuiColor_init(0x95, 0xa5, 0xa6, 0xff)
#define vui_color_asbestos VuiColor_init(0x7f, 0x8c, 0x8d, 0xff)
#define vui_color_wet_asphalt VuiColor_init(0x34, 0x49, 0x5e, 0xff)
#define vui_color_midnight_blue VuiColor_init(0x2c, 0x3e, 0x50, 0xff)

void VuiStyle_init_default(VuiStyle* style);

// ===========================================================================================
//
//
// Render
//
//
// ===========================================================================================

#ifndef VuiTextureId
#define VuiTextureId uint32_t
#endif

typedef struct {
	VuiVec2 pos;
	VuiVec2 uv;
	VuiColor color;
} VuiVertex;

typedef struct {
	VuiTextureId texture_id;
	uint32_t verts_start_idx;
	uint32_t indices_start_idx;
	uint32_t indices_count;
} VuiRenderCmd;

#ifndef VuiVertexIdxT
#define VuiVertexIdxT uint32_t
#endif
typedef VuiVertexIdxT VuiVertexIdx;

typedef struct {
	VuiStk(VuiRenderCmd) cmds;
    VuiStk(VuiVertex) verts;
	VuiStk(VuiVertexIdx) indices;
    uint64_t hash;
} VuiWindowRender;

void vui_render_line(VuiVec2 start_pos, VuiVec2 end_pos, VuiColor color, float width);
void vui_render_rect(const VuiRect* rect, VuiColor color, float radius);
void vui_render_rect_border(const VuiRect* rect, VuiColor color, float radius, float width);
void vui_render_triangle(VuiVec2 a, VuiVec2 b, VuiVec2 c, VuiColor color);
void vui_render_triangle_border(VuiVec2 a, VuiVec2 b, VuiVec2 c, VuiColor color, float width);
void vui_render_circle(VuiVec2 pos, float radius, VuiColor color);
void vui_render_circle_border(VuiVec2 pos, float radius, VuiColor color, float width);
void vui_render_text(VuiVec2 pos, VuiFontId font_id, char* text, uint32_t text_length, VuiColor color, float wrap_at_width);
void vui_render_polyline(VuiVec2* points, uint32_t points_count, VuiColor color, float width, VuiBool connect_first_and_last);
void vui_render_convex_polygon(VuiVec2* points, uint32_t points_count, VuiColor color);
void vui_render_bezier_curve(VuiVec2 start_pos, VuiVec2 end_pos, VuiVec2 start_anchor_pos, VuiVec2 end_anchor_pos, VuiColor color, float width);

void vui_render_image(const VuiRect* rect, VuiImageId image_id, VuiColor image_tint, VuiImageScaleMode scale_mode);
void vui_render_image_(const VuiRect* rect, float image_width, float image_height, VuiTextureId texture_id, VuiRect uv_rect, VuiColor color, VuiImageScaleMode scale_mode, VuiBool is_glyph);

void vui_path_reset();
void vui_path_plot_point(VuiVec2 pt);
void vui_path_plot_arc(VuiVec2 pt, float radius, float angle_start, float angle_end, uint32_t segments_count);
void vui_path_plot_bezier_curve(VuiVec2 start_control_pt, VuiVec2 end_control_pt, VuiVec2 end_pt);
void vui_path_plot_rect(const VuiRect* rect, float radius);
void vui_path_plot_circle(VuiVec2 pt, float radius, VuiBool is_border);
void vui_render_path_stroked(VuiColor color, float width, VuiBool connect_first_and_last);
void vui_render_path_filled_convex(VuiColor color);

typedef struct {
	VuiVertex* verts;
	VuiVertexIdx* indices;
	uint32_t verts_start_idx;
} VuiRenderWriter;

VuiRenderWriter vui_render_get_writer(VuiTextureId texture_id, uint32_t verts_count, uint32_t indices_count);
void vui_render_inc_layer();
void vui_render_dec_layer();

// ====================================================================================
//
//
// Controls: the immediate mode API
//
//
// ====================================================================================

//
// this must be an enum.
// the enum count must be passed into the vui_init
typedef uint16_t VuiWindowId;

void vui_container_layout();
void vui_stack_layout();
void vui_column_layout();
void vui_row_layout();

typedef uint8_t VuiActiveChange;
enum {
	VuiActiveChange_none = 0,
	VuiActiveChange_to_inactive = vui_false + 1,
	VuiActiveChange_to_active = vui_true + 1,
};

VuiCtrl* vui_ctrl_get(VuiCtrlId ctrl_id);
void vui_ctrl_start(VuiCtrlSibId sib_id, VuiCtrlFlags flags, VuiActiveChange active_change);
void vui_ctrl_end();

void vui_box_start(VuiCtrlSibId sib_id);
void vui_box_end();
#define vui_scope_box(sib_id) _vui_defer_loop(vui_box_start(sib_id), vui_box_end())

// wrap_at_width = 0.0 to not have word wrapping
#define vui_text(sib_id, text, wrap_at_width) vui_text_(sib_id, text, strlen(text), wrap_at_width)
void vui_text_(VuiCtrlSibId sib_id, char* text, uint32_t text_length, float wrap_at_width);

void vui_image(VuiCtrlSibId sib_id, VuiImageId image_id, VuiColor image_tint);

void vui_spacing(VuiCtrlSibId sib_id, float width, float height);
void vui_separator(VuiCtrlSibId sib_id);

VuiFocusState vui_button_start(VuiCtrlSibId sib_id);
void vui_button_end();

#define vui_text_button(sib_id, text) vui_text_button_(sib_id, text, strlen(text))
VuiFocusState vui_text_button_(VuiCtrlSibId sib_id, char* text, uint32_t text_length);
VuiFocusState vui_image_button(VuiCtrlSibId sib_id, VuiImageId image_id, VuiColor image_tint);

VuiBool vui_toggle_button_start(VuiCtrlSibId sib_id, VuiBool* pressed);
void vui_toggle_button_end();
#define vui_text_toggle_button(sib_id, pressed, text) vui_text_toggle_button_(sib_id, pressed, text, strlen(text))
VuiBool vui_text_toggle_button_(VuiCtrlSibId sib_id, VuiBool* pressed, char* text, uint32_t text_length);
VuiBool vui_image_toggle_button(VuiCtrlSibId sib_id, VuiBool* pressed, VuiImageId image_id, VuiColor image_tint);

VuiBool vui_select_button_start(VuiCtrlSibId sib_id, VuiCtrlSibId* selected_sib_id);
void vui_select_button_end();
#define vui_text_select_button(sib_id, selected_sib_id, text) vui_text_select_button_(sib_id, selected_sib_id, text, strlen(text))
VuiBool vui_text_select_button_(VuiCtrlSibId sib_id, VuiCtrlSibId* selected_sib_id, char* text, uint32_t text_length);
VuiBool vui_image_select_button(VuiCtrlSibId sib_id, VuiCtrlSibId* selected_sib_id, VuiImageId image_id, VuiColor image_tint);
#define vui_image_text_select_button(sib_id, selected_sib_id, text, image_id, image_tint) \
	vui_image_text_select_button_(sib_id, selected_sib_id, text, strlen(text), image_id, image_tint)
VuiBool vui_image_text_select_button_(VuiCtrlSibId sib_id, VuiCtrlSibId* selected_sib_id, char* text, uint32_t text_length, VuiImageId image_id, VuiColor image_tint);

VuiBool vui_check_box(VuiCtrlSibId sib_id, VuiBool* checked);
#define vui_text_check_box(sib_id, checked, text) vui_text_check_box_(sib_id, checked, text, strlen(text))
VuiBool vui_text_check_box_(VuiCtrlSibId sib_id, VuiBool* checked, char* text, uint32_t text_length);
VuiBool vui_image_check_box(VuiCtrlSibId sib_id, VuiBool* checked, VuiImageId image_id, VuiColor image_tint);

VuiBool vui_radio_button(VuiCtrlSibId sib_id, VuiCtrlSibId* selected_sib_id);
#define vui_text_radio_button(sib_id, selected_sib_id, text) vui_text_radio_button_(sib_id, selected_sib_id, text, strlen(text))
VuiBool vui_text_radio_button_(VuiCtrlSibId sib_id, VuiCtrlSibId* selected_sib_id, char* text, uint32_t text_length);
VuiBool vui_image_radio_button(VuiCtrlSibId sib_id, VuiCtrlSibId* selected_sib_id, VuiImageId image_id, VuiColor image_tint);

/*
void vui_progress_bar(VuiVec2 size, float value, float min, float max, VuiProgressBarStyle* style);

void vui_scroll_bar(VuiCtrlSibId sib_id, float length, float* content_offset, float content_length, VuiBool is_horizontal, VuiScrollBarStyle* style);
void vui_scroll_view_start(VuiCtrlSibId sib_id, VuiVec2* size, VuiVec2* content_offset, VuiScrollViewFlags flags, VuiScrollViewStyle* style);
void vui_scroll_view_end();
*/

VuiBool vui_text_box(VuiCtrlSibId sib_id, char* string_in_out, uint32_t string_in_out_cap);
VuiBool vui_input_box_uint(VuiCtrlSibId sib_id, uint32_t* value);
VuiBool vui_input_box_sint(VuiCtrlSibId sib_id, int32_t* value);
VuiBool vui_input_box_float(VuiCtrlSibId sib_id, float* value);

/*

VuiBool vui_drag_button(Vui* vui, VuiCtrlSibId sib_id, VuiVec2 size, VuiButtonStyle* style);
void vui_drag_button_start(Vui* vui, VuiCtrlSibId sib_id, VuiVec2 size, VuiButtonStyle* style);
// returns true when being held or has been pressed on this frame
VuiBool vui_drag_button_end(Vui* vui);
*/

// ===========================================================================================
//
//
// Vui Public API
//
//
// ===========================================================================================

typedef struct {
	float width;
	float height;
	VuiTextureId texture_id;
	VuiRect uv_rect;
} VuiImage;

VuiImageId vui_image_add(VuiImage* image);
VuiImage* vui_image_get(VuiImageId image_id);
void vui_image_remove(VuiImageId image_id);

typedef uint16_t VuiViewportId;

typedef void (*VuiRenderGlyphFn)(const VuiRect* rect, VuiTextureId glyph_texture_id, const VuiRect* uv_rect);
typedef VuiVec2 (*VuiPositionTextFn)(void* userdata, VuiFontId font_id, char* text, uint32_t text_length, VuiVec2 top_left, VuiRenderGlyphFn render_glyph_fn);
typedef void (*VuiTextBoxFocusChange)(VuiBool focused);

typedef struct {
	VuiPositionTextFn position_text_fn;
	void* position_text_userdata;
	VuiTextBoxFocusChange text_box_focus_change_fn;
	uint16_t windows_count;
	void* allocator;
	VuiStyle* style;
} VuiSetup;

VuiBool vui_init(VuiSetup* setup);

void vui_frame_start(VuiBool right_to_left);
void vui_frame_end();

void vui_window_start(VuiWindowId id, VuiVec2 size);
void vui_window_end();

VuiWindowRender* vui_window_render(VuiWindowId id, float scale_factor, VuiBool pixel_snapping);
void vui_window_set_mouse_focused(VuiWindowId id);
void vui_window_set_focused(VuiWindowId id);

void vui_window_dump_render(VuiWindowId id, FILE* file);

// allocate zeroed memory that is cleared at the end of the frame.
// useful if you need to make things like styles for child controls.
#define vui_frame_data_alloc_elmt(T) (T*)vui_frame_data_alloc(sizeof(T), alignof(T));
void* vui_frame_data_alloc(uint32_t size, uint32_t align);

#endif // VUI_H

