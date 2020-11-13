
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

void _VuiPool_expand(_VuiPool* pool, uint32_t new_cap, uintptr_t elmt_size, uintptr_t elmt_align) {
	vui_assert(new_cap >= pool->cap, "vui pool can only expand");

	//
	// expand the pool, this is fine since we store id's everywhere.
	//
	uint32_t bitset_size = (pool->cap / 8) + 1;
	uintptr_t cap_bytes = ((uintptr_t)pool->cap * elmt_size) + pool->elmts_start_byte_idx;

	uint32_t new_bitset_size = (new_cap / 8) + 1;
	uint32_t new_elmts_start_byte_idx = (uintptr_t)vui_ptr_round_up_align((void*)(uintptr_t)new_bitset_size, elmt_align);
	uintptr_t new_cap_bytes = ((uintptr_t)new_cap * elmt_size) +
		new_elmts_start_byte_idx;

	void* new_data = vui_mem_realloc(_vui.allocator, pool->data, cap_bytes, new_cap_bytes, elmt_align);
	pool->data = new_data;

	void* elmts = vui_ptr_add(new_data, pool->elmts_start_byte_idx);
	void* new_elmts = vui_ptr_add(new_data, new_elmts_start_byte_idx);

	//
	// shift the elements to their new elmts_start_byte_idx.
	memmove(new_elmts, elmts, (uintptr_t)pool->cap * elmt_size);
	// zero the new bits of the is_allocated_bitset.
	memset(elmts, 0, new_elmts_start_byte_idx - pool->elmts_start_byte_idx);
	// then zero all the new elements.
	memset(vui_ptr_add(new_elmts, (uintptr_t)pool->cap * elmt_size), 0,
		((uintptr_t)(new_cap - pool->cap) * elmt_size));

	//
	// setup the free list, by visiting each element and store an
	// index to the next element.
	for (uint32_t i = pool->cap; i < new_cap; i += 1) {
		*(uint32_t*)vui_ptr_add(new_elmts, i * elmt_size) = i + 2;
	}

	pool->free_list_head_id = pool->cap + 1;
	pool->cap = new_cap;
	pool->elmts_start_byte_idx = new_elmts_start_byte_idx;
}

