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

#include "duaLib.h"
#include "dataStructures.h"
#include "crc.h"
#include "triggerFactory.h"

#define DEVICE_COUNT 3 
#define MAX_CONTROLLER_COUNT 4
#define VENDOR_ID 0x54c
#define DUALSENSE_DEVICE_ID 0x0ce6
#define DUALSHOCK4_DEVICE_ID 0x05c4
#define DUALSHOCK4V2_DEVICE_ID 0x09cc
#define UNKNOWN 0
#define DUALSHOCK4 1
#define DUALSENSE 2

namespace duaLibUtils {
	bool letGo(hid_device* handle, uint8_t deviceType, uint8_t connectionType) {
		if (handle && deviceType == DUALSENSE && (connectionType == HID_API_BUS_USB || connectionType == HID_API_BUS_UNKNOWN)) {
			dualsenseData::ReportOut02 data = {};
			data.ReportID = 0x02;

			data.State.ResetLights = true;
			data.State.AllowLedColor = false;
			data.State.AllowAudioControl = false;
			data.State.AllowAudioControl2 = false;
			data.State.AllowAudioMute = false;
			data.State.AllowColorLightFadeAnimation = false;
			data.State.AllowHapticLowPassFilter = false;
			data.State.AllowHeadphoneVolume = false;
			data.State.AllowLightBrightnessChange = false;
			data.State.AllowMicVolume = false;
			data.State.AllowMotorPowerLevel = false;
			data.State.AllowMuteLight = false;
			data.State.AllowPlayerIndicators = false;
			data.State.AllowSpeakerVolume = false;
			data.State.UseRumbleNotHaptics = false;
			data.State.RumbleEmulationLeft = 0;
			data.State.RumbleEmulationRight = 0;

			data.State.AllowLeftTriggerFFB = true;
			data.State.AllowRightTriggerFFB = true;
			TriggerEffectGenerator::Off(data.State.LeftTriggerFFB, 0);
			TriggerEffectGenerator::Off(data.State.RightTriggerFFB, 0);

			uint8_t res = hid_write(handle, reinterpret_cast<unsigned char*>(&data), sizeof(data));
			return true;
		}
		else if (handle && deviceType == DUALSENSE && connectionType == HID_API_BUS_BLUETOOTH) {
			dualsenseData::ReportOut31 data = {};

			data.Data.ReportID = 0x31;
			data.Data.flag = 2;
			data.Data.State.ResetLights = true;
			uint32_t crc = compute(data.CRC.Buff, sizeof(data) - 4);
			data.CRC.CRC = crc;
			uint8_t res = hid_write(handle, reinterpret_cast<unsigned char*>(&data), sizeof(data));

			data.Data.State.ResetLights = false;
			data.Data.State.AllowLedColor = true;
			data.Data.State.AllowLeftTriggerFFB = true;
			data.Data.State.AllowRightTriggerFFB = true;
			data.Data.State.LedBlue = 255;
			TriggerEffectGenerator::Off(data.Data.State.LeftTriggerFFB, 0);
			TriggerEffectGenerator::Off(data.Data.State.RightTriggerFFB, 0);
			crc = compute(data.CRC.Buff, sizeof(data) - 4);
			data.CRC.CRC = crc;
			res = hid_write(handle, reinterpret_cast<unsigned char*>(&data), sizeof(data));

			return true;
		}

		return false;
	}

	bool getHardwareVersion(hid_device* handle, dualsenseData::ReportFeatureInVersion& report) {
		if (!handle) return false;

		unsigned char buffer[64] = { };
		buffer[0] = 0x20;
		int res = hid_get_feature_report(handle, buffer, sizeof(buffer));

		if (res > 0) {
			const auto versionReport = *reinterpret_cast<dualsenseData::ReportFeatureInVersion*>(buffer);
			report = versionReport;
			return true;
		}

		return false;
	}

	bool getMacAddress(hid_device* handle, std::string& outMac) {
		if (!handle) return false;

		unsigned char buffer[20] = {};
		buffer[0] = 0x09; // Report ID
		int res = hid_get_feature_report(handle, buffer, sizeof(buffer));

		if (res > 0) {
			const auto macReport = *reinterpret_cast<dualsenseData::ReportFeatureInMacAll*>(buffer);
			char tmp[18];
			snprintf(tmp, sizeof(tmp), "%02X:%02X:%02X:%02X:%02X:%02X",
					 macReport.ClientMac[5], macReport.ClientMac[4], macReport.ClientMac[3],
					 macReport.ClientMac[2], macReport.ClientMac[1], macReport.ClientMac[0]);
			outMac = tmp;
			return true;
		}
		return false;
	}

	bool isValid(hid_device* handle) {
		if (!handle) return false;

		std::string address;
		bool res = getMacAddress(handle, address);
		if (res) {
			return true;
		}

		return false;
	}

#ifdef _WIN32
	static std::wstring Utf8ToWide(const char* utf8) {
		int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
		if (wlen <= 0) return L"";
		std::vector<wchar_t> buf(wlen);
		MultiByteToWideChar(CP_UTF8, 0, utf8, -1, buf.data(), wlen);
		return std::wstring(buf.data());
	}
#endif

