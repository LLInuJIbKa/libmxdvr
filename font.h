/**
 * @file font.h
 * @author Ruei-Yuan Lu (RueiYuan.Lu@gmail.com)
 * @brief Font rendering module
 * @details This module provides font rendering feature by using Pango. Users can use pango markup language to encode text with attributes.
 */
#ifndef FONT_H_
#define FONT_H_


/**
 * @brief Text layout object
 */
typedef struct text_layout* text_layout;

/**
 * @brief Create text layout object.
 * @param width surface width
 * @param height surface height
 * @retval object handle
 */
text_layout text_layout_create(const int width, const int height);

/**
 * @brief Delete text layout object.
 * @param layout Target object to delete
 */
void text_layout_destroy(text_layout layout);

/**
 * @brief Set font to use for text layout.
 * @param layout Target text layout object
 * @param font Font name, can be queried by using fc-list command
 * @param size Font size in pixel
 */
void text_layout_set_font(text_layout layout, const char* font, const int size);

/**
 * @brief Render pango markup string to internal buffer.
 * @details Render pango markup string to internal buffer. The internal buffer will be clean before rendering text.
 * @param layout Target text layout object
 * @param markup_text String to render
 * @see http://developer.gnome.org/pango/stable/PangoMarkupFormat.html
 */
void text_layout_render_markup_text(text_layout layout, const char* markup_text);

/**
 * @brief Write data in internal buffer to existed yuv420p surface.
 * @param layout Target text layout object that contains rendered image
 * @param x X coordinate of output image
 * @param y Y coordinate of output image
 * @param image Pointer to output image
 * @param img_w width of output image
 * @param img_h height of output image
 */
void text_layout_copy_to_yuv420p(const text_layout layout, const int x, const int y, unsigned char* image, const int img_w, const int img_h);

/**
 * @brief Write data in internal buffer to existed yuv422 surface.
 * @param layout Target text layout object that contains rendered image
 * @param x X coordinate of output image
 * @param y Y coordinate of output image
 * @param image Pointer to output image
 * @param img_w width of output image
 * @param img_h height of output image
 */
void text_layout_copy_to_yuv422(const text_layout layout, const int x, const int y, unsigned char* image, const int img_w, const int img_h);

/**
 * @brief Write data in internal buffer to existed yuv422p surface.
 * @param layout Target text layout object that contains rendered image
 * @param x X coordinate of output image
 * @param y Y coordinate of output image
 * @param image Pointer to output image
 * @param img_w width of output image
 * @param img_h height of output image
 */
void text_layout_copy_to_yuv422p(const text_layout layout, const int x, const int y, unsigned char* image, const int img_w, const int img_h);


#endif /* FONT_H_ */
