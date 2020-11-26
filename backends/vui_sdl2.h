
typedef union SDL_Event SDL_Event;

//
// call this function for every polled event
void vui_sdl2_process_event(const SDL_Event* e);

//
// this function needs to be called before vui_frame_start but after processing all SDL events.
void vui_sdl2_frame_start();