	bool GetID(const char* narrowPath, const char** ID, int* size) {
	#ifdef _WIN32
		GUID hidGuid;
		GUID outContainerId;
		HidD_GetHidGuid(&hidGuid);

		HDEVINFO devs = SetupDiGetClassDevs(
			&hidGuid,
			nullptr,
			nullptr,
			DIGCF_DEVICEINTERFACE | DIGCF_PRESENT
		);
		if (devs == INVALID_HANDLE_VALUE) {
			return false;
		}

		SP_DEVICE_INTERFACE_DATA ifData = { sizeof(ifData) };
		DWORD index = 0;
		std::wstring targetPath = Utf8ToWide(narrowPath);
		std::transform(targetPath.begin(), targetPath.end(), targetPath.begin(), ::tolower);

		while (SetupDiEnumDeviceInterfaces(devs, nullptr, &hidGuid, index++, &ifData)) {
			DWORD needed = 0;
			SetupDiGetDeviceInterfaceDetailW(devs, &ifData, nullptr, 0, &needed, nullptr);
			auto detailBuf = (SP_DEVICE_INTERFACE_DETAIL_DATA_W*)malloc(needed);
			detailBuf->cbSize = sizeof(*detailBuf);
			SP_DEVINFO_DATA devInfo = { sizeof(devInfo) };

			if (SetupDiGetDeviceInterfaceDetailW(
				devs, &ifData,
				detailBuf, needed,
				nullptr,
				&devInfo
				)) {
				if (targetPath == detailBuf->DevicePath) {
					DEVPROPTYPE propType = 0;
					DWORD cb = sizeof(GUID);
					if (SetupDiGetDevicePropertyW(
						devs,
						&devInfo,
						&DEVPKEY_Device_ContainerId,
						&propType,
						reinterpret_cast<PBYTE>(&outContainerId),
						cb,
						&cb,
						0
						)) {
						free(detailBuf);
						SetupDiDestroyDeviceInfoList(devs);

						wchar_t guidStr[39] = {};
						StringFromGUID2(outContainerId, guidStr, _countof(guidStr));

						*size = sizeof(guidStr);
						static char buffer[39] = {};
						std::wcstombs(buffer, guidStr, sizeof(buffer));
						std::transform(buffer, buffer + std::strlen(buffer), buffer, [](unsigned char c) {return std::tolower(c); });
						*ID = buffer;

						return true;
					}
				}
			}
			free(detailBuf);
		}

		SetupDiDestroyDeviceInfoList(devs);
	#endif
		return false;
	}

	struct trigger {
		uint8_t force[11] = {};
	};

	struct controller {
		hid_device* handle = 0;
		uint16_t sceHandle = 0;
		uint8_t playerIndex = 0;
		uint8_t deviceType = UNKNOWN;
		uint8_t seqNo = 0;
		std::mutex lock;
		uint8_t connectionType = 0;
		bool opened = false;
		bool wasMicBtnClicked = false;
		bool isMicMuted = false;
		bool wasDisconnected = false;
		bool valid = true;
		uint8_t failedReadCount = 0;
		dualsenseData::USBGetStateData currentInputState = {};
		dualsenseData::SetStateData lastOutputState = {};
		dualsenseData::SetStateData currentOutputState = {};
		dualsenseData::ReportFeatureInVersion versionReport = {};
		std::string macAddress = "";
		std::string systemIdentifier = "";
		const char* lastPath = "";
		const char* id = "";
		uint16_t idSize = 0;
		trigger L2 = {};
		trigger R2 = {};
		uint8_t triggerMask = 0;
	};
}

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
duaLibUtils::controller g_controllers[MAX_CONTROLLER_COUNT] = {};
std::atomic<bool> g_threadRunning = false;
std::atomic<bool> g_initialized = false;
std::atomic<bool> g_particularMode = false;
std::thread g_readThread;
std::thread g_watchThread;

