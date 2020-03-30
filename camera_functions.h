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
#ifndef CAMERA_FUNCTIONS__
#define CAMERA_FUNCTIONS__

#include <C/FlyCapture2_C.h>
#include <math.h>

#define FC2FNE(fn, c, ...) do{fc2Error err = FC2_ERROR_OK; if(FC2_ERROR_OK != (err=fn(c __VA_OPT__(,) __VA_ARGS__))){ \
    fc2DestroyContext(c); ERRX(#fn "(): %s", fc2ErrorToDescription(err));}}while(0)

#define FC2FNW(fn, c, ...) do{fc2Error err = FC2_ERROR_OK; if(FC2_ERROR_OK != (err=fn(c __VA_OPT__(,) __VA_ARGS__))){ \
    WARNX(#fn "(): %s", fc2ErrorToDescription(err)); return err;}}while(0)

void PrintCameraInfo(fc2Context context, unsigned int n);
const char *getPropName(fc2PropertyType t);
fc2Error getproperty(fc2Context context, fc2PropertyType t);
fc2Error getpropertyInfo(fc2Context context, fc2PropertyType t);
fc2Error setfloat(fc2PropertyType t, fc2Context context, float f);
fc2Error propOnOff(fc2PropertyType t, fc2Context context, BOOL onOff);
#define autoExpOff(c)           propOnOff(FC2_AUTO_EXPOSURE, c, false)
#define whiteBalOff(c)          propOnOff(FC2_WHITE_BALANCE, c, false)
#define gammaOff(c)             propOnOff(FC2_GAMMA, c, false)
#define trigModeOff(c)          propOnOff(FC2_TRIGGER_MODE, c, false)
#define trigDelayOff(c)         propOnOff(FC2_TRIGGER_DELAY, c, false)
#define frameRateOff(c)         propOnOff(FC2_FRAME_RATE, c, false)
// +set: saturation, hue, sharpness
#define setbrightness(c, b)     setfloat(FC2_BRIGHTNESS, c, b)
#define setexp(c, e)            setfloat(FC2_SHUTTER, c, e)
#define setgain(c, g)           setfloat(FC2_GAIN, c, g)

#endif // CAMERA_FUNCTIONS__
