// SPDX-FileNotice: Modified from the original version by the BlocksDS project, starting from 2023.

#pragma once
#include "raster.h"

int LogoConvert(unsigned char *srcp, unsigned char *dstp, unsigned char white);
bool LogoConvert(RasterImage &img, unsigned char *dstp);
