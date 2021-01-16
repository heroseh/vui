#ifndef vui_stbtt_glyph_texture_default_width_and_height
#define vui_stbtt_glyph_texture_default_width_and_height 512
#endif

#ifndef vui_stbtt_glyph_texture_default_margin
#define vui_stbtt_glyph_texture_default_margin 1
#endif

#ifndef vui_stbtt_glyph_texture_default_oversample_x
#define vui_stbtt_glyph_texture_default_oversample_x 1
#endif

#ifndef vui_stbtt_glyph_texture_default_oversample_y
#define vui_stbtt_glyph_texture_default_oversample_y 1
#endif

// ==========================================================
//
//
// Internal API
//
//
// ==========================================================

#define _VuiFontId_pool_id_MASK 0x000fffff
#define _VuiFontId_pool_id_SHIFT 0
#define _VuiFontId_counter_MASK 0xfff00000
#define _VuiFontId_counter_SHIFT 20

#define _VuiGlyphTextureId_pool_id_MASK 0x000fffff
#define _VuiGlyphTextureId_pool_id_SHIFT 0
#define _VuiGlyphTextureId_counter_MASK 0xfff00000
#define _VuiGlyphTextureId_counter_SHIFT 20

typedef struct _VuiStbttInfo _VuiStbttInfo;
struct _VuiStbttInfo {
	stbtt_fontinfo inner;
	uint16_t counter;
};

typedef struct _VuiStbttRect _VuiStbttRect;
struct _VuiStbttRect {
	uint32_t x;
	uint32_t y;
	uint32_t w;
	uint32_t h;
};

typedef struct _VuiStbttStyledGlyph _VuiStbttStyledGlyph;
struct _VuiStbttStyledGlyph {
	VuiFontId font_id;
	float line_height;
	int stb_glyph_idx;
	uint32_t tex_height;
};

typedef struct _VuiStbttStyledGlyphRect _VuiStbttStyledGlyphRect;
struct _VuiStbttStyledGlyphRect {
	float scale;
	VuiRect offset;
	_VuiStbttRect tex;
};

typedef struct _VuiStbttGlyphTexture _VuiStbttGlyphTexture;
struct _VuiStbttGlyphTexture {
	VuiStk(uint8_t) pixels;
	VuiStk(_VuiStbttStyledGlyph) styled_glyphs;
	VuiStk(_VuiStbttStyledGlyphRect) styled_glyph_rects;
	VuiStk(_VuiStbttRect) empty_rects;
	uint32_t width_and_height;
	VuiTextureId texture_id;
	uint32_t margin;
	uint8_t oversample_x;
	uint8_t oversample_y;
	uint16_t counter;
};

typedef_VuiPool(_VuiStbttInfo);
typedef_VuiPool(_VuiStbttGlyphTexture);
typedef struct _VuiStbtt _VuiStbtt;
struct _VuiStbtt {
	VuiPool(_VuiStbttInfo) font_pool;
	VuiPool(_VuiStbttGlyphTexture) glyph_texture_pool;
};

static _VuiStbtt _vui_stbtt = {0};

static _VuiStbttGlyphTexture* _vui_stbtt_glyph_texture_get(VuiGlyphTextureId glyph_texture_id) {
	vui_assert(glyph_texture_id, "cannot get a glyph texture with a NULL identifier");

	//
	// get the counter and ensure that it matches the one stored in the glyph_texture_id
	VuiPoolId pool_id = (glyph_texture_id & _VuiGlyphTextureId_pool_id_MASK) >> _VuiGlyphTextureId_pool_id_SHIFT;
	uint16_t counter = (glyph_texture_id & _VuiGlyphTextureId_counter_MASK) >> _VuiGlyphTextureId_counter_SHIFT;
	_VuiStbttGlyphTexture* tex = _VuiPool_id_to_ptr((_VuiPool*)&_vui_stbtt.glyph_texture_pool, pool_id, sizeof(_VuiStbttGlyphTexture));
	vui_assert(tex->counter == counter, "trying to get a glyph texture with an old identifier");

	return tex;
}

