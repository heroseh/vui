//
// BSD Zero Clause License
//
// Copyright (c) 2020 Henry Rose
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
// OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.
//

#include <stdarg.h> // va_start, va_end

#ifdef _WIN32
#define PRINTF_UINTPTR "%llu"
#else
#define PRINTF_UINTPTR "%lu"
#endif

static DasBool das_is_power_of_two(uintptr_t v) {
	return (v != 0) && ((v & (v - 1)) == 0);
}

void* das_ptr_round_up_align(void* ptr, uintptr_t align) {
	das_debug_assert(das_is_power_of_two(align), "align must be a power of two but got: " PRINTF_UINTPTR, align);
	return (void*)(((uintptr_t)ptr + (align - 1)) & ~(align - 1));
}

void* das_ptr_round_down_align(void* ptr, uintptr_t align) {
	das_debug_assert(das_is_power_of_two(align), "align must be a power of two but got: " PRINTF_UINTPTR, align);
	return (void*)((uintptr_t)ptr & ~(align - 1));
}

#ifndef DAS_NO_SYSTEM_ALCTOR

#include <malloc.h>
#ifdef _WIN32

// fortunately, windows provides aligned memory allocation function
void* _das_aligned_malloc(void* alloc_data, void* ptr, uintptr_t old_size, uintptr_t size, uintptr_t align) {
	return _aligned_malloc(size, align);
}
void* _das_aligned_realloc(void* alloc_data, void* ptr, uintptr_t old_size, uintptr_t size, uintptr_t align) {
	return _aligned_realloc(ptr, size, align);
}
void* _das_aligned_free(void* alloc_data, void* ptr, uintptr_t old_size, uintptr_t size, uintptr_t align) {
	_aligned_free(ptr);
	return NULL;
}

#else // posix

//
// the C11 standard says malloc is guaranteed aligned to alignof(max_align_t).
// so allocations that have alignment less than or equal to this, can directly call malloc, realloc and free.
// luckly this is for most allocations.
//
// but there are alignments that are larger (for example is intel AVX256 primitives).
// these require calls aligned_alloc.
void* _das_aligned_malloc(void* alloc_data, void* ptr, uintptr_t old_size, uintptr_t size, uintptr_t align) {
	if (align <= alignof(max_align_t)) {
		return malloc(size);
	}

	return aligned_alloc(align, size);
}

void* _das_aligned_realloc(void* alloc_data, void* ptr, uintptr_t old_size, uintptr_t size, uintptr_t align) {
	if (align <= alignof(max_align_t)) {
		return realloc(ptr, size);
	}

	//
	// there is no aligned realloction on any posix based systems :(
	void* new_ptr = aligned_alloc(align, size);
	memcpy(new_ptr, ptr, das_min(old_size, size));
	free(ptr);
	return new_ptr;
}

void* _das_aligned_free(void* alloc_data, void* ptr, uintptr_t old_size, uintptr_t size, uintptr_t align) {
	free(ptr);
	return NULL;
}
#endif

DasAllocFn das_system_alctor_fns[3] = {
	_das_aligned_malloc,
	_das_aligned_realloc,
	_das_aligned_free,
};

#endif // DAS_NO_SYSTEM_ALCTOR


#ifndef DAS_DEFAULT_ALCTOR

#ifndef DAS_NO_SYSTEM_ALCTOR
#define DAS_DEFAULT_ALCTOR { .fns = das_system_alctor_fns, .data = NULL }
#else
#define DAS_DEFAULT_ALCTOR { .fns = NULL, .data = NULL }
#endif // DAS_NO_SYSTEM_ALCTOR

#endif // DAS_DEFAULT_ALCTOR
thread_local DasAlctor _das_alctor = DAS_DEFAULT_ALCTOR;

#ifndef DAS_DEFAULT_OUT_OF_MEM_HANDLER
#define DAS_DEFAULT_OUT_OF_MEM_HANDLER { .fn = NULL, .data = NULL }
#endif
thread_local DasOutOfMemHandler _das_out_of_mem_handler = DAS_DEFAULT_OUT_OF_MEM_HANDLER;

