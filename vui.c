
// ===========================================================================================
//
//
// memory allocation - element pool
//
//
// ===========================================================================================

typedef uint32_t VuiPoolId;

typedef struct _VuiPool _VuiPool;
struct _VuiPool {
	/*
	uint8_t is_allocated_bitset[(cap / 8) + 1]
	T elements[cap]
	*/
	void* data;
	uint32_t elmts_start_byte_idx;
	uint32_t count;
	uint32_t cap;
	uint32_t free_list_head_id;
};

#define VuiPool(T) VuiPool_##T

#define typedef_VuiPool(T) \
typedef struct { \
	T* VuiPool_data; \
	uint32_t elmts_start_byte_idx; \
	uint32_t count; \
	uint32_t cap; \
	uint32_t free_list_head_id; \
} VuiPool_##T;

// ===========================================================================================
//
//
// Internal
//
//
// ===========================================================================================

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
			// signed offset from the cursor_idx.
			// cursor_idx to cursor_idx + select_offset
			// will be the selection range.
			int32_t select_offset;
			_VuiInputBoxType type;
			VuiBool has_changed;
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
		VuiCtrlId parent_ctrl_id;
		VuiCtrlId sibling_prev_ctrl_id;
		float fill_portion_width;
		float fill_portion_height;
		VuiCtrlAttrChange* ctrl_attr_change_list_heads[VuiCtrlAttr_COUNT];
		VuiCtrlAttrs ctrl_attrs;
		VuiVec2* mouse_scroll_focused_content_offset;
		VuiVec2* mouse_scroll_focused_size;
	} build;

	//
	// this is the temporary style that is used interpolate between the current style and the target style.
	// HACK: a style could be any size as it is generally unique for every type of control.
	// so we just make sure the _vui_style_buf is definately big enough.
	union { VuiCtrlStyle style; char style_buf[256]; };

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
};

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

void vui_input_add_text(const char* string, uint32_t string_length) {
	// ignore if a text box is not focused
	if (_vui.input.focused_text_box.string == NULL) return;

	_vui.input.focused_text_box.has_changed = vui_true;
	if (_vui.input.focused_text_box.select_offset != 0) { // if we are selecting
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
	}

	for (uint32_t i = 0; i < string_length; i += 1) {
		// + 1 to give space for the null terminator.
		if (_vui.input.focused_text_box.string_len + 1 >= _vui.input.focused_text_box.string_cap)
			break;

		char ch = string[i];
		VuiBool copy = vui_false;
		switch (_vui.input.focused_text_box.type) {
			case _VuiInputBoxType_text: copy = vui_true; break;
			case _VuiInputBoxType_float:
				if (ch == '.') {
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
				if (ch == '-') {
					copy = _vui.input.focused_text_box.string_len == 0 ||
						(_vui.input.focused_text_box.cursor_idx == 0 && _vui.input.focused_text_box.string[0] != '-');
					if (copy) break;
				}
				// fallthrough
			case _VuiInputBoxType_u32:
				copy = ch >= '0' && ch <= '9';
				break;
		}

		//
		// copy the byte if it is valid for this type of input box
		if (copy) {
			char* dst_string = _vui.input.focused_text_box.string;
			uint32_t dst_idx = _vui.input.focused_text_box.cursor_idx;

			//
			// shift the characters to the right of the index over by one.
			// TODO: maybe validate the string upfront and do this shift all in one go.
			//       it is unluckly that much text will come through here in one go though.
			if (dst_idx < _vui.input.focused_text_box.string_len) {
				void* src = dst_string + dst_idx;
				memmove(src + 1, src, _vui.input.focused_text_box.string_len - dst_idx);
			}

			//
			// insert the character
			_vui.input.focused_text_box.string[_vui.input.focused_text_box.cursor_idx] = ch;
			_vui.input.focused_text_box.cursor_idx += 1;
			_vui.input.focused_text_box.string_len += 1;
		}
	}

	_vui.input.focused_text_box.string[_vui.input.focused_text_box.string_len] = '\0';
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

VuiFocusState _vui_ctrl_focus_state(VuiCtrlId ctrl_id) {
	//
	// keyboard state
	//

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

VuiCtrlState VuiCtrl_state(VuiCtrl* ctrl) {
	VuiCtrlStateFlags state_flags = ctrl->state_flags;
	//
	// return the highest set bit.
	// x86 & arm have instructions to count leading zeros
	// this can be used to find the highest set bit without a loop.
	// not sure if this is worth the extra ifdef complication in the code.
	VuiCtrlState state = 0;
	while (state_flags >>= 1) {
		state += 1;
	}
	return state;
}

void VuiCtrl_target_style(VuiCtrl* ctrl, const VuiCtrlStyle* target_style) {
	if (ctrl->style == target_style || ctrl->target_style == target_style)
		return;

	if (ctrl->target_style == NULL)
		ctrl->interp_ratio = 0.f;

	if (ctrl->style == NULL) {
		ctrl->style = target_style;
		ctrl->target_style = NULL;
	} else {
		// TODO: replace this block with this line when we work on interpolating styles
		// ctrl->target_style = target_style;
		ctrl->style = target_style;
		ctrl->target_style = NULL;
	}
}

VuiThickness VuiCtrl_interp_margin(VuiCtrl* ctrl) {
	VuiThickness margin = {0};
	if (ctrl->style) {
		if (ctrl->target_style) {
			VuiThickness_lerp(&margin, &ctrl->target_style->margin, &ctrl->style->margin, ctrl->interp_ratio);
		} else {
			margin = ctrl->style->margin;
		}
	}
	return margin;
}

VuiThickness VuiCtrl_interp_padding(VuiCtrl* ctrl) {
	VuiThickness padding = {0};
	if (ctrl->style) {
		if (ctrl->target_style) {
			VuiThickness_lerp(&padding, &ctrl->target_style->padding, &ctrl->style->padding, ctrl->interp_ratio);
		} else {
			padding = ctrl->style->padding;
		}
	}
	return padding;
}

void VuiCtrlStyle_interp(VuiCtrlStyle* result, const VuiCtrlStyle* to, const VuiCtrlStyle* from, float interp_ratio) {
	VuiThickness_lerp(&result->margin, &to->margin, &from->margin, interp_ratio);
	VuiThickness_lerp(&result->padding, &to->padding, &from->padding, interp_ratio);
	result->bg_color = VuiColor_lerp(to->bg_color, from->bg_color, interp_ratio);
	result->border_color = VuiColor_lerp(to->border_color, from->border_color, interp_ratio);
	result->border_width = vui_lerp(to->border_width, from->border_width, interp_ratio);
	result->radius = vui_lerp(to->radius, from->radius, interp_ratio);
}

void VuiTextStyle_interp(VuiCtrlStyle* result, const VuiCtrlStyle* to, const VuiCtrlStyle* from, float interp_ratio) {
	VuiCtrlStyle_interp(result, to, from, interp_ratio);
	VuiTextStyle* result_ = (VuiTextStyle*)result;
	VuiTextStyle* to_ = (VuiTextStyle*)to;
	VuiTextStyle* from_ = (VuiTextStyle*)from;
	result_->line_height = vui_lerp(to_->line_height, from_->line_height, interp_ratio);
	result_->color = VuiColor_lerp(to_->color, from_->color, interp_ratio);
	result_->font_id = to_->font_id;
}

void VuiSeparatorStyle_interp(VuiCtrlStyle* result, const VuiCtrlStyle* to, const VuiCtrlStyle* from, float interp_ratio) {
	VuiCtrlStyle_interp(result, to, from, interp_ratio);
	VuiSeparatorStyle* result_ = (VuiSeparatorStyle*)result;
	VuiSeparatorStyle* to_ = (VuiSeparatorStyle*)to;
	VuiSeparatorStyle* from_ = (VuiSeparatorStyle*)from;
	result_->size = vui_lerp(to_->size, from_->size, interp_ratio);
}

void VuiButtonStyle_interp(VuiCtrlStyle* result, const VuiCtrlStyle* to, const VuiCtrlStyle* from, float interp_ratio) {
	VuiCtrlStyle_interp(result, to, from, interp_ratio);
}

void VuiCheckBoxStyle_interp(VuiCtrlStyle* result, const VuiCtrlStyle* to, const VuiCtrlStyle* from, float interp_ratio) {
	VuiCtrlStyle_interp(result, to, from, interp_ratio);
	VuiCheckBoxStyle* result_ = (VuiCheckBoxStyle*)result;
	VuiCheckBoxStyle* to_ = (VuiCheckBoxStyle*)to;
	VuiCheckBoxStyle* from_ = (VuiCheckBoxStyle*)from;
	result_->check_color = VuiColor_lerp(to_->check_color, from_->check_color, interp_ratio);
	result_->check_size = vui_lerp(to_->check_size, from_->check_size, interp_ratio);
}

void VuiRadioButtonStyle_interp(VuiCtrlStyle* result, const VuiCtrlStyle* to, const VuiCtrlStyle* from, float interp_ratio) {
	VuiCtrlStyle_interp(result, to, from, interp_ratio);
}

void VuiSliderStyle_interp(VuiCtrlStyle* result, const VuiCtrlStyle* to, const VuiCtrlStyle* from, float interp_ratio) {
	VuiCtrlStyle_interp(result, to, from, interp_ratio);
	VuiSliderStyle* result_ = (VuiSliderStyle*)result;
	VuiSliderStyle* to_ = (VuiSliderStyle*)to;
	VuiSliderStyle* from_ = (VuiSliderStyle*)from;
	result_->button_width = vui_lerp(to_->button_width, from_->button_width, interp_ratio);
	result_->bar_height = vui_lerp(to_->bar_height, from_->button_width, interp_ratio);
}

void VuiProgressBarStyle_interp(VuiCtrlStyle* result, const VuiCtrlStyle* to, const VuiCtrlStyle* from, float interp_ratio) {
	VuiCtrlStyle_interp(result, to, from, interp_ratio);
	VuiProgressBarStyle* result_ = (VuiProgressBarStyle*)result;
	VuiProgressBarStyle* to_ = (VuiProgressBarStyle*)to;
	VuiProgressBarStyle* from_ = (VuiProgressBarStyle*)from;
	result_->bar_color = VuiColor_lerp(to_->bar_color, from_->bar_color, interp_ratio);
}

void VuiScrollBarStyle_interp(VuiCtrlStyle* result, const VuiCtrlStyle* to, const VuiCtrlStyle* from, float interp_ratio) {
	VuiCtrlStyle_interp(result, to, from, interp_ratio);
}

void VuiScrollViewStyle_interp(VuiCtrlStyle* result, const VuiCtrlStyle* to, const VuiCtrlStyle* from, float interp_ratio) {
	VuiCtrlStyle_interp(result, to, from, interp_ratio);
}

void VuiTextBoxStyle_interp(VuiCtrlStyle* result, const VuiCtrlStyle* to, const VuiCtrlStyle* from, float interp_ratio) {
	VuiCtrlStyle_interp(result, to, from, interp_ratio);
}

VuiStyleSheet vui_ss = {
	.image = {
		.margin = vui_margin_default,
		.padding = vui_padding_default,
	},
	.box_panel = {
		.margin = vui_margin_default,
		.padding = vui_padding_default,
		.bg_color = vui_color_midnight_blue,
		.border_color = vui_color_wet_asphalt,
		.border_width = vui_border_width_default,
		.radius = vui_radius_default,
	},
	.text_header = {
		.margin = VuiThickness_init(4.f, 4.f, 4.f, 0.f),
		.padding = vui_padding_default,
		.color = vui_color_white,
		.line_height = vui_line_height_header,
		.font_id = 0,
	},
	.text_menu = {
		.margin = vui_margin_default,
		.padding = vui_padding_default,
		.color = vui_color_white,
		.line_height = vui_line_height_menu,
		.font_id = 0,
	},
	.separator = {
		.margin = vui_margin_default,
		.padding = vui_padding_default,
		.bg_color = vui_color_gray,
		.radius = vui_radius_default,
		.size = vui_separator_size_default,
	},
	.button_action = {
		[VuiCtrlState_default] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
			.bg_color = vui_color_midnight_blue,
			.border_color = vui_color_asbestos,
			.border_width = vui_border_width_default,
			.radius = vui_radius_default,
			.text_style = &vui_ss.text_menu,
			.image_style = &vui_ss.image,
		},
		[VuiCtrlState_focused] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
			.bg_color = vui_color_wet_asphalt,
			.border_color = vui_color_concrete,
			.border_width = vui_border_width_default,
			.radius = vui_radius_default,
			.text_style = &vui_ss.text_menu,
			.image_style = &vui_ss.image,
		},
		[VuiCtrlState_active] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
			.bg_color = vui_color_concrete,
			.border_color = vui_color_wet_asphalt,
			.border_width = vui_border_width_default,
			.radius = vui_radius_default,
			.text_style = &vui_ss.text_menu,
			.image_style = &vui_ss.image,
		},
		[VuiCtrlState_disabled] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
			.bg_color = vui_color_wet_asphalt,
			.border_color = vui_color_midnight_blue,
			.border_width = vui_border_width_default,
			.radius = vui_radius_default,
			.text_style = &vui_ss.text_menu,
			.image_style = &vui_ss.image,
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
			.text_style = &vui_ss.text_menu,
			.image_style = &vui_ss.image,
			.check_color = vui_color_amethyst,
			.check_size = vui_check_size_default,
		},
		[VuiCtrlState_focused] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
			.bg_color = vui_color_wet_asphalt,
			.border_color = vui_color_concrete,
			.border_width = vui_border_width_default,
			.radius = vui_radius_default,
			.text_style = &vui_ss.text_menu,
			.image_style = &vui_ss.image,
			.check_color = vui_color_amethyst,
			.check_size = vui_check_size_default,
		},
		[VuiCtrlState_active] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
			.bg_color = vui_color_wet_asphalt,
			.border_color = vui_color_concrete,
			.border_width = vui_border_width_default,
			.radius = vui_radius_default,
			.text_style = &vui_ss.text_menu,
			.image_style = &vui_ss.image,
			.check_color = vui_color_amethyst,
			.check_size = vui_check_size_default,
		},
		[VuiCtrlState_disabled] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
			.bg_color = vui_color_wet_asphalt,
			.border_color = vui_color_midnight_blue,
			.border_width = vui_border_width_default,
			.radius = vui_radius_default,
			.text_style = &vui_ss.text_menu,
			.image_style = &vui_ss.image,
			.check_color = vui_color_amethyst,
			.check_size = vui_check_size_default,
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
			.text_style = &vui_ss.text_menu,
			.image_style = &vui_ss.image,
			.check_color = vui_color_amethyst,
			.check_size = vui_check_size_default,
		},
		[VuiCtrlState_focused] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
			.bg_color = vui_color_wet_asphalt,
			.border_color = vui_color_concrete,
			.border_width = vui_border_width_default,
			.radius = 100.f,
			.text_style = &vui_ss.text_menu,
			.image_style = &vui_ss.image,
			.check_color = vui_color_amethyst,
			.check_size = vui_check_size_default,
		},
		[VuiCtrlState_active] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
			.bg_color = vui_color_wet_asphalt,
			.border_color = vui_color_concrete,
			.border_width = vui_border_width_default,
			.radius = 100.f,
			.text_style = &vui_ss.text_menu,
			.image_style = &vui_ss.image,
			.check_color = vui_color_amethyst,
			.check_size = vui_check_size_default,
		},
		[VuiCtrlState_disabled] = {
			.margin = vui_margin_default,
			.padding = vui_padding_default,
			.bg_color = vui_color_wet_asphalt,
			.border_color = vui_color_midnight_blue,
			.border_width = vui_border_width_default,
			.radius = 100.f,
			.text_style = &vui_ss.text_menu,
			.image_style = &vui_ss.image,
			.check_color = vui_color_amethyst,
			.check_size = vui_check_size_default,
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
			.text_style = &vui_ss.text_menu,
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
			.text_style = &vui_ss.text_menu,
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
			.text_style = &vui_ss.text_menu,
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
			.text_style = &vui_ss.text_menu,
			.selection_color = VuiColor_init(0x34, 0x98, 0xdb, 0x80),
			.cursor_color = vui_color_amethyst,
			.cursor_width = vui_cursor_width_default,
		},
	},
	.slider_bar = {
		.margin = vui_margin_default,
		.padding = vui_padding_default,
		.bg_color = vui_color_midnight_blue,
		.border_color = vui_color_concrete,
		.border_width = vui_border_width_default,
		.radius = vui_radius_default,
	},
	.slider = {
		.margin = vui_margin_default,
		.bar_style = &vui_ss.slider_bar,
		.button_style = vui_ss.button_action,
		.bar_height = vui_slider_bar_height_default,
		.button_width = vui_slider_button_width_default,
	},
	.progress_bar = {
		.margin = vui_margin_default,
		.padding = vui_padding_default,
		.bg_color = vui_color_midnight_blue,
		.border_color = vui_color_concrete,
		.border_width = vui_border_width_default,
		.radius = vui_radius_default,
		.bar_color = vui_color_wisteria,
	},
	.scroll_view = {
		.margin = vui_margin_default,
		.padding = vui_padding_default,
		.bg_color = vui_color_midnight_blue,
		.border_color = vui_color_wet_asphalt,
		.border_width = vui_border_width_default,
		.radius = vui_radius_default,
		.bar_style = &vui_ss.scroll_bar,
	},
	.scroll_bar = {
		.margin = vui_margin_default,
		.padding = vui_padding_default,
		.bg_color = vui_color_midnight_blue,
		.border_color = vui_color_wet_asphalt,
		.border_width = vui_border_width_default,
		.radius = vui_radius_default,
		.slider_style = vui_ss.scroll_bar_slider,
		.slider_width = vui_scroll_bar_width_default,
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
	vui_render_image_(rect, 0.f, 0.f, glyph_texture_id, *uv_rect, _vui_render_glyph_color, VuiImageScaleMode_stretch, vui_true);
}