static uint32_t _vui_stbtt_find_styled_glyph_id(_VuiStbttGlyphTexture* tex, VuiFontId font_id, float line_height, int stb_glyph_idx) {
	//
	// see if the glyph already exists in the glyph texture
	_VuiStbttStyledGlyph* styled_glyphs = tex->styled_glyphs;
	uint32_t count = VuiStk_count(styled_glyphs);
	uint32_t glyph_id = 0;
	for (uint32_t i = 0; i < count; i += 1) {
		_VuiStbttStyledGlyph* glyph = &styled_glyphs[i];
		if (glyph->stb_glyph_idx == stb_glyph_idx && glyph->line_height == line_height && glyph->font_id == font_id) {
			glyph_id = i + 1;
			break;
		}
	}

	return glyph_id;
}

static VuiBool _vui_stbtt_glyph_texture_add_empty_rect(_VuiStbttGlyphTexture* tex, _VuiStbttRect* rect) {
	uint32_t sum = rect->w + rect->h;

	//
	// the empty rectangles are stored in size order low to high.
	// iterate in reverse to find the rectangle that is smaller.
	// then move after the rectangle so we insert after it.
	uint32_t rect_i = VuiStk_count(tex->empty_rects);
	while (rect_i--) {
		_VuiStbttRect* r = &tex->empty_rects[rect_i];
		if (r->w + r->h <= sum) {
			rect_i += 1;
			break;
		}
	}

	if (rect_i == UINT32_MAX) {
		rect_i = 0;
	}

	_VuiStbttRect* r = VuiStk_insert(&tex->empty_rects, rect_i);
	if (!r) return vui_false;
	*r = *rect;

	return vui_true;
}

// ==========================================================
//
//
// Public API
//
//
// ==========================================================

VuiFontId vui_stbtt_font_add(const uint8_t* font_file_bytes) {
	//
	// allocate a new font from the font pool and zero the memory
	VuiPoolId pool_id = 0;
	_VuiStbttInfo* info = _VuiPool_alloc((_VuiPool*)&_vui_stbtt.font_pool, &pool_id, sizeof(_VuiStbttInfo), alignof(_VuiStbttInfo));
	if (info == NULL) return 0;
	memset(&info->inner, 0, sizeof(info->inner));

	//
	// initialize the font, return 0 on failure
	if (stbtt_InitFont(&info->inner, font_file_bytes, 0) == 0)
		return 0;

	//
	// create the font identifier by combining the pool_id and the counter
	VuiFontId font_id = (pool_id << _VuiFontId_pool_id_SHIFT) & _VuiFontId_pool_id_MASK;
	font_id |= (info->counter << _VuiFontId_counter_SHIFT) & _VuiFontId_counter_MASK;

	return font_id;
}

void vui_stbtt_font_remove(VuiFontId font_id) {
	vui_assert(font_id, "cannot remove an font with a NULL identifier");

	//
	// get the counter and ensure that it matches the one stored in the font_id
	VuiPoolId pool_id = (font_id & _VuiFontId_pool_id_MASK) >> _VuiFontId_pool_id_SHIFT;
	uint16_t counter = (font_id & _VuiFontId_counter_MASK) >> _VuiFontId_counter_SHIFT;
	_VuiStbttInfo* info = _VuiPool_id_to_ptr((_VuiPool*)&_vui_stbtt.font_pool, pool_id, sizeof(_VuiStbttInfo));
	vui_assert(info->counter == counter, "trying to remove an info with an old identifier");

	//
	// when we reach the maximum value a counter can hold, set the value to zero.
	// else increment by one.
	if (info->counter == _VuiFontId_counter_MASK >> _VuiFontId_counter_SHIFT) {
		info->counter = 0;
	} else {
		info->counter += 1;
	}

	//
	// now deallocate the font entry in the font pool
	_VuiPool_dealloc((_VuiPool*)&_vui_stbtt.font_pool, pool_id, sizeof(_VuiStbttInfo), alignof(_VuiStbttInfo));
}

stbtt_fontinfo* vui_stbtt_font_get(VuiFontId font_id) {
	vui_assert(font_id, "cannot get an font with a NULL identifier");

	//
	// get the counter and ensure that it matches the one stored in the font_id
	VuiPoolId pool_id = (font_id & _VuiFontId_pool_id_MASK) >> _VuiFontId_pool_id_SHIFT;
	uint16_t counter = (font_id & _VuiFontId_counter_MASK) >> _VuiFontId_counter_SHIFT;
	_VuiStbttInfo* info = _VuiPool_id_to_ptr((_VuiPool*)&_vui_stbtt.font_pool, pool_id, sizeof(_VuiStbttInfo));
	vui_assert(info->counter == counter, "trying to remove an info with an old identifier");

	return &info->inner;
}