void* das_realloc(void* ptr, uintptr_t old_size, uintptr_t size, uintptr_t align) {
	DasAllocFn fn = _das_alctor.fns[ptr ? 1 : 0];
	void* new_ptr = fn(_das_alctor.data, ptr, old_size, size, align);
	if (new_ptr) return new_ptr;

	//
	// run the out of memory handler to give the application a chance
	// to free some memory or change the allocator.
	if (_das_out_of_mem_handler.fn && _das_out_of_mem_handler.fn(_das_out_of_mem_handler.data, &_das_alctor, size)) {
		void* new_ptr = fn(_das_alctor.data, ptr, old_size, size, align);
		if (new_ptr) return new_ptr;
	}

	fprintf(stderr, "failed to allocate " PRINTF_UINTPTR " bytes\n", size);
	abort();
}

void das_dealloc(void* ptr, uintptr_t old_size, uintptr_t align) {
	DasAllocFn fn = _das_alctor.fns[2];
	void* new_ptr = fn(_das_alctor.data, ptr, old_size, 0, align);
}

DasAlctor das_swap_alctor(DasAlctor alctor) {
	DasAlctor a = _das_alctor;
	_das_alctor = alctor;
	return a;
}

DasOutOfMemHandler das_swap_out_of_mem_handler(DasOutOfMemHandler handler) {
	DasOutOfMemHandler h = _das_out_of_mem_handler;
	_das_out_of_mem_handler = handler;
	return h;
}

void _DasStk_init_with_cap(_DasStk* stk, uint32_t cap, uint32_t elmt_size, uint32_t elmt_align) {
	*stk = (_DasStk){0};
	_DasStk_resize_cap(stk, cap, elmt_size, elmt_align);
}

void _DasStk_deinit(_DasStk* stk, uint32_t elmt_size, uint32_t elmt_align) {
	das_dealloc(stk->data, stk->cap * elmt_size, elmt_align);
	stk->data = NULL;
	stk->count = 0;
	stk->cap = 0;
}

void* _DasStk_get(_DasStk* stk, uint32_t idx, uint32_t elmt_size) {
	das_assert(idx < stk->count, "idx '%u' is out of bounds for a stack of count '%u'", idx, stk->count);
	return das_ptr_add(stk->data, idx * elmt_size);
}

void _DasStk_resize(_DasStk* stk, uint32_t new_count, DasBool zero, uint32_t elmt_size, uint32_t elmt_align) {
	if (stk->cap < new_count) {
		_DasStk_resize_cap(stk, das_max(new_count, stk->cap * 2), elmt_size, elmt_align);
	}

	uint32_t count = stk->count;
	if (zero && new_count > count) {
		memset(das_ptr_add(stk->data, count * elmt_size), 0, (new_count - count) * elmt_size);
	}

	stk->count = new_count;
}

void _DasStk_resize_cap(_DasStk* stk, uint32_t new_cap, uint32_t elmt_size, uint32_t elmt_align) {
	new_cap = das_max(das_max(DasStk_min_cap, new_cap), stk->count);
	if (stk->cap == new_cap) return;

	uintptr_t size = (uintptr_t)stk->cap * (uintptr_t)elmt_size;
	uintptr_t new_size = (uintptr_t)new_cap * (uintptr_t)elmt_size;
	void* ptr = das_realloc(stk->data, size, new_size, elmt_align);

	stk->data = ptr;
	stk->cap = new_cap;
}

void* _DasStk_insert_many(_DasStk* stk, uint32_t idx, void* elmts, uint32_t elmts_count, uint32_t elmt_size, uint32_t elmt_align) {
	das_assert(idx <= stk->count, "insert idx '%u' must be less than or equal to count of '%u'", idx, stk->count);

    uint32_t requested_count = stk->count + elmts_count;
    if (stk->cap < requested_count) {
		_DasStk_resize_cap(stk, das_max(requested_count, stk->cap * 2), elmt_size, elmt_align);
    }

    void* dst = das_ptr_add(stk->data, idx * elmt_size);

	// shift the elements from idx to (idx + elmts_count), to the right
	// to make room for the elements
    memmove(das_ptr_add(dst, elmts_count * elmt_size), dst, (stk->count - idx) * elmt_size);

	if (elmts) {
		// copy the elements to the stack
		memcpy(dst, elmts, (elmts_count * elmt_size));
	}

    stk->count += elmts_count;
	return dst;
}

