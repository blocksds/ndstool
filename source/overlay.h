// SPDX-FileNotice: Modified from the original version by the BlocksDS project, starting from 2023.

#pragma once

#pragma pack(1)

struct OverlayEntry
{
	unsigned_int id;
	unsigned_int ram_address;
	unsigned_int ram_size;
	unsigned_int bss_size;
	unsigned_int sinit_init;
	unsigned_int sinit_init_end;
	unsigned_int file_id;
	unsigned_int reserved;
};

#pragma pack()

#define OVERLAY_FMT		"overlay_%04u.bin"
