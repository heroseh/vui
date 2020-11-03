#ifndef DAS_H
#define DAS_H

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

//
//
// DAS - dynamically allocated storage
//
// A simple dynamic allocation library with a stack and double ended queue implementation.
//
// # Features
// - array-like stack (DasStk)
// - double ended queue (DasDeque)
// - thread local custom allocator (DasAlctor)
// - allocation API that uses the custom allocator (das_alloc, das_realloc, das_dealloc)
// - compiles as C11 and C++11
// - simple API to keep user code as simple as possible.
//     - no type decorations required on the Stack and Deque API
//     - out of memory errors are never returned, instead they are handled through a global thread local callback
// - no executable bloat
//     - we define a single function for each operation, that can be used for all types of DasStk/DasDeque
// - fast compile times
//     - we do not generate any code, other than single line macros
//
// # Supported Platforms
// The code should ideally compile on any C and C++ compiler with thread_local and typeof (language extension) support.
// C11 and C++11 are the current targets, since this is when thread_local was included in the standards.
// If it doesn't mess with the code too much, I am open to supporting older versions if anyone wants to submit some patches :)
//
// - Windows (clang)
// - Linux (clang & gcc)
// - MacOS (clang) - untested but should work since it's clang. let me know if you can test it!
// - IOS (clang) - untested but should work since it's clang. let me know if you can test it!
// - Android (gcc) - untested but should work since it's gcc. let me know if you can test it!
//
// # How to Build
//
// You only need to copy **das.h** and **das.c** into your project.
//
// In one of your C or C++ compilation units, include the **das.h** header and **das.c** source files like so:
//
// #include "das.h"
// #include "das.c"
//
//
// To use the library in other files, you need to include the **das.h** header.
//
// #include "das.h"
//

#ifdef __cplusplus
#include <type_traits>
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h> // fprintf, vsnprintf
#include <stdlib.h> // abort
#include <string.h> // memcpy, memmove
#include <stddef.h> // ptrdiff_t

typedef uint8_t DasBool;
#define das_false 0
#define das_true 1

