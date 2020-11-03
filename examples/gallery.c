
#define GL_GLEXT_PROTOTYPES

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#define STB_RECT_PACK_IMPLEMENTATION
#include "deps/stb_rect_pack.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "deps/stb_truetype.h"

#include "deps/das.h"
#include "deps/das.c"

#include "../vui.h"
#include "../vui.c"

#define screen_width 1280
#define screen_height 720

//
// =====================================
//
// vui styles
//
// =====================================
//

#define vui_default_padding VuiThickness_init_even(8.0)
#define vui_default_margin VuiThickness_init_even(4.0)

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

#define vui_default_font (VuiFont) { .line_height = 18.0, .user_id = 0, }

VuiCtrlStyle ctrl_style = { vui_default_margin, vui_default_padding };

VuiTextStyle section_text_style = {
	.ctrl = { vui_default_margin, vui_default_padding },
	.color = vui_color_white,
	.font = { .user_id = 0, .line_height = 32 },
};

VuiTextButtonStyle green_text_button_style = {
	.button = {
		.ctrl = { vui_default_margin, vui_default_padding },
		.radius = 1.0,
		.border_width = 3.0,
		.inactive_bg_color = vui_color_nephritis,
		.inactive_border_color = vui_color_asbestos,
		.focused_bg_color = vui_color_emerald,
		.focused_border_color = vui_color_concrete,
		.pressed_bg_color = vui_color_concrete,
		.pressed_border_color = vui_color_emerald,
	},
	.inactive_color = vui_color_white,
	.focused_color = vui_color_white,
	.pressed_color = vui_color_black,
	.font = { .user_id = 0, .line_height = 32 },
};

VuiTextButtonStyle red_text_button_style = {
	.button = {
		.ctrl = { vui_default_margin, vui_default_padding },
		.radius = 1.0,
		.border_width = 3.0,
		.inactive_bg_color = vui_color_pomegranate,
		.inactive_border_color = vui_color_asbestos,
		.focused_bg_color = vui_color_alizarin,
		.focused_border_color = vui_color_concrete,
		.pressed_bg_color = vui_color_concrete,
		.pressed_border_color = vui_color_alizarin,
	},
	.inactive_color = vui_color_white,
	.focused_color = vui_color_white,
	.pressed_color = vui_color_black,
	.font = { .user_id = 0, .line_height = 32 },
};

VuiSeparatorStyle seperator_style = {
	.ctrl = { vui_default_margin, vui_default_padding },
	.color = vui_color_asbestos,
	.width = 4,
	.radius = 10.0,
};

VuiTextBoxStyle text_box_style = {
	.ctrl = { vui_default_margin, VuiThickness_init_even(4.0) },
	.radius = 1.0,
	.border_width = 3.0,
	.inactive_bg_color = vui_color_midnight_blue,
	.inactive_border_color = vui_color_wisteria,
	.focused_bg_color = vui_color_midnight_blue,
	.focused_border_color = vui_color_amethyst,
	.text_color = vui_color_white,
	.text_font = { .user_id = 0, .line_height = 32 },
	.selection_color = VuiColor_init(0x20, 0xac, 0xec, 0x88),
	.selection_radius = 1.0,
	.cursor_color = vui_color_amethyst,
	.cursor_width = 2.0,
	.cursor_radius = 1.0,
};

VuiTextCheckBoxStyle text_check_box_style = {
	.check_box = {
		.ctrl = { vui_default_margin, VuiThickness_init_even(4.0) },
		.radius = 2.0,
		.border_width = 2.0,
		.inactive_bg_color = vui_color_midnight_blue,
		.inactive_border_color = vui_color_wisteria,
		.focused_bg_color = vui_color_midnight_blue,
		.focused_border_color = vui_color_amethyst,
		.checked_color = vui_color_amethyst,
		.size = 24,
	},
	.color = vui_color_white,
	.font = { .user_id = 0, .line_height = 32 },
};

VuiTextRadioButtonStyle text_radio_button_style = {
	.radio_button = {
		.ctrl = { vui_default_margin, VuiThickness_init_even(2) },
		.border_width = 2.0,
		.inactive_bg_color = vui_color_midnight_blue,
		.inactive_border_color = vui_color_green_sea,
		.focused_bg_color = vui_color_midnight_blue,
		.focused_border_color = vui_color_turquoise,
		.selected_color = vui_color_turquoise,
		.size = 24,
		.selected_size = 12,
	},
	.color = vui_color_white,
	.font = { .user_id = 0, .line_height = 32 },
};

