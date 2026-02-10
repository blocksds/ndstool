// SPDX-FileNotice: Modified from the original version by the BlocksDS project, starting from 2023.

#pragma once
#include "ndstree.h"

void Create();
void Sha1Hmac(u8 output[20], FILE* f, unsigned int pos, unsigned int size);