void vui_render_text(VuiVec2 left_top, VuiFontId font_id, float line_height, char* text, uint32_t text_length, VuiColor color, float wrap_at_width) {
	if (text_length) {
		_vui_render_glyph_color = color;
		_vui.position_text_fn(_vui.position_text_userdata, font_id, line_height, text, text_length, wrap_at_width, left_top, _vui_render_glyph);
	}
}

void vui_render_polyline(VuiVec2* points, uint32_t points_count, VuiColor color, float width, VuiBool connect_first_and_last) {
	vui_assert(points_count >= 2, "path must have atleast 2 points to render a polyline");

	float half_width = width / 2.0;

	uint32_t points_end = points_count;
	if (!connect_first_and_last) {
		points_end -= 1;
	}

	VuiVec2 start = points[0];
	VuiVec2 prev_vec = VuiVec2_zero;
	VuiVec2 rect_points[4];
	for (uint32_t idx = 0; idx < points_end; idx += 1) {
		VuiVec2 end = points[idx == points_count - 1 ? 0 : idx + 1];

		//
		// get the direction vector of the line.
		// then get both perpendicular offset vectors.
		VuiVec2 vec = VuiVec2_norm(VuiVec2_sub(end, start));
		VuiVec2 vec_left = VuiVec2_scale(VuiVec2_perp_left(vec), half_width);
		VuiVec2 vec_right = VuiVec2_scale(VuiVec2_perp_right(vec), half_width);

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
			if (!vui_approx_eq(inv_vec.x * prev_vec.x + inv_vec.y * prev_vec.y, 0.0f)) {
				// normalize if they are not perpendicular
				middle_vec = VuiVec2_norm(middle_vec);
			}

			rect_points[0] = VuiVec2_add(start, VuiVec2_scale(middle_vec, half_width));
			rect_points[1] = VuiVec2_add(start, VuiVec2_scale(prev_vec, half_width));
			rect_points[2] = start;
			rect_points[3] = VuiVec2_add(start, VuiVec2_scale(inv_vec, half_width));

			vui_render_convex_polygon(rect_points, 4, color);
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

	VuiRect clip_rect = _vui.render.clip_rect;
	for (uint32_t idx = 0; idx < points_count; idx += 1) {
		VuiVertex* vert = &w.verts[idx];
		*vert = VuiVertex_init(VuiRect_clip_pt(&clip_rect, points[idx]), VuiVec2_zero, color);
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

void vui_render_bezier_curve(VuiVec2 start_pos, VuiVec2 end_pos, VuiVec2 start_anchor_pos, VuiVec2 end_anchor_pos, VuiColor color, float width) {

}

void vui_render_image(const VuiRect* rect, VuiImageId image_id, VuiColor image_tint, VuiImageScaleMode scale_mode) {
	VuiImage* image = vui_image_get(image_id);
	vui_render_image_(rect, image->width, image->height, image->texture_id, image->uv_rect, image_tint, scale_mode, vui_false);
}

void vui_render_image_(const VuiRect* rect_ptr, float image_width, float image_height, VuiTextureId texture_id, VuiRect uv_rect, VuiColor color, VuiImageScaleMode scale_mode, VuiBool is_glyph) {
	VuiRenderWriter w = vui_render_get_writer(texture_id, 4, 6);
	vui_ensure_alloc_ok(w.verts);

	VuiRect rect = *rect_ptr;
	switch (scale_mode) {
		case VuiImageScaleMode_stretch:
			// do nothing, the size of the rectangle will stretch the image by default.
			break;
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
	vui_assert(ctrl_id, "cannot remove an ctrl with a NULL identifier");
	VuiPoolId pool_id = (ctrl_id & _VuiCtrlId_pool_id_MASK) >> _VuiCtrlId_pool_id_SHIFT;
	uint16_t counter = (ctrl_id & _VuiCtrlId_counter_MASK) >> _VuiCtrlId_counter_SHIFT;
	_VuiCtrl* ctrl = _VuiPool_id_to_ptr((_VuiPool*)&_vui.ctrl_pool, pool_id, sizeof(_VuiCtrl));
	vui_assert(ctrl->counter == counter, "trying to remove an ctrl with an old identifier");
	_VuiPool_dealloc((_VuiPool*)&_vui.ctrl_pool, pool_id, sizeof(_VuiCtrl), alignof(_VuiCtrl));
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
	vui_assert(ctrl->counter == counter, "trying to get an ctrl with an old identifier");
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
		parent->child_first_id = ctrl->id;
		parent->child_last_id = ctrl->id;
	}
	ctrl->parent_id = _vui.build.parent_ctrl_id;
}

void VuiImage_render(VuiCtrl* ctrl, const VuiCtrlStyle* style, VuiRect* content_rect) {
	VuiImageScaleMode scale_mode = ctrl->attributes.image_scale_mode;
	vui_render_image(content_rect, ctrl->image_id, ctrl->image_tint, scale_mode);
}

void VuiText_render(VuiCtrl* ctrl, const VuiCtrlStyle* style, VuiRect* content_rect) {
	const VuiTextStyle* style_ = (VuiTextStyle*)style;
	char* text = &_vui.render.w->text[ctrl->text_start_idx];
	vui_render_text(content_rect->left_top, style_->font_id, style_->line_height, text, ctrl->text_length, style_->color, ctrl->text_wrap_width);
}

void VuiCheckBoxCheck_render(VuiCtrl* ctrl, const VuiCtrlStyle* style, VuiRect* content_rect) {
	VuiCtrl* parent = vui_ctrl_get(ctrl->parent_id);
	VuiBool is_active = (parent->state_flags & VuiCtrlStateFlags_active) == VuiCtrlStateFlags_active;
	if (is_active)
		vui_render_rect(content_rect, ((VuiCheckBoxStyle*)parent->style)->check_color, parent->style->radius);
}

void VuiProgressBar_render(VuiCtrl* ctrl, const VuiCtrlStyle* style, VuiRect* content_rect) {
	VuiCtrl* parent = vui_ctrl_get(ctrl->parent_id);
	vui_render_rect(content_rect, ((VuiProgressBarStyle*)parent->style)->bar_color, parent->style->radius);
}

static VuiVec2 vui_get_text_size(char* text, uint32_t text_length, float wrap_at_width, VuiFontId font_id, float line_height) {
	return _vui.position_text_fn(_vui.position_text_userdata, font_id, line_height, text, text_length, wrap_at_width, VuiVec2_zero, NULL);
}

void VuiTextBoxCursor_render(VuiCtrl* ctrl, const VuiCtrlStyle* _style, VuiRect* content_rect) {
	VuiCtrl* parent = vui_ctrl_get(ctrl->parent_id);
	const VuiTextBoxStyle* style = (VuiTextBoxStyle*)parent->style;
	const VuiTextStyle* text_style = style->text_style;

	//
	// if we are focused, render the cursor or selection box.
	if (vui_ctrl_is_focused(parent->id) && _vui.input.focused_text_box.string) {
		float cursor_width = 0.f;
		VuiColor color = {0};

		// get the offset to the cursor
		VuiVec2 start_idx_offset = vui_get_text_size(_vui.input.focused_text_box.string, _vui.input.focused_text_box.cursor_idx, 0.f, text_style->font_id, text_style->line_height);
		if (_vui.input.focused_text_box.select_offset) {
			//
			// we are selecting so find the other end of the selection box
			VuiVec2 end_idx_offset = vui_get_text_size(_vui.input.focused_text_box.string,
				_vui.input.focused_text_box.cursor_idx + _vui.input.focused_text_box.select_offset, 0.f, text_style->font_id, text_style->line_height);
			if (_vui.input.focused_text_box.select_offset < 0) {
				VuiVec2 tmp = end_idx_offset;
				end_idx_offset = start_idx_offset;
				start_idx_offset = tmp;
			}

			color = style->selection_color;
			cursor_width = end_idx_offset.x - start_idx_offset.x;
		} else {
			//
			// no selection, we have a cursor
			color = style->cursor_color;
			cursor_width = style->cursor_width;

			//
			// evenly place the cursor in between two characters
			if (_vui.input.focused_text_box.string_len > 0) {
				start_idx_offset.x -= cursor_width / 2.f;
			}
		}

		VuiCtrl* text_ctrl = vui_ctrl_get(ctrl->sibling_prev_id);
		float left = text_ctrl->rect.left + text_style->padding.left + start_idx_offset.x;
		float top = text_ctrl->rect.top + text_style->padding.top;
		VuiRect rect = VuiRect_init_wh(left, top, cursor_width, text_style->line_height);

		vui_render_rect(&rect, color, style->radius);
	}
}

void vui_ctrl_start_(VuiCtrlSibId sib_id, VuiCtrlFlags flags, VuiActiveChange active_change, const VuiCtrlStyle* style, VuiCtrlStyleInterpFn style_interp_fn, VuiCtrlRenderFn render_fn) {
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
	ctrl->flags = flags;
	ctrl->style_interp_fn = style_interp_fn;
	ctrl->render_fn = render_fn;
	if (style) {
		VuiCtrl_target_style(ctrl, style);
	}

	if (flags & VuiCtrlFlags_focusable) {
		VuiFocusState focus_state = _vui_ctrl_focus_state(ctrl->id);
		ctrl->focus_state = focus_state;

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
}

void vui_ctrl_end() {
	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);
	_vui.build.parent_ctrl_id = ctrl->parent_id;
	_vui.build.sibling_prev_ctrl_id = ctrl->id;
}

void vui_text_(VuiCtrlSibId sib_id, char* text, uint32_t text_length, float wrap_at_width, const VuiTextStyle* style) {
	uint32_t text_start_idx = VuiStk_count(_vui.build.w->text);
	if (text_length) {
		char* t = VuiStk_push_many(&_vui.build.w->text, text_length);
		vui_ensure_alloc_ok(t);
		memcpy(t, text, text_length);
	}

	VuiCtrl* parent = vui_ctrl_get(_vui.build.parent_ctrl_id);

	VuiVec2 size = vui_get_text_size(text, text_length, wrap_at_width, style->font_id, style->line_height);
	if (size.y == 0.f) {
		size.y = style->line_height;
	}

	size.x += VuiThickness_horizontal(&style->padding);
	size.y += VuiThickness_vertical(&style->padding);

	vui_scope_width(size.x)
	vui_scope_height(size.y) {
		vui_ctrl_start_(sib_id, 0, 0, &style->ctrl, VuiTextStyle_interp, VuiText_render);
		VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);
		ctrl->text_start_idx = text_start_idx;
		ctrl->text_length = text_length;
		ctrl->text_wrap_width = wrap_at_width;
		vui_ctrl_end();
	}
}

void vui_image(VuiCtrlSibId sib_id, VuiImageId image_id, VuiColor image_tint, const VuiCtrlStyle* style) {
	vui_ctrl_start_(sib_id, 0, 0, style, VuiCtrlStyle_interp, VuiImage_render);
	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);
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
		vui_ctrl_start_(sib_id, 0, 0, NULL, NULL, NULL);
		vui_ctrl_end();
	}
}