VuiGlyphTextureId vui_stbtt_glyph_texture_add(VuiTextureId texture_id, uint32_t width_and_height, uint32_t margin, uint8_t oversample_x, uint8_t oversample_y) {
	//
	// allocate a new glyph texture from the pool
	VuiPoolId pool_id = 0;
	_VuiStbttGlyphTexture* tex = _VuiPool_alloc((_VuiPool*)&_vui_stbtt.glyph_texture_pool, &pool_id, sizeof(_VuiStbttGlyphTexture), alignof(_VuiStbttGlyphTexture));
	if (tex == NULL) return 0;

	//
	// zero the glyph texture but keep the counter the same
	uint16_t counter = tex->counter;
	memset(tex, 0, sizeof(*tex));
	tex->counter = counter;
	tex->texture_id = texture_id;
	tex->width_and_height = width_and_height;
	tex->margin = margin ? margin : vui_stbtt_glyph_texture_default_margin;
	tex->oversample_x = oversample_x ? oversample_x : vui_stbtt_glyph_texture_default_oversample_x;
	tex->oversample_y = oversample_y ? oversample_y : vui_stbtt_glyph_texture_default_oversample_y;
	if (!VuiStk_resize_cap(&tex->pixels, (uintptr_t)width_and_height * (uintptr_t)width_and_height))
		return 0;

	//
	// create the glyph texture identifier by combining the pool_id and the counter
	VuiGlyphTextureId glyph_texture_id = (pool_id << _VuiGlyphTextureId_pool_id_SHIFT) & _VuiGlyphTextureId_pool_id_MASK;
	glyph_texture_id |= (tex->counter << _VuiGlyphTextureId_counter_SHIFT) & _VuiGlyphTextureId_counter_MASK;

	return glyph_texture_id;
}

void vui_stbtt_glyph_texture_remove(VuiGlyphTextureId glyph_texture_id) {
	vui_assert(glyph_texture_id, "cannot remove an glyph texture with a NULL identifier");

	//
	// get the counter and ensure that it matches the one stored in the glyph_texture_id
	VuiPoolId pool_id = (glyph_texture_id & _VuiGlyphTextureId_pool_id_MASK) >> _VuiGlyphTextureId_pool_id_SHIFT;
	uint16_t counter = (glyph_texture_id & _VuiGlyphTextureId_counter_MASK) >> _VuiGlyphTextureId_counter_SHIFT;
	_VuiStbttGlyphTexture* tex = _VuiPool_id_to_ptr((_VuiPool*)&_vui_stbtt.glyph_texture_pool, pool_id, sizeof(_VuiStbttGlyphTexture));
	vui_assert(tex->counter == counter, "trying to remove a glyph texture with an old identifier");

	//
	// when we reach the maximum value a counter can hold, set the value to zero.
	// else increment by one.
	if (tex->counter == _VuiFontId_counter_MASK >> _VuiFontId_counter_SHIFT) {
		tex->counter = 0;
	} else {
		tex->counter += 1;
	}

	//
	// deallocate all the dynamic memory in the glyph texture
	//
	tex->pixels = VuiStk_deinit(tex->pixels);
	tex->styled_glyphs = VuiStk_deinit(tex->styled_glyphs);
	tex->styled_glyph_rects = VuiStk_deinit(tex->styled_glyph_rects);
	tex->empty_rects = VuiStk_deinit(tex->empty_rects);

	//
	// now deallocate the glyph texture entry in the pool
	_VuiPool_dealloc((_VuiPool*)&_vui_stbtt.glyph_texture_pool, pool_id, sizeof(_VuiStbttGlyphTexture), alignof(_VuiStbttGlyphTexture));
}

uint8_t* vui_stbtt_glyph_texture_get_pixels_and_wh(VuiGlyphTextureId glyph_texture_id, uint32_t* width_and_height_out) {
	_VuiStbttGlyphTexture* tex = _vui_stbtt_glyph_texture_get(glyph_texture_id);
	if (width_and_height_out) *width_and_height_out = tex->width_and_height;
	return tex->pixels;
}

VuiBool vui_stbtt_glyph_texture_resize(VuiGlyphTextureId glyph_texture_id, uint32_t width_and_height) {
	_VuiStbttGlyphTexture* tex = _vui_stbtt_glyph_texture_get(glyph_texture_id);
	tex->width_and_height = width_and_height;
	return VuiStk_resize_cap(&tex->pixels, (uintptr_t)width_and_height * (uintptr_t)width_and_height);
}

