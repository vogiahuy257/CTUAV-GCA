/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "GimbalController.h"
#include "GimbalControllerSettings.h"
#include "MAVLinkProtocol.h"
#include "ParameterManager.h"
#include "QGCLoggingCategory.h"
#include "QmlObjectListModel.h"
#include "SettingsManager.h"
#include "Vehicle.h"

// Khai báo một category cho hệ thống logging (dùng để ghi log/debug)
// "qgc.gimbal.gimbalcontroller" sẽ xuất hiện khi in log liên quan đến gimbal controller
QGC_LOGGING_CATEGORY(GimbalControllerLog, "qgc.gimbal.gimbalcontroller")

// Constructor của lớp GimbalController
GimbalController::GimbalController(Vehicle *vehicle)
    : QObject(vehicle)                         // Kế thừa QObject, gắn parent là vehicle
    , _vehicle(vehicle)                        // Lưu lại con trỏ đến Vehicle được truyền vào
    , _gimbals(new QmlObjectListModel(this))   // Tạo list model để quản lý danh sách gimbal, parent là this
{
    // qCDebug(GimbalControllerLog) << Q_FUNC_INFO << this;
    // (dòng debug bị comment, dùng để in tên hàm + con trỏ object khi constructor chạy)

    // Kết nối signal mavlinkMessageReceived từ Vehicle với slot _mavlinkMessageReceived trong GimbalController
    // => Mỗi khi vehicle nhận 1 MAVLink message thì _mavlinkMessageReceived sẽ được gọi để xử lý
    (void) connect(_vehicle, &Vehicle::mavlinkMessageReceived, this, &GimbalController::_mavlinkMessageReceived);

    // Đặt khoảng thời gian (interval) cho timer _rateSenderTimer là 500ms
    _rateSenderTimer.setInterval(500);

    // Kết nối signal timeout của QTimer với slot _rateSenderTimeout
    // => Sau mỗi 500ms, nếu timer chạy, hàm _rateSenderTimeout sẽ được gọi để gửi command giữ quyền điều khiển gimbal
    (void) connect(&_rateSenderTimer, &QTimer::timeout, this, &GimbalController::_rateSenderTimeout);
}


GimbalController::~GimbalController()
{
    // qCDebug(GimbalControllerLog) << Q_FUNC_INFO << this;
}

void GimbalController::setActiveGimbal(Gimbal *gimbal)
{
    if (!gimbal) {
        qCDebug(GimbalControllerLog) << "Set active gimbal: attempted to set a nullptr, returning";
        return;
    }

    if (gimbal != _activeGimbal) {
        qCDebug(GimbalControllerLog) << "Set active gimbal:" << gimbal;
        _activeGimbal = gimbal;
        emit activeGimbalChanged();
    }
}

void GimbalController::_mavlinkMessageReceived(const mavlink_message_t &message)
{
    // Không xử lý message ngay nếu các tham số (parameters) của vehicle chưa sẵn sàng.
    // Nếu làm sớm, quá trình bắt tay (handshake) với gimbal manager có thể thất bại
    // vì lúc đó hệ thống còn đang bận với nhiều luồng khác như tải tham số, tải mission,...
    if (!_vehicle->parameterManager()->parametersReady()) {
        return; // Thoát nếu chưa sẵn sàng
    }

    // Xử lý message theo loại msgid
    switch (message.msgid) {
    case MAVLINK_MSG_ID_HEARTBEAT:
        // Nhận được Heartbeat → gọi hàm xử lý heartbeat của gimbal
        _handleHeartbeat(message);
        break;

    case MAVLINK_MSG_ID_GIMBAL_MANAGER_INFORMATION:
        // Nhận thông tin từ Gimbal Manager (gửi thông số gimbal như device_id, capability,...)
        _handleGimbalManagerInformation(message);
        break;

    case MAVLINK_MSG_ID_GIMBAL_MANAGER_STATUS:
        // Nhận trạng thái hiện tại của Gimbal Manager (ai đang điều khiển, chế độ,...)
        _handleGimbalManagerStatus(message);
        break;

    case MAVLINK_MSG_ID_GIMBAL_DEVICE_ATTITUDE_STATUS:
        // Nhận thông tin attitude (góc pitch, yaw, roll) thực tế từ gimbal
        _handleGimbalDeviceAttitudeStatus(message);
        break;

    default:
        // Nếu message không phải loại trên → bỏ qua, không xử lý
        break;
    }
}

