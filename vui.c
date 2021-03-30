#ifndef VUI_H
#include "vui.h"
#endif

#include <stddef.h>
#include <stdarg.h>
#include <signal.h>

// ===========================================================================================
//
//
// Internal
//
//
// ===========================================================================================

#define _vui_ctrls_init_cap 4096
#define _VuiArenaAlctor_arena_size 8192

typedef struct {
	void* next;
} _VuiArenaHeader;

typedef struct {
	void* arenas_head;
	void* arena;
	uint32_t pos;
} _VuiArenaAlctor;

void _VuiArenaAlctor_init(_VuiArenaAlctor* alctor) {
	*alctor = (_VuiArenaAlctor){0};
	alctor->arenas_head = vui_mem_alloc(_vui.allocator, _VuiArenaAlctor_arena_size, 1);
	memset(alctor->arenas_head, 0, _VuiArenaAlctor_arena_size);
	alctor->arena = alctor->arenas_head;
}

void _VuiArenaAlctor_reset(_VuiArenaAlctor* alctor) {
	alctor->arena = alctor->arenas_head;
	alctor->pos = sizeof(_VuiArenaHeader);
	memset(alctor->arena + alctor->pos, 0, _VuiArenaAlctor_arena_size - alctor->pos);
}

void* _VuiArenaAlctor_alloc(_VuiArenaAlctor* alctor, uintptr_t size, uintptr_t align) {
	while (1) {
		// an allocation gets the pointer by adding the position to the start of the buffer.
		void* ptr = vui_ptr_add(alctor->arena, alctor->pos);
		// rounds up the pointer so it aligned as requested.
		ptr = vui_ptr_round_up_align(ptr, align);
		uint32_t next_pos = vui_ptr_diff(ptr, alctor->arena) + size;
		// checks to see if it fits in the linear buffer.
		if (next_pos <= _VuiArenaAlctor_arena_size) {
			// and just increments the position for the next allocation.
			alctor->pos = next_pos;
			return ptr;
		} else {
			// allocate a new arena
			void* arena = vui_mem_alloc(_vui.allocator, _VuiArenaAlctor_arena_size, 1);
			// add it to the arena link list
			((_VuiArenaHeader*)alctor->arena)->next = arena;
			// now make it the current arena and zero its memory
			alctor->arena = arena;
			alctor->pos = sizeof(_VuiArenaHeader);
			memset(alctor->arena, 0, _VuiArenaAlctor_arena_size);
		}
	}
}

typedef struct {
    float x;
    float y;
    float offset_x;
    float offset_y;
    float wheel_offset_x;
    float wheel_offset_y;
    VuiMouseButtons buttons_is_pressed;
    VuiMouseButtons buttons_has_been_pressed;
    VuiMouseButtons buttons_has_been_released;
} _VuiMouse;

typedef uint8_t _VuiInputBoxType;
enum {
	_VuiInputBoxType_text,
	_VuiInputBoxType_float,
	_VuiInputBoxType_u32,
	_VuiInputBoxType_s32,
};

#define _vui_input_box_cap 16

typedef struct VuiRenderLayer VuiRenderLayer;
struct VuiRenderLayer {
	VuiStk(VuiRenderCmd) cmds;
    VuiStk(VuiVertex) verts;
	VuiStk(VuiVertexIdx) indices;
};

typedef struct {
	VuiCtrlId root_ctrl_id;
	VuiVec2 size;
    VuiCtrlId focused_ctrl_id;
	VuiStk(char) text;
	VuiStk(VuiRenderLayer) render_layers;
	VuiWindowRender render;
} _VuiWindow;

typedef uint8_t VuiCtrlAttrType;
enum {
	VuiCtrlAttrType_float,
	VuiCtrlAttrType_vec2,
	VuiCtrlAttrType_align,
	VuiCtrlAttrType_bool,
	VuiCtrlAttrType_image_scale_mode,
};

typedef struct VuiCtrlAttrChange VuiCtrlAttrChange;
struct VuiCtrlAttrChange {
	VuiCtrlAttrChange* prev;
	VuiCtrlAttrValue value;
};

typedef uint32_t _VuiFlags;
enum {
	_VuiFlags_out_of_memory = 0x1,
	_VuiFlags_pixel_snapping = 0x2,
	_VuiFlags_right_to_left = 0x4,
};

typedef struct _VuiImage _VuiImage;
struct _VuiImage {
	VuiImage inner;
	uint16_t counter;
};
typedef_VuiPool(_VuiImage);

typedef struct _VuiCtrl _VuiCtrl;
struct _VuiCtrl {
	VuiCtrl inner;
	uint16_t counter;
};
typedef_VuiPool(_VuiCtrl);
typedef struct {
	VuiPositionTextFn position_text_fn;
	void* position_text_userdata;
	void* allocator;

	_VuiWindow* windows;
	uint16_t windows_count;

	_VuiFlags flags;
	VuiWindowId mouse_focused_window_id;
	VuiWindowId focused_window_id;
	VuiCtrlId mouse_scroll_focused_ctrl_id;
	VuiCtrlId mouse_focused_ctrl_id;

	VuiPool(_VuiCtrl) ctrl_pool;
	VuiPool(_VuiImage) image_pool;
	_VuiArenaAlctor frame_data_alctor;

	struct {
		_VuiMouse mouse;
		VuiInputActions actions;
		VuiBool is_mouse_over_ctrl;

		struct {
			char* string;
			uint32_t string_len;
			uint32_t string_cap;
			uint32_t cursor_idx;
			uint32_t cursor_max_last_unchanged_column_idx;
			// signed offset from the cursor_idx.
			// cursor_idx to cursor_idx + select_offset
			// will be the selection range.
			int32_t select_offset;
			_VuiInputBoxType type;
			VuiBool has_changed;
			VuiBool is_multiline;
			VuiBool has_cursor_moved;
			VuiBool has_cursor_moved_last_frame;
		} focused_text_box;

		//
		// non text box related data.
		struct {
			uint32_t string_len;
			uint32_t edit_string_len;
			char string[_vui_input_box_cap];
			char edit_string[_vui_input_box_cap];
		} input_box;
	} input;

	//
	// the data that is used when building the UI tree.
	// this is the first stage of the pipeline.
	struct {
		_VuiWindow* w;
		uint32_t frame_idx;
		float dt;
		VuiCtrlId parent_ctrl_id;
		VuiCtrlId sibling_prev_ctrl_id;
		float fill_portion_width;
		float fill_portion_height;
		VuiCtrlAttrChange* ctrl_attr_change_list_heads[VuiCtrlAttr_COUNT];
		VuiCtrlAttrs ctrl_attrs;
		VuiVec2* mouse_scroll_focused_content_offset;
		VuiVec2* mouse_scroll_focused_size;
		VuiStk(VuiBool) disabled_stack;
		VuiStk(VuiCtrlId) popover_ctrl_ids;
	} build;

	//
	// the data that is used when rendering the UI tree.
	// this is the third stage of the pipeline.
	struct {
		_VuiWindow* w;
		VuiRect clip_rect;
		uint32_t layer_idx;
		VuiStk(VuiVec2) path_points;
	} render;
} _Vui;

_Vui _vui = {0};

VuiCtrlAttrType VuiCtrlAttr_types[VuiCtrlAttr_COUNT] = {
	[VuiCtrlAttr_width] = VuiCtrlAttrType_float,
	[VuiCtrlAttr_width_min] = VuiCtrlAttrType_float,
	[VuiCtrlAttr_width_max] = VuiCtrlAttrType_float,
	[VuiCtrlAttr_height] = VuiCtrlAttrType_float,
	[VuiCtrlAttr_height_min] = VuiCtrlAttrType_float,
	[VuiCtrlAttr_height_max] = VuiCtrlAttrType_float,
	[VuiCtrlAttr_offset] = VuiCtrlAttrType_vec2,
	[VuiCtrlAttr_align] = VuiCtrlAttrType_align,
	[VuiCtrlAttr_layout_spacing] = VuiCtrlAttrType_float,
	[VuiCtrlAttr_layout_wrap_spacing] = VuiCtrlAttrType_float,
	[VuiCtrlAttr_layout_wrap] = VuiCtrlAttrType_bool,
    [VuiCtrlAttr_image_scale_mode] = VuiCtrlAttrType_image_scale_mode,
    [VuiCtrlAttr_style_transition_time] = VuiCtrlAttrType_float,
};

uint16_t VuiCtrlAttr_offsets[VuiCtrlAttr_COUNT] = {
	[VuiCtrlAttr_width] = offsetof(VuiCtrlAttrs, width),
	[VuiCtrlAttr_width_min] = offsetof(VuiCtrlAttrs, width_min),
	[VuiCtrlAttr_width_max] = offsetof(VuiCtrlAttrs, width_max),
	[VuiCtrlAttr_height] = offsetof(VuiCtrlAttrs, height),
	[VuiCtrlAttr_height_min] = offsetof(VuiCtrlAttrs, height_min),
	[VuiCtrlAttr_height_max] = offsetof(VuiCtrlAttrs, height_max),
	[VuiCtrlAttr_offset] = offsetof(VuiCtrlAttrs, offset),
	[VuiCtrlAttr_align] = offsetof(VuiCtrlAttrs, align),
	[VuiCtrlAttr_layout_spacing] = offsetof(VuiCtrlAttrs, layout_spacing),
	[VuiCtrlAttr_layout_wrap_spacing] = offsetof(VuiCtrlAttrs, layout_wrap_spacing),
	[VuiCtrlAttr_layout_wrap] = offsetof(VuiCtrlAttrs, layout_wrap),
    [VuiCtrlAttr_image_scale_mode] = offsetof(VuiCtrlAttrs, image_scale_mode),
    [VuiCtrlAttr_style_transition_time] = offsetof(VuiCtrlAttrs, style_transition_time),
};

char* VuiCtrlState_strings[VuiCtrlState_COUNT] = {
	[VuiCtrlState_default] = "default",
	[VuiCtrlState_focused] = "focused",
	[VuiCtrlState_active] = "active",
	[VuiCtrlState_disabled] = "disabled",
};

void vui_push_disabled(VuiBool enabled) {
	VuiBool* t = VuiStk_push(&_vui.build.disabled_stack);
	vui_ensure_alloc_ok(t);
	*t = enabled;
}

void vui_pop_disabled() {
	vui_assert(VuiStk_count(_vui.build.disabled_stack), "vui_pop_disabled has been called more than vui_push_disabled")
	VuiStk_pop(_vui.build.disabled_stack);
}

void _vui_push_ctrl_attr(VuiCtrlAttr attr, VuiCtrlAttrValue value) {
	//
	// allocate a new change to store the old attribute.
	VuiCtrlAttrChange* old_attr_change = vui_frame_data_alloc_elmt(VuiCtrlAttrChange);
	old_attr_change->prev = _vui.build.ctrl_attr_change_list_heads[attr];
	_vui.build.ctrl_attr_change_list_heads[attr] = old_attr_change;

	void* ptr = vui_ptr_add(&_vui.build.ctrl_attrs, (uintptr_t)VuiCtrlAttr_offsets[attr]);
	VuiCtrlAttrType type = VuiCtrlAttr_types[attr];

	//
	// store the orignal value in the old_attr_change (history entry)
	// and then set the value in the global attributes structure
	switch (type) {
		case VuiCtrlAttrType_float:
			old_attr_change->value.float_ = *(float*)ptr;
			*(float*)ptr = value.float_;
			break;
		case VuiCtrlAttrType_vec2:
			old_attr_change->value.vec2 = *(VuiVec2*)ptr;
			*(VuiVec2*)ptr = value.vec2;
			break;
		case VuiCtrlAttrType_align:
			old_attr_change->value.align = *(VuiAlign*)ptr;
			*(VuiAlign*)ptr = value.align;
			break;
		case VuiCtrlAttrType_bool:
			old_attr_change->value.float_ = *(VuiBool*)ptr;
			*(VuiBool*)ptr = value.bool_;
			break;
		case VuiCtrlAttrType_image_scale_mode:
			old_attr_change->value.float_ = *(VuiImageScaleMode*)ptr;
			*(VuiImageScaleMode*)ptr = value.image_scale_mode;
			break;
	}
}

void _vui_pop_ctrl_attr(VuiCtrlAttr attr) {
	VuiCtrlAttrChange* old_attr_change = _vui.build.ctrl_attr_change_list_heads[attr];

	void* ptr = vui_ptr_add(&_vui.build.ctrl_attrs, (uintptr_t)VuiCtrlAttr_offsets[attr]);
	VuiCtrlAttrType type = VuiCtrlAttr_types[attr];

	//
	// restore the orignal value from the old_attr_change (history entry)
	// by storing the value in the global attributes structure
	switch (type) {
		case VuiCtrlAttrType_float:
			*(float*)ptr = old_attr_change->value.float_;
			break;
		case VuiCtrlAttrType_vec2:
			*(VuiVec2*)ptr = old_attr_change->value.vec2;
			break;
		case VuiCtrlAttrType_align:
			*(VuiAlign*)ptr = old_attr_change->value.align;
			break;
		case VuiCtrlAttrType_bool:
			*(VuiBool*)ptr = old_attr_change->value.bool_;
			break;
		case VuiCtrlAttrType_image_scale_mode:
			*(VuiImageScaleMode*)ptr = old_attr_change->value.image_scale_mode;
			break;
	}

	_vui.build.ctrl_attr_change_list_heads[attr] = old_attr_change->prev;
}

// ===========================================================================================
//
//
// memory allocation - element pool implementation
//
//
// ===========================================================================================

static inline void _VuiPool_assert_idx(_VuiPool* pool, uint32_t idx) {
	vui_debug_assert(idx < pool->cap, "idx is out of the memory boundary of the pool. idx is '%u' but cap is '%u'", idx, pool->cap);
}

static inline VuiBool _VuiPool_is_allocated(_VuiPool* pool, uint32_t idx) {
	_VuiPool_assert_idx(pool, idx);
	uint8_t bit = 1 << (idx % 8);
	return (((uint8_t*)pool->data)[idx / 8] & bit) == bit;
}

static inline void _VuiPool_set_allocated(_VuiPool* pool, uint32_t idx) {
	_VuiPool_assert_idx(pool, idx);
	uint8_t bit = 1 << (idx % 8);
	((uint8_t*)pool->data)[idx / 8] |= bit;
}

static inline void _VuiPool_set_free(_VuiPool* pool, uint32_t idx) {
	_VuiPool_assert_idx(pool, idx);
	((uint8_t*)pool->data)[idx / 8] &= ~(1 << (idx % 8));
}

void _VuiPool_reset(_VuiPool* pool, uintptr_t elmt_size) {
	//
	// set all the bits to 0 so all elements are marked as free
	uint8_t* is_alloced_bitset = pool->data;
	memset(is_alloced_bitset, 0, pool->elmts_start_byte_idx);

	//
	// now go through and set up the link list, so every element points to the next.
	void* elmts = vui_ptr_add(pool->data, pool->elmts_start_byte_idx);
	uintptr_t cap = pool->cap;
	for (uintptr_t i = 0; i < cap; i += 1) {
		// + 2 instead of 1 because we use id's here and not indexes.
		*(uintptr_t*)vui_ptr_add(elmts, i * elmt_size) = i + 2;
	}
	pool->count = 0;
	pool->free_list_head_id = 1;
}

VuiBool _VuiPool_resize_cap(_VuiPool* pool, uint32_t new_cap, uintptr_t elmt_size, uintptr_t elmt_align) {
	//
	// resizing the pool is fine since we store id's everywhere.
	//
	uintptr_t bitset_size = pool->elmts_start_byte_idx;
	uintptr_t new_bitset_size = (new_cap / 8) + 1 + elmt_align - 1;
	uintptr_t new_cap_bytes = ((uintptr_t)new_cap * elmt_size) + new_bitset_size;

	void* data = pool->data;
	void* new_data = vui_mem_alloc(_vui.allocator, new_cap_bytes, alignof(uint8_t));
	if (new_data == NULL) {
		_vui.flags |= _VuiFlags_out_of_memory;
		return vui_false;
	}

	//
	// zero the new bits of the is_allocated_bitset.
	memset(vui_ptr_add(new_data, bitset_size), 0, new_bitset_size - bitset_size);

	// then zero all the new elements.
	memset(vui_ptr_add(new_data, new_bitset_size + (uintptr_t)pool->cap * elmt_size), 0, ((uintptr_t)(new_cap - pool->cap) * elmt_size));

	if (data) {
		uintptr_t cap_bytes = ((uintptr_t)pool->cap * elmt_size) + bitset_size;

		//
		// copy the bitset over to the new buffer
		memcpy(new_data, data, bitset_size);

		//
		// copy the elements to their new location.
		void* elmts = vui_ptr_add(data, pool->elmts_start_byte_idx);
		void* new_elmts = vui_ptr_add(new_data, new_bitset_size);
		memcpy(new_elmts, elmts, (uintptr_t)pool->cap * elmt_size);

		//
		// deallocate the old buffer.
		vui_mem_dealloc(_vui.allocator, data, cap_bytes, alignof(uint8_t));
	}

	//
	// setup the free list, by visiting each element and store an
	// index to the next element.
	for (uint32_t i = pool->cap; i < new_cap; i += 1) {
		*(uint32_t*)vui_ptr_add(vui_ptr_add(new_data, new_bitset_size), i * elmt_size) = i + 2;
	}

	pool->data = new_data;
	pool->free_list_head_id = pool->cap + 1;
	pool->cap = new_cap;
	pool->elmts_start_byte_idx = new_bitset_size;
	return vui_true;
}

VuiBool _VuiPool_reset_and_populate(_VuiPool* pool, void* elmts, uint32_t count, uintptr_t elmt_size, uintptr_t elmt_align) {
	_VuiPool_reset(pool, elmt_size);
	if (pool->cap < count) {
		if (!_VuiPool_resize_cap(pool, vui_max(pool->cap ? pool->cap * 2 : 64, count), elmt_size, elmt_align))
			return vui_false;
	}

	//
	// set the elements to allocated.
	// the last byte is set manually as only some of the bits will be on.
	memset(pool->data, 0xff, count / 8);
	uint32_t remaining_count = count % 8;
	if (remaining_count) ((uint8_t*)pool->data)[count / 8] = (1 << remaining_count) - 1;

	//
	// copy the elements and set the values in the pool structure
	memcpy(vui_ptr_add(pool->data, pool->elmts_start_byte_idx), elmts, (uintptr_t)count * elmt_size);
	pool->count = count;
	pool->free_list_head_id = count + 1;

	return vui_true;
}

void _VuiPool_init(_VuiPool* pool, uint32_t cap, uintptr_t elmt_size, uintptr_t elmt_align) {
	uintptr_t bitset_size = (cap / 8) + 1;
	uintptr_t elmts_start_byte_idx = (uintptr_t)vui_ptr_round_up_align((void*)bitset_size, elmt_align);
	uintptr_t cap_bytes = ((uintptr_t)cap * elmt_size) + elmts_start_byte_idx;
	pool->data = vui_mem_alloc(_vui.allocator, cap_bytes, elmt_align);
	memset(vui_ptr_add(pool->data, elmts_start_byte_idx), 0, (uintptr_t)cap * elmt_size);

	pool->elmts_start_byte_idx = elmts_start_byte_idx;
	pool->cap = cap;
	_VuiPool_reset(pool, elmt_size);
	pool->free_list_head_id = 1;
}

void _VuiPool_deinit(_VuiPool* pool, uintptr_t elmt_size, uintptr_t elmt_align) {
	uintptr_t bitset_size = (pool->cap / 8) + 1;
	uintptr_t elmts_start_byte_idx = (uintptr_t)vui_ptr_round_up_align((void*)bitset_size, elmt_align);
	uintptr_t cap = ((uintptr_t)pool->cap * elmt_size) + elmts_start_byte_idx;
	vui_mem_dealloc(_vui.allocator, pool->data, cap, elmt_align);
	*pool = (_VuiPool){0};
}

void* _VuiPool_alloc(_VuiPool* pool, VuiPoolId* id_out, uintptr_t elmt_size, uintptr_t elmt_align) {
	if (pool->count == pool->cap) {
		if (!_VuiPool_resize_cap(pool, pool->cap ? pool->cap * 2 : 64, elmt_size, elmt_align)) {
			return NULL;
		}
	}

	//
	// allocate an element and remove it from the free list
	uintptr_t alloced_id = pool->free_list_head_id;
	vui_debug_assert(!_VuiPool_is_allocated(pool, alloced_id - 1), "allocated element is in the free list of the pool");
	_VuiPool_set_allocated(pool, alloced_id - 1);
	uint32_t* alloced_elmt = (uint32_t*)vui_ptr_add(pool->data, (uintptr_t)pool->elmts_start_byte_idx + ((alloced_id - 1) * elmt_size));
	pool->free_list_head_id = *alloced_elmt;

	pool->count += 1;
	*id_out = alloced_id;
	return alloced_elmt;
}

void _VuiPool_assert_id(_VuiPool* pool, uint32_t elmt_id) {
	vui_debug_assert(elmt_id, "elmt_id is null, cannot deallocate a null element");
	vui_debug_assert(elmt_id <= pool->cap, "elmt_id is out of the memory boundary of the pool. idx is '%u' but cap is '%u'",
		elmt_id - 1, pool->cap);
	vui_debug_assert(_VuiPool_is_allocated(pool, elmt_id - 1), "cannot get pointer to a element that is not allocated");
}

void _VuiPool_dealloc(_VuiPool* pool, uint32_t elmt_id, uintptr_t elmt_size, uintptr_t elmt_align) {
	_VuiPool_assert_id(pool, elmt_id);

	uint32_t* elmt_next_free_id = &pool->free_list_head_id;
	void* elmts = vui_ptr_add(pool->data, pool->elmts_start_byte_idx);
	//
	// the free list is stored in low to high order, to try to keep allocations near eachother.
	// move up the free list until elmt_id is less than that node.
	while (1) {
		uint32_t nfi = *elmt_next_free_id;
		if (nfi > elmt_id || nfi == pool->cap) break;
		elmt_next_free_id = (uint32_t*)vui_ptr_add(elmts, ((uintptr_t)nfi - 1) * elmt_size);
	}

	_VuiPool_set_free(pool, elmt_id - 1);
	// point to the next element in the list
	*(uint32_t*)vui_ptr_add(pool->data, (uintptr_t)pool->elmts_start_byte_idx + ((uintptr_t)(elmt_id - 1) * elmt_size)) = *elmt_next_free_id;
	// get the previous element to point to this newly deallocated block
	*elmt_next_free_id = elmt_id;
	pool->count -= 1;
}

void* _VuiPool_id_to_ptr(_VuiPool* pool, VuiPoolId elmt_id, uintptr_t elmt_size) {
	_VuiPool_assert_id(pool, elmt_id);
	return vui_ptr_add(pool->data, (uintptr_t)pool->elmts_start_byte_idx + ((uintptr_t)(elmt_id - 1) * elmt_size));
}

VuiPoolId _VuiPool_ptr_to_id(_VuiPool* pool, void* ptr, uintptr_t elmt_size) {
	return (vui_ptr_diff(ptr, vui_ptr_add(pool->data, (uintptr_t)pool->elmts_start_byte_idx)) / elmt_size) + 1;
}


// ===========================================================================================
//
//
// Misc Helpers
//
//
// ===========================================================================================

noreturn void _vui_abort(const char* file, int line, const char* func, char* assert_test, char* message_fmt, ...) {
	if (assert_test) {
		fprintf(stderr, "assertion failed: %s\nmessage: ", assert_test);
	} else {
		fprintf(stderr, "abort reason: ");
	}

	va_list args;
	va_start(args, message_fmt);
	vfprintf(stderr, message_fmt, args);
	va_end(args);

	fprintf(stderr, "\nfile: %s:%d\n%s\n", file, line, func);
	abort();
}

uint32_t vui_utf8_codepoint(const char* str, int32_t* out_codepoint) {
	uint32_t bytes = 0;
	if (0xf0 == (0xf8 & str[0])) {
		// 4 byte utf8 codepoint
		*out_codepoint = ((0x07 & str[0]) << 18) | ((0x3f & str[1]) << 12) |
		((0x3f & str[2]) << 6) | (0x3f & str[3]);
		bytes = 4;
	} else if (0xe0 == (0xf0 & str[0])) {
		// 3 byte utf8 codepoint
		*out_codepoint =
		((0x0f & str[0]) << 12) | ((0x3f & str[1]) << 6) | (0x3f & str[2]);
		bytes = 3;
	} else if (0xc0 == (0xe0 & str[0])) {
		// 2 byte utf8 codepoint
		*out_codepoint = ((0x1f & str[0]) << 6) | (0x3f & str[1]);
		bytes = 2;
	} else {
		// 1 byte utf8 codepoint otherwise
		*out_codepoint = str[0];
		bytes = 1;
	}

	return bytes;
}

uint32_t vui_utf8_prev_char(const char* str, uint32_t idx) {
	while (idx--) {
		if (vui_utf8_is_codepoint_boundary(str[idx])) {
			break;
		}
	}

	if (idx == UINT32_MAX)
		idx = 0;

	return idx;
}

VuiBool vui_utf8_is_codepoint_boundary(char ch) {
	// this is bit magic equivalent to: b < 128 || b >= 192
	return ch >= -0x40;
}

VuiBool vui_utf8_is_word_delimiter(int32_t codept) {
	// all of the control characters are delimiters
	if (codept < 32) return vui_true;

	char word_delimiters[] = " ,.-!?:;";

	int32_t delimiter = 0;
	uint32_t delimiter_i = 0;
	while (delimiter_i < sizeof(word_delimiters)) {
		delimiter_i += vui_utf8_codepoint(&word_delimiters[delimiter_i], &delimiter);
		if (delimiter == codept) return vui_true;
	}
	return vui_false;
}

VuiBool vui_utf8_is_whitespace(int32_t codept) {
	char ws_codepts[] = " \t";

	int32_t ws = 0;
	uint32_t ws_i = 0;
	while (ws_i < sizeof(ws_codepts)) {
		ws_i += vui_utf8_codepoint(&ws_codepts[ws_i], &ws);
		if (ws == codept) return vui_true;
	}
	return vui_false;
}

VuiVec2 vui_cubic_bezier_curve_interp(VuiVec2 points[4], float progress) {
	VuiVec2 tmp_buf[4];
	memcpy(tmp_buf, points, sizeof(VuiVec2) * 4);

	size_t number_of_points = 4;
	while (number_of_points > 1) {
		for (size_t i = 0; i < number_of_points - 1; ++i) {
			tmp_buf[i].x = vui_lerp(tmp_buf[i + 1].x, tmp_buf[i].x, progress);
			tmp_buf[i].y = vui_lerp(tmp_buf[i + 1].y, tmp_buf[i].y, progress);

		}
		number_of_points -= 1;
	}

	return tmp_buf[0];
}

// ===========================================================================================
//
//
// Stack - LIFO (inspired by stb stretchy buffer)
// will not work for a type with an alignment greater than alignof(void*)
//
//
// ===========================================================================================

void* _VuiStk_maybe_expand(void** stk_ptr, uint32_t elmts_count, uint32_t elmt_size) {
	void* stk = *stk_ptr;
	uint32_t new_count = elmts_count;
	if (stk == NULL) goto RESIZE_CAP;

	_VuiStkHeader* h = _VuiStk_header(stk);
	new_count += h->count;

	if (new_count > h->cap) {
RESIZE_CAP: {}
		// if we have a capacity workout the new capacity by times by 2
		// or using the new_count if it is greater the double our current capacity.
		uint32_t new_cap = stk != NULL && h->cap ? h->cap * 2 : vui_stk_init_cap;
		if (new_count > new_cap)
			new_cap = new_count;

		if (!_VuiStk_resize_cap(&stk, new_cap, elmt_size)) return NULL;
	}

	*stk_ptr = stk;
	return stk;
}

void* _VuiStk_push_many(void** stk_ptr, uint32_t elmts_count, uint32_t elmt_size) {
	void* stk = _VuiStk_maybe_expand(stk_ptr, elmts_count, elmt_size);
	if (stk == NULL) return NULL;
	_VuiStkHeader* h = _VuiStk_header(stk);

	void* elmt = (void*)((char*)stk + (uintptr_t)elmt_size * (uintptr_t)h->count);
	h->count += elmts_count;
	return elmt;
}

void* _VuiStk_insert_many(void** stk_ptr, uint32_t idx, uint32_t elmts_count, uint32_t elmt_size) {
	void* stk = _VuiStk_maybe_expand(stk_ptr, elmts_count, elmt_size);
	if (stk == NULL) return NULL;
	_VuiStkHeader* h = _VuiStk_header(stk);

	void* elmt = (void*)((char*)stk + (uintptr_t)elmt_size * (uintptr_t)idx);
	// shift the elements from idx to (idx + elmts_count), to the right
	// to make room for the elements
    memmove((char*)elmt + (uintptr_t)elmts_count * (uintptr_t)elmt_size, elmt, (uintptr_t)(h->count - idx) * (uintptr_t)elmt_size);

	h->count += elmts_count;
	return elmt;
}

VuiBool _VuiStk_resize_cap(void** stk_ptr, uint32_t new_cap, uint32_t elmt_size) {
	void* stk = *stk_ptr;
	if (new_cap == 0) new_cap = vui_stk_init_cap;
	uint32_t cap = VuiStk_cap(stk);
	if (new_cap == cap) return vui_true;

	uintptr_t size = VuiStk_size(stk) + sizeof(_VuiStkHeader);
	uintptr_t new_size = (uintptr_t)elmt_size * (uintptr_t)new_cap + sizeof(_VuiStkHeader);
    _VuiStkHeader* ptr = vui_mem_realloc(_vui.allocator, stk ? _VuiStk_header(stk) : NULL, size, new_size, alignof(void*));
	if (ptr == NULL) return vui_false;

	ptr->cap = new_cap;

	// if the stk was uninitialized set its size to 0
    if (!stk) ptr->count = 0;

	// set the new stack pointer.
	*stk_ptr = (void*)((char*)ptr + sizeof(_VuiStkHeader));
	return vui_true;
}

