typedef struct {
	uint8_t r;
	uint8_t g;
	uint8_t b;
} s_SceLightBar;

typedef struct {
	uint8_t X;
	uint8_t Y;
} s_SceStickData;

struct s_SceFQuaternion {
	float x, y, z, w;
};

struct s_SceFVector3 {
	float x, y, z;
};

struct s_ScePadTouch {
	uint16_t x;
	uint16_t y;
	uint8_t id;
	uint8_t reserve[3];
};

struct s_ScePadTouchData {
	uint8_t touchNum;
	uint8_t reserve[3];
	uint32_t reserve1;
	s_ScePadTouch touch[2];
};

struct s_ScePadExtensionUnitData {
	uint32_t extensionUnitId;
	uint8_t reserve[1];
	uint8_t dataLength;
	uint8_t data[10];
};

struct s_ScePadVibrationParam {
	uint8_t largeMotor;
	uint8_t smallMotor;
};

struct s_ScePadVolumeGain {
	uint8_t speakerVolume;
	uint8_t headsetVolume;
	uint8_t padding;
	uint8_t micGain;
};

struct s_ScePadInfo {
	float batteryLevel;
	int touchpadWidth;
	int touchpadHeight;
	uint8_t flag1;
	uint8_t flag2;
	uint8_t state;
	uint8_t isValid;
};

typedef struct {
	uint32_t ExtensionUnitId;
	uint8_t Reserve;
	uint8_t DataLen;
	uint8_t Data[10];
} s_ScePadExtUnitData;

typedef struct {
	uint32_t bitmask_buttons;
	s_SceStickData LeftStick;
	s_SceStickData RightStick;
	uint8_t L2_Analog;
	uint8_t R2_Analog;
	uint16_t padding;
	s_SceFQuaternion orientation;
	s_SceFVector3 acceleration;
	s_SceFVector3 angularVelocity;
	s_ScePadTouchData touchData;
	bool connected;
	uint64_t timestamp;
	s_ScePadExtUnitData extUnitData;
	uint8_t connectionCount;
	uint8_t reserved[2];
	uint8_t deviceUniqueDataLen;
	uint8_t deviceUniqueData[12];

} s_ScePadData;