void GimbalController::_handleHeartbeat(const mavlink_message_t &message)
{
    if (!_potentialGimbalManagers.contains(message.compid)) {
        qCDebug(GimbalControllerLog) << "new potential gimbal manager component:" << message.compid;
    }

    PotentialGimbalManager &gimbalManager = _potentialGimbalManagers[message.compid];

    // Note that we are working over potential gimbal managers here, instead of potential gimbals.
    // This is because we address the gimbal manager by compid, but a gimbal device might have an
    // id different than the message compid it comes from. For more information see https://mavlink.io/en/services/gimbal_v2.html
    if (!gimbalManager.receivedInformation && (gimbalManager.requestGimbalManagerInformationRetries > 0)) {
        _requestGimbalInformation(message.compid);
        --gimbalManager.requestGimbalManagerInformationRetries;
    }
}

void GimbalController::_handleGimbalManagerInformation(const mavlink_message_t &message)
{
    mavlink_gimbal_manager_information_t information{};
    mavlink_msg_gimbal_manager_information_decode(&message, &information);

    if (information.gimbal_device_id == 0) {
        qCWarning(GimbalControllerLog) << "_handleGimbalManagerInformation for invalid gimbal device:"
                             << information.gimbal_device_id << ", from component id:" << message.compid;
        return;
    }

    qCDebug(GimbalControllerLog) << "_handleGimbalManagerInformation for gimbal device:" << information.gimbal_device_id << ", component id:" << message.compid;

    const GimbalPairId pairId{message.compid, information.gimbal_device_id};

    auto gimbalIt = _potentialGimbals.find(pairId);
    if (gimbalIt == _potentialGimbals.constEnd()) {
        gimbalIt = _potentialGimbals.insert(pairId, new Gimbal(this));
    }

    Gimbal *const gimbal = gimbalIt.value();
    gimbal->setManagerCompid(message.compid);
    gimbal->setDeviceId(information.gimbal_device_id);

    if (!gimbal->_receivedInformation) {
        qCDebug(GimbalControllerLog) << "gimbal manager with compId:" << message.compid
                           << " is responsible for gimbal device:" << information.gimbal_device_id;
    }

    gimbal->_receivedInformation = true;
    // It is important to flag our potential gimbal manager as well, so we stop requesting gimbal_manger_information message
    PotentialGimbalManager &gimbalManager = _potentialGimbalManagers[message.compid];
    gimbalManager.receivedInformation = true;

    _checkComplete(*gimbal, pairId);
}

