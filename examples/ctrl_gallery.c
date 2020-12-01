
#include <errno.h>

#define GL_GLEXT_PROTOTYPES

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#define STB_RECT_PACK_IMPLEMENTATION
#include "deps/stb_rect_pack.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "deps/stb_truetype.h"

#include "../vui.h"
#include "../vui.c"

#include "../backends/vui_sdl2.h"
#include "../backends/vui_sdl2.c"

#include "../backends/vui_opengl.h"
#include "../backends/vui_opengl.c"

#include "../backends/vui_stbtruetype.h"
#include "../backends/vui_stbtruetype.c"

#include "../backends/vui_stbtruetype_manager.h"
#include "../backends/vui_stbtruetype_manager.c"

#define screen_width 1280
#define screen_height 720

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

typedef enum {
	AppWindowId_main,
	AppWindowId_COUNT,
} AppWindowId;

typedef struct App App;
struct App {
	SDL_Window* window;
	VuiGlyphTextureId ascii_glyph_texture_id;
	VuiGlyphTextureId etc_glyph_texture_id;
	VuiFontId default_font_id;
	VuiImageId images[5];
	VuiStk(char) font_file_bytes;
	struct {
		GLuint tex_ascii_glyph_texture;
		GLuint tex_etc_glyph_texture;
		GLuint tex_image;
		GLuint program;
		float projection[16];
	} opengl;
};

App app;