int readFunc() {
	while (g_threadRunning) {
		bool allInvalid = true;

		for (auto& controller : g_controllers) {
			std::lock_guard<std::mutex> guard(controller.lock);

			if (controller.valid && controller.opened && controller.deviceType == DUALSENSE) {
				allInvalid = false;
				bool isBt = controller.connectionType == HID_API_BUS_BLUETOOTH ? true : false;

				dualsenseData::ReportIn01USB  inputUsb = {};
				dualsenseData::ReportIn31  inputBt = {};
				inputUsb.ReportID = 0x01;
				inputBt.Data.ReportID = 0x31;

				int32_t res = -1;

				if (isBt) {
					res = hid_read(controller.handle, reinterpret_cast<unsigned char*>(&inputBt), sizeof(inputBt));
				}
				else {
					res = hid_read(controller.handle, reinterpret_cast<unsigned char*>(&inputUsb), sizeof(inputUsb));
				}

				dualsenseData::USBGetStateData inputData = isBt ? inputBt.Data.State.StateData : inputUsb.State;

				if (controller.failedReadCount >= 254) {
					controller.valid = false;
				}

				if (res == -1) {
					controller.failedReadCount++;
					continue;
				}
				else if (res > 0) {
					controller.failedReadCount = 0;

					if (controller.currentOutputState.LedRed != controller.lastOutputState.LedRed ||
						controller.currentOutputState.LedGreen != controller.lastOutputState.LedGreen ||
						controller.currentOutputState.LedBlue != controller.lastOutputState.LedBlue ||
						controller.wasDisconnected) {
						controller.currentOutputState.AllowLedColor = true;
					}
					else {
						controller.currentOutputState.AllowLedColor = false;
					}

					bool oldStyle = ((controller.versionReport.HardwareInfo & 0x00FFFF00) < 0x00000400);
					switch (controller.playerIndex) {
						case 1:
							controller.currentOutputState.PlayerLight1 = false;
							controller.currentOutputState.PlayerLight2 = false;
							controller.currentOutputState.PlayerLight3 = true;
							controller.currentOutputState.PlayerLight4 = false;
							controller.currentOutputState.PlayerLight5 = false;
							break;

						case 2:
							controller.currentOutputState.PlayerLight1 = false;
							controller.currentOutputState.PlayerLight2 = oldStyle ? true : true;
							controller.currentOutputState.PlayerLight3 = false;
							controller.currentOutputState.PlayerLight4 = oldStyle ? true : false;
							controller.currentOutputState.PlayerLight5 = false;
							break;

						case 3:
							controller.currentOutputState.PlayerLight1 = true;
							controller.currentOutputState.PlayerLight2 = oldStyle ? false : true;
							controller.currentOutputState.PlayerLight3 = oldStyle ? true : false;
							controller.currentOutputState.PlayerLight4 = false;
							controller.currentOutputState.PlayerLight5 = oldStyle ? true : false;
							break;

						case 4:
							controller.currentOutputState.PlayerLight1 = true;
							controller.currentOutputState.PlayerLight2 = true;
							controller.currentOutputState.PlayerLight3 = false;
							controller.currentOutputState.PlayerLight4 = oldStyle ? true : false;
							controller.currentOutputState.PlayerLight5 = oldStyle ? true : false;
							break;
					}

					if (controller.currentOutputState.PlayerLight1 != controller.lastOutputState.PlayerLight1 ||
						controller.currentOutputState.PlayerLight2 != controller.lastOutputState.PlayerLight2 ||
						controller.currentOutputState.PlayerLight3 != controller.lastOutputState.PlayerLight3 ||
						controller.currentOutputState.PlayerLight4 != controller.lastOutputState.PlayerLight4 ||
						controller.wasDisconnected) {
						controller.currentOutputState.AllowPlayerIndicators = true;
					}
					else {
						controller.currentOutputState.AllowPlayerIndicators = false;
					}

					if (!inputData.ButtonMute && controller.currentInputState.ButtonMute) {
						controller.isMicMuted = !controller.isMicMuted;
						controller.currentOutputState.MuteLightMode = controller.isMicMuted ? dualsenseData::MuteLight::On : dualsenseData::MuteLight::Off;
						controller.currentOutputState.MicMute = controller.isMicMuted;
						controller.currentOutputState.AllowMuteLight = true;
						if (isBt) { // mic led won't change without these on bluetooth
							controller.currentOutputState.AllowLedColor = true;
							controller.currentOutputState.AllowPlayerIndicators = true;
						}
					}
					else {
						controller.currentOutputState.AllowMuteLight = false;
					}

					if (controller.wasDisconnected) {
						controller.currentOutputState.MicMute = controller.isMicMuted;
						controller.currentOutputState.AllowMuteLight = true;
					}

					if (controller.triggerMask & SCE_PAD_TRIGGER_EFFECT_TRIGGER_MASK_L2 || controller.wasDisconnected) {
						controller.currentOutputState.AllowLeftTriggerFFB = true;
						for (int i = 0; i < 11; i++) {
							controller.currentOutputState.LeftTriggerFFB[i] = controller.L2.force[i];
						}
					}
					else {
						controller.currentOutputState.AllowLeftTriggerFFB = false;
					}

					if (controller.triggerMask & SCE_PAD_TRIGGER_EFFECT_TRIGGER_MASK_R2 || controller.wasDisconnected) {
						controller.currentOutputState.AllowRightTriggerFFB = true;
						for (int i = 0; i < 11; i++) {
							controller.currentOutputState.RightTriggerFFB[i] = controller.R2.force[i];
						}
					}
					else {
						controller.currentOutputState.AllowRightTriggerFFB = false;
					}

					controller.triggerMask = 0;
					res = -1;

					if (controller.connectionType == HID_API_BUS_USB || controller.connectionType == HID_API_BUS_UNKNOWN) {
						dualsenseData::ReportOut02 usbOutput = {};

						usbOutput.ReportID = 0x02;
						usbOutput.State = controller.currentOutputState;

						if ((controller.currentOutputState != controller.lastOutputState) || controller.wasDisconnected) {
							res = hid_write(controller.handle, reinterpret_cast<unsigned char*>(&usbOutput), sizeof(usbOutput));
						}
					}
					else if (controller.connectionType == HID_API_BUS_BLUETOOTH) {
						dualsenseData::ReportOut31 btOutput = {};

						btOutput.Data.ReportID = 0x31;
						btOutput.Data.flag = 2;
						btOutput.Data.State = controller.currentOutputState;

						uint32_t crc = compute(btOutput.CRC.Buff, sizeof(btOutput) - 4);
						btOutput.CRC.CRC = crc;
						if ((controller.currentOutputState != controller.lastOutputState) || controller.wasDisconnected) {
							res = hid_write(controller.handle, reinterpret_cast<unsigned char*>(&btOutput), sizeof(btOutput));
						}
					}

					if (res > 0) {
						controller.lastOutputState = controller.currentOutputState;
						controller.wasDisconnected = false;

						std::cout << "Controller idx " << controller.sceHandle
							<< " path=" << controller.macAddress
							<< " connType=" << (int)controller.connectionType
							<< std::endl;
					}

					controller.currentInputState = inputData;
				}
			}
			else if (!controller.valid && controller.opened) {
				controller.lastPath = "";
				controller.macAddress = "";
				controller.wasDisconnected = true;
			}
		}

		if (allInvalid) {
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}

		std::this_thread::sleep_for(std::chrono::nanoseconds(200));
	}

	return 0;
}

