#ifndef VUI_STBTT_MANAGER_H
#define VUI_STBTT_MANAGER_H

typedef uint32_t VuiGlyphTextureId;

//
// YOU NEED TO IMPLEMENT this function to choose which glyph texture you want styled glyphs (font_id + line_height + codept) to go in.
//
// eg. you may want a group of fonts to use one glyph texture, and another group use a different texture.
//
// another eg. you may want to precompute a glyph for all ASCII characters at program start up: (codept >= 33 && codept <= 126)
//             and then you may also want the rest of the unicode characters to be in a different that is glyph texture that is
//             cleared and repacked every fram.
//
VuiGlyphTextureId vui_stbtt_get_styled_glyph_texture_id(VuiFontId font_id, float line_height, int32_t codept);

//
// creates a font
//
// @param font_file_bytes:
//     a pointer to the start of the ttf font file. the memory the pointer
//     points to must persist as long as this font exists.
//
// @return: a value of zero is returned on failure, otherwise a valid font identifier is returned.
//
VuiFontId vui_stbtt_font_add(const uint8_t* font_file_bytes);

//
// removes the font
//
// @param font_id: the identifier of the font you want to remove.
//
void vui_stbtt_font_remove(VuiFontId font_id);

//
// gets the stbtt_fontinfo of the font
//
// @param font_id: the identifier of the font that you want the font info for.
//
// @return: a pointer to the font info.
//     WANGING: this pointer can be made invalid if vui_stbtt_font_add is called after this function.
//              this is if the internal font pool reallocates. just make sure you call this function
//              again if you add another font.
//
stbtt_fontinfo* vui_stbtt_font_get(VuiFontId font_id);

//
// creates a glyph texture
//
// @param texture_id:
//     the user's made up texture identifier that you want to be passed to VUI when refering to this glyph texture.
//     this is so that when you come to render a window and access the render commands in the VuiWindowRender structure.
//     the VuiRenderCmd.texture_id will use this @param(texture_id) when refering to this glyph texture.
//
// @param width_and_height:
//     the initial width and height of the texture in pixels. if you provide 0 then a internal default value is used.
//
// @param margin: the spacing around every glyph in the texture. if you provide 0 then a internal default value is used.
//
// @param oversample_x, oversample_y: see stbtt_PackSetOversampling for documentation. if you provide 0 then a internal default value is used.
//
// @return: a value of zero is returned on failure, otherwise a valid glyph texture identifier is returned.
//
VuiGlyphTextureId vui_stbtt_glyph_texture_add(VuiTextureId texture_id, uint32_t width_and_height, uint32_t margin, uint8_t oversample_x, uint8_t oversample_y);

//
// removes the glyph texture
//
// @param glyph_texture_id: the identifier of the glyph texture you want to remove.
//
void vui_stbtt_glyph_texture_remove(VuiGlyphTextureId glyph_texture_id);

//
// gets the pixels of the glyph texture. you should only call this after vui_stbtt_glyph_texture_pack
// has been called for this @param(glyph_texture_id). make sure you do not call this after you use
// vui_stbtt_glyph_texture_resize, as it will mess with the buffer.
//
// @param glyph_texture_id: the identifier of the glyph texture you want to get the pixels of.
//
// @param width_and_height_out:
//     a pointer to write the width_and_height of the glyph texture to.
//     this can be NULL if you do not want this data.
//
// @return: a pointer to the start of the pixels, the size of the buffer is *width_and_height_out * *width_and_height_out
//
uint8_t* vui_stbtt_glyph_texture_get_pixels_and_wh(VuiGlyphTextureId glyph_texture_id, uint32_t* width_and_height_out);

//
// resizes the glyph texture. use this function to reserve more space or shrink the current buffer.
// WARNING: if you call this function after vui_stbtt_glyph_texture_pack, it will make the pixel data invalid.
// so be sure to call the pack function after the resize.
//
// @param glyph_texture_id: the identifier of the glyph texture you want to get the pixels of.
//
// @param width_and_height: the new width and height of the texture in pixels
//
// @return: vui_false on allocation failure and vui_true on success.
//
VuiBool vui_stbtt_glyph_texture_resize(VuiGlyphTextureId glyph_texture_id, uint32_t width_and_height);

//
// adds a glyph that you want to be rendered to the texture using a certain font and line height.
// these glyphs are only rendered to the texture when vui_stbtt_glyph_texture_pack is called.
//
// @param glyph_texture_id: the identifier of the glyph texture you want to render this glyph to.
//
// @param font_id: the font identifier that you want this glyph to be rendered with.
//
// @param line_height: the height of the glyph in pixels
//
// @param stb_glyph_idx:
//     the glyph index that is returned from the stbtt_FindGlyphIndex function.
//     WARNING: this glyph index is unique to the font it was used with. so make sure
//              that the font info you pass in to stbtt_FindGlyphIndex is the one
//              that is refered to be the @param(font_id)
//
// @return: vui_false on allocation failure and vui_true on success.
//
VuiBool vui_stbtt_glyph_texture_add_styled_glyph(VuiGlyphTextureId glyph_texture_id, VuiFontId font_id, float line_height, int stb_glyph_idx);

//
// clears the internal list of styled glyphs that are going to be rendered to a texture.
//
// @param glyph_texture_id: the identifier of the glyph texture
//
void vui_stbtt_glyph_texture_clear_styled_glyphs(VuiGlyphTextureId glyph_texture_id);

//
// this goes over internal list of styled glyphs and renders them to the internal pixel buffer.
// if the pixel buffer cannot hold all of the glyphs, it will be doubled in size until it can.
//
// @param glyph_texture_id: the identifier of the glyph texture.
//
// @return: vui_false on allocation failure and vui_true on success.
//
VuiBool vui_stbtt_glyph_texture_pack(VuiGlyphTextureId glyph_texture_id);

#endif // VUI_STBTT_MANAGER_H