void _VuiStk_remove_range_shift(void* stk, uint32_t start_idx, uint32_t end_idx, uint32_t elmt_size) {
	_VuiStkHeader* h = _VuiStk_header(stk);
	vui_assert(start_idx <= h->count, "start idx '%u' must be less than the count of '%u'", start_idx, h->count);
	vui_assert(end_idx <= h->count, "end idx '%u' must be less than or equal to count of '%u'", end_idx, h->count);

	uintptr_t remove_count = end_idx - start_idx;
	void* dst = ((char*)stk + (uintptr_t)start_idx * (uintptr_t)elmt_size);

	if (end_idx < h->count) {
		void* src = (char*)dst + (remove_count * (uintptr_t)elmt_size);
		memmove(dst, src, ((uintptr_t)h->count - ((uintptr_t)start_idx + remove_count)) * (uintptr_t)elmt_size);
	}
	h->count -= remove_count;
}

// ===========================================================================================
//
//
// Common Types
//
//
// ===========================================================================================

VuiVec2 VuiRect_center(const VuiRect* r) {
	return VuiVec2_add(r->left_top, VuiVec2_mul_scalar(VuiRect_size(*r), 0.5));
}

VuiRect VuiRect_clip(const VuiRect* a, const VuiRect* b) {
    if (
        a->left_top.x >= b->right_bottom.x || a->left_top.y >= b->right_bottom.y ||
        b->left_top.x >= a->right_bottom.x || b->left_top.y >= a->right_bottom.y
    ) {
        return VuiRect_zero;
    }

    return VuiRect_init_v2(VuiVec2_max(a->left_top, b->left_top), VuiVec2_min(a->right_bottom, b->right_bottom));
}

VuiVec2 VuiRect_clip_pt(const VuiRect* rect, VuiVec2 pt) {
	return VuiVec2_clamp(pt, rect->left_top, rect->right_bottom);
}

VuiBool VuiRect_intersects(const VuiRect* a, const VuiRect* b) {
    return a->left_top.x < b->right_bottom.x &&
        b->left_top.x < a->right_bottom.x &&
        a->left_top.y < b->right_bottom.y &&
        b->left_top.y < a->right_bottom.y;
}

VuiBool VuiRect_intersects_pt(const VuiRect* r, VuiVec2 pt) {
	return r->left_top.x <= pt.x && r->right_bottom.x >= pt.x &&
		r->left_top.y <= pt.y && r->right_bottom.y >= pt.y;
}

// ===========================================================================================
//
//
// Input
//
//
// ===========================================================================================

void vui_input_set_mouse_pos(float x, float y) {
    _vui.input.mouse.offset_x = x - _vui.input.mouse.x;
    _vui.input.mouse.offset_y = y - _vui.input.mouse.y;
    _vui.input.mouse.x = x;
    _vui.input.mouse.y = y;
}

void vui_input_set_mouse_wheel_offset(float wheel_offset_x, float wheel_offset_y) {
    _vui.input.mouse.wheel_offset_x = wheel_offset_x;
    _vui.input.mouse.wheel_offset_y = wheel_offset_y;
}

void vui_input_set_mouse_button_pressed(VuiMouseButtons buttons) {
    _vui.input.mouse.buttons_is_pressed |= buttons;
    _vui.input.mouse.buttons_has_been_pressed |= buttons;
}

void vui_input_set_mouse_button_released(VuiMouseButtons buttons) {
    _vui.input.mouse.buttons_is_pressed &= ~buttons;
    _vui.input.mouse.buttons_has_been_released |= buttons;
}

void vui_input_add_actions(VuiInputActions actions) {
    _vui.input.actions |= actions;
}

uint32_t _vui_string_remove_range_shift(char* string, uint32_t string_len, uint32_t start_idx, uint32_t end_idx) {
	uint32_t remove_count = end_idx - start_idx;
	if (end_idx < string_len) {
		char* dst = string + start_idx;
		void* src = string + end_idx;
		memmove(dst, src, string_len - end_idx);
	}

	string_len -= end_idx - start_idx;
	string[string_len] = '\0';
	return string_len;
}

static void _vui_input_text_remove_selected() {
	//
	// get the selection range
	uint32_t start_idx = _vui.input.focused_text_box.cursor_idx;
	uint32_t end_idx = _vui.input.focused_text_box.cursor_idx + _vui.input.focused_text_box.select_offset;
	if (_vui.input.focused_text_box.select_offset < 0) {
		uint32_t tmp = start_idx;
		start_idx = end_idx;
		end_idx = tmp;
	}

	//
	// remove the selected text from the string by doing a shift remove.
	_vui.input.focused_text_box.string_len =
		_vui_string_remove_range_shift(_vui.input.focused_text_box.string, _vui.input.focused_text_box.string_len, start_idx, end_idx);
	_vui.input.focused_text_box.cursor_idx = start_idx;
	_vui.input.focused_text_box.select_offset = 0;
	_vui.input.focused_text_box.has_cursor_moved = vui_true;
}

static void _vui_input_text_insert(const char* string, uint32_t string_length) {
	char* dst_string = _vui.input.focused_text_box.string;
	uint32_t dst_idx = _vui.input.focused_text_box.cursor_idx;
	uint32_t dst_idx_end = dst_idx + string_length;

	//
	// if the string will exceed the capacity then reduce the string length to allow for a null terminator at the end.
	if (dst_idx_end >= _vui.input.focused_text_box.string_cap) {
		uint32_t amount = (dst_idx_end - _vui.input.focused_text_box.string_cap) + 1;
		string_length -= string_length < amount ? string_length : amount;
		dst_idx_end = dst_idx + string_length;
	}

	if (string_length) {
		//
		// shift the characters to the right of the index over by string_length.
		if (dst_idx < _vui.input.focused_text_box.string_len) {
			void* src = dst_string + dst_idx;
			memmove(src + string_length, src, _vui.input.focused_text_box.string_len - dst_idx);
		}

		//
		// insert the string
		memcpy(&_vui.input.focused_text_box.string[_vui.input.focused_text_box.cursor_idx], string, string_length);
		_vui.input.focused_text_box.cursor_idx += string_length;
		_vui.input.focused_text_box.string_len += string_length;
		_vui.input.focused_text_box.string[_vui.input.focused_text_box.string_len] = '\0';
		_vui.input.focused_text_box.has_cursor_moved = vui_true;
	}
}

void vui_input_add_text(const char* string, uint32_t string_length) {
	// ignore if a text box is not focused
	if (_vui.input.focused_text_box.string == NULL) return;

	_vui.input.focused_text_box.has_changed = vui_true;
	if (_vui.input.focused_text_box.select_offset != 0) { // if we are selecting
		_vui_input_text_remove_selected();
	}

	uint32_t codept_size = 0;
	for (uint32_t i = 0; i < string_length; i += codept_size) {
		int32_t codept = 0;
		codept_size = vui_utf8_codepoint(&string[i], &codept);

		if (_vui.input.focused_text_box.string_len + codept_size >= _vui.input.focused_text_box.string_cap)
			break;

		VuiBool copy = vui_false;
		switch (_vui.input.focused_text_box.type) {
			case _VuiInputBoxType_text: copy = !_vui.input.focused_text_box.is_multiline || codept != '\n'; break;
			case _VuiInputBoxType_float:
				if (codept == '.') {
					// we have a full stop, only insert this after the first character
					// and make sure its the only decimal place in the float string.
					if (_vui.input.focused_text_box.cursor_idx > 0) {
						VuiBool has_full_stop = vui_false;
						for (uint32_t idx = 0; idx < _vui.input.focused_text_box.string_len; idx += 1) {
							if (_vui.input.focused_text_box.string[idx] == '.') {
								has_full_stop = vui_true;
								break;
							}
						}

						copy = !has_full_stop;
					}
				}
				if (copy) break;
				// fallthrough
			case _VuiInputBoxType_s32:
				if (codept == '-') {
					copy = _vui.input.focused_text_box.string_len == 0 ||
						(_vui.input.focused_text_box.cursor_idx == 0 && _vui.input.focused_text_box.string[0] != '-');
					if (copy) break;
				}
				// fallthrough
			case _VuiInputBoxType_u32:
				copy = codept >= '0' && codept <= '9';
				break;
		}

		//
		// copy the byte if it is valid for this type of input box
		if (copy) {
			_vui_input_text_insert(&string[i], codept_size);
		}
	}

	_vui.input.focused_text_box.string[_vui.input.focused_text_box.string_len] = '\0';
}

VuiVec2 vui_mouse_pos() {
	return VuiVec2_init(_vui.input.mouse.x, _vui.input.mouse.y);
}

VuiBool vui_has_mouse_over_ctrl() {
	return _vui.input.is_mouse_over_ctrl;
}

VuiBool vui_has_mouse_focused_ctrl() {
	return _vui.mouse_focused_ctrl_id != 0;
}

VuiBool vui_has_mouse_scroll_focused_ctrl() {
	return _vui.mouse_scroll_focused_ctrl_id != 0;
}

VuiBool vui_has_text_box_focused() {
	return _vui.input.focused_text_box.string != NULL;
}

VuiBool vui_ctrl_is_mouse_focused(VuiCtrlId ctrl_id) {
	return _vui.mouse_focused_ctrl_id == ctrl_id;
}

VuiBool vui_ctrl_is_focused(VuiCtrlId ctrl_id) {
	return _vui.windows[_vui.focused_window_id].focused_ctrl_id == ctrl_id;
}

VuiBool vui_ctrl_is_mouse_scroll_focused(VuiCtrlId ctrl_id) {
	return _vui.mouse_scroll_focused_ctrl_id == ctrl_id;
}

void vui_ctrl_set_focused(VuiCtrlId ctrl_id) {
	_VuiWindow* w = &_vui.windows[_vui.focused_window_id];

	//
	// remove focus from the currently focused control.
	if (w->focused_ctrl_id) {
		VuiCtrl* focused_ctrl = vui_ctrl_get(w->focused_ctrl_id);
		focused_ctrl->state_flags &= ~VuiCtrlStateFlags_focused;
	}

	//
	// if a control was supplied, then set it to be focus.
	if (ctrl_id) {
		VuiCtrl* ctrl = vui_ctrl_get(ctrl_id);
		ctrl->state_flags |= VuiCtrlStateFlags_focused;
	}

	w->focused_ctrl_id = ctrl_id;
	if (_vui.input.focused_text_box.string) {
		_vui.input.focused_text_box.string = NULL;
		_vui.input.focused_text_box.string_len = 0;
		_vui.input.focused_text_box.string_cap = 0;
	}
}

void _vui_ctrl_set_mouse_focused(VuiCtrlId ctrl_id) {
	//
	// remove focus from the currently focused control unless it is keyboard focused.
	if (_vui.mouse_focused_ctrl_id && _vui.mouse_focused_ctrl_id != _vui.windows[_vui.focused_window_id].focused_ctrl_id) {
		VuiCtrl* focused_ctrl = vui_ctrl_get(_vui.mouse_focused_ctrl_id);
		focused_ctrl->state_flags &= ~VuiCtrlStateFlags_focused;
	}

	//
	// if a control was supplied, make that the new mouse focused control
	if (ctrl_id) {
		VuiCtrl* ctrl = vui_ctrl_get(ctrl_id);
		ctrl->state_flags |= VuiCtrlStateFlags_focused;
	}

	_vui.mouse_focused_ctrl_id = ctrl_id;
}

void _vui_ctrl_set_mouse_scroll_focused(VuiCtrlId ctrl_id) {
	//
	// set the focus to the control that was requested
	_vui.mouse_scroll_focused_ctrl_id = ctrl_id;
}

VuiFocusState _vui_ctrl_focus_state(VuiCtrlId ctrl_id, VuiBool disable_keyboard_actions) {
	//
	// keyboard state
	//

	if (!disable_keyboard_actions) {
		if ((_vui.input.actions & VuiInputActions_focus_pressed) == VuiInputActions_focus_pressed) {
			if (vui_ctrl_is_focused(ctrl_id)) {
				return VuiFocusState_pressed | VuiFocusState_held | VuiFocusState_focused;
			}
		}

		if ((_vui.input.actions & VuiInputActions_focus_held) == VuiInputActions_focus_held) {
			if (vui_ctrl_is_focused(ctrl_id)) {
				return VuiFocusState_held | VuiFocusState_focused;
			}
		}

		if ((_vui.input.actions & VuiInputActions_focus_released) == VuiInputActions_focus_released) {
			if (vui_ctrl_is_focused(ctrl_id)) {
				return VuiFocusState_released | VuiFocusState_focused;
			}
		}
	}

	//
	// mouse state
	//

    VuiBool is_ctrl_mouse_focused = vui_ctrl_is_mouse_focused(ctrl_id);
    if ((_vui.input.mouse.buttons_has_been_pressed & VuiMouseButtons_left) == VuiMouseButtons_left) {
        if (is_ctrl_mouse_focused) {
            vui_ctrl_set_focused(ctrl_id);
			if ((_vui.input.mouse.buttons_has_been_released & VuiMouseButtons_left) == VuiMouseButtons_left) {
				return VuiFocusState_pressed | VuiFocusState_held | VuiFocusState_released | VuiFocusState_focused;
			}
			return VuiFocusState_pressed | VuiFocusState_held | VuiFocusState_focused;
        } else {
            if (vui_ctrl_is_focused(ctrl_id)) {
                vui_ctrl_set_focused(0);
            }
        }
    }

    if ((_vui.input.mouse.buttons_is_pressed & VuiMouseButtons_left) == VuiMouseButtons_left) {
        if (vui_ctrl_is_focused(ctrl_id)) {
			return VuiFocusState_held | VuiFocusState_focused;
        }
    }

    if ((_vui.input.mouse.buttons_has_been_released & VuiMouseButtons_left) == VuiMouseButtons_left) {
        if (vui_ctrl_is_mouse_focused(ctrl_id)) {
            return VuiFocusState_released | VuiFocusState_focused;
        }
    }

    if (is_ctrl_mouse_focused) {
        return VuiFocusState_focused;
    } else {
        return VuiFocusState_none;
    }
}

// ===========================================================================================
//
//
// Control & Style Types & Funcions
//
//
// ===========================================================================================

VuiStyleSheet vui_ss = {
	.image = {
		[VuiCtrlState_default] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
		},
		[VuiCtrlState_focused] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
		},
		[VuiCtrlState_active] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
		},
		[VuiCtrlState_disabled] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
		},
	},
	.box_panel = {
		[VuiCtrlState_default] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
			.bg_color = vui_color_midnight_blue,
			.border_color = vui_color_wet_asphalt,
			.border_width = vui_border_width_default,
			.radius = vui_radius_default,
		},
		[VuiCtrlState_focused] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
			.bg_color = vui_color_midnight_blue,
			.border_color = vui_color_wet_asphalt,
			.border_width = vui_border_width_default,
			.radius = vui_radius_default,
		},
		[VuiCtrlState_active] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
			.bg_color = vui_color_midnight_blue,
			.border_color = vui_color_wet_asphalt,
			.border_width = vui_border_width_default,
			.radius = vui_radius_default,
		},
		[VuiCtrlState_disabled] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
			.bg_color = vui_color_midnight_blue,
			.border_color = vui_color_wet_asphalt,
			.border_width = vui_border_width_default,
			.radius = vui_radius_default,
		},
	},
	.text_header = {
		[VuiCtrlState_default] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
			.text_color = vui_color_white,
			.text_line_height = vui_line_height_header,
			.font_id = 0,
		},
		[VuiCtrlState_focused] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
			.text_color = vui_color_white,
			.text_line_height = vui_line_height_header,
			.font_id = 0,
		},
		[VuiCtrlState_active] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
			.text_color = vui_color_white,
			.text_line_height = vui_line_height_header,
			.font_id = 0,
		},
		[VuiCtrlState_disabled] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
			.text_color = vui_color_white,
			.text_line_height = vui_line_height_header,
			.font_id = 0,
		},
	},
	.text_menu = {
		[VuiCtrlState_default] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
			.text_color = vui_color_white,
			.text_line_height = vui_line_height_menu,
			.font_id = 0,
		},
		[VuiCtrlState_focused] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
			.text_color = vui_color_white,
			.text_line_height = vui_line_height_menu,
			.font_id = 0,
		},
		[VuiCtrlState_active] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
			.text_color = vui_color_white,
			.text_line_height = vui_line_height_menu,
			.font_id = 0,
		},
		[VuiCtrlState_disabled] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
			.text_color = vui_color_gray,
			.text_line_height = vui_line_height_menu,
			.font_id = 0,
		},
	},
	.separator = {
		[VuiCtrlState_default] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
			.bg_color = vui_color_gray,
			.radius = vui_radius_default,
			.separator_size = vui_separator_size_default,
		},
	},
	.button_action = {
		[VuiCtrlState_default] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
			.bg_color = vui_color_midnight_blue,
			.border_color = vui_color_asbestos,
			.border_width = vui_border_width_default,
			.radius = vui_radius_default,
			.text_styles = vui_ss.text_menu,
			.image_styles = vui_ss.image,
		},
		[VuiCtrlState_focused] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
			.bg_color = vui_color_wet_asphalt,
			.border_color = vui_color_concrete,
			.border_width = vui_border_width_default,
			.radius = vui_radius_default,
			.text_styles = vui_ss.text_menu,
			.image_styles = vui_ss.image,
		},
		[VuiCtrlState_active] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
			.bg_color = vui_color_concrete,
			.border_color = vui_color_wet_asphalt,
			.border_width = vui_border_width_default,
			.radius = vui_radius_default,
			.text_styles = vui_ss.text_menu,
			.image_styles = vui_ss.image,
		},
		[VuiCtrlState_disabled] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
			.bg_color = vui_color_wet_asphalt,
			.border_color = vui_color_midnight_blue,
			.border_width = vui_border_width_default,
			.radius = vui_radius_default,
			.text_styles = vui_ss.text_menu,
			.image_styles = vui_ss.image,
		},
	},
	.check_box = {
		[VuiCtrlState_default] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
			.bg_color = vui_color_midnight_blue,
			.border_color = vui_color_asbestos,
			.border_width = vui_border_width_default,
			.radius = vui_radius_default,
			.text_styles = vui_ss.text_menu,
			.image_styles = vui_ss.image,
			.check_color = vui_color_amethyst,
			.check_box_size = vui_check_box_size_default,
			.check_size = 0.f,
		},
		[VuiCtrlState_focused] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
			.bg_color = vui_color_wet_asphalt,
			.border_color = vui_color_concrete,
			.border_width = vui_border_width_default,
			.radius = vui_radius_default,
			.text_styles = vui_ss.text_menu,
			.image_styles = vui_ss.image,
			.check_color = vui_color_amethyst,
			.check_box_size = vui_check_box_size_default,
			.check_size = vui_check_size_focused_default,
		},
		[VuiCtrlState_active] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
			.bg_color = vui_color_wet_asphalt,
			.border_color = vui_color_concrete,
			.border_width = vui_border_width_default,
			.radius = vui_radius_default,
			.text_styles = vui_ss.text_menu,
			.image_styles = vui_ss.image,
			.check_color = vui_color_amethyst,
			.check_box_size = vui_check_box_size_default,
			.check_size = vui_check_size_active_default,
		},
		[VuiCtrlState_disabled] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
			.bg_color = vui_color_wet_asphalt,
			.border_color = vui_color_midnight_blue,
			.border_width = vui_border_width_default,
			.radius = vui_radius_default,
			.text_styles = vui_ss.text_menu,
			.image_styles = vui_ss.image,
			.check_color = vui_color_amethyst,
			.check_box_size = vui_check_box_size_default,
			.check_size = 0.f,
		},
	},
	.radio_button = {
		[VuiCtrlState_default] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
			.bg_color = vui_color_midnight_blue,
			.border_color = vui_color_asbestos,
			.border_width = vui_border_width_default,
			.radius = 100.f,
			.text_styles = vui_ss.text_menu,
			.image_styles = vui_ss.image,
			.check_color = vui_color_amethyst,
			.check_box_size = vui_check_box_size_default,
			.check_size = 0.f,
		},
		[VuiCtrlState_focused] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
			.bg_color = vui_color_wet_asphalt,
			.border_color = vui_color_concrete,
			.border_width = vui_border_width_default,
			.radius = 100.f,
			.text_styles = vui_ss.text_menu,
			.image_styles = vui_ss.image,
			.check_color = vui_color_amethyst,
			.check_box_size = vui_check_box_size_default,
			.check_size = vui_check_size_focused_default,
		},
		[VuiCtrlState_active] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
			.bg_color = vui_color_wet_asphalt,
			.border_color = vui_color_concrete,
			.border_width = vui_border_width_default,
			.radius = 100.f,
			.text_styles = vui_ss.text_menu,
			.image_styles = vui_ss.image,
			.check_color = vui_color_amethyst,
			.check_box_size = vui_check_box_size_default,
			.check_size = vui_check_size_active_default,
		},
		[VuiCtrlState_disabled] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
			.bg_color = vui_color_wet_asphalt,
			.border_color = vui_color_midnight_blue,
			.border_width = vui_border_width_default,
			.radius = 100.f,
			.text_styles = vui_ss.text_menu,
			.image_styles = vui_ss.image,
			.check_color = vui_color_amethyst,
			.check_box_size = vui_check_box_size_default,
			.check_size = 0.f,
		},
	},
	.text_box = {
		[VuiCtrlState_default] = {
			.margin = vui_margin_default,
			.padding = {0},
			.bg_color = vui_color_midnight_blue,
			.border_color = vui_color_asbestos,
			.border_width = vui_border_width_default,
			.radius = vui_radius_default,
			.bar_styles = vui_ss.scroll_bar,
			.text_styles = vui_ss.text_menu,
			.selection_color = VuiColor_init(0x34, 0x98, 0xdb, 0x80),
			.cursor_color = vui_color_amethyst,
			.cursor_width = vui_cursor_width_default,
		},
		[VuiCtrlState_focused] = {
			.margin = vui_margin_default,
			.padding = {0},
			.bg_color = vui_color_wet_asphalt,
			.border_color = vui_color_concrete,
			.border_width = vui_border_width_default,
			.radius = vui_radius_default,
			.bar_styles = vui_ss.scroll_bar,
			.text_styles = vui_ss.text_menu,
			.selection_color = VuiColor_init(0x34, 0x98, 0xdb, 0x80),
			.cursor_color = vui_color_amethyst,
			.cursor_width = vui_cursor_width_default,
		},
		[VuiCtrlState_active] = {
			.margin = vui_margin_default,
			.padding = {0},
			.bg_color = vui_color_wet_asphalt,
			.border_color = vui_color_concrete,
			.border_width = vui_border_width_default,
			.radius = vui_radius_default,
			.bar_styles = vui_ss.scroll_bar,
			.text_styles = vui_ss.text_menu,
			.selection_color = VuiColor_init(0x34, 0x98, 0xdb, 0x80),
			.cursor_color = vui_color_amethyst,
			.cursor_width = vui_cursor_width_default,
		},
		[VuiCtrlState_disabled] = {
			.margin = vui_margin_default,
			.padding = {0},
			.bg_color = vui_color_wet_asphalt,
			.border_color = vui_color_midnight_blue,
			.border_width = vui_border_width_default,
			.radius = vui_radius_default,
			.bar_styles = vui_ss.scroll_bar,
			.text_styles = vui_ss.text_menu,
			.selection_color = VuiColor_init(0x34, 0x98, 0xdb, 0x80),
			.cursor_color = vui_color_amethyst,
			.cursor_width = vui_cursor_width_default,
		},
	},
	.slider_bar = {
		[VuiCtrlState_default] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
			.bg_color = vui_color_midnight_blue,
			.border_color = vui_color_concrete,
			.border_width = vui_border_width_default,
			.radius = vui_radius_default,
		},
	},
	.slider = {
		[VuiCtrlState_default] = {
			.margin = vui_margin_default,
			.bar_styles = vui_ss.slider_bar,
			.button_styles = vui_ss.button_action,
			.bar_height = vui_slider_bar_height_default,
			.button_width = vui_slider_button_width_default,
		},
		[VuiCtrlState_focused] = {
			.margin = vui_margin_default,
			.bar_styles = vui_ss.slider_bar,
			.button_styles = vui_ss.button_action,
			.bar_height = vui_slider_bar_height_default,
			.button_width = vui_slider_button_width_default,
		},
		[VuiCtrlState_active] = {
			.margin = vui_margin_default,
			.bar_styles = vui_ss.slider_bar,
			.button_styles = vui_ss.button_action,
			.bar_height = vui_slider_bar_height_default,
			.button_width = vui_slider_button_width_default,
		},
		[VuiCtrlState_disabled] = {
			.margin = vui_margin_default,
			.bar_styles = vui_ss.slider_bar,
			.button_styles = vui_ss.button_action,
			.bar_height = vui_slider_bar_height_default,
			.button_width = vui_slider_button_width_default,
		},
	},
	.progress_bar = {
		[VuiCtrlState_default] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
			.bg_color = vui_color_midnight_blue,
			.border_color = vui_color_concrete,
			.border_width = vui_border_width_default,
			.radius = vui_radius_default,
			.bar_color = vui_color_wisteria,
		},
	},
	.scroll_view = {
		[VuiCtrlState_default] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
			.bg_color = vui_color_midnight_blue,
			.border_color = vui_color_wet_asphalt,
			.border_width = vui_border_width_default,
			.radius = vui_radius_default,
			.bar_styles = vui_ss.scroll_bar,
		},
	},
	.scroll_bar = {
		[VuiCtrlState_default] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
			.bg_color = vui_color_midnight_blue,
			.border_color = vui_color_wet_asphalt,
			.border_width = vui_border_width_default,
			.radius = vui_radius_default,
			.slider_styles = vui_ss.scroll_bar_slider,
			.slider_width = vui_scroll_bar_width_default,
		},
	},
	.scroll_bar_slider = {
		[VuiCtrlState_default] = {
			.margin = {0},
			.padding = {0},
			.bg_color = vui_color_pumpkin,
			.border_color = vui_color_carrot,
			.border_width = vui_border_width_default,
			.radius = vui_radius_default,
		},
		[VuiCtrlState_focused] = {
			.margin = {0},
			.padding = {0},
			.bg_color = vui_color_carrot,
			.border_color = vui_color_orange,
			.border_width = vui_border_width_default,
			.radius = vui_radius_default,
		},
		[VuiCtrlState_active] = {
			.margin = {0},
			.padding = {0},
			.bg_color = vui_color_orange,
			.border_color = vui_color_carrot,
			.border_width = vui_border_width_default,
			.radius = vui_radius_default,
		},
		[VuiCtrlState_disabled] = {
			.margin = {0},
			.padding = {0},
			.bg_color = vui_color_wet_asphalt,
			.border_color = vui_color_midnight_blue,
			.border_width = vui_border_width_default,
			.radius = vui_radius_default,
		},
	},
	.popover = {
		[VuiCtrlState_default] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
			.bg_color = vui_color_midnight_blue,
			.border_color = vui_color_wet_asphalt,
			.border_width = vui_border_width_default,
			.radius = vui_radius_default,
		},
	},
};

// ===========================================================================================
//
//
// Render
//
//
// ===========================================================================================

void vui_render_line(VuiVec2 start_pos, VuiVec2 end_pos, VuiColor color, float width) {
	vui_path_plot_point(start_pos);
	vui_path_plot_point(end_pos);
	vui_render_path_stroked(color, width, vui_false);
}

void vui_render_rect(const VuiRect* rect, VuiColor color, float radius) {
	vui_path_plot_rect(rect, radius);
	vui_render_path_filled_convex(color);
}

void vui_render_rect_border(const VuiRect* rect, VuiColor color, float radius, float width) {
	float half_width = width * 0.5;
	VuiRect border_rect;
	border_rect.left = rect->left + half_width;
	border_rect.top = rect->top + half_width;
	border_rect.bottom = rect->bottom - half_width;
	border_rect.right = rect->right - half_width;
	vui_path_plot_rect(&border_rect, radius);
	vui_render_path_stroked(color, width, vui_true);
}

void vui_render_triangle(VuiVec2 a, VuiVec2 b, VuiVec2 c, VuiColor color) {
	vui_path_plot_point(a);
	vui_path_plot_point(b);
	vui_path_plot_point(c);
	vui_render_path_filled_convex(color);
}

void vui_render_triangle_border(VuiVec2 a, VuiVec2 b, VuiVec2 c, VuiColor color, float width) {
	vui_path_plot_point(a);
	vui_path_plot_point(b);
	vui_path_plot_point(c);
	vui_render_path_stroked(color, width, vui_true);
}

void vui_render_circle(VuiVec2 pos, float radius, VuiColor color) {
	vui_path_plot_circle(pos, radius, vui_false);
	vui_render_path_filled_convex(color);
}

void vui_render_circle_border(VuiVec2 pos, float radius, VuiColor color, float width) {
	vui_path_plot_circle(pos, radius, vui_true);
	vui_render_path_stroked(color, width, vui_true);
}

static VuiColor _vui_render_glyph_color = {0};
void _vui_render_glyph(const VuiRect* rect, VuiTextureId glyph_texture_id, const VuiRect* uv_rect) {
	vui_render_image_(rect, 0.f, 0.f, glyph_texture_id, *uv_rect, _vui_render_glyph_color, VuiImageScaleMode_stretch, vui_true, 1.0);
}

void vui_render_text(VuiVec2 left_top, VuiFontId font_id, float line_height, char* text, uint32_t text_length, VuiColor color, float word_wrap_at_width) {
	if (text_length) {
		_vui_render_glyph_color = color;
		VuiPositionTextArgs args = {0};
		args.userdata = _vui.position_text_userdata;
		args.font_id = font_id;
		args.line_height = line_height;
		args.text = text;
		args.text_length = text_length;
		args.word_wrap_at_width = word_wrap_at_width;
		args.top_left = left_top;
		args.render_glyph_fn = _vui_render_glyph;
		_vui.position_text_fn(&args);
	}
}

