// SPDX-FileNotice: Modified from the original version by the BlocksDS project, starting from 2023.

#pragma once

void ExtractFiles(char *ndsfilename, const char *filerootdir);
void ExtractOverlayFiles();
void Extract(char *outfilename, bool indirect_offset, unsigned int offset, bool indirect_size, unsigned size, bool with_footer = false);
