/*
 * Copyright (C) 2016 The Android Open Source Project
 * Copyright (C) 2019 EPAM Systems Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// #define LOG_NDEBUG 0
#define LOG_TAG "vehicle.xenvm"

#include <android-base/macros.h>
#include <android/log.h>
#include <cutils/properties.h>
#include <log/log.h>
#include <utils/SystemClock.h>

#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "DefaultConfig.h"
#include "VisVehicleHal.h"

namespace android {
namespace hardware {
namespace automotive {
namespace vehicle {
namespace V2_0 {

using namespace impl;

namespace xenvm {

VisVehicleHal::VisVehicleHal(VehiclePropertyStore* propStore)
    : mPropStore(propStore),
      mHvacPowerProps(std::begin(kHvacPowerProperties), std::end(kHvacPowerProperties)),
      mRecurrentTimer(
          std::bind(&VisVehicleHal::onContinuousPropertyTimer, this, std::placeholders::_1)) {
    initStaticConfig();
    mMainSubscriptionId = 0;
    mSubscribed = false;
    mValuesAreDirty = true;
}

VisVehicleHal::~VisVehicleHal() {
    // mVisClient.unsubscribeAll();
    mVisClient.stop();
}

VehicleHal::VehiclePropValuePtr VisVehicleHal::get(const VehiclePropValue& requestedPropValue,
                                                   StatusCode* outStatus) {
    ALOGD("%s [GET]propId: 0x%x area: 0x%x", __func__, requestedPropValue.prop,
          requestedPropValue.areaId);
    std::lock_guard<std::mutex> lock(mLock);

    VehicleAreaProperty prop = {.prop = requestedPropValue.prop, .area = requestedPropValue.areaId};

    auto it = mVPropertyToVisName.find(prop);
    if (it != mVPropertyToVisName.end()) {
        if ((mVisClient.getConnectedState() != epam::ConnState::STATE_CONNECTED) &&
            mValuesAreDirty) {
            *outStatus = StatusCode::TRY_AGAIN;
            ALOGD("[GET] State != STATE_CONNECTED and values are dirty!");
            return nullptr;
        }

        if (mValuesAreDirty && (!fetchAllAndSubscribe())) {
            *outStatus = StatusCode::TRY_AGAIN;
            return nullptr;
        }
    }
    VehiclePropValuePtr v = nullptr;
    auto internalPropValue = mPropStore->readValueOrNull(requestedPropValue);
    if (internalPropValue != nullptr) {
        v = getValuePool()->obtain(*internalPropValue);
    }
    *outStatus = v != nullptr ? StatusCode::OK : StatusCode::INVALID_ARG;
    return v;
}

std::string VisVehicleHal::vehiclePropValueToString(VehiclePropValue val) const {
    switch (getPropType(val.prop)) {
        case VehiclePropertyType::STRING:
            ALOGV("Converting VehiclePropertyType::STRING");
            return val.value.stringValue;
        case VehiclePropertyType::FLOAT:
            ALOGV("Converting VehiclePropertyType::FLOAT");
            return std::to_string(val.value.floatValues[0]);
        case VehiclePropertyType::INT32:
            ALOGV("Converting VehiclePropertyType::INT32");
            return std::to_string(val.value.int32Values[0]);
        case VehiclePropertyType::INT64:
            ALOGV("Converting VehiclePropertyType::INT64");
            return std::to_string(val.value.int64Values[0]);
        case VehiclePropertyType::BOOLEAN:
            ALOGV("Converting VehiclePropertyType::BOOLEAN");
            return std::to_string(val.value.int32Values[0]);
        case VehiclePropertyType::INT32_VEC: {
            ALOGV("Converting VehiclePropertyType::INT32_VEC");
            std::string str = "[";
            for (size_t i = 0; i < val.value.int32Values.size(); ++i) {
                str += std::to_string(val.value.int32Values[i]) + ",";
            }
            str[str.length() - 1] = ']';
            return str;
        }
        case VehiclePropertyType::INT64_VEC: {
            ALOGV("Converting VehiclePropertyType::INT64_VEC");
            std::string str = "[";
            for (size_t i = 0; i < val.value.int64Values.size(); ++i) {
                str += std::to_string(val.value.int64Values[i]) + ",";
            }
            str[str.length() - 1] = ']';
            return str;
        }
        case VehiclePropertyType::FLOAT_VEC: {
            ALOGV("Converting VehiclePropertyType::FLOAT_VEC");
            std::string str = "[";
            for (size_t i = 0; i < val.value.floatValues.size(); ++i) {
                str += std::to_string(val.value.floatValues[i]) + ",";
            }
            str[str.length() - 1] = ']';
            return str;
        }
        case VehiclePropertyType::BYTES:
            ALOGV("Converting VehiclePropertyType::BYTES");
            ALOGE("Conversion from BYTES is unsupported");
            return "";
        case VehiclePropertyType::MIXED:
            ALOGV("Converting VehiclePropertyType::MIXED");
            ALOGE("Conversion to MIXED is unsupported");
            return "";
        default:
            ALOGE("Invalid property type %d", val.prop);
            return "";
    }
    return "";
}

StatusCode VisVehicleHal::set(const VehiclePropValue& propValue) {
    ALOGD("%s [SET]propId: 0x%x area: 0x%x", __func__, propValue.prop, propValue.areaId);
    std::lock_guard<std::mutex> lock(mLock);

    VehicleAreaProperty prop = {.prop = propValue.prop, .area = propValue.areaId};

    auto it = mVPropertyToVisName.find(prop);
    if (it != mVPropertyToVisName.end()) {
        if ((mVisClient.getConnectedState() != epam::ConnState::STATE_CONNECTED) &&
            mValuesAreDirty) {
            ALOGD("%s [SET]propId: 0x%x", __func__, propValue.prop);
            return StatusCode::TRY_AGAIN;
        }

        if (mValuesAreDirty) {
            if (!fetchAllAndSubscribe()) return StatusCode::TRY_AGAIN;
        }
        ALOGD("Found mapped property 0x%x area 0x%x to vis %s", propValue.prop, propValue.areaId,
              it->second.c_str());
        std::stringstream aa;
        aa << "\"" << it->second << "\":" << vehiclePropValueToString(propValue);
        epam::Status st = mVisClient.setPropertySync(aa.str());
        if (st != epam::Status::OK) {
            ALOGE("SET retrned != OK [%d]", st);
            return StatusCode::TRY_AGAIN;
        }
    }
    if (!mPropStore->writeValue(propValue, false)) {
        return StatusCode::INVALID_ARG;
    }

    return StatusCode::OK;
}

std::vector<VehiclePropConfig> VisVehicleHal::listProperties() const {
    return mPropStore->getAllConfigs();
}

StatusCode VisVehicleHal::subscribe(int32_t property, float sampleRate) {
    ALOGI("%s propId: 0x%x, sampleRate: %f", __func__, property, sampleRate);
    fetchAllAndSubscribe();
    if (isContinuousProperty(property)) {
        mRecurrentTimer.registerRecurrentEvent(hertzToNanoseconds(sampleRate), property);
    }
    return StatusCode::OK;
}

StatusCode VisVehicleHal::unsubscribe(int32_t property) {
    ALOGI("%s propId: 0x%x", __func__, property);
    if (isContinuousProperty(property)) {
        mRecurrentTimer.unregisterRecurrentEvent(property);
    }
    return StatusCode::OK;
}

void VisVehicleHal::subscriptionHandler(const epam::CommandResult& result) {
    for (auto& item : result) {
        /* Several vehicle properties may be mapped to one VIS property. Will find & update all of
         * these. */
        auto range = mVisNameToVProperty.equal_range(item.first);
        for (auto it = range.first; it != range.second; ++it) {
            ALOGV("Will convert VIS property %s to vehicle prop", item.first.c_str());
            auto result = mPropStore->readValueOrNull(it->second.prop, it->second.area);
            if (result) {
                auto val = result.get();
                if (jsonToVehicle(item.second, *val)) {
                    if (mPropStore->writeValue(*val, true)) {
                        ALOGV("Value for property %d area=%d|%s updated to %s", it->second.prop,
                              it->second.area, it->first.c_str(),
                              vehiclePropValueToString(*val).c_str());
                        auto v = getValuePool()->obtain(*val);
                        v->timestamp = elapsedRealtimeNano();
                        doHalEvent(std::move(v));
                    } else {
                        ALOGE("Unable to update property %d area=%d|%s", it->second.prop,
                              it->second.area, it->first.c_str());
                    }
                }
            } else {
                ALOGE("Unable to read current value for prop 0x%x area=0x%x from propertystore",
                      it->second.prop, it->second.area);
            }
        }
    }