void _VuiPool_reset_and_populate(_VuiPool* pool, void* elmts, uint32_t count, uintptr_t elmt_size, uintptr_t elmt_align) {
	_VuiPool_reset(pool, elmt_size);
	if (pool->cap < count) {
		_VuiPool_expand(pool, vui_max(pool->cap ? pool->cap * 2 : 64, count), elmt_size, elmt_align);
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
		_VuiPool_expand(pool, pool->cap ? pool->cap * 2 : 64, elmt_size, elmt_align);
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

typedef struct VuiStyleChange VuiStyleChange;
struct VuiStyleChange {
	VuiStyleChange* prev;
	VuiStyle* style;
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
	VuiTextBoxFocusChange text_box_focus_change_fn;
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
		VuiStyle* style;
		VuiStyle local_style;
		VuiCtrlAttrChange* ctrl_state_attr_change_list_heads[VuiCtrlState_COUNT][VuiCtrlAttr_COUNT];
		VuiStyleChange* style_change_list_head;
		VuiVec2* mouse_scroll_focused_content_offset;
		VuiVec2* mouse_scroll_focused_size;
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

void _vui_push_ctrl_attr(VuiCtrlState ctrl_state, VuiCtrlAttr attr, VuiCtrlAttrValue value) {
	VuiStyle* local_style = &_vui.build.local_style;
	VuiCtrlAttrValue* style_value = &local_style->state_attr_values[ctrl_state][attr];

	//
	// see if there is already a value in that state's attribute.
	VuiCtrlAttrFlags attr_flag = (VuiCtrlAttrFlags)1 << (VuiCtrlAttrFlags)attr;
	if (local_style->state_attr_flags[ctrl_state] & attr_flag) {
		//
		// a value already exists so allocate a new change to store the old attribute.
		VuiCtrlAttrChange* old_attr_change = vui_frame_data_alloc_elmt(VuiCtrlAttrChange);
		old_attr_change->prev = _vui.build.ctrl_state_attr_change_list_heads[ctrl_state][attr];
		_vui.build.ctrl_state_attr_change_list_heads[ctrl_state][attr] = old_attr_change;

		//
		// store the orignal value in the old_attr_change (history entry)
		old_attr_change->value = *style_value;
	} else {
		// set the flag to say that a value exists for the attribute.
		local_style->state_attr_flags[ctrl_state] |= attr_flag;
	}

	// now overwrite the value with the new one.
	memset(style_value, 0, sizeof(*style_value));
	*style_value = value;
}

void _vui_pop_ctrl_attr(VuiCtrlState ctrl_state, VuiCtrlAttr attr) {
	VuiStyle* local_style = &_vui.build.local_style;
	VuiCtrlAttrValue* style_value = &local_style->state_attr_values[ctrl_state][attr];

	VuiCtrlAttrChange* old_attr_change = _vui.build.ctrl_state_attr_change_list_heads[ctrl_state][attr];
	if (old_attr_change) {
		//
		// restore the orignal value from the old_attr_change (history entry)
		*style_value = old_attr_change->value;
		_vui.build.ctrl_state_attr_change_list_heads[ctrl_state][attr] = old_attr_change->prev;
	} else {
		// we have popped all the attribute values off for this state.
		// so remove the flags from the the state's attribute flags.
		// this will make future calls to _VuiCtrl_style_attr avoid getting
		// this state's attribute value and instead use the global style at _vui.build.style
		local_style->state_attr_flags[ctrl_state] &= ~((VuiCtrlAttrFlags)1 << (VuiCtrlAttrFlags)attr);
		memset(style_value, 0, sizeof(*style_value));
	}
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

		stk = _VuiStk_resize_cap(stk, new_cap, elmt_size);
		if (stk == NULL) return NULL;
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

void* _VuiStk_resize_cap(void* stk, uint32_t new_cap, uint32_t elmt_size) {
	if (new_cap == 0) new_cap = vui_stk_init_cap;
	uint32_t cap = VuiStk_cap(stk);
	if (new_cap == cap) return stk;

	uintptr_t size = VuiStk_size(stk) + sizeof(_VuiStkHeader);
	uintptr_t new_size = (uintptr_t)elmt_size * (uintptr_t)new_cap + sizeof(_VuiStkHeader);
    _VuiStkHeader* ptr = vui_mem_realloc(_vui.allocator, stk ? _VuiStk_header(stk) : NULL, size, new_size, alignof(void*));
	if (ptr == NULL) return NULL;

	ptr->cap = new_cap;

	// if the stk was uninitialized set its size to 0
    if (!stk) ptr->count = 0;

	// return the new stack pointer.
	return (void*)((char*)ptr + sizeof(_VuiStkHeader));
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

void vui_input_add_text(char* string, uint32_t string_length) {
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
		_vui.text_box_focus_change_fn(vui_false);
		_vui.input.focused_text_box.string = NULL;
		_vui.input.focused_text_box.string_len = 0;
		_vui.input.focused_text_box.string_cap = 0;
	}
}

void _vui_ctrl_set_mouse_focused(VuiCtrlId ctrl_id) {
	//
	// remove focus from the currently focused control.
	if (_vui.mouse_focused_ctrl_id) {
		VuiCtrl* focused_ctrl = vui_ctrl_get(_vui.mouse_focused_ctrl_id);
		focused_ctrl->state_flags &= ~VuiCtrlStateFlags_mouse_focused;
	}

	//
	// if a control was supplied, make that the new mouse focused control
	if (ctrl_id) {
		VuiCtrl* ctrl = vui_ctrl_get(ctrl_id);
		ctrl->state_flags |= VuiCtrlStateFlags_mouse_focused;
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
			return VuiFocusState_pressed;
		}
	}

	if ((_vui.input.actions & VuiInputActions_focus_held) == VuiInputActions_focus_held) {
		if (vui_ctrl_is_focused(ctrl_id)) {
			return VuiFocusState_held;
		}
	}

	if ((_vui.input.actions & VuiInputActions_focus_released) == VuiInputActions_focus_released) {
		if (vui_ctrl_is_focused(ctrl_id)) {
			return VuiFocusState_released;
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
				return VuiFocusState_released;
			}
            return VuiFocusState_pressed;
        } else {
            if (vui_ctrl_is_focused(ctrl_id)) {
                vui_ctrl_set_focused(0);
            }
        }
    }

    if ((_vui.input.mouse.buttons_is_pressed & VuiMouseButtons_left) == VuiMouseButtons_left) {
        if (vui_ctrl_is_focused(ctrl_id)) {
            return VuiFocusState_held;
        }
    }

    if ((_vui.input.mouse.buttons_has_been_released & VuiMouseButtons_left) == VuiMouseButtons_left) {
        if (vui_ctrl_is_mouse_focused(ctrl_id)) {
            return VuiFocusState_released;
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

void _VuiStyle_set_attr(VuiStyle* style, VuiCtrlAttr attr, VuiCtrlState ctrl_state, VuiCtrlAttrValue value) {
	style->state_attr_flags[ctrl_state] |= (VuiCtrlAttrFlags)1 << (VuiCtrlAttrFlags)attr;
	VuiCtrlAttrValue* style_value = &style->state_attr_values[ctrl_state][attr];
	memset(style_value, 0, sizeof(*style_value));
	*style_value = value;
}

void _VuiStyle_unset_attr(VuiStyle* style, VuiCtrlAttr attr, VuiCtrlState ctrl_state) {
	style->state_attr_flags[ctrl_state] &= ~((VuiCtrlAttrFlags)1 << (VuiCtrlAttrFlags)attr);
	VuiCtrlAttrValue* style_value = &style->state_attr_values[ctrl_state][attr];
	memset(style_value, 0, sizeof(*style_value));
}

void vui_push_style(VuiStyle* style) {
	VuiStyleChange* c = vui_frame_data_alloc_elmt(VuiStyleChange);
	c->prev = _vui.build.style_change_list_head;
	c->style = _vui.build.style;
	_vui.build.style = style;
}

void vui_pop_style() {
	VuiStyleChange* c = _vui.build.style_change_list_head;
	_vui.build.style = c->style;
	_vui.build.style_change_list_head = c->prev;
}

const VuiCtrlAttrValue* VuiStyle_attr(VuiStyle* style, VuiCtrlAttr attr, VuiCtrlStateFlags ctrl_state_flags) {
	//
	// look through each state from important to least important.
	// if the control has that state and the attribute is enabled, then return a pointer to that attribute.
	VuiCtrlStateFlags state_flags = ctrl_state_flags | VuiCtrlState_default;
	VuiCtrlAttrFlags attr_flag = (VuiCtrlAttrFlags)1 << (VuiCtrlAttrFlags)attr;
	for (VuiCtrlState state = 0; state < VuiCtrlState_COUNT; state += 1) {
		if ((state_flags & state) && style->state_attr_flags[state] & attr_flag) {
			return &style->state_attr_values[state][attr];
		}
	}

	return NULL;
}

const VuiCtrlAttrValue* _VuiCtrl_style_attr(VuiCtrl* ctrl, VuiCtrlAttr attr) {
	//
	// look in the local style first, return if we find an attribute with this control state
	const VuiCtrlAttrValue* v = VuiStyle_attr(&_vui.build.local_style, attr, ctrl->state_flags);
	if (v) return v;

	//
	// now look in the global style, return if we find an attribute with this control state
	v = VuiStyle_attr(_vui.build.style, attr, ctrl->state_flags);
	if (v) return v;

	// no attributes found, so return a zero value
	static VuiCtrlAttrValue zero_value = {0};
	return &zero_value;
}

void VuiStyle_init_default(VuiStyle* style) {
	memset(style, 0, sizeof(VuiStyle));
	VuiStyle_set_width(style, VuiCtrlState_default, vui_auto_len);
	VuiStyle_set_height(style, VuiCtrlState_default, vui_auto_len);
	VuiStyle_set_bg_color(style, VuiCtrlState_default, vui_color_emerald);
	VuiStyle_set_border_color(style, VuiCtrlState_default, vui_color_orange);
	VuiStyle_set_border_width(style, VuiCtrlState_default, 4.f);
	VuiStyle_set_bg_color(style, VuiCtrlState_mouse_focused, vui_color_nephritis);
	VuiStyle_set_border_color(style, VuiCtrlState_mouse_focused, vui_color_pumpkin);
	VuiStyle_set_border_width(style, VuiCtrlState_mouse_focused, 4.f);
	VuiStyle_set_text_color(style, VuiCtrlState_default, vui_color_white);
	VuiStyle_set_check_color(style, VuiCtrlState_default, vui_color_wisteria);
	VuiStyle_set_check_size(style, VuiCtrlState_default, 24.f);
	VuiStyle_set_text_height(style, VuiCtrlState_default, 32.f);
	VuiStyle_set_text_cursor_width(style, VuiCtrlState_default, 4.f);
	VuiStyle_set_text_cursor_color(style, VuiCtrlState_default, vui_color_wisteria);
	VuiStyle_set_text_selection_color(style, VuiCtrlState_default, VuiColor_init(0x34, 0x98, 0xdb, 0x80));

    VuiStyle_set_scroll_bar_width(style, VuiCtrlState_default, 24.f);
    VuiStyle_set_scroll_bar_border_width(style, VuiCtrlState_default, 4.f);
    VuiStyle_set_scroll_bar_border_color(style, VuiCtrlState_default, vui_color_dark_gray);
    VuiStyle_set_scroll_bar_bg_color(style, VuiCtrlState_default, vui_color_black);
    VuiStyle_set_scroll_bar_radius(style, VuiCtrlState_default, 4.f);
    VuiStyle_set_scroll_bar_margin(style, VuiCtrlState_default, VuiThickness_init_even(2.f));
    VuiStyle_set_scroll_bar_slider_border_width(style, VuiCtrlState_default, 4.f);
    VuiStyle_set_scroll_bar_slider_border_color(style, VuiCtrlState_default, vui_color_alizarin);
    VuiStyle_set_scroll_bar_slider_bg_color(style, VuiCtrlState_default, vui_color_orange);

	VuiStyle_set_progress_bar_color(style, VuiCtrlState_default, vui_color_alizarin);
}

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

void vui_render_text(VuiVec2 left_top, VuiFontId font_id, float text_height, char* text, uint32_t text_length, VuiColor color, float wrap_at_width) {
	if (text_length) {
		_vui_render_glyph_color = color;
		_vui.position_text_fn(_vui.position_text_userdata, font_id, text_height, text, text_length, left_top, _vui_render_glyph);
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
		vert->pos = VuiRect_clip_pt(&clip_rect, points[idx]);
		vert->uv = VuiVec2_zero;
		vert->color = color;
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

	w.verts[0] = (VuiVertex){
		.pos = clipped_rect.left_top,
		.uv = uv_rect.left_top,
		.color = color,
	};

	w.verts[1] = (VuiVertex){
		.pos = VuiVec2_init(clipped_rect.right_bottom.x, clipped_rect.left_top.y),
		.uv = VuiVec2_init(uv_rect.right_bottom.x, uv_rect.left_top.y),
		.color = color,
	};

	w.verts[2] = (VuiVertex){
		.pos = clipped_rect.right_bottom,
		.uv = uv_rect.right_bottom,
		.color = color,
	};

	w.verts[3] = (VuiVertex){
		.pos = VuiVec2_init(clipped_rect.left_top.x, clipped_rect.right_bottom.y),
		.uv = VuiVec2_init(uv_rect.left_top.x, uv_rect.right_bottom.y),
		.color = color,
	};

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
		}
		sib_prev->sibling_next_id = ctrl->id;
	} else {
		VuiCtrl* parent = vui_ctrl_get(_vui.build.parent_ctrl_id);
		parent->child_first_id = ctrl->id;
		parent->child_last_id = ctrl->id;
	}
	ctrl->parent_id = _vui.build.parent_ctrl_id;
}

void vui_ctrl_start(VuiCtrlSibId sib_id, VuiCtrlFlags flags, VuiActiveChange active_change) {
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
			if (focus_state == VuiFocusState_pressed || focus_state == VuiFocusState_double_pressed || focus_state == VuiFocusState_held) {
				ctrl->state_flags |= VuiCtrlStateFlags_active;
			} else {
				ctrl->state_flags &= ~VuiCtrlStateFlags_active;
			}
		} else if (flags & VuiCtrlFlags_selectable) {
			if (focus_state == VuiFocusState_pressed) {
				ctrl->state_flags |= VuiCtrlStateFlags_active;
			}
		} else if (flags & VuiCtrlFlags_toggleable) {
			if (focus_state == VuiFocusState_pressed) {
				ctrl->state_flags ^= VuiCtrlStateFlags_active;
			}
		}
	}

	//
	// copy over the attributes.
	//
	for (VuiCtrlAttr attr = 0; attr < VuiCtrlAttr_COUNT; attr += 1) {
		ctrl->attributes[attr] = *_VuiCtrl_style_attr(ctrl, attr);
	}

	_vui.build.parent_ctrl_id = ctrl->id;
	_vui.build.sibling_prev_ctrl_id = 0;
}

void vui_ctrl_end() {
	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);
	_vui.build.parent_ctrl_id = ctrl->parent_id;
	_vui.build.sibling_prev_ctrl_id = ctrl->id;
}

void vui_box_start(VuiCtrlSibId sib_id) {
	vui_ctrl_start(sib_id, VuiCtrlFlags_background | VuiCtrlFlags_border, 0);
}

void vui_box_end() {
	vui_ctrl_end();
}

void vui_box(VuiCtrlSibId sib_id) {
	vui_box_start(sib_id);
	vui_box_end();
}

VuiVec2 vui_get_text_size(char* text, uint32_t text_length, float wrap_at_width, VuiFontId font_id, float text_height) {
	return _vui.position_text_fn(_vui.position_text_userdata, font_id, text_height, text, text_length, VuiVec2_zero, NULL);
}

void vui_text_(VuiCtrlSibId sib_id, char* text, uint32_t text_length, float wrap_at_width) {
	uint32_t text_start_idx = VuiStk_count(_vui.build.w->text);
	if (text_length) {
		char* t = VuiStk_push_many(&_vui.build.w->text, text_length);
		vui_ensure_alloc_ok(t);
		memcpy(t, text, text_length);
	}

	VuiCtrl* parent = vui_ctrl_get(_vui.build.parent_ctrl_id);

	float text_height = parent->attributes[VuiCtrlAttr_text_height].float_;
	VuiVec2 size = vui_get_text_size(text, text_length, wrap_at_width,
		parent->attributes[VuiCtrlAttr_text_font_id].font_id, text_height);
	if (size.y == 0.f) {
		size.y = text_height;
	}

	vui_scope_padding(VuiCtrlState_default, VuiThickness_zero)
	vui_scope_width(VuiCtrlState_default, size.x)
	vui_scope_height(VuiCtrlState_default, size.y) {
		vui_ctrl_start(sib_id, _VuiCtrlFlags_text, 0);
		VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);
		ctrl->text_start_idx = text_start_idx;
		ctrl->text_length = text_length;
		ctrl->text_wrap_width = wrap_at_width;
		vui_ctrl_end();
	}
}