VuiProgressBarStyle progress_bar_style = {
	.box = {
		.ctrl = { vui_default_margin, VuiThickness_init_even(4) },
		.bg_color = vui_color_pumpkin,
		.border = { .color = vui_color_asbestos, .width = 4.0 },
		.radius = 4.0,
	},
	.bar_color = vui_color_sun_flower,
};

VuiScrollViewStyle scroll_view_style = {
	.box = {
		.ctrl = { vui_default_margin, vui_default_padding },
		.bg_color = VuiColor_transparent,
		.border = { .color = vui_color_asbestos, .width = 1.0 },
		.radius = 1.0,
	},
	.scroll_bar = {
		.box = {
			.ctrl = {0},
			.bg_color = vui_color_midnight_blue,
			.border = { .color = vui_color_wet_asphalt, .width = 2.0 },
			.radius = 1.0,
		},
		.width = 20.0,
		.slider = {
			.ctrl = { vui_default_margin, vui_default_padding },
			.radius = 1.0,
			.border_width = 3.0,
			.inactive_bg_color = vui_color_carrot,
			.inactive_border_color = vui_color_pumpkin,
			.focused_bg_color = vui_color_orange,
			.focused_border_color = vui_color_carrot,
			.pressed_bg_color = vui_color_sun_flower,
			.pressed_border_color = vui_color_orange,
		},
	}
};

typedef struct {
	stbtt_fontinfo* info;
	DasStk(int32_t) codepts;
	stbtt_packedchar* codepts_packed_chars;
	uint32_t codepts_packed_chars_cap;
	float scale;
} ScaledFont;

void ScaledFont_init(ScaledFont* font, stbtt_fontinfo* info, float pixel_height) {
	*font = (ScaledFont){0};
	font->info = info;
	font->scale = stbtt_ScaleForPixelHeight(info, pixel_height);
}

void ScaledFont_clear_codepts(ScaledFont* f) {
	f->codepts.count = 0;
}

void ScaledFont_add_codept(ScaledFont* f, int32_t codept) {
	int32_t* codepts = f->codepts.DasStk_data;
	for (uint32_t i = 0; i < f->codepts.count; i += 1) {
		if (codepts[i] == codept) return;
	}

	DasStk_push(&f->codepts, &codept);
}

VuiBool ScaledFont_pack_codepts(ScaledFont* f, stbtt_pack_context* spc) {
	if (f->codepts_packed_chars_cap != f->codepts.cap) {
		f->codepts_packed_chars = das_realloc_array(f->codepts_packed_chars, f->codepts_packed_chars_cap, f->codepts.cap);
	}

	stbtt_pack_range r = {0};
	r.font_size = 32;
	r.chardata_for_range = f->codepts_packed_chars;
	r.num_chars = f->codepts.count;
	r.array_of_unicode_codepoints = f->codepts.DasStk_data;

	return stbtt_PackFontRanges(spc, f->info->data, 0, &r, 1) == 1;
}

uint32_t ScaledFont_get_codept_idx(ScaledFont* f, int32_t codept) {
	int32_t* codepts = f->codepts.DasStk_data;
	for (uint32_t i = 0; i < f->codepts.count; i += 1) {
		if (codepts[i] == codept) return i;
	}

	vui_assert(vui_false, "cannot find codept %lc in the scaled font codept cache. make sure codepoint is added with ScaledFont_add_codept", codept);
}

stbtt_fontinfo liberation_sans = {0};
stbtt_fontinfo liberation_mono = {0};

ScaledFont liberation_sans_32px = {0};
ScaledFont liberation_mono_32px = {0};

typedef struct {
	uint8_t* pixels;
	uint32_t width;
	uint32_t height;
} GlyphTexture;

GlyphTexture glyph_texture = {0};

const char* vertex_shader_src =
	"#version 330 core\n"
	"layout(location = 0) in vec2 v_pos;\n"
	"layout(location = 1) in vec2 v_uv;\n"
	"layout(location = 2) in vec4 v_color;\n"
	"smooth out vec2 f_uv;\n"
	"smooth out vec4 f_color;\n"
	"uniform mat4 u_mvp;\n"
	"void main() {\n"
	"  gl_Position = u_mvp * vec4(v_pos, 0.0, 1.0);\n"
	"  f_uv = v_uv;\n"
	"  f_color = v_color;\n"
	"}";

