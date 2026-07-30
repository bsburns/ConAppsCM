#pragma once
/*------------------------------------------------------------------
 * SX_common.h
 *
 * Saturn X Common header file
 *
 * July 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */
#include <bit> // Required for std::bit_cast
#include <span>

#include "logger.h"
using namespace my_logger;


#define VERSION "0.1"

enum class SX_ReturnCodes : int {
	NOMINAL = 0,
	INCORRECT_PKT_SIZE = 1,
	BAD_MAGIC = 2,
	BAD_CFG_CRC = 3,
	BAD_ZERO_FIELD = 4
};

// Inline functions
template <typename T>
void serialize16(std::vector<uint8_t>& data, int offset, T value) {
	data[offset++] = (value >> 8) & 0xFF;
	data[offset] = value & 0xFF;
}
template <typename T>
void serialize32(std::vector<uint8_t>& data, int offset, T value) {
	data[offset++] = (value >> 24) & 0xFF;
	data[offset++] = (value >> 16) & 0xFF;
	data[offset++] = (value >> 8) & 0xFF;
	data[offset] = value & 0xFF;
}
template <typename T>
void serialize64(std::vector<uint8_t>& data, int offset, T value) {
	data[offset++] = (value >> 56) & 0xFF;
	data[offset++] = (value >> 48) & 0xFF;
	data[offset++] = (value >> 40) & 0xFF;
	data[offset++] = (value >> 32) & 0xFF;
	data[offset++] = (value >> 24) & 0xFF;
	data[offset++] = (value >> 16) & 0xFF;
	data[offset++] = (value >> 8) & 0xFF;
	data[offset] = value & 0xFF;
}
inline void serializeFloat(std::vector<uint8_t>& data, int offset, float value) {
	std::array<uint8_t, 4> fByteArray = std::bit_cast<std::array<uint8_t, 4>>(value);
	data[offset++] = fByteArray[0];
	data[offset++] = fByteArray[1];
	data[offset++] = fByteArray[2];
	data[offset] = fByteArray[3];
}
inline void serializeDouble(std::vector<uint8_t>& data, int offset, double value) {
	std::array<uint8_t, 8> fByteArray = std::bit_cast<std::array<uint8_t, 8>>(value);
	data[offset++] = fByteArray[0];
	data[offset++] = fByteArray[1];
	data[offset++] = fByteArray[2];
	data[offset++] = fByteArray[3];
	data[offset++] = fByteArray[4];
	data[offset++] = fByteArray[5];
	data[offset++] = fByteArray[6];
	data[offset] = fByteArray[7];
}

template <typename T>
T deserialize16(const std::vector<uint8_t>& data, int offset) {
	return (static_cast<T>(data[offset]) << 8) | static_cast<T>(data[offset + 1]);
}

template <typename T>
T deserialize32(const std::vector<uint8_t>& data, int offset) {
	return (static_cast<T>(data[offset]) << 24) |
		(static_cast<T>(data[offset + 1]) << 16) |
		(static_cast<T>(data[offset + 2]) << 8) |
		static_cast<T>(data[offset + 3]);
}
template <typename T>
T deserialize64(const std::vector<uint8_t>& data, int offset) {
	return (static_cast<T>(data[offset]) << 56) | (static_cast<T>(data[offset + 1]) << 48)
		| (static_cast<T>(data[offset + 2]) << 40) | (static_cast<T>(data[offset + 3]) << 32)
		| (static_cast<T>(data[offset + 4]) << 24) | (static_cast<T>(data[offset + 5]) << 16)
		| (static_cast<T>(data[offset + 6]) << 8) | static_cast<T>(data[offset + 7]);
}

inline float deserializeFloat(const std::vector<uint8_t>& data, int offset) {
	std::array<uint8_t, 4> fByteArray;
	fByteArray[0] = data[offset];
	fByteArray[1] = data[offset + 1];
	fByteArray[2] = data[offset + 2];
	fByteArray[3] = data[offset + 3];
	return std::bit_cast<float>(fByteArray);
}

inline double deserializeDouble(const std::vector<uint8_t>& data, int offset) {
	std::array<uint8_t, 8> dByteArray;
	dByteArray[0] = data[offset];
	dByteArray[1] = data[offset + 1];
	dByteArray[2] = data[offset + 2];
	dByteArray[3] = data[offset + 3];
	dByteArray[4] = data[offset + 4];
	dByteArray[5] = data[offset + 5];
	dByteArray[6] = data[offset + 6];
	dByteArray[7] = data[offset + 7];
	return std::bit_cast<double>(dByteArray);
}

/***********************************************************
*                         STATUS PACKET
************************************************************/
 
// Multicast Address that SX device will send Status messages too
#define SX_DEVICE_STATUS_ADDR "239.26.192.101"
#define SX_DEVICE_STATUS_PORT_U1 "25005"
#define SX_DEVICE_STATUS_PORT_U2 "25006"
#define SX_DEVICE_STATUS_PORT_U3 "25007"
#define SX_DEVICE_STATUS_PORT_U1 "25008"

// Enum indicating the reason a control
// packet was dropped after being received
// at the Plaser Unit.
enum class SX_PacketDropStatus : uint8_t {
	NO_DROP = 0,
	BAD_MESSAGE=1,
	STALE_MESSAGE = 2,
	BAD_CRC = 3,
	BAD_FRAME = 4,
	BAD_ELEMENT = 5,
};

// Enum indicating reason for pending reboot (if any).
enum class SX_RebootReason : uint8_t {
	NO_REBOOT = 0,
	SOFTWARE_UPDATE = 1,
	OVER_TEMPERATURE = 2,
};

// Enum indicating Plaser state
enum class SX_Plaser_State : uint8_t {
	LOW_POWER_MODE = 0,
	IDLE = 1,
	ACTIVE = 2,
};

// Enum indicating Laser Terminal state
enum class SX_LaserTermState : uint8_t {
	IDLE = 0,
	ACTIVE = 1,
	FAULTED = 2,
};

// Enum indicating star tracker state - TBR.
enum class SX_StarTrkState : uint8_t {
	OFF = 0,
	ACTIVE = 1,
	FAULTED = 2,
};