VuiBool vui_stbtt_glyph_texture_add_styled_glyph(VuiGlyphTextureId glyph_texture_id, VuiFontId font_id, float line_height, int stb_glyph_idx) {
	_VuiStbttGlyphTexture* tex = _vui_stbtt_glyph_texture_get(glyph_texture_id);
	uint32_t styled_glyph_id = _vui_stbtt_find_styled_glyph_id(tex, font_id, line_height, stb_glyph_idx);

	//
	// if it does not, then add it too the array.
	if (styled_glyph_id == 0) {
		//
		// push glyphs on the end, then store the arguments
		_VuiStbttStyledGlyph* g = VuiStk_push(&tex->styled_glyphs);
		if (!g) return vui_false;
		g->font_id = font_id;
		g->line_height = line_height;
		g->stb_glyph_idx = stb_glyph_idx;

		stbtt_fontinfo* info = vui_stbtt_font_get(font_id);
		float scale = stbtt_ScaleForPixelHeight(info, line_height);

		int offset_x, offset_y, offset_ex, offset_ey;
		stbtt_GetGlyphBitmapBox(info, stb_glyph_idx,
								scale * tex->oversample_x,
								scale * tex->oversample_y,
								&offset_x, &offset_y, &offset_ex, &offset_ey);

		g->tex_height = offset_ey - offset_y + tex->oversample_y - 1;
	}

	return vui_true;
}

int _vui_stbtt_styled_glyph_sort_tallest_to_shortest_cmp_fn(const void * a, const void * b) {
	return ((_VuiStbttStyledGlyph*)a)->tex_height < ((_VuiStbttStyledGlyph*)b)->tex_height ? 1 : -1;
}

void vui_stbtt_glyph_texture_clear_styled_glyphs(VuiGlyphTextureId glyph_texture_id) {
	_VuiStbttGlyphTexture* tex = _vui_stbtt_glyph_texture_get(glyph_texture_id);
	VuiStk_clear(tex->styled_glyphs);
}

