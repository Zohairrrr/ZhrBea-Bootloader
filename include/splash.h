#ifndef SPLASH_H
#define SPLASH_H

#include <efi.h>
#include "../include/gop.h"

/* Draw the animated splash / logo screen.
 * Blocks for ~1.5 seconds while animating, then returns. */
void splash_draw(void);

/* Draw the static background that stays behind the menu */
void splash_draw_background(void);

#endif