void vui_image(VuiCtrlSibId sib_id, VuiImageId image_id, VuiColor image_tint) {
	vui_scope_padding(VuiCtrlState_default, VuiThickness_zero) {
		vui_ctrl_start(sib_id, _VuiCtrlFlags_image, 0);
		VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);
		VuiImage* image = vui_image_get(image_id);
		if (ctrl->attributes[VuiCtrlAttr_width].float_ == vui_auto_len) {
			ctrl->attributes[VuiCtrlAttr_width].float_ = image->width;
		}

		if (ctrl->attributes[VuiCtrlAttr_height].float_ == vui_auto_len) {
			ctrl->attributes[VuiCtrlAttr_height].float_ = image->height;
		}

		ctrl->image_id = image_id;
		ctrl->image_tint = image_tint;
		vui_ctrl_end();
	}
}

void vui_spacing(VuiCtrlSibId sib_id, float width, float height) {
	vui_scope_width(VuiCtrlState_default, width)
	vui_scope_height(VuiCtrlState_default, width) {
		vui_ctrl_start(sib_id, 0, 0);
		vui_ctrl_end();
	}
}

void vui_separator(VuiCtrlSibId sib_id) {
	VuiCtrl* parent = vui_ctrl_get(_vui.build.parent_ctrl_id);
	VuiBool is_row = parent->layout_type == VuiLayoutType_row;
	vui_assert(is_row || parent->layout_type == VuiLayoutType_column, "vui_separator can only be used in a non-wrapping row or column layout");
	vui_assert(!parent->attributes[VuiCtrlAttr_layout_wrap].bool_, "vui_separator can only be used in a non-wrapping row or column layout");

	float size = parent->attributes[VuiCtrlAttr_separator_size].float_;
	float width = is_row ? vui_fill_len : size;
	float height = is_row ? size : vui_fill_len;

	vui_scope_width(VuiCtrlState_default, width)
	vui_scope_height(VuiCtrlState_default, height) {
		vui_ctrl_start(sib_id, 0, 0);
		vui_ctrl_end();
	}
}

VuiFocusState vui_button_start(VuiCtrlSibId sib_id) {
	VuiCtrlFlags flags =
		VuiCtrlFlags_background | VuiCtrlFlags_border | VuiCtrlFlags_focusable | VuiCtrlFlags_pressable;
	vui_ctrl_start(sib_id, flags, 0);

	return vui_ctrl_get(_vui.build.parent_ctrl_id)->focus_state;
}

void vui_button_end() {
	vui_ctrl_end();
}

VuiFocusState vui_button(VuiCtrlSibId sib_id) {
	VuiFocusState state = vui_button_start(sib_id);
	vui_button_end();
	return state;
}

VuiFocusState vui_text_button_(VuiCtrlSibId sib_id, char* text, uint32_t text_length) {
	VuiFocusState state = vui_button_start(sib_id);
	vui_scope_offset(VuiCtrlState_default, 0.f, 0.f)
	vui_scope_align(VuiCtrlState_default, VuiAlign_center) {
		vui_text_(vui_sib_id, text, text_length, 0.f);
	}
	vui_button_end();
	return state;
}

VuiFocusState vui_image_button(VuiCtrlSibId sib_id, VuiImageId image_id, VuiColor image_tint) {
	VuiFocusState state = vui_button_start(sib_id);
	vui_scope_offset(VuiCtrlState_default, 0.f, 0.f)
	vui_scope_align(VuiCtrlState_default, VuiAlign_center) {
		vui_image(vui_sib_id, image_id, image_tint);
	}
	vui_button_end();
	return state;
}

VuiFocusState vui_image_text_button_(VuiCtrlSibId sib_id, VuiImageId image_id, VuiColor image_tint, char* text, uint32_t text_length) {
	VuiFocusState state = vui_button_start(sib_id);
	vui_column_layout();

	vui_scope_offset(VuiCtrlState_default, 0.f, 0.f)
	vui_scope_align(VuiCtrlState_default, VuiAlign_center) {
		vui_image(vui_sib_id, image_id, image_tint);
		vui_text_(vui_sib_id, text, text_length, 0.f);
	}

	vui_button_end();
	return state;
}

VuiBool vui_toggle_button_start(VuiCtrlSibId sib_id, VuiBool* pressed) {
	VuiCtrlFlags flags =
		VuiCtrlFlags_background | VuiCtrlFlags_border | VuiCtrlFlags_focusable | VuiCtrlFlags_toggleable;
	vui_ctrl_start(sib_id, flags, pressed ? *pressed + 1 : 0);

	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);
	VuiBool is_active = (ctrl->state_flags & VuiCtrlStateFlags_active) == VuiCtrlStateFlags_active;
	if (pressed) *pressed = is_active;
	return is_active;
}

void vui_toggle_button_end() {
	vui_ctrl_end();
}

VuiBool vui_text_toggle_button_(VuiCtrlSibId sib_id, VuiBool* pressed, char* text, uint32_t text_length) {
	VuiBool p = vui_toggle_button_start(sib_id, pressed);
	vui_scope_offset(VuiCtrlState_default, 0.f, 0.f)
	vui_scope_align(VuiCtrlState_default, VuiAlign_center) {
		vui_text_(vui_sib_id, text, text_length, 0.f);
	}
	vui_toggle_button_end();
	return p;
}

VuiBool vui_image_toggle_button(VuiCtrlSibId sib_id, VuiBool* pressed, VuiImageId image_id, VuiColor image_tint) {
	VuiBool p = vui_toggle_button_start(sib_id, pressed);
	vui_scope_offset(VuiCtrlState_default, 0.f, 0.f)
	vui_scope_align(VuiCtrlState_default, VuiAlign_center) {
		vui_image(vui_sib_id, image_id, image_tint);
	}
	vui_toggle_button_end();
	return p;
}