int watchFunc() {
	while (g_threadRunning) {
		for (auto& controller : g_controllers) {
			bool valid;
			{
				std::lock_guard<std::mutex> guard(controller.lock);
				valid = duaLibUtils::isValid(controller.handle);
			}

			if (!valid) {
				for (int j = 0; j < DEVICE_COUNT; ++j) {
					hid_device_info* head = hid_enumerate(
						g_deviceList.devices[j].Vendor,
						g_deviceList.devices[j].Device
					);

					for (hid_device_info* info = head; info; info = info->next) {
						hid_device* handle = hid_open_path(info->path);

						if (!handle) continue;
						hid_set_nonblocking(handle, 1);

						std::string newMac;
						if (duaLibUtils::getMacAddress(handle, newMac)) {
							bool already = false;
							for (int k = 0; k < MAX_CONTROLLER_COUNT; ++k) {
								std::lock_guard<std::mutex> guard(g_controllers[k].lock);
								if (g_controllers[k].macAddress == newMac) {
									already = true;
									break;
								}
							}

							if (!already) {

								std::lock_guard<std::mutex> guard(controller.lock);
								controller.handle = handle;
								controller.macAddress = newMac;
								controller.connectionType = info->bus_type;
								controller.valid = true;
								controller.failedReadCount = 0;
								controller.lastPath = info->path;

								const char* id = {};
								uint16_t size = 0;
								duaLibUtils::GetID(info->path, &id, reinterpret_cast<int*>(&size));

								controller.id = id;
								controller.idSize = size;

								uint16_t dev = g_deviceList.devices[j].Device;

								if (dev == DUALSENSE_DEVICE_ID) { controller.deviceType = DUALSENSE; }
								else if (dev == DUALSHOCK4_DEVICE_ID || dev == DUALSHOCK4V2_DEVICE_ID) { controller.deviceType = DUALSHOCK4; }

								duaLibUtils::getHardwareVersion(controller.handle, controller.versionReport);

								if (controller.deviceType == DUALSENSE && info->bus_type == HID_API_BUS_USB) {
									dualsenseData::ReportOut02 report = {};
									report.State.AllowLedColor = true;
									report.State.AllowPlayerIndicators = true;
									report.State.ResetLights = true;
									hid_write(
										controller.handle,
										reinterpret_cast<unsigned char*>(&report),
										sizeof(report)
									);
								}
								else if (controller.deviceType == DUALSENSE && info->bus_type == HID_API_BUS_BLUETOOTH) {
									dualsenseData::ReportOut31 report = {};

									report.Data.ReportID = 0x31;
									report.Data.flag = 2;
									report.Data.State.EnableRumbleEmulation = true;
									report.Data.State.UseRumbleNotHaptics = true;
									report.Data.State.AllowRightTriggerFFB = true;
									report.Data.State.AllowLeftTriggerFFB = true;
									report.Data.State.AllowLedColor = true;
									report.Data.State.ResetLights = true;

									uint32_t crc = compute(report.CRC.Buff, sizeof(report) - 4);
									report.CRC.CRC = crc;

									uint8_t res = hid_write(
										controller.handle,
										reinterpret_cast<unsigned char*>(&report),
										sizeof(report)
									);
								}

								break;
							}

							hid_close(handle);
						}
						else {
							hid_close(handle);
						}
					}

					hid_free_enumeration(head);

					{
						std::lock_guard<std::mutex> guard(controller.lock);
						if (!controller.macAddress.empty())
							break;
					}
				}
			}
			else {
				std::string cur;
				bool ok;
				{
					std::lock_guard<std::mutex> guard(controller.lock);
					ok = duaLibUtils::getMacAddress(controller.handle, cur);
					if (ok) controller.macAddress = cur;
				}
			}
		}

		std::this_thread::sleep_for(std::chrono::seconds(5));
	}

	return 0;
}


int scePadInit() {
	if (!g_initialized) {

	#ifdef _WIN32
		timeBeginPeriod(1);
	#endif

		int res = hid_init();

		if (res)
			return res;

		g_threadRunning = true;
		g_readThread = std::thread(readFunc);
		g_watchThread = std::thread(watchFunc);
		g_readThread.detach();
		g_watchThread.detach();
	}

	return 0;
}

int scePadTerminate(void) {
	g_threadRunning = false;
	g_initialized = false;

	for (auto& controller : g_controllers) {
		// release controller here
		controller.valid = false;
		controller.sceHandle = 0;
		controller.lastPath = "";
		controller.wasDisconnected = true;
		controller.macAddress = "";

		duaLibUtils::letGo(controller.handle, controller.deviceType, controller.connectionType);
	}
	g_particularMode = false;
	return SCE_OK;
}