void vui_separator(VuiCtrlSibId sib_id, const VuiSeparatorStyle* style) {
	VuiCtrl* parent = vui_ctrl_get(_vui.build.parent_ctrl_id);
	VuiBool is_row = parent->layout_type == VuiLayoutType_row;
	vui_assert(is_row || parent->layout_type == VuiLayoutType_column, "vui_separator can only be used in a non-wrapping row or column layout");
	vui_assert(!parent->attributes.layout_wrap, "vui_separator can only be used in a non-wrapping row or column layout");

	float width = is_row ? vui_fill_len : style->size;
	float height = is_row ? style->size : vui_fill_len;

	vui_scope_width(width)
	vui_scope_height(height) {
		vui_ctrl_start_(sib_id, 0, 0, &style->ctrl, VuiSeparatorStyle_interp, NULL);
		vui_ctrl_end();
	}
}

VuiFocusState vui_button_start(VuiCtrlSibId sib_id, const VuiButtonStyle styles[VuiCtrlState_COUNT]) {
	vui_ctrl_start_(sib_id, VuiCtrlFlags_focusable | VuiCtrlFlags_pressable, 0, NULL, VuiButtonStyle_interp, NULL);
	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);

	//
	// target the style that reflects the state of the control.
	VuiCtrl_target_style(ctrl, &styles[VuiCtrl_state(ctrl)].ctrl);

	return ctrl->focus_state;
}

void vui_button_end() {
	vui_ctrl_end();
}

VuiFocusState vui_button(VuiCtrlSibId sib_id, const VuiButtonStyle styles[VuiCtrlState_COUNT]) {
	VuiFocusState state = vui_button_start(sib_id, styles);
	vui_button_end();
	return state;
}

VuiFocusState vui_text_button_(VuiCtrlSibId sib_id, char* text, uint32_t text_length, const VuiButtonStyle styles[VuiCtrlState_COUNT]) {
	VuiFocusState state = vui_button_start(sib_id, styles);
	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);

	//
	// get the target style so we can use it for the text
	VuiButtonStyle* style = ctrl->target_style ? (VuiButtonStyle*)ctrl->target_style : (VuiButtonStyle*)ctrl->style;

	vui_scope_offset(0.f, 0.f)
	vui_scope_align(VuiAlign_center) {
		vui_text_(vui_sib_id, text, text_length, 0.f, style->text_style);
	}

	vui_button_end();
	return state;
}

