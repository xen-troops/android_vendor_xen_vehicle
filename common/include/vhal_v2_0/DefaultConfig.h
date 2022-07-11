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

#ifndef android_hardware_automotive_vehicle_V2_0_impl_DefaultConfig_H_
#define android_hardware_automotive_vehicle_V2_0_impl_DefaultConfig_H_

#include <android/hardware/automotive/vehicle/2.0/IVehicle.h>
#include <vhal_v2_0/VehicleUtils.h>

namespace android {
namespace hardware {
namespace automotive {
namespace vehicle {
namespace V2_0 {

namespace impl {
//
// Some handy constants to avoid conversions from enum to int.
constexpr int ABS_ACTIVE = static_cast<int>(VehicleProperty::ABS_ACTIVE);
constexpr int AP_POWER_STATE_REQ = static_cast<int>(VehicleProperty::AP_POWER_STATE_REQ);
constexpr int AP_POWER_STATE_REPORT = static_cast<int>(VehicleProperty::AP_POWER_STATE_REPORT);
constexpr int DOOR_1_LEFT = static_cast<int>(VehicleAreaDoor::ROW_1_LEFT);
constexpr int DOOR_1_RIGHT = static_cast<int>(VehicleAreaDoor::ROW_1_RIGHT);
constexpr int TRACTION_CONTROL_ACTIVE = static_cast<int>(VehicleProperty::TRACTION_CONTROL_ACTIVE);
constexpr int VEHICLE_MAP_SERVICE = static_cast<int>(VehicleProperty::VEHICLE_MAP_SERVICE);
constexpr int WHEEL_TICK = static_cast<int>(VehicleProperty::WHEEL_TICK);
constexpr int ALL_WHEELS =
    static_cast<int>(VehicleAreaWheel::LEFT_FRONT | VehicleAreaWheel::RIGHT_FRONT |
          VehicleAreaWheel::LEFT_REAR | VehicleAreaWheel::RIGHT_REAR);
constexpr int HVAC_LEFT = static_cast<int>(VehicleAreaSeat::ROW_1_LEFT |
                          VehicleAreaSeat::ROW_2_LEFT | VehicleAreaSeat::ROW_2_CENTER);
constexpr int HVAC_RIGHT = static_cast<int>(VehicleAreaSeat::ROW_1_RIGHT |
                           VehicleAreaSeat::ROW_2_RIGHT);
constexpr int HVAC_ALL = HVAC_LEFT | HVAC_RIGHT;

/**
 * This property is used for test purpose to generate fake events. Here is the test package that
 * is referencing this property definition: packages/services/Car/tests/vehiclehal_test
 */
const int32_t kGenerateFakeDataControllingProperty =
    0x0666 | VehiclePropertyGroup::VENDOR | VehicleArea::GLOBAL | VehiclePropertyType::MIXED;

const int32_t kGenerateFakeDataControllingPropertyFv =
    0x0667 | VehiclePropertyGroup::VENDOR | VehicleArea::GLOBAL | VehiclePropertyType::FLOAT_VEC;

const int32_t kHvacPowerProperties[] = {
    toInt(VehicleProperty::HVAC_FAN_SPEED),
    toInt(VehicleProperty::HVAC_FAN_DIRECTION),
};

struct ConfigDeclaration {
    VehiclePropConfig config;