int scePadOpen(int userID, int, int, void*) {
	if (userID > DEVICE_COUNT + 1 || userID < 0)
		return SCE_PAD_ERROR_INVALID_ARG;

	int index = userID - 1;
	bool wasAlreadyOpened = false;
	int lastUnused = 0;
	int firstUnused = -1;
	int occupiedCount = 0;
	int count = 0;

	for (auto& controller : g_controllers) {
		std::lock_guard<std::mutex> guard(controller.lock);

		if (controller.sceHandle == 0 && controller.playerIndex != userID) {
			if (firstUnused == -1)
				firstUnused = count;

			lastUnused++;
		}
		else if (controller.sceHandle == sizeof(controller) * (occupiedCount + 1) || controller.playerIndex == userID) {
			if (occupiedCount > DEVICE_COUNT) {
				wasAlreadyOpened = true;
				return SCE_PAD_ERROR_ALREADY_OPENED;
			}
		}
		else {
			occupiedCount++;
		}

		count++;
	}

	if (!wasAlreadyOpened) {
		int handle = sizeof(duaLibUtils::controller) * (firstUnused + 1);
		g_controllers[firstUnused].sceHandle = handle;
		g_controllers[firstUnused].opened = true;
		g_controllers[firstUnused].playerIndex = userID;
		return handle;
	}

	return SCE_PAD_ERROR_NO_HANDLE;
}

int scePadSetParticularMode(bool mode) {
	g_particularMode = mode;
	return SCE_OK;
}

int scePadReadState(int handle, void* data) {
	for (auto& controller : g_controllers) {
		std::lock_guard<std::mutex> guard(controller.lock);

		if (controller.sceHandle != handle) continue;
		if (!controller.valid) return SCE_PAD_ERROR_DEVICE_NOT_CONNECTED;

		s_ScePadData state = {};

		if (controller.deviceType == DUALSENSE) {
		#pragma region buttons
			uint32_t bitmaskButtons = 0;
			if (controller.currentInputState.ButtonCross) bitmaskButtons |= SCE_BM_CROSS;
			if (controller.currentInputState.ButtonCircle) bitmaskButtons |= SCE_BM_CIRCLE;
			if (controller.currentInputState.ButtonTriangle) bitmaskButtons |= SCE_BM_TRIANGLE;
			if (controller.currentInputState.ButtonSquare) bitmaskButtons |= SCE_BM_SQUARE;

			if (controller.currentInputState.ButtonL1) bitmaskButtons |= 0x00000400;
			if (controller.currentInputState.ButtonL2) bitmaskButtons |= 0x00000100;
			if (controller.currentInputState.ButtonR1) bitmaskButtons |= 0x00000800;
			if (controller.currentInputState.ButtonR2) bitmaskButtons |= 0x00000200;

			if (controller.currentInputState.ButtonL3) bitmaskButtons |= 0x00000002;
			if (controller.currentInputState.ButtonR3) bitmaskButtons |= 0x00000004;

			if (controller.currentInputState.DPad == dualsenseData::Direction::North) bitmaskButtons |= 0x00000010;
			if (controller.currentInputState.DPad == dualsenseData::Direction::South) bitmaskButtons |= 0x00000040;
			if (controller.currentInputState.DPad == dualsenseData::Direction::East) bitmaskButtons |= 0x00000020;
			if (controller.currentInputState.DPad == dualsenseData::Direction::West) bitmaskButtons |= 0x00000080;

			if (controller.currentInputState.ButtonOptions) bitmaskButtons |= 0x00000008;

			if (controller.currentInputState.ButtonPad) bitmaskButtons |= 0x00100000;

			if (g_particularMode) {
				if (controller.currentInputState.ButtonCreate) bitmaskButtons |= 0x00000001;
				if (controller.currentInputState.ButtonHome) bitmaskButtons |= 0x00010000;
			}

			state.bitmask_buttons = bitmaskButtons;
		#pragma endregion

		#pragma region sticks
			state.LeftStick.X = controller.currentInputState.LeftStickX;
			state.LeftStick.Y = controller.currentInputState.LeftStickY;
			state.RightStick.Y = controller.currentInputState.RightStickX;
			state.RightStick.Y = controller.currentInputState.RightStickY;
		#pragma endregion

		#pragma region triggers
			state.L2_Analog = controller.currentInputState.TriggerLeft;
			state.R2_Analog = controller.currentInputState.TriggerRight;
		#pragma endregion

		#pragma region gyro
			state.orientation.w = 0; // what is orientation?
			state.orientation.x = 0;
			state.orientation.y = 0;
			state.orientation.z = 0;

			state.acceleration.x = (float)(controller.currentInputState.AccelerometerX) / 8192;
			state.acceleration.y = (float)(controller.currentInputState.AccelerometerY) / 8192;
			state.acceleration.z = (float)(controller.currentInputState.AccelerometerZ) / 8192;

			state.angularVelocity.x = (float)(controller.currentInputState.AngularVelocityX) / 8192;
			state.angularVelocity.y = (float)(controller.currentInputState.AngularVelocityY) / 8192;
			state.angularVelocity.z = (float)(controller.currentInputState.AngularVelocityZ) / 8192;
		#pragma endregion

		#pragma region touchpad
			state.touchData.touchNum = controller.currentInputState.touchData.Timestamp;

			state.touchData.touch[0].id = controller.currentInputState.touchData.Finger[0].Index;
			state.touchData.touch[0].x = controller.currentInputState.touchData.Finger[0].FingerX;
			state.touchData.touch[0].y = controller.currentInputState.touchData.Finger[0].FingerY;

			state.touchData.touch[1].id = controller.currentInputState.touchData.Finger[1].Index;
			state.touchData.touch[1].x = controller.currentInputState.touchData.Finger[1].FingerX;
			state.touchData.touch[1].y = controller.currentInputState.touchData.Finger[1].FingerY;
		#pragma endregion

		#pragma region misc
			state.connected = controller.valid;
			state.timestamp = controller.currentInputState.DeviceTimeStamp;
			state.extUnitData = {};
			state.connectionCount = 0;
			for (int j = 0; j < 12; j++)
				state.deviceUniqueData[j] = {};
			state.deviceUniqueDataLen = sizeof(state.deviceUniqueData);
		#pragma endregion
		}

		std::memcpy(data, &state, sizeof(state));
		return SCE_OK;
	}
	return SCE_PAD_ERROR_INVALID_HANDLE;
}