void* _DasStk_push_many(_DasStk* stk, void* elmts, uint32_t elmts_count, uint32_t elmt_size, uint32_t elmt_align) {
	uint32_t idx = stk->count;
	uint32_t new_count = stk->count + elmts_count;
	if (new_count > stk->cap) {
		_DasStk_resize_cap(stk, das_max(new_count, stk->cap * 2), elmt_size, elmt_align);
	}
	stk->count = new_count;
	void* dst = das_ptr_add(stk->data, idx * elmt_size);
	if (elmts) {
		memcpy(dst, elmts, elmt_size * elmts_count);
	}
	return dst;
}

uint32_t _DasStk_pop_many(_DasStk* stk, void* elmts_out, uint32_t elmts_count, uint32_t elmt_size) {
	elmts_count = das_min(elmts_count, stk->count);
	stk->count -= elmts_count;
	if (elmts_out) {
		memcpy(elmts_out, das_ptr_add(stk->data, stk->count * elmt_size), elmts_count * elmt_size);
	}

	return elmts_count;
}

void _DasStk_swap_remove_range(_DasStk* stk, uint32_t start, uint32_t end, void* elmts_out, uint32_t elmt_size) {
	das_assert(start <= stk->count, "start idx '%u' must be less than the count of '%u'", start, stk->count);
	das_assert(end <= stk->count, "end idx '%u' must be less than or equal to count of '%u'", end, stk->count);

	uint32_t remove_count = end - start;
	void* dst = das_ptr_add(stk->data, start * elmt_size);
	if (elmts_out) memcpy(elmts_out, dst, remove_count * elmt_size);

	uint32_t src_idx = stk->count;
	stk->count -= remove_count;
	if (remove_count > stk->count) remove_count = stk->count;
	src_idx -= remove_count;

	void* src = das_ptr_add(stk->data, src_idx * elmt_size);
	memmove(dst, src, remove_count * elmt_size);
}

void _DasStk_shift_remove_range(_DasStk* stk, uint32_t start, uint32_t end, void* elmts_out, uint32_t elmt_size) {
	das_assert(start <= stk->count, "start idx '%u' must be less than the count of '%u'", start, stk->count);
	das_assert(end <= stk->count, "end idx '%u' must be less than or equal to count of '%u'", end, stk->count);

	uint32_t remove_count = end - start;
	void* dst = das_ptr_add(stk->data, start * elmt_size);
	if (elmts_out) memcpy(elmts_out, dst, remove_count * elmt_size);

	if (end < stk->count) {
		void* src = das_ptr_add(dst, remove_count * elmt_size);
		memmove(dst, src, (stk->count - (start + remove_count)) * elmt_size);
	}
	stk->count -= remove_count;
}


void DasStk_push_str_fmt(DasStk(char)* stk, char* fmt, ...) {
	va_list args;

	va_start(args, fmt);
	// add 1 so we have enough room for the null terminator that vsnprintf always outputs
	uint32_t count = vsnprintf(NULL, 0, fmt, args) + 1;
	va_end(args);

	if (count == 1) return;

	uint32_t required_cap = stk->count + count;
	if (required_cap > stk->cap) {
		DasStk_resize_cap(stk, required_cap);
	}

	va_start(args, fmt);
	count = vsnprintf((char*)das_ptr_add(stk->DasStk_data, stk->count), count, fmt, args);
	va_end(args);

	stk->count += count;
}

static inline uint32_t _DasDeque_wrapping_add(uint32_t cap, uint32_t idx, uint32_t value) {
    uint32_t res = idx + value;
    if (res >= cap) res = value - (cap - idx);
    return res;
}

static inline uint32_t _DasDeque_wrapping_sub(uint32_t cap, uint32_t idx, uint32_t value) {
    uint32_t res = idx - value;
    if (res > idx) res = cap - (value - idx);
    return res;
}

void _DasDeque_init_with_cap(_DasDeque* deque, uint32_t cap, uint32_t elmt_size, uint32_t elmt_align) {
	*deque = (_DasDeque){0};
	_DasDeque_resize_cap(deque, cap, elmt_size, elmt_align);
}

void _DasDeque_deinit(_DasDeque* deque, uint32_t elmt_size, uint32_t elmt_align) {
	das_dealloc(deque->data, deque->_cap * elmt_size, elmt_align);

	deque->data = NULL;
	deque->_cap = 0;
	deque->_front_idx = 0;
	deque->_back_idx = 0;
}

