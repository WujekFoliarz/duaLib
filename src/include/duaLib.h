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
#include "triggerFactory.h"

#ifndef _SCE_PAD_TRIGGER_EFFECT_H
#define _SCE_PAD_TRIGGER_EFFECT_H

#define SCE_PAD_TRIGGER_EFFECT_TRIGGER_MASK_L2			0x01
#define SCE_PAD_TRIGGER_EFFECT_TRIGGER_MASK_R2			0x02

#define SCE_PAD_TRIGGER_EFFECT_PARAM_INDEX_FOR_L2		0
#define SCE_PAD_TRIGGER_EFFECT_PARAM_INDEX_FOR_R2		1

#define SCE_PAD_TRIGGER_EFFECT_TRIGGER_NUM				2

/* Definition of control point num */
#define SCE_PAD_TRIGGER_EFFECT_CONTROL_POINT_NUM		10

typedef enum ScePadTriggerEffectMode {
	SCE_PAD_TRIGGER_EFFECT_MODE_OFF,
	SCE_PAD_TRIGGER_EFFECT_MODE_FEEDBACK,
	SCE_PAD_TRIGGER_EFFECT_MODE_WEAPON,
	SCE_PAD_TRIGGER_EFFECT_MODE_VIBRATION,
	SCE_PAD_TRIGGER_EFFECT_MODE_MULTIPLE_POSITION_FEEDBACK,
	SCE_PAD_TRIGGER_EFFECT_MODE_SLOPE_FEEDBACK,
	SCE_PAD_TRIGGER_EFFECT_MODE_MULTIPLE_POSITION_VIBRATION,
} ScePadTriggerEffectMode;

/**
 *E
 *  @brief parameter for setting the trigger effect to off mode.
 *         Off Mode: Stop trigger effect.
 **/
typedef struct ScePadTriggerEffectOffParam {
	uint8_t padding[48];
} ScePadTriggerEffectOffParam;

/**
 *E
 *  @brief parameter for setting the trigger effect to Feedback mode.
 *         Feedback Mode: The motor arm pushes back trigger.
 *                        Trigger obtains stiffness at specified position.
 **/
typedef struct ScePadTriggerEffectFeedbackParam {
	uint8_t position;	/*E position where the strength of target trigger start changing(0~9). */
	uint8_t strength;	/*E strength that the motor arm pushes back target trigger(0~8 (0: Same as Off mode)). */
	uint8_t padding[46];
} ScePadTriggerEffectFeedbackParam;

/**
 *E
 *  @brief parameter for setting the trigger effect to Weapon mode.
 *         Weapon Mode: Emulate weapon like gun trigger.
 **/
typedef struct ScePadTriggerEffectWeaponParam {
	uint8_t startPosition;	/*E position where the stiffness of trigger start changing(2~7). */
	uint8_t endPosition;	/*E position where the stiffness of trigger finish changing(startPosition+1~8). */
	uint8_t strength;		/*E strength of gun trigger(0~8 (0: Same as Off mode)). */
	uint8_t padding[45];
} ScePadTriggerEffectWeaponParam;

/**
 *E
 *  @brief parameter for setting the trigger effect to Vibration mode.
 *         Vibration Mode: Vibrates motor arm around specified position.
 **/
typedef struct ScePadTriggerEffectVibrationParam {
	uint8_t position;	/*E position where the motor arm start vibrating(0~9). */
	uint8_t amplitude;	/*E vibration amplitude(0~8 (0: Same as Off mode)). */
	uint8_t frequency;	/*E vibration frequency(0~255[Hz] (0: Same as Off mode)). */
	uint8_t padding[45];
} ScePadTriggerEffectVibrationParam;

/**
 *E
 *  @brief parameter for setting the trigger effect to ScePadTriggerEffectMultiplePositionFeedbackParam mode.
 *         Multi Position Feedback Mode: The motor arm pushes back trigger.
 *                                       Trigger obtains specified stiffness at each control point.
 **/
typedef struct ScePadTriggerEffectMultiplePositionFeedbackParam {
	uint8_t strength[SCE_PAD_TRIGGER_EFFECT_CONTROL_POINT_NUM];	/*E strength that the motor arm pushes back target trigger at position(0~8 (0: Same as Off mode)).
																 *  strength[0] means strength of motor arm at position0.
																 *  strength[1] means strength of motor arm at position1.
																 *  ...
																 * */
	uint8_t padding[38];
} ScePadTriggerEffectMultiplePositionFeedbackParam;

