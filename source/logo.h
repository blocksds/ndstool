#pragma once
#include "raster.h"

int LogoConvert(unsigned char *srcp, unsigned char *dstp, unsigned char white);
bool LogoConvert(RasterImage &img, unsigned char *dstp);