#if 0
    // Reinject keyevent
    {
        char propValue[PROPERTY_VALUE_MAX] = {};
        property_get("inject.key", propValue, "0");
        if (propValue[0] != '0') {
            auto result = mPropStore->readValueOrNull(toInt(VehicleProperty::HW_KEY_INPUT), 0);
            if (result) {
                auto val = result.get();
                {
                    auto v = getValuePool()->obtain(*val);
                    v->timestamp = elapsedRealtimeNano();
                    doHalEvent(std::move(v));
                }
                ALOGI("Keyevent %d - %d injected for display %d", val->value.int32Values[1],
                      val->value.int32Values[0], val->value.int32Values[2]);
            }
            property_set("inject.key", "0");
        }
    }
#endif
}

void VisVehicleHal::createPropertyMappingsFromConfig() {
    char propValue[PROPERTY_VALUE_MAX] = {};
    property_get("persist.vehicle.prop-mapping", propValue,
                 "/vendor/etc/vehicle/vehicle-mappings.json");
    std::ifstream configFile(propValue);
    if (configFile.is_open()) {
        Json::Value root;
        Json::Reader reader;
        if (reader.parse(configFile, root, false)) {
            if (root.isArray()) {
                for (unsigned int i = 0; i < root.size(); i++) {
                    Json::Value val = root[i];
                    int androidId = val["android-property-id"].asInt();
                    int areaId = val["area-id"].asInt();
                    std::string androidString = val["android-property-id-as-string"].asString();
                    std::string visString = val["vis-property"].asString();
                    std::string description = val["description"].asString();
                    ALOGD("Mapping (%s,0x%x,0x%x) => (%s) [%s]", androidString.c_str(), androidId,
                          areaId, visString.c_str(), description.c_str());

                    VehicleAreaProperty prop = {
                        .prop = androidId,
                        .area = areaId,
                    };
                    auto it = mVPropertyToVisName.find(prop);
                    if (it != mVPropertyToVisName.end()) {
                        mVPropertyToVisName.erase(prop);
                        auto range = mVisNameToVProperty.equal_range(it->second);
                        for (auto iit = range.first; iit != range.second; ++iit) {
                            if (iit->second == prop) {
                                mVisNameToVProperty.erase(iit);
                                break;
                            }
                        }
                    }
                    mVPropertyToVisName.emplace(prop, visString);
                    mVisNameToVProperty.emplace(visString, prop);
                }
            }
        } else {
            ALOGE("Property config parsing failed %s", reader.getFormatedErrorMessages().c_str());
            // abort();
        }

    } else {
        ALOGE("Unable to open property mapping file %s", propValue);
        // abort();
    }
}

