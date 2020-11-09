
#define GL_GLEXT_PROTOTYPES

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#define STB_RECT_PACK_IMPLEMENTATION
#include "deps/stb_rect_pack.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "deps/stb_truetype.h"

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

typedef struct {
	stbtt_fontinfo* info;
	VuiStk(int32_t) codepts;
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
	VuiStk_clear(f->codepts);
}

void ScaledFont_add_codept(ScaledFont* f, int32_t codept) {
	int32_t* codepts = f->codepts;
	for (uint32_t i = 0; i < VuiStk_count(f->codepts); i += 1) {
		if (codepts[i] == codept) return;
	}

	int32_t* t = VuiStk_push(&f->codepts);
	*t = codept;
}

VuiBool ScaledFont_pack_codepts(ScaledFont* f, stbtt_pack_context* spc) {
	if (f->codepts_packed_chars_cap != VuiStk_cap(f->codepts)) {
		f->codepts_packed_chars = vui_mem_realloc_array(
			stbtt_packedchar,
			_vui.allocator,
			f->codepts_packed_chars,
			f->codepts_packed_chars_cap,
			VuiStk_cap(f->codepts)
		);
	}

	stbtt_pack_range r = {0};
	r.font_size = 32;
	r.chardata_for_range = f->codepts_packed_chars;
	r.num_chars = VuiStk_count(f->codepts);
	r.array_of_unicode_codepoints = f->codepts;

	return stbtt_PackFontRanges(spc, f->info->data, 0, &r, 1) == 1;
}