VuiBool vui_image_text_toggle_button_(VuiCtrlSibId sib_id, VuiBool* pressed, VuiImageId image_id, VuiColor image_tint, char* text, uint32_t text_length) {
	VuiBool p = vui_toggle_button_start(sib_id, pressed);
	vui_column_layout();
	vui_scope_offset(VuiCtrlState_default, 0.f, 0.f)
	vui_scope_align(VuiCtrlState_default, VuiAlign_center) {
		vui_image(vui_sib_id, image_id, image_tint);
		vui_text_(vui_sib_id, text, text_length, 0.f);
	}
	vui_toggle_button_end();
	return p;
}

VuiBool vui_select_button_start(VuiCtrlSibId sib_id, VuiCtrlSibId* selected_sib_id) {
	VuiCtrlFlags flags =
		VuiCtrlFlags_background | VuiCtrlFlags_border | VuiCtrlFlags_focusable | VuiCtrlFlags_selectable;
	vui_ctrl_start(sib_id, flags, (*selected_sib_id == sib_id) + 1);

	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);
	VuiBool is_active = (ctrl->state_flags & VuiCtrlStateFlags_active) == VuiCtrlStateFlags_active;
	if (is_active) *selected_sib_id = sib_id;
	return is_active;
}

void vui_select_button_end() {
	vui_button_end();
}

VuiBool vui_text_select_button_(VuiCtrlSibId sib_id, VuiCtrlSibId* selected_sib_id, char* text, uint32_t text_length) {
	VuiBool p = vui_select_button_start(sib_id, selected_sib_id);
	vui_scope_offset(VuiCtrlState_default, 0.f, 0.f)
	vui_scope_align(VuiCtrlState_default, VuiAlign_center) {
		vui_text_(vui_sib_id, text, text_length, 0.f);
	}
	vui_select_button_end();
	return p;
}

VuiBool vui_image_select_button(VuiCtrlSibId sib_id, VuiCtrlSibId* selected_sib_id, VuiImageId image_id, VuiColor image_tint) {
	VuiBool p = vui_select_button_start(sib_id, selected_sib_id);
	vui_scope_offset(VuiCtrlState_default, 0.f, 0.f)
	vui_scope_align(VuiCtrlState_default, VuiAlign_center) {
		vui_image(vui_sib_id, image_id, image_tint);
	}
	vui_select_button_end();
	return p;
}

VuiBool vui_image_text_select_button_(VuiCtrlSibId sib_id, VuiCtrlSibId* selected_sib_id, VuiImageId image_id, VuiColor image_tint, char* text, uint32_t text_length) {
	VuiBool p = vui_select_button_start(sib_id, selected_sib_id);
	vui_column_layout();

	vui_scope_offset(VuiCtrlState_default, 0.f, 0.f)
	vui_scope_align(VuiCtrlState_default, VuiAlign_center) {
		vui_image(vui_sib_id, image_id, image_tint);
		vui_text_(vui_sib_id, text, text_length, 0.f);
	}

	vui_select_button_end();
	return p;
}


VuiBool vui_check_box(VuiCtrlSibId sib_id, VuiBool* checked) {
	VuiBool is_active = vui_false;
	vui_scope_width(VuiCtrlState_default, vui_auto_len)
	vui_scope_height(VuiCtrlState_default, vui_auto_len) {
		VuiCtrlFlags flags =
			VuiCtrlFlags_background | VuiCtrlFlags_border | VuiCtrlFlags_focusable | VuiCtrlFlags_toggleable;
		vui_ctrl_start(sib_id, flags, checked ? *checked + 1 : 0);
		VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);
		is_active = (ctrl->state_flags & VuiCtrlStateFlags_active) == VuiCtrlStateFlags_active;
		if (checked) *checked = is_active;

		float check_size = _VuiCtrl_style_attr(ctrl, VuiCtrlAttr_check_size)->float_;
		VuiColor check_color = _VuiCtrl_style_attr(ctrl, VuiCtrlAttr_check_color)->color;
		vui_scope_bg_color(VuiCtrlState_default, check_color)
		vui_scope_margin(VuiCtrlState_default, VuiThickness_zero)
		vui_scope_padding(VuiCtrlState_default, VuiThickness_zero)
		vui_scope_width(VuiCtrlState_default, check_size)
		vui_scope_height(VuiCtrlState_default, check_size) {
			VuiCtrlFlags checked_flags = is_active ? VuiCtrlFlags_background : 0;
			vui_ctrl_start(vui_sib_id, checked_flags, 0);
			vui_ctrl_end();
		}


		vui_ctrl_end();
	}
	return is_active;
}

VuiBool vui_text_check_box_(VuiCtrlSibId sib_id, VuiBool* checked, char* text, uint32_t text_length) {
	vui_ctrl_start(sib_id, VuiCtrlFlags_focusable | VuiCtrlFlags_toggleable, checked ? *checked + 1 : 0);
	vui_column_layout();

	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);
	VuiBool c = (ctrl->state_flags & VuiCtrlStateFlags_active) == VuiCtrlStateFlags_active;
	if (!checked) checked = &c;

	VuiBool state;
	vui_scope_offset(VuiCtrlState_default, 0.f, 0.f)
	vui_scope_align(VuiCtrlState_default, VuiAlign_center) {
		state = vui_check_box(vui_sib_id, checked);
		VuiCtrl* check_box = vui_ctrl_get(_vui.build.sibling_prev_ctrl_id);
		if (check_box->state_flags & VuiCtrlStateFlags_active) {
			ctrl->state_flags |= VuiCtrlStateFlags_active;
		} else {
			ctrl->state_flags &= ~VuiCtrlStateFlags_active;
		}
		vui_text_(vui_sib_id, text, text_length, 0.f);
	}

	vui_ctrl_end();
	return state;
}

VuiBool vui_image_check_box(VuiCtrlSibId sib_id, VuiBool* checked, VuiImageId image_id, VuiColor image_tint) {
	vui_ctrl_start(sib_id, VuiCtrlFlags_focusable | VuiCtrlFlags_toggleable, checked ? *checked + 1 : 0);
	vui_column_layout();

	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);
	VuiBool c = (ctrl->state_flags & VuiCtrlStateFlags_active) == VuiCtrlStateFlags_active;
	if (!checked) checked = &c;

	VuiBool state;
	vui_scope_offset(VuiCtrlState_default, 0.f, 0.f)
	vui_scope_align(VuiCtrlState_default, VuiAlign_center) {
		state = vui_check_box(vui_sib_id, checked);
		VuiCtrl* check_box = vui_ctrl_get(_vui.build.sibling_prev_ctrl_id);
		if (check_box->state_flags & VuiCtrlStateFlags_active) {
			ctrl->state_flags |= VuiCtrlStateFlags_active;
		} else {
			ctrl->state_flags &= ~VuiCtrlStateFlags_active;
		}
		vui_image(vui_sib_id, image_id, image_tint);
	}

	return state;
}

VuiBool vui_radio_button(VuiCtrlSibId sib_id, VuiCtrlSibId* selected_sib_id) {
	VuiBool is_active = vui_false;
	vui_scope_radius(VuiCtrlState_default, 100.f)
	vui_scope_width(VuiCtrlState_default, vui_auto_len)
	vui_scope_height(VuiCtrlState_default, vui_auto_len) {
		VuiCtrlFlags flags =
			VuiCtrlFlags_background | VuiCtrlFlags_border | VuiCtrlFlags_focusable | VuiCtrlFlags_selectable;
		vui_ctrl_start(sib_id, flags, (*selected_sib_id == sib_id) + 1);
		VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);
		is_active = (ctrl->state_flags & VuiCtrlStateFlags_active) == VuiCtrlStateFlags_active;
		if (is_active) *selected_sib_id = sib_id;

		float check_size = _VuiCtrl_style_attr(ctrl, VuiCtrlAttr_check_size)->float_;
		VuiColor check_color = _VuiCtrl_style_attr(ctrl, VuiCtrlAttr_check_color)->color;
		vui_scope_bg_color(VuiCtrlState_default, check_color)
		vui_scope_margin(VuiCtrlState_default, VuiThickness_zero)
		vui_scope_padding(VuiCtrlState_default, VuiThickness_zero)
		vui_scope_width(VuiCtrlState_default, check_size)
		vui_scope_height(VuiCtrlState_default, check_size) {
			VuiCtrlFlags checked_flags = is_active ? VuiCtrlFlags_background : 0;
			vui_ctrl_start(vui_sib_id, checked_flags, 0);
			vui_ctrl_end();
		}


		vui_ctrl_end();
	}
	return is_active;
}