/**
 *E
 *  @brief parameter for setting the trigger effect to Feedback3 mode.
 *         Slope Feedback Mode: The motor arm pushes back trigger between two spedified control points.
 *                              Stiffness of the trigger is changing depending on the set place.
 **/
typedef struct ScePadTriggerEffectSlopeFeedbackParam {

	uint8_t startPosition;	/*E position where the strength of target trigger start changing(0~endPosition). */
	uint8_t endPosition; 	/*E position where the strength of target trigger finish changing(startPosition+1~9). */
	uint8_t startStrength;	/*E strength when trigger's position is startPosition(1~8) */
	uint8_t endStrength;	/*E strength when trigger's position is endPosition(1~8) */
	uint8_t padding[44];
} ScePadTriggerEffectSlopeFeedbackParam;

/**
 *E
 *  @brief parameter for setting the trigger effect to Vibration2 mode.
 *         Multi Position Vibration Mode: Vibrates motor arm around specified control point.
 *                                        Trigger vibrates specified amplitude at each control point.
 **/
typedef struct ScePadTriggerEffectMultiplePositionVibrationParam {
	uint8_t frequency;												/*E vibration frequency(0~255 (0: Same as Off mode)) */
	uint8_t amplitude[SCE_PAD_TRIGGER_EFFECT_CONTROL_POINT_NUM];	/*E vibration amplitude at position(0~8 (0: Same as Off mode)).
																	 *  amplitude[0] means amplitude of vibration at position0.
																	 *  amplitude[1] means amplitude of vibration at position1.
																	 *  ...
																	 * */
	uint8_t padding[37];
} ScePadTriggerEffectMultiplePositionVibrationParam;

/**
 *E
 *  @brief parameter for setting the trigger effect mode.
 **/
typedef union ScePadTriggerEffectCommandData {
	ScePadTriggerEffectOffParam							offParam;
	ScePadTriggerEffectFeedbackParam					feedbackParam;
	ScePadTriggerEffectWeaponParam						weaponParam;
	ScePadTriggerEffectVibrationParam					vibrationParam;
	ScePadTriggerEffectMultiplePositionFeedbackParam	multiplePositionFeedbackParam;
	ScePadTriggerEffectSlopeFeedbackParam				slopeFeedbackParam;
	ScePadTriggerEffectMultiplePositionVibrationParam	multiplePositionVibrationParam;
} ScePadTriggerEffectCommandData;

/**
 *E
 *  @brief parameter for setting the trigger effect.
 **/
typedef struct ScePadTriggerEffectCommand {
	ScePadTriggerEffectMode mode;
	uint8_t padding[4];
	ScePadTriggerEffectCommandData commandData;
} ScePadTriggerEffectCommand;

/**
 *E
 *  @brief parameter for the scePadSetTriggerEffect function.
 **/
typedef struct ScePadTriggerEffectParam {

	uint8_t triggerMask;		/*E Set trigger mask to activate trigger effect commands.
								 *  SCE_PAD_TRIGGER_EFFECT_TRIGGER_MASK_L2 : 0x01
								 *  SCE_PAD_TRIGGER_EFFECT_TRIGGER_MASK_R2 : 0x02
								 * */
	uint8_t padding[7];

	ScePadTriggerEffectCommand command[SCE_PAD_TRIGGER_EFFECT_TRIGGER_NUM];	/*E command[SCE_PAD_TRIGGER_EFFECT_PARAM_INDEX_FOR_L2] is for L2 trigger setting
																			 *  and param[SCE_PAD_TRIGGER_EFFECT_PARAM_INDEX_FOR_R2] is for R2 trgger setting.
																			 * */
} ScePadTriggerEffectParam;

#if defined(__cplusplus) && __cplusplus >= 201103L
static_assert(sizeof(ScePadTriggerEffectParam) == 120, "ScePadTriggerEffectParam has incorrect size");
#endif

#endif /* _SCE_PAD_TRIGGER_EFFECT_H */

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