void GimbalController::_handleGimbalManagerStatus(const mavlink_message_t &message)
{
    mavlink_gimbal_manager_status_t status{};
    mavlink_msg_gimbal_manager_status_decode(&message, &status);

    // qCDebug(GimbalControllerLog) << "_handleGimbalManagerStatus for gimbal device:" << status.gimbal_device_id << ", component id:" << message.compid;

    if (status.gimbal_device_id == 0) {
        qCDebug(GimbalControllerLog) << "gimbal manager with compId:" << message.compid
        << "reported status of gimbal device id:" << status.gimbal_device_id << "which is not a valid gimbal device id";
        return;
    }

    const GimbalPairId pairId{message.compid, status.gimbal_device_id};

    auto gimbalIt = _potentialGimbals.find(pairId);
    if (gimbalIt == _potentialGimbals.constEnd()) {
        gimbalIt = _potentialGimbals.insert(pairId, new Gimbal(this));
    }

    Gimbal *const gimbal = gimbalIt.value();
    if (gimbal->deviceId()->rawValue().toUInt() == 0) {
        gimbal->setDeviceId(status.gimbal_device_id);
    } else if (gimbal->deviceId()->rawValue().toUInt() != status.gimbal_device_id) {
        qCWarning(GimbalControllerLog) << "conflicting GIMBAL_MANAGER_STATUS.gimbal_device_id:" << status.gimbal_device_id;
    }

    if (gimbal->managerCompid()->rawValue().toUInt() == 0) {
        gimbal->setManagerCompid(message.compid);
    } else if (gimbal->managerCompid()->rawValue().toUInt() != message.compid) {
        qCWarning(GimbalControllerLog) << "conflicting GIMBAL_MANAGER_STATUS compid:" << message.compid;
    }

    // Only log this message once
    if (!gimbal->_receivedStatus) {
        qCDebug(GimbalControllerLog) << "_handleGimbalManagerStatus: gimbal manager with compId" << message.compid
                                     << "is responsible for gimbal device" << status.gimbal_device_id;
    }

    gimbal->_receivedStatus = true;

    const bool haveControl =
        (status.primary_control_sysid == MAVLinkProtocol::instance()->getSystemId()) &&
        (status.primary_control_compid == MAVLinkProtocol::getComponentId());

    const bool othersHaveControl = !haveControl &&
        (status.primary_control_sysid != 0 && status.primary_control_compid != 0);

    if (gimbal->gimbalHaveControl() != haveControl) {
        gimbal->setGimbalHaveControl(haveControl);
    }

    if (gimbal->gimbalOthersHaveControl() != othersHaveControl) {
        gimbal->setGimbalOthersHaveControl(othersHaveControl);
    }

    _checkComplete(*gimbal, pairId);
}

void GimbalController::_handleGimbalDeviceAttitudeStatus(const mavlink_message_t &message)
{
    mavlink_gimbal_device_attitude_status_t attitude_status{};
    mavlink_msg_gimbal_device_attitude_status_decode(&message, &attitude_status);

    GimbalPairId pairId{};

    if (attitude_status.gimbal_device_id == 0) {
        // If gimbal_device_id is 0, we must take the compid of the message
        pairId.deviceId = message.compid;

        // We do a reverse lookup here
        const auto foundGimbal = std::find_if(_potentialGimbals.begin(), _potentialGimbals.end(),
                     [this, pairId](Gimbal *gimbal) { return (gimbal->deviceId()->rawValue().toUInt() == pairId.deviceId); });

        if (foundGimbal == _potentialGimbals.constEnd()) {
            qCDebug(GimbalControllerLog) << "_handleGimbalDeviceAttitudeStatus for unknown device id:"
                               << pairId.deviceId << "from component id:" << message.compid;
            return;
        }

        pairId.managerCompid = foundGimbal.key().managerCompid;
    } else if (attitude_status.gimbal_device_id <= 6) {
         // If the gimbal_device_id field is set to 1-6, we must use this device id instead
        pairId.deviceId = attitude_status.gimbal_device_id;
        pairId.managerCompid = message.compid;
    } else {
        // Otherwise, this is invalid and we don't know how to deal with it.
        qCDebug(GimbalControllerLog) << "_handleGimbalDeviceAttitudeStatus for invalid device id: "
                           << attitude_status.gimbal_device_id << " from component id: " << message.compid;
        return;
    }

    auto gimbalIt = _potentialGimbals.find(pairId);
    if (gimbalIt == _potentialGimbals.end()) {
        gimbalIt = _potentialGimbals.insert(pairId, new Gimbal(this));
    }

    Gimbal *const gimbal = gimbalIt.value();

    gimbal->setRetracted((attitude_status.flags & GIMBAL_DEVICE_FLAGS_RETRACT) > 0);
    gimbal->setYawLock((attitude_status.flags & GIMBAL_DEVICE_FLAGS_YAW_LOCK) > 0);
    gimbal->_neutral = (attitude_status.flags & GIMBAL_DEVICE_FLAGS_NEUTRAL) > 0;

    float roll, pitch, yaw;
    mavlink_quaternion_to_euler(attitude_status.q, &roll, &pitch, &yaw);

    gimbal->setAbsoluteRoll(qRadiansToDegrees(roll));
    gimbal->setAbsolutePitch(qRadiansToDegrees(pitch));

    const bool yaw_in_vehicle_frame = _yawInVehicleFrame(attitude_status.flags);
    if (yaw_in_vehicle_frame) {
        const float bodyYaw = qRadiansToDegrees(yaw);
        float absoluteYaw = bodyYaw + _vehicle->heading()->rawValue().toFloat();
        if (absoluteYaw > 180.0f) {
            absoluteYaw -= 360.0f;
        }

        gimbal->setBodyYaw(bodyYaw);
        gimbal->setAbsoluteYaw(absoluteYaw);

    } else {
        const float absoluteYaw = qRadiansToDegrees(yaw);
        float bodyYaw = absoluteYaw - _vehicle->heading()->rawValue().toFloat();
        if (bodyYaw < -180.0f) {
            bodyYaw += 360.0f;
        }

        gimbal->setBodyYaw(bodyYaw);
        gimbal->setAbsoluteYaw(absoluteYaw);
    }

    gimbal->_receivedAttitude = true;

    _checkComplete(*gimbal, pairId);
}

