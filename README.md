# duaLib
An open source version of the official sony controller library (dualshock 4/dualsense).
It aims to replicate the original library's behavior, possibly making it crossplatform

### Our [discord server](https://discord.com/invite/AFYvxf282U)

## Progress

| Controller | USB | Bluetooth |
| -----------|-----|-----------|
| DualShock 4|❌|❌|
| DualSense  |✅|✅|

| Function                                                                                  | Implementation  | Comment  |
| -------------                                                                             | -               |------------- | 
| int scePadInit()                                                                          |✅              |
| int scePadOpen(int userID, int, int, void*)                                               |⚠️              | The handle numbers are not accurate to libScePad's. Probably not important though
| int scePadSetParticularMode(bool mode)                                                    |✅              | 
| int scePadReadState(int handle, void* data)                                               |⚠️              | Orientation data missing
| int scePadSetLightBar(int handle, s_SceLightBar* lightbar)                                |✅              |
| int scePadGetContainerIdInformation(int handle, s_ScePadContainerIdInfo* containerIdInfo) |⚠️              | Windows only
| int scePadGetControllerBusType(int handle, int* busType)                                  |✅              |
| int scePadGetControllerInformation(int handle, s_ScePadInfo* info)                        |✅              |
| int scePadGetControllerType(int handle, s_SceControllerType* controllerType)              |✅              |
| int scePadGetHandle(int userID, int, int)                                                 |✅              |
| int scePadGetJackState(int handle, int* state)											|✅              |
| int scePadGetTriggerEffectState(int handle, uint8_t state[2])                             |✅              | 
| int scePadIsControllerUpdateRequired(int handle)                                          |❌              |
| int scePadRead(int handle, void* data, int count)                                         |❌              |
| int scePadResetLightBar(int handle)                                                       |✅              |
| int scePadResetOrientation(int handle)                                                    |❌              |
| int scePadSetAngularVelocityDeadbandState(int handle, bool state)                         |❌              |
| int scePadSetAudioOutPath(int handle, int path)                                           |❌              |
| int scePadSetMotionSensorState(int handle, bool state)                                    |❌              |
| int scePadSetTiltCorrectionState(int handle, bool state)                                  |❌              |
| int scePadSetTriggerEffect(int handle, ScePadTriggerEffectParam* triggerEffect)           |✅              |
| int scePadSetVibration(int handle, s_ScePadVibrationParam* vibration)                     |❌              |
| int scePadSetVibrationMode(int handle, int mode)                                          |❌              |
| int scePadSetVolumeGain(int handle, s_ScePadVolumeGain* gainSettings)                     |❌              |
| int scePadIsSupportedAudioFunction(int handle)                                            |❌              |
| int scePadTerminate(void)                                                                 |✅              |

 ## Credits
 https://gist.github.com/Nielk1/6d54cc2c00d2201ccb8c2720ad7538db
 
 https://controllers.fandom.com/wiki/Sony_DualSense

 https://controllers.fandom.com/wiki/Sony_DualShock_4

 https://www.psxhax.com/threads/prospero-directory-tree-listing-dumping-ps5-4-03-filesystem-script.12810/

 https://github.com/Inori/GPCS4/tree/master/GPCS4/SceModules/ScePad