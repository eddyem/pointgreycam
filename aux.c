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
#include <stdarg.h>

#include "aux.h"
#include "cmdlnopts.h"

int verbose(verblevel levl, const char *fmt, ...){
    if((unsigned)verbose_level < levl) return 0;
    va_list ar; int i;
    va_start(ar, fmt);
    i = vprintf(fmt, ar);
    va_end(ar);
    printf("\n");
    fflush(stdout);
    return i;
}
