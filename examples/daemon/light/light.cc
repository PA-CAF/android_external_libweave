// Copyright 2015 The Weave Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "examples/daemon/common/daemon.h"

#include <weave/device.h>

#include <base/bind.h>
#include <base/memory/weak_ptr.h>

// LightHandler is a command handler example that shows
// how to handle commands for a Weave light.
class LightHandler {
 public:
  LightHandler() = default;
  void Register(weave::Device* device) {
    device_ = device;

    device->AddStateDefinitionsFromJson(R"({
      "onOff": {"state": ["on", "standby"]},
      "brightness": {"brightness": "integer"},
      "colorXY": {
        "colorSetting": {
          "properties": {
            "colorX": {"minimum": 0.0, "maximum": 1.0},
            "colorY": {"minimum": 0.0, "maximum": 1.0}
          }
        },
        "colorCapRed": {
          "properties": {
            "colorX": {"minimum": 0.0, "maximum": 1.0},
            "colorY": {"minimum": 0.0, "maximum": 1.0}
          }
        },
        "colorCapGreen": {
          "properties": {
            "colorX": {"minimum": 0.0, "maximum": 1.0},
            "colorY": {"minimum": 0.0, "maximum": 1.0}
          }
        },
        "colorCapBlue": {
          "properties": {
            "colorX": {"minimum": 0.0, "maximum": 1.0},
            "colorY": {"minimum": 0.0, "maximum": 1.0}
          }
        }
      }
    })");

    device->SetStatePropertiesFromJson(R"({
      "onOff":{"state": "standby"},
      "brightness":{"brightness": 0},
      "colorXY": {
        "colorSetting": {"colorX": 0, "colorY": 0},
        "colorCapRed":  {"colorX": 0.674, "colorY": 0.322},
        "colorCapGreen":{"colorX": 0.408, "colorY": 0.517},
        "colorCapBlue": {"colorX": 0.168, "colorY": 0.041}
      }
    })",
                                       nullptr);

    device->AddCommandDefinitionsFromJson(R"({
      "onOff": {
         "setConfig":{
           "parameters": {
             "state": ["on", "standby"]
           }
         }
       },
       "brightness": {
         "setConfig":{
           "parameters": {
             "brightness": {
               "type": "integer",
               "minimum": 0,
               "maximum": 100
             }
           }
        }
      },
      "_colorXY": {
        "_setConfig": {
          "minimalRole": "user",
          "parameters": {
            "_colorSetting": {
              "type": "object",
              "properties": {
                "_colorX": {
                  "type": "number",
                  "minimum": 0,
                  "maximum": 1
                },
                "_colorY": {
                  "type": "number",
                  "minimum": 0,
                  "maximum": 1
                }
              }
            }
          }
        }
      }
    })");
    device->AddCommandHandler("onOff.setConfig",
                              base::Bind(&LightHandler::OnOnOffSetConfig,
                                         weak_ptr_factory_.GetWeakPtr()));
    device->AddCommandHandler("brightness.setConfig",
                              base::Bind(&LightHandler::OnBrightnessSetConfig,
                                         weak_ptr_factory_.GetWeakPtr()));
    device->AddCommandHandler("_colorXY._setConfig",
                              base::Bind(&LightHandler::OnColorXYSetConfig,
                                         weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  void OnBrightnessSetConfig(const std::weak_ptr<weave::Command>& command) {
    auto cmd = command.lock();
    if (!cmd)
      return;
    LOG(INFO) << "received command: " << cmd->GetName();
    int32_t brightness_value = 0;
    if (cmd->GetParameters()->GetInteger("brightness", &brightness_value)) {
      // Display this command in terminal.
      LOG(INFO) << cmd->GetName() << " brightness: " << brightness_value;

      if (brightness_state_ != brightness_value) {
        brightness_state_ = brightness_value;
        UpdateLightState();
      }
      cmd->Complete({}, nullptr);
      return;
    }
    weave::ErrorPtr error;
    weave::Error::AddTo(&error, FROM_HERE, "example", "invalid_parameter_value",
                        "Invalid parameters");
    cmd->Abort(error.get(), nullptr);
  }

  void OnOnOffSetConfig(const std::weak_ptr<weave::Command>& command) {
    auto cmd = command.lock();
    if (!cmd)
      return;
    LOG(INFO) << "received command: " << cmd->GetName();
    std::string requested_state;
    if (cmd->GetParameters()->GetString("state", &requested_state)) {
      LOG(INFO) << cmd->GetName() << " state: " << requested_state;

      bool new_light_status = requested_state == "on";
      if (new_light_status != light_status_) {
        light_status_ = new_light_status;

        LOG(INFO) << "Light is now: " << (light_status_ ? "ON" : "OFF");
        UpdateLightState();
      }
      cmd->Complete({}, nullptr);
      return;
    }
    weave::ErrorPtr error;
    weave::Error::AddTo(&error, FROM_HERE, "example", "invalid_parameter_value",
                        "Invalid parameters");
    cmd->Abort(error.get(), nullptr);
  }

  void OnColorXYSetConfig(const std::weak_ptr<weave::Command>& command) {
    auto cmd = command.lock();
    if (!cmd)
      return;
    LOG(INFO) << "received command: " << cmd->GetName();
    auto params = cmd->GetParameters();
    base::DictionaryValue* colorXY = nullptr;
    if (params->GetDictionary("_colorSetting", &colorXY)) {
      bool updateState = false;
      double X = 0.0;
      double Y = 0.0;
      if (colorXY->GetDouble("_colorX", &X)) {
        color_X_ = X;
        updateState = true;
      }

      if (colorXY->GetDouble("_colorY", &Y)) {
        color_Y_ = Y;
        updateState = true;
      }

      if (updateState)
        UpdateLightState();

      cmd->Complete({}, nullptr);
      return;
    }

    weave::ErrorPtr error;
    weave::Error::AddTo(&error, FROM_HERE, "example", "invalid_parameter_value",
                        "Invalid parameters");
    cmd->Abort(error.get(), nullptr);
  }

  void UpdateLightState() {
    base::DictionaryValue state;
    state.SetString("onOff.state", light_status_ ? "on" : "standby");
    state.SetInteger("brightness.brightness", brightness_state_);

    std::unique_ptr<base::DictionaryValue> colorXY(new base::DictionaryValue());
    colorXY->SetDouble("colorX", color_X_);
    colorXY->SetDouble("colorY", color_Y_);
    state.Set("colorXY.colorSetting", colorXY.get());
    device_->SetStateProperties(state, nullptr);
    colorXY.release();
  }

  weave::Device* device_{nullptr};

  // Simulate the state of the light.
  bool light_status_;
  int32_t brightness_state_;
  double color_X_{0.0};
  double color_Y_{0.0};
  base::WeakPtrFactory<LightHandler> weak_ptr_factory_{this};
};

int main(int argc, char** argv) {
  Daemon::Options opts;
  if (!opts.Parse(argc, argv)) {
    Daemon::Options::ShowUsage(argv[0]);
    return 1;
  }
  Daemon daemon{opts};
  LightHandler handler;
  handler.Register(daemon.GetDevice());
  daemon.Run();
  return 0;
}