void VisVehicleHal::onVisConnectionStatusUpdate(bool connected) {
    std::lock_guard<std::mutex> lock(mLock);
    ALOGI("Received connection state update to %d from VisClient", connected);
    if (!connected) mValuesAreDirty = true;
}

// Parse supported properties list and generate vector of property values to hold current values.
void VisVehicleHal::onCreate() {
    static constexpr bool shouldUpdateStatus = true;

    for (auto& it : kVehicleProperties) {
        VehiclePropConfig cfg = it.config;
        int32_t numAreas = cfg.areaConfigs.size();
        // A global property will have only a single area
        if (isGlobalProp(cfg.prop)) {
            numAreas = 1;
        }

        for (int i = 0; i < numAreas; i++) {
            int32_t curArea;

            if (isGlobalProp(cfg.prop)) {
                curArea = 0;
            } else {
                curArea = cfg.areaConfigs[i].areaId;
            }

            // Create a separate instance for each individual zone
            VehiclePropValue prop = {
                .prop = cfg.prop,
                .areaId = curArea,
            };

            if (it.initialAreaValues.size() > 0) {
                auto valueForAreaIt = it.initialAreaValues.find(curArea);
                if (valueForAreaIt != it.initialAreaValues.end()) {
                    prop.value = valueForAreaIt->second;
                } else {
                    ALOGW("%s failed to get default value for prop 0x%x area 0x%x", __func__,
                          cfg.prop, curArea);
                }
            } else {
                prop.value = it.initialValue;
            }

            if (it.initialAreaToVIS.size() > 0) {
                auto areaToVis = it.initialAreaToVIS.find(curArea);
                if (areaToVis != it.initialAreaToVIS.end()) {
                    ALOGD("Found mapping prop 0x%x area 0x%x to vis %s", cfg.prop, curArea,
                          areaToVis->second.c_str());
                    VehicleAreaProperty prop = {.prop = cfg.prop, .area = curArea};
                    mVPropertyToVisName.emplace(prop, areaToVis->second);
                    mVisNameToVProperty.emplace(areaToVis->second, prop);

                } else {
                    ALOGE("Failed to find prop 0x%x area 0x%x mapping to vis", cfg.prop, curArea);
                }
            }

            mPropStore->writeValue(prop, shouldUpdateStatus);
        }
    }

    std::function<void(bool)> connHandler =
        std::bind(&VisVehicleHal::onVisConnectionStatusUpdate, this, std::placeholders::_1);
    createPropertyMappingsFromConfig();
    mVisClient.registerServerConnectionhandler(connHandler);
    mVisClient.start();
    subscribeToAll();
}