VuiFocusState vui_image_button(VuiCtrlSibId sib_id, VuiImageId image_id, VuiColor image_tint, const VuiButtonStyle styles[VuiCtrlState_COUNT]) {
	VuiFocusState state = vui_button_start(sib_id, styles);
	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);

	//
	// get the target style so we can use it for the image
	VuiButtonStyle* style = ctrl->target_style ? (VuiButtonStyle*)ctrl->target_style : (VuiButtonStyle*)ctrl->style;

	vui_scope_offset(0.f, 0.f)
	vui_scope_align(VuiAlign_center) {
		vui_image(vui_sib_id, image_id, image_tint, style->image_style);
	}
	vui_button_end();
	return state;
}

VuiFocusState vui_image_text_button_(VuiCtrlSibId sib_id, VuiImageId image_id, VuiColor image_tint, char* text, uint32_t text_length, const VuiButtonStyle styles[VuiCtrlState_COUNT]) {
	VuiFocusState state = vui_button_start(sib_id, styles);
	vui_column_layout();
	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);

	//
	// get the target style so we can use it for the image and text
	VuiButtonStyle* style = ctrl->target_style ? (VuiButtonStyle*)ctrl->target_style : (VuiButtonStyle*)ctrl->style;

	vui_scope_offset(0.f, 0.f)
	vui_scope_align(VuiAlign_center) {
		vui_image(vui_sib_id, image_id, image_tint, style->image_style);
		vui_text_(vui_sib_id, text, text_length, 0.f, style->text_style);
	}

	vui_button_end();
	return state;
}

VuiBool vui_toggle_button_start(VuiCtrlSibId sib_id, VuiBool* pressed, const VuiButtonStyle styles[VuiCtrlState_COUNT]) {
	vui_ctrl_start_(sib_id, VuiCtrlFlags_focusable | VuiCtrlFlags_toggleable, pressed ? *pressed + 1 : 0, NULL, VuiButtonStyle_interp, NULL);
	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);

	//
	// target the style that reflects the state of the control.
	VuiCtrl_target_style(ctrl, &styles[VuiCtrl_state(ctrl)].ctrl);

	VuiBool is_active = (ctrl->state_flags & VuiCtrlStateFlags_active) == VuiCtrlStateFlags_active;
	if (pressed) *pressed = is_active;
	return is_active;
}

void vui_toggle_button_end() {
	vui_ctrl_end();
}

VuiBool vui_text_toggle_button_(VuiCtrlSibId sib_id, VuiBool* pressed, char* text, uint32_t text_length, const VuiButtonStyle styles[VuiCtrlState_COUNT]) {
	VuiBool p = vui_toggle_button_start(sib_id, pressed, styles);
	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);

	//
	// get the target style so we can use it for the text
	VuiButtonStyle* style = ctrl->target_style ? (VuiButtonStyle*)ctrl->target_style : (VuiButtonStyle*)ctrl->style;

	vui_scope_offset(0.f, 0.f)
	vui_scope_align(VuiAlign_center) {
		vui_text_(vui_sib_id, text, text_length, 0.f, style->text_style);
	}
	vui_toggle_button_end();
	return p;
}

VuiBool vui_image_toggle_button(VuiCtrlSibId sib_id, VuiBool* pressed, VuiImageId image_id, VuiColor image_tint, const VuiButtonStyle styles[VuiCtrlState_COUNT]) {
	VuiBool p = vui_toggle_button_start(sib_id, pressed, styles);
	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);

	//
	// get the target style so we can use it for the image
	VuiButtonStyle* style = ctrl->target_style ? (VuiButtonStyle*)ctrl->target_style : (VuiButtonStyle*)ctrl->style;

	vui_scope_offset(0.f, 0.f)
	vui_scope_align(VuiAlign_center) {
		vui_image(vui_sib_id, image_id, image_tint, style->image_style);
	}
	vui_toggle_button_end();
	return p;
}

VuiBool vui_image_text_toggle_button_(VuiCtrlSibId sib_id, VuiBool* pressed, VuiImageId image_id, VuiColor image_tint, char* text, uint32_t text_length, const VuiButtonStyle styles[VuiCtrlState_COUNT]) {
	VuiBool p = vui_toggle_button_start(sib_id, pressed, styles);
	vui_column_layout();
	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);

	//
	// get the target style so we can use it for the image and text
	VuiButtonStyle* style = ctrl->target_style ? (VuiButtonStyle*)ctrl->target_style : (VuiButtonStyle*)ctrl->style;

	vui_scope_offset(0.f, 0.f)
	vui_scope_align(VuiAlign_center) {
		vui_image(vui_sib_id, image_id, image_tint, style->image_style);
		vui_text_(vui_sib_id, text, text_length, 0.f, style->text_style);
	}
	vui_toggle_button_end();
	return p;
}

VuiBool vui_select_button_start(VuiCtrlSibId sib_id, VuiCtrlSibId* selected_sib_id, const VuiButtonStyle styles[VuiCtrlState_COUNT]) {
	vui_ctrl_start_(sib_id, VuiCtrlFlags_focusable | VuiCtrlFlags_selectable, (*selected_sib_id == sib_id) + 1, NULL, VuiButtonStyle_interp, NULL);
	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);

	//
	// target the style that reflects the state of the control.
	VuiCtrl_target_style(ctrl, &styles[VuiCtrl_state(ctrl)].ctrl);

	VuiBool is_active = (ctrl->state_flags & VuiCtrlStateFlags_active) == VuiCtrlStateFlags_active;
	if (is_active) *selected_sib_id = sib_id;
	return is_active;
}

void vui_select_button_end() {
	vui_button_end();
}

VuiBool vui_text_select_button_(VuiCtrlSibId sib_id, VuiCtrlSibId* selected_sib_id, char* text, uint32_t text_length, const VuiButtonStyle styles[VuiCtrlState_COUNT]) {
	VuiBool p = vui_select_button_start(sib_id, selected_sib_id, styles);
	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);

	//
	// get the target style so we can use it for the text
	VuiButtonStyle* style = ctrl->target_style ? (VuiButtonStyle*)ctrl->target_style : (VuiButtonStyle*)ctrl->style;

	vui_scope_offset(0.f, 0.f)
	vui_scope_align(VuiAlign_center) {
		vui_text_(vui_sib_id, text, text_length, 0.f, style->text_style);
	}
	vui_select_button_end();
	return p;
}

VuiBool vui_image_select_button(VuiCtrlSibId sib_id, VuiCtrlSibId* selected_sib_id, VuiImageId image_id, VuiColor image_tint, const VuiButtonStyle styles[VuiCtrlState_COUNT]) {
	VuiBool p = vui_select_button_start(sib_id, selected_sib_id, styles);
	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);

	//
	// get the target style so we can use it for the imagetext
	VuiButtonStyle* style = ctrl->target_style ? (VuiButtonStyle*)ctrl->target_style : (VuiButtonStyle*)ctrl->style;

	vui_scope_offset(0.f, 0.f)
	vui_scope_align(VuiAlign_center) {
		vui_image(vui_sib_id, image_id, image_tint, style->image_style);
	}
	vui_select_button_end();
	return p;
}

VuiBool vui_image_text_select_button_(VuiCtrlSibId sib_id, VuiCtrlSibId* selected_sib_id, VuiImageId image_id, VuiColor image_tint, char* text, uint32_t text_length, const VuiButtonStyle styles[VuiCtrlState_COUNT]) {
	VuiBool p = vui_select_button_start(sib_id, selected_sib_id, styles);
	vui_column_layout();
	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);

	//
	// get the target style so we can use it for the image and text
	VuiButtonStyle* style = ctrl->target_style ? (VuiButtonStyle*)ctrl->target_style : (VuiButtonStyle*)ctrl->style;

	vui_scope_offset(0.f, 0.f)
	vui_scope_align(VuiAlign_center) {
		vui_image(vui_sib_id, image_id, image_tint, style->image_style);
		vui_text_(vui_sib_id, text, text_length, 0.f, style->text_style);
	}

	vui_select_button_end();
	return p;
}


VuiBool vui_check_box(VuiCtrlSibId sib_id, VuiBool* checked, const VuiCheckBoxStyle styles[VuiCtrlState_COUNT]) {
	VuiBool is_active = vui_false;
	vui_scope_width(vui_auto_len)
	vui_scope_height(vui_auto_len) {
		vui_ctrl_start_(sib_id, VuiCtrlFlags_focusable | VuiCtrlFlags_toggleable, checked ? *checked + 1 : 0, NULL, VuiCheckBoxStyle_interp, NULL);
		VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);

		//
		// target the style that reflects the state of the control.
		const VuiCheckBoxStyle* style = &styles[VuiCtrl_state(ctrl)];
		VuiCtrl_target_style(ctrl, &style->ctrl);

		is_active = (ctrl->state_flags & VuiCtrlStateFlags_active) == VuiCtrlStateFlags_active;
		if (checked) *checked = is_active;

		vui_scope_width(style->check_size)
		vui_scope_height(style->check_size) {
			vui_ctrl_start_(vui_sib_id, 0, 0, NULL, NULL, VuiCheckBoxCheck_render);
			vui_ctrl_end();
		}

		vui_ctrl_end();
	}
	return is_active;
}

VuiBool vui_text_check_box_(VuiCtrlSibId sib_id, VuiBool* checked, char* text, uint32_t text_length, const VuiCheckBoxStyle styles[VuiCtrlState_COUNT]) {
	vui_ctrl_start_(sib_id, VuiCtrlFlags_focusable | VuiCtrlFlags_toggleable, checked ? *checked + 1 : 0, NULL, NULL, NULL);
	vui_column_layout();

	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);
	VuiBool c = (ctrl->state_flags & VuiCtrlStateFlags_active) == VuiCtrlStateFlags_active;
	if (!checked) checked = &c;

	VuiBool state;
	vui_scope_offset(0.f, 0.f)
	vui_scope_align(VuiAlign_center) {
		state = vui_check_box(vui_sib_id, checked, styles);
		VuiCtrl* check_box = vui_ctrl_get(_vui.build.sibling_prev_ctrl_id);

		//
		// synchronize the active states between the check box and the parent
		if (check_box->state_flags & VuiCtrlStateFlags_active) {
			ctrl->state_flags |= VuiCtrlStateFlags_active;
		} else {
			ctrl->state_flags &= ~VuiCtrlStateFlags_active;
		}

		//
		// get the target style so we can use it for the text
		VuiCheckBoxStyle* style = check_box->target_style ? (VuiCheckBoxStyle*)check_box->target_style : (VuiCheckBoxStyle*)check_box->style;

		vui_text_(vui_sib_id, text, text_length, 0.f, style->text_style);
	}

	vui_ctrl_end();
	return state;
}

