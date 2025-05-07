#pragma once

#include <mutex>
#include <iostream>
#include <hidapi.h>
#include <stdio.h>
#include <wchar.h>
#include <cstdint>

#ifdef _WIN32
	#include <Windows.h>
	#include <timeapi.h>
	#pragma comment(lib, "winmm.lib")
#endif

#include "dataStructures.h"
#include "crc.h"
#include "originalLibScePadStructs.h"

#define SCE_OK 0

#define DEVICE_COUNT 3
#define MAX_CONTROLLER_COUNT 4
#define VENDOR_ID 0x54c
#define DUALSENSE_DEVICE_ID 0x0ce6
#define DUALSHOCK4_DEVICE_ID 0x05c4
#define DUALSHOCK4V2_DEVICE_ID 0x09cc
#define DUALSHOCK4 0
#define DUALSENSE 1

struct device {
	int Vendor = 0;
	int Device = 0;
};

struct deviceList {
	device devices[DEVICE_COUNT];

	deviceList() {
		devices[0] = { VENDOR_ID, DUALSENSE_DEVICE_ID };
		devices[1] = { VENDOR_ID, DUALSHOCK4_DEVICE_ID };
		devices[2] = { VENDOR_ID, DUALSHOCK4V2_DEVICE_ID };
	}
};

deviceList g_deviceList = {};

