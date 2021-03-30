#ifndef VUI_STBTT_H
#define VUI_STBTT_H

#ifndef VUI_H
#include "vui.h"
#endif

#ifndef __STB_INCLUDE_STB_TRUETYPE_H__
#include "stb_truetype.h"
#endif

// =====================================================================
//
// YOU NEED TO IMPLEMENT these functions if you have your own Font Manager and texture packer system.
// you can just use the vui_stbtruetype_manager.h/c files that implement a simple font manager and texture packer if you do not have one.
//
// see vui_stbtruetype_manager.c and look for these functions if you need to see how these can be implemented.
//

//
// gets the stbtt_fontinfo for the font with the font_id
stbtt_fontinfo* vui_stbtt_get_info(VuiFontId font_id);

//
// this is called to render a glyph with a certain font and line height.
// you will need to call render_glyph_fn(rect, texture_id, uv_rect) internally.
void vui_stbtt_render_glyph(VuiVec2 baseline_pos, VuiFontId font_id, float line_height, int32_t codept, int stb_glyph_idx, VuiBool align_to_integer, VuiRenderGlyphFn render_glyph_fn);

//
// this function is called when the text control is being sized up.
// this is before rendering, so you can ensure a glyph with this font and line height exists somewhere in a texture.
VuiBool vui_stbtt_found_glyph(VuiFontId font_id, float line_height, int32_t codept, int stb_glyph_idx);

//
// =====================================================================

//
// this is the function that gets passed in to the position_text_fn field of the VuiSetup structure when initializing VUI.
VuiPositionTextRet vui_stbtt_position_text(VuiPositionTextArgs* args);

#endif