void GimbalController::_requestGimbalInformation(uint8_t compid)
{
    qCDebug(GimbalControllerLog) << "_requestGimbalInformation(" << compid << ")";

    if (_vehicle) {
        _vehicle->sendMavCommand(compid,
                                 MAV_CMD_REQUEST_MESSAGE,
                                 false /* no error */,
                                 MAVLINK_MSG_ID_GIMBAL_MANAGER_INFORMATION);
    }
}

void GimbalController::_checkComplete(Gimbal &gimbal, GimbalPairId pairId)
{
    if (gimbal._isComplete) {
        // Already complete, nothing to do.
        return;
    }

    if (!gimbal._receivedInformation && gimbal._requestInformationRetries > 0) {
        _requestGimbalInformation(pairId.managerCompid);
        --gimbal._requestInformationRetries;
    }
    // Limit to 1 second between set message interface requests
    static qint64 lastRequestStatusMessage = 0;
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!gimbal._receivedStatus && (gimbal._requestStatusRetries > 0) && (now - lastRequestStatusMessage > 1000)) {
        lastRequestStatusMessage = now;
        _vehicle->sendMavCommand(pairId.managerCompid,
                                 MAV_CMD_SET_MESSAGE_INTERVAL,
                                 false /* no error */,
                                 MAVLINK_MSG_ID_GIMBAL_MANAGER_STATUS,
                                 (gimbal._requestStatusRetries > 2) ? 0 : 5000000); // request default rate, if we don't succeed, last attempt is fixed 0.2 Hz instead
        --gimbal._requestStatusRetries;
        qCDebug(GimbalControllerLog) << "attempt to set GIMBAL_MANAGER_STATUS message at"
                           << (gimbal._requestStatusRetries > 2 ? "default rate" : "0.2 Hz") << "interval for device:"
                           << gimbal.deviceId()->rawValue().toUInt() << "manager compID:" << pairId.managerCompid
                           << ", retries remaining:" << gimbal._requestStatusRetries;
    }

    if (!gimbal._receivedAttitude && (gimbal._requestAttitudeRetries > 0) &&
        gimbal._receivedInformation && (pairId.deviceId != 0)) {
        // We request the attitude directly from the gimbal device component.
        // We can only do that once we have received the gimbal manager information
        // telling us which gimbal device it is responsible for.
        uint8_t gimbalDeviceCompid = pairId.deviceId;
        // If the device ID is 1-6, we need to request the message from the manager itself.
        if (gimbalDeviceCompid <= 6) {
            gimbalDeviceCompid = pairId.managerCompid;
        }
        _vehicle->sendMavCommand(gimbalDeviceCompid,
                                 MAV_CMD_SET_MESSAGE_INTERVAL,
                                 false /* no error */,
                                 MAVLINK_MSG_ID_GIMBAL_DEVICE_ATTITUDE_STATUS,
                                 0 /* request default rate */);

        --gimbal._requestAttitudeRetries;
    }

    if (!gimbal._receivedInformation || !gimbal._receivedStatus || !gimbal._receivedAttitude) {
        // Not complete yet.
        return;
    }

    gimbal._isComplete = true;

    // If there is no current active gimbal, set this one as active
    if (!_activeGimbal) {
        setActiveGimbal(&gimbal);
    }

    _gimbals->append(&gimbal);
    // This is needed for new Gimbals telemetry to be available for the user to show in flyview telemetry panel
    _vehicle->_addFactGroup(&gimbal, QStringLiteral("%1%2%3").arg(_gimbalFactGroupNamePrefix).arg(pairId.managerCompid).arg(pairId.deviceId));
}