void* _DasDeque_get(_DasDeque* deque, uint32_t idx, uint32_t elmt_size) {
	uint32_t count = DasDeque_count(deque);
	das_assert(idx < count, "idx '%u' is out of bounds for a deque of count '%u'", idx, count);
    idx = _DasDeque_wrapping_add(deque->_cap, deque->_front_idx, idx);
    return das_ptr_add(deque->data, idx * elmt_size);
}

void _DasDeque_resize_cap(_DasDeque* deque, uint32_t cap, uint32_t elmt_size, uint32_t elmt_align) {
	// add one because the back_idx needs to point to the next empty element slot.
	cap += 1;
	cap = das_max(DasDeque_min_cap, cap);
	uint32_t count = DasDeque_count(deque);
	if (cap < count) cap = count;
	if (cap == deque->_cap) return;

	uintptr_t bc = (uintptr_t)deque->_cap * (uintptr_t)elmt_size;
	uintptr_t new_bc = (uintptr_t)cap * (uintptr_t)elmt_size;
	void* data = das_realloc(deque->data, bc, new_bc, elmt_align);
	deque->data = data;

	uint32_t old_cap = deque->_cap;
	deque->_cap = cap;

	// move the front_idx and back_idx around to resolve the gaps that could have been created after resizing the buffer
	//
    // A - no gaps created so no need to change
    // --------
    //   F     B        F     B
    // [ V V V . ] -> [ V V V . . . . ]
    //
    // B - less elements before back_idx than elements from front_idx, so copy back_idx after the front_idx
    //       B F                  F         B
    // [ V V . V V V ] -> [ . . . V V V V V . . . . . ]
    //
    // C - more elements before back_idx than elements from front_idx, so copy front_idx to the end
    //       B F           B           F
    // [ V V . V] -> [ V V . . . . . . V ]
    //
    if (deque->_front_idx <= deque->_back_idx) { // A
    } else if (deque->_back_idx  < old_cap - deque->_front_idx) { // B
        memcpy(das_ptr_add(data, old_cap * elmt_size), data, deque->_back_idx * elmt_size);
        deque->_back_idx += old_cap;
        das_debug_assert(deque->_back_idx > deque->_front_idx, "back_idx must come after front_idx");
    } else { // C
        uint32_t new_front_idx = cap - (old_cap - deque->_front_idx);
        memcpy(das_ptr_add(data, new_front_idx * elmt_size), das_ptr_add(data, deque->_front_idx * elmt_size), (old_cap - deque->_front_idx) * elmt_size);
        deque->_front_idx = new_front_idx;
        das_debug_assert(deque->_back_idx < deque->_front_idx, "front_idx must come after back_idx");
    }

	das_debug_assert(deque->_back_idx < cap, "back_idx must remain in bounds");
	das_debug_assert(deque->_front_idx < cap, "front_idx must remain in bounds");
}

void _DasDeque_push_front_many(_DasDeque* deque, void* elmts, uint32_t elmts_count, uint32_t elmt_size, uint32_t elmt_align) {
	uint32_t new_count = DasDeque_count(deque) + elmts_count;
	if (deque->_cap < new_count + 1) {
		_DasDeque_resize_cap(deque, das_max(deque->_cap * 2, new_count), elmt_size, elmt_align);
	}

	if (elmts) {
		if (elmts_count > deque->_front_idx) {
			//
			// there is enough elements that pushing on the front
			// will cause the front_idx to loop around.
			// so copy in two parts
			// eg. pushing 3 values
			//     F B                    B
			// [ . V . . . . . ] -> [ V V . . . V V ]
			uint32_t rem_count = elmts_count - deque->_front_idx;
			// copy to the end of the buffer
			memcpy(das_ptr_add(deque->data, (deque->_cap - rem_count) * elmt_size), elmts, rem_count * elmt_size);
			// copy to the beginning of the buffer
			memcpy(deque->data, das_ptr_add(elmts, rem_count * elmt_size), deque->_front_idx * elmt_size);
		} else {
			//
			// coping the elements can be done in a single copy
			memcpy(das_ptr_add(deque->data, (deque->_front_idx - elmts_count) * elmt_size), elmts, elmts_count * elmt_size);
		}
	}

	deque->_front_idx = _DasDeque_wrapping_sub(deque->_cap, deque->_front_idx, elmts_count);
}