void vui_render_polyline(VuiVec2* points, uint32_t points_count, VuiColor color, float width, VuiBool connect_first_and_last) {
	vui_assert(points_count >= 2, "path must have atleast 2 points to render a polyline");

	uint32_t points_end = points_count;
	if (!connect_first_and_last) {
		points_end -= 1;
	}

	float half_width = width / 2.f;

	VuiVec2 start = points[0];
	VuiVec2 prev_vec = VuiVec2_zero;
	VuiVec2 rect_points[4];
	for (uint32_t idx = 0; idx < points_end; idx += 1) {
		VuiVec2 end = points[idx == points_count - 1 ? 0 : idx + 1];

		//
		// get the direction vector of the line.
		// then get both perpendicular offset vectors.
		VuiVec2 vec = VuiVec2_norm(VuiVec2_sub(end, start));
		VuiVec2 vec_left = VuiVec2_mul_scalar(VuiVec2_perp_left(vec), half_width);
		VuiVec2 vec_right = VuiVec2_mul_scalar(VuiVec2_perp_right(vec), half_width);

		if ((connect_first_and_last || idx > 0) && points_count > 2) {
			if (idx == 0) {
				prev_vec = VuiVec2_norm(VuiVec2_sub(points[0], points[points_count - 1]));
			}
			//
			// render a connecting quad piece to fill the gap inbetween two lines.
			// we do this by getting the middle vector of the gap.
			//
			//
			//                   / middle_vec
			//                  /
			//                 /
			// prev_vec       /
			// ------------> /
			//               ^
			//               |
			//               |
			//               | inv_vec
			//               |
			//               |
			//
			//
			VuiVec2 inv_vec = VuiVec2_init(-vec.x, -vec.y);
			VuiVec2 middle_vec = VuiVec2_add(inv_vec, prev_vec);
			if (VuiVec2_dot(vec, prev_vec) > 0.5f) {
				//
				//
				// normalize if the current line is within a 90 degree view of the previous line
				//
				//
				// corner piece is draw here
				//                   |
				//                   |
				//                   |      /
				//                   |     /
				//                   |    / upper 45 degree
				//                   |   /
				//                   |  /
				// previous line     v /
				// -----------------> /  if the line is within this cone, then this code will exectue
				//                    \
				//                     \
				//                      \
				//                       \
				//                        \ lower 45 degree
				//                         \
				//
				//
				middle_vec = VuiVec2_norm(middle_vec);
			}

			VuiRenderLayer* layer = &_vui.render.w->render_layers[_vui.render.layer_idx];
			uint32_t vertices_count = VuiStk_count(layer->verts);

			//
			// multiply cross to see which side of the line to place the corner piece
			//
			float mc = VuiVec2_mul_cross_vec(prev_vec, inv_vec);
			VuiVec2 v1_dir;
			uint32_t v1_idx_from_prev_line;
			uint32_t v3_idx_from_next_line;
			if (mc < 0.f) {
				v1_dir = VuiVec2_perp_left(prev_vec);

				// reuse the vertex made with rect_points[1] from the previous line
				v1_idx_from_prev_line = vertices_count - 3;

				// reuse the vertex made with rect_points[0] from the next line (add an extra 2 here is to skip over the two new vertices below)
				v3_idx_from_next_line = vertices_count + 2;
			} else {
				v1_dir = VuiVec2_perp_right(prev_vec);

				// reuse the vertex made with rect_points[2] from the previous line
				v1_idx_from_prev_line = vertices_count - 2;

				// reuse the vertex made with rect_points[3] from the next line (add an extra 2 here is to skip over the two new vertices below)
				v3_idx_from_next_line = vertices_count + 5;
			}

			if (idx == 0) {
				// if connect_first_and_last and on the first line.
				// then move the v1_idx_from_prev_line to point to the last line that will be main in the future.
				v1_idx_from_prev_line += points_count * 4 + (points_count - 1) * 2;
			}

			//
			// use the dot product to see how much to extend out the middle vector.
			float d = VuiVec2_dot(middle_vec, v1_dir);

			//
			// now create the two new vertices; one the extend middle point and the other for where the two lines meet.
			VuiRenderWriter w = vui_render_get_writer(0, 2, 6);
			vui_ensure_alloc_ok(w.verts);

			w.verts[0] = (VuiVertex) {
				.pos = VuiRect_clip_pt(&_vui.render.clip_rect, VuiVec2_add(start, VuiVec2_mul_scalar(middle_vec, half_width + half_width * (1.0 - d)))),
				.uv = VuiVec2_zero,
				.color = color
			};
			w.verts[1] = (VuiVertex) { .pos = VuiRect_clip_pt(&_vui.render.clip_rect, start), .uv = VuiVec2_zero, .color = color };

			//
			// now create the indices that for the corner piece quad.
			w.indices[0] = w.verts_start_idx;
			w.indices[1] = v1_idx_from_prev_line;
			w.indices[2] = w.verts_start_idx + 1;
			w.indices[3] = w.verts_start_idx + 1;
			w.indices[4] = v3_idx_from_next_line;
			w.indices[5] = w.verts_start_idx;
		}


		//
		// plot a rectangle using the perpendicular offset vectors,
		// to create a line with the specified width.
		//

		rect_points[0] = VuiVec2_add(start, vec_left);
		rect_points[1] = VuiVec2_add(end, vec_left);
		rect_points[2] = VuiVec2_add(end, vec_right);
		rect_points[3] = VuiVec2_add(start, vec_right);

		vui_render_convex_polygon(rect_points, 4, color);

		start = end;
		prev_vec = vec;
	}
}

void vui_render_convex_polygon(VuiVec2* points, uint32_t points_count, VuiColor color) {
	vui_assert(points_count >= 3, "path must have atleast 3 points to be filled");
	uint32_t indices_count = (points_count - 2) * 3;

	VuiRenderWriter w = vui_render_get_writer(0, points_count, indices_count);
	vui_ensure_alloc_ok(w.verts);

	for (uint32_t idx = 0; idx < points_count; idx += 1) {
		VuiVertex* vert = &w.verts[idx];
		*vert = VuiVertex_init(VuiRect_clip_pt(&_vui.render.clip_rect, points[idx]), VuiVec2_zero, color);
	}


	// draw triangles like OpenGLs triangle fan.
	uint32_t idx = 0;
	for (uint32_t vert_idx = 1; vert_idx < points_count - 1; vert_idx += 1) {
		w.indices[idx] = w.verts_start_idx;
		w.indices[idx + 1] = w.verts_start_idx + vert_idx;
		w.indices[idx + 2] = w.verts_start_idx + vert_idx + 1;
		idx += 3;
	}
}

void vui_render_bezier_curve(VuiVec2 points[4], VuiColor color, float width) {
	float step = 0.01;

	//
	// create all the points from the start and just before the end
	for (float progress = 0.0f; progress < 1.0f; progress += step) {
		VuiVec2 point = vui_cubic_bezier_curve_interp(points, progress);
		vui_path_plot_point(point);
	}

	//
	// now finish the curve by plotting the last point dead at the end.
	VuiVec2 point = vui_cubic_bezier_curve_interp(points, 1.0);
	vui_path_plot_point(point);

	vui_render_path_stroked(color, width, vui_false);
}

void vui_render_image(const VuiRect* rect, VuiImageId image_id, VuiColor image_tint, VuiImageScaleMode scale_mode, float scale) {
	VuiImage* image = vui_image_get(image_id);
	vui_render_image_(rect, image->width, image->height, image->texture_id, image->uv_rect, image_tint, scale_mode, vui_false, scale);
}

void vui_render_image_(const VuiRect* rect_ptr, float image_width, float image_height, VuiTextureId texture_id, VuiRect uv_rect, VuiColor color, VuiImageScaleMode scale_mode, VuiBool is_glyph, float scale) {
	VuiRenderWriter w = vui_render_get_writer(texture_id, 4, 6);
	vui_ensure_alloc_ok(w.verts);

	VuiRect rect = *rect_ptr;
	switch (scale_mode) {
		case VuiImageScaleMode_stretch: {
			// do nothing, the size of the rectangle will stretch the image by default.
			break;
		};
		case VuiImageScaleMode_uniform:
		case VuiImageScaleMode_uniform_crop: {
			float width = VuiRect_width(&rect);
			float height = VuiRect_height(&rect);
			VuiBool cond = image_width > image_height;
			if (scale_mode == VuiImageScaleMode_uniform_crop) {
				cond = !cond;
			}

			if (cond) {
				//
				// width is larger than the height. (unless uniform_crop then this is the opposite way round)
				// so the width will take up the whole width of the rectangle passed in.
				// so scale the height of the image to maintain the aspect ratio.
				float image_aspect_ratio = image_height / image_width;
				float scale_factor_width = width / image_width;
				float scale_factor_height = scale_factor_width * image_aspect_ratio;
				rect.bottom = rect.top + height * scale_factor_height;
			} else {
				//
				// height is larger than the width. (unless uniform_crop then this is the opposite way round)
				// so the height will take up the whole height of the rectangle passed in.
				// so scale the width of the image to maintain the aspect ratio.
				float image_aspect_ratio = image_width / image_height;
				float scale_factor_height = height / image_height;
				float scale_factor_width = scale_factor_height * image_aspect_ratio;
				rect.right = rect.left + width * scale_factor_width;
			}
			break;
		};
		case VuiImageScaleMode_none:
			rect.right = rect.left + image_width;
			rect.bottom = rect.top + image_height;
			break;
	}

	//
	// apply the scale that is center aligned.
	//
	if (scale != 1.0) {
		float width = VuiRect_width(&rect);
		float height = VuiRect_height(&rect);
		float diff_x = ((width * scale) - width) * 0.5f;
		float diff_y = ((height * scale) - height) * 0.5f;
		rect.left -= diff_x;
		rect.right += diff_x;
		rect.top -= diff_y;
		rect.bottom += diff_y;
	}

	VuiRect clipped_rect = VuiRect_clip(&rect, &_vui.render.clip_rect);
	{
		//
		// clip the side of the uv coordiates by the same ratio the rectangle got clipped
		float width = rect.right_bottom.x - rect.left_top.x;
		float uv_width = uv_rect.right_bottom.x - uv_rect.left_top.x;
		float w_ratio = uv_width / width;

		float height = rect.right_bottom.y - rect.left_top.y;
		float uv_height = uv_rect.right_bottom.y - uv_rect.left_top.y;
		float h_ratio = uv_height / height;

		// left
		float diff = clipped_rect.left_top.x - rect.left_top.x;
		if (diff > 0.0) uv_rect.left_top.x += diff * w_ratio;

		// right
		diff = rect.right_bottom.x - clipped_rect.right_bottom.x;
		if (diff > 0.0) uv_rect.right_bottom.x -= diff * w_ratio;

		// top
		diff = clipped_rect.left_top.y - rect.left_top.y;
		if (diff > 0.0) uv_rect.left_top.y += diff * h_ratio;

		// bottom
		diff = rect.right_bottom.y - clipped_rect.right_bottom.y;
		if (diff > 0.0) uv_rect.right_bottom.y -= diff * h_ratio;
	}

	if (is_glyph) {
		uv_rect.left_top.x = -uv_rect.left_top.x;
		uv_rect.left_top.y = -uv_rect.left_top.y;
		uv_rect.right_bottom.x = -uv_rect.right_bottom.x;
		uv_rect.right_bottom.y = -uv_rect.right_bottom.y;
	}

	w.verts[0] = VuiVertex_init(clipped_rect.left_top, uv_rect.left_top, color);

	w.verts[1] = VuiVertex_init(
		VuiVec2_init(clipped_rect.right_bottom.x, clipped_rect.left_top.y),
		VuiVec2_init(uv_rect.right_bottom.x, uv_rect.left_top.y),
		color);

	w.verts[2] = VuiVertex_init(clipped_rect.right_bottom, uv_rect.right_bottom, color);

	w.verts[3] = VuiVertex_init(
		VuiVec2_init(clipped_rect.left_top.x, clipped_rect.right_bottom.y),
		VuiVec2_init(uv_rect.left_top.x, uv_rect.right_bottom.y),
		color);

	w.indices[0] = w.verts_start_idx;
	w.indices[1] = w.verts_start_idx + 1;
	w.indices[2] = w.verts_start_idx + 2;
	w.indices[3] = w.verts_start_idx + 2;
	w.indices[4] = w.verts_start_idx + 3;
	w.indices[5] = w.verts_start_idx;
}

void vui_path_reset() {
	VuiStk_clear(_vui.render.path_points);
}

void vui_path_plot_point(VuiVec2 pt) {
	VuiVec2* t = VuiStk_push(&_vui.render.path_points);
	vui_ensure_alloc_ok(t);
	*t = pt;
}

void vui_path_plot_arc(VuiVec2 pt, float radius, float angle_start, float angle_end, uint32_t segments_count) {
	uint32_t points_count = segments_count + 1;
	VuiVec2* points = VuiStk_push_many(&_vui.render.path_points, points_count);
	vui_ensure_alloc_ok(points);

	float angle_step = (angle_end - angle_start) / points_count;
	float angle = angle_start;
	for (uint32_t idx = 0; idx < points_count; idx += 1) {
		points[idx] = VuiVec2_init(pt.x + (cosf(angle) * radius), pt.y + (-sinf(angle) * radius));
		angle += angle_step;
	}
}

void vui_path_plot_bezier_curve(VuiVec2 start_control_pt, VuiVec2 end_control_pt, VuiVec2 end_pt) {
	vui_assert(vui_false, "unimplemented: vui_path_plot_bezier_curve");
}

uint32_t vui_calc_circle_segments_count(float radius) {
	uint32_t min = 12;
	uint32_t max = 512;
	return vui_clamp((uint32_t)(M_PI * 2.0f / acosf((radius - max) / radius)), min, max);
}

void vui_path_plot_rect(const VuiRect* rect, float radius) {
	float w = vui_max(VuiRect_width(rect), 0.f) / 2.f;
	float h = vui_max(VuiRect_height(rect), 0.f) / 2.f;
	radius = vui_min(radius, vui_min(w, h));

	if (radius == 0.f) {
		vui_path_plot_point(rect->left_top);
		vui_path_plot_point(VuiRect_right_top(*rect));
		vui_path_plot_point(rect->right_bottom);
		vui_path_plot_point(VuiRect_left_bottom(*rect));
	} else {
		float top = M_PI / 2.f;
		float right = M_PI * 2.f;
		float bottom = (M_PI * 3.f) / 2.f;
		float left = M_PI;
		uint32_t segments_count = vui_calc_circle_segments_count(radius);

		vui_path_plot_arc(VuiVec2_init(rect->left_top.x + radius, rect->left_top.y + radius), radius, left, top, segments_count);
		vui_path_plot_arc(VuiVec2_init(rect->right_bottom.x - radius, rect->left_top.y + radius), radius, top, 0.0, segments_count);
		vui_path_plot_arc(VuiVec2_init(rect->right_bottom.x - radius, rect->right_bottom.y - radius), radius, right, bottom, segments_count);
		vui_path_plot_arc(VuiVec2_init(rect->left_top.x + radius, rect->right_bottom.y - radius), radius, bottom, left, segments_count);
	}
}

void vui_path_plot_circle(VuiVec2 pt, float radius, VuiBool is_border) {
	uint32_t segments_count = vui_calc_circle_segments_count(radius);
	if (is_border) segments_count *= 2;
	float angle_end = M_PI * 2.0f;
	vui_path_plot_arc(pt, radius, 0.0f, angle_end, segments_count);
}

void vui_render_path_stroked(VuiColor color, float width, VuiBool connect_first_and_last) {
	vui_render_polyline(_vui.render.path_points, VuiStk_count(_vui.render.path_points), color, width, connect_first_and_last);
	vui_path_reset();
}

void vui_render_path_filled_convex(VuiColor color) {
	vui_render_convex_polygon(_vui.render.path_points, VuiStk_count(_vui.render.path_points), color);
	vui_path_reset();
}

VuiRenderWriter vui_render_get_writer(VuiTextureId texture_id, uint32_t verts_count, uint32_t indices_count) {
	_VuiWindow* window = _vui.render.w;
	VuiRenderLayer* layer = &window->render_layers[_vui.render.layer_idx];

	//
	// here we create a new command if any of these are true:
	// - there are no commands in the layer
	// - the texture_id does not match
	//
	VuiRenderCmd* cmd = VuiStk_count(layer->cmds) == 0 ? NULL : &VuiStk_last(layer->cmds);
	if (!cmd || cmd->texture_id != texture_id) {
		cmd = VuiStk_push(&layer->cmds);
		vui_ensure_alloc_ok(cmd, (VuiRenderWriter){0});
		cmd->texture_id = texture_id;
		cmd->verts_start_idx = VuiStk_count(layer->verts);
		cmd->indices_start_idx = VuiStk_count(layer->indices);
		cmd->indices_count = 0;
	}

	cmd->indices_count += indices_count;

	VuiRenderWriter w = {0};
	w.verts_start_idx = VuiStk_count(layer->verts);
	w.verts = VuiStk_push_many(&layer->verts, verts_count);
	vui_ensure_alloc_ok(w.verts, (VuiRenderWriter){0});
	w.indices = VuiStk_push_many(&layer->indices, indices_count);
	vui_ensure_alloc_ok(w.indices, (VuiRenderWriter){0});
	return w;
}

void vui_render_inc_layer() {
	_vui.render.layer_idx += 1;
	_VuiWindow* window = _vui.render.w;
	if (_vui.render.layer_idx == VuiStk_count(window->render_layers)) {
		VuiRenderLayer* layer = VuiStk_push(&window->render_layers);
		vui_ensure_alloc_ok(layer);
		*layer = (VuiRenderLayer){0};
	}
}

void vui_render_dec_layer() {
	vui_assert(_vui.render.layer_idx > 0, "cannot decrement layer when we are already on layer 0");
	_vui.render.layer_idx -= 1;
}

// ====================================================================================
//
//
// Controls: the immediate mode API
//
//
// ====================================================================================

void _vui_assert_layout_change() {
	vui_assert(_vui.build.sibling_prev_ctrl_id == 0, "cannot change the layout when the control already has a child");
}

void vui_stack_layout() {
	_vui_assert_layout_change();
	vui_ctrl_get(_vui.build.parent_ctrl_id)->layout_type = VuiLayoutType_stack;
}

void vui_column_layout() {
	_vui_assert_layout_change();
	vui_ctrl_get(_vui.build.parent_ctrl_id)->layout_type = VuiLayoutType_column;
}

void vui_row_layout() {
	_vui_assert_layout_change();
	vui_ctrl_get(_vui.build.parent_ctrl_id)->layout_type = VuiLayoutType_row;
}

#define vui_fnv_hash_32_initial 0x811c9dc5
#define vui_fnv_hash_64_initial 0xcbf29ce484222325

uint32_t vui_fnv_hash_32(char* bytes, uint32_t byte_count, uint32_t hash) {
	char* bytes_end = bytes + byte_count;
	while (bytes < bytes_end) {
		hash = hash ^ *bytes;
		hash = hash * 0x01000193;
		bytes += 1;
	}
	return hash;
}

uint64_t vui_fnv_hash_64(char* bytes, uint32_t byte_count, uint64_t hash) {
	char* bytes_end = bytes + byte_count;
	while (bytes < bytes_end) {
		hash = hash ^ *bytes;
		hash = hash * 0x00000100000001B3;
		bytes += 1;
	}
	return hash;
}

#define _VuiCtrlId_pool_id_MASK  0x000fffff
#define _VuiCtrlId_pool_id_SHIFT 0
#define _VuiCtrlId_counter_MASK  0xfff00000
#define _VuiCtrlId_counter_SHIFT 20

VuiCtrl* _vui_ctrl_alloc(VuiCtrlId* id_out) {
	VuiPoolId pool_id = 0;
	_VuiCtrl* ctrl = _VuiPool_alloc((_VuiPool*)&_vui.ctrl_pool, &pool_id, sizeof(_VuiCtrl), alignof(_VuiCtrl));
	memset(&ctrl->inner, 0, sizeof(VuiCtrl));
	VuiCtrlId ctrl_id = (pool_id << _VuiCtrlId_pool_id_SHIFT) & _VuiCtrlId_pool_id_MASK;
	ctrl_id |= (ctrl->counter << _VuiCtrlId_counter_SHIFT) & _VuiCtrlId_counter_MASK;
	*id_out = ctrl_id;
	return &ctrl->inner;
}

void _vui_ctrl_dealloc(VuiCtrlId ctrl_id) {
	_VuiWindow* w = &_vui.windows[_vui.focused_window_id];
	if (ctrl_id == _vui.mouse_focused_ctrl_id) {
		_vui_ctrl_set_mouse_focused(0);
	}
	if (ctrl_id == _vui.mouse_scroll_focused_ctrl_id) {
		_vui_ctrl_set_mouse_scroll_focused(0);
	}
	if (ctrl_id == w->focused_ctrl_id) {
		vui_ctrl_set_focused(0);
	}

	vui_assert(ctrl_id, "cannot remove an ctrl with a NULL identifier");
	VuiPoolId pool_id = (ctrl_id & _VuiCtrlId_pool_id_MASK) >> _VuiCtrlId_pool_id_SHIFT;
	uint16_t counter = (ctrl_id & _VuiCtrlId_counter_MASK) >> _VuiCtrlId_counter_SHIFT;
	_VuiCtrl* ctrl = _VuiPool_id_to_ptr((_VuiPool*)&_vui.ctrl_pool, pool_id, sizeof(_VuiCtrl));
	vui_assert(ctrl->counter == counter, "trying to remove an ctrl with an old identifier");
	_VuiPool_dealloc((_VuiPool*)&_vui.ctrl_pool, pool_id, sizeof(_VuiCtrl), alignof(_VuiCtrl));
}

VuiCtrlId vui_ctrl_get_id() {
	return _vui.build.parent_ctrl_id;
}

VuiCtrlId vui_ctrl_get_prev_id() {
	return _vui.build.sibling_prev_ctrl_id;
}

VuiCtrl* vui_ctrl_get(VuiCtrlId ctrl_id) {
	vui_assert(ctrl_id, "cannot get an ctrl with a NULL identifier");
	return vui_ctrl_try_get(ctrl_id);
}

VuiCtrl* vui_ctrl_try_get(VuiCtrlId ctrl_id) {
	if (ctrl_id == 0) return NULL;
	VuiPoolId pool_id = (ctrl_id & _VuiCtrlId_pool_id_MASK) >> _VuiCtrlId_pool_id_SHIFT;
	uint16_t counter = (ctrl_id & _VuiCtrlId_counter_MASK) >> _VuiCtrlId_counter_SHIFT;
	_VuiCtrl* ctrl = _VuiPool_id_to_ptr((_VuiPool*)&_vui.ctrl_pool, pool_id, sizeof(_VuiCtrl));
	vui_assert(ctrl->counter == counter, "trying to get an ctrl with an old identifier... internal counter of '%u' but a counter of '%u' was requested", ctrl->counter, counter);
	return &ctrl->inner;
}

void _vui_ctrl_unlink(VuiCtrl* ctrl) {
	//
	// if we have a previous sibling then make it point to our next sibling.
	// else we are the first child, so make the parent point to our next sibling.
	if (ctrl->sibling_prev_id) {
		VuiCtrl* sib = vui_ctrl_get(ctrl->sibling_prev_id);
		sib->sibling_next_id = ctrl->sibling_next_id;
	} else if (ctrl->parent_id) {
		VuiCtrl* parent = vui_ctrl_get(ctrl->parent_id);
		parent->child_first_id = ctrl->sibling_next_id;
	}

	//
	// if we have a next sibling then make it point to our previous sibling.
	// else we are the last child, so make the parent point to our previous sibling.
	if (ctrl->sibling_next_id) {
		VuiCtrl* sib = vui_ctrl_get(ctrl->sibling_next_id);
		sib->sibling_prev_id = ctrl->sibling_prev_id;
	} else if (ctrl->parent_id) {
		VuiCtrl* parent = vui_ctrl_get(ctrl->parent_id);
		parent->child_last_id = ctrl->sibling_prev_id;
	}

	ctrl->sibling_prev_id = 0;
	ctrl->sibling_next_id = 0;
}

void _vui_ctrl_insert(VuiCtrl* ctrl) {
	//
	// if there is a previously built previous sibling, then insert this control after.
	// else we are the first control of the parent.
	if (_vui.build.sibling_prev_ctrl_id) {
		VuiCtrl* sib_prev = vui_ctrl_get(_vui.build.sibling_prev_ctrl_id);
		ctrl->sibling_prev_id = _vui.build.sibling_prev_ctrl_id;
		ctrl->sibling_next_id = sib_prev->sibling_next_id;

		if (sib_prev->sibling_next_id) {
			VuiCtrl* sib_next = vui_ctrl_get(sib_prev->sibling_next_id);
			sib_next->sibling_prev_id = ctrl->id;
		} else {
			VuiCtrl* parent = vui_ctrl_get(_vui.build.parent_ctrl_id);
			parent->child_last_id = ctrl->id;
		}
		sib_prev->sibling_next_id = ctrl->id;
	} else {
		VuiCtrl* parent = vui_ctrl_get(_vui.build.parent_ctrl_id);
		if (parent->child_first_id) {
			VuiCtrl* og_first = vui_ctrl_get(parent->child_first_id);
			og_first->sibling_prev_id = ctrl->id;

			ctrl->sibling_next_id = parent->child_first_id;
			parent->child_first_id = ctrl->id;
		} else {
			parent->child_first_id = ctrl->id;
			parent->child_last_id = ctrl->id;
		}
	}
	ctrl->parent_id = _vui.build.parent_ctrl_id;
}

void _VuiCtrl_style_interp(VuiCtrl* ctrl, float dt) {
	if (ctrl->styles == NULL) return;
	const VuiCtrlStyle* to = &ctrl->styles[ctrl->state];
	const VuiCtrlStyle* from = &ctrl->prev_style;

	float prev_state_time = ctrl->state_time;
	ctrl->state_time += dt;

	if (prev_state_time == 0.f && ctrl->attributes.style_transition_time == 0.f) {
		//
		// state has been changed this frame and we do not have a transition time.
		// so just copy the style we are going to.
		//
		ctrl->style = *to;
		return;
	}

	//
	// do not interpolate when we have finished the transition.
	//
	if (prev_state_time >= ctrl->attributes.style_transition_time)
		return;

	float interp_ratio = 1.0;
	if (ctrl->state_time < ctrl->attributes.style_transition_time) {
		interp_ratio = ctrl->state_time / ctrl->attributes.style_transition_time;
	}

	ctrl->style.font_id = to->font_id;

	VuiThickness_lerp(&ctrl->style.margin, &to->margin, &from->margin, interp_ratio);
	VuiThickness_lerp(&ctrl->style.padding, &to->padding, &from->padding, interp_ratio);
	ctrl->style.bg_color = VuiColor_lerp(to->bg_color, from->bg_color, interp_ratio);
	ctrl->style.border_color = VuiColor_lerp(to->border_color, from->border_color, interp_ratio);
	ctrl->style.border_width = vui_lerp(to->border_width, from->border_width, interp_ratio);
	ctrl->style.radius = vui_lerp(to->radius, from->radius, interp_ratio);

	for (uint32_t i = 0; i < VuiCtrlStyle_max_colors; i += 1) {
		ctrl->style.colors[i] = VuiColor_lerp(to->colors[i], from->colors[i], interp_ratio);
	}

	for (uint32_t i = 0; i < VuiCtrlStyle_max_sizes; i += 1) {
		ctrl->style.sizes[i] = vui_lerp(to->sizes[i], from->sizes[i], interp_ratio);
	}

	VuiCtrlStyleUserExt_lerp(ctrl->style, to, from, interp_ratio);
}

void VuiImage_render(VuiCtrl* ctrl, VuiRect* content_rect, float interp_ratio) {
	VuiImageScaleMode scale_mode = ctrl->attributes.image_scale_mode;
	vui_render_image(content_rect, ctrl->image_id, ctrl->image_tint, scale_mode, interp_ratio);
}

void VuiText_render(VuiCtrl* ctrl, VuiRect* content_rect, float interp_ratio) {
	const VuiCtrlStyle* style = &ctrl->style;
	char* text = &_vui.render.w->text[ctrl->text_start_idx];
	vui_render_text(content_rect->left_top, style->font_id, style->text_line_height, text, ctrl->text_length, style->text_color, ctrl->text_word_wrap_at_width);
}

void VuiCheckBoxCheck_render(VuiCtrl* ctrl, VuiRect* content_rect, float interp_ratio) {
	VuiCtrl* parent = vui_ctrl_get(ctrl->parent_id);
	VuiBool is_active = (parent->state_flags & VuiCtrlStateFlags_active) == VuiCtrlStateFlags_active;
	if (content_rect->left != content_rect->right)
		vui_render_rect(content_rect, parent->style.check_color, parent->style.radius);
}

void VuiProgressBar_render(VuiCtrl* ctrl, VuiRect* content_rect, float interp_ratio) {
	VuiCtrl* parent = vui_ctrl_get(ctrl->parent_id);
	vui_render_rect(content_rect, parent->style.bar_color, parent->style.radius);
}