bool GimbalController::_tryGetGimbalControl()
{
    if (!_activeGimbal) {
        qCDebug(GimbalControllerLog) << "_tryGetGimbalControl: active gimbal is nullptr, returning";
        return false;
    }

    if (_activeGimbal->gimbalOthersHaveControl()) {
        qCDebug(GimbalControllerLog) << "Others in control, showing popup for user to confirm control..";
        emit showAcquireGimbalControlPopup();
        return false;
    } else if (!_activeGimbal->gimbalHaveControl()) {
        qCDebug(GimbalControllerLog) << "Nobody in control, acquiring control ourselves..";
        acquireGimbalControl();
    }

    return true;
}

bool GimbalController::_yawInVehicleFrame(uint32_t flags)
{
    if ((flags & GIMBAL_DEVICE_FLAGS_YAW_IN_VEHICLE_FRAME) > 0) {
        return true;
    } else if ((flags & GIMBAL_DEVICE_FLAGS_YAW_IN_EARTH_FRAME) > 0) {
        return false;
    } else {
        // For backwards compatibility: if both new flags are 0, yaw lock defines the frame.
        return ((flags & GIMBAL_DEVICE_FLAGS_YAW_LOCK) == 0);
    }
}

void GimbalController::gimbalPitchStart(int direction)
{
    if (!_activeGimbal) {
        qCDebug(GimbalControllerLog) << "gimbalPitchStart: active gimbal is nullptr, returning";
        return;
    }

    const float speed = SettingsManager::instance()->gimbalControllerSettings()->joystickButtonsSpeed()->rawValue().toInt();
    activeGimbal()->setPitchRate(direction * speed);

    sendRate();
}

void GimbalController::gimbalYawStart(int direction)
{
    if (!_activeGimbal) {
        qCDebug(GimbalControllerLog) << "gimbalYawStart: active gimbal is nullptr, returning";
        return;
    }

    const float speed = SettingsManager::instance()->gimbalControllerSettings()->joystickButtonsSpeed()->rawValue().toInt();
    activeGimbal()->setYawRate(direction * speed);
    sendRate();
}

void GimbalController::gimbalPitchStop()
{
    if (!_activeGimbal) {
        qCDebug(GimbalControllerLog) << "gimbalPitchStop: active gimbal is nullptr, returning";
        return;
    }

    activeGimbal()->setPitchRate(0.0f);
    sendRate();
}

void GimbalController::gimbalYawStop()
{
    if (!_activeGimbal) {
        qCDebug(GimbalControllerLog) << "gimbalYawStop: active gimbal is nullptr, returning";
        return;
    }

    activeGimbal()->setYawRate(0.0f);
    sendRate();
}

void GimbalController::centerGimbal()
{
    if (!_activeGimbal) {
        qCDebug(GimbalControllerLog) << "gimbalYawStep: active gimbal is nullptr, returning";
        return;
    }
    sendPitchBodyYaw(0.0, 0.0);
}

