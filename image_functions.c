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

#include <fitsio.h>
#include <pthread.h>
#include <stdio.h>
#include <usefull_macros.h>

#include "camera_functions.h"
#include "cmdlnopts.h"
#include "image_functions.h"

int GrabImage(fc2Context context, fc2Image *convertedImage){
    fc2Error error;
    fc2Image rawImage;
    // start capture
    FC2FNE(fc2StartCapture, context);
    error = fc2CreateImage(&rawImage);
    if(error != FC2_ERROR_OK){
        printf("Error in fc2CreateImage: %s\n", fc2ErrorToDescription(error));
        return -1;
    }
    // Retrieve the image
    error = fc2RetrieveBuffer(context, &rawImage);
    if (error != FC2_ERROR_OK){
        printf("Error in retrieveBuffer: %s\n", fc2ErrorToDescription(error));
        return -1;
    }
    // Convert image to gray
    windowData *win = getWin();
    if(win) pthread_mutex_lock(&win->mutex);
    error = fc2ConvertImageTo(FC2_PIXEL_FORMAT_MONO8, &rawImage, convertedImage);
    if(win) pthread_mutex_unlock(&win->mutex);
    if(error != FC2_ERROR_OK){
        printf("Error in fc2ConvertImageTo: %s\n", fc2ErrorToDescription(error));
        return -1;
    }
    fc2StopCapture(context);
    fc2DestroyImage(&rawImage);
    return 0;
}


/**
 * Convert gray (unsigned short) into RGB components (GLubyte)
 * @argument L   - gray level (0..1)
 * @argument rgb - rgb array (GLubyte [3])
 */
void gray2rgb(double gray, GLubyte *rgb){
    int i = gray * 4.;
    double x = (gray - (double)i * .25) * 4.;
    GLubyte r = 0, g = 0, b = 0;
    //r = g = b = (gray < 1) ? gray * 256 : 255;
    switch(i){
        case 0:
            g = (GLubyte)(255. * x);
            b = 255;
        break;
        case 1:
            g = 255;
            b = (GLubyte)(255. * (1. - x));
        break;
        case 2:
            r = (GLubyte)(255. * x);
            g = 255;
        break;
        case 3:
            r = 255;
            g = (GLubyte)(255. * (1. - x));
        break;
        default:
            r = 255;
    }
    *rgb++ = r;
    *rgb++ = g;
    *rgb   = b;
}

static colorfn_type ft = COLORFN_LINEAR;

// all colorfun's should get argument in [0, 1] and return in [0, 1]
static double linfun(double arg){ return arg; } // bung for PREVIEW_LINEAR
static double logfun(double arg){ return log(1.+arg) / 0.6931472; } // for PREVIEW_LOG [log_2(x+1)]
static double (*colorfun)(double) = linfun; // default function to convert color

colorfn_type get_colorfun(){return ft;}

void change_colorfun(colorfn_type f){
    DBG("New colorfn: %d", f);
    switch (f){
        case COLORFN_LOG:
            colorfun = logfun;
            ft = COLORFN_LOG;
        break;
        case COLORFN_SQRT:
            colorfun = sqrt;
            ft = COLORFN_SQRT;
        break;
        default: // linear
            colorfun = linfun;
            ft = COLORFN_LINEAR;
    }
}

// cycle switch between palettes
void roll_colorfun(){
    colorfn_type t = ++ft;
    if(t == COLORFN_MAX) t = COLORFN_LINEAR;
    change_colorfun(t);
}

/**
 * @brief equalize - hystogram equalization
 * @param ori (io) - input/output data
 * @param w,h,s    - image width, height and stride
 * @return data allocated here
 */
static uint8_t *equalize(uint8_t *ori, int w, int h, int s){
    uint8_t *retn = MALLOC(uint8_t, s*h);

    double orig_hysto[256] = {0.}; // original hystogram
    uint8_t eq_levls[256] = {0};   // levels to convert: newpix = eq_levls[oldpix]
    for(int y = 0; y < h; ++y){
        uint8_t *ptr = &ori[y * s];
        for(int x = 0; x < w; ++x)
            ++orig_hysto[*ptr++];
    }
    double part = (double)(w*h - 1) / 256., N = 0.;
    for(size_t i = 0; i < 256; ++i){
        N += orig_hysto[i];
        eq_levls[i] = (uint8_t)(N/part);
    }

    for(int y = 0; y < h; ++y){
        uint8_t *iptr = &ori[y * s];
        uint8_t *optr = &retn[y * s];
        for(int x = 0; x < w; ++x){
            //*optr++ = *iptr++;
            *optr++ = eq_levls[*iptr++];
        }
    }
    return retn;
}