// Enum indicating GNSS state - TBR.
enum class SX_GNSS_State : uint8_t {
	OFF = 0,
	ACTIVE = 1,
	FAULTED = 2,
};

enum class SX_SwUpdateStatus : uint8_t {
	UNKNOWN = 0,
	NO_UPDATE_REQUIRED = 1,
	FAULTED = 2,
	GET_VERSION = 3,
	FETCHING = 4,
	HEALTH_CHECK = 5,
	WRITING = 6,
	POST_CHECK = 7,
	REBOOT_REQUIRED = 8,
	DISABLED = 9,
	OTHER = 10,
};

enum class SX_LB_SelfTestState : uint8_t {
	NOT_RUN= 0,
	IN_PROGRESS = 1,
	SUCCEEDED = 2,
	FAILED = 3,
};

enum class SX_SelfTestBitmask {
	LaserBridgeInitialization = 0,
	LaserBridgeInternalNetwork_CrossStrapNetworkCheck = 1,
	LaserBridgeFRAM_STSAFE_FunctionalCheck = 2,
	StarTrackerFunctionalCheck = 3,
	GPS_FunctionalCheck = 4,
	LaserOpticsModuleHeaterTest = 5,
	LaserBridgeHeaterTest = 6,
	LaserCoarsePointerEncoderCheck = 7,
	LaserEDFA_VOA_test = 8,
	LaserOpticalEfficiencyCheck = 9,
	LaserTerminalD_AxisResistanceSweep = 10,
	CFP2_EDFA_FiberCheck = 11,
	CFP2_TransceiverInitializationAlertCheck = 12,
	LaserTerminalLaunchLockContinuityCheck = 13,
	LaserBridge_SpaceX_TransceiverChecks = 14,
	LaserBridgeHardwareWatchdogCheck = 15,
};

class PacketStatusMessage {
public:
	static constexpr uint32_t MAGIC_VALUE = 288;
	static constexpr uint32_t CFG_CRC = 398683496;

	uint32_t magic = MAGIC_VALUE;
	uint32_t Config_CRC = CFG_CRC;
	uint64_t counter; // Monotonically incrementing value This field’s recommended use is to detect stale packets.
	uint32_t status = 0; //LaserBridge Status
	uint8_t cmds_rx = 0; // Command Packet received 1 if the Plaser Unit has received a Command
						 // Packet from the Space Vehicle in
						 // the last 100ms. 0 otherwise.This rolls
						 // up the following four fields.
	uint8_t cmds_rx_U1 = 0; // Command Packet received via Control Interface 1
							// 1 if the Plaser Unit has received a Command
							// Packet from the Space Vehicle over
							// the Control Interface of Plaser Unit 1 in
							//the last 100ms. 0 otherwise.
	uint8_t cmds_rx_U2 = 0; // Command Packet received via Control Interface 2
							//1 if the Plaser Unit has received a Command
							// Packet from the Space Vehicle over
							// the Control Interface of Plaser Unit 2 in
							// the last 100ms. 0 otherwise.
	uint8_t cmds_rx_U3 = 0; // Command Packet received via Control Interface 3
							// 1 if the Plaser Unit has received a Command
							// Packet from the Space Vehicle over
							// the Control Interface of Plaser Unit 3 in
							// the last 100ms. 0 otherwise.
	uint8_t cmds_rx_U4 = 0; // Command Packet received via Control Interface 4
							// 1 if the Plaser Unit has received a Command
							// Packet from the Space Vehicle over
							// the Control Interface of Plaser Unit 4 in
							// the last 100ms. 0 otherwise.
	SX_PacketDropStatus LastPacketDropStatus = SX_PacketDropStatus::NO_DROP;
	uint8_t Reboot_pending = 0; // 1 if the Plaser Unit will reboot itself
								// shortly. 0 otherwise.
	SX_RebootReason Reboot_reason = SX_RebootReason::NO_REBOOT;
	uint64_t uptime = 0; // Plaser Unit uptime Time this Plaser Unit has been booted
						 //since the last reboot, in nanos.
	SX_Plaser_State PlaserUnitState = SX_Plaser_State::LOW_POWER_MODE;
	SX_LaserTermState LaserTermState = SX_LaserTermState::IDLE;
	SX_StarTrkState StarTrackerState = SX_StarTrkState::OFF;
	
	SX_GNSS_State GNSS_State = SX_GNSS_State::OFF;
	uint16_t TelemetryRequests = 0; // Telemetry downlink requested This counter increments each time the
								    // Plaser Unit requests a telemetry downlink.
	uint16_t NetEntryPktsRx = 0;    // Number of Network Entry Packets received
									// by the Plaser Unit from the Space
									// Vehicle.This counter rolls over to 0 from
									// maximum value.
	uint16_t ValidNetEntryPktsRx = 0; // Number of Network Entry Packets valid Number of received Network Entry Packets
									// that were successfully parsed by the
									// Plaser Unit.This counter rolls over to 0
									// from maximum value.
	uint32_t NetEntryPktHash = 0;   // Network Entry Packet Hash The first 32 bits of the sha256 hash of
									// the software update file fetched, in bigendian
									// format. Initially 0 before a network
									// packet entry packet is received.
	uint64_t NextNetworkOperationalExpectedTimestamp_ns = 0; // GPS time in nanoseconds.Indicates the
									// next expected timestamp at which the
									// Plaser Unit has a network path to ground
									// (see Network operational), or the current
									// time if the Plaser Unit currently expects
									// network connectivity.This timestamp is
									// a prediction and is limited by SpaceX internal
									// look - ahead duration.
	float NextNetworkOperationalExpectedDuration_s = 0.0; // Expected duration for the next contiguous
									// period of Plaser Unit network connectivity
									// to ground(see Next Network Operational
									// Expected Timestamp), or expected
									// remaining duration if the Plaser Unit currently
									// expects network connectivity.This
									// duration is a prediction and is limited by
									// SpaceX internal look - ahead duration.
	float TargetEphemerisStaleness = 0.0; // Indicates how stale(in seconds) the
									// ephemerides are for the currently active
									// Laser Terminal target.This is the difference
									// between the current time and
									// the ephemeris timestamp of the satellite
									// which the terminal is currently targeting,
									// or -1 if the terminal has no target assigned.
	uint8_t PlaserUnitGNCC_Valid = 0; // 1 if navigation information coming from
									// the Plaser Unit should be trusted and used
									// by the Laser Terminal. 0 otherwise.
	uint8_t SpaceVehicleAttValid = 0; // Plaser will indicate here whether its received
									// attitude is valid or not.
	uint8_t SpaceVehicleEphemerisValid = 0; // Plaser will indicate here whether its received
									// ephemeris is valid or not.
	// LaserBridge Status > Software Update	Status
	uint8_t SwUpdateFetchEnabled = 0; // if software update fetch from Space Vehicle
									// is enabled. 0 otherwise.
	uint32_t SwUpdateFetchedBytes = 0; // SpaceVehicle software update fetch bytes Progress meter for software update fetch
									// from the Space Vehicle : number of bytes
									// fetched so far.
	uint8_t SwUpdateEnabled = 0; // Space Vehicle software update enabled 1 if software update via Space Vehicle is
									// enabled. 0 otherwise.
	SX_SwUpdateStatus SwUpdateStatus = SX_SwUpdateStatus::UNKNOWN; // Space Vehicle software update status
									// The Space Vehicle should not take action
									// in response to any of these.This status is
									// purely informational.
	uint64_t SwUpdateFileHash = 0;  // The first 64 bits of the sha256 hash of the
									// software update file fetched, in big - endian
									// format.Initially 0 before a software update
									// fetch has completed.