    /* This value will be used as an initial value for the property. If this field is specified for
     * property that supports multiple areas then it will be used for all areas unless particular
     * area is overridden in initialAreaValue field. */
    VehiclePropValue::RawValue initialValue;
    /* Use initialAreaValues if it is necessary to specify different values per each area. */
    std::map<int32_t, VehiclePropValue::RawValue> initialAreaValues;
    /* Used for Property mapping(propid,areaid) => VIS property name */
    std::map<int32_t, std::string> initialAreaToVIS;
};

const ConfigDeclaration kVehicleProperties[]{
    {.config =
         {
             .prop = toInt(VehicleProperty::INFO_FUEL_CAPACITY),
             .access = VehiclePropertyAccess::READ,
             .changeMode = VehiclePropertyChangeMode::STATIC,
         },
     .initialValue = {.floatValues = {100}},
     .initialAreaToVIS = {{0,"Attribute.Vehicle.Drivetrain.FuelSystem.TankCapacity"}}
    },

    {.config =
         {
             .prop = toInt(VehicleProperty::INFO_FUEL_TYPE),
             .access = VehiclePropertyAccess::READ,
             .changeMode = VehiclePropertyChangeMode::STATIC,
         },
     .initialValue = {.int32Values = {1}},
     .initialAreaToVIS = {{0,"Attribute.Vehicle.Drivetrain.FuelSystem.FuelType"}}
    },

    {.config =
	 {
	      .prop = toInt(VehicleProperty::RANGE_REMAINING),
	      .access = VehiclePropertyAccess::READ_WRITE,
	      .changeMode = VehiclePropertyChangeMode::CONTINUOUS,
	      .minSampleRate = 1.0f,
	      .maxSampleRate = 2.0f,
	 },
     .initialValue = {.floatValues = {1000000.0f}},  // units in meters
     .initialAreaToVIS = {{0,"Attribute.Vehicle.Drivetrain.FuelSystem.RangeRemain"}},
    },

    {.config =
         {
             .prop = toInt(VehicleProperty::INFO_MAKE),
             .access = VehiclePropertyAccess::READ,
             .changeMode = VehiclePropertyChangeMode::STATIC,
         },
     .initialValue = {.stringValue = "VIS based Vehicle"},
     .initialAreaToVIS = {{0,"Attribute.Vehicle.VehicleIdentification.Brand"}},
    },
    {.config =
         {
             .prop = toInt(VehicleProperty::PERF_VEHICLE_SPEED),
             .access = VehiclePropertyAccess::READ,
             .changeMode = VehiclePropertyChangeMode::ON_CHANGE,
             .minSampleRate = 1.0f,
             .maxSampleRate = 10.0f,
         },
     .initialValue = {.floatValues = {0.0f}},
     .initialAreaToVIS = {{0,"Signal.Emulator.telemetry.veh_speed"}},
    },

    {.config =
         {
             .prop = toInt(VehicleProperty::PERF_ODOMETER),
             .access = VehiclePropertyAccess::READ,
             .changeMode = VehiclePropertyChangeMode::CONTINUOUS,
         },
     .initialValue = {.floatValues = {0.0f}},
     .initialAreaToVIS = {{0,"Signal.Emulator.telemetry.odo"}},
    },
    {
        .config =
            {
                .prop = toInt(VehicleProperty::ENGINE_RPM),
                .access = VehiclePropertyAccess::READ,
                .changeMode = VehiclePropertyChangeMode::CONTINUOUS,
                .minSampleRate = 1.0f,
                .maxSampleRate = 10.0f,
            },
        .initialValue = {.floatValues = {0.0f}},
        .initialAreaToVIS = {{0,"Signal.Emulator.telemetry.engrpm"}},
    },
    {.config =
         {
             .prop = toInt(VehicleProperty::FUEL_LEVEL),
             .access = VehiclePropertyAccess::READ,
             .changeMode = VehiclePropertyChangeMode::CONTINUOUS,
         },
     .initialValue = {.floatValues = {15000}},
     .initialAreaToVIS = {{0,"Signal.Emulator.telemetry.avgfuellvl"}},
    },
    {.config =
         {
             .prop = toInt(VehicleProperty::FUEL_DOOR_OPEN),
             .access = VehiclePropertyAccess::READ,
             .changeMode = VehiclePropertyChangeMode::ON_CHANGE,
         },
     .initialValue = {.int32Values = {0}},
     .initialAreaToVIS = {{0,"Sensor.Vehicle.Tank.TankDoor"}},
    },

    {.config =
         {
             .prop = toInt(VehicleProperty::CURRENT_GEAR),
             .access = VehiclePropertyAccess::READ,
             .changeMode = VehiclePropertyChangeMode::ON_CHANGE,
         },
     .initialValue = {.int32Values = {toInt(VehicleGear::GEAR_PARK)}},
     .initialAreaToVIS = {{0,"Attribute.Vehicle.Drivetrain.Transmission.CurrentGear"}},
    },

    {.config =
         {
             .prop = toInt(VehicleProperty::PARKING_BRAKE_ON),
             .access = VehiclePropertyAccess::READ,
             .changeMode = VehiclePropertyChangeMode::ON_CHANGE,
         },
     .initialValue = {.int32Values = {1}},
     .initialAreaToVIS = {{0,"Signal.Emulator.telemetry.prkbrkstat"}},
    },

    {.config =
         {
             .prop = toInt(VehicleProperty::FUEL_LEVEL_LOW),
             .access = VehiclePropertyAccess::READ,
             .changeMode = VehiclePropertyChangeMode::ON_CHANGE,
         },
     .initialValue = {.int32Values = {0}},
     .initialAreaToVIS = {{0,"Sensor.Vehicle.Drivetrain.FuelSystem.LowFuelLevel"}},
    },

    {.config =
         {
             .prop = toInt(VehicleProperty::HW_KEY_INPUT),
             .access = VehiclePropertyAccess::READ,
             .changeMode = VehiclePropertyChangeMode::ON_CHANGE,
         },
     .initialValue = {.int32Values = {0, 0, 0}},
     .initialAreaToVIS = {{0, "Actuator.Vehicle.Cabin.HwInput"}},
    },

    {.config = {.prop = toInt(VehicleProperty::HVAC_POWER_ON),
                .access = VehiclePropertyAccess::READ_WRITE,
                .changeMode = VehiclePropertyChangeMode::ON_CHANGE,
                .areaConfigs = {VehicleAreaConfig{.areaId = HVAC_ALL}},
                // TODO(bryaneyler): Ideally, this is generated dynamically from
                // kHvacPowerProperties.
                .configArray =
                    {
                        0x12400500,  // HVAC_FAN_SPEED
                        0x12400501   // HVAC_FAN_DIRECTION
                    }},
     .initialValue = {.int32Values = {1}},
     .initialAreaToVIS = {{HVAC_ALL, "Actuator.Vehicle.Cabin.HVAC.IsPowerOn"}},
    },

    {
        .config = {.prop = toInt(VehicleProperty::HVAC_DEFROSTER),
                   .access = VehiclePropertyAccess::READ_WRITE,
                   .changeMode = VehiclePropertyChangeMode::ON_CHANGE,
                   .areaConfigs =
                       {VehicleAreaConfig{.areaId = toInt(VehicleAreaWindow::FRONT_WINDSHIELD)},
                        VehicleAreaConfig{.areaId = toInt(VehicleAreaWindow::REAR_WINDSHIELD)}}},
        .initialValue = {.int32Values = {0}},  // Will be used for all areas.
        .initialAreaToVIS = {
                {toInt(VehicleAreaWindow::FRONT_WINDSHIELD),"Actuator.Vehicle.Cabin.HVAC.IsFrontDefrosterActive"},
                {toInt(VehicleAreaWindow::REAR_WINDSHIELD),"Actuator.Vehicle.Cabin.HVAC.IsRearDefrosterActive"},
        },

    },

    {.config = {.prop = toInt(VehicleProperty::HVAC_MAX_DEFROST_ON),
                .access = VehiclePropertyAccess::READ_WRITE,
                .changeMode = VehiclePropertyChangeMode::ON_CHANGE,
                .areaConfigs = {VehicleAreaConfig{.areaId = HVAC_ALL}}},
     .initialValue = {.int32Values = {0}},
     .initialAreaToVIS = {{HVAC_ALL, "Actuator.Vehicle.Cabin.HVAC.IsMaxDefrosterOn"}},
    },

    {.config = {.prop = toInt(VehicleProperty::HVAC_RECIRC_ON),
                .access = VehiclePropertyAccess::READ_WRITE,
                .changeMode = VehiclePropertyChangeMode::ON_CHANGE,
                .areaConfigs = {VehicleAreaConfig{.areaId = HVAC_ALL}}},
     .initialValue = {.int32Values = {1}},
     .initialAreaToVIS = {{HVAC_ALL, "Actuator.Vehicle.Cabin.HVAC.IsRecirculationActive"}},
    },

    {.config = {.prop = toInt(VehicleProperty::HVAC_AUTO_RECIRC_ON),
                .access = VehiclePropertyAccess::READ_WRITE,
                .changeMode = VehiclePropertyChangeMode::ON_CHANGE,
                .areaConfigs = {VehicleAreaConfig{.areaId = HVAC_ALL}}},
     .initialValue = {.int32Values = {0}},
     .initialAreaToVIS = {{HVAC_ALL, "Actuator.Vehicle.Cabin.HVAC.IsAutoRecirculationActive"}},
    },

    {.config = {.prop = toInt(VehicleProperty::HVAC_AC_ON),
                .access = VehiclePropertyAccess::READ_WRITE,
                .changeMode = VehiclePropertyChangeMode::ON_CHANGE,
                .areaConfigs = {VehicleAreaConfig{.areaId = HVAC_ALL}}},
     .initialValue = {.int32Values = {1}},
     .initialAreaToVIS = {{HVAC_ALL, "Actuator.Vehicle.Cabin.HVAC.IsAirConditioningActive"}},
    },

    {.config = {.prop = toInt(VehicleProperty::HVAC_MAX_AC_ON),
                .access = VehiclePropertyAccess::READ_WRITE,
                .changeMode = VehiclePropertyChangeMode::ON_CHANGE,
                .areaConfigs = {VehicleAreaConfig{.areaId = HVAC_ALL}}},
     .initialValue = {.int32Values = {0}},
     .initialAreaToVIS = {{HVAC_ALL, "Actuator.Vehicle.Cabin.HVAC.IsMaxAcActive"}},
    },

    {.config = {.prop = toInt(VehicleProperty::HVAC_AUTO_ON),
                .access = VehiclePropertyAccess::READ_WRITE,
                .changeMode = VehiclePropertyChangeMode::ON_CHANGE,
                .areaConfigs = {VehicleAreaConfig{.areaId = HVAC_ALL}}},
     .initialValue = {.int32Values = {1}},
     .initialAreaToVIS = {{HVAC_ALL, "Actuator.Vehicle.Cabin.HVAC.IsAutoOn"}},
    },

    {.config = {.prop = toInt(VehicleProperty::HVAC_DUAL_ON),
                .access = VehiclePropertyAccess::READ_WRITE,
                .changeMode = VehiclePropertyChangeMode::ON_CHANGE,
                .areaConfigs = {VehicleAreaConfig{.areaId = HVAC_ALL}}},
     .initialValue = {.int32Values = {0}},
     .initialAreaToVIS = {{HVAC_ALL, "Actuator.Vehicle.Cabin.HVAC.IsDualOn"}},
    },

    {.config = {.prop = toInt(VehicleProperty::HVAC_FAN_SPEED),
                .access = VehiclePropertyAccess::READ_WRITE,
                .changeMode = VehiclePropertyChangeMode::ON_CHANGE,
                .areaConfigs = {VehicleAreaConfig{
                    .areaId = HVAC_ALL, .minInt32Value = 1, .maxInt32Value = 7}}},
     .initialValue = {.int32Values = {3}},
     .initialAreaToVIS = {{HVAC_ALL, "Actuator.Vehicle.Cabin.HVAC.Row1.FanSpeed"}},
    },

    {.config = {.prop = toInt(VehicleProperty::HVAC_FAN_DIRECTION),
                .access = VehiclePropertyAccess::READ_WRITE,
                .changeMode = VehiclePropertyChangeMode::ON_CHANGE,
                .areaConfigs = {VehicleAreaConfig{.areaId = HVAC_ALL}}},
     .initialValue = {.int32Values = {toInt(VehicleHvacFanDirection::FACE)}},
     .initialAreaToVIS = {{HVAC_ALL, "Actuator.Vehicle.Cabin.HVAC.Row1.FanDirection"}},
    },

    {.config = {.prop = toInt(VehicleProperty::HVAC_STEERING_WHEEL_HEAT),
                .access = VehiclePropertyAccess::READ_WRITE,
                .changeMode = VehiclePropertyChangeMode::ON_CHANGE,
                .areaConfigs = {VehicleAreaConfig{
                    .areaId = (0), .minInt32Value = -2, .maxInt32Value = 2}}},
     .initialValue = {.int32Values = {0}},
     .initialAreaToVIS = {{0, "Signal.Emulator.telemetry.stw_temp"}},
    },  // +ve values for heating and -ve for cooling

    {.config = {.prop = toInt(VehicleProperty::HVAC_TEMPERATURE_SET),
                .access = VehiclePropertyAccess::READ_WRITE,
                .changeMode = VehiclePropertyChangeMode::ON_CHANGE,
                .areaConfigs = {VehicleAreaConfig{
                                    .areaId = HVAC_LEFT, .minFloatValue = 16, .maxFloatValue = 32,
                                },
                                VehicleAreaConfig{
                                    .areaId = HVAC_RIGHT, .minFloatValue = 16, .maxFloatValue = 32,
                                }}},
     .initialAreaValues = {{HVAC_LEFT, {.floatValues = {16}}},
                           {HVAC_RIGHT, {.floatValues = {20}}}},
     .initialAreaToVIS = {{HVAC_LEFT,"Actuator.Vehicle.Cabin.HVAC.Row1.Left.Temperature"},
             {HVAC_RIGHT,"Actuator.Vehicle.Cabin.HVAC.Row1.Right.Temperature"}}
    },

    {.config =
         {
             .prop = toInt(VehicleProperty::ENV_OUTSIDE_TEMPERATURE),
             .access = VehiclePropertyAccess::READ,
             .changeMode = VehiclePropertyChangeMode::CONTINUOUS,
             .minSampleRate = 1.0f,
             .maxSampleRate = 2.0f,
         },
     .initialValue = {.floatValues = {25.0f}},
     .initialAreaToVIS = {{HVAC_ALL, "Signal.Emulator.telemetry.airtemp_outsd"}},
    },

    {.config = {.prop = toInt(VehicleProperty::HVAC_TEMPERATURE_DISPLAY_UNITS),
                .access = VehiclePropertyAccess::READ_WRITE,
                .changeMode = VehiclePropertyChangeMode::ON_CHANGE,
                .areaConfigs = {VehicleAreaConfig{.areaId = (0)}}},
     .initialValue = {.int32Values = {(int)VehicleUnit::FAHRENHEIT}},
     .initialAreaToVIS = {{0, "Actuator.Vehicle.Cabin.HVAC.temperatureDisplayUnits"}},
    },

    {.config =
         {
             .prop = toInt(VehicleProperty::GEAR_SELECTION),
             .access = VehiclePropertyAccess::READ_WRITE,
             .changeMode = VehiclePropertyChangeMode::ON_CHANGE,
         },
     .initialValue = {.int32Values = {toInt(VehicleGear::GEAR_PARK)}},
     .initialAreaToVIS = {{0,"Attribute.Vehicle.Drivetrain.Transmission.CurrentGear"}},
    },

    {.config =
         {
             .prop = toInt(VehicleProperty::IGNITION_STATE),
             .access = VehiclePropertyAccess::READ,
             .changeMode = VehiclePropertyChangeMode::ON_CHANGE,
         },
     .initialValue = {.int32Values = {toInt(VehicleIgnitionState::ON)}},
     .initialAreaToVIS = {{0,"Sensor.Vehicle.IgnitionState"}},
    },

    {.config =
         {
             .prop = toInt(VehicleProperty::ENGINE_OIL_LEVEL),
             .access = VehiclePropertyAccess::READ,
             .changeMode = VehiclePropertyChangeMode::ON_CHANGE,
         },
     .initialValue = {.int32Values = {toInt(VehicleOilLevel::NORMAL)}},
     .initialAreaToVIS = {{0,"Signal.Emulator.telemetry.oil_press"}},
    },

    {.config =
         {
             .prop = toInt(VehicleProperty::ENGINE_OIL_TEMP),
             .access = VehiclePropertyAccess::READ,
             .changeMode = VehiclePropertyChangeMode::CONTINUOUS,
             .minSampleRate = 0.1,  // 0.1 Hz, every 10 seconds
             .maxSampleRate = 10,   // 10 Hz, every 100 ms
         },
     .initialValue = {.floatValues = {101.0f}},
     .initialAreaToVIS = {{0,"Signal.Emulator.telemetry.engoiltemp"}},
    },



    {.config = {.prop = toInt(VehicleProperty::DOOR_LOCK),
                .access = VehiclePropertyAccess::READ,
                .changeMode = VehiclePropertyChangeMode::ON_CHANGE,
                .areaConfigs = {VehicleAreaConfig{.areaId = DOOR_1_LEFT},
                                VehicleAreaConfig{.areaId = DOOR_1_RIGHT}}},
     .initialAreaValues = {{DOOR_1_LEFT, {.int32Values = {1}}},
                           {DOOR_1_RIGHT, {.int32Values = {1}}}},
     .initialAreaToVIS = {{DOOR_1_LEFT,"Actuator.Vehicle.Cabin.Door.Row1.Left.IsLocked"},
             {DOOR_1_RIGHT,"Actuator.Vehicle.Cabin.Door.Row1.Right.IsLocked"}},
    },


    {.config = {.prop = ABS_ACTIVE,
                .access = VehiclePropertyAccess::READ,
                .changeMode = VehiclePropertyChangeMode::ON_CHANGE},
     .initialValue = {.int32Values = {0}},
     .initialAreaToVIS = {{0,"Actuator.Vehicle.ADAS.ABS.IsActive"}},
    },

    {.config = {.prop = TRACTION_CONTROL_ACTIVE,
                .access = VehiclePropertyAccess::READ,
                .changeMode = VehiclePropertyChangeMode::ON_CHANGE},
     .initialValue = {.int32Values = {0}},
     .initialAreaToVIS = {{0,"Actuator.Vehicle.ADAS.TCS.IsActive"}},
    },


     {.config =
          {
              .prop = WHEEL_TICK,
              .access = VehiclePropertyAccess::READ,
              .changeMode = VehiclePropertyChangeMode::CONTINUOUS,
              .configArray = {ALL_WHEELS, 50000, 50000, 50000, 50000},
              .minSampleRate = 1.0f,
              .maxSampleRate = 10.0f,
          },
      .initialValue = {.int64Values = {0, 100000, 200000, 300000, 400000}},
      .initialAreaToVIS = {{0,"Sensor.Vehicle.WheelTick"}},
     },

     /* Not mapped, internal, fake, used for testing */
    {
        .config =
            {
                .prop = kGenerateFakeDataControllingProperty,
                .access = VehiclePropertyAccess::WRITE,
                .changeMode = VehiclePropertyChangeMode::ON_CHANGE,
            },
    },
    /* Not mapped, internal, fake, used for testing */
    {
       .config =
           {
               .prop = kGenerateFakeDataControllingPropertyFv,
               .access = VehiclePropertyAccess::READ_WRITE,
               .changeMode = VehiclePropertyChangeMode::ON_CHANGE,
           },
           .initialValue = {.floatValues = {0, 0, 0}},
           .initialAreaToVIS = {{0,"Test.Fv"}},
    },

      /* Not mapped, internal */
     {.config =
          {
              .prop = toInt(VehicleProperty::NIGHT_MODE),
              .access = VehiclePropertyAccess::READ,
              .changeMode = VehiclePropertyChangeMode::ON_CHANGE,
          },
      .initialValue = {.int32Values = {0}}},

      /* Not mapped, internal */
     {.config = {.prop = toInt(VehicleProperty::DISPLAY_BRIGHTNESS),
                 .access = VehiclePropertyAccess::READ_WRITE,
                 .changeMode = VehiclePropertyChangeMode::ON_CHANGE,
                 .areaConfigs = {VehicleAreaConfig{.minInt32Value = 0, .maxInt32Value = 100}}},
      .initialValue = {.int32Values = {100}}},

     /* Not mapped, internal */
    {.config = {.prop = toInt(VehicleProperty::AP_POWER_STATE_REQ),
                .access = VehiclePropertyAccess::READ,
                .changeMode = VehiclePropertyChangeMode::ON_CHANGE,
                .configArray = {3}},
     .initialValue = {.int32Values = {toInt(VehicleApPowerStateReq::ON), 0}}},

     /* Not mapped, internal */
    {.config = {.prop = toInt(VehicleProperty::AP_POWER_STATE_REPORT),
                .access = VehiclePropertyAccess::WRITE,
                .changeMode = VehiclePropertyChangeMode::ON_CHANGE},
     .initialValue = {.int32Values = {toInt(VehicleApPowerStateReport::WAIT_FOR_VHAL), 0}}},

     /* Not mapped, internal */
    {.config = {.prop = VEHICLE_MAP_SERVICE,
                .access = VehiclePropertyAccess::READ_WRITE,
                .changeMode = VehiclePropertyChangeMode::ON_CHANGE}
    },

    /*{.config =
         {
             .prop = toInt(VehicleProperty::INFO_EV_BATTERY_CAPACITY),
             .access = VehiclePropertyAccess::READ,
             .changeMode = VehiclePropertyChangeMode::STATIC,
         },
     .initialValue = {.floatValues = {150000}}},*/

    /*{.config =
         {
             .prop = toInt(VehicleProperty::INFO_EV_CONNECTOR_TYPE),
             .access = VehiclePropertyAccess::READ,
             .changeMode = VehiclePropertyChangeMode::STATIC,
         },
     .initialValue = {.int32Values = {1}}},*/

    /*{.config =
         {
             .prop = toInt(VehicleProperty::EV_BATTERY_LEVEL),
             .access = VehiclePropertyAccess::READ,
             .changeMode = VehiclePropertyChangeMode::ON_CHANGE,
         },
     .initialValue = {.floatValues = {150000}}},*/

    /*{.config =
         {
             .prop = toInt(VehicleProperty::EV_CHARGE_PORT_OPEN),
             .access = VehiclePropertyAccess::READ,
             .changeMode = VehiclePropertyChangeMode::ON_CHANGE,
         },
     .initialValue = {.int32Values = {0}}},*/

    /*{.config =
         {
             .prop = toInt(VehicleProperty::EV_CHARGE_PORT_CONNECTED),
             .access = VehiclePropertyAccess::READ,
             .changeMode = VehiclePropertyChangeMode::ON_CHANGE,
         },
     .initialValue = {.int32Values = {0}}},*/

    /*{.config =
         {
             .prop = toInt(VehicleProperty::EV_BATTERY_INSTANTANEOUS_CHARGE_RATE),
             .access = VehiclePropertyAccess::READ,
             .changeMode = VehiclePropertyChangeMode::ON_CHANGE,
         },
     .initialValue = {.floatValues = {0}}},*/

};

}  // namespace impl

}  // namespace V2_0
}  // namespace vehicle
}  // namespace automotive
}  // namespace hardware
}  // namespace android

#endif // android_hardware_automotive_vehicle_V2_0_impl_DefaultConfig_H_
