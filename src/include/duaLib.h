#pragma once

#include <mutex>
#include <iostream>
#include <hidapi.h>
#include <stdio.h>
#include <wchar.h>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <atomic>      
#include <thread>       
#include <chrono>      
#include <cstring>    

#ifdef _WIN32
	#include <Windows.h>
	#include <setupapi.h>
	#pragma comment(lib, "setupapi.lib")
	#pragma comment(lib, "hid.lib")
	#pragma comment(lib, "winmm.lib")
	#include <initguid.h>
	#include <devpkey.h>
	#include <hidsdi.h>
#else
	#include <clocale>
	#include <cstdlib>
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
#define UNKNOWN 0
#define DUALSHOCK4 1
#define DUALSENSE 2

struct device {
	uint16_t Vendor = 0;
	uint16_t Device = 0;
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