const char* fragment_shader_src =
	"#version 330 core\n"
	"smooth in vec2 f_uv;\n"
	"smooth in vec4 f_color;\n"
	"out vec4 out_color;\n"
	"uniform sampler2D u_texture;\n"
	"void main() {\n"
	"  if (f_uv == vec2(0.0)) {\n"
	"	out_color = f_color;\n"
	"  } else if (f_uv.x < 0.0) {\n"
	"	out_color = vec4(1.0, 1.0, 1.0, texture(u_texture, -f_uv).r) * f_color;\n"
	"  } else {\n"
	"	out_color = texture(u_texture, f_uv) * f_color;\n"
	"  }\n"
	"}";

void update_glyph_texture() {
	stbtt_pack_context pack_ctx = {0};
	vui_assert(stbtt_PackBegin(&pack_ctx, glyph_texture.pixels, glyph_texture.width, glyph_texture.height, 0, 1, NULL), "failed to create packing context");

	vui_assert(ScaledFont_pack_codepts(&liberation_sans_32px, &pack_ctx), "failed to pack codepoints in texture");
	vui_assert(ScaledFont_pack_codepts(&liberation_mono_32px, &pack_ctx), "failed to pack codepoints in texture");

	stbtt_PackEnd(&pack_ctx);
}

char* control_texts[] = {"here", "are", "some", "text", "controls"};
VuiRect image_uv_rects[] = {
	VuiRect_init(0, 0, 1, 1),
	VuiRect_init(1, 1, 0, 0),
	VuiRect_init(0, 0, 0.5, 0.5),
	VuiRect_init(1, 0, 0, 1),
	VuiRect_init(0.5, 0.5, 1, 1),
};

