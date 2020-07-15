/*
 * Copyright (C) 2016 The Android Open Source Project
 * Copyright (C) 2018 EPAM Systems Inc.
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

#ifndef COMMON_INCLUDE_VHAL_V2_0_VISVEHICLEHAL_H_
#define COMMON_INCLUDE_VHAL_V2_0_VISVEHICLEHAL_H_

#include <vhal_v2_0/RecurrentTimer.h>
#include <vhal_v2_0/VehicleHal.h>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "VisClient.h"
#include "vhal_v2_0/VehiclePropertyStore.h"

using epam::VisClient;

namespace android {
namespace hardware {
namespace automotive {
namespace vehicle {
namespace V2_0 {
namespace xenvm {

class VisVehicleHal : public VehicleHal {
    friend VehiclePropertyStore;
    using VehicleAreaProperty = VehiclePropertyStore::RecordId;

 public:
    explicit VisVehicleHal(VehiclePropertyStore* propStore);
    ~VisVehicleHal();

    //  Methods from VehicleHal
    void onCreate() override;
    std::vector<VehiclePropConfig> listProperties();
    VehiclePropValuePtr get(const VehiclePropValue& requestedPropValue,
                            StatusCode* outStatus) override;
    StatusCode set(const VehiclePropValue& propValue) override;
    StatusCode subscribe(int32_t property, float sampleRate) override;
    StatusCode unsubscribe(int32_t property) override;

    VehicleHal::VehiclePropValuePtr createApPowerStateReq(VehicleApPowerStateReq state, int32_t param);

 private:
    void initStaticConfig();
    void createPropertyMappingsFromConfig();
    void subscribeToAll();
    bool jsonToVehicle(const Json::Value& jval, struct VehiclePropValue& val);
    void onContinuousPropertyTimer(const std::vector<int32_t>& properties);
    bool isContinuousProperty(int32_t propId) const;
    constexpr std::chrono::nanoseconds hertzToNanoseconds(float hz) const {
        return std::chrono::nanoseconds(static_cast<int64_t>(1000000000L / hz));
    }
    void subscriptionHandler(const epam::CommandResult& /*result*/);
    void onVisConnectionStatusUpdate(bool);
    bool fetchAllAndSubscribe();
    std::string vehiclePropValueToString(VehiclePropValue val) const;

    VehiclePropertyStore* mPropStore;
    std::unordered_set<int32_t> mHvacPowerProps;
    RecurrentTimer mRecurrentTimer;
    std::map<VehicleAreaProperty, std::string> mVPropertyToVisName;
    std::multimap<std::string, VehicleAreaProperty> mVisNameToVProperty;
    VisClient mVisClient;
    size_t mMainSubscriptionId;
    bool mSubscribed;
    std::mutex mLock;
    bool mValuesAreDirty;
};

}  // namespace xenvm
}  // namespace V2_0
}  // namespace vehicle
}  // namespace automotive
}  // namespace hardware
}  // namespace android

#endif /* COMMON_INCLUDE_VHAL_V2_0_VISVEHICLEHAL_H_ */
