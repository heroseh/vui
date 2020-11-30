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
#define VUI_DEBUG_CTRL_LAYOUT 1

#define vui_debug_ctrl_layout_dump_file_path "/tmp/vui_ctrls"

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

#define vui_auto_len INFINITY
#define vui_fill_len (-INFINITY)

//
// use this macro to get a unique sibling identifier based on the line number.
// if the function is used to define a new control, then get the identifier to be based in as a parameter.
#define vui_sib_id __LINE__

static inline float vui_min(float a, float b) { return a < b ? a : b; }
static inline float vui_max(float a, float b) { return a > b ? a : b; }
static inline float vui_clamp(float v, float min, float max) { return v < min ? min : (v > max ? max : v); }
static inline float vui_lerp(float to, float from, float t) { return (to - from) * t + from; }

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
VuiBool _VuiStk_resize_cap(void** stk_ptr, uint32_t new_cap, uint32_t elmt_size);
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
// @return: on allocation failure vui_false is returned, otherwise returns vui_true
//
// @example:
// VuiStk_resize_cap(&stk, 24);
//
#define VuiStk_resize_cap(stk_ptr, new_cap) _VuiStk_resize_cap((void**)stk_ptr, new_cap, sizeof(**(stk_ptr)))

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
// remove an element at @param(idx) by shifting the ones to it's right to the left.
// this retains the order of elements but comes at a cost of copying all the elements to it's right.
//
#define VuiStk_remove_shift(stk, idx) VuiStk_remove_range_shift(stk, idx, (idx) + 1)

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
#define VuiStk_remove_range_shift(stk, start_idx, end_idx) _VuiStk_remove_range_shift(stk, start_idx, end_idx, sizeof(*(stk)))

