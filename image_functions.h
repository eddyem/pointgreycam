/*
 * This file is part of the grasshopper project.
 * Copyright 2020 Edward V. Emelianov <edward.emelianoff@gmail.com>.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#ifndef IMAGE_FUNCTIONS__
#define IMAGE_FUNCTIONS__

#include <C/FlyCapture2_C.h>
#include <GL/glut.h>
#include "imageview.h"

// functions for converting grayscale value into colour
typedef enum{
    COLORFN_LINEAR, // linear
    COLORFN_LOG,    // ln
    COLORFN_SQRT,   // sqrt
    COLORFN_MAX     // end of list
} colorfn_type;

int GrabImage(fc2Context context, fc2Image *convertedImage);
void change_displayed_image(windowData *win, fc2Image *convertedImage);

void gray2rgb(double gray, GLubyte *rgb);
colorfn_type get_colorfun();
void change_colorfun(colorfn_type f);
void roll_colorfun();

int writefits(char *filename, fc2Image *convertedImage);

#endif // IMAGE_FUNCTIONS__