void build_ui() {
	ScaledFont_clear_codepts(&liberation_sans_32px);
	ScaledFont_clear_codepts(&liberation_mono_32px);

	vui_frame_start();

	vui_window_start(0, VuiVec2_init(screen_width, screen_height));

	static VuiVec2 sv_size = VuiVec2_fill;
	static VuiVec2 sv_content_offset = VuiVec2_zero;
	vui_scroll_view_start(1, &sv_size, &sv_content_offset, VuiScrollViewFlags_vertical_scroll | VuiScrollViewFlags_always_show_horizontal_bar | VuiScrollViewFlags_resizable, &scroll_view_style);

	VuiCtrlId start_id = 22;
	vui_row_layout_start(0, VuiVec2_init(vui_auto_len, vui_auto_len), &VuiRowColumnLayoutStyle_zero);
	{
		static DasStk(char) text_box_string_a = {0};
		static DasStk(char) text_box_string_b = {0};
		vui_text_box(1203, 200, &text_box_string_a, &text_box_style);
		vui_text_box(1204, 200, &text_box_string_b, &text_box_style);

		vui_text("Buttons", 0, &section_text_style);

		vui_column_layout_start(0, VuiVec2_init(vui_auto_len, vui_auto_len), &VuiRowColumnLayoutStyle_zero);

		for (int i = 0; i < sizeof(control_texts) / sizeof(*control_texts); i += 1) {
			if (vui_text_button(start_id + i, control_texts[i], &green_text_button_style)) {
				printf("Buttons.%s has been pressed\n", control_texts[i]);
			}
		}
		start_id += sizeof(control_texts) / sizeof(*control_texts);

		vui_column_layout_end();

		vui_column_layout_start(0, VuiVec2_init(vui_auto_len, vui_auto_len), &VuiRowColumnLayoutStyle_zero);

		for (int i = 0; i < sizeof(image_uv_rects) / sizeof(*image_uv_rects); i += 1) {
			if (vui_image_button(start_id + i, VuiVec2_init(100, 50), 1, image_uv_rects[i], VuiColor_white, &green_text_button_style.button)) {
				printf("Buttons.image has been pressed\n");
			}
		}
		start_id += sizeof(image_uv_rects) / sizeof(*image_uv_rects);

		vui_column_layout_end();

		vui_separator(&seperator_style);
	}

	{
		vui_text("Toggle_buttons", 0, &section_text_style);

		vui_column_layout_start(0, VuiVec2_init(vui_auto_len, vui_auto_len), &VuiRowColumnLayoutStyle_zero);

		static VuiBool text_state[sizeof(control_texts) / sizeof(*control_texts)] = {0};

		for (int i = 0; i < sizeof(control_texts) / sizeof(*control_texts); i += 1) {
			if (vui_text_toggle_button(start_id + i, &text_state[i], control_texts[i], &red_text_button_style)) {
				printf("Toggle_Buttons.%s is selected\n", control_texts[i]);
			}
		}
		start_id += sizeof(control_texts) / sizeof(*control_texts);

		vui_column_layout_end();

		static VuiBool image_state[sizeof(image_uv_rects) / sizeof(*image_uv_rects)] = {0};
		vui_column_layout_start(0, VuiVec2_init(vui_auto_len, vui_auto_len), &VuiRowColumnLayoutStyle_zero);

		for (int i = 0; i < sizeof(image_uv_rects) / sizeof(*image_uv_rects); i += 1) {
			if (vui_image_toggle_button(start_id + i, &image_state[i], VuiVec2_init(100, 50), 1, image_uv_rects[i], VuiColor_white, &red_text_button_style.button)) {
				printf("Toggle_Buttons.image is selected\n");
			}
		}
		start_id += sizeof(image_uv_rects) / sizeof(*image_uv_rects);

		vui_column_layout_end();

		vui_separator(&seperator_style);
	}

	{
		vui_text("Select_buttons", 0, &section_text_style);

		vui_column_layout_start(0, VuiVec2_init(vui_auto_len, vui_auto_len), &VuiRowColumnLayoutStyle_zero);

		static VuiCtrlId selected_button_id = 0;
		for (int i = 0; i < sizeof(control_texts) / sizeof(*control_texts); i += 1) {
			if (vui_text_select_button(start_id + i, &selected_button_id, control_texts[i], &red_text_button_style)) {
				printf("Select_Buttons.%s is selected\n", control_texts[i]);
			}
		}
		start_id += sizeof(control_texts) / sizeof(*control_texts);

		vui_column_layout_end();

		static VuiBool image_state[sizeof(image_uv_rects) / sizeof(*image_uv_rects)] = {0};
		vui_column_layout_start(0, VuiVec2_init(vui_auto_len, vui_auto_len), &VuiRowColumnLayoutStyle_zero);

		for (int i = 0; i < sizeof(image_uv_rects) / sizeof(*image_uv_rects); i += 1) {
			if (vui_image_select_button(start_id + i, &selected_button_id, VuiVec2_init(100, 50), 1, image_uv_rects[i], VuiColor_white, &red_text_button_style.button)) {
				printf("Select_Buttons.image is selected\n");
			}
		}
		start_id += sizeof(image_uv_rects) / sizeof(*image_uv_rects);
		vui_column_layout_end();

		vui_separator(&seperator_style);
	}

	{
		vui_text("Check_Boxes", 0, &section_text_style);

		vui_column_layout_start(0, VuiVec2_init(vui_auto_len, vui_auto_len), &VuiRowColumnLayoutStyle_zero);

		static VuiBool text_state[sizeof(control_texts) / sizeof(*control_texts)] = {0};

		for (int i = 0; i < sizeof(control_texts) / sizeof(*control_texts); i += 1) {
			if (vui_text_check_box(start_id + i, &text_state[i], control_texts[i], &text_check_box_style)) {
				printf("Check_Boxes.%s is selected\n", control_texts[i]);
			}
		}
		start_id += sizeof(control_texts) / sizeof(*control_texts);

		vui_column_layout_end();

		static VuiBool image_state[sizeof(image_uv_rects) / sizeof(*image_uv_rects)] = {0};
		vui_column_layout_start(0, VuiVec2_init(vui_auto_len, vui_auto_len), &VuiRowColumnLayoutStyle_zero);

		for (int i = 0; i < sizeof(image_uv_rects) / sizeof(*image_uv_rects); i += 1) {
			if (vui_image_check_box(start_id + i, &image_state[i], VuiVec2_init(100, 50), 1, image_uv_rects[i], VuiColor_white, &text_check_box_style.check_box)) {
				printf("Check_Boxes.image is selected\n");
			}
		}
		start_id += sizeof(image_uv_rects) / sizeof(*image_uv_rects);

		vui_column_layout_end();

		vui_separator(&seperator_style);
	}

	{
		vui_text("Radio_Buttons", 0, &section_text_style);

		vui_column_layout_start(0, VuiVec2_init(vui_auto_len, vui_auto_len), &VuiRowColumnLayoutStyle_zero);

		static VuiCtrlId selected_button_id = 0;
		for (int i = 0; i < sizeof(control_texts) / sizeof(*control_texts); i += 1) {
			if (vui_text_radio_button(start_id + i, &selected_button_id, control_texts[i], &text_radio_button_style)) {
				printf("Radio_Buttons.%s is selected\n", control_texts[i]);
			}
		}
		start_id += sizeof(control_texts) / sizeof(*control_texts);

		vui_column_layout_end();

		static VuiBool image_state[sizeof(image_uv_rects) / sizeof(*image_uv_rects)] = {0};
		vui_column_layout_start(0, VuiVec2_init(vui_auto_len, vui_auto_len), &VuiRowColumnLayoutStyle_zero);

		for (int i = 0; i < sizeof(image_uv_rects) / sizeof(*image_uv_rects); i += 1) {
			if (vui_image_radio_button(start_id + i, &selected_button_id, VuiVec2_init(100, 50), 1, image_uv_rects[i], VuiColor_white, &text_radio_button_style.radio_button)) {
				printf("Radio_Buttons.image is selected\n");
			}
		}
		start_id += sizeof(image_uv_rects) / sizeof(*image_uv_rects);
		vui_column_layout_end();

		vui_separator(&seperator_style);
	}

	{
		vui_text("Progress_Bars", 0, &section_text_style);

		vui_column_layout_start(0, VuiVec2_init(vui_auto_len, vui_auto_len), &VuiRowColumnLayoutStyle_zero);
		static VuiBool update_value = vui_false;
		static float value = 0.f;
		static VuiBool inc_value = vui_true;
		vui_progress_bar(VuiVec2_init(200, 40), value, 0, 100, &progress_bar_style);

		if (vui_text_toggle_button(1239, &update_value, "update", &red_text_button_style)) {
			if (inc_value) value += 0.5;
			else value -= 0.5;
			if (value > 100 || value < 0) inc_value = !inc_value;
		}
		vui_column_layout_end();

		vui_separator(&seperator_style);
	}

	vui_row_layout_end();
	vui_scroll_view_end();

	vui_window_end();

	vui_frame_end();
}