void VisVehicleHal::subscribeToAll() {
    // Subscribe to all
    ALOGV("Will try to subscribe to all VIS properties");
    std::future<epam::WMessageResult> f;
    std::string str(VisClient::kAllTag);

    std::function<void(const epam::CommandResult&)> resultHandler =
        std::bind(&VisVehicleHal::subscriptionHandler, this, std::placeholders::_1);

    epam::Status st = mVisClient.subscribeProperty(str, resultHandler, f);
    ALOGV("Subscribe retrned OK = %d", st == epam::Status::OK);
    if (!f.valid()) {
        ALOGE("Unable to subscribe: future is not valid !");
        return;
    }
    if (f.wait_for(std::chrono::seconds(4)) != std::future_status::ready) {
        ALOGE("Unable to receive result, timeout %d !!!!", 4);
        return;
    }
    epam::WMessageResult sr = f.get();
    if (sr.status == epam::Status::OK) {
        mMainSubscriptionId = sr.subscriptionId;
        ALOGV("Subscribed with id =%zu", mMainSubscriptionId);
        mSubscribed = true;
    }
}

bool VisVehicleHal::fetchAllAndSubscribe() {
    if ((mValuesAreDirty == true) &&
        (mVisClient.getConnectedState() == epam::ConnState::STATE_CONNECTED)) {
        epam::WMessageResult sr;
        epam::Status st = mVisClient.getPropertySync("*", sr);
        if (st != epam::Status::OK) {
            ALOGE("Unable to get * from VIS");
            return false;
        }
        ALOGI("Fetched all from VIS!");
        auto result = sr.commandResult;

        for (auto& item : result) {
            auto it = mVisNameToVProperty.find(item.first);
            if (it != mVisNameToVProperty.end()) {
                ALOGV("Will convert VIS property %s to vehicle prop", item.first.c_str());
                auto result = mPropStore->readValueOrNull(it->second.prop, it->second.area);
                if (result) {
                    auto val = result.get();
                    if (jsonToVehicle(item.second, *val)) {
                        ALOGV("Result converted !");
                        if (mPropStore->writeValue(*val, false)) {
                            ALOGV("Value for property %d area= %d|%s updated", it->second.prop,
                                  it->second.area, it->first.c_str());
                        } else {
                            ALOGE("Unable to update property %d area= %d|%s", it->second.prop,
                                  it->second.area, it->first.c_str());
                        }
                    }
                } else {
                    ALOGE("Unable to read current value for prop 0x%x area 0x%x from store",
                          it->second.prop, it->second.area);
                }
            } else {
                ALOGV("Vis parameter name %s is not mapped to any vehicle property",
                      item.first.c_str());
            }
        }
        subscribeToAll();
        mValuesAreDirty = false;
        return true;
    }
    return false;
}

