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
#include "image_functions.h"
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
    char *newname = check_filename(prefix, "fits");
    if(newname && writefits(newname, convertedImage))
        VDBG("FITS file saved into %s", newname);
}

// manage some menu/shortcut events
static void winevt_manage(windowData *win, fc2Image *convertedImage){
    if(win->winevt & WINEVT_SAVEIMAGE){ // save image
        VDBG("Try to make screenshot");
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
        fc2StopCapture(context);
        fc2DestroyContext(context);
        signals(ret);
    }
    if(!G.showimage && !outfprefix){ // not display image & not save it?
        ERRX("You should point file name or option `display image`");
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
            if(mainwin->winevt & WINEVT_GETIMAGE){
                mainwin->winevt &= ~WINEVT_GETIMAGE;
                if(!GrabImage(context, &convertedImage))
                    change_displayed_image(mainwin, &convertedImage);
            }
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