#define opengl_assert_no_error() _opengl_assert_no_error(__FILE__, __LINE__)
void _opengl_assert_no_error(char* file, uint32_t line) {
	char* message = NULL;
	switch (glGetError()) {
		case GL_NO_ERROR: break;
		case GL_INVALID_ENUM: message = "invalid enum"; break;
		case GL_INVALID_VALUE: message = "invalid value"; break;
		case GL_INVALID_OPERATION: message = "invalid operation"; break;
		case GL_INVALID_FRAMEBUFFER_OPERATION: message = "invalid frame buffer opertation"; break;
		case GL_OUT_OF_MEMORY: message = "out of memory"; break;
		default:
			vui_assert(0, "opengl error: unhandled error %s:%u\n", file, line);
	   		break;
	}

	vui_assert(message == NULL, "opengl error: %s at %s:%u\n", message, file, line);
}

static void mat4x4_ortho(float* out, float left, float right, float bottom, float top, float znear, float zfar) {
    #define T(a, b) (a * 4 + b)

    out[T(0,0)] = 2.0f / (right - left);
    out[T(0,1)] = 0.0f;
    out[T(0,2)] = 0.0f;
    out[T(0,3)] = 0.0f;

    out[T(1,1)] = 2.0f / (top - bottom);
    out[T(1,0)] = 0.0f;
    out[T(1,2)] = 0.0f;
    out[T(1,3)] = 0.0f;

    out[T(2,2)] = -2.0f / (zfar - znear);
    out[T(2,0)] = 0.0f;
    out[T(2,1)] = 0.0f;
    out[T(2,3)] = 0.0f;

    out[T(3,0)] = -(right + left) / (right - left);
    out[T(3,1)] = -(top + bottom) / (top - bottom);
    out[T(3,2)] = -(zfar + znear) / (zfar - znear);
    out[T(3,3)] = 1.0f;

    #undef T
}

typedef struct OpenGLRenderState OpenGLRenderState;
struct OpenGLRenderState {
	GLuint tex_glyph;
	GLuint tex_image;
	GLuint program;
	GLuint vao;
	GLuint vbo;
	GLuint ibo;
	float projection[16];
};

OpenGLRenderState render_state = {0};

