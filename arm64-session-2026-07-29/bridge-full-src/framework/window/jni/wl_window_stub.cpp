/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "zidl/window_stub.h"
#include <vector>
#include "ipc_skeleton.h"
#include <key_event.h>
#include "pointer_event.h"
#include "window_manager_hilog.h"
#include <transaction/rs_transaction.h>

namespace OHOS {
namespace Rosen {
namespace {
constexpr HiviewDFX::HiLogLabel LABEL = {LOG_CORE, HILOG_DOMAIN_WINDOW, "WindowStub"};
}

int WindowStub::OnRemoteRequest(uint32_t code, MessageParcel& data, MessageParcel& reply, MessageOption& option)
{
    if (staticDestroyMonitor_.IsDestroyed()) {
        WLOGFE("Main thread finished, static data has been destroyed");
        return ERR_INVALID_STATE;
    }
    if (data.ReadInterfaceToken() != GetDescriptor()) {
        WLOGFE("InterfaceToken check failed");
        return ERR_TRANSACTION_FAILED;
    }
    WindowMessage msgId = static_cast<WindowMessage>(code);
    switch (msgId) {
        case WindowMessage::TRANS_ID_UPDATE_WINDOW_RECT: {
            int32_t posX = 0;
            int32_t posY = 0;
            uint32_t width = 0;
            uint32_t height = 0;
            if (!data.ReadInt32(posX) || !data.ReadInt32(posY) || !data.ReadUint32(width) || !data.ReadUint32(height)) {
                TLOGE(WmsLogTag::WMS_LAYOUT, "read rect failed");
                return ERR_INVALID_DATA;
            }
            Rect rect { posX, posY, width, height };
            bool decoStatus = false;
            if (!data.ReadBool(decoStatus)) {
                TLOGE(WmsLogTag::WMS_LAYOUT, "read decoStatus failed");
                return ERR_INVALID_DATA;
            }
            uint32_t changeReason = 0;
            if (!data.ReadUint32(changeReason)) {
                TLOGE(WmsLogTag::WMS_LAYOUT, "read changeReason failed");
                return ERR_INVALID_DATA;
            }
            if (changeReason < static_cast<uint32_t>(WindowSizeChangeReason::UNDEFINED) ||
                changeReason > static_cast<uint32_t>(WindowSizeChangeReason::END)) {
                TLOGE(WmsLogTag::WMS_LAYOUT, "Unknown reason");
                return ERR_INVALID_DATA;
            }
            WindowSizeChangeReason reason = static_cast<WindowSizeChangeReason>(changeReason);
            bool hasRSTransaction = false;
            if (!data.ReadBool(hasRSTransaction)) {
                TLOGE(WmsLogTag::WMS_LAYOUT, "read hasRSTransaction failed");
                return ERR_INVALID_DATA;
            }
            if (hasRSTransaction) {
                auto rsTransaction = data.ReadParcelable<RSTransaction>();
                if (!rsTransaction) {
                    WLOGFE("RSTransaction unMarsh failed");
                    return -1;
                }
                std::shared_ptr<RSTransaction> transaction(rsTransaction);
                UpdateWindowRect(rect, decoStatus, reason, transaction);
            } else {
                UpdateWindowRect(rect, decoStatus, reason);
            }
            break;
        }
        case WindowMessage::TRANS_ID_UPDATE_WINDOW_MODE: {
            uint32_t windowMode = 0;
            if (!data.ReadUint32(windowMode)) {
                TLOGE(WmsLogTag::WMS_LAYOUT, "read windowMode failed");
                return ERR_INVALID_DATA;
            }
            if (windowMode < static_cast<uint32_t>(WindowMode::WINDOW_MODE_UNDEFINED) ||
                windowMode > static_cast<uint32_t>(WindowMode::WINDOW_MODE_PIP)) {
                TLOGE(WmsLogTag::WMS_LAYOUT, "invalid windowMode: %{public}d", windowMode);
                return ERR_INVALID_DATA;
            }
            WindowMode mode = static_cast<WindowMode>(windowMode);
            UpdateWindowMode(mode);
            break;
        }
        case WindowMessage::TRANS_ID_UPDATE_MODE_SUPPORT_INFO: {
            uint32_t windowModeSupportType = 0;
            if (!data.ReadUint32(windowModeSupportType)) {
                TLOGE(WmsLogTag::WMS_LAYOUT, "read windowModeSupportType failed");
                return ERR_INVALID_DATA;
            }
            UpdateWindowModeSupportType(windowModeSupportType);
            break;
        }
        case WindowMessage::TRANS_ID_UPDATE_FOCUS_STATUS: {
            bool focused = data.ReadBool();
            UpdateFocusStatus(focused);
            break;
        }
        case WindowMessage::TRANS_ID_UPDATE_AVOID_AREA: {
            sptr<AvoidArea> avoidArea = data.ReadStrongParcelable<AvoidArea>();
            if (avoidArea == nullptr) {
                return ERR_INVALID_DATA;
            }
            uint32_t type = 0;
            if (!data.ReadUint32(type) ||
                type >= static_cast<uint32_t>(AvoidAreaType::TYPE_END)) {
                return ERR_INVALID_DATA;
            }
            UpdateAvoidArea(avoidArea, static_cast<AvoidAreaType>(type));
            break;
        }
        case WindowMessage::TRANS_ID_UPDATE_WINDOW_STATE: {
            uint32_t state = 0;
            if (!data.ReadUint32(state)) {
                TLOGE(WmsLogTag::DEFAULT, "read state error");
                return ERR_INVALID_DATA;
            }
            UpdateWindowState(static_cast<WindowState>(state));
            break;
        }
        case WindowMessage::TRANS_ID_UPDATE_DRAG_EVENT: {
            PointInfo point = {0, 0};
            if (!data.ReadInt32(point.x) || !data.ReadInt32(point.y)) {
                return ERR_INVALID_DATA;
            }
            uint32_t eventType = 0;
            if (!data.ReadUint32(eventType) || eventType > static_cast<uint32_t>(DragEvent::DRAG_EVENT_END)) {
                return ERR_INVALID_DATA;
            }
            DragEvent event = static_cast<DragEvent>(eventType);
            UpdateWindowDragInfo(point, event);
            break;
        }
        case WindowMessage::TRANS_ID_UPDATE_DISPLAY_ID: {
            uint64_t from = 0;
            uint64_t to = 0;
            if (!data.ReadUint64(from) || !data.ReadUint64(to)) {
                TLOGE(WmsLogTag::DEFAULT, "read display id error");
                return ERR_INVALID_DATA;
            }
            UpdateDisplayId(from, to);
            break;
        }
        case WindowMessage::TRANS_ID_UPDATE_OCCUPIED_AREA: {
            sptr<OccupiedAreaChangeInfo> info = data.ReadParcelable<OccupiedAreaChangeInfo>();
            if (info == nullptr) {
                WLOGFE("OccupiedAreaChangeInfo is null");
                return ERR_INVALID_DATA;
            }
            std::map<AvoidAreaType, AvoidArea> avoidAreas = {};
            int res = ParseAvoidAreas(data, avoidAreas);
            if (res != ERR_NONE) {
                TLOGE(WmsLogTag::WMS_KEYBOARD, "Parse avoid areas failed, code: %{public}d", res);
                return res;
            }
            bool hasRSTransaction = data.ReadBool();
            if (hasRSTransaction) {
                auto rsTransaction = data.ReadParcelable<RSTransaction>();
                if (!rsTransaction) {
                    WLOGFE("RSTransaction unMarsh failed");
                    return ERR_INVALID_DATA;
                }
                std::shared_ptr<RSTransaction> transaction(rsTransaction);
                UpdateOccupiedAreaChangeInfo(info, avoidAreas, transaction);
            } else {
                UpdateOccupiedAreaChangeInfo(info, avoidAreas);
            }
            break;
        }
        case WindowMessage::TRANS_ID_UPDATE_OCCUPIED_AREA_AND_RECT: {
            sptr<OccupiedAreaChangeInfo> info = data.ReadParcelable<OccupiedAreaChangeInfo>();
            if (info == nullptr) {
                TLOGE(WmsLogTag::WMS_KEYBOARD, "OccupiedAreaChangeInfo is null");
                return ERR_INVALID_DATA;
            }
            std::map<AvoidAreaType, AvoidArea> avoidAreas = {};
            int res = ParseAvoidAreas(data, avoidAreas);
            if (res != ERR_NONE) {
                TLOGE(WmsLogTag::WMS_KEYBOARD, "Parse avoid areas failed, code: %{public}d", res);
                return res;
            }
            int32_t posX = 0;
            int32_t posY = 0;
            uint32_t width = 0;
            uint32_t height = 0;
            if (!data.ReadInt32(posX) || !data.ReadInt32(posY) ||
                !data.ReadUint32(width) || !data.ReadUint32(height)) {
                TLOGE(WmsLogTag::WMS_KEYBOARD, "Rect value read failed.");
                return ERR_INVALID_DATA;
            }
            struct Rect rect { posX, posY, width, height };
            bool hasRSTransaction = false;
            if (!data.ReadBool(hasRSTransaction)) {
                TLOGE(WmsLogTag::WMS_KEYBOARD, "hasRSTransaction value read failed.");
                return ERR_INVALID_DATA;
            }
            if (hasRSTransaction) {
                auto rsTransaction = data.ReadParcelable<RSTransaction>();
                if (!rsTransaction) {
                    TLOGE(WmsLogTag::WMS_KEYBOARD, "RSTransaction unMarsh failed");
                    return ERR_INVALID_DATA;
                }
                std::shared_ptr<RSTransaction> transaction(rsTransaction);
                UpdateOccupiedAreaAndRect(info, rect, avoidAreas, transaction);
            } else {
                UpdateOccupiedAreaAndRect(info, rect, avoidAreas);
            }
            break;
        }
        case WindowMessage::TRANS_ID_UPDATE_ACTIVE_STATUS: {
            bool isActive = data.ReadBool();
            UpdateActiveStatus(isActive);
            break;
        }
        case WindowMessage::TRANS_ID_GET_WINDOW_PROPERTY: {
            auto property = GetWindowProperty();
            reply.WriteParcelable(property.GetRefPtr());
            break;
        }
        case WindowMessage::TRANS_ID_NOTIFY_OUTSIDE_PRESSED: {
            NotifyTouchOutside();
            break;
        }
        case WindowMessage::TRANS_ID_NOTIFY_SCREEN_SHOT: {
            NotifyScreenshot();
            break;
        }
        case WindowMessage::TRANS_ID_NOTIFY_SCREEN_SHOT_APP_EVENT: {
            int32_t screenshotEventType = static_cast<int32_t>(ScreenshotEventType::END);
            if (!data.ReadInt32(screenshotEventType)) {
                TLOGE(WmsLogTag::WMS_ATTRIBUTE, "screenshot event type read failed.");
                return ERR_INVALID_DATA;
            }
            NotifyScreenshotAppEvent(static_cast<ScreenshotEventType>(screenshotEventType));
            break;
        }
        case WindowMessage::TRANS_ID_NOTIFY_DESTROY: {
            NotifyDestroy();
            break;
        }
        case WindowMessage::TRANS_ID_NOTIFY_FOREGROUND: {
            NotifyForeground();
            break;
        }
        case WindowMessage::TRANS_ID_NOTIFY_BACKGROUND: {
            NotifyBackground();
            break;
        }
        case WindowMessage::TRANS_ID_DUMP_INFO: {
            std::vector<std::string> params;
            if (!data.ReadStringVector(&params)) {
                WLOGFE("Fail to read params");
                return ERR_INVALID_DATA;
            }
            DumpInfo(params);
            break;
        }
        case WindowMessage::TRANS_ID_NOTIFY_CLIENT_POINT_UP: {
            auto pointerEvent = MMI::PointerEvent::Create();
            if (!pointerEvent || !pointerEvent->ReadFromParcel(data)) {
                WLOGFE("Read Pointer Event failed");
                return ERR_INVALID_DATA;
            }
            NotifyWindowClientPointUp(pointerEvent);
            break;
        }
        case WindowMessage::TRANS_ID_UPDATE_ZOOM_TRANSFORM: {
            Transform trans;
            trans.Unmarshalling(data);
            bool isDisplayZoomOn = data.ReadBool();
            UpdateZoomTransform(trans, isDisplayZoomOn);
            break;
        }
        case WindowMessage::TRANS_ID_RESTORE_SPLIT_WINDOW_MODE: {
            uint32_t splitWindowMode = 0;
            if (!data.ReadUint32(splitWindowMode)) {
                TLOGE(WmsLogTag::WMS_LAYOUT, "read splitWindowMode failed");
                return ERR_INVALID_DATA;
            }
            RestoreSplitWindowMode(splitWindowMode);
            break;
        }
        case WindowMessage::TRANS_ID_CONSUME_KEY_EVENT: {
            auto event = MMI::KeyEvent::Create();
            if (!event || !event->ReadFromParcel(data)) {
                WLOGFE("Read Pointer Event failed");
                return ERR_INVALID_DATA;
            }
            ConsumeKeyEvent(event);
            break;
        }
        case WindowMessage::TRANS_ID_NOTIFY_FOREGROUND_INTERACTIVE_STATUS: {
            bool interactive = data.ReadBool();
            NotifyForegroundInteractiveStatus(interactive);
            break;
        }
        case WindowMessage::TRANS_ID_NOTIFY_MMI_SERVICE_ONLINE: {
            uint32_t winId = data.ReadUint32();
            NotifyMMIServiceOnline(winId);
            break;
        }
        default:
            WLOGFW("unknown transaction code %{public}d", code);
            return IPCObjectStub::OnRemoteRequest(code, data, reply, option);
    }
    return ERR_NONE;
}

int WindowStub::ParseAvoidAreas(MessageParcel& data, std::map<AvoidAreaType, AvoidArea>& avoidAreas)
{
    uint32_t size = 0;
    if (!data.ReadUint32(size)) {
        TLOGE(WmsLogTag::WMS_KEYBOARD, "Read avoid area size failed");
        return ERR_INVALID_VALUE;
    }
    constexpr uint32_t AVOID_AREA_TYPE_MAX_SIZE = 100;
    if (size > AVOID_AREA_TYPE_MAX_SIZE) {
        TLOGE(WmsLogTag::WMS_KEYBOARD, "Avoid area size: %{public}d is invalid", size);
        return ERR_INVALID_VALUE;
    }
    for (uint32_t i = 0; i < size; i++) {
        uint32_t type = static_cast<uint32_t>(AvoidAreaType::TYPE_START);
        if (!data.ReadUint32(type)) {
            TLOGE(WmsLogTag::WMS_KEYBOARD, "Read avoid area size failed");
            return ERR_INVALID_VALUE;
        }
        if (type < static_cast<uint32_t>(AvoidAreaType::TYPE_START) ||
            type >= static_cast<uint32_t>(AvoidAreaType::TYPE_END)) {
            TLOGE(WmsLogTag::WMS_KEYBOARD, "invalid avoid area type: %{public}d", type);
            return ERR_INVALID_VALUE;
        }
        sptr<AvoidArea> area = data.ReadParcelable<AvoidArea>();
        if (area == nullptr) {
            TLOGE(WmsLogTag::WMS_KEYBOARD, "Read avoid area failed");
            return ERR_INVALID_VALUE;
        }
        avoidAreas[static_cast<AvoidAreaType>(type)] = *area;
    }
    return ERR_NONE;
}
} // namespace Rosen
} // namespace OHOS