//
// deallocates the stack and returns NULL.
//
// @example:
// stk = VuiStk_deinit(stk);
//
#define VuiStk_deinit(stk) ((stk) ? vui_mem_dealloc(_vui.allocator, _VuiStk_header(stk), VuiStk_size(stk) + sizeof(_VuiStkHeader), alignof(void*)), NULL : NULL)

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
#define VuiVec2_auto (VuiVec2){vui_auto_len, vui_auto_len}
#define VuiVec2_fill (VuiVec2){vui_fill_len, vui_fill_len}
static inline VuiVec2 VuiVec2_add(VuiVec2 a, VuiVec2 b) { return VuiVec2_init(a.x + b.x, a.y + b.y); }
static inline VuiVec2 VuiVec2_sub(VuiVec2 a, VuiVec2 b) { return VuiVec2_init(a.x - b.x, a.y - b.y); }
static inline VuiVec2 VuiVec2_mul(VuiVec2 a, VuiVec2 b) { return VuiVec2_init(a.x * b.x, a.y * b.y); }
static inline VuiVec2 VuiVec2_div(VuiVec2 a, VuiVec2 b) { return VuiVec2_init(a.x / b.x, a.y / b.y); }
static inline VuiVec2 VuiVec2_neg(VuiVec2 v) { return VuiVec2_init(-v.x, -v.y); }
static inline float VuiVec2_len(VuiVec2 v) { return sqrtf((v.x * v.x) + (v.y * v.y)); }
static inline VuiVec2 VuiVec2_scale(VuiVec2 v, float by) { return VuiVec2_init(v.x * by, v.y * by); }
static inline VuiVec2 VuiVec2_norm(VuiVec2 v) {
	float d = sqrtf((v.x * v.x) + (v.y * v.y));
	float k = d == 0.0 ? 0.0 : 1.0 / d;
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
#define VuiColor_white ((VuiColor){0xff, 0xff, 0xff, 0xff})
#define VuiColor_black ((VuiColor){0, 0, 0, 0xff})
#define VuiColor_transparent ((VuiColor){0})
static inline VuiColor VuiColor_lerp(VuiColor to, VuiColor from, float interp_ratio) {
	return VuiColor_init(
		vui_lerp(to.r, from.r, interp_ratio),
		vui_lerp(to.g, from.g, interp_ratio),
		vui_lerp(to.b, from.b, interp_ratio),
		vui_lerp(to.a, from.a, interp_ratio)
	);
}

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
#define VuiThickness_init(left_, top_, right_, bottom_) ((VuiVec4){.left = left_, .top = top_, .right = right_, .bottom = bottom_})
#define VuiThickness_init_even(size) ((VuiVec4){.left=size, .top=size, .right=size, .bottom=size})
#define VuiThickness_hv_init(horizontal, vertical) ((VuiVec4){.left=horizontal, .top=vertical, .right=horizontal, .bottom=vertical})
#define VuiThickness_hv(thickness) VuiVec2_init(((thickness).left + (thickness).right), (thickness).top + (thickness).bottom)
static inline float VuiThickness_horizontal(const VuiThickness* thickness) { return thickness->left + thickness->right; }
static inline float VuiThickness_vertical(const VuiThickness* thickness) { return thickness->top + thickness->bottom; }
static inline void VuiThickness_lerp(VuiThickness* result, const VuiThickness* to, const VuiThickness* from, float interp_ratio) {
	result->left = vui_lerp(to->left, from->left, interp_ratio);
	result->top = vui_lerp(to->top, from->top, interp_ratio);
	result->right = vui_lerp(to->right, from->right, interp_ratio);
	result->bottom = vui_lerp(to->bottom, from->bottom, interp_ratio);
}
#define VuiRect_zero ((VuiVec4){0})
#define VuiRect_init(left_, top_, right_, bottom_) ((VuiVec4){.left = left_, .top = top_, .right = right_, .bottom = bottom_})
#define VuiRect_init_wh(left_, top_, width, height) ((VuiVec4){.left = left_, .top = top_, .right = (left_) + (width), .bottom = (top_) + (height)})
#define VuiRect_init_v2(left_top_, right_bottom_) ((VuiVec4){.left_top = left_top_, .right_bottom = right_bottom_})

#define VuiRect_left_bottom(rect) VuiVec2_init((rect).left, (rect).bottom)
#define VuiRect_right_top(rect) VuiVec2_init((rect).right, (rect).top)
#define VuiRect_size(rect) VuiVec2_init((rect).right - (rect).left, (rect).bottom - (rect).top)
static inline float VuiRect_width(const VuiRect* rect) { return rect->right - rect->left; }
static inline float VuiRect_neg_width(const VuiRect* rect) { return rect->left - rect->right; }
static inline float VuiRect_height(const VuiRect* rect) { return rect->bottom - rect->top; }
VuiRect VuiRect_clip(const VuiRect* a, const VuiRect* b);
VuiVec2 VuiRect_clip_pt(const VuiRect* rect, VuiVec2 pt);
VuiBool VuiRect_intersects(const VuiRect* a, const VuiRect* b);
VuiBool VuiRect_intersects_pt(const VuiRect* r, VuiVec2 pt);

typedef uint32_t VuiFontId;
typedef uint32_t VuiImageId;
typedef uint32_t VuiCtrlId;
typedef uint32_t VuiCtrlSibId;

typedef enum {
    VuiFocusState_none = 0x0,
    // signals that the ctrl is mouse or keyboard focused
    VuiFocusState_focused = 0x1,
    // signals has been pressed this frame
    VuiFocusState_pressed = 0x2,
    // signals has been double pressed this frame
    VuiFocusState_double_pressed = 0x4,
    // signals is being held this frame
    VuiFocusState_held = 0x8,
    // signals has been released this frame
    VuiFocusState_released = 0x10,
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
void vui_input_add_text(const char* string, uint32_t string_length);

VuiBool vui_input_is_mouse_over_ctrl();
VuiBool vui_input_is_mouse_scroll_focused_ctrl();
VuiBool vui_input_is_mouse_focused_ctrl();

VuiBool vui_ctrl_is_mouse_focused(VuiCtrlId ctrl_id);
VuiBool vui_ctrl_is_focused(VuiCtrlId ctrl_id);
VuiBool vui_ctrl_is_mouse_scroll_focused(VuiCtrlId ctrl_id);
void vui_ctrl_set_focused(VuiCtrlId ctrl_id);

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

typedef struct VuiCtrlAttrs VuiCtrlAttrs;
struct VuiCtrlAttrs {
	float width;
	float width_min;
	float width_max;
	float height;
	float height_min;
	float height_max;
	float layout_spacing;
	float layout_wrap_spacing;
	VuiBool layout_wrap;
	VuiVec2 offset;
	VuiAlign align;
	VuiImageScaleMode image_scale_mode;
};

typedef uint8_t VuiCtrlAttr;
enum {
	VuiCtrlAttr_width,
	VuiCtrlAttr_width_min,
	VuiCtrlAttr_width_max,
	VuiCtrlAttr_height,
	VuiCtrlAttr_height_min,
	VuiCtrlAttr_height_max,
	VuiCtrlAttr_offset,
	VuiCtrlAttr_align,
	VuiCtrlAttr_layout_spacing,
	VuiCtrlAttr_layout_wrap_spacing,
	VuiCtrlAttr_layout_wrap,
    VuiCtrlAttr_image_scale_mode,
    VuiCtrlAttr_COUNT,
};

typedef union VuiCtrlAttrValue VuiCtrlAttrValue;
union VuiCtrlAttrValue {
	float float_;
	VuiVec2 vec2;
	VuiAlign align;
	VuiBool bool_;
	VuiImageScaleMode image_scale_mode;
};

typedef uint8_t VuiCtrlState;
enum {
	VuiCtrlState_default,
	VuiCtrlState_focused,
	VuiCtrlState_active,
	VuiCtrlState_disabled,
	VuiCtrlState_COUNT,
};

typedef uint8_t VuiCtrlStateFlags;
enum {
	VuiCtrlStateFlags_default = 1 << VuiCtrlState_default,
	VuiCtrlStateFlags_focused = 1 << VuiCtrlState_focused,
	VuiCtrlStateFlags_active = 1 << VuiCtrlState_active,
	VuiCtrlStateFlags_disabled = 1 << VuiCtrlState_disabled,
};

//
// push and pop attributes for a given control state that override the global style
extern void _vui_push_ctrl_attr(VuiCtrlAttr attr, VuiCtrlAttrValue value);
extern void _vui_pop_ctrl_attr(VuiCtrlAttr attr);

#define vui_push_width(value) _vui_push_ctrl_attr(VuiCtrlAttr_width, (VuiCtrlAttrValue) { .float_ = value })
#define vui_pop_width() _vui_pop_ctrl_attr(VuiCtrlAttr_width)
#define vui_scope_width(value) _vui_defer_loop(vui_push_width(value), vui_pop_width())
#define vui_push_width_ratio(value) vui_push_width(-(value))
#define vui_pop_width_ratio() vui_pop_width()
#define vui_scope_width_ratio(value) _vui_defer_loop(vui_push_width_ratio(value), vui_pop_width())

#define vui_push_width_min(value) _vui_push_ctrl_attr(VuiCtrlAttr_width_min, (VuiCtrlAttrValue) { .float_ = value })
#define vui_pop_width_min() _vui_pop_ctrl_attr(VuiCtrlAttr_width_min)
#define vui_scope_width_min(value) _vui_defer_loop(vui_push_width_min(value), vui_pop_width_min())
#define vui_push_width_min_ratio(value) vui_push_width_min(-(value))
#define vui_pop_width_min_ratio() vui_pop_width_min()
#define vui_scope_width_min_ratio(value) _vui_defer_loop(vui_push_width_min_ratio(value), vui_pop_width_min())

#define vui_push_width_max(value) _vui_push_ctrl_attr(VuiCtrlAttr_width_max, (VuiCtrlAttrValue) { .float_ = value })
#define vui_pop_width_max() _vui_pop_ctrl_attr(VuiCtrlAttr_width_max)
#define vui_scope_width_max(value) _vui_defer_loop(vui_push_width_max(value), vui_pop_width_max())
#define vui_push_width_max_ratio(value) vui_push_width_max(-(value))
#define vui_pop_width_max_ratio() vui_pop_width_max()
#define vui_scope_width_max_ratio(value) _vui_defer_loop(vui_push_width_max_ratio(value), vui_pop_width_max())

#define vui_push_height(value) _vui_push_ctrl_attr(VuiCtrlAttr_height, (VuiCtrlAttrValue) { .float_ = value })
#define vui_pop_height() _vui_pop_ctrl_attr(VuiCtrlAttr_height)
#define vui_scope_height(value) _vui_defer_loop(vui_push_height(value), vui_pop_height())
#define vui_push_height_ratio(value) vui_push_height(-(value))
#define vui_pop_height_ratio() vui_pop_height()
#define vui_scope_height_ratio(value) _vui_defer_loop(vui_push_height_ratio(value), vui_pop_height())

#define vui_push_height_min(value) _vui_push_ctrl_attr(VuiCtrlAttr_height_min, (VuiCtrlAttrValue) { .float_ = value })
#define vui_pop_height_min() _vui_pop_ctrl_attr(VuiCtrlAttr_height_min)
#define vui_scope_height_min(value) _vui_defer_loop(vui_push_height_min(value), vui_pop_height_min())
#define vui_push_height_min_ratio(value) vui_push_height_min(-(value))
#define vui_pop_height_min_ratio() vui_pop_height_min()
#define vui_scope_height_min_ratio(value) _vui_defer_loop(vui_push_height_min_ratio(value), vui_pop_height_min())

#define vui_push_height_max(value) _vui_push_ctrl_attr(VuiCtrlAttr_height_max, (VuiCtrlAttrValue) { .float_ = value })
#define vui_pop_height_max() _vui_pop_ctrl_attr(VuiCtrlAttr_height_max)
#define vui_scope_height_max(value) _vui_defer_loop(vui_push_height_max(value), vui_pop_height_max())
#define vui_push_height_max_ratio(value) vui_push_height_max(-(value))
#define vui_pop_height_max_ratio() vui_pop_height_max()
#define vui_scope_height_max_ratio(value) _vui_defer_loop(vui_push_height_max_ratio(value), vui_pop_height_max())

#define vui_push_offset(x, y) _vui_push_ctrl_attr(VuiCtrlAttr_offset, (VuiCtrlAttrValue) { .vec2 = VuiVec2_init(x, y) })
#define vui_pop_offset() _vui_pop_ctrl_attr(VuiCtrlAttr_offset)
#define vui_scope_offset(x, y) _vui_defer_loop(vui_push_offset(x, y), vui_pop_offset())

#define vui_push_align(value) _vui_push_ctrl_attr(VuiCtrlAttr_align, (VuiCtrlAttrValue) { .align = value })
#define vui_pop_align() _vui_pop_ctrl_attr(VuiCtrlAttr_align)
#define vui_scope_align(value) _vui_defer_loop(vui_push_align(value), vui_pop_align())

#define vui_push_layout_spacing(value) _vui_push_ctrl_attr(VuiCtrlAttr_layout_spacing, (VuiCtrlAttrValue) { .float_ = value })
#define vui_pop_layout_spacing() _vui_pop_ctrl_attr(VuiCtrlAttr_layout_spacing)
#define vui_scope_layout_spacing(value) _vui_defer_loop(vui_push_layout_spacing(value), vui_pop_layout_spacing())

#define vui_push_layout_wrap_spacing(value) _vui_push_ctrl_attr(VuiCtrlAttr_layout_wrap_spacing, (VuiCtrlAttrValue) { .float_ = value })
#define vui_pop_layout_wrap_spacing() _vui_pop_ctrl_attr(VuiCtrlAttr_layout_wrap_spacing)
#define vui_scope_layout_wrap_spacing(value) _vui_defer_loop(vui_push_layout_wrap_spacing(value), vui_pop_layout_wrap_spacing())

#define vui_push_layout_wrap(value) _vui_push_ctrl_attr(VuiCtrlAttr_layout_wrap, (VuiCtrlAttrValue) { .bool_ = value })
#define vui_pop_layout_wrap() _vui_pop_ctrl_attr(VuiCtrlAttr_layout_wrap)
#define vui_scope_layout_wrap(value) _vui_defer_loop(vui_push_layout_wrap(value), vui_pop_layout_wrap())

#define vui_push_image_scale_mode(value) _vui_push_ctrl_attr(VuiCtrlAttr_image_scale_mode, (VuiCtrlAttrValue) { .image_scale_mode = value })
#define vui_pop_image_scale_mode() _vui_pop_ctrl_attr(VuiCtrlAttr_image_scale_mode)
#define vui_scope_image_scale_mode(value) _vui_defer_loop(vui_push_image_scale_mode(value), vui_pop_image_scale_mode())

typedef uint64_t VuiCtrlFlags;
enum {
	VuiCtrlFlags_focusable = 0x1,
	VuiCtrlFlags_scrollable_vertical = 0x2,
	VuiCtrlFlags_scrollable_vertical_always_show = 0x4,
	VuiCtrlFlags_scrollable_horizontal = 0x8,
	VuiCtrlFlags_scrollable_horizontal_always_show = 0x10,
	VuiCtrlFlags_resizable = 0x20,
	VuiCtrlFlags_pressable = 0x100,
	VuiCtrlFlags_toggleable = 0x200,
	VuiCtrlFlags_selectable = 0x400,
	_VuiCtrlFlags_show_vertical_bar = 0x2000,
	_VuiCtrlFlags_show_horizontal_bar = 0x4000,
};

typedef uint8_t VuiLayoutType;
enum {
	VuiLayoutType_stack,
	VuiLayoutType_row,
	VuiLayoutType_column,
};
extern char* VuiLayoutType_strings[];

typedef struct VuiCtrl VuiCtrl;
typedef struct VuiCtrlStyle VuiCtrlStyle;
typedef void (*VuiCtrlRenderFn)(VuiCtrl* ctrl, const VuiCtrlStyle* style, VuiRect* content_rect);
typedef void (*VuiCtrlStyleInterpFn)(VuiCtrlStyle* result, const VuiCtrlStyle* to, const VuiCtrlStyle* from, float interp_ratio);
struct VuiCtrl {
	VuiCtrlId id;
	VuiCtrlId parent_id;
	VuiCtrlId child_first_id;
	VuiCtrlId child_last_id;
	VuiCtrlId sibling_prev_id;
	VuiCtrlId sibling_next_id;
	VuiCtrlId scroll_content_id;
	VuiCtrlSibId sib_id;
	uint32_t last_frame_idx;
	VuiRect rect;
	VuiCtrlStateFlags state_flags;
    VuiLayoutType layout_type;
	VuiFocusState focus_state;
	VuiCtrlFlags flags;
	VuiCtrlRenderFn render_fn;
	VuiVec2 scroll_offset;

	VuiCtrlStyleInterpFn style_interp_fn;
	const VuiCtrlStyle* style;
	const VuiCtrlStyle* target_style;
	float interp_ratio;

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
		struct {
			VuiVec2 scroll_view_size;
		};
	};
	VuiCtrlAttrs attributes;
};

extern VuiCtrlState VuiCtrl_state(VuiCtrl* ctrl);
extern void VuiCtrl_target_style(VuiCtrl* ctrl, const VuiCtrlStyle* target_style);
extern VuiThickness VuiCtrl_interp_margin(VuiCtrl* ctrl);
extern VuiThickness VuiCtrl_interp_padding(VuiCtrl* ctrl);

#define inline_VuiCtrlStyle \
	struct { \
		VuiThickness margin; \
		VuiThickness padding; \
		VuiColor bg_color; \
		VuiColor border_color; \
		float border_width; \
		float radius; \
	}

struct VuiCtrlStyle {
	inline_VuiCtrlStyle;
};

extern void VuiCtrlStyle_interp(VuiCtrlStyle* result, const VuiCtrlStyle* to, const VuiCtrlStyle* from, float interp_ratio);

typedef struct VuiTextStyle VuiTextStyle;
struct VuiTextStyle {
	union {
		inline_VuiCtrlStyle;
		VuiCtrlStyle ctrl;
	};
	VuiFontId font_id;
	VuiColor color;
	float line_height;
};

extern void VuiTextStyle_interp(VuiCtrlStyle* result, const VuiCtrlStyle* to, const VuiCtrlStyle* from, float interp_ratio);

typedef struct VuiSeparatorStyle VuiSeparatorStyle;
struct VuiSeparatorStyle {
	union {
		inline_VuiCtrlStyle;
		VuiCtrlStyle ctrl;
	};
	float size;
};

extern void VuiSeparatorStyle_interp(VuiCtrlStyle* result, const VuiCtrlStyle* to, const VuiCtrlStyle* from, float interp_ratio);

#define inline_VuiButtonStyle \
	struct { \
		union { \
			inline_VuiCtrlStyle; \
			VuiCtrlStyle ctrl; \
		}; \
		const VuiCtrlStyle* image_style; \
		const VuiTextStyle* text_style; \
	}

typedef struct VuiButtonStyle VuiButtonStyle;
struct VuiButtonStyle {
	inline_VuiButtonStyle;
};

extern void VuiButtonStyle_interp(VuiCtrlStyle* result, const VuiCtrlStyle* to, const VuiCtrlStyle* from, float interp_ratio);

typedef struct VuiCheckBoxStyle VuiCheckBoxStyle;
struct VuiCheckBoxStyle {
	union {
		inline_VuiButtonStyle;
		VuiButtonStyle button;
	};
	VuiColor check_color;
	float check_size;
};

extern void VuiCheckBoxStyle_interp(VuiCtrlStyle* result, const VuiCtrlStyle* to, const VuiCtrlStyle* from, float interp_ratio);

typedef struct VuiRadioButtonStyle VuiRadioButtonStyle;
struct VuiRadioButtonStyle {
	union {
		inline_VuiButtonStyle;
		VuiButtonStyle button;
	};
	VuiColor check_color;
	float check_size;
};

extern void VuiRadioButtonStyle_interp(VuiCtrlStyle* result, const VuiCtrlStyle* to, const VuiCtrlStyle* from, float interp_ratio);

typedef struct VuiSliderStyle VuiSliderStyle;
struct VuiSliderStyle {
	union {
		inline_VuiCtrlStyle;
		VuiCtrlStyle ctrl;
	};
	const VuiCtrlStyle* bar_style;
	const VuiButtonStyle* button_style; // points to an array of styles, one for for each VuiCtrlState value
	float bar_height;
	float button_width;
};

extern void VuiSliderStyle_interp(VuiCtrlStyle* result, const VuiCtrlStyle* to, const VuiCtrlStyle* from, float interp_ratio);

typedef struct VuiProgressBarStyle VuiProgressBarStyle;
struct VuiProgressBarStyle {
	union {
		inline_VuiCtrlStyle;
		VuiCtrlStyle ctrl;
	};
	VuiColor bar_color;
};

extern void VuiProgressBarStyle_interp(VuiCtrlStyle* result, const VuiCtrlStyle* to, const VuiCtrlStyle* from, float interp_ratio);

typedef struct VuiScrollBarStyle VuiScrollBarStyle;
struct VuiScrollBarStyle {
	union {
		inline_VuiCtrlStyle;
		VuiCtrlStyle ctrl;
	};
	const VuiButtonStyle* slider_style; // points to an array of styles, one for for each VuiCtrlState value
	float slider_width;
};

extern void VuiScrollBarStyle_interp(VuiCtrlStyle* result, const VuiCtrlStyle* to, const VuiCtrlStyle* from, float interp_ratio);

typedef struct VuiScrollViewStyle VuiScrollViewStyle;
struct VuiScrollViewStyle {
	union {
		inline_VuiCtrlStyle;
		VuiCtrlStyle ctrl;
	};
	const VuiScrollBarStyle* bar_style;
};

extern void VuiScrollViewStyle_interp(VuiCtrlStyle* result, const VuiCtrlStyle* to, const VuiCtrlStyle* from, float interp_ratio);

typedef struct VuiTextBoxStyle VuiTextBoxStyle;
struct VuiTextBoxStyle {
	union {
		inline_VuiCtrlStyle;
		VuiCtrlStyle ctrl;
	};
	const VuiTextStyle* text_style;
	VuiColor selection_color;
	VuiColor cursor_color;
	float cursor_width;
};

extern void VuiTextBoxStyle_interp(VuiCtrlStyle* result, const VuiCtrlStyle* to, const VuiCtrlStyle* from, float interp_ratio);

// ===========================================================================================
//
//
// Default Style
//
//
// ===========================================================================================

#define vui_margin_default VuiThickness_init_even(4.f)
#define vui_padding_default VuiThickness_init_even(4.f)
#define vui_border_width_default 2.f
#define vui_radius_default 4.f
#define vui_line_height_header 32.f
#define vui_line_height_menu 24.f
#define vui_scroll_bar_width_default 18.f
#define vui_separator_size_default 4.f
#define vui_check_size_default 20.f
#define vui_cursor_width_default 2.f
#define vui_slider_bar_height_default 16.f
#define vui_slider_button_width_default 32.f

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

typedef struct {
	VuiCtrlStyle image;
	VuiCtrlStyle box_panel;
	VuiTextStyle text_header;
	VuiTextStyle text_menu;
	VuiSeparatorStyle separator;
	VuiButtonStyle button_action[VuiCtrlState_COUNT];
	VuiButtonStyle button_confirm[VuiCtrlState_COUNT];
	VuiButtonStyle button_deny[VuiCtrlState_COUNT];
	VuiButtonStyle button_info[VuiCtrlState_COUNT];
	VuiButtonStyle toggle_button[VuiCtrlState_COUNT];
	VuiButtonStyle select_button[VuiCtrlState_COUNT];
	VuiCheckBoxStyle check_box[VuiCtrlState_COUNT];
	VuiRadioButtonStyle radio_button[VuiCtrlState_COUNT];
	VuiCtrlStyle slider_bar;
	VuiSliderStyle slider;
	VuiProgressBarStyle progress_bar;
	VuiTextBoxStyle text_box[VuiCtrlState_COUNT];
	VuiCtrlStyle text_box_selection;
	VuiCtrlStyle text_box_cursor;
	VuiScrollViewStyle scroll_view;
	VuiScrollBarStyle scroll_bar;
	VuiButtonStyle scroll_bar_slider[VuiCtrlState_COUNT];
} VuiStyleSheet;
extern VuiStyleSheet vui_ss;

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

#ifndef VuiVertexT

typedef struct {
	VuiVec2 pos;
	VuiVec2 uv;
	VuiColor color;
} VuiVertexT;

#define VuiVertex_init(pos_, uv_, color_) (VuiVertexT) { .pos = pos_, .uv = uv_, .color = color_ }
#define VuiVertex_scale_pos(vertex, scale_factor) (vertex).pos.x *= scale_factor; (vertex).pos.y *= scale_factor;
#define VuiVertex_debug_fprintf(vertex, file) \
	fprintf(file, \
			"\t%u: { pos: [%f, %f], uv: [%f, %f], color: #%.2x%.2x%.2x%.2x }\n", \
			vert_idx, \
			vert->pos.x, \
			vert->pos.y, \
			vert->uv.x, \
			vert->uv.y, \
			vert->color.r, \
			vert->color.g, \
			vert->color.b, \
			vert->color.a);

#else // VuiVertexT

#ifndef VuiVertex_init
#error "VuiVertex_init must be defined when defining a custom VuiVertexT"
#endif // VuiVertex_init

#ifndef VuiVertex_scale_pos
#error "VuiVertex_scale_pos must be defined when defining a custom VuiVertexT"
#endif // VuiVertex_scale_pos

#ifndef VuiVertex_debug_fprintf
#error "VuiVertex_debug_fprintf must be defined when defining a custom VuiVertexT"
#endif // VuiVertex_debug_fprintf

#endif
typedef VuiVertexT VuiVertex;

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

extern void vui_render_line(VuiVec2 start_pos, VuiVec2 end_pos, VuiColor color, float width);
extern void vui_render_rect(const VuiRect* rect, VuiColor color, float radius);
extern void vui_render_rect_border(const VuiRect* rect, VuiColor color, float radius, float width);
extern void vui_render_triangle(VuiVec2 a, VuiVec2 b, VuiVec2 c, VuiColor color);
extern void vui_render_triangle_border(VuiVec2 a, VuiVec2 b, VuiVec2 c, VuiColor color, float width);
extern void vui_render_circle(VuiVec2 pos, float radius, VuiColor color);
extern void vui_render_circle_border(VuiVec2 pos, float radius, VuiColor color, float width);
extern void vui_render_text(VuiVec2 pos, VuiFontId font_id, float line_height, char* text, uint32_t text_length, VuiColor color, float wrap_at_width);
extern void vui_render_polyline(VuiVec2* points, uint32_t points_count, VuiColor color, float width, VuiBool connect_first_and_last);
extern void vui_render_convex_polygon(VuiVec2* points, uint32_t points_count, VuiColor color);
extern void vui_render_bezier_curve(VuiVec2 start_pos, VuiVec2 end_pos, VuiVec2 start_anchor_pos, VuiVec2 end_anchor_pos, VuiColor color, float width);

extern void vui_render_image(const VuiRect* rect, VuiImageId image_id, VuiColor image_tint, VuiImageScaleMode scale_mode);
extern void vui_render_image_(const VuiRect* rect, float image_width, float image_height, VuiTextureId texture_id, VuiRect uv_rect, VuiColor color, VuiImageScaleMode scale_mode, VuiBool is_glyph);

extern void vui_path_reset();
extern void vui_path_plot_point(VuiVec2 pt);
extern void vui_path_plot_arc(VuiVec2 pt, float radius, float angle_start, float angle_end, uint32_t segments_count);
extern void vui_path_plot_bezier_curve(VuiVec2 start_control_pt, VuiVec2 end_control_pt, VuiVec2 end_pt);
extern void vui_path_plot_rect(const VuiRect* rect, float radius);
extern void vui_path_plot_circle(VuiVec2 pt, float radius, VuiBool is_border);
extern void vui_render_path_stroked(VuiColor color, float width, VuiBool connect_first_and_last);
extern void vui_render_path_filled_convex(VuiColor color);

typedef struct {
	VuiVertex* verts;
	VuiVertexIdx* indices;
	uint32_t verts_start_idx;
} VuiRenderWriter;

extern VuiRenderWriter vui_render_get_writer(VuiTextureId texture_id, uint32_t verts_count, uint32_t indices_count);
extern void vui_render_inc_layer();
extern void vui_render_dec_layer();

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

extern void vui_stack_layout();
extern void vui_column_layout();
extern void vui_row_layout();

typedef uint8_t VuiActiveChange;
enum {
	VuiActiveChange_none = 0,
	VuiActiveChange_to_inactive = vui_false + 1,
	VuiActiveChange_to_active = vui_true + 1,
};

extern VuiCtrl* vui_ctrl_get(VuiCtrlId ctrl_id);
extern VuiCtrl* vui_ctrl_try_get(VuiCtrlId ctrl_id);
#define vui_ctrl_start(sib_id, style) vui_ctrl_start_(sib_id, 0, 0, style, VuiCtrlStyle_interp, NULL)
extern void vui_ctrl_start_(VuiCtrlSibId sib_id, VuiCtrlFlags flags, VuiActiveChange active_change, const VuiCtrlStyle* style, VuiCtrlStyleInterpFn style_interp_fn, VuiCtrlRenderFn render_fn);
extern void vui_ctrl_end();
static inline void vui_ctrl(VuiCtrlSibId sib_id, const VuiCtrlStyle* style) {
	vui_ctrl_start(sib_id, style);
	vui_ctrl_end();
}
#define vui_scope_ctrl(sib_id, style) _vui_defer_loop(vui_ctrl_start(sib_id, style), vui_ctrl_end())

// ====================================================================================
//
//
// Text
//
//
// @param text: the text to be displayed
// @param text_length: the length of text in bytes
// @param wrap_at_width = 0.0 to not have word wrapping
//
#define vui_text(sib_id, text, wrap_at_width, style) vui_text_(sib_id, text, strlen(text), wrap_at_width, style)
extern void vui_text_(VuiCtrlSibId sib_id, char* text, uint32_t text_length, float wrap_at_width, const VuiTextStyle* style);

// ====================================================================================
//
//
// Image
//
//
// @param image_id: the image identifier of the image you wish to display in the button
// @param image_tint: the color to be multiplied with the image pixels.
//                    can be used to make the image transparent or reduce a certain color channel.
//                    to show the image unchanged, provide a value of vui_color_white.
extern void vui_image(VuiCtrlSibId sib_id, VuiImageId image_id, VuiColor image_tint, const VuiCtrlStyle* style);

extern void vui_spacing(VuiCtrlSibId sib_id, float width, float height);
extern void vui_separator(VuiCtrlSibId sib_id, const VuiSeparatorStyle* style);

// ====================================================================================
//
//
// Button
//
//
// @param sib_id: the unique sibling identifier, see the vui_sib_id macro for more.
// @param text, text_length: see vui_text
// @param image_id, image_tint: see vui_image
//
extern VuiFocusState vui_button_start(VuiCtrlSibId sib_id, const VuiButtonStyle styles[VuiCtrlState_COUNT]);
extern void vui_button_end();
extern VuiFocusState vui_button(VuiCtrlSibId sib_id, const VuiButtonStyle styles[VuiCtrlState_COUNT]);
#define vui_text_button(sib_id, text, styles) vui_text_button_(sib_id, text, strlen(text), styles)
extern VuiFocusState vui_text_button_(VuiCtrlSibId sib_id, char* text, uint32_t text_length, const VuiButtonStyle styles[VuiCtrlState_COUNT]);
extern VuiFocusState vui_image_button(VuiCtrlSibId sib_id, VuiImageId image_id, VuiColor image_tint, const VuiButtonStyle styles[VuiCtrlState_COUNT]);
#define vui_image_text_button(sib_id, image_id, image_tint, text, styles) vui_image_text_button_(sib_id, image_id, image_tint, text, strlen(text), styles)
extern VuiFocusState vui_image_text_button_(VuiCtrlSibId sib_id, VuiImageId image_id, VuiColor image_tint, char* text, uint32_t text_length, const VuiButtonStyle styles[VuiCtrlState_COUNT]);

// ====================================================================================
//
//
// Toggle Button
//
//
// @param sib_id: the unique sibling identifier, see the vui_sib_id macro for more.
// @param pressed: a pointer to the pressed state of the toggle button.
//                 use this to change the state external or store the state to be restored at a later time.
//                 if you do not need this you can supply a value of NULL.
// @param text, text_length: see vui_text
// @param image_id, image_tint: see vui_image
//
extern VuiBool vui_toggle_button_start(VuiCtrlSibId sib_id, VuiBool* pressed, const VuiButtonStyle styles[VuiCtrlState_COUNT]);
void vui_toggle_button_end();
#define vui_text_toggle_button(sib_id, pressed, text, styles) vui_text_toggle_button_(sib_id, pressed, text, strlen(text), styles)
extern VuiBool vui_text_toggle_button_(VuiCtrlSibId sib_id, VuiBool* pressed, char* text, uint32_t text_length, const VuiButtonStyle styles[VuiCtrlState_COUNT]);
extern VuiBool vui_image_toggle_button(VuiCtrlSibId sib_id, VuiBool* pressed, VuiImageId image_id, VuiColor image_tint, const VuiButtonStyle styles[VuiCtrlState_COUNT]);
#define vui_image_text_toggle_button(sib_id, pressed, image_id, image_tint, text, styles) vui_image_text_toggle_button_(sib_id, pressed, image_id, image_tint, text, strlen(text), styles)
extern VuiBool vui_image_text_toggle_button_(VuiCtrlSibId sib_id, VuiBool* pressed, VuiImageId image_id, VuiColor image_tint, char* text, uint32_t text_length, const VuiButtonStyle styles[VuiCtrlState_COUNT]);

// ====================================================================================
//
//
// Select Button
//
//
// @param sib_id: the unique sibling identifier, see the vui_sib_id macro for more.
// @param selected_sib_id: a pointer to the currently selected sibling identifier.
//                         pass the same pointer into a series of different select buttons
//                         and when selecting a button this will happen *selected_sib_id = @param(sib_id).
// @param text, text_length: see vui_text
// @param image_id, image_tint: see vui_image
//
// @example:
//
// enum {
//     Option_one,
//     Option_two,
//     Option_three,
//     Option_four,
//     Option_COUNT,
// };
//
// static char* option_names[] = {
//     [Option_one] = "one",
//     [Option_two] = "two",
//     [Option_three] = "three",
//     [Option_four] = "four",
// };
//
// // zero is a null identifier so add one to the enumerations.
// static VuiCtrlSibId selected_sib_id = 0;
// for (int i = 0; i < Option_COUNT; i += 1) {
//     vui_text_select_button(i + 1, &selected_sib_id, option_names[i]);
// }
//
// if (*selected_sib_id > 0) {
//     int enum_value = *selected_sib_id - 1;
//     // do something with enum_value
// }
//
extern VuiBool vui_select_button_start(VuiCtrlSibId sib_id, VuiCtrlSibId* selected_sib_id, const VuiButtonStyle styles[VuiCtrlState_COUNT]);
extern void vui_select_button_end();
#define vui_text_select_button(sib_id, selected_sib_id, text, styles) vui_text_select_button_(sib_id, selected_sib_id, text, strlen(text), styles)
extern VuiBool vui_text_select_button_(VuiCtrlSibId sib_id, VuiCtrlSibId* selected_sib_id, char* text, uint32_t text_length, const VuiButtonStyle styles[VuiCtrlState_COUNT]);
extern VuiBool vui_image_select_button(VuiCtrlSibId sib_id, VuiCtrlSibId* selected_sib_id, VuiImageId image_id, VuiColor image_tint, const VuiButtonStyle styles[VuiCtrlState_COUNT]);
#define vui_image_text_select_button(sib_id, selected_sib_id, image_id, image_tint, text, styles) \
	vui_image_text_select_button_(sib_id, selected_sib_id, image_id, image_tint, text, strlen(text), styles)
extern VuiBool vui_image_text_select_button_(VuiCtrlSibId sib_id, VuiCtrlSibId* selected_sib_id, VuiImageId image_id, VuiColor image_tint, char* text, uint32_t text_length, const VuiButtonStyle styles[VuiCtrlState_COUNT]);

// ====================================================================================
//
//
// Check Box
//
//
// @param sib_id: the unique sibling identifier, see the vui_sib_id macro for more.
// @param checked: a pointer to the checked state of the check box.
//                 use this to change the state external or store the state to be restored at a later time.
//                 if you do not need this you can supply a value of NULL.
// @param text, text_length: see vui_text
// @param image_id, image_tint: see vui_image
//
//
extern VuiBool vui_check_box(VuiCtrlSibId sib_id, VuiBool* checked, const VuiCheckBoxStyle styles[VuiCtrlState_COUNT]);
#define vui_text_check_box(sib_id, checked, text, styles) vui_text_check_box_(sib_id, checked, text, strlen(text), styles)
extern VuiBool vui_text_check_box_(VuiCtrlSibId sib_id, VuiBool* checked, char* text, uint32_t text_length, const VuiCheckBoxStyle styles[VuiCtrlState_COUNT]);
extern VuiBool vui_image_check_box(VuiCtrlSibId sib_id, VuiBool* checked, VuiImageId image_id, VuiColor image_tint, const VuiCheckBoxStyle styles[VuiCtrlState_COUNT]);

// ====================================================================================
//
//
// Radio Button
//
//
// @param sib_id: the unique sibling identifier, see the vui_sib_id macro for more.
// @param selected_sib_id: a pointer to the currently selected sibling identifier.
//                         pass the same pointer into a series of different select buttons
//                         and when selecting a button this will happen *selected_sib_id = @param(sib_id).
// @param text, text_length: see vui_text
// @param image_id, image_tint: see vui_image
//
extern VuiBool vui_radio_button(VuiCtrlSibId sib_id, VuiCtrlSibId* selected_sib_id, const VuiRadioButtonStyle styles[VuiCtrlState_COUNT]);
#define vui_text_radio_button(sib_id, selected_sib_id, text, styles) vui_text_radio_button_(sib_id, selected_sib_id, text, strlen(text), styles)
extern VuiBool vui_text_radio_button_(VuiCtrlSibId sib_id, VuiCtrlSibId* selected_sib_id, char* text, uint32_t text_length, const VuiRadioButtonStyle styles[VuiCtrlState_COUNT]);
extern VuiBool vui_image_radio_button(VuiCtrlSibId sib_id, VuiCtrlSibId* selected_sib_id, VuiImageId image_id, VuiColor image_tint, const VuiRadioButtonStyle styles[VuiCtrlState_COUNT]);

// ====================================================================================
//
//
// Slider - a control used to manipulate a value using a slider.
//
//
// @param sib_id: the unique sibling identifier, see the vui_sib_id macro for more.
// @param value_out: a pointer to the value to be manipulated.
//               the value will start at @param(min) and end at @param(max).
//
extern void vui_slider_uint(VuiCtrlSibId sib_id, uint32_t* value_out, uint32_t min, uint32_t max, const VuiSliderStyle* style);
extern void vui_slider_sint(VuiCtrlSibId sib_id, int32_t* value_out, int32_t min, int32_t max, const VuiSliderStyle* style);
extern void vui_slider_float(VuiCtrlSibId sib_id, float* value_out, float min, float max, const VuiSliderStyle* style);

// ====================================================================================
//
//
// Progress Bar
//
//
// @param sib_id: the unique sibling identifier, see the vui_sib_id macro for more.
// @param value: the value used to calculate the size of the progress meter.
//               the value will start at @param(min) and end at @param(max).
//
extern void vui_progress_bar(VuiCtrlSibId sib_id, float value, float min, float max, const VuiProgressBarStyle* style);

// ====================================================================================
//
//
// Text & Input Box
//
//
// @param sib_id: the unique sibling identifier, see the vui_sib_id macro for more.
// @param string_in_out: a pointer to the buffer holding a string of characters that will be presented in the
//                       text box and written back out to when input is provided
// @param string_in_out_cap: the capacity of the string buffer pointed to by @param(string_in_out)
//
// @param value: the pointer to the value that will be presented in the text box and written back out when modified.
//
extern VuiBool vui_text_box(VuiCtrlSibId sib_id, char* string_in_out, uint32_t string_in_out_cap, const VuiTextBoxStyle styles[VuiCtrlState_COUNT]);
extern VuiBool vui_input_box_uint(VuiCtrlSibId sib_id, uint32_t* value, const VuiTextBoxStyle styles[VuiCtrlState_COUNT]);
extern VuiBool vui_input_box_sint(VuiCtrlSibId sib_id, int32_t* value, const VuiTextBoxStyle styles[VuiCtrlState_COUNT]);
extern VuiBool vui_input_box_float(VuiCtrlSibId sib_id, float* value, const VuiTextBoxStyle styles[VuiCtrlState_COUNT]);

typedef uint8_t VuiScrollFlags;
//
// WARNING: these must share the same values as the ones in VuiCtrlFlags.
enum {
	VuiScrollFlags_none = 0x0,
	VuiScrollFlags_vertical = 0x2,
	VuiScrollFlags_vertical_always_show = 0x4,
	VuiScrollFlags_horizontal = 0x8,
	VuiScrollFlags_horizontal_always_show = 0x10,
	VuiScrollFlags_resizable = 0x20,
};

// ====================================================================================
//
//
// Scroll View
//
//
// @param sib_id: the unique sibling identifier, see the vui_sib_id macro for more.
//
// @param content_offset_in_out:
//     a pointer to a content offset that can be used to keep track of the offest
//     but it can also allow you to change the offset by setting the value that is pointed too.
//     this value can be NULL if you do not need this functionality.
//
// @param size_in_out:
//     a pointer to a size that can be used to keep track of the size when @param(flags) has the VuiScrollFlags_resizable bit set.
//     but it can also allow you to change the size by setting the value that is pointed too.
//     this value can be NULL if you do not need this functionality.
//
// @param flags: binary OR the values of VuiScrollFlags to enable features of the scroll view.
//
//    VuiScrollFlags_vertical:
//        enables vertical scrolling capabilities and will show a scroll bar when needed
//
//    VuiScrollFlags_vertical_always_show:
//        if vertical scrolling is enabled, then the scroll bar will always show
//
//    VuiScrollFlags_horizontal:
//        enables horizontal scrolling capabilities and will show a scroll bar when needed
//
//    VuiScrollFlags_horizontal_always_show:
//        if horizontal scrolling is enabled, then the scroll bar will always show
//
//    VuiScrollFlags_resizable:
//        allow you to resize the scroll view. if this is enabled then the width and height style attributes
//        only initialize the size and do not change the size on future calls.
//        unless the scroll view is not executed one frame and then reinitialized later.
//
#define vui_scroll_view_start(sib_id, flags, style) vui_scroll_view_start_(sib_id, NULL, NULL, flags, style)
extern void vui_scroll_view_start_(VuiCtrlSibId sib_id, VuiVec2* content_offset_in_out, VuiVec2* size_in_out, VuiScrollFlags flags, const VuiScrollViewStyle* style);
extern void vui_scroll_view_end();

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

extern VuiImageId vui_image_add(VuiImage* image);
extern VuiImage* vui_image_get(VuiImageId image_id);
extern void vui_image_remove(VuiImageId image_id);

typedef uint16_t VuiViewportId;

typedef void (*VuiRenderGlyphFn)(const VuiRect* rect, VuiTextureId glyph_texture_id, const VuiRect* uv_rect);
typedef VuiVec2 (*VuiPositionTextFn)(void* userdata, VuiFontId font_id, float line_height, char* text, uint32_t text_length, float wrap_at_width, VuiVec2 top_left, VuiRenderGlyphFn render_glyph_fn);
typedef void (*VuiTextBoxFocusChange)(VuiBool focused);

typedef struct {
	VuiPositionTextFn position_text_fn;
	void* position_text_userdata;
	uint16_t windows_count;
	void* allocator;
	VuiFontId default_font_id;
} VuiSetup;

extern VuiBool vui_init(VuiSetup* setup);

extern void vui_frame_start(VuiBool right_to_left);
extern void vui_frame_end();

extern void vui_window_start(VuiWindowId id, VuiVec2 size);
extern void vui_window_end();

extern VuiWindowRender* vui_window_render(VuiWindowId id, float scale_factor, VuiBool pixel_snapping);
extern void vui_window_set_mouse_focused(VuiWindowId id);
extern void vui_window_set_focused(VuiWindowId id);

extern void vui_window_dump_render(VuiWindowId id, FILE* file);

// allocate zeroed memory that is cleared at the end of the frame.
#define vui_frame_data_alloc_elmt(T) (T*)vui_frame_data_alloc(sizeof(T), alignof(T));
extern void* vui_frame_data_alloc(uint32_t size, uint32_t align);

#endif // VUI_H