VuiBool vui_text_radio_button_(VuiCtrlSibId sib_id, VuiCtrlSibId* selected_sib_id, char* text, uint32_t text_length) {
	vui_ctrl_start(sib_id + vui_sib_id, VuiCtrlFlags_focusable | VuiCtrlFlags_selectable, (*selected_sib_id == sib_id) + 1);
	vui_column_layout();

	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);
	VuiBool is_active = (ctrl->state_flags & VuiCtrlStateFlags_active) == VuiCtrlStateFlags_active;
	if (is_active) *selected_sib_id = sib_id;

	VuiBool state;
	vui_scope_offset(VuiCtrlState_default, 0.f, 0.f)
	vui_scope_align(VuiCtrlState_default, VuiAlign_center) {
		state = vui_radio_button(sib_id, selected_sib_id);
		VuiCtrl* radio_button = vui_ctrl_get(_vui.build.sibling_prev_ctrl_id);
		if (radio_button->state_flags & VuiCtrlStateFlags_active) {
			ctrl->state_flags |= VuiCtrlStateFlags_active;
		} else {
			ctrl->state_flags &= ~VuiCtrlStateFlags_active;
		}
		vui_text_(vui_sib_id, text, text_length, 0.f);
	}

	vui_ctrl_end();
	return state;
}

VuiBool vui_image_radio_button(VuiCtrlSibId sib_id, VuiCtrlSibId* selected_sib_id, VuiImageId image_id, VuiColor image_tint) {
	vui_ctrl_start(sib_id + vui_sib_id, VuiCtrlFlags_focusable | VuiCtrlFlags_selectable, (*selected_sib_id == sib_id) + 1);
	vui_column_layout();

	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);
	VuiBool is_active = (ctrl->state_flags & VuiCtrlStateFlags_active) == VuiCtrlStateFlags_active;
	if (is_active) *selected_sib_id = sib_id;

	VuiBool state;
	vui_scope_offset(VuiCtrlState_default, 0.f, 0.f)
	vui_scope_align(VuiCtrlState_default, VuiAlign_center) {
		state = vui_radio_button(sib_id, selected_sib_id);
		VuiCtrl* radio_button = vui_ctrl_get(_vui.build.sibling_prev_ctrl_id);
		if (radio_button->state_flags & VuiCtrlStateFlags_active) {
			ctrl->state_flags |= VuiCtrlStateFlags_active;
		} else {
			ctrl->state_flags &= ~VuiCtrlStateFlags_active;
		}
		vui_image(vui_sib_id, image_id, image_tint);
	}

	vui_ctrl_end();
	return state;
}

void vui_progress_bar(VuiCtrlSibId sib_id, float value, float min, float max) {
	value = vui_clamp(value, min, max);
	float ratio = (value - min) / (max - min);

	vui_scope_layout_wrap(VuiCtrlState_default, vui_false)
	vui_ctrl_start(sib_id, VuiCtrlFlags_background | VuiCtrlFlags_border, 0);
		vui_column_layout();
		VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);

		VuiColor color = ctrl->attributes[VuiCtrlAttr_progress_bar_color].color;
		vui_scope_width_ratio(VuiCtrlState_default, ratio)
		vui_scope_height(VuiCtrlState_default, vui_fill_len)
		vui_scope_margin(VuiCtrlState_default, VuiThickness_zero)
		vui_scope_padding(VuiCtrlState_default, VuiThickness_zero)
		vui_scope_bg_color(VuiCtrlState_default, color)
		vui_ctrl_start(vui_sib_id, VuiCtrlFlags_background, 0);
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
		float scroll_view_border_width = ctrl->attributes[VuiCtrlAttr_border_width].float_;
		container_size.x -= scroll_view_border_width * 2;
		container_size.y -= scroll_view_border_width * 2;

		VuiThickness* scroll_bar_margin = &ctrl->attributes[VuiCtrlAttr_scroll_bar_margin].thickness;
		float scroll_bar_width = ctrl->attributes[VuiCtrlAttr_scroll_bar_width].float_;
		if (ctrl->flags & _VuiCtrlFlags_show_vertical_bar) {
			container_size.x -= scroll_bar_width + VuiThickness_horizontal(scroll_bar_margin);
		}
		if (ctrl->flags & _VuiCtrlFlags_show_horizontal_bar) {
			container_size.y -= scroll_bar_width + VuiThickness_vertical(scroll_bar_margin);
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

void vui_scroll_view_start_(VuiCtrlSibId sib_id, VuiVec2* content_offset_in_out, VuiVec2* size_in_out, VuiScrollFlags flags) {
	vui_scope_padding(VuiCtrlState_default, VuiThickness_zero)
	vui_ctrl_start(sib_id, flags | VuiCtrlFlags_background | VuiCtrlFlags_border, 0);
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
	} else if (ctrl->scroll_view_size.x == 0.f && ctrl->scroll_view_size.y == 0.f) {
		ctrl->scroll_view_size.x = ctrl->attributes[VuiCtrlAttr_width].float_;
		ctrl->scroll_view_size.y = ctrl->attributes[VuiCtrlAttr_height].float_;
	}

	//
	// a resizable scroll view uses the ctrl->scroll_view_size field as it width and height
	if (flags & VuiScrollFlags_resizable) {
		ctrl->attributes[VuiCtrlAttr_width].float_ = ctrl->scroll_view_size.x;
		ctrl->attributes[VuiCtrlAttr_height].float_ = ctrl->scroll_view_size.y;
	}

	//
	// create the infinitely sized control to house the scrollable content.
	vui_scope_margin(VuiCtrlState_default, VuiThickness_zero)
	vui_scope_width(VuiCtrlState_default, vui_auto_len)
	vui_scope_height(VuiCtrlState_default, vui_auto_len)
	vui_scope_offset(VuiCtrlState_default, 0.f, 0.f)
	vui_scope_align(VuiCtrlState_default, VuiAlign_left_top) {
		vui_ctrl_start(-1, 0, 0);
		ctrl = vui_ctrl_get(scroll_view_ctrl_id);
		ctrl->scroll_content_id = _vui.build.parent_ctrl_id;
	}

	//
	// update the scroll offset with the value stored in the content_offset_in_out
	if (content_offset_in_out) {
		VuiCtrl_set_scroll_offset(ctrl, *content_offset_in_out, vui_false);
	}
}

VuiBool _vui_scroll_bar(VuiCtrlSibId sib_id, float width, float length, float container_length, float* content_offset_in_out, float content_length, VuiBool is_horizontal) {
	if (content_length < 1.0) content_length = 1.0;
	vui_scope_width(VuiCtrlState_default, is_horizontal ? length : width)
	vui_scope_height(VuiCtrlState_default, is_horizontal ? width : length)
	vui_box_start(sib_id);
	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);

	//
	// work out the slider size
	VuiVec2 slider_size = VuiVec2_fill;
	float bar_content_length = length - ctrl->attributes[VuiCtrlAttr_border_width].float_ * 2.f;
	float* slider_long_side_length = NULL;
	if (is_horizontal) {
		slider_long_side_length = &slider_size.x;
	} else {
		slider_long_side_length = &slider_size.y;
	}

	float min_slider_len = width;
	// calculate the long side length of the slider that is clamp to a min(min_slider_len) and vui_max(bar_content_length)
	float unclamped_slider_len = bar_content_length * container_length / content_length;
	float slider_len = vui_min(bar_content_length, vui_max(min_slider_len, unclamped_slider_len));

	*slider_long_side_length = slider_len;

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
	slider_offset = vui_max(0.0, vui_min(slider_offset, max_slider_offset));
	if (is_horizontal) slider_offset_vec.x = slider_offset;
	else slider_offset_vec.y = slider_offset;

    float slider_border_width = ctrl->attributes[VuiCtrlAttr_scroll_bar_slider_border_width].float_;
    VuiColor slider_border_color = ctrl->attributes[VuiCtrlAttr_scroll_bar_slider_border_color].color;
    VuiColor slider_bg_color = ctrl->attributes[VuiCtrlAttr_scroll_bar_slider_bg_color].color;

	VuiBool offset_changed = vui_false;
	vui_scope_margin(VuiCtrlState_default, VuiThickness_zero)
	vui_scope_width(VuiCtrlState_default, slider_size.x)
	vui_scope_height(VuiCtrlState_default, slider_size.y)
	vui_scope_align(VuiCtrlState_default, VuiAlign_left_top)
	vui_scope_offset(VuiCtrlState_default, slider_offset_vec.x, slider_offset_vec.y)
	vui_scope_bg_color(VuiCtrlState_default, slider_bg_color)
	vui_scope_border_width(VuiCtrlState_default, slider_border_width)
	vui_scope_border_color(VuiCtrlState_default, slider_border_color) {
		VuiFocusState state = vui_button_start(vui_sib_id);
		offset_changed = state == VuiFocusState_held || state == VuiFocusState_pressed;
		if (offset_changed) {
			float pos_offset = is_horizontal
				? _vui.input.mouse.offset_x
				: _vui.input.mouse.offset_y;

			// offset the slider offset and convert the offset into a content offset
			slider_offset = vui_max(0.0, vui_min(slider_offset + pos_offset, max_slider_offset));
			*content_offset_in_out = max_slider_offset_clamp_ratio == 0.f ? 0.f : -(slider_offset / (bar_content_length * max_slider_offset_clamp_ratio)) * content_length;
		}
		vui_button_end();
	}

	vui_box_end();
	return offset_changed;
}