	//LaserBridge Status > Debug Command Status
	uint16_t DebugPktsRX = 0;		// Number of debug commands received by
									// the Plaser Unit.This counter rolls over to
									// 0 from maximum value.
	uint16_t DebugPktsAccepted = 0; // Number of debug commands accepted by
									// the Plaser Unit.This counter rolls over to
									// 0 from maximum value.
	uint64_t LastDebugPacketStatus = 0; // Possible debug status TBR.
	uint64_t DebugPacketHash = 0; // The first 64 bits of the sha256 hash of
									// the last debug command received, in bigendian
									// format. 0 if no debug command
									// has been received.
	// LaserBridge Status > Hardware Status
	float BusVoltage = 0.0;
	float BusCurrent = 0.0;
	uint8_t BoardHeaterEnabled = 0;
	float BoardHeaterCurrent = 0.0;
	float CoarsePointerVoltage = 0.0;
	float CoarsePointerCurrent = 0.0;
	float CFP2_voltage = 0.0;
	float CFP2_current = 0.0;
	float BoardTemperature = 0.0;
	float LaserBridge_CPU_temperature = 0.0;
	float CoarsePointerController_CPU_temperature = 0.0;
	float CFP2_temperature = 0.0;
	
	// LaserBridge Status > Laser Terminal Status
	float OpticsModuleBusVoltage = 0.0;
	float OpticsModuleBusCurrent = 0.0;
	float OpticsModule_12V_voltage = 0.0;
	float OpticsModule_12V_current = 0.0;
	float CoarsePointerStage1_MotorPosition = 0.0;
	float CoarsePointerStage2_MotorPosition = 0.0;
	float CoarsePointerStage1_MotorVelocity = 0.0;
	float CoarsePointerStage2_MotorVelocity = 0.0;
	float MainController_CPU_temperature = 0.0;
	float LaunchLockDeployCurrent = 0.0;
	float EDFA_CPU_temperature = 0.0;
	float EDFA_HP_pump_temperature = 0.0;
	uint8_t EDFA_HeaterEnabled = 0;
	float EDFA_HeaterCurrent = 0.0;
	uint8_t OpticsModuleHeaterEnabled = 0;
	float OpticsModuleHeaterVoltage = 0.0;
	float OpticsModuleHeaterCurrent = 0.0;
	SX_LB_SelfTestState SelfTestState = SX_LB_SelfTestState::NOT_RUN;
	uint64_t SelfTestResults = 0; // See Self - Test Bitmask section.
		