void change_displayed_image(windowData *win, fc2Image *convertedImage){
    if(!win || !win->image) return;
    rawimage *im = win->image;
    DBG("imh=%d, imw=%d, ch=%u, cw=%u", im->h, im->w, convertedImage->rows, convertedImage->cols);
    /*
    if(!im->rawdata || im->h != (int)convertedImage->rows || im->w != (int)convertedImage->cols){
        DBG("[re]allocate im->rawdata");
        FREE(im->rawdata);
        im->h = (int)convertedImage->rows;
        im->w = (int)convertedImage->cols;
        im->rawdata = MALLOC(GLubyte, 3 * im->h * im->w);
        if(!im->rawdata) ERR("Can't allocate memory");
    }
    printf("image data:\n");
    printf("rows=%u, cols=%u, stride=%u, datasize=%u, recds=%u\n", convertedImage->rows,
       convertedImage->cols, convertedImage->stride, convertedImage->dataSize, convertedImage->receivedDataSize);
    */
    pthread_mutex_lock(&win->mutex);
    int  x, y, w = convertedImage->cols, h = convertedImage->rows, s = convertedImage->stride;
    uint8_t *newima = equalize(convertedImage->pData, w, h, s);
    /*
    double avr, wd, max, min;
    avr = max = min = (double)*convertedImage->pData;
    for(y = 0; y < h; ++y){
        unsigned char *ptr = &convertedImage->pData[y*convertedImage->stride];
        for(x = 0; x < w; ++x, ++ptr){
            double pix = (double) *ptr;
            if(pix > max) max = pix;
            if(pix < min) min = pix;
            avr += pix;
        }
    }
    avr /= (double)(w*h);
    wd = max - min;
    if(wd > DBL_EPSILON) avr = (avr - min) / wd;	// normal average by preview
    if(avr < 0.6) wd *= avr + 0.2;
    if(wd < DBL_EPSILON) wd = 1.;
    DBG("stat: sz=(%dx%d) avr=%g wd=%g max=%g min=%g", w,h,avr, wd, max, min);
    */
    GLubyte *dst = im->rawdata;
    for(y = 0; y < h; y++){
        //unsigned char *ptr = &convertedImage->pData[y * s];
        unsigned char *ptr = &newima[y * s];
        for(x = 0; x < w; x++, dst += 3, ++ptr){
            //gray2rgb(colorfun((*ptr - min) / wd), dst);
            gray2rgb(colorfun(*ptr / 256.), dst);
        }
    }
    FREE(newima);
    win->image->changed = 1;
    pthread_mutex_unlock(&win->mutex);
}

#define TRYFITS(f, ...)                     \
do{ int status = 0;                         \
    f(__VA_ARGS__, &status);                \
    if (status){                            \
        fits_report_error(stderr, status);  \
        return 1;}                         \
}while(0)
#define WRITEKEY(...)                           \
do{ int status = 0;                             \
    fits_write_key(__VA_ARGS__, &status);       \
    if(status) fits_report_error(stderr, status);\
}while(0)

/**
 * @brief writefits - save FITS-file
 * @param filename  - full filename of output file
 * @param convertedImage - image to save
 * @return 0 if all OK
 */