void vui_scroll_view_end() {
	vui_ctrl_end();

	VuiCtrlId scroll_area_ctrl_id = _vui.build.parent_ctrl_id;
	VuiCtrl* scroll_area = vui_ctrl_get(scroll_area_ctrl_id);
	VuiCtrl* content = vui_ctrl_get(scroll_area->scroll_content_id);
	VuiVec2 content_size = VuiRect_size(content->rect);
	VuiVec2 container_size = VuiRect_size(scroll_area->rect);
	VuiThickness scroll_bar_margin = scroll_area->attributes[VuiCtrlAttr_scroll_bar_margin].thickness;
	float scroll_bar_width = scroll_area->attributes[VuiCtrlAttr_scroll_bar_width].float_;
	VuiCtrlFlags flags = scroll_area->flags;
	float scroll_view_border_width = scroll_area->attributes[VuiCtrlAttr_border_width].float_;
	VuiBool can_show_vertical_bar = flags & VuiCtrlFlags_scrollable_vertical;
	VuiBool can_show_horizontal_bar = flags & VuiCtrlFlags_scrollable_vertical;
	VuiBool show_vertical_bar = vui_false;
	VuiBool show_horizontal_bar = vui_false;
	//
	// determine whether each of the scroll bars are visible or not.
	// and use this to workout the size of the container (the viewport of the scrollable content)
	//
	{
		container_size.x -= scroll_view_border_width * 2;
		container_size.y -= scroll_view_border_width * 2;

		if (can_show_vertical_bar) {
			show_vertical_bar = flags & VuiCtrlFlags_scrollable_vertical_always_show || content_size.y > container_size.y;
		}

		if (show_vertical_bar) {
			container_size.x -= scroll_bar_width + VuiThickness_horizontal(&scroll_bar_margin);
			scroll_area->flags |= _VuiCtrlFlags_show_vertical_bar;
		} else {
			scroll_area->flags &= ~_VuiCtrlFlags_show_vertical_bar;
		}

		if (can_show_horizontal_bar) {
			show_horizontal_bar = flags & VuiCtrlFlags_scrollable_horizontal_always_show;
			if (!show_horizontal_bar) {
				show_horizontal_bar = content_size.x > container_size.x;
			}
		}

		if (show_horizontal_bar) {
			container_size.y -= scroll_bar_width + VuiThickness_vertical(&scroll_bar_margin);
			scroll_area->flags |= _VuiCtrlFlags_show_horizontal_bar;

			if (can_show_vertical_bar && !show_vertical_bar) {
				show_vertical_bar = content_size.y > container_size.y;
				if (show_vertical_bar) {
					container_size.x -= scroll_bar_width + VuiThickness_horizontal(&scroll_bar_margin);
					scroll_area->flags |= _VuiCtrlFlags_show_vertical_bar;
				} else {
					scroll_area->flags &= ~_VuiCtrlFlags_show_vertical_bar;
				}
			}

		} else {
			scroll_area->flags &= ~_VuiCtrlFlags_show_horizontal_bar;
		}
	}

	//
	// WARNING: do not use the variable scroll_area past here.
	// copy the data out fo the scroll area, as when we start new controls
	// they can reallocate the control pool and the pointer will be invalid.
	float scroll_bar_border_width = scroll_area->attributes[VuiCtrlAttr_scroll_bar_border_width].float_;
	VuiColor scroll_bar_border_color = scroll_area->attributes[VuiCtrlAttr_scroll_bar_border_color].color;
	VuiColor scroll_bar_bg_color = scroll_area->attributes[VuiCtrlAttr_scroll_bar_bg_color].color;
	float scroll_bar_radius = scroll_area->attributes[VuiCtrlAttr_scroll_bar_radius].float_;
	VuiVec2 scroll_offset = scroll_area->scroll_offset;
	VuiVec2 scroll_view_size = scroll_area->scroll_view_size;

	//
	// workout the long side lengths of the scroll bars
	float scroll_bar_length_horizontal = scroll_view_size.x - VuiThickness_horizontal(&scroll_bar_margin) - scroll_view_border_width * 2;
	float scroll_bar_length_vertical = scroll_view_size.y - VuiThickness_vertical(&scroll_bar_margin) - scroll_view_border_width * 2;
	if (flags & VuiCtrlFlags_resizable || show_vertical_bar) {
		scroll_bar_length_horizontal -= scroll_bar_width + VuiThickness_horizontal(&scroll_bar_margin);
	}
	if (flags & VuiCtrlFlags_resizable) {
		scroll_bar_length_vertical -= scroll_bar_width + VuiThickness_vertical(&scroll_bar_margin);
	}

	//
	// now setup the scroll bars and resize button if they are enabled.
	//
	VuiFocusState state = VuiFocusState_none;
	VuiBool offset_changed = vui_false;
	vui_scope_border_width(VuiCtrlState_default, scroll_bar_border_width)
	vui_scope_border_color(VuiCtrlState_default, scroll_bar_border_color)
	vui_scope_padding(VuiCtrlState_default, VuiThickness_zero)
	vui_scope_margin(VuiCtrlState_default, VuiThickness_zero)
	vui_scope_radius(VuiCtrlState_default, scroll_bar_radius)
	vui_scope_align(VuiCtrlState_default, VuiAlign_left_top)
	vui_scope_offset(VuiCtrlState_default, 0.f, 0.f)
	vui_scope_layout_wrap(VuiCtrlState_default, vui_false)
	vui_scope_layout_spacing(VuiCtrlState_default, 0.f)
	vui_scope_layout_wrap_spacing(VuiCtrlState_default, 0.f)
	vui_scope_bg_color(VuiCtrlState_default, scroll_bar_bg_color) {
		if (show_horizontal_bar) {
			vui_scope_margin(VuiCtrlState_default, scroll_bar_margin)
			vui_scope_align(VuiCtrlState_default, VuiAlign_left_bottom)
				offset_changed = _vui_scroll_bar(-2, scroll_bar_width, scroll_bar_length_horizontal, container_size.x, &scroll_offset.x, content_size.x, vui_true);
		}

		if (show_vertical_bar) {
			vui_scope_margin(VuiCtrlState_default, scroll_bar_margin)
			vui_scope_align(VuiCtrlState_default, VuiAlign_right_top)
			offset_changed = _vui_scroll_bar(-3, scroll_bar_width, scroll_bar_length_vertical, container_size.y, &scroll_offset.y, content_size.y, vui_false);
		}

		if (flags & VuiCtrlFlags_resizable) {
			vui_scope_margin(VuiCtrlState_default, scroll_bar_margin)
			vui_scope_align(VuiCtrlState_default, VuiAlign_right_bottom)
			vui_scope_width(VuiCtrlState_default, scroll_bar_width)
			vui_scope_height(VuiCtrlState_default, scroll_bar_width) {
				state = vui_button(-4);
				if (state == VuiFocusState_held || state == VuiFocusState_pressed) {
					VuiVec2 min;
					min.x = (VuiThickness_horizontal(&scroll_bar_margin) + scroll_bar_border_width + scroll_view_border_width + scroll_bar_width) * 2;
					min.y = (VuiThickness_vertical(&scroll_bar_margin) + scroll_bar_border_width + scroll_view_border_width + scroll_bar_width) * 2;

					scroll_view_size.x += _vui.input.mouse.offset_x;
					scroll_view_size.y += _vui.input.mouse.offset_y;
					scroll_view_size = VuiVec2_max(min, scroll_view_size);
				}
			}
		}
	}

	//
	// now store the data back into the scroll view as it could have change
	// if the use interacted the scroll view.
	//
	scroll_area = vui_ctrl_get(scroll_area_ctrl_id);
	VuiCtrl_set_scroll_offset(scroll_area, scroll_offset, offset_changed);
	scroll_area->scroll_view_size = scroll_view_size;
	if ((vui_ctrl_is_mouse_scroll_focused(scroll_area->id) || state == VuiFocusState_held || state == VuiFocusState_pressed) && _vui.build.mouse_scroll_focused_size) {
		*_vui.build.mouse_scroll_focused_size = scroll_view_size;
	}

	vui_ctrl_end();
}