void VuiCanvas_render(VuiCtrl* ctrl, VuiRect* content_rect, float interp_ratio) {
	for (uint32_t i = 0; i < VuiStk_count(ctrl->canvas_items); i += 1) {
		VuiCanvasItem* item = &ctrl->canvas_items[i];
		switch (item->type) {
			case VuiCanvasItemType_line:
				vui_render_line(item->line.start_pos, item->line.end_pos, item->color, item->line.width);
				break;
			case VuiCanvasItemType_line_dotted: {
				VuiVec2 vec = VuiVec2_sub(item->line_dotted.end_pos, item->line_dotted.start_pos);
				VuiVec2 vec_norm = VuiVec2_norm(vec);

				VuiVec2 min_clip = VuiVec2_abs(VuiVec2_mul_scalar(VuiVec2_perp_right(vec_norm), item->line_dotted.width * 2.f));

				VuiRect parent_clip_rect = _vui.render.clip_rect;
				VuiRect clip_rect;
				clip_rect.left_top = VuiVec2_min(item->line_dotted.start_pos, item->line_dotted.end_pos);
				clip_rect.right_bottom = VuiVec2_max(item->line_dotted.start_pos, item->line_dotted.end_pos);

				float clip_half_width = (clip_rect.right - clip_rect.left) / 4.f;
				if (clip_half_width < min_clip.x) {
					float diff = min_clip.x - clip_half_width;
					clip_rect.left -= diff;
					clip_rect.right += diff;
				}

				float clip_half_height = (clip_rect.bottom - clip_rect.top) / 4.f;
				if (clip_half_height < min_clip.y) {
					float diff = min_clip.y - clip_half_height;
					clip_rect.top -= diff;
					clip_rect.bottom += diff;
				}

				_vui.render.clip_rect = VuiRect_clip(&_vui.render.clip_rect, &clip_rect);
				// debug render
				// vui_render_rect_border(&clip_rect, vui_color_asbestos, 0.f, 4.f);

				float len = VuiVec2_len(vec);

				float iteration_step = item->line_dotted.width + item->line_dotted.gap;
				VuiVec2 iteration_step_vec = VuiVec2_mul_scalar(vec_norm, iteration_step);
				uint32_t dots_count = ceilf(len / iteration_step) + 1;

				float radius = item->line_dotted.width / 2.f;
				VuiVec2 dot_vec = VuiVec2_mul_scalar(vec_norm, radius);

				VuiVec2 start = item->line_dotted.start_pos;
				for (uint32_t i = 0; i < dots_count; i += 1) {
					VuiVec2 center_pos = VuiVec2_add(start, dot_vec);
					vui_render_circle(center_pos, radius, item->color);
					start = VuiVec2_add(start, iteration_step_vec);
				}

				_vui.render.clip_rect = parent_clip_rect;
				break;
			};
			case VuiCanvasItemType_line_dashed: {
				VuiVec2 vec = VuiVec2_sub(item->line_dashed.end_pos, item->line_dashed.start_pos);
				VuiVec2 vec_norm = VuiVec2_norm(vec);

				VuiVec2 min_clip = VuiVec2_abs(VuiVec2_mul_scalar(VuiVec2_perp_right(vec_norm), item->line_dashed.width * 2.f));

				VuiRect parent_clip_rect = _vui.render.clip_rect;
				VuiRect clip_rect;
				clip_rect.left_top = VuiVec2_min(item->line_dashed.start_pos, item->line_dashed.end_pos);
				clip_rect.right_bottom = VuiVec2_max(item->line_dashed.start_pos, item->line_dashed.end_pos);

				float clip_half_width = (clip_rect.right - clip_rect.left) / 4.f;
				if (clip_half_width < min_clip.x) {
					float diff = min_clip.x - clip_half_width;
					clip_rect.left -= diff;
					clip_rect.right += diff;
				}

				float clip_half_height = (clip_rect.bottom - clip_rect.top) / 4.f;
				if (clip_half_height < min_clip.y) {
					float diff = min_clip.y - clip_half_height;
					clip_rect.top -= diff;
					clip_rect.bottom += diff;
				}

				_vui.render.clip_rect = VuiRect_clip(&_vui.render.clip_rect, &clip_rect);
				// debug render
				// vui_render_rect_border(&clip_rect, vui_color_asbestos, 0.f, 4.f);

				float len = VuiVec2_len(vec);

				float iteration_step = item->line_dashed.dash_length + item->line_dashed.gap;
				VuiVec2 iteration_step_vec = VuiVec2_mul_scalar(vec_norm, iteration_step);
				uint32_t dashes_count = ceilf(len / iteration_step);

				VuiVec2 dash_vec = VuiVec2_mul_scalar(vec_norm, item->line_dashed.dash_length);

				VuiVec2 start = item->line_dashed.start_pos;
				for (uint32_t i = 0; i < dashes_count; i += 1) {
					VuiVec2 end = VuiVec2_add(start, dash_vec);
					vui_render_line(start, end, item->color, item->line.width);
					start = VuiVec2_add(start, iteration_step_vec);
				}

				_vui.render.clip_rect = parent_clip_rect;
				break;
			};
			case VuiCanvasItemType_rect:
				vui_render_rect(&item->rect.rect, item->color, item->rect.radius);
				break;
			case VuiCanvasItemType_rect_border:
				vui_render_rect_border(&item->rect.rect, item->color, item->rect.radius, item->rect.width);
				break;
			case VuiCanvasItemType_circle:
				vui_render_circle(item->circle.pos, item->circle.radius, item->color);
				break;
			case VuiCanvasItemType_circle_border:
				vui_render_circle_border(item->circle.pos, item->circle.radius, item->color, item->circle.width);
				break;
			case VuiCanvasItemType_triangle:
				vui_render_triangle(item->triangle.a, item->triangle.b, item->triangle.c, item->color);
				break;
			case VuiCanvasItemType_triangle_border:
				vui_render_triangle_border(item->triangle.a, item->triangle.b, item->triangle.c, item->color, item->triangle.width);
				break;
			case VuiCanvasItemType_bezier_curve:
				vui_render_bezier_curve(item->bezier_curve.points, item->color, item->bezier_curve.width);
				break;
		}
	}
}

static VuiVec2 vui_get_text_size(char* text, uint32_t text_length, float word_wrap_at_width, VuiFontId font_id, float line_height) {
	VuiPositionTextArgs args = {0};
	args.userdata = _vui.position_text_userdata;
	args.font_id = font_id;
	args.line_height = line_height;
	args.text = text;
	args.text_length = text_length;
	args.word_wrap_at_width = word_wrap_at_width;
	return _vui.position_text_fn(&args).vec2;
}

static VuiVec2 vui_get_text_cursor_pos(char* text, uint32_t text_length, float word_wrap_at_width, VuiFontId font_id, float line_height, uint32_t cursor_idx) {
	VuiPositionTextArgs args = {0};
	args.userdata = _vui.position_text_userdata;
	args.font_id = font_id;
	args.line_height = line_height;
	args.text = text;
	args.text_length = text_length;
	args.word_wrap_at_width = word_wrap_at_width;
	args.cursor_num = cursor_idx + 1;
	return _vui.position_text_fn(&args).vec2;
}

static uint32_t vui_get_text_cursor_idx(char* text, uint32_t text_length, float word_wrap_at_width, VuiFontId font_id, float line_height, VuiVec2 left_top, VuiVec2 cursor_pos) {
	VuiPositionTextArgs args = {0};
	args.userdata = _vui.position_text_userdata;
	args.font_id = font_id;
	args.line_height = line_height;
	args.text = text;
	args.text_length = text_length;
	args.word_wrap_at_width = word_wrap_at_width;
	args.top_left = left_top;
	args.cursor_pos = cursor_pos;
	return _vui.position_text_fn(&args).u32;
}

void VuiTextBoxCursor_render(VuiCtrl* ctrl, VuiRect* content_rect, float interp_ratio) {
	VuiCtrl* parent = vui_ctrl_get(ctrl->parent_id);
	if (parent->styles == NULL) {
		// move up to the scroll view if this is multiline text box
		parent = vui_ctrl_get(parent->parent_id);
	}
	const VuiCtrlStyle* style = &parent->style;
	const VuiCtrlStyle* text_styles = parent->styles[0].text_styles;

	//
	// if we are focused, render the cursor or selection box.
	if (vui_ctrl_is_focused(parent->id) && _vui.input.focused_text_box.string) {
		if (_vui.input.focused_text_box.select_offset) {
			//
			// we have a selection so get the selection range in the correct order.
			uint32_t cursor_idx_start = _vui.input.focused_text_box.cursor_idx;
			uint32_t cursor_idx_end = _vui.input.focused_text_box.cursor_idx + _vui.input.focused_text_box.select_offset;
			if (_vui.input.focused_text_box.select_offset < 0) {
				uint32_t tmp = cursor_idx_start;
				cursor_idx_start = cursor_idx_end;
				cursor_idx_end = tmp;
			}


			// get the cursor offsets of both ends of the selection box.
			VuiVec2 start_idx_offset = vui_get_text_cursor_pos(_vui.input.focused_text_box.string, _vui.input.focused_text_box.string_len, 0.f, text_styles->font_id, text_styles->text_line_height, cursor_idx_start);

			VuiVec2 end_idx_offset = vui_get_text_cursor_pos(_vui.input.focused_text_box.string, _vui.input.focused_text_box.string_len, 0.f, text_styles->font_id, text_styles->text_line_height, cursor_idx_end);

			//
			// now loop line by line and render a rectangle for each line.
			float line_height = text_styles->text_line_height;
			while (1) {
				float cursor_width = 0.f;
				if (start_idx_offset.y < end_idx_offset.y) {
					//
					// our selection box does not end this line.
					// so measure until we reach a new line character.
					// if the line is empty, then just measure a space character.
					//

					uint32_t i = cursor_idx_start;
					while (i < _vui.input.focused_text_box.string_len && _vui.input.focused_text_box.string[i] != '\n') {
						i += 1;
					}

					char* s = _vui.input.focused_text_box.string + cursor_idx_start;
					uint32_t len = i - cursor_idx_start;
					if (i == cursor_idx_start) {
						static char space_char = ' ';
						s = &space_char;
						len = 1;
					}
					VuiVec2 size = vui_get_text_size(s, len, 0.f, text_styles->font_id, text_styles->text_line_height);
					cursor_width = size.x;
					cursor_idx_start = i + 1;
				} else {
					//
					// our selection box ends this line, so use the difference between the start and end of the box.
					cursor_width = end_idx_offset.x - start_idx_offset.x;
				}

				//
				// render the rectangle
				VuiCtrl* text_ctrl = vui_ctrl_get(ctrl->sibling_prev_id);
				float left = text_ctrl->rect.left + text_styles->padding.left + start_idx_offset.x;
				float top = text_ctrl->rect.top + text_styles->padding.top + start_idx_offset.y;
				VuiRect rect = VuiRect_init_wh(left, top, cursor_width, text_styles->text_line_height);
				vui_render_rect(&rect, style->selection_color, style->radius);

				//
				// end if we have reached the same line as the end of the selection.
				if (start_idx_offset.y >= end_idx_offset.y) break;

				//
				// put the start of the box at the start of the next line.
				start_idx_offset.x = 0.f;
				start_idx_offset.y += line_height;
			}
		} else {
			//
			// no selection, we have a cursor
			// get the offset to the cursor
			VuiVec2 start_idx_offset = vui_get_text_cursor_pos(_vui.input.focused_text_box.string, _vui.input.focused_text_box.string_len, 0.f, text_styles->font_id, text_styles->text_line_height, _vui.input.focused_text_box.cursor_idx);

			//
			// evenly place the cursor in between two characters
			if (_vui.input.focused_text_box.string_len > 0) {
				start_idx_offset.x -= style->cursor_width / 2.f;
			}

			VuiCtrl* text_ctrl = vui_ctrl_get(ctrl->sibling_prev_id);
			float left = text_ctrl->rect.left + text_styles->padding.left + start_idx_offset.x;
			float top = text_ctrl->rect.top + text_styles->padding.top + start_idx_offset.y;
			VuiRect rect = VuiRect_init_wh(left, top, style->cursor_width, text_styles->text_line_height);

			vui_render_rect(&rect, style->cursor_color, style->radius);
		}
	}
}

void vui_ctrl_start_(VuiCtrlSibId sib_id, VuiCtrlFlags flags, VuiActiveChange active_change, const VuiCtrlStyle styles[VuiCtrlState_COUNT], VuiCtrlRenderFn render_fn) {
	vui_assert(sib_id, "A sibling identifier of 0 (NULL) cannot be used");
	VuiCtrl* parent_ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);

	//
	// try to find the control in the existing tree.
	VuiCtrl* ctrl = parent_ctrl->child_first_id ? vui_ctrl_get(parent_ctrl->child_first_id) : NULL;
	while (ctrl) {
		if (ctrl->sib_id == sib_id) break;
		ctrl = ctrl->sibling_next_id ? vui_ctrl_get(ctrl->sibling_next_id) : NULL;
	}

	if (ctrl) {
		//
		// the control exist, if the previous sibling is different,
		// then unlink it and insert it in the correct place.
		if (ctrl->sibling_prev_id != _vui.build.sibling_prev_ctrl_id) {
			_vui_ctrl_unlink(ctrl);
			_vui_ctrl_insert(ctrl);
		}
		vui_assert(ctrl->last_frame_idx != _vui.build.frame_idx, "Found duplicate control sibling identifier of '%u'... these must be unique", sib_id);
	} else {
		//
		// it does not exist, allocate a new one.
		VuiCtrlId id = 0;
		ctrl = _vui_ctrl_alloc(&id);
		ctrl->id = id;
		ctrl->sib_id = sib_id;

		_vui_ctrl_insert(ctrl);
	}

	//
	// set the new state
	ctrl->last_frame_idx = _vui.build.frame_idx;
	VuiCtrlFlags sb_flags = ctrl->flags & (_VuiCtrlFlags_show_vertical_bar | _VuiCtrlFlags_show_horizontal_bar);
	ctrl->flags = flags | sb_flags;
	ctrl->render_fn = render_fn;
	ctrl->styles = styles;

	//
	// try to inherit the disabled state from the parent if it is on
	VuiBool is_disabled = (parent_ctrl->state_flags & VuiCtrlStateFlags_disabled) != 0;
	if (!is_disabled && VuiStk_count(_vui.build.disabled_stack)) {
		is_disabled = VuiStk_last(_vui.build.disabled_stack);
	}

	if (is_disabled) {
		ctrl->state_flags |= VuiCtrlStateFlags_disabled;
	} else {
		ctrl->state_flags &= ~VuiCtrlStateFlags_disabled;
	}

	//
	// set or unset active if it was changed externally
	if (active_change > 0) {
		active_change -= 1;
		if (active_change) {
			ctrl->state_flags |= VuiCtrlStateFlags_active;
		} else {
			ctrl->state_flags &= ~VuiCtrlStateFlags_active;
		}
	}

	if (is_disabled) {
		ctrl->focus_state = 0;
	} else if (flags & VuiCtrlFlags_focusable) {
		VuiFocusState focus_state = _vui_ctrl_focus_state(ctrl->id, !!(flags & VuiCtrlFlags_focusable_no_keyboard_actions));
		ctrl->focus_state = focus_state;

		//
		// handled pressable and toggleable controls active state.
		// pressable means it is active when pressed or held down.
		// toggleable means it's active state is toggled with every press.
		if (flags & VuiCtrlFlags_pressable) {
			if (focus_state & VuiFocusState_held) {
				ctrl->state_flags |= VuiCtrlStateFlags_active;
			} else {
				ctrl->state_flags &= ~VuiCtrlStateFlags_active;
			}
		} else if (flags & VuiCtrlFlags_selectable) {
			if (focus_state & VuiFocusState_pressed) {
				ctrl->state_flags |= VuiCtrlStateFlags_active;
			}
		} else if (flags & VuiCtrlFlags_toggleable) {
			if (focus_state & VuiFocusState_pressed) {
				ctrl->state_flags ^= VuiCtrlStateFlags_active;
			}
		}
	}

	ctrl->attributes = _vui.build.ctrl_attrs;
	_vui.build.parent_ctrl_id = ctrl->id;
	_vui.build.sibling_prev_ctrl_id = 0;

	VuiCtrlState state = 0;
	{
		VuiCtrlStateFlags state_flags = ctrl->state_flags;
		//
		// return the highest set bit.
		// x86 & arm have instructions to count leading zeros
		// this can be used to find the highest set bit without a loop.
		// not sure if this is worth the extra ifdef complication in the code.
		while (state_flags >>= 1) {
			state += 1;
		}
	}

	if (state != ctrl->state) {
		ctrl->prev_state = ctrl->state;
		ctrl->state = state;
		ctrl->state_time = 0.f;
		if (ctrl->attributes.style_transition_time) {
			ctrl->prev_style = ctrl->style;
			ctrl->prev_animate_aux = ctrl->animate_aux;
		}
	}

	_VuiCtrl_style_interp(ctrl, _vui.build.dt);
}

void vui_ctrl_end() {
	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);
	_vui.build.parent_ctrl_id = ctrl->parent_id;
	_vui.build.sibling_prev_ctrl_id = ctrl->id;
}

void vui_text_(VuiCtrlSibId sib_id, char* text, uint32_t text_length, float word_wrap_at_width, const VuiCtrlStyle styles[VuiCtrlState_COUNT]) {
	uint32_t text_start_idx = VuiStk_count(_vui.build.w->text);
	if (text_length) {
		char* t = VuiStk_push_many(&_vui.build.w->text, text_length);
		vui_ensure_alloc_ok(t);
		memcpy(t, text, text_length);
	}

	vui_ctrl_start_(sib_id, 0, 0, styles, VuiText_render);
	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);
	const VuiCtrlStyle* style = &ctrl->style;

	{
		VuiVec2 size = vui_get_text_size(text, text_length, word_wrap_at_width, style->font_id, style->text_line_height);
		if (size.y == 0.f) {
			size.y = style->text_line_height;
		}

		size.x += VuiThickness_horizontal(&style->padding);
		size.y += VuiThickness_vertical(&style->padding);

		ctrl->attributes.width = size.x;
		ctrl->attributes.height = size.y;
	}

	ctrl->text_start_idx = text_start_idx;
	ctrl->text_length = text_length;
	ctrl->text_word_wrap_at_width = word_wrap_at_width;

	vui_ctrl_end();
}

void vui_image(VuiCtrlSibId sib_id, VuiImageId image_id, VuiColor image_tint, const VuiCtrlStyle styles[VuiCtrlState_COUNT]) {
	vui_ctrl_start_(sib_id, 0, 0, styles, VuiImage_render);
	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);
	VuiCtrlStyle* style = &ctrl->style;
	VuiImage* image = vui_image_get(image_id);
	if (ctrl->attributes.width == vui_auto_len) {
		ctrl->attributes.width = image->width + VuiThickness_horizontal(&style->padding);
	}

	if (ctrl->attributes.height == vui_auto_len) {
		ctrl->attributes.height = image->height + VuiThickness_vertical(&style->padding);
	}

	ctrl->image_id = image_id;
	ctrl->image_tint = image_tint;
	vui_ctrl_end();
}

void vui_spacing(VuiCtrlSibId sib_id, float width, float height) {
	vui_scope_width(width)
	vui_scope_height(height) {
		vui_ctrl_start_(sib_id, 0, 0, NULL, NULL);
		vui_ctrl_end();
	}
}

void vui_separator(VuiCtrlSibId sib_id, const VuiCtrlStyle styles[VuiCtrlState_COUNT]) {
	VuiCtrl* parent = vui_ctrl_get(_vui.build.parent_ctrl_id);
	VuiBool is_row = parent->layout_type == VuiLayoutType_row;
	vui_assert(is_row || parent->layout_type == VuiLayoutType_column, "vui_separator can only be used in a non-wrapping row or column layout");
	vui_assert(!parent->attributes.layout_wrap, "vui_separator can only be used in a non-wrapping row or column layout");

	vui_ctrl_start_(sib_id, 0, 0, styles, NULL);
	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);
	const VuiCtrlStyle* style = &ctrl->style;

	ctrl->attributes.width = is_row ? vui_fill_len : style->separator_size;
	ctrl->attributes.height = is_row ? style->separator_size : vui_fill_len;

	vui_ctrl_end();
}

VuiFocusState vui_button_start(VuiCtrlSibId sib_id, const VuiCtrlStyle styles[VuiCtrlState_COUNT]) {
	vui_ctrl_start_(sib_id, VuiCtrlFlags_focusable | VuiCtrlFlags_pressable, 0, styles, NULL);
	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);
	return ctrl->focus_state;
}

void vui_button_end() {
	vui_ctrl_end();
}

VuiFocusState vui_button(VuiCtrlSibId sib_id, const VuiCtrlStyle styles[VuiCtrlState_COUNT]) {
	VuiFocusState state = vui_button_start(sib_id, styles);
	vui_button_end();
	return state;
}

VuiFocusState vui_text_button_(VuiCtrlSibId sib_id, char* text, uint32_t text_length, const VuiCtrlStyle styles[VuiCtrlState_COUNT]) {
	VuiFocusState state = vui_button_start(sib_id, styles);
	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);

	//
	// the child styles are only in the VuiCtrlState_default style
	const VuiCtrlStyle* style = &styles[0];

	vui_scope_width(vui_auto_len)
	vui_scope_height(vui_auto_len)
	vui_scope_offset(0.f, 0.f)
	vui_scope_align(VuiAlign_center)
	vui_scope_disabled(vui_false) {
		vui_text_(vui_sib_id, text, text_length, 0.f, style->text_styles);
	}

	vui_button_end();
	return state;
}

VuiFocusState vui_image_button(VuiCtrlSibId sib_id, VuiImageId image_id, VuiColor image_tint, const VuiCtrlStyle styles[VuiCtrlState_COUNT]) {
	VuiFocusState state = vui_button_start(sib_id, styles);
	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);

	//
	// the child styles are only in the VuiCtrlState_default style
	const VuiCtrlStyle* style = &styles[0];

	vui_scope_width(vui_auto_len)
	vui_scope_height(vui_auto_len)
	vui_scope_offset(0.f, 0.f)
	vui_scope_align(VuiAlign_center)
	vui_scope_disabled(vui_false) {
		vui_image(vui_sib_id, image_id, image_tint, style->image_styles);
	}
	vui_button_end();
	return state;
}

VuiFocusState vui_image_text_button_(VuiCtrlSibId sib_id, VuiImageId image_id, VuiColor image_tint, char* text, uint32_t text_length, const VuiCtrlStyle styles[VuiCtrlState_COUNT]) {
	VuiFocusState state = vui_button_start(sib_id, styles);
	vui_column_layout();
	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);

	//
	// the child styles are only in the VuiCtrlState_default style
	const VuiCtrlStyle* style = &styles[0];

	vui_scope_width(vui_auto_len)
	vui_scope_height(vui_auto_len)
	vui_scope_offset(0.f, 0.f)
	vui_scope_align(VuiAlign_center)
	vui_scope_disabled(vui_false)
	{
		vui_image(vui_sib_id, image_id, image_tint, style->image_styles);
		vui_text_(vui_sib_id, text, text_length, 0.f, style->text_styles);
	}

	vui_button_end();
	return state;
}

VuiBool vui_toggle_button_start(VuiCtrlSibId sib_id, VuiBool* pressed, const VuiCtrlStyle styles[VuiCtrlState_COUNT]) {
	vui_ctrl_start_(sib_id, VuiCtrlFlags_focusable | VuiCtrlFlags_toggleable, pressed ? *pressed + 1 : 0, styles, NULL);
	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);

	VuiBool is_active = (ctrl->state_flags & VuiCtrlStateFlags_active) == VuiCtrlStateFlags_active;
	if (pressed) *pressed = is_active;
	return is_active;
}

void vui_toggle_button_end() {
	vui_ctrl_end();
}

VuiBool vui_text_toggle_button_(VuiCtrlSibId sib_id, VuiBool* pressed, char* text, uint32_t text_length, const VuiCtrlStyle styles[VuiCtrlState_COUNT]) {
	VuiBool p = vui_toggle_button_start(sib_id, pressed, styles);
	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);

	//
	// the child styles are only in the VuiCtrlState_default style
	const VuiCtrlStyle* style = &styles[0];

	vui_scope_width(vui_auto_len)
	vui_scope_height(vui_auto_len)
	vui_scope_offset(0.f, 0.f)
	vui_scope_align(VuiAlign_center)
	vui_scope_disabled(vui_false) {
		vui_text_(vui_sib_id, text, text_length, 0.f, style->text_styles);
	}
	vui_toggle_button_end();
	return p;
}

VuiBool vui_image_toggle_button(VuiCtrlSibId sib_id, VuiBool* pressed, VuiImageId image_id, VuiColor image_tint, const VuiCtrlStyle styles[VuiCtrlState_COUNT]) {
	VuiBool p = vui_toggle_button_start(sib_id, pressed, styles);
	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);

	//
	// the child styles are only in the VuiCtrlState_default style
	const VuiCtrlStyle* style = &styles[0];

	vui_scope_width(vui_auto_len)
	vui_scope_height(vui_auto_len)
	vui_scope_offset(0.f, 0.f)
	vui_scope_align(VuiAlign_center)
	vui_scope_disabled(vui_false) {
		vui_image(vui_sib_id, image_id, image_tint, style->image_styles);
	}
	vui_toggle_button_end();
	return p;
}

VuiBool vui_image_text_toggle_button_(VuiCtrlSibId sib_id, VuiBool* pressed, VuiImageId image_id, VuiColor image_tint, char* text, uint32_t text_length, const VuiCtrlStyle styles[VuiCtrlState_COUNT]) {
	VuiBool p = vui_toggle_button_start(sib_id, pressed, styles);
	vui_column_layout();
	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);

	//
	// the child styles are only in the VuiCtrlState_default style
	const VuiCtrlStyle* style = &styles[0];

	vui_scope_width(vui_auto_len)
	vui_scope_height(vui_auto_len)
	vui_scope_offset(0.f, 0.f)
	vui_scope_align(VuiAlign_center)
	vui_scope_disabled(vui_false) {
		vui_image(vui_sib_id, image_id, image_tint, style->image_styles);
		vui_text_(vui_sib_id, text, text_length, 0.f, style->text_styles);
	}
	vui_toggle_button_end();
	return p;
}

VuiBool vui_select_button_start(VuiCtrlSibId sib_id, VuiCtrlSibId* selected_sib_id, const VuiCtrlStyle styles[VuiCtrlState_COUNT]) {
	vui_ctrl_start_(sib_id, VuiCtrlFlags_focusable | VuiCtrlFlags_selectable, (*selected_sib_id == sib_id) + 1, styles, NULL);
	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);

	VuiBool is_active = (ctrl->state_flags & VuiCtrlStateFlags_active) == VuiCtrlStateFlags_active;
	if (is_active) *selected_sib_id = sib_id;
	return is_active;
}

void vui_select_button_end() {
	vui_button_end();
}

VuiBool vui_text_select_button_(VuiCtrlSibId sib_id, VuiCtrlSibId* selected_sib_id, char* text, uint32_t text_length, const VuiCtrlStyle styles[VuiCtrlState_COUNT]) {
	VuiBool p = vui_select_button_start(sib_id, selected_sib_id, styles);
	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);

	//
	// the child styles are only in the VuiCtrlState_default style
	const VuiCtrlStyle* style = &styles[0];

	vui_scope_width(vui_auto_len)
	vui_scope_height(vui_auto_len)
	vui_scope_offset(0.f, 0.f)
	vui_scope_align(VuiAlign_center)
	vui_scope_disabled(vui_false) {
		vui_text_(vui_sib_id, text, text_length, 0.f, style->text_styles);
	}
	vui_select_button_end();
	return p;
}

VuiBool vui_image_select_button(VuiCtrlSibId sib_id, VuiCtrlSibId* selected_sib_id, VuiImageId image_id, VuiColor image_tint, const VuiCtrlStyle styles[VuiCtrlState_COUNT]) {
	VuiBool p = vui_select_button_start(sib_id, selected_sib_id, styles);
	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);

	//
	// the child styles are only in the VuiCtrlState_default style
	const VuiCtrlStyle* style = &styles[0];

	vui_scope_width(vui_auto_len)
	vui_scope_height(vui_auto_len)
	vui_scope_offset(0.f, 0.f)
	vui_scope_align(VuiAlign_center)
	vui_scope_disabled(vui_false) {
		vui_image(vui_sib_id, image_id, image_tint, style->image_styles);
	}
	vui_select_button_end();
	return p;
}

VuiBool vui_image_text_select_button_(VuiCtrlSibId sib_id, VuiCtrlSibId* selected_sib_id, VuiImageId image_id, VuiColor image_tint, char* text, uint32_t text_length, const VuiCtrlStyle styles[VuiCtrlState_COUNT]) {
	VuiBool p = vui_select_button_start(sib_id, selected_sib_id, styles);
	vui_column_layout();
	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);

	//
	// the child styles are only in the VuiCtrlState_default style
	const VuiCtrlStyle* style = &styles[0];

	vui_scope_width(vui_auto_len)
	vui_scope_height(vui_auto_len)
	vui_scope_offset(0.f, 0.f)
	vui_scope_align(VuiAlign_center)
	vui_scope_disabled(vui_false) {
		vui_image(vui_sib_id, image_id, image_tint, style->image_styles);
		vui_text_(vui_sib_id, text, text_length, 0.f, style->text_styles);
	}

	vui_select_button_end();
	return p;
}


VuiBool vui_check_box(VuiCtrlSibId sib_id, VuiBool* checked, const VuiCtrlStyle styles[VuiCtrlState_COUNT]) {
	VuiBool is_active = vui_false;

	vui_ctrl_start_(sib_id, VuiCtrlFlags_focusable | VuiCtrlFlags_toggleable, checked ? *checked + 1 : 0, styles, NULL);
	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);
	const VuiCtrlStyle* style = &ctrl->style;
	ctrl->attributes.width = style->check_box_size;
	ctrl->attributes.height = style->check_box_size;

	is_active = (ctrl->state_flags & VuiCtrlStateFlags_active) == VuiCtrlStateFlags_active;
	if (checked) *checked = is_active;

	vui_scope_width(vui_auto_len)
	vui_scope_height(vui_auto_len)
	vui_scope_align(VuiAlign_center)
	vui_scope_width(style->check_size)
	vui_scope_height(style->check_size) {
		vui_ctrl_start_(vui_sib_id, 0, 0, NULL, VuiCheckBoxCheck_render);
		vui_ctrl_end();
	}

	vui_ctrl_end();
	return is_active;
}

VuiBool vui_check_box_aux_start(VuiCtrlSibId sib_id, VuiBool* checked, const VuiCtrlStyle styles[VuiCtrlState_COUNT]) {
	vui_ctrl_start_(sib_id, VuiCtrlFlags_focusable | VuiCtrlFlags_toggleable, checked ? *checked + 1 : 0, NULL, NULL);
	vui_column_layout();

	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);
	VuiBool c = (ctrl->state_flags & VuiCtrlStateFlags_active) == VuiCtrlStateFlags_active;
	if (!checked) checked = &c;
	else *checked = c;

	vui_push_offset(0.f, 0.f);
	vui_push_align(VuiAlign_center);
	VuiBool state = vui_check_box(vui_sib_id, checked, styles);

	VuiCtrl* check_box = vui_ctrl_get(_vui.build.sibling_prev_ctrl_id);

	//
	// synchronize the active states between the check box and the parent
	if (check_box->state_flags & VuiCtrlStateFlags_active) {
		ctrl->state_flags |= VuiCtrlStateFlags_active;
	} else {
		ctrl->state_flags &= ~VuiCtrlStateFlags_active;
	}

	return state;
}

void vui_check_box_aux_end() {
	vui_pop_align();
	vui_pop_offset();
	vui_ctrl_end();
}

VuiBool vui_text_check_box_(VuiCtrlSibId sib_id, VuiBool* checked, char* text, uint32_t text_length, const VuiCtrlStyle styles[VuiCtrlState_COUNT]) {
	VuiBool state = vui_check_box_aux_start(sib_id, checked, styles);

	//
	// the child styles are only in the VuiCtrlState_default style
	const VuiCtrlStyle* style = &styles[0];

	vui_text_(vui_sib_id, text, text_length, 0.f, style->text_styles);

	vui_check_box_aux_end();
	return state;
}