VuiBool vui_stbtt_glyph_texture_pack(VuiGlyphTextureId glyph_texture_id) {
	_VuiStbttGlyphTexture* tex = _vui_stbtt_glyph_texture_get(glyph_texture_id);

	//
	// initialize the texture pixels if they haven't been already
	if (tex->width_and_height == 0) {
		if (!vui_stbtt_glyph_texture_resize(glyph_texture_id, vui_stbtt_glyph_texture_default_width_and_height)) {
			return vui_false;
		}
	}

	//
	// zero the texture
	uint32_t wh = tex->width_and_height;
	memset(tex->pixels, 0, (uintptr_t)wh * (uintptr_t)wh);

	//
	// clear the empty rectangles of the glyph texture and then store the whole glyph texture as the only empty rectangle.
	VuiStk_clear(tex->empty_rects);
	_VuiStbttRect* r = VuiStk_push(&tex->empty_rects);
	if (!r) return vui_false;
	*r = (_VuiStbttRect) { 0, 0, wh, wh };

	//
	// sort all the styled glyphs tallest to shortest
	qsort(tex->styled_glyphs, VuiStk_count(tex->styled_glyphs), sizeof(*tex->styled_glyphs), _vui_stbtt_styled_glyph_sort_tallest_to_shortest_cmp_fn);
	VuiStk_clear(tex->styled_glyph_rects);
	if (!VuiStk_resize_cap(&tex->styled_glyph_rects, VuiStk_count(tex->styled_glyphs)))
		return vui_false;

	//
	// workout the size of the rectangles for the glyphs
	float sub_x = stbtt__oversample_shift(tex->oversample_x);
	float sub_y = stbtt__oversample_shift(tex->oversample_y);
	float recip_v = 1.f / tex->oversample_x;
	float recip_h = 1.f / tex->oversample_y;
	for (uint32_t glyph_i = 0; glyph_i < VuiStk_count(tex->styled_glyphs); glyph_i += 1) {
		_VuiStbttStyledGlyph* glyph = &tex->styled_glyphs[glyph_i];
		stbtt_fontinfo* info = vui_stbtt_font_get(glyph->font_id);

		float scale = stbtt_ScaleForPixelHeight(info, glyph->line_height);

		int offset_x, offset_y, offset_ex, offset_ey;
		stbtt_GetGlyphBitmapBox(info, glyph->stb_glyph_idx,
								scale * tex->oversample_x,
								scale * tex->oversample_y,
								&offset_x, &offset_y, &offset_ex, &offset_ey);

		//
		// get the width and height of the glyph
		uint32_t w = offset_ex - offset_x + tex->oversample_x - 1;
		uint32_t h = offset_ey - offset_y + tex->oversample_y - 1;

		_VuiStbttStyledGlyphRect* gr = VuiStk_push(&tex->styled_glyph_rects);
		if (!gr) return vui_false;

		gr->scale = scale;

		//
		// create the offset that can be applied to the baseline.
		gr->offset = VuiRect_init(
			(float)offset_x * recip_h + sub_x,
			(float)offset_y * recip_v + sub_y,
			(float)offset_ex * recip_h + sub_x,
			(float)offset_ey * recip_v + sub_y
		);

		//
		// add the margin to make this the outer (width and height)
		w += tex->margin * 2;
		h += tex->margin * 2;

		gr->tex.w = w;
		gr->tex.h = h;
	}

	//
	// assign all the glyphs a place in the texture
	for (uint32_t glyph_i = 0; glyph_i < VuiStk_count(tex->styled_glyphs); glyph_i += 1) {
TRY_AGAIN:{}
		_VuiStbttStyledGlyphRect* gr = &tex->styled_glyph_rects[glyph_i];

		//
		// iterate in reverse to find a empty rectangle that fits our glyph.
		// since the array is stored in small to large order.
		uint32_t rect_i = VuiStk_count(tex->empty_rects);
		while (rect_i--) {
			r = &tex->empty_rects[rect_i];
			if (r->w >= gr->tex.w && r->h >= gr->tex.h) break;
		}

		//
		// check if we are out of space so we can double the glyph texture and try again.
		if (rect_i == UINT32_MAX) {
			uint32_t old_wh = wh;
			wh *= 2;
			vui_stbtt_glyph_texture_resize(glyph_texture_id, wh);

			//
			// the new empty rectangle to the right of where the texture used to end.
			// this does not fill the height out to the new size.
			// it just creates an empty rectangle the size of the old texture.
			_VuiStbttRect empty_rect_right = { old_wh, 0, old_wh, old_wh };
			if (!_vui_stbtt_glyph_texture_add_empty_rect(tex, &empty_rect_right))
				return vui_false;

			//
			// the new empty rectangle to the bottom of the old texture, that fills the whole new width.
			_VuiStbttRect empty_rect_bottom = { 0, old_wh, wh, old_wh };
			if (!_vui_stbtt_glyph_texture_add_empty_rect(tex, &empty_rect_bottom))
				return vui_false;

			goto TRY_AGAIN;
		}

		//
		// get the rectangle that fit our glyph from the stack.
		// shift removing shouldn't be too bad, since the large
		// empty rectangles are stored from the back.
		_VuiStbttRect empty_r = tex->empty_rects[rect_i];
		VuiStk_remove_shift(tex->empty_rects, rect_i);

		//
		// give the glyph the top left hand side of the empty rectangle.
		gr->tex.x = empty_r.x + tex->margin;
		gr->tex.y = empty_r.y + tex->margin;

		//
		// if the glyph does not take up the whole width of this empty rectangle.
		// then create an empty rectangle to the right, that has the same height
		// as the glyph and consumes the rest of the width.
		if (gr->tex.w != empty_r.w) {
			_VuiStbttRect split_right_rect = {
				empty_r.x + gr->tex.w, empty_r.y,
				empty_r.w - gr->tex.w, gr->tex.h
			};

			_vui_stbtt_glyph_texture_add_empty_rect(tex, &split_right_rect);
		}

		//
		// if the glyph does not take up the whole height of this empty rectangle.
		// then create a empty rectangle underneath the glyph, that consumes the
		// rest of the width and height.
		if (gr->tex.h != empty_r.h) {
			_VuiStbttRect split_bottom_rect = {
				empty_r.x, empty_r.y + gr->tex.h,
				empty_r.w, empty_r.h - gr->tex.h
			};

			_vui_stbtt_glyph_texture_add_empty_rect(tex, &split_bottom_rect);
		}

		//
		// remove the margin from the width and the height;
		gr->tex.w -= tex->margin * 2;
		gr->tex.h -= tex->margin * 2;
	}

	//
	// render the glyphs to their assigned location in the texture.
	//
	uint8_t* pixels = tex->pixels;
	for (uint32_t glyph_i = 0; glyph_i < VuiStk_count(tex->styled_glyphs); glyph_i += 1) {
		_VuiStbttStyledGlyph* glyph = &tex->styled_glyphs[glyph_i];
		_VuiStbttStyledGlyphRect* gr = &tex->styled_glyph_rects[glyph_i];
		stbtt_fontinfo* info = vui_stbtt_font_get(glyph->font_id);
		float scale = gr->scale;

		//
		// render the glyph to its assigned location in the texture.
		stbtt_MakeGlyphBitmapSubpixel(info,
									 pixels + gr->tex.x + gr->tex.y * wh,
									 gr->tex.w - tex->oversample_x + 1,
									 gr->tex.h - tex->oversample_y + 1,
									 wh,
									 scale * tex->oversample_x,
									 scale * tex->oversample_y,
									 0,
									 0,
									 glyph->stb_glyph_idx);

		//
		// do the oversampling on the glyph that is at its assigned location in texture
		//
		if (tex->oversample_x > 1)
			stbtt__h_prefilter(pixels + gr->tex.x + gr->tex.y * wh, gr->tex.w, gr->tex.h, wh, tex->oversample_x);

		if (tex->oversample_y > 1)
			stbtt__v_prefilter(pixels + gr->tex.x + gr->tex.y * wh, gr->tex.w, gr->tex.h, wh, tex->oversample_y);
	}

	return vui_true;
}