void build_ui() {
	vui_frame_start(vui_false);

	vui_window_start(0, VuiVec2_init(screen_width, screen_height));
	vui_row_layout();

	static VuiVec2 size = {0};
	static VuiVec2 offset = {50.f, 50.f};
	vui_scope_width(vui_fill_len)
	vui_scope_height(vui_fill_len)
	vui_scroll_view_start(vui_sib_id,
		VuiScrollFlags_vertical |
		VuiScrollFlags_resizable |
		VuiScrollFlags_horizontal,
		&vui_ss.scroll_view);

	vui_row_layout();
	{

		vui_text(vui_sib_id, "Buttons", 0.f, &vui_ss.text_header);
		vui_separator(vui_sib_id, &vui_ss.separator);
		vui_scope_ctrl(vui_sib_id, NULL) {
			vui_column_layout();

			VuiFocusState state = vui_text_button(vui_sib_id, "Button 1", vui_ss.button_action);
			vui_image_button(vui_sib_id, app.images[3], VuiColor_white, vui_ss.button_action);
			vui_image_text_button(vui_sib_id, app.images[3], VuiColor_white, "Button 3", vui_ss.button_action);
		}

		vui_text(vui_sib_id, "Toggle Buttons", 0.f, &vui_ss.text_header);
		vui_separator(vui_sib_id, &vui_ss.separator);
		vui_scope_ctrl(vui_sib_id, NULL) {
			vui_column_layout();

			vui_text_toggle_button(vui_sib_id, NULL, "Button 1", vui_ss.button_action);
			vui_image_toggle_button(vui_sib_id, NULL, app.images[3], VuiColor_white, vui_ss.button_action);
			vui_image_text_toggle_button(vui_sib_id, NULL, app.images[3], VuiColor_white, "Button 3", vui_ss.button_action);
		}

		vui_text(vui_sib_id, "Select Buttons", 0.f, &vui_ss.text_header);
		vui_separator(vui_sib_id, &vui_ss.separator);
		vui_scope_ctrl(vui_sib_id, NULL) {
			vui_column_layout();

			static VuiCtrlSibId selected_sib_id = 0;
			vui_text_select_button(vui_sib_id, &selected_sib_id, "Button 1", vui_ss.button_action);
			vui_image_select_button(vui_sib_id, &selected_sib_id, app.images[3], VuiColor_white, vui_ss.button_action);
			vui_image_text_select_button(vui_sib_id, &selected_sib_id, app.images[3], VuiColor_white, "Button 3", vui_ss.button_action);
		}

		vui_text(vui_sib_id, "Check Boxes", 0.f, &vui_ss.text_header);
		vui_separator(vui_sib_id, &vui_ss.separator);
		vui_scope_ctrl(vui_sib_id, NULL) {
			vui_column_layout();

			vui_text_check_box(vui_sib_id, NULL, "Button 1", vui_ss.check_box);
			vui_image_check_box(vui_sib_id, NULL, app.images[3], VuiColor_white, vui_ss.check_box);
		}

		vui_text(vui_sib_id, "Radio Buttons", 0.f, &vui_ss.text_header);
		vui_separator(vui_sib_id, &vui_ss.separator);
		vui_scope_ctrl(vui_sib_id, NULL) {
			vui_column_layout();

			static VuiCtrlSibId selected_sib_id = 0;
			vui_text_radio_button(vui_sib_id, &selected_sib_id, "Button 1", vui_ss.radio_button);
			vui_image_radio_button(vui_sib_id, &selected_sib_id, app.images[3], VuiColor_white, vui_ss.radio_button);
		}

		vui_text(vui_sib_id, "Progress Bar", 0.f, &vui_ss.text_header);
		vui_separator(vui_sib_id, &vui_ss.separator);
		{
			static float value = 0.f;
			static VuiBool go_backward = vui_false;
			if (go_backward) {
				value -= 2.5;
			} else {
				value += 2.5;
			}
			if (value >= 500.f)  {
				go_backward = vui_true;
			} else if (value <= 0.f)  {
				go_backward = vui_false;
			}


			vui_scope_width(200.f)
			vui_scope_height(40.f)
			vui_progress_bar(vui_sib_id, value, 0.f, 500.f, &vui_ss.progress_bar);
		}

		vui_text(vui_sib_id, "Text & Input Boxes", 0.f, &vui_ss.text_header);
		vui_separator(vui_sib_id, &vui_ss.separator);
		vui_scope_ctrl(vui_sib_id, NULL) {
			vui_column_layout();

			vui_scope_align(VuiAlign_left_center) {
				vui_text(vui_sib_id, "Text:  ", 0.f, &vui_ss.text_menu);
				static char buf[20] = {0};
				vui_scope_width(200.f)
				vui_text_box(vui_sib_id, buf, sizeof(buf), vui_ss.text_box);
			}
		}
		vui_scope_ctrl(vui_sib_id, NULL) {
			vui_column_layout();

			vui_scope_align(VuiAlign_left_center) {
				vui_text(vui_sib_id, "Uint:  ", 0.f, &vui_ss.text_menu);
				static uint32_t value = 0;
				vui_scope_width(200.f) {
					vui_input_box_uint(vui_sib_id, &value, vui_ss.text_box);
					vui_slider_uint(vui_sib_id, &value, 0, 5, &vui_ss.slider);
				}
			}
		}
		vui_scope_ctrl(vui_sib_id, NULL) {
			vui_column_layout();

			vui_scope_align(VuiAlign_left_center) {
				vui_text(vui_sib_id, "Sint:  ", 0.f, &vui_ss.text_menu);
				static int32_t value = 0;
				vui_scope_width(200.f) {
					vui_input_box_sint(vui_sib_id, &value, vui_ss.text_box);
					vui_slider_sint(vui_sib_id, &value, -10, 10, &vui_ss.slider);
				}
			}
		}
		vui_scope_ctrl(vui_sib_id, NULL) {
			vui_column_layout();

			vui_scope_align(VuiAlign_left_center) {
				vui_text(vui_sib_id, "Float: ", 0.f, &vui_ss.text_menu);
				static float value = 0;
				vui_scope_width(200.f) {
					vui_input_box_float(vui_sib_id, &value, vui_ss.text_box);
					vui_slider_float(vui_sib_id, &value, -500.f, 500.f, &vui_ss.slider);
				}
			}
		}

		vui_text(vui_sib_id, "Text Wrapping", 0.f, &vui_ss.text_header);
		vui_separator(vui_sib_id, &vui_ss.separator);

		static float word_wrap_at_width = 0.f;
		vui_scope_width(200.f) {
			vui_input_box_float(vui_sib_id, &word_wrap_at_width, vui_ss.text_box);
			vui_slider_float(vui_sib_id, &word_wrap_at_width, 0.f, 1000.f, &vui_ss.slider);
		}

		vui_text(vui_sib_id, "This is an example of multi-line word-wrapping that has been...\nDesigned to be used as a test!!!\nDrag the slider to change the word_wrap_at_width value...\n\n...\n", word_wrap_at_width, &vui_ss.text_header);

	}
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

int file_read_all(char* path, VuiStk(char)* bytes_out) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) { return errno; }

	// seek the cursor to the end
    if (fseek(file, 0, SEEK_END) == -1) { goto ERR; }

	// get the location of the position we have seeked the cursor to.
	// this will tell us the file size
    long int file_size = ftell(file);
    if (file_size == -1) { goto ERR; }

	// seek the cursor back to the beginning
    if (fseek(file, 0, SEEK_SET) == -1) { goto ERR; }

	//
	// ensure the buffer has enough capacity
    if (VuiStk_cap(*bytes_out) < file_size) {
        if (!VuiStk_resize_cap(bytes_out, file_size)) {
			return ENOMEM;
		}
    }

	// read the file in
    size_t read_size = fread(*bytes_out, 1, file_size, file);
    if (read_size != file_size) { goto ERR; }

    if (fclose(file) != 0) { return errno; }

    _VuiStk_header(*bytes_out)->count = read_size;
    return 0;