VuiBool vui_image_check_box(VuiCtrlSibId sib_id, VuiBool* checked, VuiImageId image_id, VuiColor image_tint, const VuiCtrlStyle styles[VuiCtrlState_COUNT]) {
	VuiBool state = vui_check_box_aux_start(sib_id, checked, styles);

	//
	// the child styles are only in the VuiCtrlState_default style
	const VuiCtrlStyle* style = &styles[0];

	vui_scope_disabled(vui_false)
	vui_image(vui_sib_id, image_id, image_tint, style->image_styles);

	vui_check_box_aux_end();
	return state;
}

VuiBool vui_image_text_check_box_(VuiCtrlSibId sib_id, VuiBool* checked, VuiImageId image_id, VuiColor image_tint, char* text, uint32_t text_length, const VuiCtrlStyle styles[VuiCtrlState_COUNT]) {
	VuiBool state = vui_check_box_aux_start(sib_id, checked, styles);

	//
	// the child styles are only in the VuiCtrlState_default style
	const VuiCtrlStyle* style = &styles[0];

	vui_scope_disabled(vui_false)
	vui_image(vui_sib_id, image_id, image_tint, style->image_styles);
	vui_text_(vui_sib_id, text, text_length, 0.f, style->text_styles);

	vui_check_box_aux_end();
	return state;
}

VuiBool vui_radio_button(VuiCtrlSibId sib_id, VuiCtrlSibId* selected_sib_id, const VuiCtrlStyle styles[VuiCtrlState_COUNT]) {
	VuiBool is_active = vui_false;
	vui_ctrl_start_(sib_id, VuiCtrlFlags_focusable | VuiCtrlFlags_selectable, (*selected_sib_id == sib_id) + 1, styles, NULL);
	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);
	const VuiCtrlStyle* style = &ctrl->style;
	ctrl->attributes.width = style->check_box_size;
	ctrl->attributes.height = style->check_box_size;

	is_active = (ctrl->state_flags & VuiCtrlStateFlags_active) == VuiCtrlStateFlags_active;
	if (is_active) *selected_sib_id = sib_id;

	vui_scope_width(style->check_size)
	vui_scope_height(style->check_size) {
		vui_ctrl_start_(vui_sib_id, 0, 0, NULL, VuiCheckBoxCheck_render);
		vui_ctrl_end();
	}


	vui_ctrl_end();
	return is_active;
}

VuiBool vui_radio_button_aux_start(VuiCtrlSibId sib_id, VuiCtrlSibId* selected_sib_id, const VuiCtrlStyle styles[VuiCtrlState_COUNT]) {
	vui_ctrl_start_(sib_id + vui_sib_id, VuiCtrlFlags_focusable | VuiCtrlFlags_selectable, (*selected_sib_id == sib_id) + 1, NULL, NULL);
	vui_column_layout();

	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);
	VuiBool is_active = (ctrl->state_flags & VuiCtrlStateFlags_active) == VuiCtrlStateFlags_active;
	if (is_active) *selected_sib_id = sib_id;

	vui_push_offset(0.f, 0.f);
	vui_push_align(VuiAlign_center);
	VuiBool state = vui_radio_button(sib_id, selected_sib_id, styles);
	VuiCtrl* radio_button = vui_ctrl_get(_vui.build.sibling_prev_ctrl_id);

	//
	// synchronize the active states between the check box and the parent
	if (radio_button->state_flags & VuiCtrlStateFlags_active) {
		ctrl->state_flags |= VuiCtrlStateFlags_active;
	} else {
		ctrl->state_flags &= ~VuiCtrlStateFlags_active;
	}

	return state;
}

void vui_radio_button_aux_end() {
	vui_ctrl_end();
	vui_pop_offset();
	vui_pop_align();
}

VuiBool vui_text_radio_button_(VuiCtrlSibId sib_id, VuiCtrlSibId* selected_sib_id, char* text, uint32_t text_length, const VuiCtrlStyle styles[VuiCtrlState_COUNT]) {
	VuiBool state = vui_radio_button_aux_start(sib_id, selected_sib_id, styles);

	//
	// the child styles are only in the VuiCtrlState_default style
	const VuiCtrlStyle* style = &styles[0];

	vui_text_(vui_sib_id, text, text_length, 0.f, style->text_styles);

	vui_radio_button_aux_end();
	return state;
}

VuiBool vui_image_radio_button(VuiCtrlSibId sib_id, VuiCtrlSibId* selected_sib_id, VuiImageId image_id, VuiColor image_tint, const VuiCtrlStyle styles[VuiCtrlState_COUNT]) {
	VuiBool state = vui_radio_button_aux_start(sib_id, selected_sib_id, styles);

	//
	// the child styles are only in the VuiCtrlState_default style
	const VuiCtrlStyle* style = &styles[0];

	vui_scope_disabled(vui_false)
	vui_image(vui_sib_id, image_id, image_tint, style->image_styles);

	vui_radio_button_aux_end();
	return state;
}

VuiBool vui_image_text_radio_button_(VuiCtrlSibId sib_id, VuiCtrlSibId* selected_sib_id, VuiImageId image_id, VuiColor image_tint, char* text, uint32_t text_length, const VuiCtrlStyle styles[VuiCtrlState_COUNT]) {
	VuiBool state = vui_radio_button_aux_start(sib_id, selected_sib_id, styles);

	//
	// the child styles are only in the VuiCtrlState_default style
	const VuiCtrlStyle* style = &styles[0];

	vui_scope_disabled(vui_false)
	vui_image(vui_sib_id, image_id, image_tint, style->image_styles);
	vui_text_(vui_sib_id, text, text_length, 0.f, style->text_styles);

	vui_radio_button_aux_end();
	return state;
}

typedef uint8_t _VuiSliderType;
enum {
	_VuiSliderType_float,
	_VuiSliderType_u32,
	_VuiSliderType_s32,
};

// returns vui_true when the button has been released
VuiBool _vui_slider(VuiCtrlSibId sib_id, void* value_out, void* min, void* max, const VuiCtrlStyle styles[VuiCtrlState_COUNT], _VuiSliderType type) {
	vui_scope_height(vui_auto_len)
	vui_scope_layout_wrap(vui_false)
	vui_ctrl_start_(sib_id, 0, 0, styles, NULL);

	VuiBool released = vui_false;
	vui_scope_align(VuiAlign_left_top) {
		VuiCtrl* parent = vui_ctrl_get(_vui.build.parent_ctrl_id);
		const VuiCtrlStyle* style = &parent->style;

		//
		// the child styles are only in the VuiCtrlState_default style
		const VuiCtrlStyle* bar_styles = styles[0].bar_styles;
		const VuiCtrlStyle* button_styles = styles[0].button_styles;

		//
		// create the bar of the slider. the bar is shrunk by the button width and the margin to fit it inside the parent's clipping rectangle.
		float parent_width = VuiRect_width(&parent->rect);
		float parent_inner_width = 0.f;
		float bar_width = 0.f;
		if (parent_width != 0.f) {
			parent_inner_width = parent_width - VuiThickness_horizontal(&style->padding) - style->border_width * 2.f;
			bar_width = parent_inner_width - style->button_width - VuiThickness_horizontal(&bar_styles->margin);
		}
		vui_scope_width(bar_width)
		vui_scope_height(style->bar_height)
		vui_scope_offset(style->button_width * 0.5f, style->button_width * 0.25f)
		vui_ctrl(vui_sib_id, bar_styles);

		//
		// calculate the button's offset by using the size of the bar and it's margin.
		VuiCtrlId bar_ctrl_id = _vui.build.sibling_prev_ctrl_id;
		VuiCtrl* bar = vui_ctrl_get(bar_ctrl_id);
		float ratio_value_to_screen = VuiRect_width(&bar->rect);
		float button_offset;
		switch (type) {
			case _VuiSliderType_float:
				ratio_value_to_screen /= *(float*)max - *(float*)min;
				button_offset = ratio_value_to_screen * (*(float*)value_out - *(float*)min);
				break;
			case _VuiSliderType_u32:
				ratio_value_to_screen /= *(uint32_t*)max - *(uint32_t*)min;
				button_offset = ratio_value_to_screen * (*(uint32_t*)value_out - *(uint32_t*)min);
				break;
			case _VuiSliderType_s32:
				ratio_value_to_screen /= *(int32_t*)max - *(int32_t*)min;
				button_offset = ratio_value_to_screen * (*(int32_t*)value_out - *(int32_t*)min);
				break;
		}

		button_offset += bar_styles->margin.left - button_styles->margin.left;

		//
		// make the button and if it is held down, then modify the value.
		VuiFocusState button_state;
		vui_scope_offset(button_offset, bar_styles->margin.top - button_styles->margin.top)
		vui_scope_width(style->button_width)
		vui_scope_height(style->button_width)
		button_state = vui_button(vui_sib_id, button_styles);
		if (button_state & VuiFocusState_held) {
			bar = vui_ctrl_get(bar_ctrl_id);
			float mouse_offset = _vui.input.mouse.x - bar->rect.left;
			if (mouse_offset < 0.f) mouse_offset = 0.f;
			float value = mouse_offset / ratio_value_to_screen;
			switch (type) {
				case _VuiSliderType_float: *(float*)value_out = value + *(float*)min; break;
				case _VuiSliderType_u32: *(uint32_t*)value_out = (uint32_t)roundf(value) + *(uint32_t*)min; break;
				case _VuiSliderType_s32: *(int32_t*)value_out =  (int32_t)roundf(value) + *(int32_t*)min; break;
			}
		}

		released = (button_state & VuiFocusState_released) == VuiFocusState_released;
	}

	vui_ctrl_end();
	return released;
}

VuiBool vui_slider_uint(VuiCtrlSibId sib_id, uint32_t* value_out, uint32_t min, uint32_t max, const VuiCtrlStyle styles[VuiCtrlState_COUNT]) {
	*value_out = *value_out < min ? min : (*value_out > max ? max : *value_out);
	VuiBool released = _vui_slider(sib_id, value_out, &min, &max, styles, _VuiSliderType_u32);
	*value_out = *value_out < min ? min : (*value_out > max ? max : *value_out);
	return released;
}

VuiBool vui_slider_sint(VuiCtrlSibId sib_id, int32_t* value_out, int32_t min, int32_t max, const VuiCtrlStyle styles[VuiCtrlState_COUNT]) {
	int32_t og_value = *value_out;
	*value_out = *value_out < min ? min : (*value_out > max ? max : *value_out);
	VuiBool released = _vui_slider(sib_id, value_out, &min, &max, styles, _VuiSliderType_s32);
	*value_out = *value_out < min ? min : (*value_out > max ? max : *value_out);
	return released;
}

VuiBool vui_slider_float(VuiCtrlSibId sib_id, float* value_out, float min, float max, const VuiCtrlStyle styles[VuiCtrlState_COUNT]) {
	*value_out = vui_clamp(*value_out, min, max);
	VuiBool released = _vui_slider(sib_id, value_out, &min, &max, styles, _VuiSliderType_float);
	*value_out = vui_clamp(*value_out, min, max);
	return released;
}

void vui_progress_bar(VuiCtrlSibId sib_id, float value, float min, float max, const VuiCtrlStyle styles[VuiCtrlState_COUNT]) {
	value = vui_clamp(value, min, max);
	float ratio = (value - min) / (max - min);

	vui_scope_layout_wrap(vui_false)
	vui_ctrl_start_(sib_id, 0, 0, styles, NULL);
		vui_column_layout();

		vui_scope_width_ratio(ratio)
		vui_scope_height(vui_fill_len)
		vui_ctrl_start_(vui_sib_id, 0, 0, NULL, VuiProgressBar_render);
		vui_ctrl_end();
	vui_ctrl_end();
}

void VuiCtrl_set_scroll_offset(VuiCtrl* ctrl, VuiVec2 offset, VuiBool holding_scroll_bar_slider) {
	//
	// work out the container size of the scroll view.
	// this is the viewport of the scroll view that is shorten
	// when a scroll bar is visible.
	VuiVec2 container_size = VuiRect_size(ctrl->rect);
	{
		const VuiCtrlStyle* style = &ctrl->style;
		//
		// the child styles are only in the VuiCtrlState_default style
		const VuiCtrlStyle* bar_styles = ctrl->styles[0].bar_styles;
		const VuiCtrlStyle* slider_styles = bar_styles[0].slider_styles;
		container_size.x -= style->border_width * 2.f + VuiThickness_horizontal(&style->padding);
		container_size.y -= style->border_width * 2.f + VuiThickness_vertical(&style->padding);

		if (ctrl->flags & _VuiCtrlFlags_show_vertical_bar) {
			//
			// we have a vertical scroll bar, so remove the outer width of the control from the container's width
			container_size.x -= bar_styles->slider_width +
				VuiThickness_horizontal(&bar_styles->margin) + VuiThickness_horizontal(&bar_styles->padding) +
				bar_styles->border_width * 2.f + VuiThickness_horizontal(&slider_styles->margin);
		}
		if (ctrl->flags & _VuiCtrlFlags_show_horizontal_bar) {
			//
			// we have a horizontal scroll bar, so remove the outer height of the control from the container's height
			container_size.y -= bar_styles->slider_width +
				VuiThickness_vertical(&bar_styles->margin) + VuiThickness_vertical(&bar_styles->padding) +
				bar_styles->border_width * 2.f + VuiThickness_vertical(&slider_styles->margin);
		}
	}

	//
	// set the scroll offset and make sure it does go scroll further than the size of the content
	VuiCtrl* content = vui_ctrl_get(ctrl->scroll_content_id);
	VuiVec2 min = VuiVec2_min(VuiVec2_add(VuiVec2_neg(VuiRect_size(content->rect)), container_size), VuiVec2_zero);
	VuiVec2 max = VuiVec2_zero;
	ctrl->scroll_offset = VuiVec2_clamp(offset, min, max);

	//
	// if this is the scroll focused control, then write back out to the pointer that holds the scroll offset passed in to vui_scroll_view_start_
	if ((vui_ctrl_is_mouse_scroll_focused(ctrl->id) || holding_scroll_bar_slider) && _vui.build.mouse_scroll_focused_content_offset) {
		*_vui.build.mouse_scroll_focused_content_offset = ctrl->scroll_offset;
	}
}

void vui_scroll_view_start_(VuiCtrlSibId sib_id, VuiVec2* content_offset_in_out, VuiVec2* size_in_out, VuiScrollFlags flags, const VuiCtrlStyle styles[VuiCtrlState_COUNT]) {
	vui_ctrl_start_(sib_id, flags | VuiCtrlFlags_focusable_scroll, 0, styles, NULL);
	VuiCtrlId scroll_view_ctrl_id = _vui.build.parent_ctrl_id;
	VuiCtrl* ctrl = vui_ctrl_get(scroll_view_ctrl_id);

	//
	// keep track of the in_out pointers so we can potentially set these later in the pipeline.
	if (vui_ctrl_is_mouse_scroll_focused(ctrl->id)) {
		_vui.build.mouse_scroll_focused_content_offset = content_offset_in_out;
		_vui.build.mouse_scroll_focused_size = size_in_out;
	}

	if (size_in_out) {
		ctrl->scroll_view_size = *size_in_out;
	}

	if (ctrl->scroll_view_size.x == 0.f && ctrl->scroll_view_size.y == 0.f) {
		ctrl->scroll_view_size.x = ctrl->attributes.width;
		ctrl->scroll_view_size.y = ctrl->attributes.height;
		if (size_in_out) *size_in_out = ctrl->scroll_view_size;
	}

	//
	// a resizable scroll view uses the ctrl->scroll_view_size field as it width and height
	if (flags & VuiScrollFlags_resizable) {
		ctrl->attributes.width = ctrl->scroll_view_size.x;
		ctrl->attributes.height = ctrl->scroll_view_size.y;
	}

	//
	// if the axis can be scolled, make it automatic so we can infinitely grow.
	// otherwise fill the parent.
	vui_push_width(flags & VuiScrollFlags_horizontal ? vui_auto_len : vui_fill_len);
	vui_push_height(flags & VuiScrollFlags_vertical ? vui_auto_len : vui_fill_len);

	//
	// create the infinitely sized control to house the scrollable content.
	vui_scope_offset(0.f, 0.f)
	vui_scope_align(VuiAlign_left_top)
	vui_ctrl_start_(-1, 0, 0, NULL, NULL);
	ctrl = vui_ctrl_get(scroll_view_ctrl_id);
	ctrl->scroll_content_id = _vui.build.parent_ctrl_id;

	vui_pop_width();
	vui_pop_height();

	//
	// update the scroll offset with the value stored in the content_offset_in_out
	if (content_offset_in_out) {
		VuiCtrl_set_scroll_offset(ctrl, *content_offset_in_out, vui_false);
	}
}

//
// @param length:
//     the length of the long side of the scroll bar control. this excludes the margin.
//     e.g. for a vertical bar, this will be the height of the control.
//
// @param container_length:
//     the length of the long side of the viewport of the scrollable content.
//     e.g. for a vertical bar, this will be the height of the viewable area of the scroll area.
//
VuiBool _vui_scroll_bar(VuiCtrlSibId sib_id, float length, float container_length, float* content_offset_in_out, float content_length, VuiBool is_horizontal, const VuiCtrlStyle styles[VuiCtrlState_COUNT]) {
	if (content_length < 1.f) content_length = 1.f;
	vui_scope_width(is_horizontal ? length : vui_auto_len)
	vui_scope_height(is_horizontal ? vui_auto_len : length)
	vui_ctrl_start(sib_id, styles);
	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);
	const VuiCtrlStyle* style = &ctrl->style;
	//
	// the child styles are only in the VuiCtrlState_default style
	const VuiCtrlStyle* slider_styles = styles[0].slider_styles;

	//
	// work out the slider size
	VuiVec2 slider_size = VuiVec2_init(style->slider_width, style->slider_width);
	float bar_content_length = length - style->border_width * 2.f;
	float* slider_long_side_length = NULL;
	float (*thickness_dir_len_fn)(const VuiThickness*) = NULL;
	if (is_horizontal) {
		slider_long_side_length = &slider_size.x;
		thickness_dir_len_fn = VuiThickness_horizontal;
		bar_content_length -= VuiThickness_horizontal(&style->padding);
	} else {
		slider_long_side_length = &slider_size.y;
		thickness_dir_len_fn = VuiThickness_vertical;
		bar_content_length -= VuiThickness_vertical(&style->padding);
	}

	float min_slider_len = style->slider_width;
	// calculate the long side length of the slider that is clamp to a min(min_slider_len) and vui_max(bar_content_length)
	float unclamped_slider_len = bar_content_length * container_length / content_length;
	float slider_len = vui_min(bar_content_length, vui_max(min_slider_len, unclamped_slider_len));

	*slider_long_side_length = slider_len - thickness_dir_len_fn(&slider_styles->margin);

	//
	// work out the slider offset
	VuiVec2 slider_offset_vec = VuiVec2_zero;
	// make a max slider offset for the slider and unclamped slider.
	// then use these to create a ratio that we use to tranlate
	// scroll bar inner len into a coordinate space that suits the clamped slider
	float max_slider_offset = bar_content_length - slider_len;
	float unclampled_max_slider_offset = bar_content_length - unclamped_slider_len;
	float max_slider_offset_clamp_ratio = unclampled_max_slider_offset == 0.f ? 0.f : max_slider_offset / unclampled_max_slider_offset;

	// convert content offset into a slider offset
	// content_offset is always <= 0 and slider_offset is >= 0
	float slider_offset = ((-*content_offset_in_out / content_length) * (bar_content_length * max_slider_offset_clamp_ratio));
	slider_offset = vui_max(0.f, vui_min(slider_offset, max_slider_offset));
	if (is_horizontal) slider_offset_vec.x = slider_offset;
	else slider_offset_vec.y = slider_offset;

	VuiBool offset_changed = vui_false;
	vui_scope_width(slider_size.x)
	vui_scope_height(slider_size.y)
	vui_scope_align(VuiAlign_left_top)
	vui_scope_offset(slider_offset_vec.x, slider_offset_vec.y) {
		VuiFocusState state = vui_button_start(vui_sib_id, slider_styles);
		offset_changed = (state & VuiFocusState_held) == VuiFocusState_held;
		if (offset_changed) {
			float pos_offset = is_horizontal
				? _vui.input.mouse.offset_x
				: _vui.input.mouse.offset_y;

			// offset the slider offset and convert the offset into a content offset
			slider_offset = vui_max(0.f, vui_min(slider_offset + pos_offset, max_slider_offset));
			*content_offset_in_out = max_slider_offset_clamp_ratio == 0.f ? 0.f : -(slider_offset / (bar_content_length * max_slider_offset_clamp_ratio)) * content_length;
		}
		vui_button_end();
	}

	vui_ctrl_end();
	return offset_changed;
}

void vui_scroll_view_end() {
	vui_ctrl_end();

	VuiCtrlId scroll_view_ctrl_id = _vui.build.parent_ctrl_id;
	VuiCtrl* scroll_view = vui_ctrl_get(scroll_view_ctrl_id);
	VuiCtrl* content = vui_ctrl_get(scroll_view->scroll_content_id);
	VuiVec2 content_size = VuiRect_size(content->rect);
	VuiVec2 container_size = VuiRect_size(scroll_view->rect);
	const VuiCtrlStyle* style = &scroll_view->style;
	//
	// the child styles are only in the VuiCtrlState_default style
	const VuiCtrlStyle* bar_styles = scroll_view->styles[0].bar_styles;
	const VuiCtrlStyle* slider_styles = bar_styles[0].slider_styles;
	VuiCtrlFlags flags = scroll_view->flags;
	VuiBool can_show_vertical_bar = flags & VuiCtrlFlags_scrollable_vertical;
	VuiBool can_show_horizontal_bar = flags & VuiCtrlFlags_scrollable_horizontal;
	VuiBool show_vertical_bar = vui_false;
	VuiBool show_horizontal_bar = vui_false;

	float vertical_scroll_bar_outer_width = bar_styles->slider_width +
		VuiThickness_horizontal(&bar_styles->margin) + VuiThickness_horizontal(&bar_styles->padding) +
		bar_styles->border_width * 2.f + VuiThickness_horizontal(&bar_styles->slider_styles->margin);

	float horizontal_scroll_bar_outer_height = bar_styles->slider_width +
		VuiThickness_vertical(&bar_styles->margin) + VuiThickness_vertical(&bar_styles->padding) +
		bar_styles->border_width * 2.f + VuiThickness_vertical(&bar_styles->slider_styles->margin);


	//
	// determine whether each of the scroll bars are visible or not.
	// and use this to workout the size of the container (the viewport of the scrollable content)
	//
	{
		container_size.x -= style->border_width * 2.f + VuiThickness_horizontal(&style->padding);
		container_size.y -= style->border_width * 2.f + VuiThickness_vertical(&style->padding);

		if (can_show_vertical_bar) {
			show_vertical_bar = flags & VuiCtrlFlags_scrollable_vertical_always_show || content_size.y > container_size.y;
		}

		if (show_vertical_bar) {
			container_size.x -= vertical_scroll_bar_outer_width;
			scroll_view->flags |= _VuiCtrlFlags_show_vertical_bar;
		} else {
			scroll_view->flags &= ~_VuiCtrlFlags_show_vertical_bar;
		}

		if (can_show_horizontal_bar) {
			show_horizontal_bar = flags & VuiCtrlFlags_scrollable_horizontal_always_show;
			if (!show_horizontal_bar) {
				show_horizontal_bar = content_size.x > container_size.x;
			}
		}

		if (show_horizontal_bar) {
			container_size.y -= horizontal_scroll_bar_outer_height;
			scroll_view->flags |= _VuiCtrlFlags_show_horizontal_bar;

			if (can_show_vertical_bar && !show_vertical_bar) {
				show_vertical_bar = content_size.y > container_size.y;
				if (show_vertical_bar) {
					container_size.x -= vertical_scroll_bar_outer_width;
					scroll_view->flags |= _VuiCtrlFlags_show_vertical_bar;
				} else {
					scroll_view->flags &= ~_VuiCtrlFlags_show_vertical_bar;
				}
			}

		} else {
			scroll_view->flags &= ~_VuiCtrlFlags_show_horizontal_bar;
		}
	}

	//
	// WARNING: do not use the variable scroll_view past here.
	// copy the data out fo the scroll area, as when we start new controls
	// they can reallocate the control pool and the pointer will be invalid.
	VuiVec2 scroll_offset = scroll_view->scroll_offset;
	VuiVec2 scroll_view_size = scroll_view->scroll_view_size;

	//
	// workout the long side lengths of the scroll bars
	float scroll_bar_length_horizontal = scroll_view_size.x - style->border_width * 2 -
		VuiThickness_horizontal(&style->padding) - VuiThickness_horizontal(&bar_styles->margin);
	float scroll_bar_length_vertical = scroll_view_size.y - style->border_width * 2 -
		VuiThickness_vertical(&style->padding) - VuiThickness_vertical(&bar_styles->margin);
	if (flags & VuiCtrlFlags_resizable || show_vertical_bar) {
		scroll_bar_length_horizontal -= vertical_scroll_bar_outer_width;
	}
	if (flags & VuiCtrlFlags_resizable) {
		scroll_bar_length_vertical -= horizontal_scroll_bar_outer_height;
	}

	//
	// now setup the scroll bars and resize button if they are enabled.
	//
	VuiFocusState state = VuiFocusState_none;
	VuiBool offset_changed = vui_false;
	vui_scope_align(VuiAlign_left_top)
	vui_scope_offset(0.f, 0.f)
	vui_scope_layout_wrap(vui_false)
	vui_scope_layout_spacing(0.f)
	vui_scope_layout_wrap_spacing(0.f) {
		if (show_horizontal_bar) {
			vui_scope_align(VuiAlign_left_bottom)
				offset_changed = _vui_scroll_bar(-2, scroll_bar_length_horizontal, container_size.x, &scroll_offset.x, content_size.x, vui_true, bar_styles);
		}

		if (show_vertical_bar) {
			vui_scope_align(VuiAlign_right_top)
			offset_changed = _vui_scroll_bar(-3, scroll_bar_length_vertical, container_size.y, &scroll_offset.y, content_size.y, vui_false, bar_styles);
		}

		if (flags & VuiCtrlFlags_resizable) {
			vui_scope_width(vui_auto_len)
			vui_scope_height(vui_auto_len)
			vui_scope_align(VuiAlign_right_bottom)
			vui_ctrl_start(-4, bar_styles);
			vui_scope_width(bar_styles->slider_width)
			vui_scope_height(bar_styles->slider_width)
			state = vui_button(-4, bar_styles->slider_styles);
			vui_ctrl_end();
			if (state & VuiFocusState_held) {
				VuiVec2 min;
				min.x = vertical_scroll_bar_outer_width * 2.f;
				min.y = horizontal_scroll_bar_outer_height * 2.f;

				scroll_view_size.x += _vui.input.mouse.offset_x;
				scroll_view_size.y += _vui.input.mouse.offset_y;
				scroll_view_size = VuiVec2_max(min, scroll_view_size);
			}
		}
	}

	//
	// now store the data back into the scroll view as it could
	// have changed if we interacted with the scroll view.
	//
	scroll_view = vui_ctrl_get(scroll_view_ctrl_id);
	VuiCtrl_set_scroll_offset(scroll_view, scroll_offset, offset_changed);
	scroll_view->scroll_view_size = scroll_view_size;
	if ((vui_ctrl_is_mouse_scroll_focused(scroll_view->id) || state == VuiFocusState_held || state == VuiFocusState_pressed) && _vui.build.mouse_scroll_focused_size) {
		*_vui.build.mouse_scroll_focused_size = scroll_view_size;
	}

	vui_ctrl_end();
}

