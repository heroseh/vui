
static uint32_t _vui_stbtt_utf8codepoint(char* str, int32_t* out_codepoint) {
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

VuiVec2 vui_stbtt_position_text(void* userdata, VuiFontId font_id, float line_height, char* text, uint32_t text_length, float wrap_at_width, VuiVec2 top_left, VuiRenderGlyphFn render_glyph_fn) {
	if (text_length == 0) { return VuiVec2_zero; }

	//
	// get the font info, vertical metrics and the scale for the line height.
	//

	stbtt_fontinfo* info = vui_stbtt_get_info(font_id);
	int ascent, descent, line_gap;
	stbtt_GetFontVMetrics(info, &ascent, &descent, &line_gap);

	float scale = stbtt_ScaleForPixelHeight(info, line_height);
	ascent = roundf(ascent * scale);
	descent = roundf(descent * scale);

	//
	// setup these variables to use the first codepoint in the text.
	//

	int32_t codept = 0;
	uint32_t i = _vui_stbtt_utf8codepoint(text, &codept);
	int codept_glyph = stbtt_FindGlyphIndex(info, codept);

	//
	// move the position down to the baseline
	VuiVec2 pos = top_left;
	pos.y += ascent;

	VuiVec2 max_pos = {0};

	//
	// iterate over every codepoint and position the glyphs one by one.
	while (1) {
		if (render_glyph_fn) {
			//
			// we have a render function, so it's time to render our glyph by calling back to VUI.
			vui_stbtt_render_glyph(pos, font_id, line_height, codept, codept_glyph, vui_true, render_glyph_fn);
		} else {
			//
			// call this user implemented function to keep track of what styled glyphs we hvae.
			// so we can ensure they exist in a glyph texture somewhere.
			vui_stbtt_found_glyph(font_id, line_height, codept, codept_glyph);
		}

		//
		// advance the position by the advance width of this glyph.
		int advance_width;
		int left_side_bearing;
		stbtt_GetGlyphHMetrics(info, codept_glyph, &advance_width, &left_side_bearing);
		pos.x += advance_width * scale;

		//
		// TODO: implement word wrapping here.
		//

		//
		// stop if we have just finished positioning the last codepoint/glyph in the text.
		if (i >= text_length) break;

		//
		// get the next glyph and add the kerning spacing between the current glyph and the next glyph.
		//

		int32_t next_codept = 0;
		i += _vui_stbtt_utf8codepoint(&text[i], &next_codept);
		int next_codept_glyph = stbtt_FindGlyphIndex(info, next_codept);

		int kern;
		kern = stbtt_GetGlyphKernAdvance(info, codept_glyph, next_codept_glyph);
		pos.x += roundf(kern * scale);

		codept = next_codept;
		codept_glyph = next_codept_glyph;
	}

	max_pos.x = vui_max(max_pos.x, pos.x);
	max_pos.y = pos.y - descent;
	return max_pos;
}