ERR: {}
	int err = errno;
    fclose(file);
    return err;
}

GLuint opengl_texture_gen() {
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

	app.opengl.program = program;
}

void glyph_texture_pack_and_update(VuiGlyphTextureId glyph_texture_id, GLuint texture_id) {
	vui_stbtt_glyph_texture_pack(glyph_texture_id);

	uint32_t wh = 0;
	uint8_t* pixels = vui_stbtt_glyph_texture_get_pixels_and_wh(glyph_texture_id, &wh);
	opengl_texture_set_pixels(texture_id, GL_R8, GL_RED, wh, wh, pixels);
}

//
// implement the function that is required by the vui_stbtruetype_manager shim.
// here we decide which styled glyphs go in which glyph texture.
// in our case all ASCII glyphs using the default font sizes of VUI (header and menu) to in one.
// and the rest go in another.
VuiGlyphTextureId vui_stbtt_get_styled_glyph_texture_id(VuiFontId font_id, float line_height, int32_t codept) {
	if (codept >= 33 && codept <= 126) {
		return app.ascii_glyph_texture_id;
	} else {
		return app.etc_glyph_texture_id;
	}
}

void App_init() {
	//
	// initialize SDL with OpenGL 3.3 Core Profile
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

	//
	// create a Window with an OpenGL context
	app.window = SDL_CreateWindow("Vui Example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, screen_width, screen_height, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
	SDL_GLContext context = SDL_GL_CreateContext(app.window);
	SDL_GL_SetSwapInterval(1);

	//
	// initialize the VUI OpenGL shim
	vui_opengl_init();
	opengl_compile_and_use_shader();

	//
	// create the opengl textures, these will be uninitialized until opengl_texture_set_pixels is called.
	//
	app.opengl.tex_ascii_glyph_texture = opengl_texture_gen();
	app.opengl.tex_etc_glyph_texture = opengl_texture_gen();
	app.opengl.tex_image = opengl_texture_gen();

	//
	// load in the font from a file and create the font and glyph textures using the font manager.
	// we have a glyph texture that only contains ASCII characters and is precomputed on app initialization.
	// the other glyph texture is used for all the other misc characters and line heights.
	//
	int e = file_read_all("fonts/LiberationMono-Regular.ttf", &app.font_file_bytes);
	vui_assert(e == 0, "error reading file: %s", strerror(e));
	app.default_font_id = vui_stbtt_font_add((const uint8_t*)app.font_file_bytes);
	app.ascii_glyph_texture_id = vui_stbtt_glyph_texture_add(app.opengl.tex_ascii_glyph_texture, 0, 0, 0, 0);
	app.etc_glyph_texture_id = vui_stbtt_glyph_texture_add(app.opengl.tex_etc_glyph_texture, 0, 0, 0, 0);

	//
	// initialize VUI.
	// we pass in the position text function from the vui stb truetype shim.
	//
	VuiSetup setup = {
		.position_text_fn = vui_stbtt_position_text,
		.position_text_userdata = NULL,
		.windows_count = 1,
		.allocator = NULL,
		.default_font_id = app.default_font_id,
	};
	vui_assert(vui_init(&setup), "failed to initialize vui");

	//
	// precompute the glyph texture for all the visible ascii characters for the two font sizes VUI uses by default.
	//
	{
		stbtt_fontinfo* info = vui_stbtt_font_get(app.default_font_id);

		for (int32_t codept = 33; codept < 127; codept += 1) {
			//
			// add the glyph for the header size
			vui_stbtt_glyph_texture_add_styled_glyph(
				app.ascii_glyph_texture_id, app.default_font_id, vui_line_height_header, stbtt_FindGlyphIndex(info, codept));

			//
			// add the glyph for the menu size
			vui_stbtt_glyph_texture_add_styled_glyph(
				app.ascii_glyph_texture_id, app.default_font_id, vui_line_height_menu, stbtt_FindGlyphIndex(info, codept));
		}

		glyph_texture_pack_and_update(app.ascii_glyph_texture_id, app.opengl.tex_ascii_glyph_texture);
	}

	//
	// setup the images that are use in VUI.
	// this can be done at anytime if images are dynamically loaded.
	//
	{
		VuiImage image;

		uint32_t wh = 0;
		vui_stbtt_glyph_texture_get_pixels_and_wh(app.ascii_glyph_texture_id, &wh);

		//
		// ascii glyph texture
		image.width = wh,
		image.height = wh,
		image.texture_id = app.opengl.tex_ascii_glyph_texture,
		image.uv_rect = VuiRect_init(0.f, 0.f, 1.f, 1.f),
		app.images[0] = vui_image_add(&image);

		//
		// etc glyph texture
		// initialize the size to zero as we will update this every frame.
		image.width = 0,
		image.height = 0,
		image.texture_id = app.opengl.tex_etc_glyph_texture,
		image.uv_rect = VuiRect_init(0.f, 0.f, 1.f, 1.f),
		app.images[1] = vui_image_add(&image);

		//
		// color texture
		image.width = 256,
		image.height = 256,
		image.texture_id = app.opengl.tex_image,
		image.uv_rect = VuiRect_init(0.f, 0.f, 1.f, 1.f),
		app.images[2] = vui_image_add(&image);

		//
		// color texture subsection small icon
		image.width = 24,
		image.height = 24,
		image.texture_id = app.opengl.tex_image,
		image.uv_rect = VuiRect_init(0.5f, 0.5f, 0.7f, 0.7f),
		app.images[3] = vui_image_add(&image);

		//
		// color texture subsection rectangle
		image.width = 256,
		image.height = 128,
		image.texture_id = app.opengl.tex_image,
		image.uv_rect = VuiRect_init(0.0f, 0.0f, 1.f, 0.5f),
		app.images[4] = vui_image_add(&image);
	}

	//
	// build the color texture we use as an image.
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

		opengl_texture_set_pixels(app.opengl.tex_image, GL_RGBA, GL_RGB, 256, 256, color_plane_pixels);
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void App_update() {
	//
	// remove all the glyphs from the glyph texture that holds all the different sized or non ASCII glyphs.
	vui_stbtt_glyph_texture_clear_styled_glyphs(app.etc_glyph_texture_id);

	//
	// transfer all the information that VUI needs from SDL.
	vui_sdl2_frame_start();

	build_ui();

	//
	// pack the etc glyph texture and send the pixels to the GPU.
	glyph_texture_pack_and_update(app.etc_glyph_texture_id, app.opengl.tex_etc_glyph_texture);
}

void vui_opengl_setup_state() {
	glUseProgram(app.opengl.program);
	glUniformMatrix4fv(glGetUniformLocation(app.opengl.program, "u_mvp"), 1, GL_FALSE, app.opengl.projection);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(VuiVertex), (const void*)offsetof(VuiVertex, pos));
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(VuiVertex), (const void*)offsetof(VuiVertex, uv));
	glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VuiVertex), (const void*)offsetof(VuiVertex, color));
}

void App_render() {
	VuiWindowRender* window = vui_window_render(AppWindowId_main, 1.f, vui_false);
	vui_opengl_render(window);
}

int main(int argc, char** argv) {
	App_init();

	while (1) {
		glClearColor(0.1, 0.1, 0.1, 1.0);
		glClear(GL_COLOR_BUFFER_BIT);

		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			vui_sdl2_process_event(&event);

			switch (event.type) {
				case SDL_WINDOWEVENT: {
					switch (event.window.event) {
						case SDL_WINDOWEVENT_RESIZED:
							mat4x4_ortho(app.opengl.projection, 0.f, event.window.data1, event.window.data2, 0.f, 1.f, 0.f);
							glViewport(0, 0, event.window.data1, event.window.data2);
							break;
					}
					break;
				};
				case SDL_QUIT:
					exit(0);
			}
		}

		App_update();
		App_render();
		SDL_GL_SwapWindow(app.window);
	}

	return 0;
}