VuiBool vui_text_box_(VuiCtrlSibId sib_id, char* string_in_out, uint32_t string_in_out_cap, _VuiInputBoxType type) {
	VuiCtrlFlags flags = VuiCtrlFlags_background | VuiCtrlFlags_border | VuiCtrlFlags_focusable;
	vui_ctrl_start(sib_id, flags, 0);
	VuiCtrl* ctrl = vui_ctrl_get(_vui.build.parent_ctrl_id);
	float text_height = ctrl->attributes[VuiCtrlAttr_text_height].float_;

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
			_vui.text_box_focus_change_fn(vui_true);
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
	}

	vui_scope_offset(VuiCtrlState_default, 0.f, 0.f)
	vui_scope_align(VuiCtrlState_default, VuiAlign_left_top) {
		//
		// the text of the text box
		vui_text_(vui_sib_id, string_in_out, strlen(string_in_out), 0.f);

		//
		// if we are focused, render the cursor or selection box.
		if (vui_ctrl_is_focused(ctrl->id)) {
			VuiFontId font_id = _VuiCtrl_style_attr(ctrl, VuiCtrlAttr_text_font_id)->font_id;
			float radius = 0.f;
			VuiColor bg_color = {0};
			float cursor_width = 0.f;

			// get the offset to the cursor
			VuiVec2 start_idx_offset = vui_get_text_size(string_in_out, _vui.input.focused_text_box.cursor_idx, 0.f, font_id, text_height);
			if (_vui.input.focused_text_box.select_offset) {
				//
				// we are selecting so find the other end of the selection box
				VuiVec2 end_idx_offset = vui_get_text_size(string_in_out,
					_vui.input.focused_text_box.cursor_idx + _vui.input.focused_text_box.select_offset, 0.f, font_id, text_height);
				if (_vui.input.focused_text_box.select_offset < 0) {
					VuiVec2 tmp = end_idx_offset;
					end_idx_offset = start_idx_offset;
					start_idx_offset = tmp;
				}

				radius = _VuiCtrl_style_attr(ctrl, VuiCtrlAttr_text_selection_radius)->float_;
				bg_color = _VuiCtrl_style_attr(ctrl, VuiCtrlAttr_text_selection_color)->color;
				cursor_width = end_idx_offset.x - start_idx_offset.x;
			} else {
				//
				// no selection, we have a cursor
				radius = _VuiCtrl_style_attr(ctrl, VuiCtrlAttr_text_cursor_radius)->float_;
				bg_color = _VuiCtrl_style_attr(ctrl, VuiCtrlAttr_text_cursor_color)->color;
				cursor_width = _VuiCtrl_style_attr(ctrl, VuiCtrlAttr_text_cursor_width)->float_;

				//
				// evenly place the cursor in between two characters
				if (_vui.input.focused_text_box.string_len > 0) {
					start_idx_offset.x -= cursor_width / 2.f;
				}
			}

			//
			// place the cursor or selection box over the text
			vui_scope_border_width(VuiCtrlState_default, 0.f)
			vui_scope_bg_color(VuiCtrlState_default, bg_color)
			vui_scope_offset(VuiCtrlState_default, start_idx_offset.x, 0.f)
			vui_scope_width(VuiCtrlState_default, cursor_width)
			vui_scope_height(VuiCtrlState_default, text_height)
				vui_box(vui_sib_id);
		}
	}

	vui_ctrl_end();
	return has_changed;
}

VuiBool vui_text_box(VuiCtrlSibId sib_id, char* string_in_out, uint32_t string_in_out_cap) {
	vui_assert(string_in_out, "a string buffer must be provided");
	return vui_text_box_(sib_id, string_in_out, string_in_out_cap, _VuiInputBoxType_text);
}

VuiBool vui_input_box_uint(VuiCtrlSibId sib_id, uint32_t* value) {
	char* string = _vui.input.input_box.string;
	snprintf(string, _vui_input_box_cap, "%u", *value);

	VuiBool has_changed = vui_text_box_(sib_id, string, _vui_input_box_cap, _VuiInputBoxType_u32);

	VuiCtrl* text_box = vui_ctrl_get(_vui.build.sibling_prev_ctrl_id);
	if (vui_ctrl_is_focused(text_box->id)) {
		char* string = _vui.input.input_box.edit_string;
		*value = strtoul(string, NULL, 10);
	}
	return has_changed;
}

VuiBool vui_input_box_sint(VuiCtrlSibId sib_id, int32_t* value) {
	char* string = _vui.input.input_box.string;
	snprintf(string, _vui_input_box_cap, "%d", *value);

	VuiBool has_changed = vui_text_box_(sib_id, string, _vui_input_box_cap, _VuiInputBoxType_s32);

	VuiCtrl* text_box = vui_ctrl_get(_vui.build.sibling_prev_ctrl_id);
	if (vui_ctrl_is_focused(text_box->id)) {
		char* string = _vui.input.input_box.edit_string;
		*value = strtol(string, NULL, 10);
	}
	return has_changed;
}

