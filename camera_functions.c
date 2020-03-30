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

#include <stdio.h>
#include <usefull_macros.h>

#include "aux.h"
#include "cmdlnopts.h"
#include "camera_functions.h"

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

// return property name
const char *getPropName(fc2PropertyType t){
    if(t < FC2_BRIGHTNESS || t > FC2_UNSPECIFIED_PROPERTY_TYPE) return NULL;
    return propnames[t];
}

static void prbl(char *s, BOOL prop){
    printf("\t%s = ", s);
    if(prop) green("true");
    else red("false");
    printf("\n");
}

fc2Error getproperty(fc2Context context, fc2PropertyType t){
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
    return FC2_ERROR_OK;
}

fc2Error getpropertyInfo(fc2Context context, fc2PropertyType t){
    fc2PropertyInfo i;
    i.type = t;
    FC2FNW(fc2GetPropertyInfo, context, &i);
    if(!i.present) return FC2_ERROR_NOT_FOUND;
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


/**
 * @brief setfloat - set absolute property value (float)
 * @param t        - type of property
 * @param context  - initialized context
 * @param f        - new value
 * @return FC2_ERROR_OK if all OK
 */
fc2Error setfloat(fc2PropertyType t, fc2Context context, float f){
    fc2Property prop;
    prop.type = t;
    fc2PropertyInfo i;
    i.type = t;
    FC2FNW(fc2GetProperty, context, &prop);
    FC2FNW(fc2GetPropertyInfo, context, &i);
    if(!prop.present || !i.present) return FC2_ERROR_NOT_FOUND;
    if(prop.autoManualMode){
        if(!i.manualSupported){
            WARNX("Can't set auto-only property");
            return FC2_ERROR_PROPERTY_FAILED;
        }
        prop.autoManualMode = false;
    }
    if(!prop.absControl){
        if(!i.absValSupported){
            WARNX("Can't set non-absolute property to absolute value");
            return FC2_ERROR_PROPERTY_FAILED;
        }
        prop.absControl = true;
    }
    if(!prop.onOff){
        if(!i.onOffSupported){
            WARNX("Can't set property ON");
            return FC2_ERROR_PROPERTY_FAILED;
        }
        prop.onOff = true;
    }
    if(prop.onePush && i.onePushSupported) prop.onePush = false;
    prop.valueA = prop.valueB = 0;
    prop.absValue = f;
    FC2FNW(fc2SetProperty, context, &prop);
    // now check
    FC2FNW(fc2GetProperty, context, &prop);
    if(fabsf(prop.absValue - f) > 0.02f){
        WARNX("Can't set %s! Got %g instead of %g.", propnames[t], prop.absValue, f);
        return FC2_ERROR_FAILED;
    }
    return FC2_ERROR_OK;
}

fc2Error propOnOff(fc2PropertyType t, fc2Context context, BOOL onOff){
    fc2Property prop;
    prop.type = t;
    fc2PropertyInfo i;
    i.type = t;
    FC2FNW(fc2GetPropertyInfo, context, &i);
    FC2FNW(fc2GetProperty, context, &prop);
    if(!prop.present || !i.present) return FC2_ERROR_NOT_FOUND;
    if(prop.onOff == onOff) return FC2_ERROR_OK;
    if(!i.onOffSupported){
        WARNX("Property %s not supported state OFF", propnames[t]);
        return  FC2_ERROR_PROPERTY_FAILED;
    }
    prop.onOff = onOff;
    FC2FNW(fc2SetProperty, context, &prop);
    FC2FNW(fc2GetProperty, context, &prop);
    if(prop.onOff != onOff){
        WARNX("Can't change property %s OnOff state", propnames[t]);
        return FC2_ERROR_FAILED;
    }
    return FC2_ERROR_OK;
}

void PrintCameraInfo(fc2Context context, unsigned int n){
    fc2CameraInfo camInfo;
    fc2Error error = fc2GetCameraInfo(context, &camInfo);
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
    if(verbose_level >= VERB_MESG){
        for(fc2PropertyType t = FC2_BRIGHTNESS; t < FC2_UNSPECIFIED_PROPERTY_TYPE; ++t){
            getproperty(context, t);
            if(verbose_level >= VERB_DEBUG) getpropertyInfo(context, t);
        }
    }
}