static VuiBool _vui_text_box(VuiCtrlSibId sib_id, char* string_in_out, uint32_t string_in_out_cap, const VuiCtrlStyle styles[VuiCtrlState_COUNT], _VuiInputBoxType type, VuiBool is_multiline, VuiVec2* content_offset_in_out, VuiVec2* size_in_out, VuiScrollFlags flags) {
	vui_scope_height(vui_auto_len) {
		if (is_multiline) {
			flags |= VuiCtrlFlags_focusable;
			flags |= VuiCtrlFlags_focusable_no_keyboard_focus_nav;
			flags |= VuiCtrlFlags_focusable_no_keyboard_actions;
			vui_scroll_view_start_(sib_id, content_offset_in_out, size_in_out, flags, styles);
		} else {
			vui_ctrl_start_(sib_id, VuiCtrlFlags_focusable | VuiCtrlFlags_scrollable_horizontal | VuiCtrlFlags_focusable_no_keyboard_actions, 0, styles, NULL);
		}
	}

	VuiCtrlId text_box_ctrl_id = _vui.build.parent_ctrl_id;
	if (is_multiline) {
		VuiCtrl* ctrl = vui_ctrl_get(text_box_ctrl_id);
		text_box_ctrl_id = ctrl->parent_id;
	}
	VuiCtrl* ctrl = vui_ctrl_get(text_box_ctrl_id);

	const VuiCtrlStyle* style = &ctrl->style;
	//
	// the child styles are only in the VuiCtrlState_default style
	const VuiCtrlStyle* text_styles = styles[0].text_styles;

	// let the input box (non text box) use the internal string buffer
	if (vui_ctrl_is_focused(ctrl->id) && type != _VuiInputBoxType_text) {
		string_in_out = _vui.input.input_box.edit_string;
		string_in_out_cap = _vui_input_box_cap;
	}

	VuiBool has_changed = vui_false;
	if (vui_ctrl_is_focused(ctrl->id)) {
		if (_vui.input.focused_text_box.string == NULL) { // if gained focus
			if (type != _VuiInputBoxType_text) {
				// the input boxes (non text box) will create a string version of its value.
				// so if we gain focus, copy the result to the edit buffer.
				strncpy(_vui.input.input_box.edit_string, _vui.input.input_box.string, _vui_input_box_cap);
			}
			_vui.input.focused_text_box.string = string_in_out;
			_vui.input.focused_text_box.string_len = strlen(string_in_out);
			_vui.input.focused_text_box.string_cap = string_in_out_cap;
			_vui.input.focused_text_box.cursor_idx = 0;
			_vui.input.focused_text_box.select_offset = 0;
			_vui.input.focused_text_box.type = type;
			_vui.input.focused_text_box.is_multiline = is_multiline;
		} else {
			// we have had focus before and still do.
			// check to see if the text has changed.
			has_changed = _vui.input.focused_text_box.has_changed;
		}

		if (ctrl->focus_state & VuiFocusState_pressed) {
			VuiVec2 mouse_pos = VuiVec2_init(_vui.input.mouse.x, _vui.input.mouse.y);
			VuiVec2 left_top;
			left_top.x = ctrl->rect.left_top.x + style->padding.left_top.x + text_styles->margin.left_top.x + text_styles->padding.left_top.x + style->border_width;
			left_top.y = ctrl->rect.left_top.y + style->padding.left_top.y + text_styles->margin.left_top.y + text_styles->padding.left_top.y + style->border_width;
			left_top.x += ctrl->scroll_offset.x;
			left_top.y += ctrl->scroll_offset.y;

			_vui.input.focused_text_box.cursor_idx = vui_get_text_cursor_idx(
					_vui.input.focused_text_box.string, _vui.input.focused_text_box.string_len, 0.f,
					text_styles->font_id, text_styles->text_line_height, left_top, mouse_pos);
			_vui.input.focused_text_box.has_cursor_moved = vui_true;
		} else if (ctrl->focus_state & VuiFocusState_held) {
			VuiVec2 mouse_pos = VuiVec2_init(_vui.input.mouse.x, _vui.input.mouse.y);
			VuiVec2 left_top;
			left_top.x = ctrl->rect.left_top.x + style->padding.left_top.x + text_styles->margin.left_top.x + text_styles->padding.left_top.x + style->border_width;
			left_top.y = ctrl->rect.left_top.y + style->padding.left_top.y + text_styles->margin.left_top.y + text_styles->padding.left_top.y + style->border_width;
			left_top.x += ctrl->scroll_offset.x;
			left_top.y += ctrl->scroll_offset.y;

			uint32_t select_cursor_idx = vui_get_text_cursor_idx(
					_vui.input.focused_text_box.string, _vui.input.focused_text_box.string_len, 0.f,
					text_styles->font_id, text_styles->text_line_height, left_top, mouse_pos);
			_vui.input.focused_text_box.select_offset = select_cursor_idx - _vui.input.focused_text_box.cursor_idx;
			_vui.input.focused_text_box.has_cursor_moved = vui_true;
		}

		if (_vui.input.focused_text_box.has_cursor_moved || _vui.input.focused_text_box.has_cursor_moved_last_frame) {
			//
			// workout the scroll offset of the scroll when the cursor is outside of the box's inner boundary.
			// only if the cursor has moved.
			//

			float margin_padding_x = VuiThickness_horizontal(&style->padding) + VuiThickness_horizontal(&text_styles->margin) + VuiThickness_horizontal(&text_styles->padding);
			float box_inner_size_x = VuiRect_width(&ctrl->rect) - margin_padding_x - style->border_width * 2.f;

			float margin_padding_y = VuiThickness_vertical(&style->padding) + VuiThickness_vertical(&text_styles->margin) + VuiThickness_vertical(&text_styles->padding);
			float box_inner_size_y = VuiRect_height(&ctrl->rect) - margin_padding_y - style->border_width * 2.f;

			if (is_multiline) {
				//
				// the child styles are only in the VuiCtrlState_default style
				const VuiCtrlStyle* bar_styles = styles[0].bar_styles;
				const VuiCtrlStyle* slider_styles = bar_styles[0].slider_styles;

				if (ctrl->flags & _VuiCtrlFlags_show_vertical_bar) {
					//
					// we have a vertical scroll bar, so remove the outer width of the scroll bar from the inner size of the box
					box_inner_size_x -= bar_styles->slider_width +
						VuiThickness_horizontal(&bar_styles->margin) + VuiThickness_horizontal(&bar_styles->padding) +
						bar_styles->border_width * 2.f + VuiThickness_horizontal(&slider_styles->margin);
				}
				if (ctrl->flags & _VuiCtrlFlags_show_horizontal_bar) {
					//
					// we have a horizontal scroll bar, so remove the outer height of the scroll bar from the inner size of the box
					box_inner_size_y -= bar_styles->slider_width +
						VuiThickness_vertical(&bar_styles->margin) + VuiThickness_vertical(&bar_styles->padding) +
						bar_styles->border_width * 2.f + VuiThickness_vertical(&slider_styles->margin);
				}
			}


			//
			// get the offset of the cursor from the top left of the inner rectangle of the control.
			//
			uint32_t cursor_idx = _vui.input.focused_text_box.cursor_idx + _vui.input.focused_text_box.select_offset;
			VuiVec2 cursor_offset = vui_get_text_cursor_pos(_vui.input.focused_text_box.string, _vui.input.focused_text_box.string_len, 0.f, text_styles->font_id, text_styles->text_line_height, cursor_idx);


			float scroll_offset_x = -cursor_offset.x + box_inner_size_x;
			scroll_offset_x = vui_min(scroll_offset_x, 0.f);

			ctrl->scroll_offset.x = scroll_offset_x;

			//
			// move the scroll offset for the y axis.
			// see the comments above for the x axis and apply them to this code.
			//

			if (is_multiline) {
				float scroll_offset_y = ctrl->scroll_offset.y;
				float cursor_offset_rel_y = cursor_offset.y + text_styles->text_line_height + scroll_offset_y;

				if (ctrl->focus_state & VuiFocusState_held) {
					if (cursor_offset_rel_y >= box_inner_size_y) {
						scroll_offset_y -= vui_text_box_select_scroll_amount;
					} else if (cursor_offset_rel_y < text_styles->text_line_height) {
						scroll_offset_y += vui_text_box_select_scroll_amount;
					}
				} else {
					if (cursor_offset_rel_y >= box_inner_size_y) {
						scroll_offset_y -= (cursor_offset_rel_y - box_inner_size_y) + margin_padding_y;
					} else if (cursor_offset_rel_y < text_styles->text_line_height) {
						scroll_offset_y += (0.f - cursor_offset_rel_y) + text_styles->text_line_height + margin_padding_y;
					}
				}

				scroll_offset_y = vui_clamp(scroll_offset_y, -(cursor_offset.y + text_styles->text_line_height), 0.f);

				ctrl->scroll_offset.y = scroll_offset_y;
			}
		}
	}

	vui_scope_offset(0.f, 0.f)
	vui_scope_align(VuiAlign_left_top) {
		//
		// the text of the text box
		vui_text_(vui_sib_id, string_in_out, strlen(string_in_out), 0.f, text_styles);
		VuiCtrl* text_ctrl = vui_ctrl_get(_vui.build.sibling_prev_ctrl_id);
		ctrl = vui_ctrl_get(text_box_ctrl_id);
		if (!is_multiline) {
			ctrl->scroll_content_id = text_ctrl->id;
		}

		//
		// the control for the cursor
		vui_scope_width(vui_fill_len)
		vui_scope_height(vui_fill_len)
		vui_ctrl_start_(vui_sib_id, 0, 0, NULL, VuiTextBoxCursor_render);
		vui_ctrl_end();
	}

	if (is_multiline) {
		vui_scroll_view_end();
	} else {
		vui_ctrl_end();
	}
	return has_changed;
}

VuiBool vui_text_box(VuiCtrlSibId sib_id, char* string_in_out, uint32_t string_in_out_cap, const VuiCtrlStyle styles[VuiCtrlState_COUNT]) {
	vui_assert(string_in_out, "a string buffer must be provided");
	return _vui_text_box(sib_id, string_in_out, string_in_out_cap, styles, _VuiInputBoxType_text, vui_false, NULL, NULL, 0);
}

VuiBool vui_input_box_uint(VuiCtrlSibId sib_id, uint32_t* value, const VuiCtrlStyle styles[VuiCtrlState_COUNT]) {
	char* string = _vui.input.input_box.string;
	snprintf(string, _vui_input_box_cap, "%u", *value);

	VuiBool has_changed = _vui_text_box(sib_id, string, _vui_input_box_cap, styles, _VuiInputBoxType_u32, vui_false, NULL, NULL, 0);

	VuiCtrl* text_box = vui_ctrl_get(_vui.build.sibling_prev_ctrl_id);
	if (vui_ctrl_is_focused(text_box->id)) {
		char* string = _vui.input.input_box.edit_string;
		*value = strtoul(string, NULL, 10);
	}
	return has_changed;
}

VuiBool vui_input_box_sint(VuiCtrlSibId sib_id, int32_t* value, const VuiCtrlStyle styles[VuiCtrlState_COUNT]) {
	char* string = _vui.input.input_box.string;
	snprintf(string, _vui_input_box_cap, "%d", *value);

	VuiBool has_changed = _vui_text_box(sib_id, string, _vui_input_box_cap, styles, _VuiInputBoxType_s32, vui_false, NULL, NULL, 0);

	VuiCtrl* text_box = vui_ctrl_get(_vui.build.sibling_prev_ctrl_id);
	if (vui_ctrl_is_focused(text_box->id)) {
		char* string = _vui.input.input_box.edit_string;
		*value = strtol(string, NULL, 10);
	}
	return has_changed;
}

VuiBool vui_input_box_float(VuiCtrlSibId sib_id, float* value, const VuiCtrlStyle styles[VuiCtrlState_COUNT]) {
	char* string = _vui.input.input_box.string;
	snprintf(string, _vui_input_box_cap, "%f", *value);

	VuiBool has_changed = _vui_text_box(sib_id, string, _vui_input_box_cap, styles, _VuiInputBoxType_float, vui_false, NULL, NULL, 0);

	VuiCtrl* text_box = vui_ctrl_get(_vui.build.sibling_prev_ctrl_id);
	if (vui_ctrl_is_focused(text_box->id)) {
		char* string = _vui.input.input_box.edit_string;
		*value = strtod(string, NULL);
	}
	return has_changed;
}

VuiBool vui_text_box_multiline_(VuiCtrlSibId sib_id, char* string_in_out, uint32_t string_in_out_cap, VuiVec2* content_offset_in_out, VuiVec2* size_in_out, VuiScrollFlags flags, const VuiCtrlStyle styles[VuiCtrlState_COUNT]) {
	vui_assert(string_in_out, "a string buffer must be provided");
	return _vui_text_box(sib_id, string_in_out, string_in_out_cap, styles, _VuiInputBoxType_text, vui_true, content_offset_in_out, size_in_out, flags);
}

void vui_popover_start(VuiCtrlSibId sib_id, VuiBool* is_open, VuiCtrlId target_ctrl_id, const VuiCtrlStyle styles[VuiCtrlState_COUNT]) {
	vui_ctrl_start_(sib_id, _VuiCtrlFlags_is_popover, 0, styles, NULL);
	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);
	ctrl->popover_target_ctrl_id = target_ctrl_id;
	if (is_open) {
		ctrl->popover_is_open_ptr = is_open;
		if (*is_open) {
			ctrl->flags |= _VuiCtrlFlags_is_popover_open;
		} else {
			ctrl->flags &= ~_VuiCtrlFlags_is_popover_open;
		}
	} else {
		ctrl->flags |= _VuiCtrlFlags_is_popover_open;
	}

	VuiCtrl* target_ctrl = vui_ctrl_get(target_ctrl_id);
	vui_assert(ctrl->id != target_ctrl_id, "popover cannot target itself");
	vui_assert(
		!(target_ctrl->flags & _VuiCtrlFlags_is_popover) || target_ctrl->popover_target_ctrl_id != ctrl->id,
		"popover cannot target another popover that targets this popover"
	);
}

void vui_popover_end(void) {
	vui_ctrl_end();
}

void vui_canvas_start(VuiCtrlSibId sib_id, const VuiCtrlStyle styles[VuiCtrlState_COUNT]) {
	vui_ctrl_start_(sib_id, _VuiCtrlFlags_is_canvas, 0, styles, VuiCanvas_render);
	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);
	VuiStk_clear(ctrl->canvas_items);
}

void vui_canvas_end(void) {
	vui_ctrl_end();
}

static VuiCanvasItem* _vui_canvas_item_new() {
	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);
	vui_assert(ctrl->flags & _VuiCtrlFlags_is_canvas, "vui_canvas_* function must be called within vui_canvas_start & vui_canvas_end");
	return VuiStk_push(&ctrl->canvas_items);
}

void vui_canvas_line(VuiVec2 start_pos, VuiVec2 end_pos, VuiColor color, float width) {
	VuiCanvasItem* item = _vui_canvas_item_new();
	item->type = VuiCanvasItemType_line;
	item->line.start_pos = start_pos;
	item->line.end_pos = end_pos;
	item->line.width = width;
	item->color = color;
}

void vui_canvas_line_dotted(VuiVec2 start_pos, VuiVec2 end_pos, VuiColor color, float width, float gap) {
	VuiCanvasItem* item = _vui_canvas_item_new();
	item->type = VuiCanvasItemType_line_dotted;
	item->line_dotted.start_pos = start_pos;
	item->line_dotted.end_pos = end_pos;
	item->line_dotted.width = width;
	item->line_dotted.gap = gap;
	item->color = color;
}

void vui_canvas_line_dashed(VuiVec2 start_pos, VuiVec2 end_pos, VuiColor color, float width, float dash_length, float gap) {
	VuiCanvasItem* item = _vui_canvas_item_new();
	item->type = VuiCanvasItemType_line_dashed;
	item->line_dashed.start_pos = start_pos;
	item->line_dashed.end_pos = end_pos;
	item->line_dashed.width = width;
	item->line_dashed.dash_length = dash_length;
	item->line_dashed.gap = gap;
	item->color = color;
}

void vui_canvas_rect(const VuiRect* rect, VuiColor color, float radius) {
	VuiCanvasItem* item = _vui_canvas_item_new();
	item->type = VuiCanvasItemType_rect;
	item->rect.rect = *rect;
	item->rect.radius = radius;
	item->color = color;
}

void vui_canvas_rect_border(const VuiRect* rect, VuiColor color, float radius, float width) {
	VuiCanvasItem* item = _vui_canvas_item_new();
	item->type = VuiCanvasItemType_rect_border;
	item->rect.rect = *rect;
	item->rect.radius = radius;
	item->rect.width = width;
	item->color = color;
}

void vui_canvas_triangle(VuiVec2 a, VuiVec2 b, VuiVec2 c, VuiColor color) {
	VuiCanvasItem* item = _vui_canvas_item_new();
	item->type = VuiCanvasItemType_triangle;
	item->triangle.a = a;
	item->triangle.b = b;
	item->triangle.c = c;
	item->color = color;
}

void vui_canvas_triangle_border(VuiVec2 a, VuiVec2 b, VuiVec2 c, VuiColor color, float width) {
	VuiCanvasItem* item = _vui_canvas_item_new();
	item->type = VuiCanvasItemType_triangle_border;
	item->triangle.a = a;
	item->triangle.b = b;
	item->triangle.c = c;
	item->triangle.width = width;
	item->color = color;
}

void vui_canvas_circle(VuiVec2 pos, float radius, VuiColor color) {
	VuiCanvasItem* item = _vui_canvas_item_new();
	item->type = VuiCanvasItemType_circle;
	item->circle.pos = pos;
	item->circle.radius = radius;
	item->color = color;
}

void vui_canvas_circle_border(VuiVec2 pos, float radius, VuiColor color, float width) {
	VuiCanvasItem* item = _vui_canvas_item_new();
	item->type = VuiCanvasItemType_circle;
	item->circle.pos = pos;
	item->circle.radius = radius;
	item->circle.width = width;
	item->color = color;
}

void vui_canvas_text(VuiVec2 pos, VuiFontId font_id, float line_height, char* text, uint32_t text_length, VuiColor color, float word_wrap_at_width) {
	vui_assert(vui_false, "unimplemented");
}

void vui_canvas_polyline(VuiVec2* points, uint32_t points_count, VuiColor color, float width, VuiBool connect_first_and_last) {

	vui_assert(vui_false, "unimplemented");
}

void vui_canvas_convex_polygon(VuiVec2* points, uint32_t points_count, VuiColor color) {

	vui_assert(vui_false, "unimplemented");
}

void vui_canvas_bezier_curve(VuiVec2 points[4], VuiColor color, float width) {
	VuiCanvasItem* item = _vui_canvas_item_new();
	item->type = VuiCanvasItemType_bezier_curve;
	memcpy(item->bezier_curve.points, points, sizeof(*points) * 4);
	item->bezier_curve.width = width;
	item->color = color;
}

// ===========================================================================================
//
//
// Vui Public API
//
//
// ===========================================================================================

char* VuiLayoutType_strings[] = {
	[VuiLayoutType_stack] = "stack",
	[VuiLayoutType_row] = "row",
	[VuiLayoutType_column] = "column",
};

#define _VuiImageId_pool_id_MASK  0x000fffff
#define _VuiImageId_pool_id_SHIFT 0
#define _VuiImageId_counter_MASK  0xfff00000
#define _VuiImageId_counter_SHIFT 20

VuiImageId vui_image_add(VuiImage* image) {
	VuiPoolId pool_id = 0;
	_VuiImage* dst = _VuiPool_alloc((_VuiPool*)&_vui.image_pool, &pool_id, sizeof(_VuiImage), alignof(_VuiImage));
	dst->inner = *image;
	VuiImageId image_id = (pool_id << _VuiImageId_pool_id_SHIFT) & _VuiImageId_pool_id_MASK;
	image_id |= (dst->counter << _VuiImageId_counter_SHIFT) & _VuiImageId_counter_MASK;
	return image_id;
}

VuiImage* vui_image_get(VuiImageId image_id) {
	vui_assert(image_id, "cannot get an image with a NULL identifier");
	VuiPoolId pool_id = (image_id & _VuiImageId_pool_id_MASK) >> _VuiImageId_pool_id_SHIFT;
	uint16_t counter = (image_id & _VuiImageId_counter_MASK) >> _VuiImageId_counter_SHIFT;
	_VuiImage* image = _VuiPool_id_to_ptr((_VuiPool*)&_vui.image_pool, pool_id, sizeof(_VuiImage));
	vui_assert(image->counter == counter, "trying to get an image with an old identifier");
	return &image->inner;
}

void vui_image_remove(VuiImageId image_id) {
	vui_assert(image_id, "cannot remove an image with a NULL identifier");
	VuiPoolId pool_id = (image_id & _VuiImageId_pool_id_MASK) >> _VuiImageId_pool_id_SHIFT;
	uint16_t counter = (image_id & _VuiImageId_counter_MASK) >> _VuiImageId_counter_SHIFT;
	_VuiImage* image = _VuiPool_id_to_ptr((_VuiPool*)&_vui.image_pool, pool_id, sizeof(_VuiImage));
	vui_assert(image->counter == counter, "trying to remove an image with an old identifier");
	_VuiPool_dealloc((_VuiPool*)&_vui.image_pool, pool_id, sizeof(_VuiImage), alignof(_VuiImage));
}

VuiBool vui_init(VuiSetup* setup) {
	_vui = (_Vui){0};
	_vui.position_text_fn = setup->position_text_fn;
	_vui.position_text_userdata = setup->position_text_userdata;
	_vui.allocator = setup->allocator;
	_VuiArenaAlctor_init(&_vui.frame_data_alctor);
	_vui.windows = vui_mem_alloc_array(_VuiWindow, _vui.allocator, setup->windows_count);
	memset(_vui.windows, 0, setup->windows_count * sizeof(*_vui.windows));
	_vui.windows_count = setup->windows_count;
	_VuiPool_init((_VuiPool*)&_vui.ctrl_pool, setup->ctrls_init_cap ? setup->ctrls_init_cap : _vui_ctrls_init_cap, sizeof(_VuiCtrl), alignof(_VuiCtrl));

	vui_ss.text_header[VuiCtrlState_default].font_id = setup->default_font_id;
	vui_ss.text_header[VuiCtrlState_focused].font_id = setup->default_font_id;
	vui_ss.text_header[VuiCtrlState_active].font_id = setup->default_font_id;
	vui_ss.text_header[VuiCtrlState_disabled].font_id = setup->default_font_id;
	vui_ss.text_menu[VuiCtrlState_default].font_id = setup->default_font_id;
	vui_ss.text_menu[VuiCtrlState_focused].font_id = setup->default_font_id;
	vui_ss.text_menu[VuiCtrlState_active].font_id = setup->default_font_id;
	vui_ss.text_menu[VuiCtrlState_disabled].font_id = setup->default_font_id;
	return vui_true;
}

void _vui_find_mouse_focused_ctrls(VuiCtrl* ctrl, VuiBool is_root) {
	VuiVec2 mouse_pt = VuiVec2_init(_vui.input.mouse.x, _vui.input.mouse.y);
	VuiRect parent_clip_rect = _vui.render.clip_rect;
	if (ctrl->flags & _VuiCtrlFlags_is_popover) {
		if (!(ctrl->flags & _VuiCtrlFlags_is_popover_open))
			return;

		VuiCtrl* root_ctrl = vui_ctrl_get(_vui.build.w->root_ctrl_id);
		_vui.render.clip_rect = VuiRect_init(0, 0, root_ctrl->attributes.width, root_ctrl->attributes.height);
		if (_vui.input.mouse.buttons_has_been_pressed & VuiMouseButtons_left) {
			if (ctrl->popover_is_open_ptr && !VuiRect_intersects_pt(&ctrl->rect, mouse_pt)) {
				ctrl->flags &= ~_VuiCtrlFlags_is_popover_open;
				*ctrl->popover_is_open_ptr = vui_false;
			}
		}
	} else {
		_vui.render.clip_rect = VuiRect_clip(&_vui.render.clip_rect, &ctrl->rect);
	}

	if (!_vui.input.is_mouse_over_ctrl && !is_root && ctrl->style.bg_color.a != 0) {
		_vui.input.is_mouse_over_ctrl = VuiRect_intersects_pt(&ctrl->rect, mouse_pt);
	}

	if (VuiRect_intersects_pt(&_vui.render.clip_rect, mouse_pt)) {
		if (ctrl->flags & VuiCtrlFlags_focusable) {
			_vui_ctrl_set_mouse_focused(ctrl->id);
		}

		if (ctrl->flags & VuiCtrlFlags_focusable_scroll && ctrl->flags & (VuiCtrlFlags_scrollable_vertical | VuiCtrlFlags_scrollable_horizontal)) {
			_vui_ctrl_set_mouse_scroll_focused(ctrl->id);
		}
	}

	VuiCtrl* child = NULL;
	for (VuiCtrlId child_id = ctrl->child_first_id; child_id; child_id = child->sibling_next_id) {
		child = vui_ctrl_get(child_id);
		_vui_find_mouse_focused_ctrls(child, vui_false);
	}

	_vui.render.clip_rect = parent_clip_rect;
}

//
// unselects the text and moves the cursor to the furthest left selection
//
void _vui_text_nav_select_collapse_left() {
	uint32_t idx = _vui.input.focused_text_box.cursor_idx;
	uint32_t select_end_idx = idx + _vui.input.focused_text_box.select_offset;
	if (select_end_idx < idx) {
		_vui.input.focused_text_box.cursor_idx = select_end_idx;
	}
	_vui.input.focused_text_box.select_offset = 0;
}

//
// unselects the text and moves the cursor to the furthest right selection
//
void _vui_text_nav_select_collapse_right() {
	uint32_t idx = _vui.input.focused_text_box.cursor_idx;
	uint32_t select_end_idx = idx + _vui.input.focused_text_box.select_offset;
	if (select_end_idx > idx) {
		_vui.input.focused_text_box.cursor_idx = select_end_idx;
	}
	_vui.input.focused_text_box.select_offset = 0;
}

uint32_t _vui_text_nav_word_start(uint32_t idx) {
	char* string = _vui.input.focused_text_box.string;
	if (idx == 0) return 0;

	int32_t codept = 0;

	//
	// navigate to the previous non-whitespace codepoint to see if we start on a word delimiter or not.
	idx -= 1;
	while (idx) {
		if (vui_utf8_is_codepoint_boundary(string[idx])) {
			vui_utf8_codepoint(&string[idx], &codept);
			if (!vui_utf8_is_whitespace(codept)) {
				break;
			}
		}
		idx -= 1;
	}

	if (idx == 0) return idx;

	//
	// get the codepoint and check for a word delimiter
	vui_utf8_codepoint(&string[idx], &codept);
	VuiBool is_on_delimiter = vui_utf8_is_word_delimiter(codept);

	//
	// navigate backwards by each codepoint.
	// when we reach a non-whitespace codepoint that is the opposite of is_on_delimiter,
	// then move forward by one codepoint to be back on the last codepoint that matched
	while (idx) {
		idx -= 1;
		while (idx && !vui_utf8_is_codepoint_boundary(string[idx])) {
			idx -= 1;
		}
		uint32_t codept_size = vui_utf8_codepoint(&string[idx], &codept);
		if (vui_utf8_is_whitespace(codept) || vui_utf8_is_word_delimiter(codept) != is_on_delimiter) {
			idx += codept_size;
			break;
		}
	}

	return idx;
}

uint32_t _vui_text_nav_word_end(uint32_t idx) {
	char* string = _vui.input.focused_text_box.string;
	uint32_t string_len = _vui.input.focused_text_box.string_len;
	if (idx == string_len) return idx;

	int32_t codept = 0;

	//
	// if we don't have already, navigate forward until we have a non-whitespace codepoint.
	while (idx < string_len) {
		uint32_t codept_size = vui_utf8_codepoint(&string[idx], &codept);
		if (!vui_utf8_is_whitespace(codept)) {
			break;
		}
		idx += codept_size;
	}

	if (idx == string_len) return idx;

	//
	// get the codepoint and see if we start on a word delimiter
	vui_utf8_codepoint(&string[idx], &codept);
	VuiBool is_on_delimiter = vui_utf8_is_word_delimiter(codept);

	//
	// navigate forwards by each codepoint.
	// when we reach a codepoint that is the opposite of is_on_delimiter.
	while (idx < string_len) {
		uint32_t codept_size = vui_utf8_codepoint(&string[idx], &codept);
		if (vui_utf8_is_whitespace(codept) || vui_utf8_is_word_delimiter(codept) != is_on_delimiter) {
			break;
		}
		idx += codept_size;
	}

	return idx;
}

uint32_t _vui_text_nav_home(uint32_t idx) {
	char* string = _vui.input.focused_text_box.string;
	if (idx == 0) return 0;

	//
	// move backwards until a newline character comes before the index.
	while (idx--) {
		if (vui_utf8_is_codepoint_boundary(string[idx])) {
			char ch = string[idx];
			if (ch == '\n') {
				idx += 1;
				break;
			}
		}
	}

	if (idx == UINT32_MAX)
		idx = 0;

	return idx;
}

uint32_t _vui_text_nav_end(uint32_t idx) {
	char* string = _vui.input.focused_text_box.string;
	uint32_t string_len = _vui.input.focused_text_box.string_len;
	if (idx == string_len) return idx;


	//
	// move forwards we are on a newline character.
	while (idx < string_len) {
		if (vui_utf8_is_codepoint_boundary(string[idx])) {
			char ch = string[idx];
			if (ch == '\r' || ch == '\n')
				break;
		}
		idx += 1;
	}

	return idx;
}

uint32_t _vui_text_column_idx(uint32_t idx, uint32_t* column_idx_out) {
	char* string = _vui.input.focused_text_box.string;
	uint32_t column_idx = 0;
	if (idx == 0) {
		*column_idx_out = 0;
		return 0;
	}

	//
	// TODO: this will not word for multi codepoint characters.
	// move backwards and count how many codepoints we are from the start of the line.
	while (idx--) {
		if (vui_utf8_is_codepoint_boundary(string[idx])) {
			char ch = string[idx];
			if (ch == '\n') {
				break;
			}
			column_idx += 1;
		}
	}

	if (idx == UINT32_MAX)
		idx = 0;

	*column_idx_out = column_idx;
	return idx;
}

uint32_t _vui_text_nav_up(uint32_t idx) {
	char* string = _vui.input.focused_text_box.string;
	uint32_t start_idx = idx;
	if (idx == 0) return 0;

	uint32_t column_idx = 0;
	idx = _vui_text_column_idx(idx, &column_idx);
	// move off the new line if we have \r\n encoding
	if (idx && string[idx - 1] == '\r') {
		idx -= 1;
	}


	//
	// store this column index if this is the first up or down key to be press.
	// if not restore the column index if we have not press another key over than up or down
	// and the value stored is bigger than our current column index.
	if (_vui.input.focused_text_box.cursor_max_last_unchanged_column_idx == 0) {
		_vui.input.focused_text_box.cursor_max_last_unchanged_column_idx = column_idx;
	} else if (_vui.input.focused_text_box.cursor_max_last_unchanged_column_idx > column_idx) {
		column_idx = _vui.input.focused_text_box.cursor_max_last_unchanged_column_idx;
	}

	if (idx == 0) {
		if (string[idx] == '\r' || string[idx] == '\n') {
			// empty first line, so return the first index
			return 0;
		} else {
			// else, we are navigating up from the first line, so do not move anywhere
			return start_idx;
		}
	}

	//
	// go to the start of the previous line
	idx = _vui_text_nav_home(idx);

	//
	// now move forwards until we reach the same column we where in on the other line.
	for (uint32_t i = 0; i < column_idx; i += 1) {
		int32_t codept = 0;
		uint32_t codept_size =  vui_utf8_codepoint(&string[idx], &codept);
		if (codept == '\n' || codept == '\r') {
			// stop if we reach a newline
			break;
		}
		idx += codept_size;
	}

	return idx;
}

uint32_t _vui_text_nav_down(uint32_t idx) {
	char* string = _vui.input.focused_text_box.string;
	uint32_t string_len = _vui.input.focused_text_box.string_len;
	uint32_t start_idx = idx;
	if (idx == string_len) return idx;

	uint32_t column_idx = 0;
	if (idx) {
		idx = _vui_text_column_idx(idx, &column_idx);
		if (idx || string[idx] == '\r' || string[idx] == '\n') {
			// move off the new line
			idx += 1;
		}
	}

	//
	// store this column index if this is the first up or down key to be press.
	// if not restore the column index if we have not press another key over than up or down
	// and the value stored is bigger than our current column index.
	if (_vui.input.focused_text_box.cursor_max_last_unchanged_column_idx == 0) {
		_vui.input.focused_text_box.cursor_max_last_unchanged_column_idx = column_idx;
	} else if (_vui.input.focused_text_box.cursor_max_last_unchanged_column_idx > column_idx) {
		column_idx = _vui.input.focused_text_box.cursor_max_last_unchanged_column_idx;
	}

	//
	// go to the start of the next line
	idx = _vui_text_nav_end(idx);
	if (idx == string_len) return start_idx;
	if (string[idx] == '\r') { // handle windows \r\n
		idx += 2;
	} else {
		idx += 1;
	}

	//
	// now move forwards until we reach the same column we where in on the other line.
	for (uint32_t i = 0; i < column_idx; i += 1) {
		if (idx >= string_len) break;
		int32_t codept = 0;
		uint32_t codept_size =  vui_utf8_codepoint(&string[idx], &codept);
		if (codept == '\n' || codept == '\r') {
			// stop if we reach a newline
			break;
		}
		idx += codept_size;
	}

	return idx;
}