void render_ui() {
	static uint64_t prev_hash = 0;
	VuiRenderWindow* w = vui_window_build_render(0, 1.0, vui_false);
	/*
	vui_window_dump_render(0, stdout);
	exit(0);
	*/
	if (w->hash == prev_hash) return;

	VuiRenderLayer* l = &w->layers.DasStk_data[0];

	glBindBuffer(GL_ARRAY_BUFFER, render_state.vbo);
	glBufferData(GL_ARRAY_BUFFER, l->verts.count * sizeof(VuiRenderVert), l->verts.DasStk_data, GL_DYNAMIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, render_state.ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, l->indices.count * sizeof(VuiRenderIdx), l->indices.DasStk_data, GL_DYNAMIC_DRAW);

	glUniformMatrix4fv(glGetUniformLocation(render_state.program, "u_mvp"), 1, GL_FALSE, render_state.projection);

	GLenum index_type = 0;
	switch (sizeof(VuiRenderIdx)) {
		case sizeof(uint8_t): index_type = GL_UNSIGNED_BYTE; break;
		case sizeof(uint16_t): index_type = GL_UNSIGNED_SHORT; break;
		case sizeof(uint32_t): index_type = GL_UNSIGNED_INT; break;
	}

	VuiRenderCmd* cmds = l->cmds.DasStk_data;
	for (int i = 0; i < l->cmds.count; i += 1) {
		VuiRenderCmd* c = &cmds[i];
		GLuint tex = 0;
		switch (c->texture_id) {
			case 0: tex = render_state.tex_glyph; break;
			case 1: tex = render_state.tex_image; break;
		}

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, tex);
		glDrawElements(GL_TRIANGLES, c->indices_count, index_type, (void*)((uintptr_t)c->indices_start_idx * sizeof(VuiRenderIdx)));
	}
}

uint8_t* read_font_from_disk(char* path) {
	FILE* f = fopen(path, "rb");
	vui_assert(f, "failed to open font at %s", path);
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);

	uint8_t* font_data = das_alloc_array(uint8_t, size);

	fread(font_data, size, 1, f);
	fclose(f);
	return font_data;
}

uint32_t utf8codepoint(char* str, int32_t* out_codepoint) {
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

VuiVec2 position_text(void* userdata, VuiUserFontId user_font_id, char* text, uint32_t text_length, VuiVec2 top_left, VuiRenderGlyphFn render_glyph_fn) {
	if (text_length == 0) { return VuiVec2_zero; }
	ScaledFont* font = {0};
	switch (user_font_id) {
		case 0: font = &liberation_sans_32px; break;
		case 1: font = &liberation_mono_32px; break;
		default: printf("unreconginsed user_font_id %u\n", user_font_id); exit(1);
	}

	int ascent, descent, line_gap;
	stbtt_GetFontVMetrics(font->info, &ascent, &descent, &line_gap);

	ascent = roundf(ascent * font->scale);
	descent = roundf(descent * font->scale);

	int32_t codept = 0;
	uint32_t i = utf8codepoint(text, &codept);
	int codept_glyph = stbtt_FindGlyphIndex(font->info, codept);

	VuiVec2 pos = top_left;
	pos.y += ascent;

	VuiVec2 max_pos = {0};

	while (1) {
		if (render_glyph_fn) {
			uint32_t codept_idx = ScaledFont_get_codept_idx(font, codept);
			stbtt_aligned_quad quad = {0};
			stbtt_GetPackedQuad(font->codepts_packed_chars, glyph_texture.width, glyph_texture.height,
				codept_idx, &pos.x, &pos.y, &quad, 1);
			render_glyph_fn(
				0,
				VuiRect_init(quad.x0, quad.y0, quad.x1, quad.y1),
				VuiRect_init(quad.s0, quad.t0, quad.s1, quad.t1)
			);
		} else {
			ScaledFont_add_codept(font, codept);

			/* how wide is this character */
			int advance_width;
			int left_side_bearing;
			stbtt_GetGlyphHMetrics(font->info, codept_glyph, &advance_width, &left_side_bearing);

			/* get bounding box for character (may be offset to account for chars that dip above or below the line */
			int c_x1, c_y1, c_x2, c_y2;
			stbtt_GetGlyphBitmapBox(font->info, codept_glyph, font->scale, font->scale, &c_x1, &c_y1, &c_x2, &c_y2);
			pos.x += roundf(advance_width * font->scale);
		}

		if (i >= text_length) break;

		int32_t next_codept = 0;
		i += utf8codepoint(&text[i], &next_codept);
		int next_codept_glyph = stbtt_FindGlyphIndex(font->info, next_codept);

		/* add kerning */
		int kern;
		kern = stbtt_GetGlyphKernAdvance(font->info, codept, next_codept);
		pos.x += roundf(kern * font->scale);

		codept = next_codept;
		codept_glyph = next_codept_glyph;
	}

	max_pos.x = vui_max(max_pos.x, pos.x);
	max_pos.y = pos.y - descent;
	return max_pos;
}

void vui_text_box_focus_change(VuiBool focused) {
	if (focused) SDL_StartTextInput();
	else SDL_StopTextInput();
}

GLuint opengl_gen_texture() {
	GLuint tex;
	glGenTextures(1, &tex);
	return tex;
}

void opengl_texture_set_pixels(GLuint tex, GLenum internal_format, GLenum format, uint32_t width, uint32_t height, void* pixels) {
	glBindTexture(GL_TEXTURE_2D, tex);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

	glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format, GL_UNSIGNED_BYTE, pixels);

	glBindTexture(GL_TEXTURE_2D, 0);
}

