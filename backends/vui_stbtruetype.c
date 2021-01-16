
VuiPositionTextRet vui_stbtt_position_text(VuiPositionTextArgs* args) {
	if (args->text_length == 0) { return (VuiPositionTextRet){ .vec2 = VuiVec2_zero }; }

	//
	// get the font info, vertical metrics and the scale for the line height.
	//

	stbtt_fontinfo* info = vui_stbtt_get_info(args->font_id);
	int ascent, descent, line_gap;
	stbtt_GetFontVMetrics(info, &ascent, &descent, &line_gap);

	float scale = stbtt_ScaleForPixelHeight(info, args->line_height);
	ascent = roundf(ascent * scale);
	descent = roundf(descent * scale);

	//
	// move the position down to the baseline
	VuiVec2 pos = args->top_left;
	pos.y += ascent;
	float line_start_x = args->top_left.x;

	VuiVec2 max_pos = {0};

	uint32_t i = 0;
	int32_t codept = 0;
	int codept_glyph = 0;
	VuiBool is_not_first_word_on_line = vui_false;
	VuiVec2 word_start_pos = pos;
	uint32_t word_start_i = i;
	uint32_t word_start_codept = 0;
	uint32_t word_start_glyph = 0;
	// disable the first scan ahead phase if word wrapping is disabled (word_wrap_at_width == 0.f)
	uint32_t word_end_i = args->word_wrap_at_width == 0.f ? args->text_length : i;
	//
	// if word_wrap_at_width is enabled (> 0.f), then this loop works in two phases.
	// one iteration is to see if a word will go past the wrap wrapping boundary.
	// if so the position of the baseline is moved to the next line.
	//
	// then in the next iteration, we actually position each glyph in the word.
	//
	// if word wrapping is disabled, then the whole text is just positioned in a single loop.
	//
	while (i < args->text_length) {
		VuiBool found_delimiter = vui_false;
		//
		// if we are scanning ahead this iteration and this is the first word on the line.
		// this will never wrap to a new line, so find where the group of word delimiters
		// end and continue to the next iteration. so stop on the first non word delimiter after
		// we have found a word delimiter. so the word delimiters are included in the previous word.
		VuiBool is_scanning_word_for_wrapping = word_start_i == word_end_i;
		if (is_scanning_word_for_wrapping && !is_not_first_word_on_line) {
			while (1) {
				int32_t next_codept = 0;
				uint32_t codept_size = vui_utf8_codepoint(&args->text[i], &next_codept);
				VuiBool found = vui_utf8_is_word_delimiter(next_codept);
				if (!found) { if (found_delimiter) break; }
				else found_delimiter = vui_true;
				i += codept_size;
			}
			word_end_i = i;
			i = word_start_i;
			is_not_first_word_on_line = vui_true;
			continue;
		}

		i += vui_utf8_codepoint(&args->text[i], &codept);
		codept_glyph = stbtt_FindGlyphIndex(info, codept);
		if (is_scanning_word_for_wrapping) {
			word_start_codept = codept;
			word_start_glyph = codept_glyph;
		}

		//
		// iterate over each glyph and maybe render the glyph if we are not scanning.
		// if we are scanning, we check to see if this word does wrap to a new line.
		while (1) {
			int advance_width;
			int left_side_bearing;
			stbtt_GetGlyphHMetrics(info, codept_glyph, &advance_width, &left_side_bearing);
			float advance_width_f = advance_width * scale;

			if (!is_scanning_word_for_wrapping) {
				if (args->cursor_num) {
					if (i > args->cursor_num - 1) {
						// move the position from the baseline to the top of the line.
						pos.y -= ascent;
						return (VuiPositionTextRet){ .vec2 = pos };
					}
				} else if (args->cursor_pos.x || args->cursor_pos.y) {
					if (pos.x + advance_width_f > args->cursor_pos.x && pos.y - descent + line_gap > args->cursor_pos.y) {
						//
						// move back a character if the cursor is in the first half of this glyph
						if ((pos.x + advance_width_f * 0.5f) > args->cursor_pos.x) {
RETURN_CURSOR_IDX_GO_BACK:
							i = vui_utf8_prev_char(args->text, i);
						}
						if (i > args->text_length) {
							i = args->text_length;
						}
						return (VuiPositionTextRet){ .u32 = i };
					} else if (pos.y - descent + line_gap > args->cursor_pos.y + args->line_height) {
						//
						// we have definately passed the line we where looking for.
						// so go back to the last character of the line and return.
						//
						i = vui_utf8_prev_char(args->text, i);
						goto RETURN_CURSOR_IDX_GO_BACK;
					}
				}
			}

			if (codept < 32) { // is ASCII control key
				if (codept == '\n') {
					if (!is_scanning_word_for_wrapping) {
						//
						// got a newline so advance to the next line.
						if (pos.x > max_pos.x)
							max_pos.x = pos.x;
						pos.y += args->line_height;
						pos.x = line_start_x;
					}

					//
					// new line is a special word delimiter that will terminate
					// the end of a group of word delimiters as well.
					//
					is_not_first_word_on_line = vui_false;
					break;
				} else if (codept != '\r') {
					//
					// substitute the rest of the control characters as spaces.
					int advance_width;
					int left_side_bearing;
					codept_glyph = stbtt_FindGlyphIndex(info, ' ');
					stbtt_GetGlyphHMetrics(info, codept_glyph, &advance_width, &left_side_bearing);
					pos.x += advance_width * scale;
				}

				i += vui_utf8_codepoint(&args->text[i], &codept);
				codept_glyph = stbtt_FindGlyphIndex(info, codept);

				//
				// treat the control characters as a delimiter and continue to the next character.
				found_delimiter = vui_true;
				continue;
			} else {
				if (!is_scanning_word_for_wrapping) {
					if (args->render_glyph_fn) {
						//
						// we have a render function, so it's time to render our glyph by calling back to VUI.
						vui_stbtt_render_glyph(pos, args->font_id, args->line_height, codept, codept_glyph, vui_false, args->render_glyph_fn);
					} else {
						//
						// call this user implemented function to keep track of what styled glyphs we have.
						// so we can ensure they exist in a glyph texture somewhere.
						vui_stbtt_found_glyph(args->font_id, args->line_height, codept, codept_glyph);
					}
				}
			}

			//
			// advance the position by the advance width of this glyph.
			pos.x += advance_width_f;

			//
			// stop if we have just finished positioning the last codepoint/glyph in the text.
			if (i >= args->text_length) break;

			//
			// if we are scanning try and see if we wrap.
			if (
				is_scanning_word_for_wrapping &&
				is_not_first_word_on_line &&
				pos.x - line_start_x > args->word_wrap_at_width
			) {
				//
				// we wrap on this word, so advance the position to the next line
				// and stop the scanning here.
				//

				if (pos.x > max_pos.x)
					max_pos.x = pos.x;

				pos.y += args->line_height;
				pos.x = line_start_x;

				//
				// finish scanning for the end of the word. include all the word delimiters
				// that directly follow this word.
				uint32_t codept_size = 0;
				while (1) {
					VuiBool found = vui_utf8_is_word_delimiter(codept);
					if (!found) { if (found_delimiter) break; }
					else found_delimiter = vui_true;
					i += codept_size;
					codept_size = vui_utf8_codepoint(&args->text[i], &codept);
				}

				//
				// delimiters processed by themselves will need to be manually advanced here
				if (i == word_start_i) {
					i += vui_utf8_codepoint(&args->text[i], &codept);
				}
				break;
			}

			//
			// if we are scanning see if the codepoint we just positioned is a delimiter.
			if (is_scanning_word_for_wrapping) {
				found_delimiter = vui_utf8_is_word_delimiter(codept);
			}

			//
			// get the next glyph and add the kerning spacing between the current glyph and the next glyph.
			//

			int32_t next_codept = 0;
			uint32_t codept_size = vui_utf8_codepoint(&args->text[i], &next_codept);

			//
			// if we are scanning and the next codepoint is not a delimiter then this is the end of scan.
			if (is_scanning_word_for_wrapping && found_delimiter && !vui_utf8_is_word_delimiter(next_codept)) {
				break;
			}

			int next_codept_glyph = stbtt_FindGlyphIndex(info, next_codept);

			int kern;
			kern = stbtt_GetGlyphKernAdvance(info, codept_glyph, next_codept_glyph);
			pos.x += roundf(kern * scale);

			//
			// stop if we have finished properly position a word.
			// we also allowed the end of the word to apply the kerning
			// between this word and the next character that follows.
			if (!is_scanning_word_for_wrapping && i >= word_end_i) break;

			codept = next_codept;
			codept_glyph = next_codept_glyph;
			i += codept_size;
		}

		if (is_scanning_word_for_wrapping) {
			//
			// we just completed our scan of the current word.
			// so reset the variables back to what they were at start of the word
			// to begin positioning them properly.
			//

			word_end_i = i;
			i = word_start_i;
			codept = word_start_codept;
			codept_glyph = word_start_glyph;

			//
			// only reset the position if we did not move to a new line.
			if (pos.y == word_start_pos.y) {
				pos = word_start_pos;
			}
			is_not_first_word_on_line = vui_true;
		} else if (args->word_wrap_at_width > 0.f) {
			//
			// we just finished positioning a word, so store where the next word starts from.
			//
			word_start_pos = pos;
			word_start_i = i;
			word_end_i = i;
		}
	}

	if (args->cursor_num) {
		// move the position from the baseline to the top of the line.
		pos.y -= ascent;
		return (VuiPositionTextRet){ .vec2 = pos };
	} else if (args->cursor_pos.x || args->cursor_pos.y) {
		return (VuiPositionTextRet){ .u32 = args->text_length };
	}


	max_pos.x = vui_max(max_pos.x, pos.x);
	max_pos.y = pos.y - descent;
	return (VuiPositionTextRet){ .vec2 = max_pos };
}