int writefits(char *filename, fc2Image *convertedImage){
    int w = convertedImage->cols, s = convertedImage->stride, h = convertedImage->rows;
    long naxes[2] = {w, h}; //, startTime;
    double tmp = 0.0;
    //struct tm *tm_starttime;
    char buf[80];
    time_t savetime = time(NULL);
    fitsfile *fp;
    TRYFITS(fits_create_file, &fp, filename);
    TRYFITS(fits_create_img, fp, BYTE_IMG, 2, naxes);
    // FILE / Input file original name
    WRITEKEY(fp, TSTRING, "FILE", filename, "Input file original name");
    // ORIGIN / organization responsible for the data
    WRITEKEY(fp, TSTRING, "ORIGIN", "SAO RAS", "organization responsible for the data");
    // OBSERVAT / Observatory name
    WRITEKEY(fp, TSTRING, "OBSERVAT", "Special Astrophysical Observatory, Russia", "Observatory name");
    fc2Context context;
    if(FC2_ERROR_OK == fc2CreateContext(&context)){
        fc2CameraInfo camInfo;
        fc2Error error = fc2GetCameraInfo(context, &camInfo);
        if(error == FC2_ERROR_OK){
            // INSTRUME / Instrument
            WRITEKEY(fp, TSTRING, "INSTRUME", camInfo.modelName, "Instrument");
            // DETECTOR / detector
            WRITEKEY(fp, TSTRING, "DETECTOR", camInfo.sensorInfo, "Detector model");
        }
        fc2DestroyContext(context);
    }
    double pixX, pixY = pixX = 6.45;
    snprintf(buf, 80, "%g x %g", pixX, pixY);
    // PXSIZE / pixel size
    WRITEKEY(fp, TSTRING, "PXSIZE", buf, "Pixel size (um)");
    WRITEKEY(fp, TDOUBLE, "XPIXSZ", &pixX, "Pixel Size X (um)");
    WRITEKEY(fp, TDOUBLE, "YPIXSZ", &pixY, "Pixel Size Y (um)");
/*
    if(G->exptime < 2.*DBL_EPSILON) sprintf(buf, "bias");
    else if(G->dark) sprintf(buf, "dark");
    else if(G->objtype) strncpy(buf, G->objtype, 80);
    else sprintf(buf, "object");
    // IMAGETYP / object, flat, dark, bias, scan, eta, neon, push
    WRITEKEY(fp, TSTRING, "IMAGETYP", buf, "Image type");
*/
    /*
    // DATAMAX, DATAMIN / Max,min pixel value
    int itmp = 0;
    WRITEKEY(fp, TINT, "DATAMIN", &itmp, "Min pixel value");
    //itmp = G->fast ? 255 : 65535;
    itmp = 65535;
    WRITEKEY(fp, TINT, "DATAMAX", &itmp, "Max pixel value");
    WRITEKEY(fp, TUSHORT, "STATMAX", &max, "Max data value");
    WRITEKEY(fp, TUSHORT, "STATMIN", &min, "Min data value");
    WRITEKEY(fp, TDOUBLE, "STATAVR", &avr, "Average data value");
    WRITEKEY(fp, TDOUBLE, "STATSTD", &std, "Std. of data value");
    WRITEKEY(fp, TDOUBLE, "TEMP0", &G->temperature, "Camera temperature at exp. start (degr C)");
    */
    tmp = (double)G.exptime / 1000.;
    // EXPTIME / actual exposition time (sec)
    WRITEKEY(fp, TDOUBLE, "EXPTIME", &tmp, "Actual exposition time (sec)");
    // DATE / Creation date (YYYY-MM-DDThh:mm:ss, UTC)
    strftime(buf, 80, "%Y-%m-%dT%H:%M:%S", gmtime(&savetime));
    WRITEKEY(fp, TSTRING, "DATE", buf, "Creation date (YYYY-MM-DDThh:mm:ss, UTC)");
/*
    startTime = (long)expStartsAt.tv_sec;
    tm_starttime = localtime(&expStartsAt.tv_sec);
    strftime(buf, 80, "exposition starts at %d/%m/%Y, %H:%M:%S (local)", tm_starttime);
    tmp = startTime + (double)expStartsAt.tv_usec/1e6;
    WRITEKEY(fp, TDOUBLE, "UNIXTIME", &tmp, buf);
    strftime(buf, 80, "%Y/%m/%d", tm_starttime);
    // DATE-OBS / DATE (YYYY/MM/DD) OF OBS.
    WRITEKEY(fp, TSTRING, "DATE-OBS", buf, "DATE OF OBS. (YYYY/MM/DD, local)");
    strftime(buf, 80, "%H:%M:%S", tm_starttime);
    // START / Measurement start time (local) (hh:mm:ss)
    WRITEKEY(fp, TSTRING, "START", buf, "Measurement start time (hh:mm:ss, local)");
    */
    uint8_t *data = MALLOC(uint8_t, w*h);
    // mirror upside down to make right image
    for(int y = 0; y < h; y++){
        memcpy(&data[y * w], &convertedImage->pData[(h-y-1) * s], w);
    }
    int status = 0;
    fits_write_img(fp, TBYTE, 1, w * h, data, &status);
    if(status) fits_report_error(stderr, status);
    FREE(data);
    TRYFITS(fits_close_file, fp);
    return 0;
}

#undef TRYFITS
#undef WRITEKEY
