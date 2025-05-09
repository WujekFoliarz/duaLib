#include "duaLib.h"

bool getHardwareVersion(hid_device* handle, ReportFeatureInVersion& report) {
	if (!handle) return false;

	unsigned char buffer[64] = { };
	buffer[0] = 0x20;
	int res = hid_get_feature_report(handle, buffer, sizeof(buffer));

	if (res > 0) {
		const auto versionReport = *reinterpret_cast<ReportFeatureInVersion*>(buffer);
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
		const auto macReport = *reinterpret_cast<ReportFeatureInMacAll*>(buffer);
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
	USBGetStateData currentInputState = {};
	SetStateData lastOutputState = {};
	SetStateData currentOutputState = {};
	ReportFeatureInVersion versionReport = {};
	std::string macAddress = "";
	std::string systemIdentifier = "";
	const char* lastPath = "";
	const char* id = "";
	uint8_t idSize = 0;
};

controller g_controllers[MAX_CONTROLLER_COUNT] = {};
const char* g_paths[MAX_CONTROLLER_COUNT] = {};
std::atomic<bool> g_threadRunning = false;
std::atomic<bool> g_initialized = false;
std::atomic<bool> g_particularMode = false;
std::thread g_readThread;
std::thread g_watchThread;

int readFunc() {
	while (g_threadRunning) {
		bool allInvalid = true;

		for (int i = 0; i < DEVICE_COUNT; i++) {
			std::lock_guard<std::mutex> guard(g_controllers[i].lock);

			if (g_controllers[i].valid && g_controllers[i].opened) {
				allInvalid = false;

				if (g_controllers[i].deviceType == DUALSENSE && (g_controllers[i].connectionType == HID_API_BUS_USB || g_controllers[i].connectionType == HID_API_BUS_UNKNOWN)) {
					ReportIn01USB  inputData = {};
					ReportOut02    outputData = {};

					inputData.ReportID = 0x01;
					outputData.ReportID = 0x02;
					outputData.State = g_controllers[i].currentOutputState;

					int res = hid_read(
						g_controllers[i].handle,
						reinterpret_cast<unsigned char*>(&inputData),
						sizeof(inputData)
					);

					if (g_controllers[i].failedReadCount >= 254) {
						g_controllers[i].valid = false;
					}

					if (res == -1) {
						g_controllers[i].failedReadCount++;
						continue;
					}
					else if (res > 0) {
						g_controllers[i].failedReadCount = 0;

						if (outputData.State.LedRed != g_controllers[i].lastOutputState.LedRed ||
							outputData.State.LedGreen != g_controllers[i].lastOutputState.LedGreen ||
							outputData.State.LedBlue != g_controllers[i].lastOutputState.LedBlue || 
							g_controllers[i].wasDisconnected) {
							outputData.State.AllowLedColor = true;
						}

						bool oldStyle =
							((g_controllers[i].versionReport.HardwareInfo & 0x00FFFF00) < 0x00000400);
						switch (g_controllers[i].playerIndex) {
							case 1:
								outputData.State.PlayerLight1 = false;
								outputData.State.PlayerLight2 = false;
								outputData.State.PlayerLight3 = true;
								outputData.State.PlayerLight4 = false;
								outputData.State.PlayerLight5 = false;
								break;

							case 2:
								outputData.State.PlayerLight1 = false;
								outputData.State.PlayerLight2 = oldStyle ? true : true;
								outputData.State.PlayerLight3 = false;
								outputData.State.PlayerLight4 = oldStyle ? true : false;
								outputData.State.PlayerLight5 = false;
								break;

							case 3:
								outputData.State.PlayerLight1 = true;
								outputData.State.PlayerLight2 = oldStyle ? false : true;
								outputData.State.PlayerLight3 = oldStyle ? true : false;
								outputData.State.PlayerLight4 = false;
								outputData.State.PlayerLight5 = oldStyle ? true : false;
								break;

							case 4:
								outputData.State.PlayerLight1 = true;
								outputData.State.PlayerLight2 = true;
								outputData.State.PlayerLight3 = false;
								outputData.State.PlayerLight4 = oldStyle ? true : false;
								outputData.State.PlayerLight5 = oldStyle ? true : false;
								break;
						}

						if (outputData.State.PlayerLight1 != g_controllers[i].lastOutputState.PlayerLight1 ||
							outputData.State.PlayerLight2 != g_controllers[i].lastOutputState.PlayerLight2 ||
							outputData.State.PlayerLight3 != g_controllers[i].lastOutputState.PlayerLight3 ||
							outputData.State.PlayerLight4 != g_controllers[i].lastOutputState.PlayerLight4 || 
							g_controllers[i].wasDisconnected) {
							outputData.State.AllowPlayerIndicators = true;
						}

						if (!inputData.State.ButtonMute && g_controllers[i].currentInputState.ButtonMute || g_controllers[i].wasDisconnected) {
							g_controllers[i].isMicMuted = !g_controllers[i].isMicMuted;
							outputData.State.MuteLightMode = g_controllers[i].isMicMuted ? MuteLight::On : MuteLight::Off;
							outputData.State.MicMute = g_controllers[i].isMicMuted;
							outputData.State.AllowMuteLight = true;
						}

						if (compute(reinterpret_cast<uint8_t*>(&outputData.State), sizeof(SetStateData)) !=
							compute(reinterpret_cast<uint8_t*>(&g_controllers[i].lastOutputState), sizeof(SetStateData)) ||
							g_controllers[i].wasDisconnected) {
							res = hid_write(
								g_controllers[i].handle,
								reinterpret_cast<unsigned char*>(&outputData),
								sizeof(outputData)
							);
							if (res > 0) {
								g_controllers[i].lastOutputState = outputData.State;
								g_controllers[i].wasDisconnected = false;
							}
						}

						g_controllers[i].currentInputState = inputData.State;
					}
				}
				else if (g_controllers[i].deviceType == DUALSENSE && g_controllers[i].connectionType == HID_API_BUS_BLUETOOTH) {

					// Implement later

					/*uint32_t crc = compute(outputData.CRC.Buff, sizeof(outputData) - 4);
					outputData.CRC.CRC = crc;
					if (compute(reinterpret_cast<uint8_t*>(&outputData.Data.State), sizeof(SetStateData)) !=
						compute(reinterpret_cast<uint8_t*>(&g_controllers[i].lastOutputState), sizeof(SetStateData)) ||
						g_controllers[i].wasDisconnected) {
						hid_write(
							g_controllers[i].handle,
							reinterpret_cast<unsigned char*>(&outputData),
							sizeof(outputData)
						);
						g_controllers[i].lastOutputState = outputData.Data.State;
						g_controllers[i].wasDisconnected = false;
					}*/

				}
			}
			else if (!g_controllers[i].valid && g_controllers[i].opened) {
				g_controllers[i].lastPath = "";
				g_controllers[i].macAddress = "";
				g_controllers[i].wasDisconnected = true;
			}
		}

		if (allInvalid) {
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
		else {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

	return 0;
}

int watchFunc() {
	while (g_threadRunning) {
		for (int i = 0; i < MAX_CONTROLLER_COUNT; ++i) {
			bool valid;
			{
				std::lock_guard<std::mutex> guard(g_controllers[i].lock);
				valid = isValid(g_controllers[i].handle);
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
						if (getMacAddress(handle, newMac)) {
							bool already = false;
							for (int k = 0; k < MAX_CONTROLLER_COUNT; ++k) {
								std::lock_guard<std::mutex> guard(g_controllers[k].lock);
								if (g_controllers[k].macAddress == newMac) {
									already = true;
									break;
								}
							}

							if (!already) {

								std::lock_guard<std::mutex> guard(g_controllers[i].lock);
								g_controllers[i].handle = handle;
								g_controllers[i].macAddress = newMac;
								g_controllers[i].connectionType = info->bus_type;
								g_controllers[i].valid = true;
								g_controllers[i].failedReadCount = 0;
								g_controllers[i].lastPath = info->path;

								const char* id = {};
								uint8_t size = 0;
								GetID(info->path, &id, reinterpret_cast<int*>(&size));

								g_controllers[i].id = id;
								g_controllers[i].idSize = size;

								uint16_t dev = g_deviceList.devices[j].Device;
								
								if (dev == DUALSENSE_DEVICE_ID) { g_controllers[i].deviceType = DUALSENSE; }							
								else if (dev == DUALSHOCK4_DEVICE_ID || dev == DUALSHOCK4V2_DEVICE_ID) { g_controllers[i].deviceType = DUALSHOCK4; }



								getHardwareVersion(g_controllers[i].handle, g_controllers[i].versionReport);

								if (g_controllers[i].deviceType == DUALSENSE && info->bus_type == HID_API_BUS_USB) {
									ReportOut02 report = {};
									report.State.AllowLedColor = true;
									report.State.AllowPlayerIndicators = true;
									report.State.ResetLights = true;
									hid_write(
										g_controllers[i].handle,
										reinterpret_cast<unsigned char*>(&report),
										sizeof(report)
									);
								}
								else if (g_controllers[i].deviceType == DUALSENSE && info->bus_type == HID_API_BUS_BLUETOOTH) {
									ReportOut31 report = {};

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
										g_controllers[i].handle,
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
						std::lock_guard<std::mutex> guard(g_controllers[i].lock);
						if (!g_controllers[i].macAddress.empty())
							break;
					}
				}
			}
			else {
				std::string cur;
				bool ok;
				{
					std::lock_guard<std::mutex> guard(g_controllers[i].lock);
					ok = getMacAddress(g_controllers[i].handle, cur);
					if (ok) g_controllers[i].macAddress = cur;
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

int scePadOpen(int userID, int, int, void*) {
	if (userID > DEVICE_COUNT + 1 || userID < 0)
		return -1;

	int index = userID - 1;
	bool wasAlreadyOpened = false;
	int lastUnused = 0;
	int firstUnused = -1;
	int occupiedCount = 0;

	for (int i = 0; i < DEVICE_COUNT; i++) {
		std::lock_guard<std::mutex> guard(g_controllers[i].lock);

		if (g_controllers[i].sceHandle == 0 && g_controllers[i].playerIndex != userID) {
			if (firstUnused == -1)
				firstUnused = i;

			lastUnused++;
		}
		else if (g_controllers[i].sceHandle == sizeof(controller) * (occupiedCount + 1) || g_controllers[i].playerIndex == userID) {
			if (occupiedCount > DEVICE_COUNT) {
				wasAlreadyOpened = true;
				return -1;
			}
		}
		else {
			occupiedCount++;
		}
	}

	if (!wasAlreadyOpened) {
		int handle = sizeof(controller) * (firstUnused + 1);
		g_controllers[firstUnused].sceHandle = handle;
		g_controllers[firstUnused].opened = true;
		g_controllers[firstUnused].playerIndex = userID;
		return handle;
	}

	return -1;
}

int scePadSetParticularMode(bool mode) {
	g_particularMode = mode;
	return 0;
}

int scePadReadState(int handle, void* data) {
	for (int i = 0; i < DEVICE_COUNT; i++) {
		std::lock_guard<std::mutex> guard(g_controllers[i].lock);

		if (g_controllers[i].sceHandle == handle) {

			s_ScePadData state;

		#pragma region buttons
			uint32_t bitmaskButtons = {};
			if (g_controllers[i].currentInputState.ButtonCross) bitmaskButtons |= 0x00004000;
			if (g_controllers[i].currentInputState.ButtonCircle) bitmaskButtons |= 0x00002000;
			if (g_controllers[i].currentInputState.ButtonTriangle) bitmaskButtons |= 0x00001000;
			if (g_controllers[i].currentInputState.ButtonSquare) bitmaskButtons |= 0x00008000;

			if (g_controllers[i].currentInputState.ButtonL1) bitmaskButtons |= 0x00000400;
			if (g_controllers[i].currentInputState.ButtonL2) bitmaskButtons |= 0x00000100;
			if (g_controllers[i].currentInputState.ButtonR1) bitmaskButtons |= 0x00000800;
			if (g_controllers[i].currentInputState.ButtonR2) bitmaskButtons |= 0x00000200;

			if (g_controllers[i].currentInputState.ButtonL3) bitmaskButtons |= 0x00000002;
			if (g_controllers[i].currentInputState.ButtonR3) bitmaskButtons |= 0x00000004;

			if (g_controllers[i].currentInputState.DPad == Direction::North) bitmaskButtons |= 0x00000010;
			if (g_controllers[i].currentInputState.DPad == Direction::South) bitmaskButtons |= 0x00000040;
			if (g_controllers[i].currentInputState.DPad == Direction::East) bitmaskButtons |= 0x00000020;
			if (g_controllers[i].currentInputState.DPad == Direction::West) bitmaskButtons |= 0x00000080;

			if (g_controllers[i].currentInputState.ButtonOptions) bitmaskButtons |= 0x00000008;

			if (g_controllers[i].currentInputState.ButtonPad) bitmaskButtons |= 0x00100000;

			if (g_particularMode) {
				if (g_controllers[i].currentInputState.ButtonCreate) bitmaskButtons |= 0x00000001;
				if (g_controllers[i].currentInputState.ButtonHome) bitmaskButtons |= 0x00010000;
			}
		#pragma endregion

		#pragma region sticks
			state.LeftStick.X = g_controllers[i].currentInputState.LeftStickX;
			state.LeftStick.Y = g_controllers[i].currentInputState.LeftStickY;
			state.RightStick.Y = g_controllers[i].currentInputState.RightStickX;
			state.RightStick.Y = g_controllers[i].currentInputState.RightStickY;
		#pragma endregion

		#pragma region triggers
			state.L2_Analog = g_controllers[i].currentInputState.TriggerLeft;
			state.R2_Analog = g_controllers[i].currentInputState.TriggerRight;
		#pragma endregion

		#pragma region gyro
			state.orientation.w = 0; // what is orientation?
			state.orientation.x = 0;
			state.orientation.y = 0;
			state.orientation.z = 0;

			state.acceleration.x = g_controllers[i].currentInputState.AccelerometerX;
			state.acceleration.y = g_controllers[i].currentInputState.AccelerometerY;
			state.acceleration.z = g_controllers[i].currentInputState.AccelerometerZ;

			state.angularVelocity.x = g_controllers[i].currentInputState.AngularVelocityX;
			state.angularVelocity.y = g_controllers[i].currentInputState.AngularVelocityY;
			state.angularVelocity.z = g_controllers[i].currentInputState.AngularVelocityZ;
		#pragma endregion

		#pragma region touchpad
			state.touchData.touchNum = g_controllers[i].currentInputState.touchData.Timestamp;

			state.touchData.touch[0].id = g_controllers[i].currentInputState.touchData.Finger[0].Index;
			state.touchData.touch[0].x = g_controllers[i].currentInputState.touchData.Finger[0].FingerX;
			state.touchData.touch[0].y = g_controllers[i].currentInputState.touchData.Finger[0].FingerY;

			state.touchData.touch[1].id = g_controllers[i].currentInputState.touchData.Finger[1].Index;
			state.touchData.touch[1].x = g_controllers[i].currentInputState.touchData.Finger[1].FingerX;
			state.touchData.touch[1].y = g_controllers[i].currentInputState.touchData.Finger[1].FingerY;
		#pragma endregion

		#pragma region misc
			state.connected = g_controllers[i].valid;
			state.timestamp = g_controllers[i].currentInputState.DeviceTimeStamp;
			state.extUnitData = {};
			state.connectionCount = 0;
			for (int j = 0; j < 12; j++)
				state.deviceUniqueData[j] = {};
			state.deviceUniqueDataLen = sizeof(state.deviceUniqueData);
		#pragma endregion

			std::memcpy(data, &state, sizeof(state));
			return 0;
		}
	}
	return -1;
}

int scePadGetContainerIdInformation(int handle, s_ScePadContainerIdInfo* containerIdInfo) {
#ifdef _WIN32 // Windows only for now
	for (int i = 0; i < DEVICE_COUNT; i++) {
		std::lock_guard<std::mutex> guard(g_controllers[i].lock);
		if (g_controllers[i].sceHandle == handle && g_controllers[i].id != "" && g_controllers[i].idSize != 0) {
			s_ScePadContainerIdInfo info = {};
			info.size = g_controllers[i].idSize;
			strncpy_s(info.id, g_controllers[i].id, sizeof(info.id) - 1);
			info.id[sizeof(info.id) - 1] = '\0';
			*containerIdInfo = info;
			return 0;
		}
	}
	containerIdInfo->size = 0;
	containerIdInfo->id[0] = '\0';
#endif
	return -1;
}

int scePadSetLightBar(int handle, s_SceLightBar* lightbar) {
	for (int i = 0; i < DEVICE_COUNT; i++) {
		std::lock_guard<std::mutex> guard(g_controllers[i].lock);

		if (g_controllers[i].sceHandle == handle) {
			g_controllers[i].currentOutputState.LedRed = lightbar->r;
			g_controllers[i].currentOutputState.LedGreen = lightbar->g;
			g_controllers[i].currentOutputState.LedBlue = lightbar->b;
			return 0;
		}
	}
	return -1;
}

int main() {
	if (scePadInit() != SCE_OK) {
		std::cout << "Failed to initalize!" << std::endl;
	}

	//int handle = scePadOpen(1, NULL, NULL, NULL);
	int handle = scePadOpen(1, 0, 0, 0);
	//int handle2 = scePadOpen(2, NULL, NULL, NULL);

	std::cout << handle << std::endl;

	s_SceLightBar l = {};
	l.g = 255;
	scePadSetLightBar(handle, &l);
	getchar();

	s_ScePadContainerIdInfo info;
	scePadGetContainerIdInformation(handle, &info);
	std::wcout << info.id << std::endl;

	return 0;
}