void opengl_compile_and_use_shader() {
	GLuint vs, fs, program;

	vs = glCreateShader(GL_VERTEX_SHADER);
	fs = glCreateShader(GL_FRAGMENT_SHADER);

	glShaderSource(vs, 1, &vertex_shader_src, NULL);
	glCompileShader(vs);

	GLint status;
	glGetShaderiv(vs, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE)
	{
		fprintf(stderr, "vertex shader compilation failed\n");
		exit(1);
	}

	glShaderSource(fs, 1, &fragment_shader_src, NULL);
	glCompileShader(fs);

	glGetShaderiv(fs, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE)
	{
		fprintf(stderr, "fragment shader compilation failed\n");
		exit(1);
	}

	program = glCreateProgram();
	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glLinkProgram(program);

	glUseProgram(program);

	render_state.program = program;
}

void opengl_gen_vao_vbos() {
	glGenVertexArrays(1, &render_state.vao);
	glBindVertexArray(render_state.vao);

	glGenBuffers(1, &render_state.vbo);
	glBindBuffer(GL_ARRAY_BUFFER, render_state.vbo);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(VuiRenderVert), (const void*)offsetof(VuiRenderVert, pos));
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(VuiRenderVert), (const void*)offsetof(VuiRenderVert, uv));
	glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VuiRenderVert), (const void*)offsetof(VuiRenderVert, color));

	glGenBuffers(1, &render_state.ibo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, render_state.ibo);
}

typedef struct InputState InputState;
struct InputState {
	VuiMouseButtons mouse_buttons_pressed;
	VuiMouseButtons mouse_buttons_released;
	VuiInputActions actions;
	float mouse_wheel_x;
	float mouse_wheel_y;
	uint8_t enter_is_pressed: 1;
	uint8_t space_is_pressed: 1;
};

void sdl2_handle_event(SDL_Event* e, InputState* s) {
	switch (e->type) {
		case SDL_MOUSEBUTTONDOWN:
			switch (e->button.button) {
				case SDL_BUTTON_LEFT: s->mouse_buttons_pressed |= VuiMouseButtons_left; break;
				case SDL_BUTTON_MIDDLE: s->mouse_buttons_pressed |= VuiMouseButtons_middle; break;
				case SDL_BUTTON_RIGHT: s->mouse_buttons_pressed |= VuiMouseButtons_right; break;
			}
			break;
		case SDL_MOUSEBUTTONUP:
			switch (e->button.button) {
				case SDL_BUTTON_LEFT: s->mouse_buttons_released |= VuiMouseButtons_left; break;
				case SDL_BUTTON_MIDDLE: s->mouse_buttons_released |= VuiMouseButtons_middle; break;
				case SDL_BUTTON_RIGHT: s->mouse_buttons_released |= VuiMouseButtons_right; break;
			}
			break;
		case SDL_MOUSEWHEEL:
			s->mouse_wheel_x += e->wheel.x * 20.0;
			s->mouse_wheel_y += e->wheel.y * 20.0;
			break;
		case SDL_KEYUP:
			switch (e->key.keysym.sym) {
				case SDLK_RETURN: s->actions |= VuiInputActions_focus_released; s->space_is_pressed = 0; break;
				case SDLK_SPACE: s->actions |= VuiInputActions_focus_released; s->enter_is_pressed = 0; break;
			}
			break;
		case SDL_KEYDOWN:
			switch (e->key.keysym.sym) {
				case SDLK_LEFT: s->actions |= VuiInputActions_left; break;
				case SDLK_RIGHT: s->actions |= VuiInputActions_right; break;
				case SDLK_UP: s->actions |= VuiInputActions_up; break;
				case SDLK_DOWN: s->actions |= VuiInputActions_down; break;
				case SDLK_BACKSPACE: s->actions |= VuiInputActions_backspace; break;
				case SDLK_DELETE: s->actions |= VuiInputActions_delete; break;
				case SDLK_HOME: s->actions |= VuiInputActions_home; break;
				case SDLK_END: s->actions |= VuiInputActions_end; break;
				case SDLK_TAB: s->actions |= VuiInputActions_focus_next; break;
				case SDLK_RETURN: s->actions |= VuiInputActions_focus_pressed; s->space_is_pressed = 1; break;
				case SDLK_SPACE: s->actions |= VuiInputActions_focus_pressed; s->enter_is_pressed = 1; break;
			}
			break;
		case SDL_WINDOWEVENT:
			switch (e->window.event) {
				case SDL_WINDOWEVENT_RESIZED:
					mat4x4_ortho(render_state.projection, 0, e->window.data1, e->window.data2, 0, -1, 1);
					glViewport(0, 0, e->window.data1, e->window.data2);
					break;
				case SDL_WINDOWEVENT_CLOSE:
					exit(0);
					break;
				default:
					break;
			}
			break;
		case SDL_TEXTINPUT:
			vui_input_add_text(e->text.text, strlen(e->text.text));
			break;
	}
}

