
void vui_opengl_setup_state();

//
// call this right after you have initialize an OpenGL context to initialize this shim
void vui_opengl_init();

//
// renders the window by using the VuiWindowRender structure that is received from the vui_window_render function.
// WARNING: be sure to bind the correct shader and perform all the calls to glVertexAttribPointer that setup the shader attributes.
void vui_opengl_render(VuiWindowRender* w);