#define das_assert(cond, message, ...) \
	if (!(cond)) { fprintf(stderr, message"\n", ##__VA_ARGS__); abort(); }

#ifdef DAS_DEBUG_ASSERTIONS
#define das_debug_assert das_assert
#else
#define das_debug_assert(cond, message, ...) (void)(cond)
#endif

#ifndef __cplusplus
#define alignof _Alignof
#define thread_local _Thread_local
#define static_assert _Static_assert
#define _das_types_match(a, b) __builtin_types_compatible_p(a, b)
#else
#define typeof decltype
#define _das_types_match(a, b) std::is_same<a, b>()
#endif

#define das_min(a, b) \
   ({ typeof(a) _a = (a); \
       typeof(b) _b = (b); \
     _a < _b ? _a : _b; })

#define das_max(a, b) \
   ({ typeof(a) _a = (a); \
       typeof(b) _b = (b); \
     _a > _b ? _a : _b; })

#define _das_assert_type(data_ptr, elmt_ptr) \
   static_assert(_das_types_match(void*, typeof(elmt_ptr)) || _das_types_match(typeof(*(data_ptr)), typeof(*(elmt_ptr))), "type mismatch, the element pointer type does not match the type of the data")

//
// ================== DasStk ==================
//
// linear stack of elements.
// LIFO is the optimal usage, so push elements and pop them from the end.
// can be used as a growable array.
// elements are allocated using the Dynamic Allocation API that is defined lower down in this file.
//
// can store up to UINT32_MAX of elements (so UINT32_MAX * sizeof(T) bytes).
// this is so the structure is the size of 2 pointers instead of 3 on 64 bit machines.
// this helps keep data in cache that sits along side the stack structure.
// it is unlikely you will need more elements, but if you do.
// you can always copy and paste the code a run a custom solution.
//
// DasStk API example usage:
//
// DasStk(int) stack_of_ints;
// DasStk_init_with_cap(int, &stack_of_ints, 64);
//
// int value = 22;
// DasStk_push(&stack_of_ints, &value);
//
// DasStk_pop(&stack_of_ints, &value);
//

#define DasStk(T) DasStk_##T
typedef struct {
	void* data;
	uint32_t count;
	uint32_t cap;
} _DasStk;

//
// you need to make sure that you use this macro in global scope
// to define a structure for the type you want to use.
//
// we have a funky name for the data field,
// so an error will get thrown if you pass the
// wrong structure into one of the macros below.
#define typedef_DasStk(T) \
typedef struct { \
	T* DasStk_data; \
	uint32_t count; \
	uint32_t cap; \
} DasStk_##T;

typedef_DasStk(char);
typedef_DasStk(short);
typedef_DasStk(int);
typedef_DasStk(long);
typedef_DasStk(float);
typedef_DasStk(double);
typedef_DasStk(size_t);
typedef_DasStk(ptrdiff_t);
typedef_DasStk(uint8_t);
typedef_DasStk(uint16_t);
typedef_DasStk(uint32_t);
typedef_DasStk(uint64_t);
typedef_DasStk(int8_t);
typedef_DasStk(int16_t);
typedef_DasStk(int32_t);
typedef_DasStk(int64_t);

#define DasStk_min_cap 4

// initializes an empty stack with a preallocated capacity of 'cap' elements
#define DasStk_init_with_cap(stk, cap) _DasStk_init_with_cap((_DasStk*)stk, cap, sizeof(*(stk)->DasStk_data), alignof(typeof(*(stk)->DasStk_data)))
extern void _DasStk_init_with_cap(_DasStk* stk, uint32_t cap, uint32_t elmt_size, uint32_t elmt_align);

// deallocates the memory and sets the stack to being empty.
#define DasStk_deinit(stk) _DasStk_deinit((_DasStk*)stk, sizeof(*(stk)->DasStk_data), alignof(typeof(*(stk)->DasStk_data)))
extern void _DasStk_deinit(_DasStk* stk, uint32_t elmt_size, uint32_t elmt_align);

// returns a pointer to the element at idx.
// this function will abort if idx is out of bounds
#define DasStk_get(stk, idx) ((typeof((stk)->DasStk_data))_DasStk_get((_DasStk*)stk, idx, sizeof(*(stk)->DasStk_data)))
extern void* _DasStk_get(_DasStk* stk, uint32_t idx, uint32_t elmt_size);

// returns a pointer to the first element
// this function will abort if the stack is empty
#define DasStk_get_first(stk) ((typeof((stk)->DasStk_data))_DasStk_get((_DasStk*)stk, 0, sizeof(*(stk)->DasStk_data)))

// returns a pointer to the last element
// this function will abort if the stack is empty
#define DasStk_get_last(stk) ((typeof((stk)->DasStk_data))_DasStk_get((_DasStk*)stk, (stk)->count - 1, sizeof(*(stk)->DasStk_data)))

// resizes the stack to have new_count number of elements.
// if new_count is greater than stk->cap, then this function will internally call DasStk_resize_cap to make more room.
// if new_count is greater than stk->count, new elements will be uninitialized,
// unless you want the to be zeroed by passing in zero == das_true.
#define DasStk_resize(stk, new_count, zero) _DasStk_resize((_DasStk*)stk, new_count, zero, sizeof(*(stk)->DasStk_data), alignof(typeof(*(stk)->DasStk_data)))
extern void _DasStk_resize(_DasStk* stk, uint32_t new_count, DasBool zero, uint32_t elmt_size, uint32_t elmt_align);

// reallocates the capacity of the data to have enough room for new_cap number of elements.
// if new_cap is less than stk->count then new_cap is set to stk->count.
#define DasStk_resize_cap(stk, new_cap) _DasStk_resize_cap((_DasStk*)stk, new_cap, sizeof(*(stk)->DasStk_data), alignof(typeof(*(stk)->DasStk_data)))
extern void _DasStk_resize_cap(_DasStk* stk, uint32_t new_cap, uint32_t elmt_size, uint32_t elmt_align);

// insert element/s at the index position in the stack.
// the elements from the index position in the stack, will be shifted to the right.
// to make room for the inserted element/s.
// elmt/s can be NULL and a pointer to uninitialized memory is returned
// returns a pointer to the index position.
#define DasStk_insert(stk, idx, elmt) ({ _das_assert_type((stk)->DasStk_data, elmt); ((typeof((stk)->DasStk_data))_DasStk_insert_many((_DasStk*)stk, idx, elmt, 1, sizeof(*(stk)->DasStk_data), alignof(typeof(*(stk)->DasStk_data)))); })
#define _DasStk_insert(stk, idx, elmt, elmt_size, elmt_align) _DasStk_insert_many((_DasStk*)stk, idx, elmt, 1, elmt_size, elmt_align)
#define DasStk_insert_many(stk, idx, elmts, elmts_count) ({ _das_assert_type((stk)->DasStk_data, elmts); ((typeof((stk)->DasStk_data))_DasStk_insert_many((_DasStk*)stk, idx, elmts, elmts_count, sizeof(*(stk)->DasStk_data), alignof(typeof(*(stk)->DasStk_data)))); })
extern void* _DasStk_insert_many(_DasStk* stk, uint32_t idx, void* elmts, uint32_t elmts_count, uint32_t elmt_size, uint32_t elmt_align);

// pushes element/s onto the end of the stack
// elmt/s can be NULL and a pointer to uninitialized memory is returned
// returns a pointer to the first element that was added to the stack.
#define DasStk_push(stk, elmt) ({ _das_assert_type((stk)->DasStk_data, elmt); ((typeof((stk)->DasStk_data))_DasStk_push_many((_DasStk*)stk, elmt, 1, sizeof(*(stk)->DasStk_data), alignof(typeof(*(stk)->DasStk_data)))); })
#define _DasStk_push(stk, elmt, elmt_size, elmt_align) _DasStk_push_many((_DasStk*)stk, elmt, 1, elmt_size, elmt_align)
#define DasStk_push_many(stk, elmts, elmts_count) ({ _das_assert_type((stk)->DasStk_data, elmts); ((typeof((stk)->DasStk_data))_DasStk_push_many((_DasStk*)stk, elmts, elmts_count, sizeof(*(stk)->DasStk_data), alignof(typeof(*(stk)->DasStk_data)))); })
extern void* _DasStk_push_many(_DasStk* stk, void* elmts, uint32_t elmts_count, uint32_t elmt_size, uint32_t elmt_align);

// removes element/s from the end of the stack
// returns the number of elements popped from the stack.
// elements will be copied to elmts_out unless it is NULL.
#define DasStk_pop(stk, elmt_out) ({ _das_assert_type((stk)->DasStk_data, elmt_out); _DasStk_pop_many((_DasStk*)stk, elmt_out, 1, sizeof(*(stk)->DasStk_data)); })
#define _DasStk_pop(stk, elmt_out, elmt_size) _DasStk_pop_many((_DasStk*)stk, elmt_out, 1, elmt_size)
#define DasStk_pop_many(stk, elmts_out, elmts_count) ({ _das_assert_type((stk)->DasStk_data, elmts_out); _DasStk_pop_many((_DasStk*)stk, elmts_out, elmts_count, sizeof(*(stk)->DasStk_data)); })
extern uint32_t _DasStk_pop_many(_DasStk* stk, void* elmts_out, uint32_t elmts_count, uint32_t elmt_size);

// removes element/s from the start_idx up to end_idx (exclusively).
// elements at the end of the stack are moved to replace the removed elements.
// the original elements will be copied to elmts_out unless it is NULL.
#define DasStk_swap_remove(stk, idx, elmt_out) ({ _das_assert_type((stk)->DasStk_data, elmt_out); _DasStk_swap_remove_range((_DasStk*)stk, idx, (idx) + 1, elmt_out, sizeof(*(stk)->DasStk_data)); })
#define _DasStk_swap_remove(stk, idx, elmt_out, elmt_size) _DasStk_swap_remove_range((_DasStk*)stk, idx, (idx) + 1, elmt_out, elmt_size)
#define DasStk_swap_remove_range(stk, start_idx, end_idx, elmts_out) ({ _das_assert_type((stk)->DasStk_data, elmts_out); _DasStk_swap_remove_range((_DasStk*)stk, start_idx, end_idx, elmts_out, sizeof(*(stk)->DasStk_data)); })
extern void _DasStk_swap_remove_range(_DasStk* stk, uint32_t start_idx, uint32_t end_idx, void* elmts_out, uint32_t elmt_size);

// removes element/s from the start_idx up to end_idx (exclusively).
// elements that come after are shifted to the left to replace the removed elements.
// the original elements will be copied to elmts_out unless it is NULL.
#define DasStk_shift_remove(stk, idx, elmt_out) ({ _das_assert_type((stk)->DasStk_data, elmt_out); _DasStk_shift_remove_range((_DasStk*)stk, idx, (idx) + 1, elmt_out, sizeof(*(stk)->DasStk_data)); })
#define _DasStk_shift_remove(stk, idx, elmt_out, elmt_size) _DasStk_shift_remove_range((_DasStk*)stk, idx, (idx) + 1, elmt_out, elmt_size)
#define DasStk_shift_remove_range(stk, start_idx, end_idx, elmts_out) ({ _das_assert_type((stk)->DasStk_data, elmts_out); _DasStk_shift_remove_range((_DasStk*)stk, start_idx, end_idx, elmts_out, sizeof(*(stk)->DasStk_data)); })
extern void _DasStk_shift_remove_range(_DasStk* stk, uint32_t start_idx, uint32_t end_idx, void* elmts_out, uint32_t elmt_size);

// pushes the formatted string on to the end of a byte stack.
extern void DasStk_push_str_fmt(DasStk(char)* stk, char* fmt, ...);

//
// ================== DasDeque ==================
//
// double ended queue (ring buffer)
// designed to be used for FIFO or LIFO operations.
// elements are allocated using the Dynamic Allocation API that is defined lower down in this file.
//
// can store up to UINT32_MAX of elements (so UINT32_MAX * sizeof(T) bytes).
// this is so the structure is the size of 3 pointers instead of 4 on 64 bit machines.
// this helps keep data in cache that sits along side the deque structure.
// it is unlikely you will need more elements, but if you do.
// you can always copy and paste the code a run a custom solution.
//
// - empty when '_front_idx' == '_back_idx'
// - '_cap' is the number of allocated elements, but the deque can only hold '_cap' - 1.
// 		thi is because the _back_idx needs to point to the next empty element slot.
// - '_front_idx' will point to the item at the front of the queue
// - '_back_idx' will point to the item after the item at the back of the queue
//
// DasDeque API example usage:
//
// DasDeque(int) queue_of_ints;
// DasDeque_init_with_cap(&queue_of_ints, 64);
//
// int value = 22;
// DasDeque_push_back(&queue_of_ints, &value);
//
// DasDeque_pop_front(&queue_of_ints, &value);
#define DasDeque(T) DasDeque_##T
typedef struct {
	void* data;
	uint32_t _cap;
	uint32_t _front_idx;
	uint32_t _back_idx;
} _DasDeque;

//
// you need to make sure that you use this macro in global scope
// to define a structure for the type you want to use.
//
// we have a funky name for the data field,
// so an error will get thrown if you pass the
// wrong structure into one of the macros below.
#define typedef_DasDeque(T) \
typedef struct { \
	T* _DasDeque_data; \
	uint32_t _cap; \
	uint32_t _front_idx; \
	uint32_t _back_idx; \
} DasDeque_##T;

typedef_DasDeque(char);
typedef_DasDeque(short);
typedef_DasDeque(int);
typedef_DasDeque(long);
typedef_DasDeque(float);
typedef_DasDeque(double);
typedef_DasDeque(size_t);
typedef_DasDeque(ptrdiff_t);
typedef_DasDeque(uint8_t);
typedef_DasDeque(uint16_t);
typedef_DasDeque(uint32_t);
typedef_DasDeque(uint64_t);
typedef_DasDeque(int8_t);
typedef_DasDeque(int16_t);
typedef_DasDeque(int32_t);
typedef_DasDeque(int64_t);

#define DasDeque_min_cap 4

// initializes an empty deque with a preallocated capacity of 'cap' elements
#define DasDeque_init_with_cap(deque, cap) _DasDeque_init_with_cap((_DasDeque*)deque, cap, sizeof(*(deque)->_DasDeque_data), alignof(typeof(*(deque)->_DasDeque_data)))
extern void _DasDeque_init_with_cap(_DasDeque* deque, uint32_t cap, uint32_t elmt_size, uint32_t elmt_align);

// deallocates the memory and sets the deque to being empty.
#define DasDeque_deinit(deque) _DasDeque_deinit((_DasDeque*)deque, sizeof(*(deque)->_DasDeque_data), alignof(typeof(*(deque)->_DasDeque_data)))
extern void _DasDeque_deinit(_DasDeque* deque, uint32_t elmt_size, uint32_t elmt_align);

// returns the number of elements
#define DasDeque_count(deque) ((deque)->_back_idx >= (deque)->_front_idx ? (deque)->_back_idx - (deque)->_front_idx : (deque)->_back_idx + ((deque)->_cap - (deque)->_front_idx))

// returns the number of elements that can be stored in the queue before a reallocation is required.
#define DasDeque_cap(deque) ((deque)->_cap == 0 ? 0 : (deque)->_cap - 1)

// removes all the elements from the deque
#define DasDeque_clear(deque) (deque)->_front_idx = (deque)->_back_idx

// returns a pointer to the element at idx.
// this function will abort if idx is out of bounds
#define DasDeque_get(deque, idx) ((typeof((deque)->_DasDeque_data))_DasDeque_get((_DasDeque*)deque, idx, sizeof(*(deque)->_DasDeque_data)))
extern void* _DasDeque_get(_DasDeque* deque, uint32_t idx, uint32_t elmt_size);

// returns a pointer to the first element
// this function will abort if the deque is empty
#define DasDeque_get_first(deque) ((typeof((deque)->_DasDeque_data))_DasDeque_get((_DasDeque*)deque, 0, sizeof(*(deque)->_DasDeque_data)))
//
// returns a pointer to the last element
// this function will abort if the deque is empty
#define DasDeque_get_last(deque) ((typeof((deque)->_DasDeque_data))_DasDeque_get((_DasDeque*)deque, DasDeque_count(deque) - 1, sizeof(*(deque)->_DasDeque_data)))

// reallocates the capacity of the data to have enough room for new_cap number of elements.
// if new_cap is less than (deque)->count then new_cap is set to (deque)->count.
#define DasDeque_resize_cap(deque, cap) _DasDeque_resize_cap((_DasDeque*)deque, cap, sizeof(*(deque)->_DasDeque_data), alignof(typeof(*(deque)->_DasDeque_data)))
extern void _DasDeque_resize_cap(_DasDeque* deque, uint32_t cap, uint32_t elmt_size, uint32_t elmt_align);

// pushes element/s at the front of the deque
// elmt/s can be NULL and the new elements in the deque will be uninitialized.
#define DasDeque_push_front(deque, elmt) ({ _das_assert_type((deque)->_DasDeque_data, elmt); _DasDeque_push_front_many((_DasDeque*)deque, elmt, 1, sizeof(*(deque)->_DasDeque_data), alignof(typeof(*(deque)->_DasDeque_data))); })
#define _DasDeque_push_front(deque, elmt, elmt_size, elmt_align) _DasDeque_push_front_many((_DasDeque*)deque, elmt, 1, elmt_size, elmt_align)
#define DasDeque_push_front_many(deque, elmts, elmts_count) ({ _das_assert_type((deque)->_DasDeque_data, elmts); _DasDeque_push_front_many((_DasDeque*)deque, elmts, elmts_count, sizeof(*(deque)->_DasDeque_data), alignof(typeof(*(deque)->_DasDeque_data))); })
extern void _DasDeque_push_front_many(_DasDeque* deque, void* elmts, uint32_t elmts_count, uint32_t elmt_size, uint32_t elmt_align);

// pushes element/s at the back of the deque
// elmt/s can be NULL and the new elements in the deque will be uninitialized.
#define DasDeque_push_back(deque, elmt) ({ _das_assert_type((deque)->_DasDeque_data, elmt); _DasDeque_push_back_many((_DasDeque*)deque, elmt, 1, sizeof(*(deque)->_DasDeque_data), alignof(typeof(*(deque)->_DasDeque_data))); })
#define _DasDeque_push_back(deque, elmt, elmt_size, elmt_align) _DasDeque_push_back_many((_DasDeque*)deque, elmt, 1, elmt_size, elmt_align)
#define DasDeque_push_back_many(deque, elmts, elmts_count) ({ _das_assert_type((deque)->_DasDeque_data, elmts); _DasDeque_push_back_many((_DasDeque*)deque, elmts, elmts_count, sizeof(*(deque)->_DasDeque_data), alignof(typeof(*(deque)->_DasDeque_data))); })
extern void _DasDeque_push_back_many(_DasDeque* deque, void* elmts, uint32_t elmts_count, uint32_t elmt_size, uint32_t elmt_align);

// removes element/s from the front of the deque
// returns the number of elements popped from the deque.
// elements will be copied to elmts_out unless it is NULL.
#define DasDeque_pop_front(deque, elmt_out) ({ _das_assert_type((deque)->_DasDeque_data, elmt_out); _DasDeque_pop_front_many((_DasDeque*)deque, elmt_out, 1, sizeof(*(deque)->_DasDeque_data)); })
#define _DasDeque_pop_front(deque, elmt_out, elmt_size) _DasDeque_pop_front_many((_DasDeque*)deque, elmt_out, 1, elmt_size)
#define DasDeque_pop_front_many(deque, elmts_out, elmts_count) ({ _das_assert_type((deque)->_DasDeque_data, elmts_out); _DasDeque_pop_front_many((_DasDeque*)deque, elmts_out, elmts_count, sizeof(*(deque)->_DasDeque_data)); })
extern uint32_t _DasDeque_pop_front_many(_DasDeque* deque, void* elmts_out, uint32_t elmts_count, uint32_t elmt_size);

// removes element/s from the back of the deque
// returns the number of elements popped from the deque.
// elements will be copied to elmts_out unless it is NULL.
#define DasDeque_pop_back(deque, elmt_out) ({ _das_assert_type((deque)->_DasDeque_data, elmt_out); _DasDeque_pop_back_many((_DasDeque*)deque, elmt_out, 1, sizeof(*(deque)->_DasDeque_data)); })
#define _DasDeque_pop_back(deque, elmt_out, elmt_size) _DasDeque_pop_back_many((_DasDeque*)deque, elmt_out, 1, elmt_size)
#define DasDeque_pop_back_many(deque, elmts_out, elmts_count) ({ _das_assert_type((deque)->_DasDeque_data, elmts_out); _DasDeque_pop_back_many((_DasDeque*)deque, elmts_out, elmts_count, sizeof(*(deque)->_DasDeque_data)); })
extern uint32_t _DasDeque_pop_back_many(_DasDeque* deque, void* elmts_out, uint32_t elmts_count, uint32_t elmt_size);

//
// ================== Dynamic Allocation API ==================
//
// these macros will de/allocate a memory from the thread's allocator.
// memory will be uninitialized unless the allocator you have zeros the memory for you.
//
// if you allocate some memory, and you want deallocate or expand the memory.
// it is up to you provide the correct 'old_size' that you allocated the memory with in the first place.
// unless you know that the allocator you are using does not care about that.
//
// arguments for the 'align' parameters, must be a power of two and greater than 0.
//

// allocates memory that can hold a single element of type T.
// returns an appropriately aligned pointer to that allocated memory location.
#define das_alloc_elmt(T) (T*)das_alloc(sizeof(T), alignof(T))

// deallocates memory that can hold a single element of type T.
#define das_dealloc_elmt(ptr) das_dealloc(ptr, sizeof(*(ptr)), alignof(typeof(*(ptr))))

// allocates memory that can hold an array of 'count' number of T elements.
// returns an appropriately aligned pointer to that allocated memory location.
#define das_alloc_array(T, count) (T*)das_alloc((count) * sizeof(T), alignof(T))

// reallocates memory that can hold an array of 'old_count' to 'count' number of T elements.
// the memory of ptr[0] up to ptr[old_size] is preserved.
// returns a pointer to allocated memory with enough bytes to store a single element of type T.
#define das_realloc_array(ptr, old_count, count) \
	(typeof(ptr))das_realloc(ptr, (old_count) * sizeof(*(ptr)), (count) * sizeof(*(ptr)), alignof(typeof(*(ptr))))

// deallocates memory that can hold an array of 'old_count' number of T elements.
#define das_dealloc_array(ptr, old_count) das_dealloc(ptr, (old_count) * sizeof(*(ptr)), alignof(typeof(*(ptr))))

// allocates 'size' bytes of memory that is aligned to 'align'
#define das_alloc(size, align) das_realloc(NULL, 0, size, align)

// reallocates 'old_size' to 'size' bytes of memory that is aligned to 'align'
// the memory of 'ptr' up to ('ptr' + 'old_size') is preserved.
// if 'ptr' == NULL, then this will call the alloc function for current allocator instead
extern void* das_realloc(void* ptr, uintptr_t old_size, uintptr_t size, uintptr_t align);

// deallocates 'old_size' bytes of memory that is aligned to 'align'
extern void das_dealloc(void* ptr, uintptr_t old_size, uintptr_t align);

//
// ================== Custom Allocator API ==================
//

//
// helper functions when creating allocators:
//
extern void* das_ptr_round_up_align(void* ptr, uintptr_t align);
extern void* das_ptr_round_down_align(void* ptr, uintptr_t align);
#define das_ptr_add(ptr, by) (void*)((uintptr_t)(ptr) + (uintptr_t)(by))
#define das_ptr_sub(ptr, by) (void*)((uintptr_t)(ptr) - (uintptr_t)(by))
#define das_ptr_diff(to, from) ((char*)(to) - (char*)(from))

// for X86/64 and ARM. maybe be different for other architectures.
#define das_cache_line_size 64

// an allocation function for allocating, reallocating and deallocating memory.
// if !ptr -> allocate
// if ptr && size > 0 -> reallocate
// else -> deallocate
//
// returns NULL on allocation failure
typedef void* (*DasAllocFn)(void* data, void* ptr, uintptr_t old_size, uintptr_t size, uintptr_t align);

//
// DasAlctor is the custom allocator data structure.
// is used to point to the implementation of a custom allocator.
//
// EXAMPLE of a local alloctor:
//
// typedef struct {
//     void* data;
//     uint32_t pos;
//     uint32_t size;
// } LinearAlctor;
//
//     ... see das_test.c for full implementation ...
// void* LinearAlctor_alloc(void* alctor, void* ptr, uintptr_t old_size, uintptr_t size, uintptr_t align);
// void* LinearAlctor_realloc(void* alctor, void* ptr, uintptr_t old_size, uintptr_t size, uintptr_t align);
// void* LinearAlctor_dealloc(void* alctor, void* ptr, uintptr_t old_size, uintptr_t size, uintptr_t align);
//
// DasAllocFn LinearAlctor_fns[3] = {
//     LinearAlctor_alloc,
//     LinearAlctor_realloc,
//     LinearAlctor_dealloc,
// };
//
// // we zero the buffer so this allocator will allocate zeroed memory.
// char buffer[1024] = {0};
// LinearAlctor linear_alctor = { .data = buffer, .pos = 0, .size = sizeof(buffer) };
// DasAlctor alctor = { .data = &linear_alctor, .fn = LinearAlctor_fns };
//
typedef struct {
	// points to an array of 3 allocator functions in the following order:
	//     - alloc
	//     - realloc
	//     - dealloc
	//
	DasAllocFn* fns;

	// the data is an optional pointer used to point to an allocator's data structure.
	// it is passed into the DasAllocFn as the first argument.
	// this field can be NULL if you are implementing a global allocator.
	void* data;
} DasAlctor;

//
// sets the thread's allocator and returns the old one.
// can be used to swap the allocator for some allocations
// and then restore back to the original one.
//
// EXAMPLE:
//
// DasAlctor old_alctor = das_swap_alctor(new_alctor);
// int* int_ptr = das_alloc_elmt(int);
// float* float_ptr = das_alloc_elmt(float);
// das_swap_alctor(old_alctor);
//
extern DasAlctor das_swap_alctor(DasAlctor alctor);

#ifndef DAS_NO_SYSTEM_ALCTOR
//
// if you do not want a system allocator, then #define DAS_NO_SYSTEM_ALCTOR.
// you then want to define this somewhere, to default initialize all threads
// #define DAS_DEFAULT_ALCTOR { .fns = ..., .data = ... }
extern DasAllocFn das_system_alctor_fns[3];
#endif

//
// the out of memory handler is called when the das_{alloc, realloc, alloc_elmt} fails.
// this is to give the application a chance to free some memory or change the allocator.
// return das_true if you have resolved the problem and the allocation will try one more time.
// return das_false on failure, will print an error to stderr and then abort.
typedef DasBool (*DasOutOfMemHandlerFn)(void* data, DasAlctor* alctor, uintptr_t size);

//
// for implementation a out of memory handler, see the documentation for DasOutOfMemHandlerFn.
// applications should idealy be the only thing providing out of memory handlers.
//
typedef struct {
	DasOutOfMemHandlerFn fn;
	void* data;
} DasOutOfMemHandler;

//
// sets the thread's out of memory handler and returns the old one.
// can be used to swap the out of memory handler for some allocations
// and then restore back to the original one.
//
extern DasOutOfMemHandler das_swap_out_of_mem_handler(DasOutOfMemHandler handler);

#ifdef __cplusplus
}
#endif

#endif