VuiBool vui_image_check_box(VuiCtrlSibId sib_id, VuiBool* checked, VuiImageId image_id, VuiColor image_tint, const VuiCheckBoxStyle styles[VuiCtrlState_COUNT]) {
	vui_ctrl_start_(sib_id, VuiCtrlFlags_focusable | VuiCtrlFlags_toggleable, checked ? *checked + 1 : 0, NULL, NULL, NULL);
	vui_column_layout();

	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);
	VuiBool c = (ctrl->state_flags & VuiCtrlStateFlags_active) == VuiCtrlStateFlags_active;
	if (!checked) checked = &c;

	VuiBool state;
	vui_scope_offset(0.f, 0.f)
	vui_scope_align(VuiAlign_center) {
		state = vui_check_box(vui_sib_id, checked, styles);
		VuiCtrl* check_box = vui_ctrl_get(_vui.build.sibling_prev_ctrl_id);

		//
		// synchronize the active states between the check box and the parent
		if (check_box->state_flags & VuiCtrlStateFlags_active) {
			ctrl->state_flags |= VuiCtrlStateFlags_active;
		} else {
			ctrl->state_flags &= ~VuiCtrlStateFlags_active;
		}

		//
		// get the target style so we can use it for the image
		VuiCheckBoxStyle* style = check_box->target_style ? (VuiCheckBoxStyle*)check_box->target_style : (VuiCheckBoxStyle*)check_box->style;

		vui_image(vui_sib_id, image_id, image_tint, style->image_style);
	}

	vui_ctrl_end();
	return state;
}

VuiBool vui_radio_button(VuiCtrlSibId sib_id, VuiCtrlSibId* selected_sib_id, const VuiRadioButtonStyle styles[VuiCtrlState_COUNT]) {
	VuiBool is_active = vui_false;
	vui_scope_width(vui_auto_len)
	vui_scope_height(vui_auto_len) {
		vui_ctrl_start_(sib_id, VuiCtrlFlags_focusable | VuiCtrlFlags_selectable, (*selected_sib_id == sib_id) + 1, NULL, VuiRadioButtonStyle_interp, NULL);
		VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);

		//
		// target the style that reflects the state of the control.
		const VuiRadioButtonStyle* style = &styles[VuiCtrl_state(ctrl)];
		VuiCtrl_target_style(ctrl, &style->ctrl);

		is_active = (ctrl->state_flags & VuiCtrlStateFlags_active) == VuiCtrlStateFlags_active;
		if (is_active) *selected_sib_id = sib_id;

		vui_scope_width(style->check_size)
		vui_scope_height(style->check_size) {
			vui_ctrl_start_(vui_sib_id, 0, 0, NULL, NULL, VuiCheckBoxCheck_render);
			vui_ctrl_end();
		}


		vui_ctrl_end();
	}
	return is_active;
}

VuiBool vui_text_radio_button_(VuiCtrlSibId sib_id, VuiCtrlSibId* selected_sib_id, char* text, uint32_t text_length, const VuiRadioButtonStyle styles[VuiCtrlState_COUNT]) {
	vui_ctrl_start_(sib_id + vui_sib_id, VuiCtrlFlags_focusable | VuiCtrlFlags_selectable, (*selected_sib_id == sib_id) + 1, NULL, NULL, NULL);
	vui_column_layout();

	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);
	VuiBool is_active = (ctrl->state_flags & VuiCtrlStateFlags_active) == VuiCtrlStateFlags_active;
	if (is_active) *selected_sib_id = sib_id;

	VuiBool state;
	vui_scope_offset(0.f, 0.f)
	vui_scope_align(VuiAlign_center) {
		state = vui_radio_button(sib_id, selected_sib_id, styles);
		VuiCtrl* radio_button = vui_ctrl_get(_vui.build.sibling_prev_ctrl_id);

		//
		// synchronize the active states between the check box and the parent
		if (radio_button->state_flags & VuiCtrlStateFlags_active) {
			ctrl->state_flags |= VuiCtrlStateFlags_active;
		} else {
			ctrl->state_flags &= ~VuiCtrlStateFlags_active;
		}

		//
		// get the target style so we can use it for the text
		VuiRadioButtonStyle* style = radio_button->target_style ? (VuiRadioButtonStyle*)radio_button->target_style : (VuiRadioButtonStyle*)radio_button->style;

		vui_text_(vui_sib_id, text, text_length, 0.f, style->text_style);
	}

	vui_ctrl_end();
	return state;
}

VuiBool vui_image_radio_button(VuiCtrlSibId sib_id, VuiCtrlSibId* selected_sib_id, VuiImageId image_id, VuiColor image_tint, const VuiRadioButtonStyle styles[VuiCtrlState_COUNT]) {
	vui_ctrl_start_(sib_id + vui_sib_id, VuiCtrlFlags_focusable | VuiCtrlFlags_selectable, (*selected_sib_id == sib_id) + 1, NULL, NULL, NULL);
	vui_column_layout();

	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);
	VuiBool is_active = (ctrl->state_flags & VuiCtrlStateFlags_active) == VuiCtrlStateFlags_active;
	if (is_active) *selected_sib_id = sib_id;

	VuiBool state;
	vui_scope_offset(0.f, 0.f)
	vui_scope_align(VuiAlign_center) {
		state = vui_radio_button(sib_id, selected_sib_id, styles);
		VuiCtrl* radio_button = vui_ctrl_get(_vui.build.sibling_prev_ctrl_id);

		//
		// synchronize the active states between the check box and the parent
		if (radio_button->state_flags & VuiCtrlStateFlags_active) {
			ctrl->state_flags |= VuiCtrlStateFlags_active;
		} else {
			ctrl->state_flags &= ~VuiCtrlStateFlags_active;
		}

		//
		// get the target style so we can use it for the image
		VuiRadioButtonStyle* style = radio_button->target_style ? (VuiRadioButtonStyle*)radio_button->target_style : (VuiRadioButtonStyle*)radio_button->style;

		vui_image(vui_sib_id, image_id, image_tint, style->image_style);
	}

	vui_ctrl_end();
	return state;
}

typedef uint8_t _VuiSliderType;
enum {
	_VuiSliderType_float,
	_VuiSliderType_u32,
	_VuiSliderType_s32,
};

