#ifndef VUI_H
#error "expected the VUI header to be included before this file."
#endif

#ifndef __gl_h_
#error "expected the opengl header for your platform to be included before this file."
#endif

typedef struct _VuiOpenGLState _VuiOpenGLState;
struct _VuiOpenGLState {
	GLuint vao;
	GLuint vbo;
	GLuint ibo;
};

static _VuiOpenGLState _vui_opengl_state = {0};

void vui_opengl_init() {
	glGenVertexArrays(1, &_vui_opengl_state.vao);
	glBindVertexArray(_vui_opengl_state.vao);

	glGenBuffers(1, &_vui_opengl_state.vbo);
	glBindBuffer(GL_ARRAY_BUFFER, _vui_opengl_state.vbo);

	glGenBuffers(1, &_vui_opengl_state.ibo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _vui_opengl_state.ibo);

	glBindVertexArray(0);
}

void vui_opengl_render(VuiWindowRender* w) {
	glBindVertexArray(_vui_opengl_state.vao);

	glBindBuffer(GL_ARRAY_BUFFER, _vui_opengl_state.vbo);
	glBufferData(GL_ARRAY_BUFFER, VuiStk_count(w->verts) * sizeof(VuiVertex), w->verts, GL_DYNAMIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _vui_opengl_state.ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, VuiStk_count(w->indices) * sizeof(VuiVertexIdx), w->indices, GL_DYNAMIC_DRAW);

	vui_opengl_setup_state();

	GLenum index_type = 0;
	switch (sizeof(VuiVertexIdx)) {
		case sizeof(uint8_t): index_type = GL_UNSIGNED_BYTE; break;
		case sizeof(uint16_t): index_type = GL_UNSIGNED_SHORT; break;
		case sizeof(uint32_t): index_type = GL_UNSIGNED_INT; break;
	}

	VuiRenderCmd* cmds = w->cmds;
	for (int i = 0; i < VuiStk_count(w->cmds); i += 1) {
		VuiRenderCmd* c = &cmds[i];
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, c->texture_id);
		glDrawElements(GL_TRIANGLES, c->indices_count, index_type, (void*)((uintptr_t)c->indices_start_idx * sizeof(VuiVertexIdx)));
	}

	glBindVertexArray(0);
}

