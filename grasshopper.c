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

#include <C/FlyCapture2_C.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <usefull_macros.h>

#include "aux.h"
#include "cmdlnopts.h"

static fc2Error err;
#define FC2FNE(fn, ...) do{if(FC2_ERROR_OK != (err=fn(__VA_ARGS__))){fc2DestroyContext(context); \
    ERRX(#fn "(): %s", fc2ErrorToDescription(err));}}while(0)

#define FC2FNW(fn, ...) do{if(FC2_ERROR_OK != (err=fn(__VA_ARGS__))){fc2DestroyContext(context); \
    WARNX(#fn "(): %s", fc2ErrorToDescription(err)); return err;}}while(0)


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
#if 0
typedef struct _Property
{
    /** Property info type. */
    fc2PropertyType   type;
    /** Flag indicating if the property is present. */
    BOOL present;
    /**
     * Flag controlling absolute mode (real world units)
     * or non-absolute mode (camera internal units).
     */
    BOOL absControl;
    /** Flag controlling one push. */
    BOOL onePush;
    /** Flag controlling on/off. */
    BOOL onOff;
    /** Flag controlling auto. */
    BOOL autoManualMode;
    /**
     * Value A (integer).
     * Used to configure properties in non-absolute mode.
     */
    unsigned int valueA;
    /**
     * Value B (integer). For white balance, value B applies to the blue value and
     * value A applies to the red value.
     */
    unsigned int valueB;
    /**
    * Floating point value.
    * Used to configure properties in absolute mode.
    */
    float absValue;
    /** Reserved for future use. */
    unsigned int reserved[8];

    // For convenience, trigger delay is the same structure
    // used in a separate function along with trigger mode.

} fc2Property, fc2TriggerDelay;
typedef enum _fc2PropertyType
{
    FC2_BRIGHTNESS,
    FC2_AUTO_EXPOSURE,
    FC2_SHARPNESS,
    FC2_WHITE_BALANCE,
    FC2_HUE,
    FC2_SATURATION,
    FC2_GAMMA,
    FC2_IRIS,
    FC2_FOCUS,
    FC2_ZOOM,
    FC2_PAN,
    FC2_TILT,
    FC2_SHUTTER,
    FC2_GAIN,
    FC2_TRIGGER_MODE,
    FC2_TRIGGER_DELAY,
    FC2_FRAME_RATE,
    FC2_TEMPERATURE,
    FC2_UNSPECIFIED_PROPERTY_TYPE,
    FC2_PROPERTY_TYPE_FORCE_32BITS = FULL_32BIT_VALUE

} fc2PropertyType;
#endif

static const char *propnames[] = {
    [FC2_BRIGHTNESS] = "brightness",
    [FC2_AUTO_EXPOSURE] = "auto exposure",
    [FC2_SHARPNESS] = "sharpness",
    [FC2_WHITE_BALANCE] = "white balance",
    [FC2_HUE] = "hue",
    [FC2_SATURATION] = "saturation",
    [FC2_GAMMA] = "gamma",
    [FC2_IRIS] = "iris",
    [FC2_FOCUS] = "focus",
    [FC2_ZOOM] = "zoom",
    [FC2_PAN] = "pan",
    [FC2_TILT] = "tilt",
    [FC2_SHUTTER] = "shutter",
    [FC2_GAIN] = "gain",
    [FC2_TRIGGER_MODE] = "trigger mode",
    [FC2_TRIGGER_DELAY] = "trigger delay",
    [FC2_FRAME_RATE] = "frame rate",
    [FC2_TEMPERATURE] = "temperature",
    [FC2_UNSPECIFIED_PROPERTY_TYPE] = "unspecified"
};

static void prbl(char *s, BOOL prop){
    printf("\t%s = ", s);
    if(prop) green("true");
    else red("false");
    printf("\n");
}

static fc2Error getproperty(fc2Context context, fc2PropertyType t){
    fc2Property prop;
    prop.type = t;
    FC2FNW(fc2GetProperty, context, &prop);
    if(!prop.present) return FC2_ERROR_NOT_FOUND;
    if(t <= FC2_UNSPECIFIED_PROPERTY_TYPE) green("\nProperty \"%s\":\n", propnames[t]);
    prbl("absControl", prop.absControl); // 1 - world units, 0 - camera units
    prbl("onePush", prop.onePush); // "one push"
    prbl("onOff", prop.onOff);
    prbl("autoManualMode", prop.autoManualMode); // 1 - auto, 0 - manual
    printf("\tvalueA = %u\n", prop.valueA); // values in non-absolute mode
    printf("\tvalueB = %u\n", prop.valueB);
    printf("\tabsValue = %g\n", prop.absValue); // value in absolute mode
    fc2PropertyInfo i;
    i.type = t;
    FC2FNW(fc2GetPropertyInfo, context, &i);
    if(!i.present) return FC2_ERROR_OK;
    green("Property Info:\n");
    prbl("autoSupported", i.autoSupported); // can be auto
    prbl("manualSupported", i.manualSupported); // can be manual
    prbl("onOffSupported", i.onOffSupported); // can be on/off
    prbl("onePushSupported", i.onePushSupported); // can be "one push"
    prbl("absValSupported", i.absValSupported); // can be absolute
    prbl("readOutSupported", i.readOutSupported); // could be read out
    printf("\tmin = %u\n", i.min);
    printf("\tmax = %u\n", i.max);
    printf("\tabsMin = %g\n", i.absMin);
    printf("\tabsMax = %g\n", i.absMax);
    printf("\tpUnits = %s\n", i.pUnits);
    printf("\tpUnitAbbr = %s\n", i.pUnitAbbr);
    return FC2_ERROR_OK;
}

static fc2Error setexp(fc2Context context, float e){
    fc2Property prop;
    prop.type = FC2_SHUTTER;
    prop.autoManualMode = false;
    prop.absValue = e;
    FC2FNW(fc2SetProperty, context, &prop);
    // now check
    FC2FNW(fc2GetProperty, context, &prop);
    if(fabs(prop.absValue - e) > 0.0001){
        WARNX("Can't set exposure! Got %g instead of %g.", prop.absValue, e);
        return FC2_ERROR_FAILED;
    }
    return FC2_ERROR_OK;
}


static void PrintCameraInfo(fc2Context context, int n){
    fc2Error error;
    fc2CameraInfo camInfo;
    error = fc2GetCameraInfo(context, &camInfo);
    if(error != FC2_ERROR_OK){
        WARNX("fc2GetCameraInfo(): %s", fc2ErrorToDescription(error));
        return;
    }
    printf("\n\n");
    green("*** CAMERA %d INFORMATION ***\n", n);
    printf("Serial number - %u\n"
           "Camera model - %s\n"
           "Camera vendor - %s\n"
           "Sensor - %s\n"
           "Resolution - %s\n"
           "Firmware version - %s\n"
           "Firmware build time - %s\n\n",
           camInfo.serialNumber,
           camInfo.modelName,
           camInfo.vendorName,
           camInfo.sensorInfo,
           camInfo.sensorResolution,
           camInfo.firmwareVersion,
           camInfo.firmwareBuildTime);
    if(verbose_level >= VERB_DEBUG){
    for(fc2PropertyType t = FC2_BRIGHTNESS; t < FC2_UNSPECIFIED_PROPERTY_TYPE; ++t)
        getproperty(context, t);
    }
}

static void SetTimeStamping(fc2Context context, BOOL enableTimeStamp)
{
    fc2Error error;
    fc2EmbeddedImageInfo embeddedInfo;

    error = fc2GetEmbeddedImageInfo(context, &embeddedInfo);
    if (error != FC2_ERROR_OK)
    {
        printf("Error in fc2GetEmbeddedImageInfo: %s\n", fc2ErrorToDescription(error));
    }

    if (embeddedInfo.timestamp.available != 0)
    {
        embeddedInfo.timestamp.onOff = enableTimeStamp;
    }

    error = fc2SetEmbeddedImageInfo(context, &embeddedInfo);
    if (error != FC2_ERROR_OK)
    {
        printf("Error in fc2SetEmbeddedImageInfo: %s\n", fc2ErrorToDescription(error));
    }
}

static int GrabImages(fc2Context context, int numImagesToGrab)
{
    fc2Error error;
    fc2Image rawImage;
    fc2Image convertedImage;
    fc2TimeStamp prevTimestamp = {0};
    int i;

    error = fc2CreateImage(&rawImage);
    if (error != FC2_ERROR_OK)
    {
        printf("Error in fc2CreateImage: %s\n", fc2ErrorToDescription(error));
        return -1;
    }

    error = fc2CreateImage(&convertedImage);
    if (error != FC2_ERROR_OK)
    {
        printf("Error in fc2CreateImage: %s\n", fc2ErrorToDescription(error));
        return -1;
    }

    // If externally allocated memory is to be used for the converted image,
    // simply assigning the pData member of the fc2Image structure is
    // insufficient. fc2SetImageData() should be called in order to populate
    // the fc2Image structure correctly. This can be done at this point,
    // assuming that the memory has already been allocated.

    for (i = 0; i < numImagesToGrab; i++)
    {
        // Retrieve the image
        error = fc2RetrieveBuffer(context, &rawImage);
        if (error != FC2_ERROR_OK)
        {
            printf("Error in retrieveBuffer: %s\n", fc2ErrorToDescription(error));
            return -1;
        }
        else
        {
            // Get and print out the time stamp
            fc2TimeStamp ts = fc2GetImageTimeStamp(&rawImage);
            int diff = (ts.cycleSeconds - prevTimestamp.cycleSeconds) * 8000 +
                       (ts.cycleCount - prevTimestamp.cycleCount);
            prevTimestamp = ts;
            printf("timestamp [%d %d] - %d\n",
                   ts.cycleSeconds,
                   ts.cycleCount,
                   diff);
        }
    }

    if (error == FC2_ERROR_OK)
    {
        // Convert the final image to RGB
        error = fc2ConvertImageTo(FC2_PIXEL_FORMAT_MONO8, &rawImage, &convertedImage);
        if (error != FC2_ERROR_OK)
        {
            printf("Error in fc2ConvertImageTo: %s\n", fc2ErrorToDescription(error));
            return -1;
        }

        // Save it to PNG
        printf("Saving the last image to fc2TestImage.png \n");
        error = fc2SaveImage(&convertedImage, "fc2TestImage.png", FC2_PNG);
        if (error != FC2_ERROR_OK)
        {
            printf("Error in fc2SaveImage: %s\n", fc2ErrorToDescription(error));
            printf("Please check write permissions.\n");
            return -1;
        }
    }

    error = fc2DestroyImage(&rawImage);
    if (error != FC2_ERROR_OK)
    {
        printf("Error in fc2DestroyImage: %s\n", fc2ErrorToDescription(error));
        return -1;
    }

    error = fc2DestroyImage(&convertedImage);
    if (error != FC2_ERROR_OK)
    {
        printf("Error in fc2DestroyImage: %s\n", fc2ErrorToDescription(error));
        return -1;
    }

    return 0;
}

int main(int argc, char **argv){
    int ret = 0;
    initial_setup();
    char *self = strdup(argv[0]);
    parse_args(argc, argv);
    check4running(self, G.pidfile);
    FREE(self);

    signal(SIGTERM, signals); // kill (-15) - quit
    signal(SIGHUP, SIG_IGN);  // hup - ignore
    signal(SIGINT, signals);  // ctrl+C - quit
    signal(SIGQUIT, signals); // ctrl+\ - quit
    signal(SIGTSTP, SIG_IGN); // ignore ctrl+Z

    setup_con();
    fc2Context context;
    fc2PGRGuid guid;
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
        for(int i = 0; i < numCameras; ++i){
            FC2FNE(fc2GetCameraFromIndex, context, i, &guid);
            FC2FNE(fc2Connect, context, &guid);
            PrintCameraInfo(context, i);
        }
    }
    FC2FNE(fc2GetCameraFromIndex, context, G.camno, &guid);
    FC2FNE(fc2Connect, context, &guid);
    if(verbose_level >= VERB_MESG && numCameras > 1) PrintCameraInfo(context, G.camno);
    if(isnan(G.exptime)){ // no expose time -> return
        goto destr;
    }
    if(FC2_ERROR_OK != setexp(context, G.exptime)){
        ret = 1;
        goto destr;
    }
    VMESG("Set exposition to %gms", G.exptime);

    SetTimeStamping(context, TRUE);

    err = fc2StartCapture(context);
    if (err != FC2_ERROR_OK)
    {
        fc2DestroyContext(context);
        printf("Error in fc2StartCapture: %s\n", fc2ErrorToDescription(err));
        signals(12);
    }

    if (GrabImages(context, 3) != 0)
    {
        fc2DestroyContext(context);
        signals(12);
    }

    err = fc2StopCapture(context);
    if (err != FC2_ERROR_OK)
    {
        fc2DestroyContext(context);
        printf("Error in fc2StopCapture: %s\n", fc2ErrorToDescription(err));
        signals(12);
    }

destr:
    fc2DestroyContext(context);
    signals(ret);
    return ret;
}