int scePadGetContainerIdInformation(int handle, s_ScePadContainerIdInfo* containerIdInfo) {
#ifdef _WIN32 // Windows only for now
	for (auto& controller : g_controllers) {
		std::lock_guard<std::mutex> guard(controller.lock);
		if (controller.sceHandle == handle && controller.id != "" && controller.idSize != 0) {
			if (!controller.valid) return SCE_PAD_ERROR_DEVICE_NOT_CONNECTED;

			s_ScePadContainerIdInfo info = {};
			info.size = controller.idSize;
			strncpy_s(info.id, controller.id, sizeof(info.id) - 1);
			info.id[sizeof(info.id) - 1] = '\0';
			*containerIdInfo = info;
			return SCE_OK;
		}
	}
	containerIdInfo->size = 0;
	containerIdInfo->id[0] = '\0';
#endif
	return SCE_PAD_ERROR_INVALID_HANDLE;
}

int scePadSetLightBar(int handle, s_SceLightBar* lightbar) {
	for (auto& controller : g_controllers) {
		std::lock_guard<std::mutex> guard(controller.lock);

		if (controller.sceHandle != handle) continue;
		if (!controller.valid) return SCE_PAD_ERROR_DEVICE_NOT_CONNECTED;

		if (controller.deviceType == DUALSENSE) {
			controller.currentOutputState.LedRed = lightbar->r;
			controller.currentOutputState.LedGreen = lightbar->g;
			controller.currentOutputState.LedBlue = lightbar->b;
		}
		return SCE_OK;
	}
	return SCE_PAD_ERROR_INVALID_HANDLE;
}

int scePadGetHandle(int userID, int, int) {
	if (userID > DEVICE_COUNT + 1 || userID < 0) return SCE_PAD_ERROR_INVALID_PORT;

	for (int i = 0; i < DEVICE_COUNT; i++) {
		std::lock_guard<std::mutex> guard(g_controllers[i].lock);

		if (g_controllers[i].playerIndex != userID) continue;
		return g_controllers[i].sceHandle;
	}
	return SCE_PAD_ERROR_NO_HANDLE;
}

int scePadResetLightBar(int handle) {
	for (auto& controller : g_controllers) {
		std::lock_guard<std::mutex> guard(controller.lock);

		if (controller.sceHandle != handle) continue;
		if (!controller.valid) return SCE_PAD_ERROR_DEVICE_NOT_CONNECTED;

		if (controller.deviceType == DUALSENSE) {
			controller.currentOutputState.LedRed = 0;
			controller.currentOutputState.LedGreen = 0;
			controller.currentOutputState.LedBlue = 0;
		}
		return SCE_OK;
	}
	return SCE_PAD_ERROR_INVALID_HANDLE;
}