	//LaserBridge Status > PTP Status
	uint64_t PTP_TimeTransmitterID = 0; // The PTP id of the LaserBridge’s time
							// transmitter clock.
	int64_t PTP_TimeTransmitterOffset_ns = 0; // Estimated offset of the Plaser Unit’s
							// PTP time from the PTP time transmitter
							// clock’s time, in ns.
	uint64_t PTP_LaserBridgeID = 0; // The PTP id of the LaserBridge’s clock.
	uint64_t PTP_Timestamp_ns = 0; // The current time according to the Plaser
							// Unit’s PTP clock, in nanoseconds since
							// TAI epoch.
	// LaserBridge Status > User Data Status 
	// Contains Plaser network status the Space
	// Vehicle may care about.
	uint8_t UD_LowSpeedLinkUp = 0; // Indicates to the Space Vehicle that the
							// User Data Low Speed Ethernet link is up.
							// Values are 1 (up) or 0 (down).
	uint8_t UD_HighSpeedLinkUp = 0; // Indicates to the Space Vehicle that the
							// User Data High Speed QSFP Ethernet
							// link is up.Values are 1 (up) or 0 (down).
	uint8_t UD_HighSpeedOpticalLinkUp = 0; // Indicates to the Space Vehicle that the
							// User Data High Speed Active Optical
							// Transceivers Ethernet links are up.Bit 0 -
							// Lane 0 up, Bit 1 - Lane 1 up, Bit 2 - Lane
							// 2 up, Bit 3 - Lane 3 up.
	uint8_t UD_CrossStrap1Up = 0; // 1 up Some Plaser System configurations may
							// cross - strap LaserBridges.Values are 1
							// (up) or 0 (down).
	uint8_t UD_CrossStrap2Up = 0; // Some Plaser System configurations may
							// cross - strap LaserBridges.Values are 1
							// (up) or 0 (down).
	uint8_t UD_CrossStrap3Up = 0; // Some Plaser System configurations may
							// cross - strap LaserBridges.Values are 1
							// (up) or 0 (down).
	uint64_t UD_PktsRx = 0; // Number of user data packets received
							// from Space Vehicle.
	uint64_t UD_PktsTX = 0; // Number of user data packets sent to the
							// Space Vehicle.
	uint64_t UD_PktsRxDrop = 0; // Number of user data RX frames dropped.
	uint8_t NetworkOperational = 0; // 1 if this Plaser Unit has a network path to
							// ground. 0 otherwise.
	float PoP_ping_RTT_ms = -1.0; // Round trip ping time to a SpaceX Point
							// of Presence, in ms.This value is - 1.0 if
							// there is no connection to a PoP.
	float UD_OpticalLane0_BER = 0.0; // Lane 0 Bit Error Rate (BER)
							// Defaults to 0 if the Plaser System configuration
							// does not have a User Data Optical Lane 0.
	float UD_OpticalLane0_RSSI = 0.0; // Received Signal Strength Indicator (RSSI)
							// Defaults to 0 if the Plaser System configuration
							// does not have a User Data Optical Lane 0.
	uint8_t UD_OpticalLane0_LOS = 0; // Loss Of Signal status (LOS)
							// Defaults to 0 if the Plaser System configuration
							// does not have a User Data Optical Lane 0.
	float UD_OpticalLane1_BER = 0.0; // Lane 1 Bit Error Rate (BER)
							// Defaults to 0 if the Plaser System configuration
							// does not have a User Data Optical Lane 1.
	float UD_OpticalLane1_RSSI = 0.0; // Received Signal Strength Indicator (RSSI)
							// Defaults to 0 if the Plaser System configuration
							// does not have a User Data Optical Lane 1.
	uint8_t UD_OpticalLane1_LOS = 0; // Loss Of Signal status (LOS)
							// Defaults to 0 if the Plaser System configuration
							// does not have a User Data Optical Lane 1.
	float UD_OpticalLane2_BER = 0.0; // Lane 2 Bit Error Rate (BER)
							// Defaults to 0 if the Plaser System configuration
							// does not have a User Data Optical Lane 2.
	float UD_OpticalLane2_RSSI = 0.0; // Received Signal Strength Indicator (RSSI)
							// Defaults to 0 if the Plaser System configuration
							// does not have a User Data Optical Lane 2.
	uint8_t UD_OpticalLane2_LOS = 0; // Loss Of Signal status (LOS)
							// Defaults to 0 if the Plaser System configuration
							// does not have a User Data Optical Lane 2.
	float UD_OpticalLane3_BER = 0.0; // Lane 3 Bit Error Rate (BER)
							// Defaults to 0 if the Plaser System configuration
							// does not have a User Data Optical Lane 3.
	float UD_OpticalLane3_RSSI = 0.0; // Received Signal Strength Indicator (RSSI)
							// Defaults to 0 if the Plaser System configuration
							// does not have a User Data Optical Lane 3.
	uint8_t UD_OpticalLane3_LOS = 0; // Loss Of Signal status (LOS)
							// Defaults to 0 if the Plaser System configuration
							// does not have a User Data Optical Lane 3.

		
	// LaserBridge Status > GNC Polarity Feedback
	// Contains Plaser GNC polarity feedback
	// targets for host vehicle to validate
	// mounted placements.
	double ECI_PT_x_m = 0.0; // Component of Plaser Unit current ECI
							// pointing target position(m)
	double ECI_PT_y_m = 0.0; // Component of Plaser Unit current ECI
							// pointing target position(m)
	double ECI_PT_z_m = 0.0; // Component of Plaser Unit current ECI
							// pointing target position(m)
	double NormUnit_PT_x = 0.0; // Component of Plaser Unit normalized
							// pointing target in Plaser Frame(unit vector)
	double NormUnit_PT_y = 0.0; // Component of Plaser Unit normalized
							// pointing target in Plaser Frame(unit vector)
	double NormUnit_PT_z = 0.0; // Component of Plaser Unit normalized
							// pointing target in Plaser Frame(unit vector)

	PacketStatusMessage() {}
	PacketStatusMessage(const std::vector<uint8_t>& data) {
		deserialize(data);
	}