// logic gimbal camera xem nó đang gửi lệnh mavlink nào khi click điều khiển gimbal
void GimbalController::gimbalOnScreenControl(float panPct, float tiltPct, bool clickAndPoint, bool clickAndDrag, bool rateControl, bool retract, bool neutral, bool yawlock)
{
    // Pan and tilt comes as +-(0-1)

    if (!_activeGimbal) {
        qCDebug(GimbalControllerLog) << "gimbalOnScreenControl: active gimbal is nullptr, returning";
        return;
    }

    if (clickAndPoint) { // based on FOV
        const float hFov = SettingsManager::instance()->gimbalControllerSettings()->CameraHFov()->rawValue().toFloat();
        const float vFov = SettingsManager::instance()->gimbalControllerSettings()->CameraVFov()->rawValue().toFloat();

        const float panIncDesired = panPct * hFov * 0.5f;
        const float tiltIncDesired = tiltPct * vFov * 0.5f;

        const float panDesired = panIncDesired + _activeGimbal->bodyYaw()->rawValue().toFloat();
        const float tiltDesired = tiltIncDesired + _activeGimbal->absolutePitch()->rawValue().toFloat();

        if (_activeGimbal->yawLock()) {
            sendPitchAbsoluteYaw(tiltDesired, panDesired + _vehicle->heading()->rawValue().toFloat(), false);
        } else {
            sendPitchBodyYaw(tiltDesired, panDesired, false);
        }
    } else if (clickAndDrag) { // based on maximum speed
        // Should send rate commands, but it seems for some reason it is not working on AP side.
        // Pitch works ok but yaw doesn't stop, it keeps like inertia, like if it was buffering the messages.
        // So we do a workaround with angle targets
        const float maxSpeed = SettingsManager::instance()->gimbalControllerSettings()->CameraSlideSpeed()->rawValue().toFloat();

        const float panIncDesired = panPct * maxSpeed * 0.1f;
        const float tiltIncDesired = tiltPct * maxSpeed * 0.1f;

        const float panDesired = panIncDesired + _activeGimbal->bodyYaw()->rawValue().toFloat();
        const float tiltDesired = tiltIncDesired + _activeGimbal->absolutePitch()->rawValue().toFloat();

        
        if (_activeGimbal->yawLock()) {
            // Camera giữ nguyên hướng tuyệt đối trên mặt đất
            sendPitchAbsoluteYaw(tiltDesired, panDesired + _vehicle->heading()->rawValue().toFloat(), false);
        } else {
            // Camera xoay theo drone (giữ cùng góc tương đối so với mũi máy bay)
            sendPitchBodyYaw(tiltDesired, panDesired, false);
        }
    }
}

void GimbalController::sendPitchBodyYaw(float pitch, float yaw, bool showError)
{
    // Kiểm tra xem QGC có quyền điều khiển gimbal hay chưa
    if (!_tryGetGimbalControl()) {
        return; // Nếu chưa có quyền thì thoát, không gửi lệnh
    }

    // Dừng timer gửi lệnh điều khiển theo tốc độ (rate control),
    // vì ở đây ta sẽ gửi lệnh theo góc tuyệt đối
    _rateSenderTimer.stop();

    // Reset trạng thái pitch và yaw rate trong object Gimbal hiện tại
    _activeGimbal->setAbsolutePitch(0.0f); // không còn offset pitch
    _activeGimbal->setYawRate(0.0f);       // dừng mọi lệnh quay liên tục

    // Debug log (đã bị comment) - dùng để kiểm tra giá trị pitch/yaw khi gửi
    // qCDebug(GimbalControllerLog) << "sendPitch: " << pitch << " BodyYaw: " << yaw;

    // Tạo cờ điều khiển (flags) cho lệnh gimbal
    const unsigned flags = GIMBAL_MANAGER_FLAGS_ROLL_LOCK          // Khóa trục Roll
                         | GIMBAL_MANAGER_FLAGS_PITCH_LOCK         // Khóa trục Pitch
                         | GIMBAL_MANAGER_FLAGS_YAW_IN_VEHICLE_FRAME; // Yaw tính theo body frame (thân máy bay)

    // Gửi MAVLink command MAV_CMD_DO_GIMBAL_MANAGER_PITCHYAW tới gimbal manager
    _vehicle->sendMavCommand(
        _activeGimbal->managerCompid()->rawValue().toUInt(),   // compid của gimbal manager
        MAV_CMD_DO_GIMBAL_MANAGER_PITCHYAW,                    // command: điều khiển Pitch/Yaw
        showError,                                             // có hiện lỗi nếu fail không
        pitch,                                                 // góc pitch mong muốn
        yaw,                                                   // góc yaw mong muốn (theo body frame)
        NAN,                                                   // không dùng (roll)
        NAN,                                                   // không dùng (speed khác)
        flags,                                                 // các cờ điều khiển (lock, frame)
        0,                                                     // unused
        _activeGimbal->deviceId()->rawValue().toUInt());       // deviceId của gimbal (nếu có nhiều gimbal)
}


