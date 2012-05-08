#ifndef FONT_H_
#define FONT_H_

typedef struct text_layout* text_layout;

text_layout text_layout_create(const int width, const int height);
void text_layout_destroy(text_layout layout);
void text_layout_set_font(text_layout layout, const char* font, const int size);
void text_layout_render_markup_text(text_layout layout, const char* markup_text);
void text_layout_copy_to_yuv420p(const text_layout layout, const int x, const int y, unsigned char* image, const int img_w, const int img_h);


#endif /* FONT_H_ */