	std::vector<uint8_t> serialize() const {
		std::vector<uint8_t> data(Size()); // Status Message is 371 bytes
		// Serialize fields into byte vector (big-endian)
		serialize32(data, 0, magic);
		serialize32(data, 4, Config_CRC);
		serialize64(data, 8, counter);
		serialize32(data, 16, 0);
		data[20] = cmds_rx;
		data[21] = cmds_rx_U1;
		data[22] = cmds_rx_U2;
		data[23] = cmds_rx_U3;
		data[24] = cmds_rx_U4;

		data[25] = static_cast<uint8_t>(LastPacketDropStatus);
		data[26] = Reboot_pending;
		data[27] = static_cast<uint8_t>(Reboot_reason);
		serialize64(data, 28, uptime);
		data[36] = static_cast<uint8_t>(PlaserUnitState);
		data[37] = static_cast<uint8_t>(LaserTermState);
		data[38] = static_cast<uint8_t>(StarTrackerState);
		data[39] = static_cast<uint8_t>(GNSS_State);
		serialize16(data, 40, TelemetryRequests);
		serialize16(data, 42, NetEntryPktsRx);
		serialize16(data, 44, ValidNetEntryPktsRx);

		serialize32(data, 46, NetEntryPktHash);
		serialize64(data, 50, NextNetworkOperationalExpectedTimestamp_ns);
		serializeFloat(data, 58, NextNetworkOperationalExpectedDuration_s);
		serializeFloat(data, 62, TargetEphemerisStaleness);

		data[66] = PlaserUnitGNCC_Valid;
		data[67] = SpaceVehicleAttValid;
		data[68] = SpaceVehicleEphemerisValid;
		data[69] = SwUpdateFetchEnabled;

		serialize32(data, 70, SwUpdateFetchedBytes);
		data[74] = SwUpdateEnabled;
		data[75] = static_cast<uint8_t>(SwUpdateStatus);

		serialize64(data, 76, SwUpdateFileHash);

		serialize16(data, 84, DebugPktsRX);
		serialize16(data, 86, DebugPktsAccepted);
		serialize64(data, 88, LastDebugPacketStatus);
		serialize64(data, 96, DebugPacketHash);
		serializeFloat(data, 104, BusVoltage);
		serializeFloat(data, 108, BusCurrent);
		data[112] = BoardHeaterEnabled;
		serializeFloat(data, 113, BoardHeaterCurrent);
		serializeFloat(data, 117, CoarsePointerVoltage);
		serializeFloat(data, 121, CoarsePointerCurrent);
		serializeFloat(data, 125, CFP2_voltage);
		serializeFloat(data, 129, CFP2_current);
		serializeFloat(data, 133, BoardTemperature);
		serializeFloat(data, 137, LaserBridge_CPU_temperature);
		serializeFloat(data, 141, CoarsePointerController_CPU_temperature);
		serializeFloat(data, 145, CFP2_temperature);
		serializeFloat(data, 149, OpticsModuleBusVoltage);
		serializeFloat(data, 153, OpticsModuleBusCurrent);
		serializeFloat(data, 157, OpticsModule_12V_voltage);
		serializeFloat(data, 161, OpticsModule_12V_current);
		serializeFloat(data, 165, CoarsePointerStage1_MotorPosition);
		serializeFloat(data, 169, CoarsePointerStage2_MotorPosition);
		serializeFloat(data, 173, CoarsePointerStage1_MotorVelocity);
		serializeFloat(data, 177, CoarsePointerStage2_MotorVelocity);
		serializeFloat(data, 181, MainController_CPU_temperature);
		serializeFloat(data, 185, LaunchLockDeployCurrent);
		serializeFloat(data, 189, EDFA_CPU_temperature);
		serializeFloat(data, 193, EDFA_HP_pump_temperature);
		data[197] = EDFA_HeaterEnabled;
		serializeFloat(data, 198, EDFA_HeaterCurrent);
		data[202] = OpticsModuleHeaterEnabled;
		serializeFloat(data, 203, OpticsModuleHeaterVoltage);
		serializeFloat(data, 207, OpticsModuleHeaterCurrent);
		data[211] = static_cast<uint8_t>(SelfTestState);
		serialize64(data, 212, SelfTestResults);
		
		serialize64(data, 220, PTP_TimeTransmitterID);
		serialize64(data, 228, PTP_TimeTransmitterOffset_ns);
		serialize64(data, 236, PTP_LaserBridgeID);
		serialize64(data, 244, PTP_Timestamp_ns);

		data[252] = UD_LowSpeedLinkUp;
		data[253] = UD_HighSpeedLinkUp;
		data[254] = UD_HighSpeedOpticalLinkUp;
		data[255] = UD_CrossStrap1Up;
		data[256] = UD_CrossStrap2Up;
		data[257] = UD_CrossStrap3Up;
		serialize64(data, 258, UD_PktsRx);
		serialize64(data, 266, UD_PktsTX);
		serialize64(data, 274, UD_PktsRxDrop);
		data[282] = NetworkOperational;
		serializeFloat(data, 283, PoP_ping_RTT_ms);
		serializeFloat(data, 287, UD_OpticalLane0_BER);
		serializeFloat(data, 291, UD_OpticalLane0_RSSI);
		data[295] = UD_OpticalLane0_LOS;
		serializeFloat(data, 296, UD_OpticalLane1_BER);
		serializeFloat(data, 300, UD_OpticalLane1_RSSI);
		data[304] = UD_OpticalLane1_LOS;

		serializeFloat(data, 305, UD_OpticalLane2_BER);
		serializeFloat(data, 309, UD_OpticalLane2_RSSI);
		data[313] = UD_OpticalLane2_LOS;

		serializeFloat(data, 314, UD_OpticalLane3_BER);
		serializeFloat(data, 318, UD_OpticalLane3_RSSI);
		data[322] = UD_OpticalLane3_LOS;

		serializeDouble(data, 323, ECI_PT_x_m);
		serializeDouble(data, 331, ECI_PT_y_m);
		serializeDouble(data, 339, ECI_PT_z_m);
		serializeDouble(data, 347, NormUnit_PT_x);
		serializeDouble(data, 355, NormUnit_PT_y);
		serializeDouble(data, 363, NormUnit_PT_z);

		return data;
	}

