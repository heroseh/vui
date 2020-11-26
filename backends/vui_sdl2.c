#ifndef VUI_H
#error "expected the VUI header to be included before this file."
#endif

#ifndef SDL_h_
#error "expected the SDL2 header to be included before this file."
#endif

typedef struct _VuiSdl2InputState _VuiSdl2InputState;
struct _VuiSdl2InputState {
	VuiMouseButtons mouse_buttons_pressed;
	VuiMouseButtons mouse_buttons_released;
	VuiInputActions actions;
	float mouse_wheel_x;
	float mouse_wheel_y;
	uint8_t enter_is_pressed: 1;
	uint8_t space_is_pressed: 1;
};

_VuiSdl2InputState _vui_sdl2_input_state = {0};

void vui_sdl2_frame_start() {
	//
	// setup the input for VUI
	//
	{
		_VuiSdl2InputState* s = &_vui_sdl2_input_state;

		//
		// get the mouse coordiates from SDL
		int mouse_x, mouse_y;
		SDL_GetMouseState(&mouse_x, &mouse_y);

		//
		// give the mouse coordiates and wheel offset to VUI
		vui_input_set_mouse_pos(mouse_x, mouse_y);
		vui_input_set_mouse_wheel_offset(s->mouse_wheel_x, s->mouse_wheel_y);

		//
		// TODO handle in the event system when we have multiple windows
		// set the window focus
		vui_window_set_mouse_focused(0);
		vui_window_set_focused(0);

		//
		// give the mouse button state to VUI
		vui_input_set_mouse_button_pressed(s->mouse_buttons_pressed);
		vui_input_set_mouse_button_released(s->mouse_buttons_released);

		//
		// get the state of the key modifier (CTRL, SHIFT, ALT, etc)
		// and set the appropriate VuiInputActions flags.
		SDL_Keymod key_mod = SDL_GetModState();
		if (key_mod & KMOD_SHIFT) s->actions |= VuiInputActions_shift_pressed;
		if (key_mod & KMOD_CTRL) s->actions |= VuiInputActions_ctrl_pressed;
		if (s->enter_is_pressed || s->space_is_pressed) s->actions |= VuiInputActions_focus_held;

		//
		// give the VuiInputActions to VUI
		vui_input_add_actions(s->actions);

		//
		// reset the input state for the next frame
		s->mouse_buttons_pressed = 0;
		s->mouse_buttons_released = 0;
		s->mouse_wheel_x = 0;
		s->mouse_wheel_y = 0;
		s->actions = 0;
	}
}

void vui_sdl2_process_event(const SDL_Event* e) {
	_VuiSdl2InputState* s = &_vui_sdl2_input_state;

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
			if (SDL_GetModState() & KMOD_SHIFT) {
				//
				// shift is held down, so swap the axis for the wheel offsets
				s->mouse_wheel_x += e->wheel.y * 20.0;
				s->mouse_wheel_y += e->wheel.x * 20.0;
			} else {
				s->mouse_wheel_x += e->wheel.x * 20.0;
				s->mouse_wheel_y += e->wheel.y * 20.0;
			}
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
		case SDL_TEXTINPUT:
			vui_input_add_text(e->text.text, strlen(e->text.text));
			break;
	}
}