uint32_t ScaledFont_get_codept_idx(ScaledFont* f, int32_t codept) {
	int32_t* codepts = f->codepts;
	for (uint32_t i = 0; i < VuiStk_count(f->codepts); i += 1) {
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

void build_ui() {
	ScaledFont_clear_codepts(&liberation_sans_32px);
	ScaledFont_clear_codepts(&liberation_mono_32px);

	vui_frame_start(vui_false);

	vui_window_start(0, VuiVec2_init(screen_width, screen_height));

	vui_push_height(VuiCtrlState_default, 220.f);
	vui_scope_layout_wrap(VuiCtrlState_default, vui_true)
	vui_scope_layout_spacing(VuiCtrlState_default, 6.f)
	vui_scope_layout_wrap_spacing(VuiCtrlState_default, 20.f)
	vui_scope_padding(VuiCtrlState_default, VuiThickness_init_even(4))
	vui_scope_margin(VuiCtrlState_default, VuiThickness_init_even(4))
	vui_scope_box(vui_sib_id) {
	vui_pop_height(VuiCtrlState_default);
		vui_row_layout();

			vui_push_width(VuiCtrlState_default, 120.f);
			vui_push_height(VuiCtrlState_default, 120.f);
			vui_scope_box(vui_sib_id) {
			vui_pop_width(VuiCtrlState_default);
			vui_pop_height(VuiCtrlState_default);
				vui_stack_layout();

				vui_scope_align(VuiCtrlState_default, VuiAlign_left_top)
					vui_text(vui_sib_id, "lt", 0.f);

				vui_scope_align(VuiCtrlState_default, VuiAlign_center_top)
					vui_text(vui_sib_id, "ct", 0.f);

				vui_scope_align(VuiCtrlState_default, VuiAlign_right_top)
					vui_text(vui_sib_id, "rt", 0.f);

				vui_scope_align(VuiCtrlState_default, VuiAlign_left_center)
					vui_text(vui_sib_id, "lc", 0.f);

				vui_scope_align(VuiCtrlState_default, VuiAlign_center)
					vui_text(vui_sib_id, "cc", 0.f);

				vui_scope_align(VuiCtrlState_default, VuiAlign_right_center)
					vui_text(vui_sib_id, "rc", 0.f);

				vui_scope_align(VuiCtrlState_default, VuiAlign_left_bottom)
					vui_text(vui_sib_id, "lb", 0.f);

				vui_scope_align(VuiCtrlState_default, VuiAlign_center_bottom)
					vui_text(vui_sib_id, "cb", 0.f);

				vui_scope_align(VuiCtrlState_default, VuiAlign_right_bottom)
					vui_text(vui_sib_id, "rb", 0.f);
			}

			vui_scope_offset(VuiCtrlState_default, -10.f, 5.f)
			vui_scope_align(VuiCtrlState_default, VuiAlign_right_top)
				vui_text_button(vui_sib_id, "test");

			vui_scope_height(VuiCtrlState_default, vui_fill_len)
				vui_text_button(vui_sib_id, "tester");

			vui_scope_height_ratio(VuiCtrlState_default, 0.3)
				vui_text_button(vui_sib_id, "more");

			vui_scope_height(VuiCtrlState_default, vui_fill_len)
				vui_text_button(vui_sib_id, "less");

			vui_text_button(vui_sib_id, "something long");
	}

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

typedef enum {
	WindowId_main,
	WindowId_COUNT,
} WindowId;

void render_ui() {
	static uint64_t prev_hash = 0;
	VuiWindowRender* w = vui_window_render(WindowId_main, 1.0, vui_false);
	/*
	vui_window_dump_render(0, stdout);
	exit(0);
	*/
	if (w->hash == prev_hash) return;

	glBindBuffer(GL_ARRAY_BUFFER, render_state.vbo);
	glBufferData(GL_ARRAY_BUFFER, VuiStk_count(w->verts) * sizeof(VuiVertex), w->verts, GL_DYNAMIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, render_state.ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, VuiStk_count(w->indices) * sizeof(VuiVertexIdx), w->indices, GL_DYNAMIC_DRAW);

	glUniformMatrix4fv(glGetUniformLocation(render_state.program, "u_mvp"), 1, GL_FALSE, render_state.projection);

	GLenum index_type = 0;
	switch (sizeof(VuiVertexIdx)) {
		case sizeof(uint8_t): index_type = GL_UNSIGNED_BYTE; break;
		case sizeof(uint16_t): index_type = GL_UNSIGNED_SHORT; break;
		case sizeof(uint32_t): index_type = GL_UNSIGNED_INT; break;
	}

	VuiRenderCmd* cmds = w->cmds;
	for (int i = 0; i < VuiStk_count(w->cmds); i += 1) {
		VuiRenderCmd* c = &cmds[i];
		GLuint tex = 0;
		switch (c->texture_id) {
			case 0: tex = render_state.tex_glyph; break;
			case 1: tex = render_state.tex_image; break;
		}

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, tex);
		glDrawElements(GL_TRIANGLES, c->indices_count, index_type, (void*)((uintptr_t)c->indices_start_idx * sizeof(VuiVertexIdx)));
	}
}

uint8_t* read_font_from_disk(char* path) {
	FILE* f = fopen(path, "rb");
	vui_assert(f, "failed to open font at %s", path);
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);

	uint8_t* font_data = vui_mem_alloc_array(uint8_t, _vui.allocator, size);

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

VuiVec2 position_text(void* userdata, VuiFontId font_id, char* text, uint32_t text_length, VuiVec2 top_left, VuiRenderGlyphFn render_glyph_fn) {
	if (text_length == 0) { return VuiVec2_zero; }
	ScaledFont* font = {0};
	switch (font_id) {
		case 0: font = &liberation_sans_32px; break;
		case 1: font = &liberation_mono_32px; break;
		default: printf("unreconginsed font_id %u\n", font_id); exit(1);
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
			VuiRect rect = VuiRect_init(quad.x0, quad.y0, quad.x1, quad.y1);
			VuiRect uv_rect = VuiRect_init(quad.s0, quad.t0, quad.s1, quad.t1);
			render_glyph_fn(&rect, 0, &uv_rect);
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

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(VuiVertex), (const void*)offsetof(VuiVertex, pos));
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(VuiVertex), (const void*)offsetof(VuiVertex, uv));
	glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VuiVertex), (const void*)offsetof(VuiVertex, color));

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

	VuiStyle style;
	VuiStyle_init_default(&style);
	VuiSetup setup = {
		.position_text_fn = position_text,
		.position_text_userdata = NULL,
		.text_box_focus_change_fn = vui_text_box_focus_change,
		.windows_count = 1,
		.style = &style,
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