	SX_ReturnCodes deserialize(const std::vector<uint8_t>& data) {
		if (data.size() < Size()) {
			LOG(LoggerVerbosity::ERR, "Data too short for SX Command Message");
			return SX_ReturnCodes::INCORRECT_PKT_SIZE;
		}
		magic = deserialize32<uint32_t>(data, 0);
		if (magic != MAGIC_VALUE) {
			LOG(LoggerVerbosity::ERR, "SX Command Message Magic Value doesn't match: rx="
				+ std::to_string(magic)
				+ " should be " + std::to_string(MAGIC_VALUE));
			return SX_ReturnCodes::BAD_MAGIC;
		}

		Config_CRC = deserialize32<uint32_t>(data, 4);
		if (Config_CRC != CFG_CRC) {
			LOG(LoggerVerbosity::ERR, "SX Command Message Config CRC Value doesn't match: rx="
				+ std::to_string(Config_CRC)
				+ " should be " + std::to_string(CFG_CRC));
			return SX_ReturnCodes::BAD_CFG_CRC;
		}
		counter = deserialize64<uint64_t>(data, 8);
		uint32_t zero = deserialize32<uint32_t>(data, 16);
		if (zero != 0) {
			LOG(LoggerVerbosity::ERR, "SX Command Message Zero Value field is not zero: rx="
				+ std::to_string(zero)
			);
			return SX_ReturnCodes::BAD_ZERO_FIELD;
		}
		cmds_rx = data[20];
		cmds_rx_U1 = data[21];
		cmds_rx_U2 = data[22];
		cmds_rx_U3 = data[23];
		cmds_rx_U4 = data[24];
		LastPacketDropStatus = static_cast<SX_PacketDropStatus>(data[25]);
		Reboot_pending = data[26];
		Reboot_reason = static_cast<SX_RebootReason>(data[27]);
		uptime = deserialize64<uint64_t>(data, 28);
		PlaserUnitState = static_cast<SX_Plaser_State>(data[36]);
		LaserTermState = static_cast<SX_LaserTermState>(data[37]);
		StarTrackerState = static_cast<SX_StarTrkState>(data[38]);
		GNSS_State = static_cast<SX_GNSS_State>(data[39]);
		TelemetryRequests = deserialize16<uint16_t>(data, 40);
		NetEntryPktsRx = deserialize16<uint16_t>(data, 42);
		ValidNetEntryPktsRx = deserialize16<uint16_t>(data, 44);
		NetEntryPktHash = deserialize32<uint16_t>(data, 46 );
		NextNetworkOperationalExpectedTimestamp_ns = deserialize64<uint64_t>(data, 50);
		NextNetworkOperationalExpectedDuration_s = deserializeFloat(data, 58);
		TargetEphemerisStaleness = deserializeFloat(data, 62);
		PlaserUnitGNCC_Valid = data[66];
		SpaceVehicleAttValid = data[67];
		SpaceVehicleEphemerisValid = data[68];
		SwUpdateFetchEnabled = data[69];
		SwUpdateFetchedBytes = deserialize32<uint32_t>(data, 70);
		SwUpdateEnabled = data[74];
		SwUpdateStatus = static_cast<SX_SwUpdateStatus>(data[75]);
		SwUpdateFileHash = deserialize64<uint64_t>(data, 76);
		DebugPktsRX = deserialize16<uint16_t>(data, 84);
		DebugPktsAccepted = deserialize16<uint16_t>(data, 86);
		LastDebugPacketStatus = deserialize64<uint64_t>(data, 88);
		DebugPacketHash = deserialize64<uint64_t>(data, 96);
		BusVoltage = deserializeFloat(data, 104);
		BusCurrent = deserializeFloat(data, 108);
		BoardHeaterEnabled = data[112];
		BoardHeaterCurrent = deserializeFloat(data, 113);
		CoarsePointerVoltage = deserializeFloat(data, 117);
		CoarsePointerCurrent = deserializeFloat(data, 121);
		CFP2_voltage = deserializeFloat(data, 125);
		CFP2_current = deserializeFloat(data, 129);
		BoardTemperature = deserializeFloat(data, 133);
		LaserBridge_CPU_temperature = deserializeFloat(data, 137);
		CoarsePointerController_CPU_temperature = deserializeFloat(data, 141);
		CFP2_temperature = deserializeFloat(data, 145);
		OpticsModuleBusVoltage = deserializeFloat(data, 149);
		OpticsModuleBusCurrent = deserializeFloat(data, 153);
		OpticsModule_12V_voltage = deserializeFloat(data, 157);
		OpticsModule_12V_current = deserializeFloat(data, 161);
		CoarsePointerStage1_MotorPosition = deserializeFloat(data, 165);
		CoarsePointerStage2_MotorPosition = deserializeFloat(data, 169);
		CoarsePointerStage1_MotorVelocity = deserializeFloat(data, 173);
		CoarsePointerStage2_MotorVelocity = deserializeFloat(data, 177);
		MainController_CPU_temperature = deserializeFloat(data, 181);
		LaunchLockDeployCurrent = deserializeFloat(data, 185);
		EDFA_CPU_temperature = deserializeFloat(data, 189);
		EDFA_HP_pump_temperature = deserializeFloat(data, 193);
		EDFA_HeaterEnabled = data[197];
		EDFA_HeaterCurrent = deserializeFloat(data, 198);
		OpticsModuleHeaterEnabled = data[202];
		OpticsModuleHeaterVoltage = deserializeFloat(data, 203);
		OpticsModuleHeaterCurrent = deserializeFloat(data, 207);
		SelfTestState = static_cast<SX_LB_SelfTestState>(data[211]);
		SelfTestResults = deserialize64<uint64_t>(data, 212);
		PTP_TimeTransmitterID = deserialize64<uint64_t>(data, 220);
		PTP_TimeTransmitterOffset_ns = deserialize64<int64_t>(data, 228);
		PTP_LaserBridgeID = deserialize64<uint64_t>(data, 236);
		PTP_Timestamp_ns = deserialize64<uint64_t>(data, 244);
		UD_LowSpeedLinkUp = data[252];
		UD_HighSpeedLinkUp = data[253];
		UD_HighSpeedOpticalLinkUp = data[254];
		UD_CrossStrap1Up = data[255];
		UD_CrossStrap2Up = data[256];
		UD_CrossStrap3Up = data[257];
		UD_PktsRx = deserialize64<uint64_t>(data, 258);
		UD_PktsTX = deserialize64<uint64_t>(data, 266);
		UD_PktsRxDrop = deserialize64<uint64_t>(data, 274);
		NetworkOperational = data[282];
		PoP_ping_RTT_ms = deserializeFloat(data, 283);
		UD_OpticalLane0_BER = deserializeFloat(data, 287);
		UD_OpticalLane0_RSSI = deserializeFloat(data, 291);
		UD_OpticalLane0_LOS = data[295];
		UD_OpticalLane1_BER = deserializeFloat(data, 296);
		UD_OpticalLane1_RSSI = deserializeFloat(data, 300);
		UD_OpticalLane1_LOS = data[304];
		UD_OpticalLane2_BER = deserializeFloat(data, 305);
		UD_OpticalLane2_RSSI = deserializeFloat(data, 309);
		UD_OpticalLane2_LOS = data[313];
		UD_OpticalLane3_BER = deserializeFloat(data, 314);
		UD_OpticalLane3_RSSI = deserializeFloat(data, 318);
		UD_OpticalLane3_LOS = data[322];
		ECI_PT_x_m = deserializeDouble(data, 323);
		ECI_PT_y_m = deserializeDouble(data, 331);
		ECI_PT_z_m = deserializeDouble(data, 339);
		NormUnit_PT_x = deserializeDouble(data, 347);
		NormUnit_PT_y = deserializeDouble(data, 355);
		NormUnit_PT_z = deserializeDouble(data, 363);
		return SX_ReturnCodes::NOMINAL;
	}

	uint32_t Size() const { return 371; }

	std::string to_string() const {
		std::string str = "CmdMsg={";
		str += "counter=" + std::to_string(counter);
		str += ", CmdRx=" + std::to_string(cmds_rx);
		str += ", LastDrop=" + std::string(magic_enum::enum_name(LastPacketDropStatus));
		str += ", uptime=" + std::to_string(uptime);
		str += ", PlaserState=" + std::string(magic_enum::enum_name(PlaserUnitState));
		str += ", LaserTermState=" + std::string(magic_enum::enum_name(LaserTermState));
		str += ", StarTrackerState=" + std::string(magic_enum::enum_name(StarTrackerState));
		str += ", GNSS_State=" + std::string(magic_enum::enum_name(GNSS_State));
		str += ", NetworkPktsRx=" + std::to_string(NetEntryPktsRx);
		str += "}";
		return str;
	}
};