stbtt_fontinfo* vui_stbtt_get_info(VuiFontId font_id) {
	return vui_stbtt_font_get(font_id);
}

void vui_stbtt_render_glyph(VuiVec2 baseline_pos, VuiFontId font_id, float line_height, int32_t codept, int stb_glyph_idx, VuiBool align_to_integer, VuiRenderGlyphFn render_glyph_fn) {
	VuiGlyphTextureId glyph_texture_id = vui_stbtt_get_styled_glyph_texture_id(font_id, line_height, codept);
	_VuiStbttGlyphTexture* tex = _vui_stbtt_glyph_texture_get(glyph_texture_id);

	//
	// get the styled glyph from the glyph texture's internal list
	uint32_t styled_glyph_id = _vui_stbtt_find_styled_glyph_id(tex, font_id, line_height, stb_glyph_idx);
	uint32_t rects_count = VuiStk_count(tex->styled_glyph_rects);
	vui_assert(styled_glyph_id && styled_glyph_id <= rects_count, "unable to find codepoint '%lc' with a line_height of '%f' and a font_id of '%u'", codept, line_height, font_id);
	_VuiStbttStyledGlyphRect* gr = &tex->styled_glyph_rects[styled_glyph_id - 1];

	//
	// calculate the rectangle in screen space where the glyph will go.
	VuiRect rect;
	if (align_to_integer) {
		float x = floorf((baseline_pos.x + gr->offset.x) + 0.5f);
		float y = floorf((baseline_pos.y + gr->offset.y) + 0.5f);
		rect.x = x;
		rect.y = y;
		rect.ex = x + VuiRect_width(&gr->offset);
		rect.ey = y + VuiRect_height(&gr->offset);
	} else {
		rect.x = baseline_pos.x + gr->offset.x;
		rect.y = baseline_pos.y + gr->offset.y;
		rect.ex = baseline_pos.x + gr->offset.ex;
		rect.ey = baseline_pos.y + gr->offset.ey;
	}

	//
	// calculate the normalize uv coordinates where the glyph lives in the glyph texture.
	//
	float ratio = 1.f / tex->width_and_height;
	VuiRect uv_rect = VuiRect_init(
		gr->tex.x * ratio,
		gr->tex.y * ratio,
		(gr->tex.x + gr->tex.w) * ratio,
		(gr->tex.y + gr->tex.h) * ratio
	);

	//
	// call the VUI callback function to render the glyph
	render_glyph_fn(&rect, tex->texture_id, &uv_rect);
}

VuiBool vui_stbtt_found_glyph(VuiFontId font_id, float line_height, int32_t codept, int32_t stb_glyph_idx) {
	VuiGlyphTextureId glyph_texture_id = vui_stbtt_get_styled_glyph_texture_id(font_id, line_height, codept);
	return vui_stbtt_glyph_texture_add_styled_glyph(glyph_texture_id, font_id, line_height, stb_glyph_idx);
}