void _vui_slider(VuiCtrlSibId sib_id, void* value_out, void* min, void* max, const VuiSliderStyle* style, _VuiSliderType type) {
	vui_scope_height(vui_auto_len)
	vui_scope_layout_wrap(vui_false)
	vui_ctrl_start_(sib_id, 0, 0, &style->ctrl, VuiSliderStyle_interp, NULL);

	vui_scope_align(VuiAlign_left_top) {
		VuiCtrl* parent = vui_ctrl_get(_vui.build.parent_ctrl_id);

		//
		// create the bar of the slider. the bar is shrunk by the button width and the margin to fit it inside the parent's clipping rectangle.
		float parent_inner_width = VuiRect_width(&parent->rect) - VuiThickness_horizontal(&parent->style->padding) - parent->style->border_width * 2.f;
		float bar_width = parent_inner_width - style->button_width - VuiThickness_horizontal(&style->bar_style->margin);
		vui_scope_width(bar_width)
		vui_scope_height(style->bar_height)
		vui_scope_offset(style->button_width * 0.5f, style->button_width * 0.25f)
		vui_ctrl(vui_sib_id, style->bar_style);

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

		button_offset += style->bar_style->margin.left - style->button_style->margin.left;

		//
		// make the button and if it is held down, then modify the value.
		vui_scope_offset(button_offset, style->bar_style->margin.top - style->button_style->margin.top)
		vui_scope_width(style->button_width)
		vui_scope_height(style->button_width)
		if (vui_button(vui_sib_id, style->button_style) & VuiFocusState_held) {
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
	}

	vui_ctrl_end();
}

void vui_slider_uint(VuiCtrlSibId sib_id, uint32_t* value_out, uint32_t min, uint32_t max, const VuiSliderStyle* style) {
	*value_out = *value_out < min ? min : (*value_out > max ? max : *value_out);
	_vui_slider(sib_id, value_out, &min, &max, style, _VuiSliderType_u32);
	*value_out = *value_out < min ? min : (*value_out > max ? max : *value_out);
}

void vui_slider_sint(VuiCtrlSibId sib_id, int32_t* value_out, int32_t min, int32_t max, const VuiSliderStyle* style) {
	*value_out = *value_out < min ? min : (*value_out > max ? max : *value_out);
	_vui_slider(sib_id, value_out, &min, &max, style, _VuiSliderType_s32);
	*value_out = *value_out < min ? min : (*value_out > max ? max : *value_out);
}

void vui_slider_float(VuiCtrlSibId sib_id, float* value_out, float min, float max, const VuiSliderStyle* style) {
	*value_out = vui_clamp(*value_out, min, max);
	_vui_slider(sib_id, value_out, &min, &max, style, _VuiSliderType_float);
	*value_out = vui_clamp(*value_out, min, max);
}

void vui_progress_bar(VuiCtrlSibId sib_id, float value, float min, float max, const VuiProgressBarStyle* style) {
	value = vui_clamp(value, min, max);
	float ratio = (value - min) / (max - min);

	vui_scope_layout_wrap(vui_false)
	vui_ctrl_start_(sib_id, 0, 0, &style->ctrl, VuiProgressBarStyle_interp, NULL);
		vui_column_layout();

		vui_scope_width_ratio(ratio)
		vui_scope_height(vui_fill_len)
		vui_ctrl_start_(vui_sib_id, 0, 0, NULL, NULL, VuiProgressBar_render);
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
		const VuiScrollViewStyle* style = (VuiScrollViewStyle*)ctrl->style;
		const VuiScrollBarStyle* bar_style = style->bar_style;
		const VuiButtonStyle* button_style = bar_style->slider_style;
		container_size.x -= style->border_width * 2.f + VuiThickness_horizontal(&style->padding);
		container_size.y -= style->border_width * 2.f + VuiThickness_vertical(&style->padding);

		if (ctrl->flags & _VuiCtrlFlags_show_vertical_bar) {
			//
			// we have a vertical scroll bar, so remove the outer width of the control from the container's width
			container_size.x -= bar_style->slider_width +
				VuiThickness_horizontal(&bar_style->margin) + VuiThickness_horizontal(&bar_style->padding) +
				bar_style->border_width * 2.f + VuiThickness_horizontal(&button_style->margin);
		}
		if (ctrl->flags & _VuiCtrlFlags_show_horizontal_bar) {
			//
			// we have a horizontal scroll bar, so remove the outer height of the control from the container's height
			container_size.y -= bar_style->slider_width +
				VuiThickness_vertical(&bar_style->margin) + VuiThickness_vertical(&bar_style->padding) +
				bar_style->border_width * 2.f + VuiThickness_vertical(&button_style->margin);
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

void vui_scroll_view_start_(VuiCtrlSibId sib_id, VuiVec2* content_offset_in_out, VuiVec2* size_in_out, VuiScrollFlags flags, const VuiScrollViewStyle* style) {
	vui_ctrl_start_(sib_id, flags, 0, &style->ctrl, VuiScrollViewStyle_interp, NULL);
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
	// create the infinitely sized control to house the scrollable content.
	vui_scope_width(vui_auto_len)
	vui_scope_height(vui_auto_len)
	vui_scope_offset(0.f, 0.f)
	vui_scope_align(VuiAlign_left_top)
	vui_ctrl_start_(-1, 0, 0, NULL, NULL, NULL);
	ctrl = vui_ctrl_get(scroll_view_ctrl_id);
	ctrl->scroll_content_id = _vui.build.parent_ctrl_id;

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
VuiBool _vui_scroll_bar(VuiCtrlSibId sib_id, float length, float container_length, float* content_offset_in_out, float content_length, VuiBool is_horizontal, const VuiScrollBarStyle* style) {
	if (content_length < 1.f) content_length = 1.f;
	vui_scope_width(is_horizontal ? length : vui_auto_len)
	vui_scope_height(is_horizontal ? vui_auto_len : length)
	vui_ctrl_start(sib_id, &style->ctrl);
	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);

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

	*slider_long_side_length = slider_len - thickness_dir_len_fn(&style->slider_style->margin);

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
		VuiFocusState state = vui_button_start(vui_sib_id, style->slider_style);
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
	const VuiScrollViewStyle* style = (VuiScrollViewStyle*)scroll_view->style;
	const VuiScrollBarStyle* bar_style = style->bar_style;
	const VuiButtonStyle* slider_style = bar_style->slider_style;
	VuiCtrlFlags flags = scroll_view->flags;
	VuiBool can_show_vertical_bar = flags & VuiCtrlFlags_scrollable_vertical;
	VuiBool can_show_horizontal_bar = flags & VuiCtrlFlags_scrollable_vertical;
	VuiBool show_vertical_bar = vui_false;
	VuiBool show_horizontal_bar = vui_false;

	float vertical_scroll_bar_outer_width = bar_style->slider_width +
		VuiThickness_horizontal(&bar_style->margin) + VuiThickness_horizontal(&bar_style->padding) +
		bar_style->border_width * 2.f + VuiThickness_horizontal(&bar_style->slider_style->margin);

	float horizontal_scroll_bar_outer_height = bar_style->slider_width +
		VuiThickness_vertical(&bar_style->margin) + VuiThickness_vertical(&bar_style->padding) +
		bar_style->border_width * 2.f + VuiThickness_vertical(&bar_style->slider_style->margin);


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
		VuiThickness_horizontal(&style->padding) - VuiThickness_horizontal(&bar_style->margin);
	float scroll_bar_length_vertical = scroll_view_size.y - style->border_width * 2 -
		VuiThickness_vertical(&style->padding) - VuiThickness_vertical(&bar_style->margin);
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
				offset_changed = _vui_scroll_bar(-2, scroll_bar_length_horizontal, container_size.x, &scroll_offset.x, content_size.x, vui_true, bar_style);
		}

		if (show_vertical_bar) {
			vui_scope_align(VuiAlign_right_top)
			offset_changed = _vui_scroll_bar(-3, scroll_bar_length_vertical, container_size.y, &scroll_offset.y, content_size.y, vui_false, bar_style);
		}

		if (flags & VuiCtrlFlags_resizable) {
			vui_scope_width(vui_auto_len)
			vui_scope_height(vui_auto_len)
			vui_scope_align(VuiAlign_right_bottom)
			vui_ctrl_start(-4, &bar_style->ctrl);
			vui_scope_width(bar_style->slider_width)
			vui_scope_height(bar_style->slider_width)
			state = vui_button(-4, bar_style->slider_style);
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

VuiBool vui_text_box_(VuiCtrlSibId sib_id, char* string_in_out, uint32_t string_in_out_cap, const VuiTextBoxStyle styles[VuiCtrlState_COUNT], _VuiInputBoxType type) {
	vui_scope_height(vui_auto_len)
	vui_ctrl_start_(sib_id, VuiCtrlFlags_focusable | VuiCtrlFlags_scrollable_horizontal, 0, NULL, VuiTextBoxStyle_interp, NULL);
	VuiCtrlId text_box_ctrl_id = _vui.build.parent_ctrl_id;
	VuiCtrl* ctrl = vui_ctrl_get(text_box_ctrl_id);

	//
	// target the style that reflects the state of the control.
	const VuiTextBoxStyle* style = &styles[VuiCtrl_state(ctrl)];
	const VuiTextStyle* text_style = style->text_style;
	VuiCtrl_target_style(ctrl, &style->ctrl);

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
			_vui.input.focused_text_box.select_offset = strlen(string_in_out);
			_vui.input.focused_text_box.type = type;
		} else {
			// we have had focus before and still do.
			// check to see if the text has changed.
			has_changed = _vui.input.focused_text_box.has_changed;
		}

		{
			//
			// workout the scroll offset of the scroll when the cursor is outside of the box's inner boundary.
			//
			uint32_t cursor_idx = _vui.input.focused_text_box.cursor_idx + _vui.input.focused_text_box.select_offset;
			VuiVec2 cursor_offset = vui_get_text_size(_vui.input.focused_text_box.string, cursor_idx, 0.f, text_style->font_id, text_style->line_height);

			float margin_padding = VuiThickness_horizontal(&style->padding) + VuiThickness_horizontal(&text_style->margin);
			float box_inner_size = VuiRect_width(&ctrl->rect) - margin_padding - style->border_width * 2.f;
			float scroll_offset = ctrl->scroll_offset.x;
			float cursor_offset_rel = cursor_offset.x + scroll_offset;

			if (cursor_offset_rel >= box_inner_size) {
				scroll_offset -= (cursor_offset_rel - box_inner_size) + style->cursor_width + margin_padding;
			} else if (cursor_offset_rel < 0.f) {
				scroll_offset += (0.f - cursor_offset_rel) + style->cursor_width + margin_padding;
			}

			scroll_offset = vui_clamp(scroll_offset, -cursor_offset.x, 0.f);

			ctrl->scroll_offset.x = scroll_offset;
		}
	}

	vui_scope_offset(0.f, 0.f)
	vui_scope_align(VuiAlign_left_top) {
		//
		// the text of the text box
		vui_text_(vui_sib_id, string_in_out, strlen(string_in_out), 0.f, text_style);
		VuiCtrl* text_ctrl = vui_ctrl_get(_vui.build.sibling_prev_ctrl_id);
		ctrl = vui_ctrl_get(text_box_ctrl_id);
		ctrl->scroll_content_id = text_ctrl->id;

		//
		// the control for the cursor
		vui_scope_width(vui_fill_len)
		vui_scope_height(vui_fill_len)
		vui_ctrl_start_(vui_sib_id, 0, 0, NULL, NULL, VuiTextBoxCursor_render);
		vui_ctrl_end();
	}

	vui_ctrl_end();
	return has_changed;
}

VuiBool vui_text_box(VuiCtrlSibId sib_id, char* string_in_out, uint32_t string_in_out_cap, const VuiTextBoxStyle styles[VuiCtrlState_COUNT]) {
	vui_assert(string_in_out, "a string buffer must be provided");
	return vui_text_box_(sib_id, string_in_out, string_in_out_cap, styles, _VuiInputBoxType_text);
}

VuiBool vui_input_box_uint(VuiCtrlSibId sib_id, uint32_t* value, const VuiTextBoxStyle styles[VuiCtrlState_COUNT]) {
	char* string = _vui.input.input_box.string;
	snprintf(string, _vui_input_box_cap, "%u", *value);

	VuiBool has_changed = vui_text_box_(sib_id, string, _vui_input_box_cap, styles, _VuiInputBoxType_u32);

	VuiCtrl* text_box = vui_ctrl_get(_vui.build.sibling_prev_ctrl_id);
	if (vui_ctrl_is_focused(text_box->id)) {
		char* string = _vui.input.input_box.edit_string;
		*value = strtoul(string, NULL, 10);
	}
	return has_changed;
}

VuiBool vui_input_box_sint(VuiCtrlSibId sib_id, int32_t* value, const VuiTextBoxStyle styles[VuiCtrlState_COUNT]) {
	char* string = _vui.input.input_box.string;
	snprintf(string, _vui_input_box_cap, "%d", *value);

	VuiBool has_changed = vui_text_box_(sib_id, string, _vui_input_box_cap, styles, _VuiInputBoxType_s32);

	VuiCtrl* text_box = vui_ctrl_get(_vui.build.sibling_prev_ctrl_id);
	if (vui_ctrl_is_focused(text_box->id)) {
		char* string = _vui.input.input_box.edit_string;
		*value = strtol(string, NULL, 10);
	}
	return has_changed;
}

VuiBool vui_input_box_float(VuiCtrlSibId sib_id, float* value, const VuiTextBoxStyle styles[VuiCtrlState_COUNT]) {
	char* string = _vui.input.input_box.string;
	snprintf(string, _vui_input_box_cap, "%f", *value);

	VuiBool has_changed = vui_text_box_(sib_id, string, _vui_input_box_cap, styles, _VuiInputBoxType_float);

	VuiCtrl* text_box = vui_ctrl_get(_vui.build.sibling_prev_ctrl_id);
	if (vui_ctrl_is_focused(text_box->id)) {
		char* string = _vui.input.input_box.edit_string;
		*value = strtod(string, NULL);
	}
	return has_changed;
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

	vui_ss.text_header.font_id = setup->default_font_id;
	vui_ss.text_menu.font_id = setup->default_font_id;
	return vui_true;
}

void _vui_find_mouse_focused_ctrls(VuiCtrl* ctrl, VuiBool is_root) {
	VuiRect parent_clip_rect = _vui.render.clip_rect;
	_vui.render.clip_rect = VuiRect_clip(&_vui.render.clip_rect, &ctrl->rect);

	VuiVec2 mouse_pt = VuiVec2_init(_vui.input.mouse.x, _vui.input.mouse.y);
	if (!_vui.input.is_mouse_over_ctrl && !is_root) {
		_vui.input.is_mouse_over_ctrl = VuiRect_intersects_pt(&ctrl->rect, mouse_pt);
	}

	if (VuiRect_intersects_pt(&_vui.render.clip_rect, mouse_pt)) {
		if (ctrl->flags & VuiCtrlFlags_focusable) {
			_vui_ctrl_set_mouse_focused(ctrl->id);
		} else if (ctrl->flags & (VuiCtrlFlags_scrollable_vertical | VuiCtrlFlags_scrollable_horizontal)) {
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

void vui_frame_start(VuiBool right_to_left) {
	vui_assert(_vui.build.w == NULL, "cannot call vui_frame_start until vui_window_end has been called");
	_VuiArenaAlctor_reset(&_vui.frame_data_alctor);

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
		if (actions & VuiInputActions_left) {
			if ((actions & VuiInputActions_select_word_start) == VuiInputActions_select_word_start) {
			} else if ((actions & VuiInputActions_word_start) == VuiInputActions_word_start) {
			} else if ((actions & VuiInputActions_select_left) == VuiInputActions_select_left) {
				if (_vui.input.focused_text_box.cursor_idx + _vui.input.focused_text_box.select_offset) {
					_vui.input.focused_text_box.select_offset -= 1;
				}
			} else {
				if (_vui.input.focused_text_box.cursor_idx) {
					if (_vui.input.focused_text_box.select_offset < 0) {
						_vui.input.focused_text_box.cursor_idx += _vui.input.focused_text_box.select_offset;
					} else if (_vui.input.focused_text_box.select_offset == 0) {
						_vui.input.focused_text_box.cursor_idx -= 1;
					}
				}
				_vui.input.focused_text_box.select_offset = 0;
			}
		} else if (actions & VuiInputActions_right) {
			if ((actions & VuiInputActions_select_word_end) == VuiInputActions_select_word_end) {
			} else if ((actions & VuiInputActions_word_end) == VuiInputActions_word_end) {
			} else if ((actions & VuiInputActions_select_right) == VuiInputActions_select_right) {
				if (_vui.input.focused_text_box.cursor_idx + _vui.input.focused_text_box.select_offset < str_len) {
					_vui.input.focused_text_box.select_offset += 1;
				}

			} else {
				uint32_t end_idx = _vui.input.focused_text_box.cursor_idx + _vui.input.focused_text_box.select_offset;
				if (end_idx <= str_len && _vui.input.focused_text_box.select_offset > 0) {
					_vui.input.focused_text_box.cursor_idx += _vui.input.focused_text_box.select_offset;
				} else if (end_idx < str_len && _vui.input.focused_text_box.select_offset == 0) {
					_vui.input.focused_text_box.cursor_idx += 1;
				}

				_vui.input.focused_text_box.select_offset = 0;
			}
		} else if (actions & (VuiInputActions_backspace | VuiInputActions_delete)) {
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
					end_idx = str_len;
				} else if (_vui.input.focused_text_box.select_offset == 0 && end_idx < str_len) {
					// nothing is selected so add one.
					end_idx += 1;
				}
			} else {
				if ((actions & VuiInputActions_delete_to_start_of_word) == VuiInputActions_delete_to_start_of_word) {
					start_idx = 0;
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
		} else if (actions & VuiInputActions_home) {
			if ((actions & VuiInputActions_select_to_document_home) == VuiInputActions_select_to_document_home) {
				_vui.input.focused_text_box.select_offset = -(int32_t)_vui.input.focused_text_box.cursor_idx;
			} else if ((actions & VuiInputActions_document_home) == VuiInputActions_document_home) {
				_vui.input.focused_text_box.cursor_idx = 0;
				_vui.input.focused_text_box.select_offset = 0;
			} else if ((actions & VuiInputActions_select_home) == VuiInputActions_select_home) {
				uint32_t idx = _vui.input.focused_text_box.cursor_idx + _vui.input.focused_text_box.select_offset;
				while (idx) {
					char c = _vui.input.focused_text_box.string[idx - 1];
					if (c == '\n' || c == '\r') break;
					idx -= 1;
				}
				_vui.input.focused_text_box.select_offset = idx - _vui.input.focused_text_box.cursor_idx;
			} else {
				while (_vui.input.focused_text_box.cursor_idx) {
					char c = _vui.input.focused_text_box.string[_vui.input.focused_text_box.cursor_idx - 1];
					if (c == '\n' || c == '\r') break;
					_vui.input.focused_text_box.cursor_idx -= 1;
				}
				_vui.input.focused_text_box.select_offset = 0;
			}
		} else if (actions & VuiInputActions_end) {
			if ((actions & VuiInputActions_select_to_document_end) == VuiInputActions_select_to_document_end) {
				_vui.input.focused_text_box.select_offset = str_len - _vui.input.focused_text_box.cursor_idx;
			} else if ((actions & VuiInputActions_document_end) == VuiInputActions_document_end) {
				_vui.input.focused_text_box.cursor_idx = str_len;
				_vui.input.focused_text_box.select_offset = 0;
			} else if ((actions & VuiInputActions_select_end) == VuiInputActions_select_end) {
				uint32_t idx = _vui.input.focused_text_box.cursor_idx + _vui.input.focused_text_box.select_offset;
				while (idx < str_len) {
					char c = _vui.input.focused_text_box.string[idx];
					if (c == '\n' || c == '\r') break;
					idx += 1;
				}
				_vui.input.focused_text_box.select_offset = idx - _vui.input.focused_text_box.cursor_idx;
			} else {
				while (_vui.input.focused_text_box.cursor_idx < str_len) {
					char c = _vui.input.focused_text_box.string[_vui.input.focused_text_box.cursor_idx];
					if (c == '\n' || c == '\r') break;
					_vui.input.focused_text_box.cursor_idx += 1;
				}
				_vui.input.focused_text_box.select_offset = 0;
			}
		}
	}

	_vui.build.w = NULL;
}

void vui_frame_end() {
	vui_assert(_vui.build.w == NULL, "cannot call vui_frame_end until vui_window_end has been called");

	if ((_vui.input.actions & VuiInputActions_focus_prev) == VuiInputActions_focus_prev) {
		_VuiWindow* w = &_vui.windows[_vui.focused_window_id];
		VuiCtrl* ctrl = vui_ctrl_get(w->focused_ctrl_id);

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
	} else if ((_vui.input.actions & VuiInputActions_focus_next) == VuiInputActions_focus_next) {
		_VuiWindow* w = &_vui.windows[_vui.focused_window_id];
		VuiCtrl* ctrl = vui_ctrl_get(w->focused_ctrl_id);
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

	_vui.input.mouse.offset_x = 0;
	_vui.input.mouse.offset_y = 0;
	_vui.input.mouse.wheel_offset_x = 0;
	_vui.input.mouse.wheel_offset_y = 0;
	_vui.input.mouse.buttons_has_been_pressed = 0;
	_vui.input.mouse.buttons_has_been_released = 0;
	_vui.input.actions = 0;
	_vui.input.focused_text_box.has_changed = vui_false;
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
	uint16_t dir_size_offset = 0;
	float (*dir_rect_len)(const VuiRect*) = NULL;
	float (*wrap_dir_rect_len)(const VuiRect*) = NULL;
	float inner_dir_offset = 0.f;
	float inner_wrap_dir_offset = 0.f;
	float inner_dir_len = 0.f;
	float inner_wrap_dir_len = 0.f;
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
			if (*(float*)vui_ptr_add(&child->attributes, dir_size_offset) == vui_auto_len) {
				_vui_layout_ctrls(child, child_placement_area_ptr, inner_dir_len, inner_wrap_dir_len);
				total_auto_dir_lens += dir_rect_len(&child->rect);
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
				VuiThickness margin = VuiCtrl_interp_margin(child);
				available_dir_len -= child_dir_len + margin_dir_len(&margin);
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

	float layout_spacing = ctrl->attributes.layout_spacing;
	while (child) {
		VuiCtrl* child_line_start = child;
		*fill_portion_wrap_dir_len_ptr = 0.f;
		float max_wrap_dir_len = 0.0;
		if (wrap || inner_wrap_dir_len == vui_auto_len) {
			//
			// because we are wrapping or our wrap directional length is automatic.
			// loop until we have reached the end of the line and find the tallest control.
			float end_dir_coord = 0.f;
			VuiBool is_first = vui_true;
			*child_placement_area_ptr = VuiRect_init_wh(dir_start, wrap_dir_start, 0, 0);
			for (; child; child = child->sibling_next_id ? vui_ctrl_get(child->sibling_next_id) : NULL) {
				_vui_layout_ctrls(child, child_placement_area_ptr, layout_inner_width, layout_inner_height);

				// advance the length along the direction of the layout
				end_dir_coord += dir_rect_len(&child->rect);

				//
				// wrap the control back around if it exceeds the wrap length.
				if (wrap && !is_first && inner_dir_len < end_dir_coord) {
					break;
				}
				is_first = vui_false;

				end_dir_coord += layout_spacing;

				// see if the height for this control is the tallest
				float child_wrap_dir_len = wrap_dir_rect_len(&child->rect);
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

		for (VuiCtrl* line_child = child_line_start; line_child != child; line_child = line_child->sibling_next_id ? vui_ctrl_get(line_child->sibling_next_id) : NULL) {
			// so the control can just position itself within it.
			*child_placement_area_dir_len_end = *child_placement_area_dir_len_start + dir_rect_len(&line_child->rect);
			_vui_layout_ctrls(line_child, child_placement_area_ptr, line_inner_width, line_inner_height);

			//
			// advance to the next cell, recalculate the end as it can be different this time.
			*child_placement_area_dir_len_start = *child_placement_area_dir_len_start + dir_rect_len(&line_child->rect) + layout_spacing;
		}

		// advance to the new line
		wrap_dir_start += max_wrap_dir_len;
		if (wrap) {
			wrap_dir_start += wrap_spacing;
		}

		// track the max bottom right for auto sized layouts
		if (*child_placement_area_dir_len_end > *max_dir_inner_len_ptr) {
			*max_dir_inner_len_ptr = *child_placement_area_dir_len_end;
		}
		*max_wrap_dir_inner_len_ptr = wrap_dir_start;
	}

	// remove the trailing wrap spacing
	if (wrap) *max_wrap_dir_inner_len_ptr -= wrap_spacing;
}

void _vui_layout_ctrls(VuiCtrl* ctrl, VuiRect* placement_area, float parent_inner_width, float parent_inner_height) {
	if (ctrl->last_frame_idx != _vui.build.frame_idx) {
		ctrl->rect = VuiRect_zero;
		_vui_ctrl_unlink(ctrl);
		_vui_ctrl_dealloc(ctrl->id);
		return;
	}

	VuiCtrlStyle style = {0};
	if (ctrl->style) {
		if (ctrl->target_style) {
			VuiCtrlStyle_interp(&style, ctrl->target_style, ctrl->style, ctrl->interp_ratio);
		} else {
			style = *ctrl->style;
		}
	}

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
		? style.margin.right + style.padding.right + style.border_width
		: style.margin.left + style.padding.left + style.border_width;
	float inner_y = style.margin.top + style.padding.top + style.border_width;
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
				outer_width = parent_fill_portion_width;
			} else if (width < 0) { // is ratio
				float ratio = -width;
				outer_width = parent_inner_width * ratio;
			} else {
				outer_width = width + (style.margin.left + style.margin.right);
			}

			if (_vui.flags & _VuiFlags_right_to_left) {
				ctrl->rect.left = ctrl->rect.right + outer_width;
			} else {
				ctrl->rect.right = ctrl->rect.left + outer_width;
			}
			inner_width = outer_width - (style.margin.left + style.margin.right) - (style.padding.left + style.padding.right) - style.border_width * 2;
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
				outer_height = parent_fill_portion_height;
			} else if (height < 0) { // is ratio
				float ratio = -height;
				outer_height = parent_inner_height * ratio;
			} else {
				outer_height = height + (style.margin.top + style.margin.bottom);
			}

			ctrl->rect.bottom = ctrl->rect.top + outer_height;
			inner_height = outer_height - (style.margin.top + style.margin.bottom) - (style.padding.top + style.padding.bottom) - style.border_width * 2;
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
					child_placement_area = VuiRect_init(inner_x, inner_y, inner_x, inner_y);
					_vui_layout_ctrls(child, &child_placement_area, vui_auto_len, vui_auto_len);

					float width = (_vui.flags & _VuiFlags_right_to_left ? VuiRect_neg_width : VuiRect_width)(&child->rect);
					if (width > max_width) max_width = width;

					float height = VuiRect_height(&child->rect);
					if (height > max_height) max_height = height;
				}

				//
				// resolve the dimensions with automatic lengths
				//
				if (inner_width == vui_auto_len) {
					float outer_width = max_width + (style.padding.left + style.padding.right) + (style.border_width * 2) + (style.margin.left + style.margin.right);
					if (_vui.flags & _VuiFlags_right_to_left) {
						ctrl->rect.left = ctrl->rect.right + outer_width;
					} else {
						ctrl->rect.right = ctrl->rect.left + outer_width;
					}
					inner_width = max_width;
				}
				if (inner_height == vui_auto_len) {
					ctrl->rect.bottom = ctrl->rect.top + max_height + (style.padding.top + style.padding.bottom) + (style.border_width * 2) + (style.margin.top + style.margin.bottom);
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
				if (_vui.flags & _VuiFlags_right_to_left) {
					child_placement_area = VuiRect_init(inner_x + inner_width, inner_y, inner_x, inner_y + inner_height);
				} else {
					child_placement_area = VuiRect_init(inner_x, inner_y, inner_x + inner_width, inner_y + inner_height);
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
			ctrl->rect.left = ctrl->rect.right + max_inner_right_bottom.x + style.padding.left + style.border_width + style.margin.left;
		} else {
			ctrl->rect.right = ctrl->rect.left + max_inner_right_bottom.x + style.padding.right + style.border_width + style.margin.right;
		}
	}
	if (inner_height == vui_auto_len) {
		ctrl->rect.bottom = ctrl->rect.top + max_inner_right_bottom.y + style.padding.bottom + style.border_width + style.margin.bottom;
	}

	//
	// if the scroll view has been initialized with vui_fill_len or vui_auto_len.
	// then store the calculated size in ctrl->scroll_view_size.
	if (ctrl->flags & VuiCtrlFlags_scrollable_horizontal | VuiCtrlFlags_scrollable_vertical) {
		if (ctrl->attributes.width == vui_fill_len || ctrl->attributes.width == vui_auto_len) {
			ctrl->scroll_view_size.x = VuiRect_width(&ctrl->rect) - VuiThickness_horizontal(&style.margin);
			if (vui_ctrl_is_mouse_scroll_focused(ctrl->id) && _vui.build.mouse_scroll_focused_size)
				*_vui.build.mouse_scroll_focused_size = ctrl->scroll_view_size;
		}

		if (ctrl->attributes.height == vui_fill_len || ctrl->attributes.height == vui_auto_len) {
			ctrl->scroll_view_size.y = VuiRect_height(&ctrl->rect) - VuiThickness_vertical(&style.margin);
			if (vui_ctrl_is_mouse_scroll_focused(ctrl->id) && _vui.build.mouse_scroll_focused_size)
				*_vui.build.mouse_scroll_focused_size = ctrl->scroll_view_size;
		}
	}

	//
	// if we actually are being placed somewhere.
	// workout where in the placement_area we go.
	VuiVec2 placement_size = VuiRect_size(*placement_area);
	if (placement_size.x > 0.f && placement_size.y > 0.f) {
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

void _vui_layout_ctrls_finalize(VuiCtrl* ctrl, VuiVec2 offset, float root_width) {
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
	VuiThickness margin = VuiCtrl_interp_margin(ctrl);
	ctrl->rect.left += margin.left;
	ctrl->rect.top += margin.top;
	ctrl->rect.right -= margin.right;
	ctrl->rect.bottom -= margin.bottom;

	VuiCtrl* child = NULL;
	for (VuiCtrlId child_id = ctrl->child_first_id; child_id; child_id = child->sibling_next_id) {
		child = vui_ctrl_get(child_id);
		_vui_layout_ctrls_finalize(child, offset, root_width);
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

void vui_window_end() {
	vui_assert(_vui.build.w != NULL, "cannot call vui_window_end until vui_window_start has been called");

	VuiCtrl* root = vui_ctrl_get(_vui.build.parent_ctrl_id);
	vui_assert(root->parent_id == 0, "cannot end the window without ending all of it's child controls");
	vui_ctrl_end();

	float width = root->attributes.width;
	float height = root->attributes.height;
	VuiRect placement_area = VuiRect_init_wh(0.f, 0.f, 0.f, 0.f);

	_vui_layout_ctrls(root, &placement_area, width, height);
	if (_vui.flags & _VuiFlags_right_to_left) {
		float tmp = root->rect.left;
		root->rect.left = root->rect.right;
		root->rect.right = tmp;
	}

	VuiCtrl* child = NULL;
	for (VuiCtrlId child_id = root->child_first_id; child_id; child_id = child->sibling_next_id) {
		child = vui_ctrl_get(child_id);
		_vui_layout_ctrls_finalize(child, (VuiVec2){0}, root->attributes.width);
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
	_vui.render.clip_rect = VuiRect_clip(&_vui.render.clip_rect, &inner_rect);

	if (ctrl->style) {
		if (ctrl->target_style) {
			ctrl->style_interp_fn(&_vui.style, ctrl->target_style, ctrl->style, ctrl->interp_ratio);
		} else {
			ctrl->style_interp_fn(&_vui.style, ctrl->style, ctrl->style, ctrl->interp_ratio);
		}
	} else {
		memset(_vui.style_buf, 0, sizeof(_vui.style_buf));
	}

	float border_width_half = _vui.style.border_width / 2.0;
	if (_vui.style.bg_color.a) {
		if (_vui.style.border_width) {
			inner_rect.left += border_width_half;
			inner_rect.top += border_width_half;
			inner_rect.bottom -= border_width_half;
			inner_rect.right -= border_width_half;
		}
		vui_render_rect(&inner_rect, _vui.style.bg_color, _vui.style.radius);
		if (_vui.style.border_width) {
			inner_rect.left -= border_width_half;
			inner_rect.top -= border_width_half;
			inner_rect.bottom += border_width_half;
			inner_rect.right += border_width_half;
		}
	}

	if (_vui.style.border_width) {
		vui_render_rect_border(&ctrl->rect, _vui.style.border_color, _vui.style.radius, _vui.style.border_width);

		inner_rect.left += _vui.style.border_width;
		inner_rect.top += _vui.style.border_width;
		inner_rect.bottom -= _vui.style.border_width;
		inner_rect.right -= _vui.style.border_width;
		_vui.render.clip_rect = VuiRect_clip(&_vui.render.clip_rect, &inner_rect);
	}

	inner_rect.left += _vui.style.padding.left;
	inner_rect.top += _vui.style.padding.top;
	inner_rect.bottom -= _vui.style.padding.bottom;
	inner_rect.right -= _vui.style.padding.right;
	_vui.render.clip_rect = VuiRect_clip(&_vui.render.clip_rect, &inner_rect);

	if (ctrl->render_fn) {
		ctrl->render_fn(ctrl, &_vui.style, &inner_rect);
	}

	//
	// now render the children;
	//
	VuiCtrl* child = NULL;
	for (VuiCtrlId child_id = ctrl->child_first_id; child_id; child_id = child->sibling_next_id) {
		child = vui_ctrl_get(child_id);
		_vui_render_ctrls(child);
	}

	_vui.render.clip_rect = parent_clip_rect;
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

		//
		// flattern all these layers into one big array.
		VuiRenderCmd* cmds = VuiStk_push_many(&w->render.cmds, VuiStk_count(layer->cmds));
		vui_ensure_alloc_ok(cmds, NULL);
		memcpy(cmds, layer->cmds, VuiStk_count(layer->cmds) * sizeof(VuiRenderCmd));

		VuiVertex* verts = VuiStk_push_many(&w->render.verts, VuiStk_count(layer->verts));
		vui_ensure_alloc_ok(verts, NULL);
		memcpy(verts, layer->verts, VuiStk_count(layer->verts) * sizeof(VuiVertex));

		VuiVertexIdx* indices = VuiStk_push_many(&w->render.indices, VuiStk_count(layer->indices));
		vui_ensure_alloc_ok(indices, NULL);
		memcpy(indices, layer->indices, VuiStk_count(layer->indices) * sizeof(VuiVertexIdx));
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