VuiBool vui_input_box_float(VuiCtrlSibId sib_id, float* value) {
	char* string = _vui.input.input_box.string;
	snprintf(string, _vui_input_box_cap, "%f", *value);

	VuiBool has_changed = vui_text_box_(sib_id, string, _vui_input_box_cap, _VuiInputBoxType_float);

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
	_vui.text_box_focus_change_fn = setup->text_box_focus_change_fn;
	_vui.allocator = setup->allocator;
	_VuiArenaAlctor_init(&_vui.frame_data_alctor);
	_vui.windows = vui_mem_alloc_array(_VuiWindow, _vui.allocator, setup->windows_count);
	memset(_vui.windows, 0, setup->windows_count * sizeof(*_vui.windows));
	_vui.windows_count = setup->windows_count;
	_vui.build.style = setup->style;
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
				// while we have no previous siblings navigate up the parent;
				while (ctrl && ctrl->sibling_prev_id == 0) {
					ctrl = vui_ctrl_get(ctrl->parent_id);
				}
				if (!ctrl) break;

				// goto the previous sibling
				ctrl = vui_ctrl_get(ctrl->sibling_prev_id);
				// no descend to the most last child
				while (ctrl->child_last_id) ctrl = vui_ctrl_get(ctrl->child_last_id);
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
						ctrl = vui_ctrl_get(ctrl->parent_id);
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

	//
	// copy over the attributes.
	//
	for (VuiCtrlAttr attr = 0; attr < VuiCtrlAttr_COUNT; attr += 1) {
		root_ctrl->attributes[attr] = *_VuiCtrl_style_attr(root_ctrl, attr);
	}
	root_ctrl->attributes[VuiCtrlAttr_width].float_ = size.x;
	root_ctrl->attributes[VuiCtrlAttr_height].float_ = size.y;

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
	VuiCtrlAttr dir_attr = 0;
	VuiCtrlAttr wrap_dir_attr = 0;
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
		dir_attr = VuiCtrlAttr_width;
		wrap_dir_attr = VuiCtrlAttr_height;
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
		dir_attr = VuiCtrlAttr_height;
		wrap_dir_attr = VuiCtrlAttr_width;
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
		wrap_spacing = ctrl->attributes[VuiCtrlAttr_layout_wrap_spacing].float_;
		wrap = ctrl->attributes[VuiCtrlAttr_layout_wrap].bool_;
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
			if (child->attributes[dir_attr].float_ == vui_auto_len) {
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
			float child_dir_len = child->attributes[dir_attr].float_;
			if (child_dir_len == vui_auto_len) {
				continue;
			} else if (child_dir_len == vui_fill_len) {
				fill_ctrls_count += 1;
			} else if (child_dir_len < 0) { // is ratio
				// remove the ratio from the available_dir_len
				float ratio = -child_dir_len;
				available_dir_len -= inner_dir_len * ratio;
			} else {
				available_dir_len -= child_dir_len + margin_dir_len(&child->attributes[VuiCtrlAttr_margin].thickness);
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

	float layout_spacing = ctrl->attributes[VuiCtrlAttr_layout_spacing].float_;
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
			// advance to the next cell
			*child_placement_area_dir_len_start = *child_placement_area_dir_len_end + layout_spacing;
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

	const VuiThickness* margin = &ctrl->attributes[VuiCtrlAttr_margin].thickness;
	const VuiThickness* padding = &ctrl->attributes[VuiCtrlAttr_padding].thickness;

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

	float border_width = ctrl->flags & VuiCtrlFlags_border ? ctrl->attributes[VuiCtrlAttr_border_width].float_ : 0.f;
	float inner_x = (_vui.flags & _VuiFlags_right_to_left)
		? margin->right + padding->right + border_width
		: margin->left + padding->left + border_width;
	float inner_y = margin->top + padding->top + border_width;
	float inner_width = vui_auto_len;
	float inner_height = vui_auto_len;

	{
		float width = ctrl->attributes[VuiCtrlAttr_width].float_;

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
				outer_width = width + (margin->left + margin->right);
			}

			if (_vui.flags & _VuiFlags_right_to_left) {
				ctrl->rect.left = ctrl->rect.right + outer_width;
			} else {
				ctrl->rect.right = ctrl->rect.left + outer_width;
			}
			inner_width = outer_width - (margin->left + margin->right) - (padding->left + padding->right) - border_width * 2;
		}
	}

	{
		float height = ctrl->attributes[VuiCtrlAttr_height].float_;
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
				outer_height = height + (margin->top + margin->bottom);
			}

			ctrl->rect.bottom = ctrl->rect.top + outer_height;
			inner_height = outer_height - (margin->top + margin->bottom) - (padding->top + padding->bottom) - border_width * 2;
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
					float outer_width = max_width + (padding->left + padding->right) + (border_width * 2) + (margin->left + margin->right);
					if (_vui.flags & _VuiFlags_right_to_left) {
						ctrl->rect.left = ctrl->rect.right + outer_width;
					} else {
						ctrl->rect.right = ctrl->rect.left + outer_width;
					}
					inner_width = max_width;
				}
				if (inner_height == vui_auto_len) {
					ctrl->rect.bottom = ctrl->rect.top + max_height + (padding->top + padding->bottom) + (border_width * 2) + (margin->top + margin->bottom);
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
		content_ctrl->rect.left += ctrl->scroll_offset.x;
		content_ctrl->rect.right += ctrl->scroll_offset.x;
		content_ctrl->rect.top += ctrl->scroll_offset.y;
		content_ctrl->rect.bottom += ctrl->scroll_offset.y;
	}

	//
	// resolve the dimensions with automatic lengths
	//
	if (inner_width == vui_auto_len) {
		if (_vui.flags & _VuiFlags_right_to_left) {
			ctrl->rect.left = ctrl->rect.right + max_inner_right_bottom.x + padding->left + border_width + margin->left;
		} else {
			ctrl->rect.right = ctrl->rect.left + max_inner_right_bottom.x + padding->right + border_width + margin->right;
		}
	}
	if (inner_height == vui_auto_len) {
		ctrl->rect.bottom = ctrl->rect.top + max_inner_right_bottom.y + padding->bottom + border_width + margin->bottom;
	}

	//
	// if we actually are being placed somewhere.
	// workout where in the placement_area we go.
	VuiVec2 placement_size = VuiRect_size(*placement_area);
	if (placement_size.x > 0.f && placement_size.y > 0.f) {
		VuiVec2 size = VuiRect_size(ctrl->rect);
		VuiVec2 offset = ctrl->attributes[VuiCtrlAttr_offset].vec2;
		VuiAlign align = ctrl->attributes[VuiCtrlAttr_align].align;

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
	const VuiThickness* margin = &ctrl->attributes[VuiCtrlAttr_margin].thickness;
	ctrl->rect.left += margin->left;
	ctrl->rect.top += margin->top;
	ctrl->rect.right -= margin->right;
	ctrl->rect.bottom -= margin->bottom;

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

	float width = root->attributes[VuiCtrlAttr_width].float_;
	float height = root->attributes[VuiCtrlAttr_height].float_;
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
		_vui_layout_ctrls_finalize(child, (VuiVec2){0}, root->attributes[VuiCtrlAttr_width].float_);
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

	if (flags & VuiCtrlFlags_background) {
		VuiColor color = ctrl->attributes[VuiCtrlAttr_bg_color].color;
		float radius = ctrl->attributes[VuiCtrlAttr_radius].float_;
		if (flags & VuiCtrlFlags_border) {
			float half_width = ctrl->attributes[VuiCtrlAttr_border_width].float_ / 2.f;
			inner_rect.left += half_width;
			inner_rect.top += half_width;
			inner_rect.bottom -= half_width;
			inner_rect.right -= half_width;
		}
		vui_render_rect(&inner_rect, color, radius);
		if (flags & VuiCtrlFlags_border) {
			float half_width = ctrl->attributes[VuiCtrlAttr_border_width].float_ / 2.f;
			inner_rect.left -= half_width;
			inner_rect.top -= half_width;
			inner_rect.bottom += half_width;
			inner_rect.right += half_width;
		}
	}

	if (flags & VuiCtrlFlags_border) {
		VuiColor color = ctrl->attributes[VuiCtrlAttr_border_color].color;
		float width = ctrl->attributes[VuiCtrlAttr_border_width].float_;
		float radius = ctrl->attributes[VuiCtrlAttr_radius].float_;
		vui_render_rect_border(&ctrl->rect, color, radius, width);

		inner_rect.left += width;
		inner_rect.top += width;
		inner_rect.bottom -= width;
		inner_rect.right -= width;
		_vui.render.clip_rect = VuiRect_clip(&_vui.render.clip_rect, &inner_rect);
	}

	if (flags & _VuiCtrlFlags_image) {
		VuiImageScaleMode scale_mode = ctrl->attributes[VuiCtrlAttr_image_scale_mode].image_scale_mode;
		vui_render_image(&ctrl->rect, ctrl->image_id, ctrl->image_tint, scale_mode);
	}

	if (flags & _VuiCtrlFlags_text) {
		VuiFontId font_id = ctrl->attributes[VuiCtrlAttr_text_font_id].font_id;
		char* text = &_vui.render.w->text[ctrl->text_start_idx];
		VuiColor color = ctrl->attributes[VuiCtrlAttr_text_color].color;
		float text_height = ctrl->attributes[VuiCtrlAttr_text_height].float_;
		vui_render_text(ctrl->rect.left_top, font_id, text_height, text, ctrl->text_length, color, ctrl->text_wrap_width);
	}

	const VuiThickness* padding = &ctrl->attributes[VuiCtrlAttr_padding].thickness;
	inner_rect.left += padding->left;
	inner_rect.top += padding->top;
	inner_rect.bottom -= padding->bottom;
	inner_rect.right -= padding->right;
	_vui.render.clip_rect = VuiRect_clip(&_vui.render.clip_rect, &inner_rect);

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
				verts[v_idx].pos.x = verts[v_idx].pos.x * scale_factor;
				verts[v_idx].pos.y = verts[v_idx].pos.y * scale_factor;
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
			fprintf(file,
					"\t%u: { pos: [%f, %f], uv: [%f, %f], color: #%.2x%.2x%.2x%.2x }\n",
					vert_idx,
					vert->pos.x,
					vert->pos.y,
					vert->uv.x,
					vert->uv.y,
					vert->color.r,
					vert->color.g,
					vert->color.b,
					vert->color.a);
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