void vui_frame_start(VuiBool right_to_left, float dt) {
	vui_assert(_vui.build.w == NULL, "cannot call vui_frame_start until vui_window_end has been called");
	_VuiArenaAlctor_reset(&_vui.frame_data_alctor);
	_vui.build.dt = dt;

	if (right_to_left) {
		_vui.flags |= _VuiFlags_right_to_left;
	} else {
		_vui.flags &= ~_VuiFlags_right_to_left;
	}

	_vui_ctrl_set_mouse_focused(0);
	_vui_ctrl_set_mouse_scroll_focused(0);
	_vui.input.is_mouse_over_ctrl = vui_false;

	_VuiWindow* windows = _vui.windows;
	uint16_t windows_count = _vui.windows_count;
	for (int i = 0; i < windows_count; i += 1) {
		_VuiWindow* w = &windows[i];
		_vui.build.w = w;

		if (_vui.mouse_focused_window_id == i && w->root_ctrl_id) {
			_vui.render.clip_rect = VuiRect_init(0, 0, w->size.x, w->size.y);
			_vui_find_mouse_focused_ctrls(vui_ctrl_get(w->root_ctrl_id), vui_true);
		}

		w->size.x = 0;
		w->size.y = 0;
		VuiStk_clear(w->text);
	}

	//
	// scroll the control that is scroll focused
	//
	if (_vui.mouse_scroll_focused_ctrl_id) {
		VuiCtrl* scroll_focused_ctrl = vui_ctrl_get(_vui.mouse_scroll_focused_ctrl_id);
		VuiVec2 scroll_offset = VuiVec2_init(
			scroll_focused_ctrl->scroll_offset.x + _vui.input.mouse.wheel_offset_x,
			scroll_focused_ctrl->scroll_offset.y + _vui.input.mouse.wheel_offset_y);
		VuiCtrl_set_scroll_offset(scroll_focused_ctrl, scroll_offset, vui_false);
	}

	//
	// process the keyboard input for the focused text/input box
	//
	if (_vui.input.focused_text_box.string) {
		uint32_t str_len = strlen(_vui.input.focused_text_box.string);
		VuiInputActions actions = _vui.input.actions;
		uint32_t cursor_idx = _vui.input.focused_text_box.cursor_idx;
		if (actions & VuiInputActions_left) {

			if ((actions & VuiInputActions_select_word_start) == VuiInputActions_select_word_start) {
				uint32_t start_idx = _vui.input.focused_text_box.cursor_idx + _vui.input.focused_text_box.select_offset;
				uint32_t idx = _vui_text_nav_word_start(start_idx);
				_vui.input.focused_text_box.select_offset += idx - start_idx;

			} else if ((actions & VuiInputActions_word_start) == VuiInputActions_word_start) {
				_vui_text_nav_select_collapse_left();
				_vui.input.focused_text_box.cursor_idx = _vui_text_nav_word_start(_vui.input.focused_text_box.cursor_idx);

			} else if ((actions & VuiInputActions_select_left) == VuiInputActions_select_left) {
				if (_vui.input.focused_text_box.cursor_idx + _vui.input.focused_text_box.select_offset) {
					_vui.input.focused_text_box.select_offset -= 1;
				}

			} else { // left key with no modifiers
				if (_vui.input.focused_text_box.cursor_idx) {
					if (_vui.input.focused_text_box.select_offset < 0) {
						_vui.input.focused_text_box.cursor_idx += _vui.input.focused_text_box.select_offset;
					} else if (_vui.input.focused_text_box.select_offset == 0) {
						_vui.input.focused_text_box.cursor_idx -= 1;
					}
				}
				_vui.input.focused_text_box.select_offset = 0;
			}

			_vui.input.focused_text_box.cursor_max_last_unchanged_column_idx = 0;
		} else if (actions & VuiInputActions_right) {

			if ((actions & VuiInputActions_select_word_end) == VuiInputActions_select_word_end) {
				uint32_t start_idx = _vui.input.focused_text_box.cursor_idx + _vui.input.focused_text_box.select_offset;
				uint32_t idx = _vui_text_nav_word_end(start_idx);
				_vui.input.focused_text_box.select_offset += idx - start_idx;

			} else if ((actions & VuiInputActions_word_end) == VuiInputActions_word_end) {
				_vui_text_nav_select_collapse_right();
				_vui.input.focused_text_box.cursor_idx = _vui_text_nav_word_end(_vui.input.focused_text_box.cursor_idx);

			} else if ((actions & VuiInputActions_select_right) == VuiInputActions_select_right) {
				if (_vui.input.focused_text_box.cursor_idx + _vui.input.focused_text_box.select_offset < str_len) {
					_vui.input.focused_text_box.select_offset += 1;
				}

			} else { // right key with no modifiers
				uint32_t end_idx = _vui.input.focused_text_box.cursor_idx + _vui.input.focused_text_box.select_offset;
				if (end_idx <= str_len && _vui.input.focused_text_box.select_offset > 0) {
					_vui.input.focused_text_box.cursor_idx += _vui.input.focused_text_box.select_offset;
				} else if (end_idx < str_len && _vui.input.focused_text_box.select_offset == 0) {
					_vui.input.focused_text_box.cursor_idx += 1;
				}

				_vui.input.focused_text_box.select_offset = 0;
			}

			_vui.input.focused_text_box.cursor_max_last_unchanged_column_idx = 0;
		} else if (actions & VuiInputActions_up) {

			if ((actions & VuiInputActions_select_up) == VuiInputActions_select_up) {
				uint32_t start_idx = _vui.input.focused_text_box.cursor_idx + _vui.input.focused_text_box.select_offset;
				uint32_t idx = _vui_text_nav_up(start_idx);
				_vui.input.focused_text_box.select_offset += idx - start_idx;

			} else { // up key with no modifiers
				_vui_text_nav_select_collapse_left();
				_vui.input.focused_text_box.cursor_idx = _vui_text_nav_up(_vui.input.focused_text_box.cursor_idx);
			}

		} else if (actions & VuiInputActions_down) {

			if ((actions & VuiInputActions_select_down) == VuiInputActions_select_down) {
				uint32_t start_idx = _vui.input.focused_text_box.cursor_idx + _vui.input.focused_text_box.select_offset;
				uint32_t idx = _vui_text_nav_down(start_idx);
				_vui.input.focused_text_box.select_offset += idx - start_idx;

			} else { // down key with no modifiers
				_vui_text_nav_select_collapse_right();
				_vui.input.focused_text_box.cursor_idx = _vui_text_nav_down(_vui.input.focused_text_box.cursor_idx);
			}

		} else if (actions & (VuiInputActions_backspace | VuiInputActions_delete)) {

			//
			// get the start and end index of the selection if there is one.
			uint32_t start_idx = 0;
			uint32_t end_idx = 0;
			if (_vui.input.focused_text_box.select_offset > 0) {
				start_idx = _vui.input.focused_text_box.cursor_idx;
				end_idx = _vui.input.focused_text_box.cursor_idx + _vui.input.focused_text_box.select_offset;
			} else {
				start_idx = _vui.input.focused_text_box.cursor_idx + _vui.input.focused_text_box.select_offset;
				end_idx = _vui.input.focused_text_box.cursor_idx;
			}

			if ((actions & VuiInputActions_delete) == VuiInputActions_delete) {

				if ((actions & VuiInputActions_delete_to_end_of_word) == VuiInputActions_delete_to_end_of_word) {
					end_idx = _vui_text_nav_word_end(end_idx);
				} else if (_vui.input.focused_text_box.select_offset == 0 && end_idx < str_len) {
					// nothing is selected so add one.
					end_idx += 1;
				}

			} else {

				if ((actions & VuiInputActions_delete_to_start_of_word) == VuiInputActions_delete_to_start_of_word) {
					start_idx = _vui_text_nav_word_start(start_idx);
				} else if (_vui.input.focused_text_box.select_offset == 0 && start_idx > 0) {
					// nothing is selected so add one.
					start_idx -= 1;
				}

			}

			_vui.input.focused_text_box.cursor_idx = start_idx;
			_vui.input.focused_text_box.select_offset = 0;

			if (start_idx != end_idx) {
				_vui.input.focused_text_box.has_changed = vui_true;
			}

			_vui.input.focused_text_box.string_len =
				_vui_string_remove_range_shift(_vui.input.focused_text_box.string, _vui.input.focused_text_box.string_len, start_idx, end_idx);

			_vui.input.focused_text_box.cursor_max_last_unchanged_column_idx = 0;
		} else if (actions & VuiInputActions_enter) {

			if (_vui.input.focused_text_box.is_multiline) {
				if (_vui.input.focused_text_box.string_len + 1 < _vui.input.focused_text_box.string_cap) {
					_vui_input_text_remove_selected();
					_vui_input_text_insert("\n", 1);
				}
			}

			_vui.input.focused_text_box.cursor_max_last_unchanged_column_idx = 0;
		} else if (actions & VuiInputActions_home) {

			if ((actions & VuiInputActions_select_to_document_home) == VuiInputActions_select_to_document_home) {
				_vui.input.focused_text_box.select_offset = -(int32_t)_vui.input.focused_text_box.cursor_idx;

			} else if ((actions & VuiInputActions_document_home) == VuiInputActions_document_home) {
				_vui.input.focused_text_box.cursor_idx = 0;
				_vui.input.focused_text_box.select_offset = 0;

			} else if ((actions & VuiInputActions_select_home) == VuiInputActions_select_home) {
				uint32_t start_idx = _vui.input.focused_text_box.cursor_idx + _vui.input.focused_text_box.select_offset;
				uint32_t idx = _vui_text_nav_home(start_idx);
				_vui.input.focused_text_box.select_offset += idx - start_idx;

			} else { // home key no modifiers
				_vui.input.focused_text_box.cursor_idx = _vui_text_nav_home(_vui.input.focused_text_box.cursor_idx);
				_vui.input.focused_text_box.select_offset = 0;
			}

			_vui.input.focused_text_box.cursor_max_last_unchanged_column_idx = 0;
		} else if (actions & VuiInputActions_end) {

			if ((actions & VuiInputActions_select_to_document_end) == VuiInputActions_select_to_document_end) {
				_vui.input.focused_text_box.select_offset = str_len - _vui.input.focused_text_box.cursor_idx;

			} else if ((actions & VuiInputActions_document_end) == VuiInputActions_document_end) {
				_vui.input.focused_text_box.cursor_idx = str_len;
				_vui.input.focused_text_box.select_offset = 0;

			} else if ((actions & VuiInputActions_select_end) == VuiInputActions_select_end) {
				uint32_t start_idx = _vui.input.focused_text_box.cursor_idx + _vui.input.focused_text_box.select_offset;
				uint32_t idx = _vui_text_nav_end(start_idx);
				_vui.input.focused_text_box.select_offset += idx - start_idx;

			} else {
				_vui.input.focused_text_box.cursor_idx = _vui_text_nav_end(_vui.input.focused_text_box.cursor_idx);
				_vui.input.focused_text_box.select_offset = 0;

			}

			_vui.input.focused_text_box.cursor_max_last_unchanged_column_idx = 0;
		}

		_vui.input.focused_text_box.has_cursor_moved =
			_vui.input.focused_text_box.has_cursor_moved || cursor_idx != _vui.input.focused_text_box.cursor_idx;
	}

	_vui.build.w = NULL;
}

void vui_frame_end() {
	vui_assert(_vui.build.w == NULL, "cannot call vui_frame_end until vui_window_end has been called");

	if ((_vui.input.actions & VuiInputActions_focus_prev) == VuiInputActions_focus_prev) {
		_VuiWindow* w = &_vui.windows[_vui.focused_window_id];
		VuiCtrl* ctrl = vui_ctrl_get(w->focused_ctrl_id ? w->focused_ctrl_id : w->root_ctrl_id);
		if (!(ctrl->flags & VuiCtrlFlags_focusable_no_keyboard_focus_nav)) {
			for (int i = 0; i < 2; i += 1) {
				//
				// find the previous control in the tree that is focusable.
				while (ctrl) {
					if (ctrl->sibling_prev_id == 0) {
						// we have no previous siblings, navigate up the parent
						ctrl = vui_ctrl_try_get(ctrl->parent_id);
						if (!ctrl) break;
					} else {
						// goto the previous sibling
						ctrl = vui_ctrl_get(ctrl->sibling_prev_id);
						// descend to the most last child of the sibling
						while (ctrl->child_last_id) ctrl = vui_ctrl_get(ctrl->child_last_id);
					}

					if (ctrl->flags & VuiCtrlFlags_focusable)
						break;
				}

				// found a control to focus on
				if (ctrl != NULL || i == 1) break;

				//
				// we have reached the root of the UI so start from the most last child
				// and try again
				ctrl = vui_ctrl_get(w->root_ctrl_id);
				while (ctrl->child_last_id) {
					ctrl = vui_ctrl_get(ctrl->child_last_id);
				}
			}

			vui_ctrl_set_focused(ctrl ? ctrl->id : 0);
		}
	} else if ((_vui.input.actions & VuiInputActions_focus_next) == VuiInputActions_focus_next) {
		_VuiWindow* w = &_vui.windows[_vui.focused_window_id];
		VuiCtrl* ctrl = vui_ctrl_get(w->focused_ctrl_id ? w->focused_ctrl_id : w->root_ctrl_id);
		if (!(ctrl->flags & VuiCtrlFlags_focusable_no_keyboard_focus_nav)) {
			for (int i = 0; i < 2; i += 1) {
				//
				// traverse the tree next until we come across another focusable control.
				while (ctrl) {
					if (ctrl->child_first_id) { // go to the first child if we have one.
						ctrl = vui_ctrl_get(ctrl->child_first_id);
					} else if (ctrl->sibling_next_id) { // go to the next sibling if we have no child
						ctrl = vui_ctrl_get(ctrl->sibling_next_id);
					} else { // else go up the parent controls until they have a next sibling.
						while (ctrl) {
							ctrl = vui_ctrl_try_get(ctrl->parent_id);
							if (ctrl && ctrl->sibling_next_id) {
								ctrl = vui_ctrl_get(ctrl->sibling_next_id);
								break;
							}
						}
					}

					if (!ctrl) break;
					if (ctrl->flags & VuiCtrlFlags_focusable)
						break;
				}

				// see if we found a control.
				// if not loop back around from the root and find the first focusable control.
				if (ctrl || i == 1) break;
				ctrl = vui_ctrl_get(w->root_ctrl_id);
			}

			vui_ctrl_set_focused(ctrl ? ctrl->id : 0);
		}
	}

	_vui.input.mouse.offset_x = 0;
	_vui.input.mouse.offset_y = 0;
	_vui.input.mouse.wheel_offset_x = 0;
	_vui.input.mouse.wheel_offset_y = 0;
	_vui.input.mouse.buttons_has_been_pressed = 0;
	_vui.input.mouse.buttons_has_been_released = 0;
	_vui.input.actions = 0;
	_vui.input.focused_text_box.has_changed = vui_false;
	_vui.input.focused_text_box.has_cursor_moved_last_frame = _vui.input.focused_text_box.has_cursor_moved;
	_vui.input.focused_text_box.has_cursor_moved = vui_false;
	_vui.build.frame_idx += 1;
}

void _vui_window_assert_id(VuiWindowId id) {
	vui_assert(id < _vui.windows_count, "window id of %u must be less than VuiSetup.windows_count of %u", id, _vui.windows_count);
}

void vui_window_start(VuiWindowId id, VuiVec2 size) {
	vui_assert(_vui.build.w == NULL, "cannot call vui_window_start until vui_window_end has been called");
	_VuiWindow* w = &_vui.windows[id];
	w->size = size;

	_vui.build.w = w;

	VuiCtrl* root_ctrl = NULL;
	if (w->root_ctrl_id) {
		root_ctrl = vui_ctrl_get(w->root_ctrl_id);
	} else {
		//
		// a root control does not exist, so allocate one.
		VuiCtrlId id = 0;
		root_ctrl = _vui_ctrl_alloc(&id);
		root_ctrl->id = id;
		root_ctrl->sib_id = 1;
		w->root_ctrl_id = id;
	}
	root_ctrl->last_frame_idx = _vui.build.frame_idx;

	root_ctrl->attributes = _vui.build.ctrl_attrs;
	root_ctrl->attributes.width = size.x;
	root_ctrl->attributes.height = size.y;

	_vui.build.ctrl_attrs.width = vui_auto_len;
	_vui.build.ctrl_attrs.height = vui_auto_len;

	_vui.build.parent_ctrl_id = root_ctrl->id;
	_vui.build.sibling_prev_ctrl_id = 0;
}

/*
VuiColumn // 500
	VuiColumn // ratio 0.4
		VuiButton // fill
			VuiImage // finite
			VuiText // auto

		VuiButton // fill
			VuiImage // finite
			VuiText // auto

	VuiText // auto -> 50

	VuiButton // fill 30 %
		VuiImage // finite
		VuiText // auto

	VuiColumn // fill 30 %
		VuiButton // fill
			VuiImage // finite
			VuiText // auto

		VuiButton // fill
			VuiImage // finite
			VuiText // auto
			*/

void _vui_layout_ctrls(VuiCtrl* ctrl, VuiRect* placement_area, float parent_inner_width, float parent_inner_height);

void _vui_layout_column_row(
	VuiCtrl* ctrl, VuiBool is_column, float inner_x, float inner_y, float inner_width, float inner_height,
	VuiVec2* max_inner_right_bottom_ptr, VuiRect* child_placement_area_ptr
) {
	if (ctrl->child_first_id == 0)
		return;

	uint16_t dir_size_offset = 0;
	float (*dir_rect_len)(const VuiRect*) = NULL;
	float (*wrap_dir_rect_len)(const VuiRect*) = NULL;
	float inner_dir_offset = 0.f;
	float inner_wrap_dir_offset = 0.f;
	float inner_dir_len = 0.f;
	float inner_wrap_dir_len = 0.f;
	float (*offset_dir_len)(VuiVec2) = NULL;
	float (*offset_wrap_dir_len)(VuiVec2) = NULL;
	float* fill_portion_dir_len_ptr = NULL;
	float* fill_portion_wrap_dir_len_ptr = NULL;
	float* max_dir_inner_len_ptr = NULL;
	float* max_wrap_dir_inner_len_ptr = NULL;
	float (*margin_dir_len)(const VuiThickness*) = NULL;
	if (is_column) {
		if (_vui.flags & _VuiFlags_right_to_left) {
			dir_rect_len = VuiRect_neg_width;
		} else {
			dir_rect_len = VuiRect_width;
		}
		inner_dir_offset = inner_x;
		inner_wrap_dir_offset = inner_y;
		inner_dir_len = inner_width;
		inner_wrap_dir_len = inner_height;
		dir_size_offset = offsetof(VuiCtrlAttrs, width);
		wrap_dir_rect_len = VuiRect_height;
		fill_portion_dir_len_ptr = &_vui.build.fill_portion_width;
		fill_portion_wrap_dir_len_ptr = &_vui.build.fill_portion_height;
		max_dir_inner_len_ptr = &max_inner_right_bottom_ptr->x;
		max_wrap_dir_inner_len_ptr = &max_inner_right_bottom_ptr->y;
		margin_dir_len = VuiThickness_horizontal;
		offset_dir_len = VuiVec2_x;
		offset_wrap_dir_len = VuiVec2_y;
	} else {
		if (_vui.flags & _VuiFlags_right_to_left) {
			wrap_dir_rect_len = VuiRect_neg_width;
		} else {
			wrap_dir_rect_len = VuiRect_width;
		}
		inner_dir_offset = inner_y;
		inner_wrap_dir_offset = inner_x;
		inner_dir_len = inner_height;
		inner_wrap_dir_len = inner_width;
		dir_size_offset = offsetof(VuiCtrlAttrs, height);
		dir_rect_len = VuiRect_height;
		fill_portion_dir_len_ptr = &_vui.build.fill_portion_height;
		fill_portion_wrap_dir_len_ptr = &_vui.build.fill_portion_width;
		max_dir_inner_len_ptr = &max_inner_right_bottom_ptr->y;
		max_wrap_dir_inner_len_ptr = &max_inner_right_bottom_ptr->x;
		margin_dir_len = VuiThickness_vertical;
		offset_dir_len = VuiVec2_y;
		offset_wrap_dir_len = VuiVec2_x;
	}

	//
	// only allow wrap if this layout does not have an automatic lenght in the direction of the layout.
	float wrap_spacing = 0.f;
	VuiBool wrap = vui_false;
	if (inner_dir_len != vui_auto_len) {
		wrap_spacing = ctrl->attributes.layout_wrap_spacing;
		wrap = ctrl->attributes.layout_wrap;
	}

	//
	// work out the fill_portion_dir_len.
	// this is only available for a non wrapping layouts with a fixed length.
	if (!wrap && inner_dir_len != vui_auto_len) {
		//
		// determine all the sizes of the automatic lengths in the direction of the layout
		float total_auto_dir_lens = 0.f;
		*child_placement_area_ptr = VuiRect_init_wh(inner_x, inner_y, 0, 0);
		VuiCtrl* child = NULL;
		for (VuiCtrlId child_id = ctrl->child_first_id; child_id; child_id = child->sibling_next_id) {
			child = vui_ctrl_get(child_id);
			if (*(float*)vui_ptr_add(&child->attributes, dir_size_offset) == vui_auto_len && !(child->flags & _VuiCtrlFlags_is_popover)) {
				_vui_layout_ctrls(child, child_placement_area_ptr, inner_dir_len, inner_wrap_dir_len);
				total_auto_dir_lens += dir_rect_len(&child->rect) + fabsf(offset_dir_len(child->attributes.offset));
			}
		}

		//
		// now go over the children and remove the ratios
		// from the available_dir_len and count up how many
		// controls want to fill the available space
		//
		float available_dir_len = inner_dir_len - total_auto_dir_lens;
		uint32_t fill_ctrls_count = 0;
		for (VuiCtrlId child_id = ctrl->child_first_id; child_id; child_id = child->sibling_next_id) {
			child = vui_ctrl_get(child_id);
			float child_dir_len = *(float*)vui_ptr_add(&child->attributes, dir_size_offset);
			if (child_dir_len == vui_auto_len) {
				continue;
			} else if (child_dir_len == vui_fill_len) {
				fill_ctrls_count += 1;
			} else if (child_dir_len < 0) { // is ratio
				// remove the ratio from the available_dir_len
				float ratio = -child_dir_len;
				available_dir_len -= inner_dir_len * ratio;
			} else {
				available_dir_len -= child_dir_len + margin_dir_len(&child->style.margin);
			}
		}

		//
		// we now have the portion length that children with a length (in the direction of the layout) of vui_fill_len can use
		//
		if (available_dir_len > 0.f && fill_ctrls_count) {
			*fill_portion_dir_len_ptr = available_dir_len / fill_ctrls_count;
		}
	}

	//
	// now begin laying out the children in the layout
	//
	float dir_start = inner_dir_offset;
	float wrap_dir_start = inner_wrap_dir_offset;
	VuiCtrl* child = vui_ctrl_get(ctrl->child_first_id);

	// layouts with wrap or an automatic length in the direction of the layout.
	// cannot get have children with vui_fill_len or ratio.
	// these variable stop the child controls from filling and using ratio.
	// they will be converted to vui_auto_len when _vui_layout_ctrls.
	float layout_inner_width = inner_width;
	if (wrap && is_column) {
		layout_inner_width = vui_auto_len;
	}
	float layout_inner_height = inner_height;
	if (wrap && !is_column) {
		layout_inner_height = vui_auto_len;
	}

	VuiRect rect_zero = {0};

	float layout_spacing = ctrl->attributes.layout_spacing;
	while (child) {
		VuiCtrl* child_line_start = child;
		float max_wrap_dir_len = 0.0;
		if (wrap || inner_wrap_dir_len == vui_auto_len) {
			//
			// because we are wrapping or our wrap directional length is automatic.
			// loop until we have reached the end of the line and find the tallest(for column)/widest(for row) control.
			float end_dir_coord = 0.f;
			VuiBool is_first = vui_true;
			*child_placement_area_ptr = VuiRect_init_wh(dir_start, wrap_dir_start, 0, 0);
			for (; child; child = child->sibling_next_id ? vui_ctrl_get(child->sibling_next_id) : NULL) {
				if (child->flags & _VuiCtrlFlags_is_popover) {
					continue;
				}
				_vui_layout_ctrls(child, child_placement_area_ptr, layout_inner_width, layout_inner_height);

				// advance the length along the direction of the layout
				end_dir_coord += dir_rect_len(&child->rect) + fabsf(offset_dir_len(child->attributes.offset));

				//
				// wrap the control back around if it exceeds the wrap length.
				if (wrap && !is_first && inner_dir_len < end_dir_coord) {
					break;
				}
				is_first = vui_false;

				end_dir_coord += layout_spacing;

				// see if the height for this control is the tallest
				float child_wrap_dir_len = wrap_dir_rect_len(&child->rect) + fabsf(offset_wrap_dir_len(child->attributes.offset));
				if (child_wrap_dir_len > max_wrap_dir_len) {
					max_wrap_dir_len = child_wrap_dir_len;
				}
			}
		} else {
			// because we are not wrapping and have a finite inner size.
			// we can just use the inner_wrap_dir_len  as the max_wrap_dir_len.
			max_wrap_dir_len = inner_wrap_dir_len;
			child = NULL;
		}

		*fill_portion_wrap_dir_len_ptr = max_wrap_dir_len;

		//
		// go back over controls in this line and lay them out properly this time.
		// setup the placement area for the child in a layout direction agnostic way.
		float line_inner_width = layout_inner_width;
		float line_inner_height = layout_inner_height;
		float* child_placement_area_dir_len_start = NULL;
		float* child_placement_area_dir_len_end = NULL;
		if (is_column) {
			if (_vui.flags & _VuiFlags_right_to_left) {
				child_placement_area_ptr->right = dir_start;
				child_placement_area_dir_len_start = &child_placement_area_ptr->right;
				child_placement_area_dir_len_end = &child_placement_area_ptr->left;
			} else {
				child_placement_area_ptr->left = dir_start;
				child_placement_area_dir_len_start = &child_placement_area_ptr->left;
				child_placement_area_dir_len_end = &child_placement_area_ptr->right;
			}
			line_inner_height = max_wrap_dir_len;
			child_placement_area_ptr->top = wrap_dir_start;
			child_placement_area_ptr->bottom = wrap_dir_start + max_wrap_dir_len;
		} else {
			if (_vui.flags & _VuiFlags_right_to_left) {
				child_placement_area_ptr->right = wrap_dir_start;
				child_placement_area_ptr->left = wrap_dir_start + max_wrap_dir_len;
			} else {
				child_placement_area_ptr->left = wrap_dir_start;
				child_placement_area_ptr->right = wrap_dir_start + max_wrap_dir_len;
			}
			line_inner_width = max_wrap_dir_len;
			child_placement_area_ptr->top = dir_start;
			child_placement_area_dir_len_start = &child_placement_area_ptr->top;
			child_placement_area_dir_len_end = &child_placement_area_ptr->bottom;
		}

		VuiCtrl* line_child;
		VuiCtrl* last_child;
		for (line_child = child_line_start; line_child != child; line_child = line_child->sibling_next_id ? vui_ctrl_get(line_child->sibling_next_id) : NULL) {
			VuiRect* cpa = child_placement_area_ptr;
			if (!(line_child->flags & _VuiCtrlFlags_is_popover)) {
				// so the control can just position itself within it.
				*child_placement_area_dir_len_end = *child_placement_area_dir_len_start + dir_rect_len(&line_child->rect) + fabsf(offset_dir_len(line_child->attributes.offset));
			} else {
				cpa = &rect_zero;
			}
			_vui_layout_ctrls(line_child, cpa, line_inner_width, line_inner_height);

			if (!(line_child->flags & _VuiCtrlFlags_is_popover)) {
				//
				// advance to the next cell, recalculate the end as it can be different this time.
				*child_placement_area_dir_len_start = *child_placement_area_dir_len_start + dir_rect_len(&line_child->rect) + fabsf(offset_dir_len(line_child->attributes.offset)) + layout_spacing;
			}
			last_child = line_child;
		}

		// advance to the new line
		wrap_dir_start += max_wrap_dir_len;
		if (wrap) {
			wrap_dir_start += wrap_spacing;
		}

		float end_coord = *child_placement_area_dir_len_start - layout_spacing;

		// track the max bottom right for auto sized layouts
		if (end_coord > *max_dir_inner_len_ptr) {
			*max_dir_inner_len_ptr = end_coord;
		}
		*max_wrap_dir_inner_len_ptr = wrap_dir_start;
	}

	// remove the trailing wrap spacing
	if (wrap) *max_wrap_dir_inner_len_ptr -= wrap_spacing;
}