void VisVehicleHal::initStaticConfig() {
    for (auto&& it = std::begin(kVehicleProperties); it != std::end(kVehicleProperties); ++it) {
        mPropStore->registerProperty(it->config);
    }
}

void VisVehicleHal::onContinuousPropertyTimer(const std::vector<int32_t>& properties) {
    VehiclePropValuePtr v;
    auto& pool = *getValuePool();

    for (int32_t property : properties) {
        if (isContinuousProperty(property)) {
            auto internalPropValue = mPropStore->readValueOrNull(property);
            if (internalPropValue != nullptr) {
                v = pool.obtain(*internalPropValue);
            }
        } else {
            ALOGE("Unexpected onContinuousPropertyTimer for property: 0x%x", property);
        }

        if (v.get()) {
            v->timestamp = elapsedRealtimeNano();
            doHalEvent(std::move(v));
        }
    }
}

bool VisVehicleHal::isContinuousProperty(int32_t propId) const {
    const VehiclePropConfig* config = mPropStore->getConfigOrNull(propId);
    if (config == nullptr) {
        ALOGW("Config not found for property: 0x%x", propId);
        return false;
    }
    return config->changeMode == VehiclePropertyChangeMode::CONTINUOUS;
}

bool VisVehicleHal::jsonToVehicle(const Json::Value& jval, struct VehiclePropValue& val) {
    switch (getPropType(val.prop)) {
        case VehiclePropertyType::STRING:
            ALOGV("Converting VehiclePropertyType::STRING");
            if (!jval.isString()) {
                ALOGE("Can't convert to STRING vehicle property 0x%x", val.prop);
                return false;
            }
            val.value.stringValue = jval.asString();
            break;
        case VehiclePropertyType::FLOAT:
            ALOGV("Converting VehiclePropertyType::FLOAT");
            if (!jval.isConvertibleTo(Json::realValue)) {
                ALOGE("Can't convert to FLOAT vehicle property 0x%x", val.prop);
                return false;
            } else {
                auto v = getValuePool()->obtainFloat(jval.asFloat());
                val.value = v->value;
            }
            break;
        case VehiclePropertyType::INT32:
            ALOGV("Converting VehiclePropertyType::INT32");
            if (!jval.isInt()) {
                ALOGE("Can't convert to FLOAT vehicle property 0x%x", val.prop);
                return false;
            } else {
                auto v = getValuePool()->obtainInt32(jval.asInt());
                val.value = v->value;
            }
            break;
        case VehiclePropertyType::INT64:
            ALOGV("Converting VehiclePropertyType::INT64");
            if (!jval.isInt64()) {
                ALOGE("Can't convert to FLOAT vehicle property 0x%x", val.prop);
                return false;
            } else {
                auto v = getValuePool()->obtainInt64(jval.asInt64());
                val.value = v->value;
            }
            break;
        case VehiclePropertyType::BOOLEAN:
            ALOGV("Converting VehiclePropertyType::BOOLEAN");
            if (jval.isBool()) {
                auto v = getValuePool()->obtainBoolean(jval.asBool());
                val.value = v->value;
            } else if (jval.isInt()) {
                auto v = getValuePool()->obtainBoolean(static_cast<bool>(jval.asInt()));
                val.value = v->value;
            } else if (jval.isInt64()) {
                auto v = getValuePool()->obtainBoolean(static_cast<bool>(jval.asInt64()));
                val.value = v->value;
            } else {
                ALOGE("Can't convert to BOOLEAN vehicle property 0x%x", val.prop);
                return false;
            }
            break;
        case VehiclePropertyType::INT32_VEC:
            ALOGD("Converting VehiclePropertyType::INT32_VEC");
            if (!jval.isArray()) {
                ALOGE("Can't convert to INT32_VEC vehicle property 0x%x", val.prop);
                return false;
            } else {
                if (jval[0].isInt()) {
                    for (unsigned int i = 0; i < jval.size(); i++) {
                        if (val.value.int32Values.size() > i) {
                            val.value.int32Values[i] = jval[i].asInt();
                        } else {
                            ALOGE("Error converting[%d] to INT32_VEC vprop 0x%x size %zu:%u", i,
                                  val.prop, val.value.int32Values.size(), jval.size());
                        }
                    }
                } else {
                    ALOGE("Can't convert to INT32_VEC vehicle property 0x%x", val.prop);
                    return false;
                }
            }
            break;
        case VehiclePropertyType::INT64_VEC:
            ALOGD("Converting VehiclePropertyType::INT64_VEC");
            if (!jval.isArray()) {
                ALOGE("Can't convert to INT64_VEC vehicle property 0x%x", val.prop);
                return false;
            } else {
                if (jval[0].isInt64()) {
                    for (unsigned int i = 0; i < jval.size(); i++) {
                        if (val.value.int64Values.size() > i) {
                            val.value.int64Values[i] = jval[i].asInt64();
                        } else {
                            ALOGE("Error converting[%d] to INT64_VEC vprop 0x%x size %zu:%u", i,
                                  val.prop, val.value.int64Values.size(), jval.size());
                        }
                    }
                } else {
                    ALOGE("Can't convert to INT64_VEC vehicle property 0x%x", val.prop);
                    return false;
                }
            }
            break;
        case VehiclePropertyType::FLOAT_VEC:
            ALOGV("Converting VehiclePropertyType::FLOAT_VEC");
            if (!jval.isArray()) {
                ALOGE("Can't convert to FLOAT_VEC vehicle property 0x%x", val.prop);
                return false;
            } else {
                if (jval[0].isConvertibleTo(Json::realValue)) {
                    for (unsigned int i = 0; i < jval.size(); i++) {
                        if (val.value.floatValues.size() > i) {
                            val.value.floatValues[i] = jval[i].asFloat();
                        } else {
                            ALOGE("Error converting[%d] to FLOAT_VEC vprop 0x%x size %zu:%u", i,
                                  val.prop, val.value.floatValues.size(), jval.size());
                        }
                    }
                } else {
                    ALOGE("Can't convert to FLOAT_VEC vehicle property 0x%x", val.prop);
                    return false;
                }
            }
            break;
        case VehiclePropertyType::BYTES:
            ALOGV("Converting VehiclePropertyType::BYTES");
            ALOGE("Conversion from BYTES is unsupported");
            return false;
            break;
        case VehiclePropertyType::MIXED:
            ALOGV("Converting VehiclePropertyType::MIXED");
            ALOGE("Conversion to MIXED is unsupported");
            return false;
            break;
        default:
            ALOGE("Invalid property type %d", val.prop);
            return false;
            break;
    }
    return true;
}

}  // namespace xenvm
}  // namespace V2_0
}  // namespace vehicle
}  // namespace automotive
}  // namespace hardware
}  // namespace android