int scePadSetTriggerEffect(int handle, ScePadTriggerEffectParam* triggerEffect) {
	for (auto& controller : g_controllers) {
		std::lock_guard<std::mutex> guard(controller.lock);

		if (controller.sceHandle != handle) continue;
		if (!controller.valid) return SCE_PAD_ERROR_DEVICE_NOT_CONNECTED;
		if (controller.deviceType != DUALSENSE) return SCE_PAD_ERROR_NOT_PERMITTED;

		controller.triggerMask = triggerEffect->triggerMask;

		for (int i = 0; i <= 1; i++) {
			duaLibUtils::trigger _trigger = {};

			if (triggerEffect->command[i].mode == ScePadTriggerEffectMode::SCE_PAD_TRIGGER_EFFECT_MODE_OFF) {
				TriggerEffectGenerator::Off(_trigger.force, 0);
			}
			else if (triggerEffect->command[i].mode == ScePadTriggerEffectMode::SCE_PAD_TRIGGER_EFFECT_MODE_FEEDBACK) {
				TriggerEffectGenerator::Feedback(_trigger.force, 0, triggerEffect->command[i].commandData.feedbackParam.position, triggerEffect->command[i].commandData.feedbackParam.strength);
			}
			else if (triggerEffect->command[i].mode == ScePadTriggerEffectMode::SCE_PAD_TRIGGER_EFFECT_MODE_WEAPON) {
				TriggerEffectGenerator::Weapon(_trigger.force, 0, triggerEffect->command[i].commandData.weaponParam.startPosition, triggerEffect->command[i].commandData.weaponParam.endPosition, triggerEffect->command[i].commandData.weaponParam.strength);
			}
			else if (triggerEffect->command[i].mode == ScePadTriggerEffectMode::SCE_PAD_TRIGGER_EFFECT_MODE_VIBRATION) {
				TriggerEffectGenerator::Vibration(_trigger.force, 0, triggerEffect->command[i].commandData.vibrationParam.position, triggerEffect->command[i].commandData.vibrationParam.amplitude, triggerEffect->command[i].commandData.vibrationParam.frequency);
			}
			else if (triggerEffect->command[i].mode == ScePadTriggerEffectMode::SCE_PAD_TRIGGER_EFFECT_MODE_SLOPE_FEEDBACK) {
				TriggerEffectGenerator::SlopeFeedback(_trigger.force, 0, triggerEffect->command[i].commandData.slopeFeedbackParam.startPosition, triggerEffect->command[i].commandData.slopeFeedbackParam.endPosition, triggerEffect->command[i].commandData.slopeFeedbackParam.startStrength, triggerEffect->command[i].commandData.slopeFeedbackParam.endStrength);
			}
			else if (triggerEffect->command[i].mode == ScePadTriggerEffectMode::SCE_PAD_TRIGGER_EFFECT_MODE_MULTIPLE_POSITION_FEEDBACK) {
				TriggerEffectGenerator::MultiplePositionFeedback(_trigger.force, 0, triggerEffect->command[i].commandData.multiplePositionFeedbackParam.strength);
			}
			else if (triggerEffect->command[i].mode == ScePadTriggerEffectMode::SCE_PAD_TRIGGER_EFFECT_MODE_MULTIPLE_POSITION_VIBRATION) {
				TriggerEffectGenerator::MultiplePositionVibration(_trigger.force, 0, triggerEffect->command[i].commandData.multiplePositionVibrationParam.frequency, triggerEffect->command[i].commandData.multiplePositionVibrationParam.amplitude);
			}

			if (i == SCE_PAD_TRIGGER_EFFECT_PARAM_INDEX_FOR_L2) {
				for (int i = 0; i < 11; i++) {
					controller.L2.force[i] = _trigger.force[i];
				}
			}
			else if (i == SCE_PAD_TRIGGER_EFFECT_PARAM_INDEX_FOR_R2) {
				for (int i = 0; i < 11; i++) {
					controller.R2.force[i] = _trigger.force[i];
				}
			}
		}

		return SCE_OK;
	}
	return SCE_PAD_ERROR_INVALID_HANDLE;
}

int scePadGetControllerBusType(int handle, int* busType) {
	for (auto& controller : g_controllers) {
		std::lock_guard<std::mutex> guard(controller.lock);

		if (controller.sceHandle != handle) continue;
		if (!controller.valid) return SCE_PAD_ERROR_DEVICE_NOT_CONNECTED;

		if (controller.deviceType == DUALSENSE) {
			*busType = controller.connectionType;
		}
		return SCE_OK;
	}
	return SCE_PAD_ERROR_INVALID_HANDLE;
}

int scePadGetControllerInformation(int handle, s_ScePadInfo* info) {
	for (auto& controller : g_controllers) {
		std::lock_guard<std::mutex> guard(controller.lock);

		if (controller.sceHandle != handle) continue;
		if (!controller.valid) return SCE_PAD_ERROR_DEVICE_NOT_CONNECTED;

		s_ScePadInfo _info = {};

		_info.touchPadInfo.resolution.x = controller.deviceType == DUALSENSE ? 1920 : 1920;
		_info.touchPadInfo.resolution.y = controller.deviceType == DUALSENSE ? 1080 : 1080; // I don't think the dualshock 4 res in 1080 here but that's what the original library outputs
		_info.touchPadInfo.pixelDensity = DUALSENSE ? 44.86 : 44;
		_info.stickInfo.deadZoneLeft = DUALSENSE ? 13 : 13;
		_info.stickInfo.deadZoneRight = DUALSENSE ? 13 : 13;
		_info.connectionType = SCE_PAD_CONNECTION_TYPE_LOCAL;
		_info.connectedCount = 1;
		_info.connected = controller.valid;
		_info.deviceClass = SCE_PAD_DEVICE_CLASS_STANDARD;

		*info = _info;
		return SCE_OK;
	}
	return SCE_PAD_ERROR_INVALID_HANDLE;
}

int scePadGetControllerType(int handle, s_SceControllerType* controllerType) {
	for (auto& controller : g_controllers) {
		std::lock_guard<std::mutex> guard(controller.lock);

		if (controller.sceHandle != handle) continue;
		if (!controller.valid) return SCE_PAD_ERROR_DEVICE_NOT_CONNECTED;

		if (controller.deviceType == DUALSENSE) {
			*controllerType = (s_SceControllerType)controller.deviceType;
		}

		return SCE_OK;
	}
	return SCE_PAD_ERROR_INVALID_HANDLE;
}