void update_input(InputState* s) {
	int mouse_x, mouse_y;
	SDL_GetMouseState(&mouse_x, &mouse_y);

	vui_input_set_mouse_pos(mouse_x, mouse_y);
	vui_input_set_mouse_wheel_offset(s->mouse_wheel_x, s->mouse_wheel_y);

	//
	// TODO handle in the event system when we have multiple windows
	vui_window_set_mouse_focused(0);
	vui_window_set_focused(0);

	vui_input_set_mouse_button_pressed(s->mouse_buttons_pressed);
	vui_input_set_mouse_button_released(s->mouse_buttons_released);

	SDL_Keymod key_mod = SDL_GetModState();

	if (key_mod & KMOD_SHIFT) s->actions |= VuiInputActions_shift_pressed;
	if (key_mod & KMOD_CTRL) s->actions |= VuiInputActions_ctrl_pressed;
	if (s->enter_is_pressed || s->space_is_pressed) s->actions |= VuiInputActions_focus_held;
	vui_input_add_actions(s->actions);

	//
	// reset the input state for the next frame
	s->mouse_buttons_pressed = 0;
	s->mouse_buttons_released = 0;
	s->mouse_wheel_x = 0;
	s->mouse_wheel_y = 0;
	s->actions = 0;
}

int main(int argc, char** argv) {
	SDL_Init(SDL_INIT_VIDEO);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	SDL_Window* window = SDL_CreateWindow("Vui Example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, screen_width, screen_height, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
	SDL_GLContext context = SDL_GL_CreateContext(window);

	opengl_compile_and_use_shader();
	opengl_gen_vao_vbos();

	render_state.tex_glyph = opengl_gen_texture();
	render_state.tex_image = opengl_gen_texture();

	VuiSetup setup = {
		.position_text_fn = position_text,
		.position_text_userdata = NULL,
		.text_box_focus_change_fn = vui_text_box_focus_change,
		.windows_count = 1,
	};
	vui_assert(vui_init(&setup), "failed to initialize vui");

	uint8_t* liberation_sans_font_data = read_font_from_disk("fonts/LiberationSans-Regular.ttf");
	vui_assert(stbtt_InitFont(&liberation_sans, liberation_sans_font_data, 0), "failed to initialize liberation_sans with stbtt");

	uint8_t* liberation_mono_font_data = read_font_from_disk("fonts/LiberationMono-Regular.ttf");
	vui_assert(stbtt_InitFont(&liberation_mono, liberation_mono_font_data, 0), "failed to initialize liberation_mono with stbtt");

	ScaledFont_init(&liberation_sans_32px, &liberation_sans, 32);
	ScaledFont_init(&liberation_mono_32px, &liberation_mono, 32);

	glyph_texture.pixels = malloc(1024 * 1024);
	glyph_texture.width = 1024;
	glyph_texture.height = 1024;

	{
		// r, g, b
		char* color_plane_pixels = malloc(256 * 256 * 3);

		uint32_t idx = 0;
		for (int r = 0; r <= 255; r += 1) {
			for (int b = 0; b <= 255; b += 1) {
				color_plane_pixels[idx] = r;
				color_plane_pixels[idx + 1] = 0x0;
				color_plane_pixels[idx + 2] = b;
				idx += 3;
			}
		}

		opengl_texture_set_pixels(render_state.tex_image, GL_RGBA, GL_RGB, 256, 256, color_plane_pixels);
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	InputState input_state = {0};
	while (1) {
		glClearColor(0.1,0.1,0.1,0.0);
		glClear(GL_COLOR_BUFFER_BIT);

		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			sdl2_handle_event(&event, &input_state);
		}

		update_input(&input_state);

		build_ui();
		update_glyph_texture();
		opengl_texture_set_pixels(render_state.tex_glyph, GL_RGBA, GL_RED, glyph_texture.width, glyph_texture.height, glyph_texture.pixels);

		render_ui();
		SDL_GL_SwapWindow(window);
	}

	return 0;
}

