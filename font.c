#include <stdlib.h>
#include <memory.h>
#include <pango/pangocairo.h>
#include "font.h"


/**
 * @brief Text layout object
 * @details You should <b>NOT</b> access this data structure directly.
 */
struct text_layout
{
	/** @brief Cairo surface */
	cairo_surface_t* surface;

	/** @brief Cairo handle */
	cairo_t* cairo;

	/** @brief Pango layout object */
	PangoLayout* layout;

	/** @brief Pointer to RAW image data */
	unsigned char* surface_data;

	/** @brief Width of buffer */
	int width;

	/** @brief Height of buffer */
	int height;
};

text_layout text_layout_create(const int width, const int height)
{
	text_layout layout = NULL;

	layout = calloc(1, sizeof(struct text_layout));

	layout->surface = cairo_image_surface_create(CAIRO_FORMAT_A8, width, height);
	layout->cairo = cairo_create(layout->surface);
	layout->layout = pango_cairo_create_layout(layout->cairo);
	layout->width = width;
	layout->height = height;

	/* Set default font */

	text_layout_set_font(layout, "Liberation Sans", 24);
	cairo_set_source_rgba(layout->cairo, 1.0, 1.0, 1.0, 1.0);

	return layout;
}

void text_layout_destroy(text_layout layout)
{
	if(!layout) return;
	if(layout->cairo)	cairo_destroy(layout->cairo);
	if(layout->surface)	cairo_surface_destroy(layout->surface);

}

void text_layout_set_font(text_layout layout, const char* font, const int size)
{
	PangoFontDescription *desc;

	desc = pango_font_description_from_string (font);
	if(!desc) return;
	pango_layout_set_font_description(layout->layout, desc);
	pango_font_description_set_size(desc, PANGO_SCALE * size);
	pango_font_description_free(desc);
}

void text_layout_render_markup_text(text_layout layout, const char* markup_text)
{
	if(!layout||!markup_text) return;

	layout->surface_data = cairo_image_surface_get_data(layout->surface);
	memset(layout->surface_data, 0, layout->width * layout->height);
	pango_layout_set_markup(layout->layout, markup_text, -1);
	pango_cairo_show_layout(layout->cairo, layout->layout);

}

void text_layout_copy_to_yuv420p(const text_layout layout, const int x, const int y, unsigned char* image, const int img_w, const int img_h)
{
	int i,j;
	unsigned char* src;
	unsigned char* destY;
	unsigned char* destU;
	unsigned char* destV;

	if(!layout||!layout->surface_data) return;

	for(i = 0; i < layout->height; ++i)
	{
		src = &(layout->surface_data[layout->width * i]);
		destY = &(image[img_w * (y + i) + x]);
		destU = &(image[img_w * img_h + img_w / 2 * (y / 2 + i / 2) + x / 2]);
		destV = &(image[img_w * img_h * 5 / 4 + img_w / 2 * (y / 2 + i / 2) + x / 2]);


		for(j = 0; j < layout->width; ++j)
		{
			if(x + j >= img_w) break;
			if(src[j])
			{
				destY[j] = src[j];
				destU[j / 2] = -128;
				destV[j / 2] = -128;
			}
		}
	}
}

void text_layout_copy_to_yuv422(const text_layout layout, const int x, const int y, unsigned char* image, const int img_w, const int img_h)
{
	int i,j;
	unsigned char* src;
	unsigned char* destY;

	if(!layout||!layout->surface_data) return;

	for(i = 0; i < layout->height; ++i)
	{
		src = &(layout->surface_data[layout->width * i]);
		destY = &(image[img_w * (y + i) * 2  + x * 2]);


		for(j = 0; j < layout->width; ++j)
		{
			if(x + j >= img_w) break;
			if(src[j])
			{
				destY[j * 2] = src[j];
				destY[j * 2 + 1] = -128;
				if((x + j) % 2)
					destY[j * 2 - 1] = -128;
				else
					destY[j * 2 + 3] = -128;
			}
		}
	}
}