/***********************************************************
*                        TELEMETRY PACKET
* Both Real-Time and Stored telemetry packets are opaque 
* to the SV.  They are at most 1458 bytes in size.
************************************************************/


// Multicast Address that SX device will send Telemetry messages too
#define SX_DEVICE_TLM_RT_ADDR "239.26.192.102"
#define SX_DEVICE_TLM_RT_PORT "10017"
#define SX_DEVICE_TLM_ST_ADDR "239.26.192.103"
#define SX_DEVICE_TLM_ST_PORT "10017"


/***********************************************************
*                        COMMAND PACKET
************************************************************/
// Multicast Address that SX device will send Telemetry messages too
#define SX_DEVICE_CMD_ADDR "239.26.192.100"
#define SX_DEVICE_CMD_PORT "25001"


enum class SX_PlaserOperationalMode : uint8_t {
	LOW_POWER = 0, // Plaser Unit powered to survival temperature.
	IDLE = 1, // Plaser Unit powered to operational temperature.
	ACTIVE = 2, // Plaser Unit attempts to acquire laser links.
	DEPLOY = 3, // Plaser Unit releases	laser terminal launch lock.
	SELF_TEST = 4, // Hardware health check - out.
	EMI_TEST = 5, // Hardware EMI test mode.
	GNC_POLARITY_TEST = 6, // Plaser Unit GNC Polarity test mode.
};


class PacketCommandMessage {
public:
	static constexpr uint32_t MAGIC_VALUE = 288;
	static constexpr uint32_t CFG_CRC = 230396551;
	
	uint32_t magic = MAGIC_VALUE;
	uint32_t Config_CRC = CFG_CRC;
	uint64_t counter; // Monotonically incrementing value This field’s recommended use is to detect stale packets.
	SX_PlaserOperationalMode Plaser_1_ModeRequest = SX_PlaserOperationalMode::LOW_POWER; //	Requested Plaser Unit 1 Operatonal Mode
	uint8_t Plaser_1_TLM_Req = 0; // TEST ONLY - Must be set to 0 in flight.
								// On the rising edge of this field(when
								// set to 1 from 0), force Plaser Unit 1
								// to increment the Telemetry downlink
								// requested flag in the Status Packet by 1.
	uint8_t Plaser_1_FetchSwUpd = 0; // TEST ONLY - Must be set to 0 in flight.
								// When set to 1, force Plaser Unit 1 to fetch
								// a software update from the SpaceVehicle.
	uint8_t Plaser_1_UD_TestMode = 0; // TEST ONLY - Must be set to 0 in flight.
								// While set to 1, Plaser Unit 1 will not route
								// User Data out its laser.The alternative
								// User Data output location is determined
								// at test time by the test hardware configuration.
	uint8_t Plaser_1_SignalIntegrityTestMode = 0; // TEST ONLY - Must be set to 0 in flight.
								// While set to 1, PlaserUnit 1 will excercise
								// signal integrity testing.
	
	// Plaser Unit 2 Commands These values should be set to 0 for Plaser
	// Systems without a Plaser Unit 2.
	SX_PlaserOperationalMode Plaser_2_ModeRequest = SX_PlaserOperationalMode::LOW_POWER; //	Requested Plaser Unit 2 Operatonal Mode
	uint8_t Plaser_2_TLM_Req = 0; // TEST ONLY - Must be set to 0 in flight.
								// On the rising edge of this field(when
								// set to 1 from 0), force Plaser Unit 2
								// to increment the Telemetry downlink
								// requested flag in the Status Packet by 1.
	uint8_t Plaser_2_FetchSwUpd = 0; // TEST ONLY - Must be set to 0 in flight.
								// When set to 1, force Plaser Unit 1 to fetch
								// a software update from the SpaceVehicle.
	uint8_t Plaser_2_UD_TestMode = 0; // TEST ONLY - Must be set to 0 in flight.
								// While set to 1, Plaser Unit 2 will not route
								// User Data out its laser.The alternative
								// User Data output location is determined
								// at test time by the test hardware configuration.
	uint8_t Plaser_2_SignalIntegrityTestMode = 0; // TEST ONLY - Must be set to 0 in flight.
								// While set to 1, PlaserUnit 2 will excercise
								// signal integrity testing.


	// Plaser Unit 3 Commands These values should be set to 0 for Plaser
	// Systems without a Plaser Unit 3.
	SX_PlaserOperationalMode Plaser_3_ModeRequest = SX_PlaserOperationalMode::LOW_POWER; //	Requested Plaser Unit 3 Operatonal Mode
	uint8_t Plaser_3_TLM_Req = 0; // TEST ONLY - Must be set to 0 in flight.
								// On the rising edge of this field(when
								// set to 1 from 0), force Plaser Unit 3
								// to increment the Telemetry downlink
								// requested flag in the Status Packet by 1.
	uint8_t Plaser_3_FetchSwUpd = 0; // TEST ONLY - Must be set to 0 in flight.
								// When set to 1, force Plaser Unit 1 to fetch
								// a software update from the SpaceVehicle.
	uint8_t Plaser_3_UD_TestMode = 0; // TEST ONLY - Must be set to 0 in flight.
								// While set to 1, Plaser Unit 3 will not route
								// User Data out its laser.The alternative
								// User Data output location is determined
								// at test time by the test hardware configuration.
	uint8_t Plaser_3_SignalIntegrityTestMode = 0; // TEST ONLY - Must be set to 0 in flight.
								// While set to 1, PlaserUnit 3 will excercise
								// signal integrity testing.