void GimbalController::sendPitchAbsoluteYaw(float pitch, float yaw, bool showError)
{
    // Kiểm tra xem QGC có quyền điều khiển gimbal chưa
    if (!_tryGetGimbalControl()) {
        return; // Nếu chưa có quyền thì thoát
    }

    // Dừng timer gửi lệnh điều khiển theo tốc độ (rate control),
    // vì ở đây ta sẽ gửi lệnh theo góc tuyệt đối
    _rateSenderTimer.stop();

    // Reset trạng thái trong object Gimbal hiện tại
    _activeGimbal->setAbsolutePitch(0.0f); // Xóa offset pitch cũ
    _activeGimbal->setYawRate(0.0f);       // Dừng lệnh yaw theo vận tốc

    // Chuẩn hóa giá trị yaw về khoảng [-180, 180]
    if (yaw > 180.0f) {
        yaw -= 360.0f;
    }

    if (yaw < -180.0f) {
        yaw += 360.0f;
    }

    // Debug log (đã comment) - để in pitch và yaw tuyệt đối
    // qCDebug() << "sendPitch: " << pitch << " absoluteYaw: " << yaw;

    // Tạo cờ điều khiển gimbal
    const unsigned flags = GIMBAL_MANAGER_FLAGS_ROLL_LOCK          // Khóa trục Roll
                         | GIMBAL_MANAGER_FLAGS_PITCH_LOCK         // Khóa trục Pitch
                         | GIMBAL_MANAGER_FLAGS_YAW_LOCK           // Khóa trục Yaw (giữ yaw tuyệt đối)
                         | GIMBAL_MANAGER_FLAGS_YAW_IN_EARTH_FRAME; // Yaw tính theo Earth frame (tọa độ thế giới, không phụ thuộc body frame)

    // Gửi MAVLink command MAV_CMD_DO_GIMBAL_MANAGER_PITCHYAW tới gimbal manager
    _vehicle->sendMavCommand(
        _activeGimbal->managerCompid()->rawValue().toUInt(),   // compid của gimbal manager
        MAV_CMD_DO_GIMBAL_MANAGER_PITCHYAW,                    // command: điều khiển Pitch/Yaw
        showError,                                             // có hiện lỗi nếu fail không
        pitch,                                                 // góc pitch mong muốn
        yaw,                                                   // góc yaw tuyệt đối (so với Earth frame)
        NAN,                                                   // không dùng (roll)
        NAN,                                                   // không dùng (speed khác)
        flags,                                                 // cờ điều khiển (roll/pitch lock, yaw lock, earth frame)
        0,                                                     // unused
        _activeGimbal->deviceId()->rawValue().toUInt());       // deviceId của gimbal (nếu nhiều gimbal)
}


void GimbalController::toggleGimbalRetracted(bool set)
{
    if (!_tryGetGimbalControl()) {
        return;
    }

    uint32_t flags = 0;
    if (set) {
        flags |= GIMBAL_DEVICE_FLAGS_RETRACT;
    } else {
        flags &= ~GIMBAL_DEVICE_FLAGS_RETRACT;
    }

    sendPitchYawFlags(flags);
}

