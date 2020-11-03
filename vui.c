
//
// =============================================
//
// internal
//
// =============================================
//

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
	alctor->arenas_head = das_alloc(_VuiArenaAlctor_arena_size, 1);
	((_VuiArenaHeader*)alctor->arenas_head)->next = NULL;
}

void _VuiArenaAlctor_reset(_VuiArenaAlctor* alctor) {
	alctor->arena = alctor->arenas_head;
	alctor->pos = sizeof(_VuiArenaHeader);
	memset(alctor->arena + alctor->pos, 0, _VuiArenaAlctor_arena_size - alctor->pos);
}

void* _VuiArenaAlctor_alloc(_VuiArenaAlctor* alctor, uintptr_t size, uintptr_t align) {
	while (1) {
		// an allocation gets the pointer by adding the position to the start of the buffer.
		void* ptr = das_ptr_add(alctor->arena, alctor->pos);
		// rounds up the pointer so it aligned as requested.
		ptr = das_ptr_round_up_align(ptr, align);
		uint32_t next_pos = das_ptr_diff(ptr, alctor->arena) + size;
		// checks to see if it fits in the linear buffer.
		if (next_pos <= _VuiArenaAlctor_arena_size) {
			// and just increments the position for the next allocation.
			alctor->pos = next_pos;
			return ptr;
		} else {
			// allocate a new arena
			void* arena = das_alloc(_VuiArenaAlctor_arena_size, 1);
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
#define _VuiInputBoxType_text 0
#define _VuiInputBoxType_float 1
#define _VuiInputBoxType_u32 2
#define _VuiInputBoxType_s32 3

typedef struct {
    _VuiMouse mouse;
    VuiInputActions actions;
	DasStk(char) input_box_string;
	DasStk(char) input_box_edit_string;
	DasStk(char)* text_box_string;
	uint32_t text_box_cursor_idx;
	int32_t text_box_select_offset;
	_VuiInputBoxType text_box_type;
	VuiBool has_changed;
	VuiBool is_mouse_over_ctrl;
} _VuiInput;

typedef struct {
	VuiVec2 size;
    VuiCtrlIdHash focused_ctrl_id_hash;
	DasStk(VuiCtrl) ctrls;
	DasStk(char) text;
	VuiRenderWindow render;
} _VuiWindow;

typedef struct {
	uint32_t ctrl_idx;
	uint32_t children_ctrl_end_idx;
	VuiAlign align;
	VuiVec2 offset;
} _VuiStackChild;
typedef_DasStk(_VuiStackChild);

typedef struct {
	_VuiWindow* w;
	uint32_t parent_ctrl_idx;
	uint32_t render_ctrl_idx;
	uint32_t render_layer_idx;
	uint32_t render_text_idx;
	VuiRect render_clip_rect;
	DasStk(_VuiStackChild) stack_children;
	DasStk(VuiVec2) path_points;
	VuiVec2 last_ctrl_top_left;
	VuiVec2 last_ctrl_size;
} _VuiBuild;

typedef struct {
	VuiPositionTextFn position_text_fn;
	void* position_text_userdata;
	VuiTextBoxFocusChange text_box_focus_change_fn;
	_VuiInput input;
	_VuiWindow* windows;
	uint16_t windows_count;
	_VuiBuild build;
	VuiWindowId mouse_focused_window_id;
	VuiWindowId focused_window_id;
    VuiCtrlIdHash scroll_mouse_focused_ctrl_id_hash;
    VuiCtrlIdHash mouse_focused_ctrl_id_hash;
	_VuiArenaAlctor frame_data_alctor;
} _Vui;

_Vui _vui = {0};

//
// =============================================
//
// general
//
// =============================================
//

VuiBool VuiVec2_has_auto(VuiVec2 v) { return v.x == vui_auto_len || v.y == vui_auto_len; }
VuiBool VuiVec2_has_fill(VuiVec2 v) { return v.x == vui_fill_len || v.y == vui_fill_len; }
VuiVec2 VuiVec2_add(VuiVec2 a, VuiVec2 b) { return VuiVec2_init(a.x + b.x, a.y + b.y); }
VuiVec2 VuiVec2_sub(VuiVec2 a, VuiVec2 b) { return VuiVec2_init(a.x - b.x, a.y - b.y); }
VuiVec2 VuiVec2_mul(VuiVec2 a, VuiVec2 b) { return VuiVec2_init(a.x * b.x, a.y * b.y); }
VuiVec2 VuiVec2_div(VuiVec2 a, VuiVec2 b) { return VuiVec2_init(a.x / b.x, a.y / b.y); }
float VuiVec2_len(VuiVec2 v) { return sqrtf((v.x * v.x) + (v.y * v.y)); }
VuiVec2 VuiVec2_scale(VuiVec2 v, float by) { return VuiVec2_init(v.x * by, v.y * by); }
VuiVec2 VuiVec2_norm(VuiVec2 v) {
	float k = 1.0 / sqrtf((v.x * v.x) + (v.y * v.y));
	return VuiVec2_init(v.x * k, v.y * k);
}
VuiVec2 VuiVec2_perp_left(VuiVec2 v) { return VuiVec2_init(v.y, -v.x); }
VuiVec2 VuiVec2_perp_right(VuiVec2 v) { return VuiVec2_init(-v.y, v.x); }

VuiVec2 VuiVec2_min(VuiVec2 a, VuiVec2 b) { return VuiVec2_init(vui_min(a.x, b.x), vui_min(a.y, b.y)); }
VuiVec2 VuiVec2_max(VuiVec2 a, VuiVec2 b) { return VuiVec2_init(vui_max(a.x, b.x), vui_max(a.y, b.y)); }
VuiVec2 VuiVec2_clamp(VuiVec2 v, VuiVec2 min, VuiVec2 max) {
	return VuiVec2_init(vui_clamp(v.x, min.x, max.x), vui_clamp(v.y, min.y, max.y));
}

VuiVec2 VuiRect_bottom_left(VuiRect rect) {
    return VuiVec2_init(rect.top_left.x, rect.bottom_right.y);
}

VuiVec2 VuiRect_top_right(VuiRect rect) {
    return VuiVec2_init(rect.bottom_right.x, rect.top_left.y);
}

VuiVec2 VuiRect_size(VuiRect rect) {
    return VuiVec2_init(vui_max(0.0, rect.bottom_right.x - rect.top_left.x), vui_max(0.0, rect.bottom_right.y - rect.top_left.y));
}

VuiBool VuiRect_is_zero(VuiRect rect) {
    return rect.top_left.x == rect.bottom_right.x || rect.top_left.y == rect.bottom_right.y;
}

VuiRect VuiRect_clip(VuiRect a, VuiRect b) {
    if (
        a.top_left.x >= b.bottom_right.x || a.top_left.y >= b.bottom_right.y ||
        b.top_left.x >= a.bottom_right.x || b.top_left.y >= a.bottom_right.y
    ) {
        return VuiRect_zero;
    }

    return (VuiRect) { VuiVec2_max(a.top_left, b.top_left), VuiVec2_min(a.bottom_right, b.bottom_right) };
}

VuiVec2 VuiRect_clip_pt(VuiRect rect, VuiVec2 pt) {
	return VuiVec2_clamp(pt, rect.top_left, rect.bottom_right);
}

VuiBool VuiRect_intersects(VuiRect a, VuiRect b) {
    return a.top_left.x < b.bottom_right.x &&
        b.top_left.x < a.bottom_right.x &&
        a.top_left.y < b.bottom_right.y &&
        b.top_left.y < a.bottom_right.y;
}

VuiBool VuiRect_intersects_pt(VuiRect r, VuiVec2 pt) {
	return r.top_left.x <= pt.x && r.bottom_right.x >= pt.x &&
		r.top_left.y <= pt.y && r.bottom_right.y >= pt.y;
}

//
// =============================================
//
// input
//
// =============================================
//

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

void vui_input_add_text(char* string, uint32_t string_length) {
	// ignore if a text box is not focused
	if (_vui.input.text_box_string == NULL) return;

	_vui.input.has_changed = vui_true;
	if (_vui.input.text_box_select_offset) {
		uint32_t start_idx = 0;
		uint32_t end_idx = 0;
		if (_vui.input.text_box_select_offset > 0) {
			start_idx = _vui.input.text_box_cursor_idx;
			end_idx = _vui.input.text_box_cursor_idx + _vui.input.text_box_select_offset;
		} else {
			start_idx = _vui.input.text_box_cursor_idx + _vui.input.text_box_select_offset;
			end_idx = _vui.input.text_box_cursor_idx;
		}

		DasStk_shift_remove_range(_vui.input.text_box_string, start_idx, end_idx, NULL);
		_vui.input.text_box_cursor_idx = start_idx;
		_vui.input.text_box_select_offset = 0;
	}

	uint32_t copied_count = 0;
	for (uint32_t i = 0; i < string_length; i += 1) {
		char ch = string[i];
		VuiBool copy = vui_false;
		switch (_vui.input.text_box_type) {
			case _VuiInputBoxType_text: copy = vui_true; break;
			case _VuiInputBoxType_float:
				copy = ch >= '0' && ch <= '9';
				if (!copy && ch == '.') {
					VuiBool has_full_stop = vui_false;
					for (uint32_t idx = 0; idx < _vui.input.text_box_string->count; idx += 1) {
						if (_vui.input.text_box_string->DasStk_data[idx] == '.') {
							has_full_stop = vui_true;
							break;
						}
					}

					copy = !has_full_stop && _vui.input.text_box_string->count > 0;
				} else if (!copy && ch == '-') {
					copy = _vui.input.text_box_string->count == 0 ||
						(_vui.input.text_box_cursor_idx == 0 && _vui.input.text_box_string->DasStk_data[0] != '-');
				}
				break;
			case _VuiInputBoxType_u32:
				copy = ch >= '0' && ch <= '9';
				break;
			case _VuiInputBoxType_s32:
				copy = ch >= '0' && ch <= '9';
				if (ch == '-') {
					copy = _vui.input.text_box_string->count == 0 ||
						(_vui.input.text_box_cursor_idx == 0 && _vui.input.text_box_string->DasStk_data[0] != '-');
				}
				break;
		}

		if (copy) {
			DasStk_insert(_vui.input.text_box_string, _vui.input.text_box_cursor_idx, &string[i]);
			copied_count += 1;
		}
	}

	_vui.input.text_box_cursor_idx += copied_count;
}

VuiBool vui_has_mouse_over_ctrl() {
	return _vui.input.is_mouse_over_ctrl;
}

VuiBool vui_has_mouse_focused_ctrl() {
	return _vui.mouse_focused_ctrl_id_hash != 0;
}

VuiBool vui_has_scroll_mouse_focused_ctrl() {
	return _vui.scroll_mouse_focused_ctrl_id_hash != 0;
}

VuiBool vui_ctrl_is_mouse_focused(VuiCtrlIdHash id_hash) {
	return _vui.mouse_focused_ctrl_id_hash == id_hash;
}

VuiBool vui_ctrl_is_focused(VuiCtrlIdHash id_hash) {
	return _vui.windows[_vui.focused_window_id].focused_ctrl_id_hash == id_hash;
}

VuiBool vui_ctrl_is_scroll_mouse_focused(VuiCtrlIdHash id_hash) {
	return _vui.scroll_mouse_focused_ctrl_id_hash == id_hash;
}

void vui_ctrl_set_focused(VuiCtrlIdHash id_hash) {
	_vui.windows[_vui.focused_window_id].focused_ctrl_id_hash = id_hash;
	if (_vui.input.text_box_string) {
		_vui.text_box_focus_change_fn(vui_false);
		_vui.input.text_box_string = NULL;
	}
}

VuiFocusState vui_ctrl_focus_state(VuiCtrlIdHash id_hash) {
	//
	// keyboard state
	//

	if ((_vui.input.actions & VuiInputActions_focus_pressed) == VuiInputActions_focus_pressed) {
		if (vui_ctrl_is_focused(id_hash)) {
			return VuiFocusState_pressed;
		}
	}

	if ((_vui.input.actions & VuiInputActions_focus_held) == VuiInputActions_focus_held) {
		if (vui_ctrl_is_focused(id_hash)) {
			return VuiFocusState_held;
		}
	}

	if ((_vui.input.actions & VuiInputActions_focus_released) == VuiInputActions_focus_released) {
		if (vui_ctrl_is_focused(id_hash)) {
			return VuiFocusState_released;
		}
	}

	//
	// mouse state
	//

    VuiBool is_ctrl_mouse_focused = vui_ctrl_is_mouse_focused(id_hash);
    if ((_vui.input.mouse.buttons_has_been_pressed & VuiMouseButtons_left) == VuiMouseButtons_left) {
        if (is_ctrl_mouse_focused) {
            vui_ctrl_set_focused(id_hash);
			if ((_vui.input.mouse.buttons_has_been_released & VuiMouseButtons_left) == VuiMouseButtons_left) {
				return VuiFocusState_released;
			}
            return VuiFocusState_pressed;
        } else {
            if (vui_ctrl_is_focused(id_hash)) {
                vui_ctrl_set_focused(0);
            }
        }
    }

    if ((_vui.input.mouse.buttons_is_pressed & VuiMouseButtons_left) == VuiMouseButtons_left) {
        if (vui_ctrl_is_focused(id_hash)) {
            return VuiFocusState_held;
        }
    }

    if ((_vui.input.mouse.buttons_has_been_released & VuiMouseButtons_left) == VuiMouseButtons_left) {
        if (vui_ctrl_is_mouse_focused(id_hash)) {
            return VuiFocusState_released;
        }
    }

    if (is_ctrl_mouse_focused) {
        return VuiFocusState_focused;
    } else {
        return VuiFocusState_none;
    }
}

VuiVec2 vui_ctrl_get_last_top_left() {
	return _vui.build.last_ctrl_top_left;
}

VuiVec2 vui_ctrl_get_last_size() {
	return _vui.build.last_ctrl_size;
}

//
// =============================================
//
// resources
//
// =============================================
//


float VuiThickness_horizontal(VuiThickness thickness) { return thickness.left + thickness.right; }
float VuiThickness_vertical(VuiThickness thickness) { return thickness.top + thickness.bottom; }
VuiVec2 VuiThickness_size(VuiThickness thickness) {
    return VuiVec2_init(VuiThickness_horizontal(thickness), VuiThickness_vertical(thickness));
}

//
// =============================================
//
// render
//
// =============================================
//

void vui_render_line(VuiVec2 start_pos, VuiVec2 end_pos, VuiColor color, float width) {
	vui_path_plot_point(start_pos);
	vui_path_plot_point(end_pos);
	vui_render_path_stroked(color, width, vui_false);
}

void vui_render_rect(VuiRect rect, VuiColor color, float radius) {
	vui_path_plot_rect(rect, radius);
	vui_render_path_filled_convex(color);
}

void vui_render_rect_border(VuiRect rect, VuiColor color, float radius, float width) {
	float half_width = width / 2.0;
	rect.top_left.x += half_width;
	rect.top_left.y += half_width;
	rect.bottom_right.x -= half_width;
	rect.bottom_right.y -= half_width;
	vui_path_plot_rect(rect, radius);
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

void vui_render_text(VuiVec2 pos, VuiFont* font, char* text, uint32_t text_byte_count, VuiColor color, float wrap_at_width) {

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

	VuiRect clip_rect = _vui.build.render_clip_rect;
	for (uint32_t idx = 0; idx < points_count; idx += 1) {
		VuiRenderVert* vert = &w.verts[idx];
		vert->pos = VuiRect_clip_pt(clip_rect, points[idx]);
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


void vui_render_image(VuiTextureId texture_id, VuiRect rect, VuiRect uv_rect, VuiColor color) {
	vui_render_image_(texture_id, rect, uv_rect, color, vui_false);
}

void vui_render_image_(VuiTextureId texture_id, VuiRect rect, VuiRect uv_rect, VuiColor color, VuiBool is_glyph) {
	VuiRenderWriter w = vui_render_get_writer(texture_id, 4, 6);
	VuiRect clipped_rect = VuiRect_clip(rect, _vui.build.render_clip_rect);

	{
		//
		// clip the side of the uv coordiates by the same ratio the rectangle got clipped
		float width = rect.bottom_right.x - rect.top_left.x;
		float uv_width = uv_rect.bottom_right.x - uv_rect.top_left.x;
		float w_ratio = uv_width / width;

		float height = rect.bottom_right.y - rect.top_left.y;
		float uv_height = uv_rect.bottom_right.y - uv_rect.top_left.y;
		float h_ratio = uv_height / height;

		// left
		float diff = clipped_rect.top_left.x - rect.top_left.x;
		if (diff > 0.0) uv_rect.top_left.x += diff * w_ratio;

		// right
		diff = rect.bottom_right.x - clipped_rect.bottom_right.x;
		if (diff > 0.0) uv_rect.bottom_right.x -= diff * w_ratio;

		// top
		diff = clipped_rect.top_left.y - rect.top_left.y;
		if (diff > 0.0) uv_rect.top_left.y += diff * h_ratio;

		// bottom
		diff = rect.bottom_right.y - clipped_rect.bottom_right.y;
		if (diff > 0.0) uv_rect.bottom_right.y -= diff * h_ratio;
	}

	if (is_glyph) {
		uv_rect.top_left.x = -uv_rect.top_left.x;
		uv_rect.top_left.y = -uv_rect.top_left.y;
		uv_rect.bottom_right.x = -uv_rect.bottom_right.x;
		uv_rect.bottom_right.y = -uv_rect.bottom_right.y;
	}

	w.verts[0] = (VuiRenderVert){
		.pos = clipped_rect.top_left,
		.uv = uv_rect.top_left,
		.color = color,
	};

	w.verts[1] = (VuiRenderVert){
		.pos = VuiVec2_init(clipped_rect.bottom_right.x, clipped_rect.top_left.y),
		.uv = VuiVec2_init(uv_rect.bottom_right.x, uv_rect.top_left.y),
		.color = color,
	};

	w.verts[2] = (VuiRenderVert){
		.pos = clipped_rect.bottom_right,
		.uv = uv_rect.bottom_right,
		.color = color,
	};

	w.verts[3] = (VuiRenderVert){
		.pos = VuiVec2_init(clipped_rect.top_left.x, clipped_rect.bottom_right.y),
		.uv = VuiVec2_init(uv_rect.top_left.x, uv_rect.bottom_right.y),
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
	_vui.build.path_points.count = 0;
}

void vui_path_plot_point(VuiVec2 pt) {
	DasStk_push(&_vui.build.path_points, &pt);
}

void vui_path_plot_arc(VuiVec2 pt, float radius, float angle_start, float angle_end, uint32_t segments_count) {
	uint32_t points_count = segments_count + 1;
	VuiVec2* points = DasStk_push_many(&_vui.build.path_points, NULL, points_count);

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

void vui_path_plot_rect(VuiRect rect, float radius) {
	float w = (rect.bottom_right.x - rect.top_left.x) / 2.0;
	float h = (rect.bottom_right.y - rect.top_left.y) / 2.0;
	radius = vui_min(radius, vui_min(w, h));

	if (radius == 0.0) {
		vui_path_plot_point(rect.top_left);
		vui_path_plot_point(VuiRect_top_right(rect));
		vui_path_plot_point(rect.bottom_right);
		vui_path_plot_point(VuiRect_bottom_left(rect));
	} else {
		float top = M_PI / 2.0;
		float right = M_PI * 2.0;
		float bottom = (M_PI * 3.0) / 2.0;
		float left = M_PI;
		uint32_t segments_count = vui_calc_circle_segments_count(radius);

		vui_path_plot_arc(VuiVec2_init(rect.top_left.x + radius, rect.top_left.y + radius), radius, left, top, segments_count);
		vui_path_plot_arc(VuiVec2_init(rect.bottom_right.x - radius, rect.top_left.y + radius), radius, top, 0.0, segments_count);
		vui_path_plot_arc(VuiVec2_init(rect.bottom_right.x - radius, rect.bottom_right.y - radius), radius, right, bottom, segments_count);
		vui_path_plot_arc(VuiVec2_init(rect.top_left.x + radius, rect.bottom_right.y - radius), radius, bottom, left, segments_count);
	}
}

void vui_path_plot_circle(VuiVec2 pt, float radius, VuiBool is_border) {
	uint32_t segments_count = vui_calc_circle_segments_count(radius);
	if (is_border) segments_count *= 2;
	float angle_end = M_PI * 2.0f;
	vui_path_plot_arc(pt, radius, 0.0f, angle_end, segments_count);
}

void vui_render_path_stroked(VuiColor color, float width, VuiBool connect_first_and_last) {
	vui_render_polyline(_vui.build.path_points.DasStk_data, _vui.build.path_points.count, color, width, connect_first_and_last);
	vui_path_reset();
}

void vui_render_path_filled_convex(VuiColor color) {
	vui_render_convex_polygon(_vui.build.path_points.DasStk_data, _vui.build.path_points.count, color);
	vui_path_reset();
}

VuiRenderWriter vui_render_get_writer(VuiTextureId texture_id, uint32_t verts_count, uint32_t indices_count) {
	VuiRenderWindow* window = &_vui.build.w->render;
	VuiRenderLayer* layer = DasStk_get(&window->layers, _vui.build.render_layer_idx);

	//
	// here we create a new command if any of these are true:
	// - there are no commands in the layer
	// - the texture_id does not match
	//
	VuiRenderCmd* cmd = layer->cmds.count == 0 ? NULL : DasStk_get_last(&layer->cmds);
	if (!cmd || cmd->texture_id != texture_id) {
		cmd = DasStk_push(&layer->cmds, NULL);
		cmd->texture_id = texture_id;
		cmd->verts_start_idx = layer->verts.count;
		cmd->indices_start_idx = layer->indices.count;
		cmd->indices_count = 0;
	}

	cmd->indices_count += indices_count;

	VuiRenderWriter w = {0};
	w.verts_start_idx = layer->verts.count;
	w.verts = DasStk_push_many(&layer->verts, NULL, verts_count);
	w.indices = DasStk_push_many(&layer->indices, NULL, indices_count);
	return w;
}

void vui_render_inc_layer() {
	_vui.build.render_layer_idx += 1;
	VuiRenderWindow* window = &_vui.build.w->render;
	if (_vui.build.render_layer_idx == window->layers.count) {
		VuiRenderLayer* layer = DasStk_push(&window->layers, NULL);
		*layer = (VuiRenderLayer){0};
	}
}

void vui_render_dec_layer() {
	vui_assert(_vui.build.render_layer_idx > 0, "cannot decrement layer when we are already on layer 0");
	_vui.build.render_layer_idx -= 1;
}


//
// =============================================
//
// controls
//
// =============================================
//

VuiCtrlStyle VuiCtrlStyle_zero = {0};
VuiRowColumnLayoutStyle VuiRowColumnLayoutStyle_zero = {0};
void vui_ctrl_render();
void vui_container_layout_start(VuiCtrlId id, VuiVec2 size, VuiCtrlStyle* style) {
	vui_ctrl_start(id, size, vui_ctrl_render, style);
}

void vui_container_layout_end() {
	vui_ctrl_end();
}

void vui_stack_layout_set_next_pos(VuiAlign align, VuiVec2 offset) {
	VuiCtrl* parent_ctrl = DasStk_get(&_vui.build.w->ctrls, _vui.build.parent_ctrl_idx);
	vui_assert(parent_ctrl->layout.type == VuiLayoutType_stack,
			"attempting to set a stack layout next position for a %s layout", VuiLayoutType_strings[parent_ctrl->layout.type]);
	parent_ctrl->layout.stack.offset = offset;
	parent_ctrl->layout.stack.align = align;
}

void vui_stack_layout_start(VuiCtrlId id, VuiVec2 size, VuiCtrlStyle* style) {
	vui_ctrl_start(id, size, vui_ctrl_render, style);
	VuiCtrl* ctrl = DasStk_get_last(&_vui.build.w->ctrls);
	ctrl->layout.type = VuiLayoutType_stack;
	ctrl->layout.stack.stack_child_start_idx = _vui.build.stack_children.count;
}

void vui_stack_layout_end(VuiVec2 auto_size_extension) {
	VuiCtrl* ctrl = DasStk_get(&_vui.build.w->ctrls, _vui.build.parent_ctrl_idx);
    if (ctrl->rect.bottom_right.x == vui_auto_len) ctrl->max_bottom_right.x += auto_size_extension.x;
    if (ctrl->rect.bottom_right.y == vui_auto_len) ctrl->max_bottom_right.y += auto_size_extension.y;
	vui_ctrl_end();
}

void vui_column_layout_start(VuiCtrlId id, VuiVec2 size, VuiRowColumnLayoutStyle* style) {
	vui_ctrl_start(id, size, vui_ctrl_render, &style->ctrl);
	VuiCtrl* ctrl = DasStk_get_last(&_vui.build.w->ctrls);
	_VuiLayout* l = &ctrl->layout;
	l->row_column.spacing = style->spacing;
	l->row_column.wrap_spacing = style->wrap_spacing;
	l->row_column.wrap = style->wrap;
	VuiThickness* p = &ctrl->style->padding;
	l->row_column.next_ctrl_top_left = VuiVec2_add(ctrl->rect.top_left, VuiVec2_init(p->left, p->top));
	l->type = VuiLayoutType_column;
}

void vui_column_layout_end() {
	vui_ctrl_end();
}

void vui_row_layout_start(VuiCtrlId id, VuiVec2 size, VuiRowColumnLayoutStyle* style) {
	vui_ctrl_start(id, size, vui_ctrl_render, &style->ctrl);
	VuiCtrl* ctrl = DasStk_get_last(&_vui.build.w->ctrls);
	_VuiLayout* l = &ctrl->layout;
	l->row_column.spacing = style->spacing;
	l->row_column.wrap_spacing = style->wrap_spacing;
	l->row_column.wrap = style->wrap;
	VuiThickness* p = &ctrl->style->padding;
	l->row_column.next_ctrl_top_left = VuiVec2_add(ctrl->rect.top_left, VuiVec2_init(p->left, p->top));
	l->type = VuiLayoutType_row;
}

void vui_row_layout_end() {
	vui_ctrl_end();
}

float vui_row_column_remaining_length() {
	VuiCtrl* ctrl = DasStk_get(&_vui.build.w->ctrls, _vui.build.parent_ctrl_idx);
	_VuiLayout* l = &ctrl->layout;
	vui_assert(l->type == VuiLayoutType_row || l->type == VuiLayoutType_column, "can only get remaining length in row or column layouts");

	if (l->type == VuiLayoutType_row) {
		vui_assert(ctrl->rect.bottom_right.y != vui_auto_len, "cannot get the remaining length for a row with auto height");
		return (ctrl->rect.bottom_right.y - ctrl->rect.top_left.y)
			- (l->row_column.next_ctrl_top_left.y - ctrl->rect.top_left.y)
			- ctrl->style->padding.bottom;
	} else {
		vui_assert(ctrl->rect.bottom_right.x != vui_auto_len, "cannot get the remaining length for a column with auto width");
		return (ctrl->rect.bottom_right.x - ctrl->rect.top_left.x)
			- (l->row_column.next_ctrl_top_left.x - ctrl->rect.top_left.x)
			- ctrl->style->padding.right;
	}
}

VuiVec2 vui_row_column_next_ctrl_top_left() {
	VuiCtrl* ctrl = DasStk_get(&_vui.build.w->ctrls, _vui.build.parent_ctrl_idx);
	return ctrl->layout.row_column.next_ctrl_top_left;
}

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

void _vui_find_focused_ctrls() {
	VuiCtrl* ctrls = _vui.build.w->ctrls.DasStk_data;
	uint32_t parent_ctrl_idx = _vui.build.parent_ctrl_idx;


	VuiRect parent_clip_rect = _vui.build.render_clip_rect;

	VuiCtrl* ctrl = &ctrls[_vui.build.render_ctrl_idx];
	VuiThickness* padding = &ctrl->style->padding;
	VuiVec2 mouse_pt = VuiVec2_init(_vui.input.mouse.x, _vui.input.mouse.y);

	if (
		!_vui.input.is_mouse_over_ctrl &&
		_vui.build.render_ctrl_idx > 0 && // is not window root control
		ctrl->layout.type == VuiLayoutType_container
	) {
		_vui.input.is_mouse_over_ctrl = VuiRect_intersects_pt(ctrl->rect, mouse_pt);
	}

	// get the parents inner rect
	VuiRect ctrl_inner_rect = {
		VuiVec2_add(ctrl->rect.top_left, VuiVec2_init(padding->left, padding->top)),
		VuiVec2_sub(ctrl->rect.bottom_right, VuiVec2_init(padding->right, padding->bottom))
	};

	// make the next clip rect by appling the ctrl inner rect to the parent clip rect
	_vui.build.render_clip_rect = VuiRect_clip(ctrl_inner_rect, parent_clip_rect);
	_vui.build.parent_ctrl_idx = _vui.build.render_ctrl_idx;

	// move to the next ctrl, this will either be the first child if one exists.
	// or it'll be the sibling of the ctrl (loop will exit right away).
	_vui.build.render_ctrl_idx += 1;
	while (_vui.build.render_ctrl_idx < _vui.build.w->ctrls.count) {
		VuiCtrl* child_ctrl = &ctrls[_vui.build.render_ctrl_idx];
		// end the loop when all children have been processed
		if (child_ctrl->parent_idx != _vui.build.parent_ctrl_idx) break;

		if (VuiRect_intersects_pt(VuiRect_clip(child_ctrl->rect, _vui.build.render_clip_rect), mouse_pt)) {
			if ((child_ctrl->id_hash & VuiCtrlId_focusable_mask) == VuiCtrlId_focusable_mask) {
				_vui.mouse_focused_ctrl_id_hash = child_ctrl->id_hash;
			} else if ((child_ctrl->id_hash & VuiCtrlId_scrollable_mask) == VuiCtrlId_scrollable_mask) {
				_vui.scroll_mouse_focused_ctrl_id_hash = child_ctrl->id_hash;
			}
		}

		// process children of this child
		_vui_find_focused_ctrls();
	}

	_vui.build.parent_ctrl_idx = parent_ctrl_idx;
	_vui.build.render_clip_rect = parent_clip_rect;
}

VuiCtrlIdHash vui_ctrl_gen_id_hash(VuiCtrlId id) {
	VuiCtrl* parent_ctrl = DasStk_get(&_vui.build.w->ctrls, _vui.build.parent_ctrl_idx);
	//
	// hash the id with its parent's id. this will essentially create a unique id.
	// focusable and scrollables flags are stored at the MSBs.
	// so use the VuiCtrlId_mask to clear those bits from the result and the parent so we dont pass them to the child.
	// then set them if they existed in the id in the first place.
	VuiCtrlId id_hash = vui_fnv_hash_32((char*)&id, sizeof(id), parent_ctrl->id_hash & VuiCtrlId_mask) & VuiCtrlId_mask;
	id_hash |= id & (VuiCtrlId_focusable_mask | VuiCtrlId_scrollable_mask);
	return id_hash;
}

void vui_ctrl_start(VuiCtrlId id, VuiVec2 size, VuiRenderCtrlFn render_ctrl_fn, VuiCtrlStyle* style) {
	VuiCtrl* ctrl = DasStk_push(&_vui.build.w->ctrls, NULL);
    VuiThickness* margin = &style->margin;

	uint32_t parent_ctrl_idx = _vui.build.parent_ctrl_idx;
	VuiCtrl* parent_ctrl = DasStk_get(&_vui.build.w->ctrls, parent_ctrl_idx);

	VuiRect rect = {0};
	VuiThickness* parent_padding = &parent_ctrl->style->padding;
	if (parent_ctrl->layout.type == VuiLayoutType_container) {
		rect.top_left =
			VuiVec2_add(parent_ctrl->rect.top_left,
					VuiVec2_add(VuiVec2_init(parent_padding->left, parent_padding->top),
						VuiVec2_init(margin->left, margin->top)));
	} else if (parent_ctrl->layout.type == VuiLayoutType_stack) {
		rect.top_left = parent_ctrl->rect.top_left;
	} else {
		rect.top_left = VuiVec2_add(parent_ctrl->layout.row_column.next_ctrl_top_left, VuiVec2_init(margin->left, margin->top));
	}

    {
        if (size.x == vui_auto_len) {
            rect.bottom_right.x = vui_auto_len;
        } else if (size.x == vui_fill_len) {
			vui_assert(parent_ctrl->layout.type == VuiLayoutType_container || parent_ctrl->layout.type == VuiLayoutType_stack || (parent_ctrl->layout.type == VuiLayoutType_row && !parent_ctrl->layout.row_column.wrap), "vui_fill_len can only be used in container, stack, or row layouts. the row layout cannot wrap");
			rect.bottom_right.x = parent_ctrl->rect.bottom_right.x - parent_padding->right;
			if (parent_ctrl->layout.type != VuiLayoutType_stack) {
				rect.bottom_right.x -= margin->right;
			}
        } else {
            rect.bottom_right.x = rect.top_left.x + size.x;
        }

        if (size.y == vui_auto_len) {
            rect.bottom_right.y = vui_auto_len;
        } else if (size.y == vui_fill_len) {
			vui_assert(parent_ctrl->layout.type == VuiLayoutType_container || parent_ctrl->layout.type == VuiLayoutType_stack || (parent_ctrl->layout.type == VuiLayoutType_column && !parent_ctrl->layout.row_column.wrap), "vui_fill_len can only be used in container, stack, or column layouts. the column layout cannot wrap");
			rect.bottom_right.y = parent_ctrl->rect.bottom_right.y - parent_padding->bottom;
			if (parent_ctrl->layout.type != VuiLayoutType_stack) {
				rect.bottom_right.y -= margin->bottom;
			}
        } else {
            rect.bottom_right.y = rect.top_left.y + size.y;
        }
    }

	VuiThickness* padding = &style->padding;

	VuiCtrlIdHash id_hash = vui_ctrl_gen_id_hash(id);
	_vui.build.parent_ctrl_idx = _vui.build.w->ctrls.count - 1;

    *ctrl = (VuiCtrl) {
		.parent_idx = parent_ctrl_idx,
		.id_hash = id_hash,
		.style = style,
        .rect = rect,
        .max_bottom_right = VuiVec2_zero,
        .layout = {0},
		.render_fn = render_ctrl_fn,
    };

	if (parent_ctrl->layout.type == VuiLayoutType_row || parent_ctrl->layout.type == VuiLayoutType_column) {
		ctrl->layout.row_column.next_ctrl_top_left = VuiVec2_add(rect.top_left, VuiVec2_init(padding->left, padding->top));
	}
}

void _vui_offset_ctrls(uint32_t start_idx, uint32_t end_idx, VuiVec2 offset) {
	VuiCtrl* ctrls = _vui.build.w->ctrls.DasStk_data;
	for (uint32_t idx = start_idx; idx < end_idx; idx += 1) {
		VuiCtrl* child_ctrl = &ctrls[idx];

		child_ctrl->rect.top_left = VuiVec2_add(child_ctrl->rect.top_left, offset);
		child_ctrl->rect.bottom_right = VuiVec2_add(child_ctrl->rect.bottom_right, offset);
	}
}

void vui_ctrl_resolve_auto(VuiCtrl* ctrl) {
    // is atleast one of the dimensions is auto then set that/those dimension/s to the max_bottom_right
	if (ctrl->rect.bottom_right.x == vui_auto_len) {
		ctrl->rect.bottom_right.x = ctrl->max_bottom_right.x + ctrl->style->padding.right;
	}

	if (ctrl->rect.bottom_right.y == vui_auto_len) {
		ctrl->rect.bottom_right.y = ctrl->max_bottom_right.y + ctrl->style->padding.bottom;
	}
}

void vui_ctrl_end() {
	uint32_t ctrl_idx = _vui.build.parent_ctrl_idx;
	VuiCtrl* ctrl = DasStk_get(&_vui.build.w->ctrls, ctrl_idx);

    VuiThickness* ctrl_margin = &ctrl->style->margin;
    VuiThickness* ctrl_padding = &ctrl->style->padding;

	vui_ctrl_resolve_auto(ctrl);

	if (ctrl->layout.type == VuiLayoutType_stack) {
		VuiVec2 top_left = VuiVec2_add(ctrl->rect.top_left, VuiVec2_init(ctrl_padding->left, ctrl_padding->top));
		VuiVec2 bottom_right = VuiVec2_sub(ctrl->rect.bottom_right, VuiVec2_init(ctrl_padding->right, ctrl_padding->bottom));
		VuiVec2 layout_size = VuiVec2_sub(bottom_right, top_left);

		for (uint16_t i = ctrl->layout.stack.stack_child_start_idx; i < _vui.build.stack_children.count; i += 1) {
			_VuiStackChild* child = DasStk_get(&_vui.build.stack_children, i);
			VuiCtrl* child_ctrl = DasStk_get(&_vui.build.w->ctrls, child->ctrl_idx);
			VuiVec2 offset = child->offset;
			VuiVec2 size = VuiRect_size(child_ctrl->rect);

			switch (child->align) {
				case VuiAlign_top_left: break;
				case VuiAlign_top_center:
					offset.x += (layout_size.x / 2.0) - (size.x / 2.0);
					break;
				case VuiAlign_top_right:
					offset.x += layout_size.x - size.x;
					break;
				case VuiAlign_center_left:
					offset.y += (layout_size.y / 2.0) - (size.y / 2.0);
					break;
				case VuiAlign_center:
					offset.x += (layout_size.x / 2.0) - (size.x / 2.0);
					offset.y += (layout_size.y / 2.0) - (size.y / 2.0);
					break;
				case VuiAlign_center_right:
					offset.x += layout_size.x - size.x;
					offset.y += (layout_size.y / 2.0) - (size.y / 2.0);
					break;
				case VuiAlign_bottom_left:
					offset.y += layout_size.y - size.y;
					break;
				case VuiAlign_bottom_center:
					offset.x += (layout_size.x / 2.0) - (size.x / 2.0);
					offset.y += layout_size.y - size.y;
					break;
				case VuiAlign_bottom_right:
					offset.x += layout_size.x - size.x;
					offset.y += layout_size.y - size.y;
					break;
			}

			if (offset.x != 0 || offset.y != 0) {
				_vui_offset_ctrls(child->ctrl_idx, child->children_ctrl_end_idx, offset);
			}
		}

		_vui.build.stack_children.count = ctrl->layout.stack.stack_child_start_idx;
	}

    VuiRect ctrl_rect = ctrl->rect;

	_vui.build.parent_ctrl_idx = ctrl->parent_idx;
	VuiCtrl* parent_ctrl = DasStk_get(&_vui.build.w->ctrls, ctrl->parent_idx);
    VuiRect parent_rect = parent_ctrl->rect;
	VuiThickness* parent_padding = &parent_ctrl->style->padding;

    VuiVec2 old_top_left = ctrl_rect.top_left;
    switch (parent_ctrl->layout.type) {
        case VuiLayoutType_container:
			vui_assert(!parent_ctrl->layout.container.has_child, "another child has been added to a container layout, this layout only allows for a single child");
			parent_ctrl->layout.container.has_child = vui_true;
			break;
        case VuiLayoutType_stack: {
			_VuiStackChild* c = DasStk_push(&_vui.build.stack_children, NULL);
			c->ctrl_idx = ctrl_idx;
			c->children_ctrl_end_idx = _vui.build.w->ctrls.count;
			c->align = parent_ctrl->layout.stack.align;
			c->offset = parent_ctrl->layout.stack.offset;
			break;
		};
        case VuiLayoutType_row:
        case VuiLayoutType_column: {
            while (1) {
                _VuiLayout* parent_layout = &parent_ctrl->layout;
                float* max_pt = &parent_ctrl->layout.row_column.max_pt;
                VuiVec2* next_ctrl_top_left = &parent_ctrl->layout.row_column.next_ctrl_top_left;

                VuiVec2 ctrl_outer_top_left = VuiVec2_init(
                        ctrl_rect.top_left.x - ctrl_margin->left,
                        ctrl_rect.top_left.y - ctrl_margin->top);
                if (parent_layout->type == VuiLayoutType_row) {
                    VuiBool is_first_on_row = vui_approx_eq(ctrl_outer_top_left.y - ctrl_padding->top, parent_rect.top_left.y);
                    float parent_end_y = parent_rect.bottom_right.y - parent_padding->bottom;

                    float ctrl_end_y = ctrl_rect.bottom_right.y + ctrl_margin->bottom;
                    // if the control needs to wrap then move the ctrl to the start of the next row
                    if (!is_first_on_row && parent_layout->row_column.wrap && ctrl_end_y > parent_end_y) {
                        *max_pt += parent_layout->row_column.wrap_spacing;
                        VuiVec2 size = VuiRect_size(ctrl_rect);
                        ctrl_rect.top_left.x = *max_pt + ctrl_margin->left;
                        ctrl_rect.top_left.y = parent_rect.top_left.y + parent_padding->top + ctrl_margin->top;
                        ctrl_rect.bottom_right.x = ctrl_rect.top_left.x + size.x;
                        ctrl_rect.bottom_right.y = ctrl_rect.top_left.y + size.y;
                        continue;
                    }

                    // record the new max width (used for wrapping)
                    *max_pt = vui_max(*max_pt, ctrl_rect.bottom_right.x + ctrl_margin->right);
                    // set the next ctrl top left to the next row
                    next_ctrl_top_left->x = ctrl_outer_top_left.x;
                    next_ctrl_top_left->y = ctrl_end_y + parent_layout->row_column.spacing;
                } else { // VuiLayoutType_column
                    VuiBool is_first_on_column = vui_approx_eq(ctrl_outer_top_left.x - ctrl_padding->left, parent_rect.top_left.x);
                    float parent_end_x = parent_rect.bottom_right.x - parent_padding->right;

                    float ctrl_end_x = ctrl_rect.bottom_right.x + ctrl_margin->right;
                    // if the control needs to wrap then move the ctrl to the start of the next column
                    if (!is_first_on_column && parent_layout->row_column.wrap && ctrl_end_x > parent_end_x) {
                        *max_pt += parent_layout->row_column.wrap_spacing;
                        VuiVec2 size = VuiRect_size(ctrl_rect);
                        ctrl_rect.top_left.x = parent_rect.top_left.x + parent_padding->left + ctrl_margin->left;
                        ctrl_rect.top_left.y = *max_pt + ctrl_margin->top;
                        ctrl_rect.bottom_right.x = ctrl_rect.top_left.x + size.x;
                        ctrl_rect.bottom_right.y = ctrl_rect.top_left.y + size.y;
                        continue;
                    }

                    // record the new max height (used for wrapping)
                    *max_pt = vui_max(*max_pt, ctrl_rect.bottom_right.y + ctrl_margin->bottom);
                    // set the next ctrl top left to the next column
                    next_ctrl_top_left->x = ctrl_end_x + parent_layout->row_column.spacing;
                    next_ctrl_top_left->y = ctrl_outer_top_left.y;
                }

                break;
            }

            break;
        };
    }

	//
    // if top_left has changed, update all positions of itself and its children
    if (ctrl_rect.top_left.x != old_top_left.x || ctrl_rect.top_left.y != old_top_left.y) {
		ctrl->rect = ctrl_rect;
        VuiVec2 offset = VuiVec2_sub(ctrl_rect.top_left, old_top_left);
		_vui_offset_ctrls(ctrl_idx + 1, _vui.build.w->ctrls.count, offset);
	}

    // if the parent has an auto width or height then record the max bottom right
    if (VuiVec2_has_auto(parent_rect.bottom_right)) {
        VuiVec2 ctrl_max_bottom_right = ctrl_rect.bottom_right;
		if (parent_ctrl->layout.type != VuiLayoutType_stack) { // all layouts other than stack use margin to space controls
			ctrl_max_bottom_right = VuiVec2_add(ctrl_max_bottom_right, VuiVec2_init(ctrl_margin->right, ctrl_margin->bottom));
		}
        VuiVec2 orig = parent_ctrl->max_bottom_right;
        parent_ctrl->max_bottom_right = VuiVec2_max(orig, ctrl_max_bottom_right);
    }

	_vui.build.last_ctrl_top_left = ctrl_rect.top_left;
	_vui.build.last_ctrl_size = VuiRect_size(ctrl_rect);
}

typedef struct {
    float border_width;
    VuiColor border_color;
    float radius;
    VuiBool has_bg_color;
} _VuiBoxFrameState;

void vui_ctrl_render() {
	VuiCtrl* ctrls = _vui.build.w->ctrls.DasStk_data;
	uint32_t parent_ctrl_idx = _vui.build.parent_ctrl_idx;

	VuiRect parent_clip_rect = _vui.build.render_clip_rect;

	VuiCtrl* ctrl = &ctrls[_vui.build.render_ctrl_idx];
	VuiThickness* padding = &ctrl->style->padding;

	// get the parents inner rect
	VuiRect ctrl_inner_rect = {
		VuiVec2_add(ctrl->rect.top_left, VuiVec2_init(padding->left, padding->top)),
		VuiVec2_sub(ctrl->rect.bottom_right, VuiVec2_init(padding->right, padding->bottom))
	};

	// make the next clip rect by appling the ctrl inner rect to the parent clip rect
	_vui.build.render_clip_rect = VuiRect_clip(ctrl_inner_rect, parent_clip_rect);

	_vui.build.parent_ctrl_idx = _vui.build.render_ctrl_idx;

	// move to the next ctrl, this will either be the first child if one exists.
	// or it'll be the sibling of the ctrl.
	_vui.build.render_ctrl_idx += 1;
	while (_vui.build.render_ctrl_idx < _vui.build.w->ctrls.count) {
		VuiCtrl* child_ctrl = &ctrls[_vui.build.render_ctrl_idx];
		if (child_ctrl->parent_idx != _vui.build.parent_ctrl_idx) break;

		child_ctrl->render_fn();
	}

	_vui.build.parent_ctrl_idx = parent_ctrl_idx;
	_vui.build.render_clip_rect = parent_clip_rect;
}

void vui_box_render() {
	VuiCtrl* ctrl = DasStk_get(&_vui.build.w->ctrls, _vui.build.render_ctrl_idx);
	VuiBoxStyle* style = (VuiBoxStyle*)ctrl->style;
	VuiColor bg_color = style->bg_color;
	float border_width = style->border.width;
	if (bg_color.a != 0) {
		VuiRect bg_rect = ctrl->rect;
		bg_rect.top_left.x += border_width;
		bg_rect.top_left.y += border_width;
		bg_rect.bottom_right.x -= border_width;
		bg_rect.bottom_right.y -= border_width;
		vui_render_rect(bg_rect, bg_color, style->radius);
	}

	if (border_width > 0.0) {
		float border_radius = style->radius > 0.0 ? style->radius + border_width : 0.0;
		vui_render_rect_border(ctrl->rect, style->border.color, border_radius, border_width);
	}

	vui_ctrl_render();
}

void vui_box_start(VuiCtrlId id, VuiVec2 size, VuiBoxStyle* style) {
    vui_ctrl_start(id, size, &vui_box_render, &style->ctrl);

	float width = style->border.width;
    if (width > 0.0) {
		uint32_t ctrl_idx = _vui.build.parent_ctrl_idx;
		VuiCtrl* ctrl = DasStk_get(&_vui.build.w->ctrls, ctrl_idx);

        ctrl->rect.top_left.x += width;
        ctrl->rect.top_left.y += width;

        ctrl->rect.bottom_right.x -= width;
        ctrl->rect.bottom_right.y -= width;

		if (ctrl->layout.type == VuiLayoutType_row || ctrl->layout.type == VuiLayoutType_column) {
			ctrl->layout.row_column.next_ctrl_top_left.x += width;
			ctrl->layout.row_column.next_ctrl_top_left.y += width;
		}
    }
}

void vui_box_end() {
	uint32_t ctrl_idx = _vui.build.parent_ctrl_idx;
	VuiCtrl* ctrl = DasStk_get(&_vui.build.w->ctrls, ctrl_idx);
	VuiBoxStyle* style = (VuiBoxStyle*)ctrl->style;

	vui_ctrl_resolve_auto(ctrl);

	float width = style->border.width;
	if (width > 0.0) {
		ctrl->rect.top_left.x -= width;
		ctrl->rect.top_left.y -= width;

		ctrl->rect.bottom_right.x += width;
		ctrl->rect.bottom_right.y += width;

		if (ctrl->layout.type == VuiLayoutType_row || ctrl->layout.type == VuiLayoutType_column) {
			ctrl->layout.row_column.next_ctrl_top_left.x += width;
			ctrl->layout.row_column.next_ctrl_top_left.y += width;
		}
	}

    vui_ctrl_end();
}

static VuiColor _vui_render_glyph_color = {0};
void _vui_render_glyph(VuiTextureId glyph_texture_id, VuiRect rect, VuiRect uv_rect) {
	vui_render_image_(glyph_texture_id, rect, uv_rect, _vui_render_glyph_color, vui_true);
}

void vui_text_render() {
	VuiCtrl* ctrl = DasStk_get(&_vui.build.w->ctrls, _vui.build.render_ctrl_idx);
	VuiTextStyle* style = (VuiTextStyle*)ctrl->style;

	char* text = DasStk_get(&_vui.build.w->text, _vui.build.render_text_idx);
	uint32_t text_length = strlen(text);
	_vui.build.render_text_idx += text_length + 1;

	if (text_length) {
		_vui_render_glyph_color = style->color;
		_vui.position_text_fn(_vui.position_text_userdata, style->font.user_id, text, text_length, ctrl->rect.top_left, _vui_render_glyph);
	}

	vui_ctrl_render();
}

VuiVec2 vui_get_text_size(char* text, uint32_t text_length, float wrap_at_width, VuiTextStyle* style) {
	return _vui.position_text_fn(_vui.position_text_userdata, style->font.user_id, text, text_length, VuiVec2_zero, NULL);
}

void vui_text_(char* text, uint32_t text_length, float wrap_at_width, VuiTextStyle* style) {
	VuiVec2 size = vui_get_text_size(text, text_length, wrap_at_width, style);
	vui_text_with_size(size, text, text_length, wrap_at_width, style);
}

void vui_text_with_size(VuiVec2 size, char* text, uint32_t text_length, float wrap_at_width, VuiTextStyle* style) {
	if (text_length) DasStk_push_many(&_vui.build.w->text, text, text_length);
	*DasStk_push(&_vui.build.w->text, NULL) = '\0';
	vui_ctrl_start(1, size, vui_text_render, &style->ctrl);
	vui_ctrl_end();
}

typedef struct {
	VuiCtrlStyle ctrl;
	VuiTextureId texture_id;
	VuiColor color;
	VuiRect uv_rect;
} VuiImageData;

void vui_image_render() {
	VuiCtrl* ctrl = DasStk_get(&_vui.build.w->ctrls, _vui.build.render_ctrl_idx);
	VuiImageData* data = (VuiImageData*)ctrl->style;
	vui_render_image(data->texture_id, ctrl->rect, data->uv_rect, data->color);
	vui_ctrl_render();
}

void vui_image(VuiVec2 size, VuiTextureId texture_id, VuiRect uv_rect, VuiColor color, VuiCtrlStyle* style) {
	VuiImageData* data = vui_frame_data_alloc_elmt(VuiImageData);
	data->ctrl = *style;
	data->texture_id = texture_id;
	data->color = color;
	data->uv_rect = uv_rect;
	vui_ctrl_start(0, size, vui_image_render, &data->ctrl);
	vui_ctrl_end();
}

void vui_spacing(VuiVec2 size) {
	vui_assert_finite_size(size, "size");
	vui_ctrl_start(1, size, vui_ctrl_render, &VuiCtrlStyle_zero);
	vui_ctrl_end();
}

void vui_separator_render() {
	VuiCtrl* ctrl = DasStk_get(&_vui.build.w->ctrls, _vui.build.render_ctrl_idx);
	VuiCtrl* parent_ctrl = DasStk_get(&_vui.build.w->ctrls, _vui.build.parent_ctrl_idx);
	VuiSeparatorStyle* style = (VuiSeparatorStyle*)ctrl->style;

	VuiBool is_row = parent_ctrl->layout.type == VuiLayoutType_row;
	VuiRect rect = ctrl->rect;

	if (is_row) {
		rect.bottom_right.x = parent_ctrl->rect.bottom_right.x - style->ctrl.margin.right - parent_ctrl->style->padding.right;
	} else {
		rect.bottom_right.y = parent_ctrl->rect.bottom_right.y - style->ctrl.margin.bottom - parent_ctrl->style->padding.bottom;
	}

	vui_render_rect(rect, style->color, style->radius);
	vui_ctrl_render();
}

void vui_separator(VuiSeparatorStyle* style) {
	VuiCtrl* parent_ctrl = DasStk_get(&_vui.build.w->ctrls, _vui.build.parent_ctrl_idx);
	VuiBool is_row = parent_ctrl->layout.type == VuiLayoutType_row;
	vui_assert(is_row || parent_ctrl->layout.type == VuiLayoutType_column, "vui_seperator can only be used in a non-wrapping row or column layout");
	vui_assert(!parent_ctrl->layout.row_column.wrap, "vui_seperator can only be used in a non-wrapping row or column layout");

	VuiVec2 size = is_row ? VuiVec2_init(0, style->width) : VuiVec2_init(style->width, 0);
	vui_ctrl_start(1, size, vui_separator_render, &style->ctrl);
	vui_ctrl_end();
}

void vui_button_core_start(VuiCtrlId id, VuiVec2 size, VuiBool pressed, VuiButtonStyle* style) {
	VuiBoxStyle* bs = vui_frame_data_alloc_elmt(VuiBoxStyle);
	bs->ctrl = style->ctrl;
	bs->radius = style->radius;
	bs->border.width = style->border_width;
	VuiCtrlIdHash id_hash = vui_ctrl_gen_id_hash(id | VuiCtrlId_focusable_mask);
	if (pressed) {
		bs->bg_color = style->pressed_bg_color;
		bs->border.color = style->pressed_border_color;
	} else if (vui_ctrl_is_focused(id_hash) || vui_ctrl_is_mouse_focused(id_hash)) {
		bs->bg_color = style->focused_bg_color;
		bs->border.color = style->focused_border_color;
	} else {
		bs->bg_color = style->inactive_bg_color;
		bs->border.color = style->inactive_border_color;
	}

	vui_box_start(id | VuiCtrlId_focusable_mask, size, bs);
}

void vui_button_core_end() {
	vui_box_end();
}

VuiBool vui_button_start(VuiCtrlId id, VuiVec2 size, VuiButtonStyle* style) {
	VuiCtrlIdHash id_hash = vui_ctrl_gen_id_hash(id | VuiCtrlId_focusable_mask);
    VuiFocusState state = vui_ctrl_focus_state(id_hash);
	VuiBool pressed = state == VuiFocusState_pressed || state == VuiFocusState_held || state == VuiFocusState_double_pressed;
	vui_button_core_start(id, size, pressed, style);
	return state == VuiFocusState_released;
}

void vui_button_end() {
	vui_button_core_end();
}

VuiBool vui_press_button_start(VuiCtrlId id, VuiVec2 size, VuiButtonStyle* style) {
	VuiCtrlIdHash id_hash = vui_ctrl_gen_id_hash(id | VuiCtrlId_focusable_mask);
    VuiFocusState state = vui_ctrl_focus_state(id_hash);
	VuiBool pressed = state == VuiFocusState_pressed || state == VuiFocusState_held || state == VuiFocusState_double_pressed;
	vui_button_core_start(id, size, pressed, style);
	return pressed;
}

void vui_press_button_end() {
	vui_button_core_end();
}

void vui_text_button_text(VuiBool pressed, char* text, uint32_t text_length, VuiTextButtonStyle* style) {
	VuiTextStyle* ts = vui_frame_data_alloc_elmt(VuiTextStyle);
	ts->font = style->font;
	VuiCtrlIdHash id_hash = vui_get_ctrl_id_hash();
	if (pressed) {
		ts->color = style->pressed_color;
	} else if (vui_ctrl_is_focused(id_hash) || vui_ctrl_is_mouse_focused(id_hash)) {
		ts->color = style->focused_color;
	} else {
		ts->color = style->inactive_color;
	}
	vui_text_(text, text_length, 0.0, ts);
}

VuiBool vui_text_button_(VuiCtrlId id, char* text, uint32_t text_length, VuiTextButtonStyle* style) {
	VuiBool released = vui_button_start(id, VuiVec2_auto, &style->button);

	VuiCtrlIdHash id_hash = vui_get_ctrl_id_hash();
    VuiFocusState state = vui_ctrl_focus_state(id_hash);
	VuiBool pressed = state == VuiFocusState_pressed || state == VuiFocusState_held || state == VuiFocusState_double_pressed;
	vui_text_button_text(pressed, text, text_length, style);

	vui_button_end();
	return released;
}

VuiBool vui_image_button(VuiCtrlId id, VuiVec2 size, VuiTextureId texture_id, VuiRect uv_rect, VuiColor color, VuiButtonStyle* style) {
	VuiBool released = vui_button_start(id, size, style);

	size.x -= VuiThickness_horizontal(style->ctrl.padding) + style->border_width * 2;
	size.y -= VuiThickness_vertical(style->ctrl.padding) + style->border_width * 2;
	vui_image(size, texture_id, uv_rect, color, &VuiCtrlStyle_zero);

	vui_button_end();
	return released;
}

VuiBool vui_toggle_button_start(VuiCtrlId id, VuiVec2 size, VuiBool* pressed, VuiButtonStyle* style) {
	VuiCtrlIdHash id_hash = vui_ctrl_gen_id_hash(id | VuiCtrlId_focusable_mask);
    if (vui_ctrl_focus_state(id_hash) == VuiFocusState_pressed) {
		*pressed = !*pressed;
	}
	vui_button_core_start(id, size, *pressed, style);
	return *pressed;
}

// returns *pressed, that is toggled when this has been pressed on this frame
void vui_toggle_button_end() {
	vui_button_core_end();
}

VuiBool vui_text_toggle_button_(VuiCtrlId id, VuiBool* pressed, char* text, uint32_t text_length, VuiTextButtonStyle* style) {
	VuiBool p = vui_toggle_button_start(id, VuiVec2_auto, pressed, &style->button);
	vui_text_button_text(p, text, text_length, style);
	vui_toggle_button_end();
	return p;
}

VuiBool vui_image_toggle_button(VuiCtrlId id, VuiBool* pressed, VuiVec2 size, VuiTextureId texture_id, VuiRect uv_rect, VuiColor color, VuiButtonStyle* style) {
	VuiBool p = vui_toggle_button_start(id, VuiVec2_auto, pressed, style);

	size.x -= VuiThickness_horizontal(style->ctrl.padding) + style->border_width * 2;
	size.y -= VuiThickness_vertical(style->ctrl.padding) + style->border_width * 2;
	vui_image(size, texture_id, uv_rect, color, &VuiCtrlStyle_zero);

	vui_toggle_button_end();
	return p;
}

VuiBool vui_select_button_start(VuiCtrlId id, VuiVec2 size, VuiCtrlId* selected_id, VuiButtonStyle* style) {
	VuiCtrlIdHash id_hash = vui_ctrl_gen_id_hash(id | VuiCtrlId_focusable_mask);
	VuiFocusState state = vui_ctrl_focus_state(id_hash);
    if (state == VuiFocusState_pressed || state == VuiFocusState_released) {
		*selected_id = id;
	}
	VuiBool pressed = id == *selected_id;
	vui_button_core_start(id, size, pressed, style);
	return pressed;
}

void vui_select_button_end() {
	vui_button_core_end();
}

VuiBool vui_text_select_button_(VuiCtrlId id, VuiCtrlId* selected_id, char* text, uint32_t text_length, VuiTextButtonStyle* style) {
	VuiBool p = vui_select_button_start(id, VuiVec2_auto, selected_id, &style->button);
	vui_text_button_text(p, text, text_length, style);
	vui_select_button_end();
	return p;
}

VuiBool vui_image_select_button(VuiCtrlId id, VuiCtrlId* selected_id, VuiVec2 size, VuiTextureId texture_id, VuiRect uv_rect, VuiColor color, VuiButtonStyle* style) {
	VuiBool p = vui_select_button_start(id, VuiVec2_auto, selected_id, style);

	size.x -= VuiThickness_horizontal(style->ctrl.padding) + style->border_width * 2;
	size.y -= VuiThickness_vertical(style->ctrl.padding) + style->border_width * 2;
	vui_image(size, texture_id, uv_rect, color, &VuiCtrlStyle_zero);

	vui_select_button_end();
	return p;
}

VuiBool vui_image_text_select_button_(VuiCtrlId id, VuiCtrlId* selected_id, char* text, uint32_t text_length, VuiVec2 image_size, VuiTextureId texture_id, VuiRect uv_rect, VuiColor color, VuiTextButtonStyle* style) {
	VuiBool p = vui_select_button_start(id, VuiVec2_auto, selected_id, &style->button);

	vui_stack_layout_start(0, VuiVec2_auto, &VuiCtrlStyle_zero);

	vui_stack_layout_set_next_pos(VuiAlign_center_left, VuiVec2_zero);
	vui_image(image_size, texture_id, uv_rect, color, &VuiCtrlStyle_zero);

	vui_stack_layout_set_next_pos(VuiAlign_center_right, VuiVec2_zero);
	vui_text_button_text(p, text, text_length, style);

	image_size.x += style->button.ctrl.padding.left;
	image_size.y = 0;
	vui_stack_layout_end(image_size);
	vui_select_button_end();
	return p;
}

typedef struct {
	VuiCtrlStyle ctrl;
	VuiColor color;
	float radius;
} _VuiCheckBoxCheckedStyle;

void vui_check_box_checked_render() {
	VuiCtrl* ctrl = DasStk_get(&_vui.build.w->ctrls, _vui.build.render_ctrl_idx);
	_VuiCheckBoxCheckedStyle* style = (_VuiCheckBoxCheckedStyle*)ctrl->style;
	vui_render_rect(ctrl->rect, style->color, style->radius);
	vui_ctrl_render();
}

VuiCheckState vui_check_box(VuiCtrlId id, VuiCheckState* check_state, VuiCheckBoxStyle* style) {
	VuiBoxStyle* bs = vui_frame_data_alloc_elmt(VuiBoxStyle);
	bs->ctrl = style->ctrl;
	bs->radius = style->radius;
	bs->border.width = style->border_width;
	VuiCtrlIdHash id_hash = vui_ctrl_gen_id_hash(id | VuiCtrlId_focusable_mask);
	if (vui_ctrl_is_focused(id_hash) || vui_ctrl_is_mouse_focused(id_hash)) {
		bs->bg_color = style->focused_bg_color;
		bs->border.color = style->focused_border_color;
	} else {
		bs->bg_color = style->inactive_bg_color;
		bs->border.color = style->inactive_border_color;
	}

	vui_box_start(id | VuiCtrlId_focusable_mask, VuiVec2_init(style->size, style->size), bs);
	if (vui_ctrl_focus_state(vui_get_ctrl_id_hash()) == VuiFocusState_released) {
		*check_state = !*check_state;
	}
	VuiVec2 size = {
		style->size - style->border_width * 2 - VuiThickness_horizontal(style->ctrl.padding),
		style->size - style->border_width * 2 - VuiThickness_vertical(style->ctrl.padding)
	};
	switch (*check_state) {
		case VuiCheckState_checked: {
			_VuiCheckBoxCheckedStyle* s = vui_frame_data_alloc_elmt(_VuiCheckBoxCheckedStyle);
			s->color = style->checked_color;
			s->radius = style->radius;
			vui_ctrl_start(1, size, vui_check_box_checked_render, &s->ctrl);
			vui_ctrl_end();
			break;
		};
		case VuiCheckState_not_checked:
			break;
		case VuiCheckState_partially_checked: {
			size.x /= 1.5;
			size.y /= 1.5;
			VuiCtrlStyle* s = vui_frame_data_alloc_elmt(VuiCtrlStyle);
			s->margin = VuiThickness_init_hv(size.x / 3.75, size.y / 3.75);
			vui_ctrl_start(1, size, vui_check_box_checked_render, s);
			vui_ctrl_end();
			break;
		};
	}
	vui_box_end();
	return *check_state;
}

VuiCheckState vui_text_check_box_(VuiCtrlId id, VuiCheckState* check_state, char* text, uint32_t text_length, VuiTextCheckBoxStyle* style) {
	vui_column_layout_start(id | VuiCtrlId_focusable_mask, VuiVec2_auto, &VuiRowColumnLayoutStyle_zero);
	if (vui_ctrl_focus_state(vui_get_ctrl_id_hash()) == VuiFocusState_released) {
		*check_state = !*check_state;
	}

	VuiTextStyle* ts = vui_frame_data_alloc_elmt(VuiTextStyle);
	ts->ctrl = style->check_box.ctrl;
	ts->color = style->color;
	ts->font = style->font;

	VuiVec2 text_size = vui_get_text_size(text, text_length, 0, ts);
	float size = style->check_box.size;

	if (size < text_size.y) {
		vui_row_layout_start(0, VuiVec2_auto, &VuiRowColumnLayoutStyle_zero);
		vui_spacing(VuiVec2_init(0, (text_size.y - size) / 2.0));
	}
	VuiCheckState state = vui_check_box(0, check_state, &style->check_box);
	if (size < text_size.y) {
		vui_row_layout_end();
	}

	if (size > text_size.y) {
		vui_row_layout_start(0, VuiVec2_auto, &VuiRowColumnLayoutStyle_zero);
		vui_spacing(VuiVec2_init(0, (size - text_size.y) / 2.0));
	}
	vui_text_with_size(text_size, text, text_length, 0, ts);
	if (size > text_size.y) {
		vui_row_layout_end();
	}

	vui_column_layout_end();
	return state;
}

VuiCheckState vui_image_check_box(VuiCtrlId id, VuiCheckState* check_state, VuiVec2 image_size, VuiTextureId texture_id, VuiRect uv_rect, VuiColor color, VuiCheckBoxStyle* style) {
	vui_column_layout_start(id | VuiCtrlId_focusable_mask, VuiVec2_auto, &VuiRowColumnLayoutStyle_zero);
	if (vui_ctrl_focus_state(vui_get_ctrl_id_hash()) == VuiFocusState_released) {
		*check_state = !*check_state;
	}

	float size = style->size;
	if (size < image_size.y) {
		vui_row_layout_start(0, VuiVec2_auto, &VuiRowColumnLayoutStyle_zero);
		vui_spacing(VuiVec2_init(0, (image_size.y - size) / 2.0));
	}
	VuiCheckState state = vui_check_box(0, check_state, style);
	if (size < image_size.y) {
		vui_row_layout_end();
	}

	if (size > image_size.y) {
		vui_row_layout_start(0, VuiVec2_auto, &VuiRowColumnLayoutStyle_zero);
		vui_spacing(VuiVec2_init(0, (size - image_size.y) / 2.0));
	}
	vui_image(image_size, texture_id, uv_rect, color, &style->ctrl);
	if (size > image_size.y) {
		vui_row_layout_end();
	}

	vui_column_layout_end();
	return state;
}

void vui_radio_button_render() {
	VuiCtrl* ctrl = DasStk_get(&_vui.build.w->ctrls, _vui.build.render_ctrl_idx);
	VuiRadioButtonStyle* style = (VuiRadioButtonStyle*)ctrl->style;
	float radius = style->size / 2.0;
	VuiVec2 pos = VuiVec2_add(ctrl->rect.top_left, VuiVec2_init(radius, radius));

	VuiColor bg_color = {0};
	VuiColor border_color = {0};
	VuiCtrlIdHash id_hash = ctrl->id_hash;
	if (
		vui_ctrl_is_focused(id_hash) ||
		vui_ctrl_is_mouse_focused(id_hash) ||
		// has child control
		(_vui.build.w->ctrls.count > _vui.build.render_ctrl_idx + 1 &&
			 DasStk_get(&_vui.build.w->ctrls, _vui.build.render_ctrl_idx + 1)->parent_idx == _vui.build.render_ctrl_idx))
	{
		bg_color = style->focused_bg_color;
		border_color = style->focused_border_color;
	} else {
		bg_color = style->inactive_bg_color;
		border_color = style->inactive_border_color;
	}

	vui_render_circle(pos, radius, bg_color);
	vui_render_circle_border(pos, radius, border_color, style->border_width);
	vui_ctrl_render();
}

void vui_radio_button_selected_render() {
	VuiCtrl* parent_ctrl = DasStk_get(&_vui.build.w->ctrls, _vui.build.parent_ctrl_idx);
	VuiRadioButtonStyle* parent_style = (VuiRadioButtonStyle*)parent_ctrl->style;
	float radius = parent_style->size / 2.0;
	float selected_radius = parent_style->selected_size / 2.0;
	VuiVec2 pos = VuiVec2_add(parent_ctrl->rect.top_left, VuiVec2_init(radius, radius));
	vui_render_circle(pos, selected_radius, parent_style->selected_color);
	vui_ctrl_render();
}

VuiBool vui_radio_button(VuiCtrlId id, VuiCtrlId* selected_id, VuiRadioButtonStyle* style) {
	vui_ctrl_start(id | VuiCtrlId_focusable_mask, VuiVec2_init(style->size, style->size), vui_radio_button_render, &style->ctrl);
	if (vui_ctrl_focus_state(vui_get_ctrl_id_hash()) == VuiFocusState_released) {
		*selected_id = id;
	}
	VuiBool is_selected = *selected_id == id;
	if (is_selected) {
		vui_ctrl_start(id, VuiVec2_init(style->selected_size, style->selected_size), vui_radio_button_selected_render, &VuiCtrlStyle_zero);
		vui_ctrl_end();
	}
	vui_ctrl_end();
	return is_selected;
}

VuiBool vui_text_radio_button_(VuiCtrlId id, VuiCtrlId* selected_id, char* text, uint32_t text_length, VuiTextRadioButtonStyle* style) {
	vui_column_layout_start(id | VuiCtrlId_focusable_mask, VuiVec2_auto, &VuiRowColumnLayoutStyle_zero);
	if (vui_ctrl_focus_state(vui_get_ctrl_id_hash()) == VuiFocusState_released) {
		*selected_id = id;
	}

	VuiTextStyle* ts = vui_frame_data_alloc_elmt(VuiTextStyle);
	ts->ctrl = style->radio_button.ctrl;
	ts->color = style->color;
	ts->font = style->font;
	VuiVec2 text_size = vui_get_text_size(text, text_length, 0, ts);
	float size = style->radio_button.size;

	if (size < text_size.y) {
		vui_row_layout_start(0, VuiVec2_auto, &VuiRowColumnLayoutStyle_zero);
		vui_spacing(VuiVec2_init(0, (text_size.y - size) / 2.0));
	}
	VuiBool is_selected = vui_radio_button(id, selected_id, &style->radio_button);
	if (size < text_size.y) {
		vui_row_layout_end();
	}

	if (size > text_size.y) {
		vui_row_layout_start(0, VuiVec2_auto, &VuiRowColumnLayoutStyle_zero);
		vui_spacing(VuiVec2_init(0, (size - text_size.y) / 2.0));
	}
	vui_text_(text, text_length, 0, ts);
	if (size > text_size.y) {
		vui_row_layout_end();
	}

	vui_column_layout_end();
	return is_selected;
}

VuiBool vui_image_radio_button(VuiCtrlId id, VuiCtrlId* selected_id, VuiVec2 image_size, VuiTextureId texture_id, VuiRect uv_rect, VuiColor color, VuiRadioButtonStyle* style) {
	vui_column_layout_start(id | VuiCtrlId_focusable_mask, VuiVec2_auto, &VuiRowColumnLayoutStyle_zero);
	if (vui_ctrl_focus_state(vui_get_ctrl_id_hash()) == VuiFocusState_released) {
		*selected_id = id;
	}

	float size = style->size;
	if (size < image_size.y) {
		vui_row_layout_start(0, VuiVec2_auto, &VuiRowColumnLayoutStyle_zero);
		vui_spacing(VuiVec2_init(0, (image_size.y - size) / 2.0));
	}
	VuiBool is_selected = vui_radio_button(id, selected_id, style);
	if (size < image_size.y) {
		vui_row_layout_end();
	}

	if (size > image_size.y) {
		vui_row_layout_start(0, VuiVec2_auto, &VuiRowColumnLayoutStyle_zero);
		vui_spacing(VuiVec2_init(0, (size - image_size.y) / 2.0));
	}
	vui_image(image_size, texture_id, uv_rect, color, &style->ctrl);
	if (size > image_size.y) {
		vui_row_layout_end();
	}

	vui_column_layout_end();
	return is_selected;
}

void vui_progress_bar_render() {
	VuiCtrl* ctrl = DasStk_get(&_vui.build.w->ctrls, _vui.build.render_ctrl_idx);
	VuiCtrl* parent_ctrl = DasStk_get(&_vui.build.w->ctrls, _vui.build.parent_ctrl_idx);
	VuiProgressBarStyle* parent_style = (VuiProgressBarStyle*)parent_ctrl->style;
	vui_render_rect(ctrl->rect, parent_style->bar_color, parent_style->box.radius);
	vui_ctrl_render();
}

void vui_progress_bar(VuiVec2 size, float value, float min, float max, VuiProgressBarStyle* style) {
	value = vui_clamp(value, min, max);

	vui_box_start(0, size, &style->box);

	size.x -= VuiThickness_horizontal(style->box.ctrl.padding) + style->box.border.width * 2;
	size.y -= VuiThickness_vertical(style->box.ctrl.padding) + style->box.border.width * 2;
	float ratio = size.x / (max - min);
	size.x = ratio * (value - min);
	vui_ctrl_start(0, size, vui_progress_bar_render, &VuiCtrlStyle_zero);
	vui_ctrl_end();

	vui_box_end();
}

void vui_scroll_bar(VuiCtrlId id, float length, float* content_offset, float content_length, VuiBool is_horizontal, VuiScrollBarStyle* style) {
	VuiVec2 size = VuiVec2_init(length, length);
	if (is_horizontal) size.y = style->width;
	else size.x = style->width;

	vui_box_start(id, size, &style->box);
	vui_stack_layout_start(0, VuiVec2_fill, &VuiCtrlStyle_zero);

	//
	// work out the slider size
	VuiVec2 slider_size = VuiVec2_fill;
	float bar_content_length = 0;
	float* slider_long_side_length = NULL;
	VuiVec2 bar_content_size = vui_get_content_size();
	if (is_horizontal) {
		slider_long_side_length = &slider_size.x;
		bar_content_length = bar_content_size.x;
	} else {
		slider_long_side_length = &slider_size.y;
		bar_content_length = bar_content_size.y;
	}

	float min_slider_len = style->width / 2.0;
	// calculate the long side length of the slider that is clamp to a min(min_slider_len) and vui_max(bar_content_length)
	float unclamped_slider_len = bar_content_length * (length / content_length);
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
	float max_slider_offset_clamp_ratio = max_slider_offset / unclampled_max_slider_offset;

	// convert content offset into a slider offset
	// content_offset is always <= 0 and slider_offset is >= 0
	float slider_offset = ((-*content_offset / content_length) * (bar_content_length * max_slider_offset_clamp_ratio));
	slider_offset = vui_max(0.0, vui_min(slider_offset, max_slider_offset));
	if (is_horizontal) slider_offset_vec.x = slider_offset;
	else slider_offset_vec.y = slider_offset;

	vui_stack_layout_set_next_pos(VuiAlign_top_left, slider_offset_vec);
	if (vui_press_button_start(1, slider_size, &style->slider)) {
		float pos_offset = is_horizontal
			? _vui.input.mouse.offset_x
			: _vui.input.mouse.offset_y;

		// offset the slider offset and convert the offset into a content offset
		slider_offset = vui_max(0.0, vui_min(slider_offset + pos_offset, max_slider_offset));
		*content_offset = -(slider_offset / (bar_content_length * max_slider_offset_clamp_ratio)) * content_length;
	}
	vui_press_button_end();

	vui_stack_layout_end(VuiVec2_zero);
	vui_box_end();
}

typedef struct {
    VuiScrollBarStyle* sb_style;
    VuiVec2* size;
    VuiVec2* content_offset;
    VuiScrollViewFlags flags;
	VuiCtrlIdHash id_hash;
} _VuiScrollViewState;
typedef_DasStk(_VuiScrollViewState);

DasStk(_VuiScrollViewState) _vui_scroll_view_state_stk = {0};

void vui_scroll_view_start(VuiCtrlId id, VuiVec2* size, VuiVec2* content_offset, VuiScrollViewFlags flags, VuiScrollViewStyle* style) {
	vui_box_start(id | VuiCtrlId_scrollable_mask, *size, &style->box);

	VuiCtrl* ctrl = DasStk_get(&_vui.build.w->ctrls, _vui.build.parent_ctrl_idx);
	vui_assert(
		!(flags & VuiScrollViewFlags_horizontal_scroll) || isfinite(ctrl->rect.bottom_right.x),
		"scroll view has horizontal scroll flags, so it must have a known width. "
		"be careful when using vui_fill_len as it can inherit vui_auto_len from its parent");
	vui_assert(
		!(flags & VuiScrollViewFlags_vertical_scroll) || isfinite(ctrl->rect.bottom_right.y),
		"scroll view has vertical scroll flags, so it must have a known height. "
		"be careful when using vui_fill_len as it can inherit vui_auto_len from its parent");

	if (size->x == vui_fill_len) size->x = (ctrl->rect.bottom_right.x - ctrl->rect.top_left.x) + style->box.border.width * 2;
	if (size->y == vui_fill_len) size->y = (ctrl->rect.bottom_right.y - ctrl->rect.top_left.y) + style->box.border.width * 2;

	_VuiScrollViewState* state = DasStk_push(&_vui_scroll_view_state_stk, NULL);
	state->id_hash = vui_get_ctrl_id_hash();
	state->sb_style = &style->scroll_bar;
	state->size = size;
	state->content_offset = content_offset;
	state->flags = flags;

	vui_stack_layout_start(0, VuiVec2_fill, &VuiCtrlStyle_zero);
	vui_stack_layout_set_next_pos(VuiAlign_top_left, *content_offset);
	vui_container_layout_start(1, VuiVec2_auto, &VuiCtrlStyle_zero);
}

void vui_scroll_view_end() {
	_VuiScrollViewState* state = DasStk_get_last(&_vui_scroll_view_state_stk);
	DasStk_pop(&_vui_scroll_view_state_stk, NULL);

	VuiCtrl* container_ctrl = vui_get_ctrl();
	vui_container_layout_end();

	// if we have scroll focus and the mouse wheel has moved.
	// offset the content_offset using the mouse wheel offset.
	if (vui_ctrl_is_scroll_mouse_focused(state->id_hash)) {
		VuiVec2 mouse_wheel_offset = VuiVec2_init(_vui.input.mouse.wheel_offset_x, _vui.input.mouse.wheel_offset_y);
		if ((state->flags & VuiScrollViewFlags_vertical_scroll) != VuiScrollViewFlags_vertical_scroll)  mouse_wheel_offset.y = 0;
		if ((state->flags & VuiScrollViewFlags_horizontal_scroll) != VuiScrollViewFlags_horizontal_scroll)  mouse_wheel_offset.x = 0;
		if (mouse_wheel_offset.x != 0.0 || mouse_wheel_offset.y != 0.0) {
			*state->content_offset = VuiVec2_add(*state->content_offset, mouse_wheel_offset);
		}
	}

	VuiVec2 content_size = VuiVec2_sub(container_ctrl->rect.bottom_right, container_ctrl->rect.top_left);

	VuiCtrl* stack_layout_ctrl = DasStk_get(&_vui.build.w->ctrls, _vui.build.parent_ctrl_idx);
	VuiVec2 scroll_view_content_size = VuiRect_size(stack_layout_ctrl->rect);
	if (!isfinite(scroll_view_content_size.x)) scroll_view_content_size.x = content_size.x;
	if (!isfinite(scroll_view_content_size.y)) scroll_view_content_size.y = content_size.y;

	float sb_width = state->sb_style->width;
	if ((state->flags & VuiScrollViewFlags_resizable) == VuiScrollViewFlags_resizable) {
		scroll_view_content_size.x -= sb_width + state->sb_style->box.border.width;
		scroll_view_content_size.y -= sb_width + state->sb_style->box.border.width;
	}

    VuiBool show_vertical =
		(state->flags & VuiScrollViewFlags_vertical_scroll) == VuiScrollViewFlags_vertical_scroll &&
			((state->flags & VuiScrollViewFlags_always_show_vertical_bar) == VuiScrollViewFlags_always_show_vertical_bar ||
				scroll_view_content_size.y < content_size.y);
    VuiBool show_horizontal =
		(state->flags & VuiScrollViewFlags_horizontal_scroll) == VuiScrollViewFlags_horizontal_scroll &&
			((state->flags & VuiScrollViewFlags_always_show_horizontal_bar) == VuiScrollViewFlags_always_show_horizontal_bar ||
				scroll_view_content_size.x < content_size.x);

	if ((state->flags & VuiScrollViewFlags_resizable) != VuiScrollViewFlags_resizable && show_vertical && show_horizontal) {
		scroll_view_content_size.x -= sb_width + state->sb_style->box.border.width;
	}

	if (show_vertical) {
		// set the pos by aligning the scroll bar to the top right
		vui_stack_layout_set_next_pos(VuiAlign_top_right, VuiVec2_zero);
		vui_scroll_bar(2, scroll_view_content_size.y, &state->content_offset->y, content_size.y, vui_false, state->sb_style);
		if (stack_layout_ctrl->rect.bottom_right.x == vui_auto_len) {
			stack_layout_ctrl->max_bottom_right.x += vui_ctrl_get_last_size().x;
		}
	}

	if (show_horizontal) {
		// set the pos by aligning the scroll bar to the top right
		vui_stack_layout_set_next_pos(VuiAlign_bottom_left, VuiVec2_zero);
		vui_scroll_bar(3, scroll_view_content_size.x, &state->content_offset->x, content_size.x, vui_true, state->sb_style);
		if (stack_layout_ctrl->rect.bottom_right.y == vui_auto_len) {
			stack_layout_ctrl->max_bottom_right.y += vui_ctrl_get_last_size().y;
		}
	}

	if ((state->flags & VuiScrollViewFlags_resizable) == VuiScrollViewFlags_resizable) {
		VuiVec2 gap_size = VuiVec2_init(sb_width, sb_width);
		vui_stack_layout_set_next_pos(VuiAlign_bottom_right, VuiVec2_zero);
		if (vui_press_button_start(4, gap_size, &state->sb_style->slider)) {
			float w = state->size->x;
			float h = state->size->y;
			w += _vui.input.mouse.offset_x;
			h += _vui.input.mouse.offset_y;

			VuiCtrl* ctrl = DasStk_get(&_vui.build.w->ctrls, _vui.build.parent_ctrl_idx);
			float min = sb_width * 2 + state->sb_style->box.border.width;
			float w_max = _vui.build.w->size.x - ctrl->rect.top_left.x;
			float h_max = _vui.build.w->size.y - ctrl->rect.top_left.y;
			if (stack_layout_ctrl->rect.bottom_right.x != vui_auto_len)
				state->size->x = vui_clamp(w, min, w_max);
			if (stack_layout_ctrl->rect.bottom_right.y != vui_auto_len)
				state->size->y = vui_clamp(h, min, h_max);
		}
		vui_press_button_end();
	}

	// max content_size with the scroll_view_content_size so we only see values that express the content exceeding the scroll_view.
	VuiVec2 min_content_offset = VuiVec2_scale(VuiVec2_sub(VuiVec2_max(content_size, scroll_view_content_size), scroll_view_content_size), -1.0);
	*state->content_offset = VuiVec2_min(VuiVec2_zero, VuiVec2_max(*state->content_offset, min_content_offset));

	vui_stack_layout_end(VuiVec2_zero);
	vui_box_end();
}

VuiBool vui_text_box_(VuiCtrlId id, float width, DasStk(char)* string, _VuiInputBoxType type, VuiTextBoxStyle* style) {
	VuiBoxStyle* bs = vui_frame_data_alloc_elmt(VuiBoxStyle);
	bs->ctrl = style->ctrl;
	bs->radius = style->radius;
	bs->border.width = style->border_width;
	VuiCtrlIdHash id_hash = vui_ctrl_gen_id_hash(id | VuiCtrlId_focusable_mask);
	if (vui_ctrl_is_focused(id_hash) || vui_ctrl_is_mouse_focused(id_hash)) {
		bs->bg_color = style->focused_bg_color;
		bs->border.color = style->focused_border_color;
	} else {
		bs->bg_color = style->inactive_bg_color;
		bs->border.color = style->inactive_border_color;
	}

	float height = style->text_font.line_height + VuiThickness_vertical(style->ctrl.padding) + style->border_width * 2;
	vui_box_start(id | VuiCtrlId_focusable_mask, VuiVec2_init(width, height), bs);

	// call this to set the focus if a mouse presses on the box
    vui_ctrl_focus_state(id_hash);

	if (vui_ctrl_is_focused(id_hash) && type != _VuiInputBoxType_text) {
		string = &_vui.input.input_box_edit_string;
	}

	VuiBool has_changed = vui_false;
	if (vui_ctrl_is_focused(id_hash)) {
		if (_vui.input.text_box_string == NULL) { // if gained focus
			if (type != _VuiInputBoxType_text) {
				string->count = 0;
				DasStk_push_many(string, _vui.input.input_box_string.DasStk_data, _vui.input.input_box_string.count);
			}
			_vui.text_box_focus_change_fn(vui_true);
			_vui.input.text_box_string = string;
			_vui.input.text_box_cursor_idx = 0;
			_vui.input.text_box_select_offset = string->count;
			_vui.input.text_box_type = type;
		} else {
			has_changed = _vui.input.has_changed;
		}
	}

	vui_stack_layout_start(1, VuiVec2_fill, &VuiCtrlStyle_zero);

	VuiTextStyle* ts = vui_frame_data_alloc_elmt(VuiTextStyle);
	ts->font = style->text_font;
	ts->color = style->text_color;
	vui_text_(string->DasStk_data, string->count, 0, ts);

	if (vui_ctrl_is_focused(id_hash)) {
		VuiVec2 start_idx_offset = _vui.position_text_fn(_vui.position_text_userdata, style->text_font.user_id, string->DasStk_data, _vui.input.text_box_cursor_idx, VuiVec2_zero, NULL);
		float cursor_width = 0;
		bs = vui_frame_data_alloc_elmt(VuiBoxStyle);
		if (_vui.input.text_box_select_offset) {
			VuiVec2 end_idx_offset = _vui.position_text_fn(_vui.position_text_userdata, style->text_font.user_id, string->DasStk_data, _vui.input.text_box_cursor_idx + _vui.input.text_box_select_offset, VuiVec2_zero, NULL);
			if (_vui.input.text_box_select_offset < 0) {
				VuiVec2 tmp = end_idx_offset;
				end_idx_offset = start_idx_offset;
				start_idx_offset = tmp;
			}

			bs->radius = style->selection_radius;
			bs->bg_color = style->selection_color;

			cursor_width = end_idx_offset.x - start_idx_offset.x;
		} else {
			cursor_width = style->cursor_width;
			bs->radius = style->cursor_radius;
			bs->bg_color = style->cursor_color;
		}

		vui_stack_layout_set_next_pos(VuiAlign_top_left, VuiVec2_init(start_idx_offset.x, 0));
		vui_box_start(1, VuiVec2_init(cursor_width, style->text_font.line_height), bs);
		vui_box_end();
	}

	vui_stack_layout_end(VuiVec2_zero);

	vui_box_end();
	return has_changed;
}

VuiBool vui_text_box(VuiCtrlId id, float width, DasStk(char)* string, VuiTextBoxStyle* style) {
	vui_assert(string, "a string buffer must be provided");
	return vui_text_box_(id, width, string, _VuiInputBoxType_text, style);
}

VuiBool vui_input_box_uint(VuiCtrlId id, float width, uint32_t* value, VuiTextBoxStyle* style) {
	VuiCtrlIdHash id_hash = vui_ctrl_gen_id_hash(id | VuiCtrlId_focusable_mask);
	DasStk(char)* string = &_vui.input.input_box_string;
	string->count = 0;
	DasStk_push_str_fmt(string, "%u", *value);

	VuiBool has_changed = vui_text_box_(id, width, string, _VuiInputBoxType_u32, style);

	if (vui_ctrl_is_focused(id_hash)) {
		DasStk(char)* string = &_vui.input.input_box_edit_string;
		// strtoul requires a null terminated string,
		// so put a null byte at the end and remove it after.
		DasStk_push_many(string, "\0", 1);
		*value = strtoul(string->DasStk_data, NULL, 10);
		string->count -= 1;
	}
	return has_changed;
}

VuiBool vui_input_box_sint(VuiCtrlId id, float width, int32_t* value, VuiTextBoxStyle* style) {
	VuiCtrlIdHash id_hash = vui_ctrl_gen_id_hash(id | VuiCtrlId_focusable_mask);
	DasStk(char)* string = &_vui.input.input_box_string;
	string->count = 0;
	DasStk_push_str_fmt(string, "%d", *value);

	VuiBool has_changed = vui_text_box_(id, width, string, _VuiInputBoxType_s32, style);

	if (vui_ctrl_is_focused(id_hash)) {
		DasStk(char)* string = &_vui.input.input_box_edit_string;
		// strtoul requires a null terminated string,
		// so put a null byte at the end and remove it after.
		DasStk_push_many(string, "\0", 1);
		*value = strtol(string->DasStk_data, NULL, 10);
		string->count -= 1;
	}
	return has_changed;
}

VuiBool vui_input_box_float(VuiCtrlId id, float width, float* value, VuiTextBoxStyle* style) {
	VuiCtrlIdHash id_hash = vui_ctrl_gen_id_hash(id | VuiCtrlId_focusable_mask);
	DasStk(char)* string = &_vui.input.input_box_string;
	string->count = 0;
	DasStk_push_str_fmt(string, "%f", *value);

	VuiBool has_changed = vui_text_box_(id, width, string, _VuiInputBoxType_float, style);

	if (vui_ctrl_is_focused(id_hash)) {
		DasStk(char)* string = &_vui.input.input_box_edit_string;
		// strtod requires a null terminated string,
		// so put a null byte at the end and remove it after.
		DasStk_push_many(string, "\0", 1);
		*value = strtod(string->DasStk_data, NULL);
		string->count -= 1;
	}
	return has_changed;
}

//
// =============================================
//
// vui
//
// =============================================
//

char* VuiLayoutType_strings[] = {
	"container",
	"stack",
	"row",
	"column",
};

VuiBool vui_init(VuiSetup* setup) {
	_vui = (_Vui){0};
	_vui.position_text_fn = setup->position_text_fn;
	_vui.position_text_userdata = setup->position_text_userdata;
	_vui.text_box_focus_change_fn = setup->text_box_focus_change_fn;
	_VuiArenaAlctor_init(&_vui.frame_data_alctor);
	_vui.windows = das_alloc_array(_VuiWindow, setup->windows_count);
	memset(_vui.windows, 0, setup->windows_count * sizeof(*_vui.windows));
	_vui.windows_count = setup->windows_count;
	return vui_true;
}

VuiCtrl* vui_get_render_ctrl() {
	vui_assert(_vui.build.render_ctrl_idx > 0, "vui_get_render_ctrl can only be called inside of render callbacks");
	return DasStk_get(&_vui.build.w->ctrls, _vui.build.render_ctrl_idx);
}

VuiCtrlIdHash vui_get_ctrl_id_hash() {
	return DasStk_get(&_vui.build.w->ctrls, _vui.build.parent_ctrl_idx)->id_hash;
}

VuiVec2 vui_get_content_size() {
	return VuiRect_size(DasStk_get(&_vui.build.w->ctrls, _vui.build.parent_ctrl_idx)->rect);
}

VuiCtrl* vui_get_ctrl() {
	return DasStk_get(&_vui.build.w->ctrls, _vui.build.parent_ctrl_idx);
}

VuiVec2 vui_next_ctrl_top_left() {
	VuiCtrl* ctrl = DasStk_get(&_vui.build.w->ctrls, _vui.build.parent_ctrl_idx);
	vui_assert(
		ctrl->layout.type == VuiLayoutType_row || ctrl->layout.type == VuiLayoutType_column,
		"vui_next_ctrl_top_left has only been implemented for row and column layouts right now.");
	return ctrl->layout.row_column.next_ctrl_top_left;
}

void vui_frame_start() {
	vui_assert(_vui.build.w == NULL, "cannot call vui_frame_start until vui_window_end has been called");
	_VuiArenaAlctor_reset(&_vui.frame_data_alctor);

	_vui.mouse_focused_ctrl_id_hash = 0;
	_vui.scroll_mouse_focused_ctrl_id_hash = 0;
	_vui.input.is_mouse_over_ctrl = vui_false;

	_VuiWindow* windows = _vui.windows;
	uint16_t windows_count = _vui.windows_count;
	for (int i = 0; i < windows_count; i += 1) {
		_VuiWindow* w = &windows[i];
		_vui.build.w = w;

		if (_vui.mouse_focused_window_id == i && w->ctrls.count > 0) {
			_vui.build.render_ctrl_idx = 0;
			_vui.build.parent_ctrl_idx = 0;
			_vui.build.render_clip_rect = VuiRect_init(0, 0, w->size.x, w->size.y);
			_vui_find_focused_ctrls();
		}

		w->ctrls.count = 0;
		w->size.x = 0;
		w->size.y = 0;
		w->text.count = 0;
	}

	if (_vui.input.text_box_string) {
		VuiInputActions actions = _vui.input.actions;
		if (actions & VuiInputActions_left) {
			if ((actions & VuiInputActions_select_word_start) == VuiInputActions_select_word_start) {
			} else if ((actions & VuiInputActions_word_start) == VuiInputActions_word_start) {
			} else if ((actions & VuiInputActions_select_left) == VuiInputActions_select_left) {
				if (_vui.input.text_box_cursor_idx + _vui.input.text_box_select_offset) {
					_vui.input.text_box_select_offset -= 1;
				}
			} else {
				if (_vui.input.text_box_cursor_idx) {
					if (_vui.input.text_box_select_offset < 0) {
						_vui.input.text_box_cursor_idx += _vui.input.text_box_select_offset;
					} else if (_vui.input.text_box_select_offset == 0) {
						_vui.input.text_box_cursor_idx -= 1;
					}
				}
				_vui.input.text_box_select_offset = 0;
			}
		} else if (actions & VuiInputActions_right) {
			if ((actions & VuiInputActions_select_word_end) == VuiInputActions_select_word_end) {
			} else if ((actions & VuiInputActions_word_end) == VuiInputActions_word_end) {
			} else if ((actions & VuiInputActions_select_right) == VuiInputActions_select_right) {
				if (_vui.input.text_box_cursor_idx + _vui.input.text_box_select_offset < _vui.input.text_box_string->count) {
					_vui.input.text_box_select_offset += 1;
				}

			} else {
				uint32_t end_idx = _vui.input.text_box_cursor_idx + _vui.input.text_box_select_offset;
				if (end_idx <= _vui.input.text_box_string->count && _vui.input.text_box_select_offset > 0) {
					_vui.input.text_box_cursor_idx += _vui.input.text_box_select_offset;
				} else if (end_idx < _vui.input.text_box_string->count && _vui.input.text_box_select_offset == 0) {
					_vui.input.text_box_cursor_idx += 1;
				}

				_vui.input.text_box_select_offset = 0;
			}
		} else if (actions & (VuiInputActions_backspace | VuiInputActions_delete)) {
			uint32_t start_idx = 0;
			uint32_t end_idx = 0;
			if (_vui.input.text_box_select_offset > 0) {
				start_idx = _vui.input.text_box_cursor_idx;
				end_idx = _vui.input.text_box_cursor_idx + _vui.input.text_box_select_offset;
			} else {
				start_idx = _vui.input.text_box_cursor_idx + _vui.input.text_box_select_offset;
				end_idx = _vui.input.text_box_cursor_idx;
			}

			if ((actions & VuiInputActions_delete) == VuiInputActions_delete) {
				if ((actions & VuiInputActions_delete_to_end_of_word) == VuiInputActions_delete_to_end_of_word) {
					end_idx = _vui.input.text_box_string->count;
				} else if (end_idx < _vui.input.text_box_string->count) {
					end_idx += 1;
				}
			} else {
				if ((actions & VuiInputActions_delete_to_start_of_word) == VuiInputActions_delete_to_start_of_word) {
					start_idx = 0;
				} else if (start_idx > 0) {
					start_idx -= 1;
				}
			}

			_vui.input.text_box_cursor_idx = start_idx;
			_vui.input.text_box_select_offset = 0;

			if (start_idx != end_idx) {
				_vui.input.has_changed = vui_true;
			}
			DasStk_shift_remove_range(_vui.input.text_box_string, start_idx, end_idx, NULL);
		} else if (actions & VuiInputActions_home) {
			if ((actions & VuiInputActions_select_to_document_home) == VuiInputActions_select_to_document_home) {
				_vui.input.text_box_select_offset = -(int32_t)_vui.input.text_box_cursor_idx;
			} else if ((actions & VuiInputActions_document_home) == VuiInputActions_document_home) {
				_vui.input.text_box_cursor_idx = 0;
				_vui.input.text_box_select_offset = 0;
			} else if ((actions & VuiInputActions_select_home) == VuiInputActions_select_home) {
				uint32_t idx = _vui.input.text_box_cursor_idx + _vui.input.text_box_select_offset;
				while (idx) {
					char c = *DasStk_get(_vui.input.text_box_string, idx - 1);
					if (c == '\n' || c == '\r') break;
					idx -= 1;
				}
				_vui.input.text_box_select_offset = idx - _vui.input.text_box_cursor_idx;
			} else {
				while (_vui.input.text_box_cursor_idx) {
					char c = *DasStk_get(_vui.input.text_box_string, _vui.input.text_box_cursor_idx - 1);
					if (c == '\n' || c == '\r') break;
					_vui.input.text_box_cursor_idx -= 1;
				}
				_vui.input.text_box_select_offset = 0;
			}
		} else if (actions & VuiInputActions_end) {
			if ((actions & VuiInputActions_select_to_document_end) == VuiInputActions_select_to_document_end) {
				_vui.input.text_box_select_offset = _vui.input.text_box_string->count - _vui.input.text_box_cursor_idx;
			} else if ((actions & VuiInputActions_document_end) == VuiInputActions_document_end) {
				_vui.input.text_box_cursor_idx = _vui.input.text_box_string->count;
				_vui.input.text_box_select_offset = 0;
			} else if ((actions & VuiInputActions_select_end) == VuiInputActions_select_end) {
				uint32_t idx = _vui.input.text_box_cursor_idx + _vui.input.text_box_select_offset;
				while (idx < _vui.input.text_box_string->count) {
					char c = *DasStk_get(_vui.input.text_box_string, idx);
					if (c == '\n' || c == '\r') break;
					idx += 1;
				}
				_vui.input.text_box_select_offset = idx - _vui.input.text_box_cursor_idx;
			} else {
				while (_vui.input.text_box_cursor_idx < _vui.input.text_box_string->count) {
					char c = *DasStk_get(_vui.input.text_box_string, _vui.input.text_box_cursor_idx);
					if (c == '\n' || c == '\r') break;
					_vui.input.text_box_cursor_idx += 1;
				}
				_vui.input.text_box_select_offset = 0;
			}
		}
	}

	_vui.build.w = NULL;
}

uint32_t _vui_get_focused_ctrl_idx() {
	_VuiWindow* w = &_vui.windows[_vui.focused_window_id];
	VuiCtrlIdHash focused_ctrl_id_hash = w->focused_ctrl_id_hash;
	VuiCtrl* ctrls = w->ctrls.DasStk_data;
	for (uint32_t i = 0; i < w->ctrls.count; i += 1) {
		if (ctrls[i].id_hash == focused_ctrl_id_hash) return i;
	}

	return 0;
}

void vui_frame_end() {
	vui_assert(_vui.build.w == NULL, "cannot call vui_frame_end until vui_window_end has been called");

	if ((_vui.input.actions & VuiInputActions_focus_prev) == VuiInputActions_focus_prev) {
		_VuiWindow* w = &_vui.windows[_vui.focused_window_id];
		VuiCtrl* ctrls = w->ctrls.DasStk_data;
		for (uint32_t i = _vui_get_focused_ctrl_idx() - 1; i-- > 0;) {
			VuiCtrlIdHash id_hash = ctrls[i].id_hash;
			if (id_hash & VuiCtrlId_focusable_mask) {
				vui_ctrl_set_focused(id_hash);
				break;
			}
		}
	} else if ((_vui.input.actions & VuiInputActions_focus_next) == VuiInputActions_focus_next) {
		_VuiWindow* w = &_vui.windows[_vui.focused_window_id];
		uint32_t count = w->ctrls.count;
		VuiCtrl* ctrls = w->ctrls.DasStk_data;
		for (uint32_t i = _vui_get_focused_ctrl_idx() + 1; i < count; i += 1) {
			VuiCtrlIdHash id_hash = ctrls[i].id_hash;
			if (id_hash & VuiCtrlId_focusable_mask) {
				vui_ctrl_set_focused(id_hash);
				break;
			}
		}
	}

	_vui.input.mouse.offset_x = 0;
	_vui.input.mouse.offset_y = 0;
	_vui.input.mouse.wheel_offset_x = 0;
	_vui.input.mouse.wheel_offset_y = 0;
	_vui.input.mouse.buttons_has_been_pressed = 0;
	_vui.input.mouse.buttons_has_been_released = 0;
	_vui.input.actions = 0;
	_vui.input.has_changed = vui_false;
}

void _vui_window_assert_id(VuiWindowId id) {
	vui_assert(id < _vui.windows_count, "window id of %u must be less than VuiSetup.windows_count of %u", id, _vui.windows_count);
}

void vui_window_start(VuiWindowId id, VuiVec2 size) {
	vui_assert(_vui.build.w == NULL, "cannot call vui_window_start until vui_window_end has been called");
	_VuiWindow* w = &_vui.windows[id];
	vui_assert(w->ctrls.count == 0, "vui_window_start has already been called this frame for this window id %u", id);
	w->size = size;

	_vui.build = (_VuiBuild){0};
	_vui.build.w = w;

	VuiCtrl* root_ctrl = DasStk_push(&w->ctrls, NULL);
	*root_ctrl = (VuiCtrl){0};
	root_ctrl->rect = VuiRect_init_with_vecs(VuiVec2_zero, w->size);
	root_ctrl->style = &VuiCtrlStyle_zero;
	root_ctrl->render_fn = &vui_ctrl_render;
}

void vui_window_end() {
	vui_assert(_vui.build.w != NULL, "cannot call vui_window_end until vui_window_start has been called");
	_vui.build.w = NULL;
}

VuiRenderWindow* vui_window_build_render(VuiWindowId id, float scale_factor, VuiBool pixel_snapping) {
	_vui_window_assert_id(id);

	_VuiWindow* w = &_vui.windows[id];
	_vui.build.w = w;

	_vui.build.parent_ctrl_idx = 0;
	_vui.build.render_ctrl_idx = 0;
	_vui.build.render_text_idx = 0;
	_vui.build.render_clip_rect = VuiRect_init_with_vecs(VuiVec2_zero, w->size);

	VuiRenderLayer* layers = w->render.layers.DasStk_data;
	uint32_t layers_count = w->render.layers.count;
	for (int idx = 0; idx < layers_count; idx += 1) {
		VuiRenderLayer* layer = &layers[idx];
		layer->indices.count = 0;
		layer->verts.count = 0;
		layer->cmds.count = 0;
	}

	_vui.build.render_layer_idx = -1;
	vui_render_inc_layer();

	DasStk_get_first(&w->ctrls)->render_fn();

	uint64_t hash = 0;
	for (int idx = 0; idx < w->render.layers.count; idx += 1) {
		VuiRenderLayer* layer = DasStk_get(&w->render.layers, idx);
		VuiRenderVert* verts = layer->verts.DasStk_data;
		for (int v_idx = 0; v_idx < layer->verts.count; v_idx += 1) {
			if (scale_factor != 1.0) {
				verts[v_idx].pos.x = verts[v_idx].pos.x * scale_factor;
				verts[v_idx].pos.y = verts[v_idx].pos.y * scale_factor;
			}
			if (pixel_snapping) {
				verts[v_idx].pos.x = roundf(verts[v_idx].pos.x);
				verts[v_idx].pos.y = roundf(verts[v_idx].pos.y);
			}
		}
		hash = vui_fnv_hash_64((char*)layer->cmds.DasStk_data, layer->cmds.count * sizeof(VuiRenderCmd), hash);
		hash = vui_fnv_hash_64((char*)layer->verts.DasStk_data, layer->verts.count * sizeof(VuiRenderVert), hash);
		hash = vui_fnv_hash_64((char*)layer->indices.DasStk_data, layer->indices.count * sizeof(VuiRenderIdx), hash);
	}
	w->render.hash = hash;
	_vui.build.w = NULL;

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
	VuiRenderLayer* layers = w->render.layers.DasStk_data;
	for (int idx = 0; idx < w->render.layers.count; idx += 1) {
		VuiRenderLayer* layer = &layers[idx];

		fprintf(file, "LAYER %u\n", idx);
		fprintf(file, "cmds.count: %u\n", layer->cmds.count);
		fprintf(file, "verts.count: %u\n", layer->verts.count);
		fprintf(file, "indices.count: %u\n", layer->indices.count);

		fprintf(file, "cmds: {\n");
		VuiRenderCmd* cmds = layer->cmds.DasStk_data;
		for (int cmd_idx = 0; cmd_idx < layer->cmds.count; cmd_idx += 1) {
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
		VuiRenderVert* verts = layer->verts.DasStk_data;
		for (int vert_idx = 0; vert_idx < layer->verts.count; vert_idx += 1) {
			VuiRenderVert* vert = &verts[vert_idx];
			fprintf(file,
					"\t%u: { pos: [%f, %f], uv: [%f, %f], color: #%x%x%x%x }\n",
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
		VuiRenderIdx* indices = layer->indices.DasStk_data;
		for (int indice_idx = 0; indice_idx < layer->indices.count; indice_idx += 1) {
			VuiRenderIdx indice = indices[indice_idx];
			fprintf(file, "\t%u: %u\n", indice_idx, indice);
		}
		fprintf(file, "}\n");
	}
}

void* vui_frame_data_alloc(uint32_t size, uint32_t align) {
	return _VuiArenaAlctor_alloc(&_vui.frame_data_alctor, size, align);
}