	// Plaser Unit 4 Commands These values should be set to 0 for Plaser
	// Systems without a Plaser Unit 4.
	SX_PlaserOperationalMode Plaser_4_ModeRequest = SX_PlaserOperationalMode::LOW_POWER; //	Requested Plaser Unit 4 Operatonal Mode
	uint8_t Plaser_4_TLM_Req = 0; // TEST ONLY - Must be set to 0 in flight.
								// On the rising edge of this field(when
								// set to 1 from 0), force Plaser Unit 4
								// to increment the Telemetry downlink
								// requested flag in the Status Packet by 1.
	uint8_t Plaser_4_FetchSwUpd = 0; // TEST ONLY - Must be set to 0 in flight.
								// When set to 1, force Plaser Unit 1 to fetch
								// a software update from the SpaceVehicle.
	uint8_t Plaser_4_UD_TestMode = 0; // TEST ONLY - Must be set to 0 in flight.
								// While set to 1, Plaser Unit 3 will not route
								// User Data out its laser.The alternative
								// User Data output location is determined
								// at test time by the test hardware configuration.
	uint8_t Plaser_4_SignalIntegrityTestMode = 0; // TEST ONLY - Must be set to 0 in flight.
								// While set to 1, PlaserUnit 3 will excercise
								// signal integrity testing.

	PacketCommandMessage() {}
	PacketCommandMessage(const std::vector<uint8_t>& data) {
		deserialize(data);
	}

	std::vector<uint8_t> serialize() const {
		std::vector<uint8_t> data(Size()); // Command Message is 40 bytes
		// Serialize fields into byte vector (big-endian)
		serialize32(data, 0, magic);
		serialize32(data, 4, Config_CRC);
		serialize64(data, 8, counter);
		serialize32(data, 16, 0);
		data[20] = static_cast<uint8_t>(Plaser_1_ModeRequest);
		data[21] = Plaser_1_TLM_Req;
		data[22] = Plaser_1_FetchSwUpd;
		data[23] = Plaser_1_UD_TestMode;
		data[24] = Plaser_1_SignalIntegrityTestMode;

		data[25] = static_cast<uint8_t>(Plaser_2_ModeRequest);
		data[26] = Plaser_2_TLM_Req;
		data[27] = Plaser_2_FetchSwUpd;
		data[28] = Plaser_2_UD_TestMode;
		data[29] = Plaser_2_SignalIntegrityTestMode;

		data[30] = static_cast<uint8_t>(Plaser_3_ModeRequest);
		data[31] = Plaser_3_TLM_Req;
		data[32] = Plaser_3_FetchSwUpd;
		data[33] = Plaser_3_UD_TestMode;
		data[34] = Plaser_3_SignalIntegrityTestMode;

		data[35] = static_cast<uint8_t>(Plaser_4_ModeRequest);
		data[36] = Plaser_4_TLM_Req;
		data[37] = Plaser_4_FetchSwUpd;
		data[38] = Plaser_4_UD_TestMode;
		data[39] = Plaser_4_SignalIntegrityTestMode;

		return data;
	}

	SX_ReturnCodes deserialize(const std::vector<uint8_t>& data) {
		if (data.size() < Size()) {
			LOG(LoggerVerbosity::ERR, "Data too short for SX Command Message");
			return SX_ReturnCodes::INCORRECT_PKT_SIZE;
		}
		magic = deserialize32<uint32_t>(data, 0);
		if (magic != MAGIC_VALUE) {
			LOG(LoggerVerbosity::ERR, "SX Command Message Magic Value doesn't match: rx="
				+ std::to_string(magic)
				+ " should be " + std::to_string(MAGIC_VALUE));
			return SX_ReturnCodes::BAD_MAGIC;
		}

		Config_CRC = deserialize32<uint32_t>(data, 4);
		if (Config_CRC != CFG_CRC) {
			LOG(LoggerVerbosity::ERR, "SX Command Message Config CRC Value doesn't match: rx="
				+ std::to_string(Config_CRC)
				+ " should be " + std::to_string(CFG_CRC));
			return SX_ReturnCodes::BAD_CFG_CRC;
		}
		counter = deserialize64<uint64_t>(data, 8);
		uint32_t zero = deserialize32<uint32_t>(data, 16);
		if (zero != 0) {
			LOG(LoggerVerbosity::ERR, "SX Command Message Zero Value field is not zero: rx="
				+ std::to_string(zero)
			);
			return SX_ReturnCodes::BAD_ZERO_FIELD;
		}
		Plaser_1_ModeRequest = static_cast<SX_PlaserOperationalMode>(data[20]);
		Plaser_1_TLM_Req = data[21];
		Plaser_1_FetchSwUpd = data[22];
		Plaser_1_UD_TestMode = data[23];
		Plaser_1_SignalIntegrityTestMode = data[24];

		Plaser_2_ModeRequest = static_cast<SX_PlaserOperationalMode>(data[25]);
		Plaser_2_TLM_Req = data[26];
		Plaser_2_FetchSwUpd = data[27];
		Plaser_2_UD_TestMode = data[28];
		Plaser_2_SignalIntegrityTestMode = data[29];

		Plaser_3_ModeRequest = static_cast<SX_PlaserOperationalMode>(data[30]);
		Plaser_3_TLM_Req = data[31];
		Plaser_3_FetchSwUpd = data[32];
		Plaser_3_UD_TestMode = data[33];
		Plaser_3_SignalIntegrityTestMode = data[34];

		Plaser_4_ModeRequest = static_cast<SX_PlaserOperationalMode>(data[35]);
		Plaser_4_TLM_Req = data[36];
		Plaser_4_FetchSwUpd = data[37];
		Plaser_4_UD_TestMode = data[38];
		Plaser_4_SignalIntegrityTestMode = data[39];
		return SX_ReturnCodes::NOMINAL;
	}

	uint32_t Size() const { return 40; }

	std::string to_string() const {
		std::string str = "CmdMsg={";
		str += "counter=" + std::to_string(counter);
		str += ", Plaser1={";
		str += "mode=" + std::string(magic_enum::enum_name(Plaser_1_ModeRequest));
		str += ", TLM=" + std::to_string(Plaser_1_TLM_Req);
		str += ", SW_UPD=" + std::to_string(Plaser_1_FetchSwUpd);
		str += ", UD_TM=" + std::to_string(Plaser_1_UD_TestMode);
		str += ", SI=" + std::to_string(Plaser_1_SignalIntegrityTestMode);
		str += "},";
		str += "}";
		return str;
	}
};