void GimbalController::sendRate()
{
    if (!_tryGetGimbalControl()) {
        return;
    }

    unsigned flags = GIMBAL_MANAGER_FLAGS_ROLL_LOCK | GIMBAL_MANAGER_FLAGS_PITCH_LOCK;

    if (_activeGimbal->yawLock()) {
        flags |= GIMBAL_MANAGER_FLAGS_YAW_LOCK;
    }

    _vehicle->sendMavCommand(
        _activeGimbal->managerCompid()->rawValue().toUInt(),
        MAV_CMD_DO_GIMBAL_MANAGER_PITCHYAW,
        false,
        NAN,
        NAN,
        _activeGimbal->pitchRate(),
        _activeGimbal->yawRate(),
        flags,
        0,
        _activeGimbal->deviceId()->rawValue().toUInt());

    qCDebug(GimbalControllerLog) << "Gimbal rate sent!";

    // Stop timeout if both unset.
    if ((_activeGimbal->pitchRate() == 0.f) && (_activeGimbal->yawRate() == 0.f)) {
        _rateSenderTimer.stop();
    } else {
        _rateSenderTimer.start();
    }
}

void GimbalController::_rateSenderTimeout()
{
    // Send rate again to avoid timeout on autopilot side.
    sendRate();
}

void GimbalController::toggleGimbalYawLock(bool set)
{
    if (!_tryGetGimbalControl()) {
        return;
    }

    // Roll and pitch are usually "locked", so with horizon and not with aircraft.
    uint32_t flags = GIMBAL_DEVICE_FLAGS_ROLL_LOCK | GIMBAL_DEVICE_FLAGS_PITCH_LOCK;
    if (set) {
        flags |= GIMBAL_DEVICE_FLAGS_YAW_LOCK;
    }

    sendPitchYawFlags(flags);
}

void GimbalController::sendPitchYawFlags(uint32_t flags)
{
    const bool yaw_in_vehicle_frame = _yawInVehicleFrame(flags);

    _vehicle->sendMavCommand(
        _activeGimbal->managerCompid()->rawValue().toUInt(),
        MAV_CMD_DO_GIMBAL_MANAGER_PITCHYAW,
        true,
        _activeGimbal->absolutePitch()->rawValue().toFloat(),
        yaw_in_vehicle_frame ? _activeGimbal->bodyYaw()->rawValue().toFloat() : _activeGimbal->absoluteYaw()->rawValue().toFloat(),
        NAN,
        NAN,
        flags,
        0,
        _activeGimbal->deviceId()->rawValue().toUInt());
}

void GimbalController::acquireGimbalControl()
{
    if (!_activeGimbal) {
        qCDebug(GimbalControllerLog) << "acquireGimbalControl: active gimbal is nullptr, returning";
        return;
    }

    _vehicle->sendMavCommand(
        _activeGimbal->managerCompid()->rawValue().toUInt(),
        MAV_CMD_DO_GIMBAL_MANAGER_CONFIGURE,
        true,
        MAVLinkProtocol::instance()->getSystemId(), // Set us in primary control.
        MAVLinkProtocol::getComponentId(), // Set us in primary control
        -1.f, // Leave secondary unchanged
        -1.f, // Leave secondary unchanged
        NAN, // Reserved
        NAN, // Reserved
        _activeGimbal->deviceId()->rawValue().toUInt());
}

void GimbalController::releaseGimbalControl()
{
    if (!_activeGimbal) {
        qCDebug(GimbalControllerLog) << "releaseGimbalControl: active gimbal is nullptr, returning";
        return;
    }

    _vehicle->sendMavCommand(
        _activeGimbal->managerCompid()->rawValue().toUInt(),
        MAV_CMD_DO_GIMBAL_MANAGER_CONFIGURE,
        true,
        -3.f, // Release primary control if we have control
        -3.f, // Release primary control if we have control
        -1.f, // Leave secondary control unchanged
        -1.f, // Leave secondary control unchanged
        NAN, // Reserved
        NAN, // Reserved
        _activeGimbal->deviceId()->rawValue().toUInt());
}