void _vui_layout_ctrls(VuiCtrl* ctrl, VuiRect* placement_area, float parent_inner_width, float parent_inner_height) {
	const VuiCtrlStyle* style = &ctrl->style;

	ctrl->flags &= ~_VuiCtrlFlags_is_laid_out;

	float parent_fill_portion_width = _vui.build.fill_portion_width;
	float parent_fill_portion_height = _vui.build.fill_portion_height;
	_vui.build.fill_portion_width = 0.f;
	_vui.build.fill_portion_height = 0.f;

	//
	// while laying out the controls we use the rectangle as a relative
	// offset from it's parent. the rectangle also uses the outer size.
	// so it includes the margin, padding and inner size.
	//
	// we remove the margin from the control's rectangle when we _vui_layout_ctrls_finalize.
	// this saves us having to deal with margin for the child controls in each type of layout.
	//
	// be aware that when we use right to left, we are still using positive offsets from the right
	// edge of the screen. in _vui_layout_ctrls_finalize, this is taken away from the width of the screen.
	//

	if (_vui.flags & _VuiFlags_right_to_left) {
		ctrl->rect.right = placement_area->right;
	} else {
		ctrl->rect.left = placement_area->left;
	}
	ctrl->rect.top = placement_area->top;

	float inner_x = (_vui.flags & _VuiFlags_right_to_left)
		? style->margin.right + style->padding.right + style->border_width
		: style->margin.left + style->padding.left + style->border_width;
	float inner_y = style->margin.top + style->padding.top + style->border_width;
	float inner_width = vui_auto_len;
	float inner_height = vui_auto_len;

	{
		float width = ctrl->attributes.width;

		//
		// if (is ratio or fill) and parent is not calculated, then default to automatic sizing
		//
		if (parent_inner_width == vui_auto_len) { // parent width has not been calculated
			// if we have a width of ratio or fill then turn that into automatic
			if ((width == vui_fill_len || width < 0)) {
				width = vui_auto_len;
			}
		}

		if (width != vui_auto_len) {
			float outer_width;
			if (width == vui_fill_len) {
				if (parent_inner_width == vui_auto_len) {
					outer_width = vui_auto_len;
				} else {
					outer_width = parent_fill_portion_width;
				}
			} else if (width < 0) { // is ratio
				float ratio = -width;
				outer_width = parent_inner_width * ratio;
			} else {
				outer_width = width + (style->margin.left + style->margin.right);
			}

			if (_vui.flags & _VuiFlags_right_to_left) {
				ctrl->rect.left = ctrl->rect.right + outer_width;
			} else {
				ctrl->rect.right = ctrl->rect.left + outer_width;
			}
			inner_width = outer_width - (style->margin.left + style->margin.right) - (style->padding.left + style->padding.right) - style->border_width * 2;
		}
	}

	{
		float height = ctrl->attributes.height;
		//
		// if (is ratio or fill) and parent is not calculated, then default to automatic sizing
		//
		if (parent_inner_height == 0) { // parent height has not been calculated
			// if we have a height of ratio or fill then turn that into automatic
			if ((height == vui_fill_len || height < 0)) {
				height = vui_auto_len;
			}
		}

		if (height != vui_auto_len) {
			float outer_height;
			if (height == vui_fill_len) {
				if (parent_inner_height == vui_auto_len) {
					outer_height = vui_auto_len;
				} else {
					outer_height = parent_fill_portion_height;
				}
			} else if (height < 0) { // is ratio
				float ratio = -height;
				outer_height = parent_inner_height * ratio;
			} else {
				outer_height = height + (style->margin.top + style->margin.bottom);
			}

			ctrl->rect.bottom = ctrl->rect.top + outer_height;
			inner_height = outer_height - (style->margin.top + style->margin.bottom) - (style->padding.top + style->padding.bottom) - style->border_width * 2;
		}
	}

	VuiRect child_placement_area = {0};
	VuiVec2 max_inner_right_bottom = {0};
	switch (ctrl->layout_type) {
		case VuiLayoutType_column:
			_vui_layout_column_row(ctrl, vui_true, inner_x, inner_y, inner_width, inner_height, &max_inner_right_bottom, &child_placement_area);
			break;
		case VuiLayoutType_row:
			_vui_layout_column_row(ctrl, vui_false, inner_x, inner_y, inner_width, inner_height, &max_inner_right_bottom, &child_placement_area);
			break;
		case VuiLayoutType_stack: {
			//
			// if we have an automatic length, layout all the children and capture the maximum size.
			// use this to make a finite inner size that we can use to make the child_placement_area.
			if (inner_width == vui_auto_len || inner_height == vui_auto_len) {
				float max_width = 0.f;
				float max_height = 0.f;
				VuiCtrl* child = NULL;
				for (VuiCtrlId child_id = ctrl->child_first_id; child_id; child_id = child->sibling_next_id) {
					child = vui_ctrl_get(child_id);
					if (child->flags & _VuiCtrlFlags_is_popover)
						continue;
					child_placement_area = VuiRect_init(inner_x, inner_y, inner_x, inner_y);
					_vui_layout_ctrls(child, &child_placement_area, vui_auto_len, vui_auto_len);

					float width = (_vui.flags & _VuiFlags_right_to_left ? VuiRect_neg_width : VuiRect_width)(&child->rect);
					float height = VuiRect_height(&child->rect);

					width += fabs(child->attributes.offset.x);
					height += fabs(child->attributes.offset.y);

					if (width > max_width) max_width = width;
					if (height > max_height) max_height = height;
				}

				//
				// resolve the dimensions with automatic lengths
				//
				if (inner_width == vui_auto_len) {
					float outer_width = max_width + (style->padding.left + style->padding.right) + (style->border_width * 2) + (style->margin.left + style->margin.right);
					if (_vui.flags & _VuiFlags_right_to_left) {
						ctrl->rect.left = ctrl->rect.right + outer_width;
					} else {
						ctrl->rect.right = ctrl->rect.left + outer_width;
					}
					inner_width = max_width;
				}
				if (inner_height == vui_auto_len) {
					ctrl->rect.bottom = ctrl->rect.top + max_height + (style->padding.top + style->padding.bottom) + (style->border_width * 2) + (style->margin.top + style->margin.bottom);
					inner_height = max_height;
				}
			}

			if (inner_width != vui_auto_len) {
				_vui.build.fill_portion_width = inner_width;
			}

			if (inner_height != vui_auto_len) {
				_vui.build.fill_portion_height = inner_height;
			}

			VuiCtrl* child = NULL;
			for (VuiCtrlId child_id = ctrl->child_first_id; child_id; child_id = child->sibling_next_id) {
				child = vui_ctrl_get(child_id);
				if (child->flags & _VuiCtrlFlags_is_popover) {
					child_placement_area = VuiRect_zero;
				} else {
					if (_vui.flags & _VuiFlags_right_to_left) {
						child_placement_area = VuiRect_init(inner_x + inner_width, inner_y, inner_x, inner_y + inner_height);
					} else {
						child_placement_area = VuiRect_init(inner_x, inner_y, inner_x + inner_width, inner_y + inner_height);
					}
				}
				_vui_layout_ctrls(child, &child_placement_area, inner_width, inner_height);
			}
			break;
		};
	}

	if (ctrl->flags & (VuiCtrlFlags_scrollable_vertical | VuiCtrlFlags_scrollable_horizontal)) {
		VuiCtrl* content_ctrl = vui_ctrl_get(ctrl->scroll_content_id);
		if (ctrl->flags & VuiCtrlFlags_scrollable_horizontal) {
			content_ctrl->rect.left += ctrl->scroll_offset.x;
			content_ctrl->rect.right += ctrl->scroll_offset.x;
		}
		if (ctrl->flags & VuiCtrlFlags_scrollable_vertical) {
			content_ctrl->rect.top += ctrl->scroll_offset.y;
			content_ctrl->rect.bottom += ctrl->scroll_offset.y;
		}
	}

	//
	// resolve the dimensions with automatic lengths
	//
	if (inner_width == vui_auto_len) {
		if (_vui.flags & _VuiFlags_right_to_left) {
			ctrl->rect.left = ctrl->rect.right + max_inner_right_bottom.x + style->padding.left + style->border_width + style->margin.left;
		} else {
			ctrl->rect.right = ctrl->rect.left + max_inner_right_bottom.x + style->padding.right + style->border_width + style->margin.right;
		}
	}
	if (inner_height == vui_auto_len) {
		ctrl->rect.bottom = ctrl->rect.top + max_inner_right_bottom.y + style->padding.bottom + style->border_width + style->margin.bottom;
	}

	//
	// if the scroll view has been initialized with vui_fill_len or vui_auto_len.
	// then store the calculated size in ctrl->scroll_view_size.
	if (ctrl->flags & (VuiCtrlFlags_scrollable_horizontal | VuiCtrlFlags_scrollable_vertical)) {
		if (ctrl->attributes.width == vui_fill_len || ctrl->attributes.width == vui_auto_len) {
			ctrl->scroll_view_size.x = VuiRect_width(&ctrl->rect) - VuiThickness_horizontal(&style->margin);
			if (vui_ctrl_is_mouse_scroll_focused(ctrl->id) && _vui.build.mouse_scroll_focused_size)
				*_vui.build.mouse_scroll_focused_size = ctrl->scroll_view_size;
		}

		if (ctrl->attributes.height == vui_fill_len || ctrl->attributes.height == vui_auto_len) {
			ctrl->scroll_view_size.y = VuiRect_height(&ctrl->rect) - VuiThickness_vertical(&style->margin);
			if (vui_ctrl_is_mouse_scroll_focused(ctrl->id) && _vui.build.mouse_scroll_focused_size)
				*_vui.build.mouse_scroll_focused_size = ctrl->scroll_view_size;
		}
	}

	//
	// if we actually are being placed somewhere.
	// workout where in the placement_area we go.
	VuiVec2 placement_size = VuiRect_size(*placement_area);
	if (!(ctrl->flags & _VuiCtrlFlags_is_popover)) {
		VuiVec2 size = VuiRect_size(ctrl->rect);
		VuiVec2 offset = ctrl->attributes.offset;
		VuiAlign align = ctrl->attributes.align;

		switch (align) {
			case VuiAlign_left_top: break;
			case VuiAlign_center_top:
				offset.x += (placement_size.x / 2.0) - (size.x / 2.0);
				break;
			case VuiAlign_right_top:
				offset.x += placement_size.x - size.x;
				break;
			case VuiAlign_left_center:
				offset.y += (placement_size.y / 2.0) - (size.y / 2.0);
				break;
			case VuiAlign_center:
				offset.x += (placement_size.x / 2.0) - (size.x / 2.0);
				offset.y += (placement_size.y / 2.0) - (size.y / 2.0);
				break;
			case VuiAlign_right_center:
				offset.x += placement_size.x - size.x;
				offset.y += (placement_size.y / 2.0) - (size.y / 2.0);
				break;
			case VuiAlign_left_bottom:
				offset.y += placement_size.y - size.y;
				break;
			case VuiAlign_center_bottom:
				offset.x += (placement_size.x / 2.0) - (size.x / 2.0);
				offset.y += placement_size.y - size.y;
				break;
			case VuiAlign_right_bottom:
				offset.x += placement_size.x - size.x;
				offset.y += placement_size.y - size.y;
				break;
		}

		ctrl->rect.left += offset.x;
		ctrl->rect.right += offset.x;
		ctrl->rect.top += offset.y;
		ctrl->rect.bottom += offset.y;
	}

	//
	// restore the parent's fill_portion_width/height
	_vui.build.fill_portion_width = parent_fill_portion_width;
	_vui.build.fill_portion_height = parent_fill_portion_height;
}

void _vui_layout_ctrls_finalize(VuiCtrl* ctrl, VuiVec2 offset, float root_width, VuiBool is_popover_root) {
	if (!is_popover_root && ctrl->flags & _VuiCtrlFlags_is_popover) {
		VuiCtrlId* t = VuiStk_push(&_vui.build.popover_ctrl_ids);
		*t = ctrl->id;
		return;
	}

	ctrl->flags |= _VuiCtrlFlags_is_laid_out;

	if (_vui.flags & _VuiFlags_right_to_left) {
		ctrl->rect.left = root_width - offset.x - ctrl->rect.left;
		offset.x += ctrl->rect.right;
		ctrl->rect.right = root_width - offset.x;
	} else {
		ctrl->rect.right += offset.x;
		offset.x += ctrl->rect.left;
		ctrl->rect.left = offset.x;
	}

	ctrl->rect.bottom += offset.y;
	offset.y += ctrl->rect.top;
	ctrl->rect.top = offset.y;

	//
	// make the outer rectangle the actual rectangle of the control by applying the margin.
	VuiThickness* margin = &ctrl->style.margin;
	ctrl->rect.left += margin->left;
	ctrl->rect.top += margin->top;
	ctrl->rect.right -= margin->right;
	ctrl->rect.bottom -= margin->bottom;

	VuiCtrl* child = NULL;
	for (VuiCtrlId child_id = ctrl->child_first_id; child_id; child_id = child->sibling_next_id) {
		child = vui_ctrl_get(child_id);
		_vui_layout_ctrls_finalize(child, offset, root_width, vui_false);
	}
}

#if VUI_DEBUG_CTRL_LAYOUT

void vui_dump_ctrls_indent(FILE* f, uint16_t indent_level) {
	static char* tabs = "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t";
	fprintf(f, "%.*s", indent_level, tabs);
}

void vui_dump_ctrls_(FILE* f, VuiCtrl* ctrl, uint16_t indent_level) {
	vui_dump_ctrls_indent(f, indent_level);
	fprintf(f, "######## Ctrl %u ########\n", ctrl->id);

	vui_dump_ctrls_indent(f, indent_level);
	fprintf(f, "sib_id: %u\n", ctrl->sib_id);

	vui_dump_ctrls_indent(f, indent_level);
	fprintf(f, "parent_id: %u\n", ctrl->parent_id);

	vui_dump_ctrls_indent(f, indent_level);
	fprintf(f, "child_first_id: %u\n", ctrl->child_first_id);

	vui_dump_ctrls_indent(f, indent_level);
	fprintf(f, "child_last_id: %u\n", ctrl->child_last_id);

	vui_dump_ctrls_indent(f, indent_level);
	fprintf(f, "sibling_prev_id: %u\n", ctrl->sibling_prev_id);

	vui_dump_ctrls_indent(f, indent_level);
	fprintf(f, "sibling_next_id: %u\n", ctrl->sibling_next_id);

	vui_dump_ctrls_indent(f, indent_level);
	fprintf(f, "last_frame_idx: %u\n", ctrl->last_frame_idx);

	vui_dump_ctrls_indent(f, indent_level);
	fprintf(f, "rect: { left: %f, top: %f, right: %f, bottom: %f }\n", ctrl->rect.left, ctrl->rect.top, ctrl->rect.right, ctrl->rect.bottom);

	vui_dump_ctrls_indent(f, indent_level);
	fprintf(f, "state_flags: 0x%x\n", ctrl->state_flags);

	vui_dump_ctrls_indent(f, indent_level);
	fprintf(f, "layout_type: %s\n", VuiLayoutType_strings[ctrl->layout_type]);

	vui_dump_ctrls_indent(f, indent_level);
	fprintf(f, "flags: 0x%lx\n", ctrl->flags);

	vui_dump_ctrls_indent(f, indent_level);
	fprintf(f, "scroll_offset: %f, %f\n", ctrl->scroll_offset.x, ctrl->scroll_offset.y);

	VuiCtrl* child = NULL;
	for (VuiCtrlId child_id = ctrl->child_first_id; child_id; child_id = child->sibling_next_id) {
		child = vui_ctrl_get(child_id);
		vui_dump_ctrls_(f, child, indent_level + 1);
	}
}

void vui_dump_ctrls(VuiCtrl* root_ctrl) {
	FILE* file = fopen(vui_debug_ctrl_layout_dump_file_path, "w");
	vui_dump_ctrls_(file, root_ctrl, 0);
	fflush(file);
	fclose(file);
}

#endif // VUI_DEBUG_CTRL_LAYOUT

void _vui_ctrl_dealloc_children(VuiCtrl* ctrl) {
	VuiCtrl* child = NULL;
	for (VuiCtrlId child_id = ctrl->child_first_id; child_id; ) {
		child = vui_ctrl_get(child_id);
		_vui_ctrl_dealloc_children(child);
		child_id = child->sibling_next_id;
		_vui_ctrl_dealloc(child->id);
	}
}

void _vui_remove_old_ctrls(VuiCtrl* ctrl) {
	VuiCtrl* child = NULL;
	for (VuiCtrlId child_id = ctrl->child_first_id; child_id;) {
		child = vui_ctrl_get(child_id);
 		child_id = child->sibling_next_id;
		if (child->last_frame_idx != _vui.build.frame_idx) {
			child->rect = VuiRect_zero;
			_vui_ctrl_dealloc_children(child);
			_vui_ctrl_unlink(child);
			_vui_ctrl_dealloc(child->id);
		} else {
			_vui_remove_old_ctrls(child);
		}
	}
}

void vui_window_end() {
	vui_assert(_vui.build.w != NULL, "cannot call vui_window_end until vui_window_start has been called");

	VuiCtrl* root = vui_ctrl_get(_vui.build.parent_ctrl_id);
	vui_assert(root->parent_id == 0, "cannot end the window without ending all of it's child controls");
	vui_ctrl_end();

	_vui_remove_old_ctrls(root);

	float width = root->attributes.width;
	float height = root->attributes.height;
	VuiRect placement_area = VuiRect_init_wh(0.f, 0.f, 0.f, 0.f);

	_vui_layout_ctrls(root, &placement_area, width, height);
	if (_vui.flags & _VuiFlags_right_to_left) {
		float tmp = root->rect.left;
		root->rect.left = root->rect.right;
		root->rect.right = tmp;
	}

	VuiStk_clear(_vui.build.popover_ctrl_ids);
	VuiCtrl* child = NULL;
	for (VuiCtrlId child_id = root->child_first_id; child_id; child_id = child->sibling_next_id) {
		child = vui_ctrl_get(child_id);
		_vui_layout_ctrls_finalize(child, (VuiVec2){0}, root->attributes.width, vui_false);
	}

	for (int idx = 0; idx < VuiStk_count(_vui.build.popover_ctrl_ids); idx += 1) {
		VuiCtrlId ctrl_id;
		VuiCtrl* popover_ctrl;
		VuiCtrl* target_ctrl;
		while (1) {
			ctrl_id = _vui.build.popover_ctrl_ids[idx];
			popover_ctrl = vui_ctrl_get(ctrl_id);
			if (!(popover_ctrl->flags & _VuiCtrlFlags_is_popover_open)) {
				goto CONTINUE;
			}

			target_ctrl = vui_ctrl_get(popover_ctrl->popover_target_ctrl_id);
			if (target_ctrl->flags & _VuiCtrlFlags_is_laid_out)
				break;

			//
			// this control is not ready to be laid out, so put it to the back.
			VuiStk_remove_shift(_vui.build.popover_ctrl_ids, idx);
			VuiCtrlId* end_of_stack = VuiStk_push(&_vui.build.popover_ctrl_ids);
			*end_of_stack = ctrl_id;
		}

		VuiVec2 target_ctrl_size = VuiRect_size(target_ctrl->rect);
		VuiVec2 size = VuiRect_size(popover_ctrl->rect);
		VuiVec2 offset = VuiVec2_add(popover_ctrl->attributes.offset, target_ctrl->rect.left_top);
		VuiAlign align = popover_ctrl->attributes.align;

		switch (align) {
			case VuiAlign_left_top:
				offset.x -= size.x;
				offset.y -= size.y;
				break;
			case VuiAlign_center_top:
				offset.x += (target_ctrl_size.x / 2.0) - (size.x / 2.0);
				offset.y -= size.y;
				break;
			case VuiAlign_right_top:
				offset.x += target_ctrl_size.x - size.x;
				offset.x += size.x;
				offset.y -= size.y;
				break;
			case VuiAlign_left_center:
				offset.y += (target_ctrl_size.y / 2.0) - (size.y / 2.0);
				offset.x -= size.x;
				break;
			case VuiAlign_center:
				offset.x += (target_ctrl_size.x / 2.0) - (size.x / 2.0);
				offset.y += (target_ctrl_size.y / 2.0) - (size.y / 2.0);
				break;
			case VuiAlign_right_center:
				offset.x += target_ctrl_size.x - size.x;
				offset.y += (target_ctrl_size.y / 2.0) - (size.y / 2.0);
				offset.x += size.x;
				break;
			case VuiAlign_left_bottom:
				offset.y += target_ctrl_size.y - size.y;
				offset.x -= size.x;
				offset.y += size.y;
				break;
			case VuiAlign_center_bottom:
				offset.x += (target_ctrl_size.x / 2.0) - (size.x / 2.0);
				offset.y += target_ctrl_size.y - size.y;
				offset.y += size.y;
				break;
			case VuiAlign_right_bottom:
				offset.x += target_ctrl_size.x - size.x;
				offset.y += target_ctrl_size.y - size.y;
				offset.x += size.x;
				offset.y += size.y;
				break;
		}

		//
		// make sure the popover doesn't go outside the boundary of the window
		VuiVec2 start = VuiVec2_add(popover_ctrl->rect.left_top, offset);
		if (start.x < 0) offset.x -= start.x;
		if (start.y < 0) offset.y -= start.y;
		VuiVec2 end = VuiVec2_add(popover_ctrl->rect.right_bottom, offset);
		if (end.x > root->rect.ex) offset.x -= end.x - root->rect.ex;
		if (end.y > root->rect.ey) offset.y -= end.y - root->rect.ey;

		_vui_layout_ctrls_finalize(popover_ctrl, offset, root->attributes.width, vui_true);
CONTINUE: {}
	}

#if VUI_DEBUG_CTRL_LAYOUT
	vui_dump_ctrls(root);
#endif

	_vui.build.w = NULL;
}

void _vui_render_ctrls(VuiCtrl* ctrl) {
	VuiCtrlFlags flags = ctrl->flags;

	VuiRect parent_clip_rect = _vui.render.clip_rect;
	VuiRect inner_rect = ctrl->rect;
	if (ctrl->flags & _VuiCtrlFlags_is_popover) {
		if (!(ctrl->flags & _VuiCtrlFlags_is_popover_open)) {
			return;
		}
		vui_render_inc_layer();
		VuiCtrl* root_ctrl = vui_ctrl_get(_vui.render.w->root_ctrl_id);
		_vui.render.clip_rect = VuiRect_init(0, 0, root_ctrl->attributes.width, root_ctrl->attributes.height);
	} else {
		_vui.render.clip_rect = VuiRect_clip(&_vui.render.clip_rect, &inner_rect);
	}

	const VuiCtrlStyle* style = &ctrl->style;

	float border_width_half = style->border_width / 2.0;
	if (style->bg_color.a) {
		if (style->border_width) {
			inner_rect.left += border_width_half;
			inner_rect.top += border_width_half;
			inner_rect.bottom -= border_width_half;
			inner_rect.right -= border_width_half;
		}
		vui_render_rect(&inner_rect, style->bg_color, style->radius);
		if (style->border_width) {
			inner_rect.left -= border_width_half;
			inner_rect.top -= border_width_half;
			inner_rect.bottom += border_width_half;
			inner_rect.right += border_width_half;
		}
	}

	if (style->border_width) {
		vui_render_rect_border(&ctrl->rect, style->border_color, style->radius, style->border_width);

		inner_rect.left += style->border_width;
		inner_rect.top += style->border_width;
		inner_rect.bottom -= style->border_width;
		inner_rect.right -= style->border_width;
		_vui.render.clip_rect = VuiRect_clip(&_vui.render.clip_rect, &inner_rect);
	}

	inner_rect.left += style->padding.left;
	inner_rect.top += style->padding.top;
	inner_rect.bottom -= style->padding.bottom;
	inner_rect.right -= style->padding.right;
	_vui.render.clip_rect = VuiRect_clip(&_vui.render.clip_rect, &inner_rect);

	float interp_ratio = 1.0;
	if (ctrl->state_time < ctrl->attributes.style_transition_time) {
		interp_ratio = ctrl->state_time / ctrl->attributes.style_transition_time;
	}

	if (ctrl->render_fn) {
		ctrl->render_fn(ctrl, &inner_rect, interp_ratio);
	}

	if (ctrl->styles && ctrl->styles[0].pre_animate_fn) {
		ctrl->styles[0].pre_animate_fn(ctrl, _vui.build.dt, interp_ratio, ctrl->state_time == _vui.build.dt);
	}

	//
	// now render the children;
	//
	VuiCtrl* child = NULL;
	for (VuiCtrlId child_id = ctrl->child_first_id; child_id; child_id = child->sibling_next_id) {
		child = vui_ctrl_get(child_id);
		_vui_render_ctrls(child);
	}

	if (ctrl->styles && ctrl->styles[0].post_animate_fn) {
		ctrl->styles[0].post_animate_fn(ctrl, _vui.build.dt, interp_ratio, ctrl->state_time == _vui.build.dt);
	}

	_vui.render.clip_rect = parent_clip_rect;
	if (ctrl->flags & _VuiCtrlFlags_is_popover) {
		vui_render_dec_layer();
	}
}

VuiWindowRender* vui_window_render(VuiWindowId id, float scale_factor, VuiBool pixel_snapping) {
	_vui_window_assert_id(id);

	if (pixel_snapping)
		_vui.flags |= _VuiFlags_pixel_snapping;

	_VuiWindow* w = &_vui.windows[id];
	_vui.render.clip_rect = VuiRect_init_v2(VuiVec2_zero, w->size);

	VuiStk(VuiRenderLayer) layers = w->render_layers;
	uint32_t layers_count = VuiStk_count(layers);
	for (int idx = 0; idx < layers_count; idx += 1) {
		VuiRenderLayer* layer = &layers[idx];
		VuiStk_clear(layer->indices);
		VuiStk_clear(layer->verts);
		VuiStk_clear(layer->cmds);
	}

	if (w->root_ctrl_id == 0) return NULL;

	_vui.render.layer_idx = -1;
	_vui.render.w = w;
	vui_render_inc_layer();

	_vui_render_ctrls(vui_ctrl_get(w->root_ctrl_id));

	VuiStk_clear(w->render.indices);
	VuiStk_clear(w->render.verts);
	VuiStk_clear(w->render.cmds);

	uint64_t hash = vui_fnv_hash_64_initial;
	layers = w->render_layers;
	layers_count = VuiStk_count(layers);
	for (int idx = 0; idx < layers_count; idx += 1) {
		VuiRenderLayer* layer = &layers[idx];
		if (scale_factor != 1.0) {
			VuiVertex* verts = layer->verts;
			for (int v_idx = 0; v_idx < VuiStk_count(layer->verts); v_idx += 1) {
				VuiVertex_scale_pos(verts[v_idx], scale_factor);
			}
		}
		hash = vui_fnv_hash_64((char*)layer->cmds, VuiStk_count(layer->cmds) * sizeof(VuiRenderCmd), hash);
		hash = vui_fnv_hash_64((char*)layer->verts, VuiStk_count(layer->verts) * sizeof(VuiVertex), hash);
		hash = vui_fnv_hash_64((char*)layer->indices, VuiStk_count(layer->indices) * sizeof(VuiVertexIdx), hash);
		uint32_t old_vertices_count = VuiStk_count(w->render.verts);
		uint32_t old_indices_count = VuiStk_count(w->render.indices);

		//
		// flattern all these layers into one big array.
		VuiRenderCmd* cmds = VuiStk_push_many(&w->render.cmds, VuiStk_count(layer->cmds));
		vui_ensure_alloc_ok(cmds, NULL);
		memcpy(cmds, layer->cmds, VuiStk_count(layer->cmds) * sizeof(VuiRenderCmd));
		if (old_vertices_count) {
			for (uint32_t i = 0; i < VuiStk_count(layer->cmds); i += 1) {
				cmds[i].verts_start_idx += old_vertices_count;
				cmds[i].indices_start_idx += old_indices_count;
			}
		}

		VuiVertex* verts = VuiStk_push_many(&w->render.verts, VuiStk_count(layer->verts));
		vui_ensure_alloc_ok(verts, NULL);
		memcpy(verts, layer->verts, VuiStk_count(layer->verts) * sizeof(VuiVertex));

		VuiVertexIdx* indices = VuiStk_push_many(&w->render.indices, VuiStk_count(layer->indices));
		vui_ensure_alloc_ok(indices, NULL);
		memcpy(indices, layer->indices, VuiStk_count(layer->indices) * sizeof(VuiVertexIdx));
		if (old_vertices_count) {
			for (uint32_t i = 0; i < VuiStk_count(layer->indices); i += 1) {
				indices[i] += old_vertices_count;
			}
		}
	}
	w->render.hash = hash;
	_vui.render.w = NULL;

	return &w->render;
}

void vui_window_set_mouse_focused(VuiWindowId id) {
	_vui_window_assert_id(id);
	_vui.mouse_focused_window_id = id;
}

void vui_window_set_focused(VuiWindowId id) {
	_vui_window_assert_id(id);
	_vui.focused_window_id = id;
}

void vui_window_dump_render(VuiWindowId id, FILE* file) {
	_vui_window_assert_id(id);
	_VuiWindow* w = &_vui.windows[id];
	VuiRenderLayer* layers = w->render_layers;
	for (int idx = 0; idx < VuiStk_count(w->render_layers); idx += 1) {
		VuiRenderLayer* layer = &layers[idx];

		fprintf(file, "LAYER %u\n", idx);
		fprintf(file, "cmds.count: %u\n", VuiStk_count(layer->cmds));
		fprintf(file, "verts.count: %u\n", VuiStk_count(layer->verts));
		fprintf(file, "indices.count: %u\n", VuiStk_count(layer->indices));

		fprintf(file, "cmds: {\n");
		VuiRenderCmd* cmds = layer->cmds;
		for (int cmd_idx = 0; cmd_idx < VuiStk_count(layer->cmds); cmd_idx += 1) {
			VuiRenderCmd* cmd = &cmds[cmd_idx];
			fprintf(file, "\t%u: {\n", cmd_idx);
			fprintf(file, "\t\ttexture_id: %u\n", cmd->texture_id);
			fprintf(file, "\t\tverts_start_idx: %u\n", cmd->verts_start_idx);
			fprintf(file, "\t\tindices_start_idx: %u\n", cmd->indices_start_idx);
			fprintf(file, "\t\tindices_count: %u\n", cmd->indices_count);
			fprintf(file, "\t}\n");
		}
		fprintf(file, "}\n");

		fprintf(file, "verts: {\n");
		VuiVertex* verts = layer->verts;
		for (int vert_idx = 0; vert_idx < VuiStk_count(layer->verts); vert_idx += 1) {
			VuiVertex* vert = &verts[vert_idx];
			VuiVertex_debug_fprintf(vert, file);
		}
		fprintf(file, "}\n");

		fprintf(file, "indices: {\n");
		VuiVertexIdx* indices = layer->indices;
		for (int indice_idx = 0; indice_idx < VuiStk_count(layer->indices); indice_idx += 1) {
			VuiVertexIdx indice = indices[indice_idx];
			fprintf(file, "\t%u: %u\n", indice_idx, indice);
		}
		fprintf(file, "}\n");
	}
}

void* vui_frame_data_alloc(uint32_t size, uint32_t align) {
	return _VuiArenaAlctor_alloc(&_vui.frame_data_alctor, size, align);
}

