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

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <usefull_macros.h>

#include "aux.h"
#include "camera_functions.h"
#include "cmdlnopts.h"
#include "imageview.h"

void signals(int sig){
    if(sig){
        signal(sig, SIG_IGN);
        DBG("Get signal %d, quit.\n", sig);
    }
    putlog("Exit with status %d", sig);
    if(G.pidfile) // remove unnesessary PID file
        unlink(G.pidfile);
    restore_console();
    exit(sig);
}


static int GrabImage(fc2Context context, fc2Image *convertedImage){
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
 * @argument L   - gray level
 * @argument rgb - rgb array (GLubyte [3])
 */
static void gray2rgb(double gray, GLubyte *rgb){
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

// functions for converting grayscale value into colour
typedef enum{
    COLORFN_LINEAR, // linear
    COLORFN_LOG,    // ln
    COLORFN_SQRT,   // sqrt
    COLORFN_MAX     // end of list
} colorfn_type;

static colorfn_type ft = COLORFN_LINEAR;

static double linfun(double arg){ return arg; } // bung for PREVIEW_LINEAR
static double logfun(double arg){ return log(1.+arg); } // for PREVIEW_LOG
static double (*colorfun)(double) = linfun; // default function to convert color

static void change_colorfun(colorfn_type f){
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

static void roll_colorfun(){
    colorfn_type t = ++ft;
    if(t == COLORFN_MAX) t = COLORFN_LINEAR;
    change_colorfun(t);
}

static void change_displayed_image(windowData *win, fc2Image *convertedImage){
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
    int  x, y, w = convertedImage->cols, h = convertedImage->rows;
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
    GLubyte *dst = im->rawdata;
    for(y = 0; y < h; y++){
        unsigned char *ptr = &convertedImage->pData[y*convertedImage->stride];
        for(x = 0; x < w; x++, dst += 3, ++ptr){
            gray2rgb(colorfun((*ptr - min) / wd), dst);
        }
    }
    win->image->changed = 1;
    pthread_mutex_unlock(&win->mutex);
}

static void savePng(fc2Image *convertedImage, char *name){
    VDBG("Save the image data into %s", name);
    fc2Error error = fc2SaveImage(convertedImage, name, FC2_PNG);
    if(error != FC2_ERROR_OK){
        fprintf(stderr, "Error in fc2SaveImage: %s\n", fc2ErrorToDescription(error));
    }
}

static void saveImages(fc2Image *convertedImage, char *prefix){
    if(G.save_png){
        char *newname = check_filename(prefix, "png");
        if(newname) savePng(convertedImage, newname);
    }
    // and save FITS here
}

// manage some menu/shortcut events
static void winevt_manage(windowData *win, fc2Image *convertedImage){
    if(win->winevt & WINEVT_SAVEIMAGE){ // save image
        DBG("Try to make screenshot");
        saveImages(convertedImage, "ScreenShot");
        win->winevt &= ~WINEVT_SAVEIMAGE;
    }
    if(win->winevt & WINEVT_ROLLCOLORFUN){
        roll_colorfun();
        win->winevt &= ~WINEVT_ROLLCOLORFUN;
        change_displayed_image(win, convertedImage);
    }
}

// main thread to deal with image
void* image_thread(_U_ void *data){
	FNAME();
    fc2Image *img = (fc2Image*) data;
	while(1){
        windowData *win = getWin();
        if(!win) pthread_exit(NULL);
		if(win->killthread){
			DBG("got killthread");
			pthread_exit(NULL);
		}
        if(win->winevt) winevt_manage(win, img);
		usleep(10000);
	}
}

int main(int argc, char **argv){
    int ret = 0;
    initial_setup();
    char *self = strdup(argv[0]);
    parse_args(argc, argv);
    char *outfprefix = NULL;
    if(G.rest_pars_num){
        if(G.rest_pars_num != 1){
            WARNX("You should point only one free argument - filename prefix");
            signals(1);
        }else outfprefix = G.rest_pars[0];
    }
    check4running(self, G.pidfile);
    FREE(self);
    signal(SIGTERM, signals); // kill (-15) - quit
    signal(SIGHUP, SIG_IGN);  // hup - ignore
    signal(SIGINT, signals);  // ctrl+C - quit
    signal(SIGQUIT, signals); // ctrl+\ - quit
    signal(SIGTSTP, SIG_IGN); // ignore ctrl+Z

    setup_con();

    windowData *mainwin = NULL;

    fc2Context context;
    fc2PGRGuid guid;
    fc2Error err = FC2_ERROR_OK;
    unsigned int numCameras = 0;

    if(FC2_ERROR_OK != (err = fc2CreateContext(&context))){
        ERRX("fc2CreateContext(): %s", fc2ErrorToDescription(err));
    }

    FC2FNE(fc2GetNumOfCameras, context, &numCameras);

    if(numCameras == 0){
        fc2DestroyContext(context);
        ERRX("No cameras detected!");
    }

    VMESG("Found %d camera[s]", numCameras);
    if(verbose_level >= VERB_MESG){
        for(unsigned int i = 0; i < numCameras; ++i){
            FC2FNE(fc2GetCameraFromIndex, context, i, &guid);
            FC2FNE(fc2Connect, context, &guid);
            PrintCameraInfo(context, i);
        }
    }
    FC2FNE(fc2GetCameraFromIndex, context, G.camno, &guid);
    FC2FNE(fc2Connect, context, &guid);
    if(verbose_level >= VERB_MESG && numCameras > 1) PrintCameraInfo(context, G.camno);
    if(isnan(G.exptime)){ // no expose time -> return
        printf("No exposure parameters given -> exit\n");
        goto destr;
    }
    // turn off all shit
    autoExpOff(context);
    whiteBalOff(context);
    gammaOff(context);
    trigModeOff(context);
    trigDelayOff(context);
    frameRateOff(context);
    if(FC2_ERROR_OK != setexp(context, G.exptime)){
        ret = 1;
        goto destr;
    }
    VMESG("Set exposition to %gms", G.exptime);
    if(!isnan(G.gain)){
        if(FC2_ERROR_OK != setgain(context, G.gain)){
            ret = 1;
            goto destr;
        }
        VMESG("Set gain value to %gdB", G.gain);
    }

    if(G.showimage){
        imageview_init();
    }
    // main cycle
    fc2Image convertedImage;
    FC2FNE(fc2CreateImage, &convertedImage);
    int N = 0;
    bool start = TRUE;
    while(1){
        if(GrabImage(context, &convertedImage)){
            fc2DestroyContext(context);
            WARNX("GrabImages()");
            signals(12);
        }
        VMESG("\nGrabbed image #%d", ++N);
        if(outfprefix){
            saveImages(&convertedImage, outfprefix);
        }
        if(G.showimage){
            if(!mainwin && start){
                DBG("Create window @ start");
                mainwin = createGLwin("Sample window", convertedImage.cols, convertedImage.rows, NULL);
                start = FALSE;
                if(!mainwin){
                    WARNX("Can't open OpenGL window, image preview will be inaccessible");
                }else
                    pthread_create(&mainwin->thread, NULL, &image_thread, (void*)&convertedImage); //(void*)mainwin);
            }
            if((mainwin = getWin())){
                DBG("change image");
                if(mainwin->killthread) goto destr;
                change_displayed_image(mainwin, &convertedImage);
                while((mainwin = getWin())){ // test paused state & grabbing custom frames
                    if((mainwin->winevt & WINEVT_PAUSE) == 0) break;
                    if(mainwin->winevt & WINEVT_GETIMAGE){
                        mainwin->winevt &= ~WINEVT_GETIMAGE;
                        if(!GrabImage(context, &convertedImage))
                            change_displayed_image(mainwin, &convertedImage);
                    }
                    usleep(10000);
                }
            }else break;
        }
        if(--G.nimages <= 0) break;
    }
    if((mainwin = getWin())) mainwin->winevt |= WINEVT_PAUSE;
destr:   
    if(G.showimage){
        while((mainwin = getWin())){
            if(mainwin->killthread) break;
        }
        DBG("Close window");
        clear_GL_context();
    }
    FC2FNE(fc2DestroyImage, &convertedImage);
    fc2StopCapture(context);
    fc2DestroyContext(context);
    signals(ret);
    return ret;
}