void _DasDeque_push_back_many(_DasDeque* deque, void* elmts, uint32_t elmts_count, uint32_t elmt_size, uint32_t elmt_align) {
	uint32_t new_count = DasDeque_count(deque) + elmts_count;
	if (deque->_cap < new_count + 1) {
		_DasDeque_resize_cap(deque, das_max(deque->_cap * 2, new_count), elmt_size, elmt_align);
	}

	if (elmts) {
		if (deque->_cap < deque->_back_idx + elmts_count) {
			//
			// there is enough elements that pushing on the back
			// will cause the back_idx to loop around.
			// so copy in two parts
			// eg. pushing 3 values
			//             F B            B     F
			// [ . . . . . V . ] -> [ V V . . . V V ]
			uint32_t rem_count = deque->_cap - deque->_back_idx;
			// copy to the end of the buffer
			memcpy(das_ptr_add(deque->data, deque->_back_idx * elmt_size), elmts, rem_count * elmt_size);
			// copy to the beginning of the buffer
			memcpy(deque->data, das_ptr_add(elmts, rem_count * elmt_size), ((elmts_count - rem_count) * elmt_size));
		} else {
			//
			// coping the elements can be done in a single copy
			memcpy(das_ptr_add(deque->data, deque->_back_idx * elmt_size), elmts, elmts_count * elmt_size);
		}
	}

	deque->_back_idx = _DasDeque_wrapping_add(deque->_cap, deque->_back_idx, elmts_count);
}

uint32_t _DasDeque_pop_front_many(_DasDeque* deque, void* elmts_out, uint32_t elmts_count, uint32_t elmt_size) {
    if (deque->_front_idx == deque->_back_idx) return 0;
	elmts_count = das_min(elmts_count, DasDeque_count(deque));

	if (elmts_out) {
		if (deque->_cap < deque->_front_idx + elmts_count) {
			//
			// there is enough elements that popping from the front
			// will cause the front_idx to loop around.
			// so copy in two parts
			// eg. popping 4 values
			//         B   F                    F B
			// [ V V V . . V V ] -> [ . . . . . V . ]
			uint32_t rem_count = deque->_cap - deque->_front_idx;
			// copy from the end of the buffer
			memcpy(elmts_out, das_ptr_add(deque->data, deque->_front_idx * elmt_size), rem_count * elmt_size);
			// copy from the beginning of the buffer
			memcpy(das_ptr_add(elmts_out, rem_count * elmt_size), deque->data, ((elmts_count - rem_count) * elmt_size));
		} else {
			//
			// coping the elements can be done in a single copy
			memcpy(elmts_out, das_ptr_add(deque->data, deque->_front_idx * elmt_size), elmts_count * elmt_size);
		}
	}

	deque->_front_idx = _DasDeque_wrapping_add(deque->_cap, deque->_front_idx, elmts_count);
	return elmts_count;
}

uint32_t _DasDeque_pop_back_many(_DasDeque* deque, void* elmts_out, uint32_t elmts_count, uint32_t elmt_size) {
    if (deque->_front_idx == deque->_back_idx) return 0;
	elmts_count = das_min(elmts_count, DasDeque_count(deque));

	if (elmts_out) {
		if (elmts_count > deque->_back_idx) {
			//
			// there is enough elements that popping from the back
			// will cause the back_idx to loop around.
			// so copy in two parts
			// eg. popping 3 values
			//     B     F              F B
			// [ V . . . V V V ] -> [ . V . . . . . ]
			uint32_t rem_count = elmts_count - deque->_back_idx;
			// copy from the end of the buffer
			memcpy(elmts_out, das_ptr_add(deque->data, (deque->_cap - rem_count) * elmt_size), rem_count * elmt_size);
			// copy from the beginning of the buffer
			memcpy(das_ptr_add(elmts_out, rem_count * elmt_size), deque->data, deque->_back_idx * elmt_size);
		} else {
			//
			// coping the elements can be done in a single copy
			memcpy(elmts_out, das_ptr_add(deque->data, (deque->_back_idx - elmts_count) * elmt_size), elmts_count * elmt_size);
		}
	}

	deque->_back_idx = _DasDeque_wrapping_sub(deque->_cap, deque->_back_idx, elmts_count);
	return elmts_count;
}