int scePadGetJackState(int handle, int* state) {
	for (auto& controller : g_controllers) {
		std::lock_guard<std::mutex> guard(controller.lock);

		if (controller.sceHandle != handle) continue;
		if (!controller.valid) return SCE_PAD_ERROR_DEVICE_NOT_CONNECTED;

		if (controller.deviceType == DUALSENSE) {
			*state = controller.currentInputState.PluggedHeadphones + controller.currentInputState.PluggedMic;
		}

		return SCE_OK;
	}
	return SCE_PAD_ERROR_INVALID_HANDLE;
}

int main() {
	if (scePadInit() != SCE_OK) {
		std::cout << "Failed to initalize!" << std::endl;
	}

	//int handle = scePadOpen(1, NULL, NULL, NULL);
	int handle = scePadOpen(1, 0, 0, 0);
	//int handle2 = scePadOpen(2, 0, 0, 0);

	std::cout << handle << std::endl;
	getchar();
	s_SceLightBar l = {};
	l.g = 255;
	scePadSetLightBar(handle, &l);

	ScePadTriggerEffectParam trigger = {};
	trigger.triggerMask = SCE_PAD_TRIGGER_EFFECT_TRIGGER_MASK_L2 | SCE_PAD_TRIGGER_EFFECT_TRIGGER_MASK_R2;
	trigger.command[SCE_PAD_TRIGGER_EFFECT_PARAM_INDEX_FOR_L2].mode = ScePadTriggerEffectMode::SCE_PAD_TRIGGER_EFFECT_MODE_MULTIPLE_POSITION_VIBRATION;
	trigger.command[SCE_PAD_TRIGGER_EFFECT_PARAM_INDEX_FOR_L2].commandData.multiplePositionVibrationParam.amplitude[0] = 0;
	trigger.command[SCE_PAD_TRIGGER_EFFECT_PARAM_INDEX_FOR_L2].commandData.multiplePositionVibrationParam.amplitude[1] = 8;
	trigger.command[SCE_PAD_TRIGGER_EFFECT_PARAM_INDEX_FOR_L2].commandData.multiplePositionVibrationParam.amplitude[2] = 0;
	trigger.command[SCE_PAD_TRIGGER_EFFECT_PARAM_INDEX_FOR_L2].commandData.multiplePositionVibrationParam.amplitude[3] = 8;
	trigger.command[SCE_PAD_TRIGGER_EFFECT_PARAM_INDEX_FOR_L2].commandData.multiplePositionVibrationParam.amplitude[4] = 0;
	trigger.command[SCE_PAD_TRIGGER_EFFECT_PARAM_INDEX_FOR_L2].commandData.multiplePositionVibrationParam.amplitude[5] = 8;
	trigger.command[SCE_PAD_TRIGGER_EFFECT_PARAM_INDEX_FOR_L2].commandData.multiplePositionVibrationParam.amplitude[6] = 0;
	trigger.command[SCE_PAD_TRIGGER_EFFECT_PARAM_INDEX_FOR_L2].commandData.multiplePositionVibrationParam.amplitude[7] = 8;
	trigger.command[SCE_PAD_TRIGGER_EFFECT_PARAM_INDEX_FOR_L2].commandData.multiplePositionVibrationParam.amplitude[8] = 0;
	trigger.command[SCE_PAD_TRIGGER_EFFECT_PARAM_INDEX_FOR_L2].commandData.multiplePositionVibrationParam.amplitude[9] = 8;
	trigger.command[SCE_PAD_TRIGGER_EFFECT_PARAM_INDEX_FOR_L2].commandData.multiplePositionVibrationParam.frequency = 15;

	trigger.command[SCE_PAD_TRIGGER_EFFECT_PARAM_INDEX_FOR_R2].mode = ScePadTriggerEffectMode::SCE_PAD_TRIGGER_EFFECT_MODE_WEAPON;
	trigger.command[SCE_PAD_TRIGGER_EFFECT_PARAM_INDEX_FOR_R2].commandData.weaponParam.startPosition = 5;
	trigger.command[SCE_PAD_TRIGGER_EFFECT_PARAM_INDEX_FOR_R2].commandData.weaponParam.endPosition = 6;
	trigger.command[SCE_PAD_TRIGGER_EFFECT_PARAM_INDEX_FOR_R2].commandData.weaponParam.strength = 8;

	scePadSetTriggerEffect(handle, &trigger);

	s_ScePadInfo info;
	scePadGetControllerInformation(handle, &info);

	ScePadTriggerEffectParam trigger2 = {};
	trigger2.triggerMask = SCE_PAD_TRIGGER_EFFECT_TRIGGER_MASK_L2;
	trigger2.command[SCE_PAD_TRIGGER_EFFECT_PARAM_INDEX_FOR_L2].mode = ScePadTriggerEffectMode::SCE_PAD_TRIGGER_EFFECT_MODE_WEAPON;
	trigger2.command[SCE_PAD_TRIGGER_EFFECT_PARAM_INDEX_FOR_L2].commandData.weaponParam.startPosition = 2;
	trigger2.command[SCE_PAD_TRIGGER_EFFECT_PARAM_INDEX_FOR_L2].commandData.weaponParam.endPosition = 7;
	trigger2.command[SCE_PAD_TRIGGER_EFFECT_PARAM_INDEX_FOR_L2].commandData.weaponParam.strength = 7;
	//scePadSetTriggerEffect(handle2, &trigger2);

	getchar();

	scePadTerminate();
	return 0;
}
