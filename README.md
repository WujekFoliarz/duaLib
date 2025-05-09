# duaLib
An open source version of the official sony controller library (dualshock 4/dualsense).
It aims to replicate the original library's behavior, possibly making it crossplatform

>hidapi.dll location = out\build\\(your build profile)\src\thirdparty\hidapi\src\windows

## Progress

| Controller | USB | Bluetooth |
| -----------|-----|-----------|
| DualShock 4|❌|❌|
| DualSense  |✅|❌|

| Function                                                                                  | Implementation  | Comment  |
| -------------                                                                             | -               |------------- | 
| int scePadInit()                                                                          |✅              |
| int scePadOpen(int userID, int, int, void*)                                               |⚠️              | The handle numbers are not accurate to libScePad's. Probably not important though
| int scePadSetParticularMode(bool mode)                                                    |✅              | 
| int scePadReadState(int handle, void* data)                                               |⚠️              | Orientation data missing
| int scePadSetLightBar(int handle, s_SceLightBar* lightbar)                                |✅              |
| int scePadGetContainerIdInformation(int handle, s_ScePadContainerIdInfo* containerIdInfo) |⚠️              | Windows only
| int scePadGetControllerBusType(int handle, int* busType)                                  |❌              |
| int scePadGetControllerInformation(int handle, s_ScePadInfo* info)                        |❌              |
| int scePadGetControllerType(int handle, s_SceControllerType* controllerType)              |❌              |
| int scePadGetHandle(int userID, int, int)                                                 |✅              |
| int scePadGetJackState(int handle, int state[4])                                          |❌              |
| int scePadGetTriggerEffectState(int handle, int state[2])                                 |❌              |
| int scePadIsControllerUpdateRequired(int handle)                                          |❌              |
| int scePadRead(int handle, void* data, int count)                                         |❌              |
| int scePadResetLightBar(int handle)                                                       |❌              |
| int scePadResetOrientation(int handle)                                                    |❌              |
| int scePadSetAngularVelocityDeadbandState(int handle, bool state)                         |❌              |
| int scePadSetAudioOutPath(int handle, int path)                                           |❌              |
| int scePadSetMotionSensorState(int handle, bool state)                                    |❌              |
| int scePadSetTiltCorrectionState(int handle, bool state)                                  |❌              |
| int scePadSetTriggerEffect(int handle, ScePadTriggerEffectParam* triggerEffect)           |❌              |
| int scePadSetVibration(int handle, s_ScePadVibrationParam* vibration)                     |❌              |
| int scePadSetVibrationMode(int handle, int mode)                                          |❌              |
| int scePadSetVolumeGain(int handle, s_ScePadVolumeGain* gainSettings)                     |❌              |
| int scePadIsSupportedAudioFunction(int handle)                                            |❌              |
| int scePadTerminate(void)                                                                 |❌              |
