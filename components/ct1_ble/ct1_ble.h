#pragma once
#include "esphome.h"
#include "fan_light.h"
#include "version.h"
#include "esp_gap_ble_api.h"

namespace ct1_ble {

class CT1BLE : public Component,
               public mqtt::MQTTDevice,
               public esp32_ble_tracker::ESPBTDeviceListener {
public:
    void setup() override {
        adv_params_.adv_int_min = 0x20;
        adv_params_.adv_int_max = 0x30;
        adv_params_.adv_type = ADV_TYPE_NONCONN_IND;
        adv_params_.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
        adv_params_.channel_map = ADV_CHNL_ALL;
        adv_params_.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;
        
        // 注册 HA 服务
        auto api = global_api_server;
        if (api != nullptr) {
            api->register_user_service({
                "control",
                {
                    {"device_id", "string"},
                    {"command", "string"},
                    {"param", "string"}
                },
                [this](const std::vector<esphome::api::ServiceArgValue>& args) {
                    this->ha_control(args[0].value_string, args[1].value_string, args[2].value_string);
                }
            });
        }
        
        // MQTT 订阅
        for (auto& dev : devices_) {
            std::string topic = "ct1/" + dev.id + "/set";
            this->subscribe(topic, [this, dev_id = dev.id](const std::string& payload) {
                this->on_mqtt_structured(dev_id, payload);
            });
        }
        
        state_ = IDLE;
        last_action_time_ = millis();
        
        ESP_LOGI("CT1", "=================================");
        ESP_LOGI("CT1", "CT1 BLE Gateway %s", FIRMWARE_VERSION);
        ESP_LOGI("CT1", "Build: %s", FIRMWARE_BUILD_DATE);
        ESP_LOGI("CT1", "Devices: %d", (int)devices_.size());
        ESP_LOGI("CT1", "=================================");
    }
    
    void loop() override {
        uint32_t now = millis();
        switch (state_) {
            case IDLE:
                if (!cmd_queue_.empty()) start_sending();
                break;
            case SENDING_START:
                if (now - last_action_time_ >= 100) {
                    send_raw(current_cmd_.end_pkt);
                    state_ = WAIT_ACK;
                    last_action_time_ = now;
                }
                break;
            case WAIT_ACK:
                if (now - last_action_time_ >= 3000) {
                    ESP_LOGW("CT1", "Timeout %s", current_cmd_.device_id.c_str());
                    auto dev = find_dev(current_cmd_.device_id);
                    if (dev) dev->pending_ack = false;
                    state_ = IDLE;
                    cmd_queue_.pop();
                }
                break;
        }
    }

    bool parse_device(const esp32_ble_tracker::ESPBTDevice& device) override {
        auto& raw = device.get_manufacturer_data();
        if (raw.size() < 20) return false;
        if (raw[0] == 0x02 && raw[1] == 0x01 && raw[3] == 0x1B && 
            raw[4] == 0xFF && raw[5] == 0x11 && raw[6] == 0x4D) {
            return parse_134d(raw);
        }
        return false;
    }

    void send_hex(const std::string& hex_str) {
        std::vector<uint8_t> data = hex_to_bytes(hex_str);
        if (data.empty()) {
            ESP_LOGW("CT1", "Invalid hex");
            return;
        }
        send_raw(data);
        ESP_LOGI("CT1", "Raw send: %d bytes", (int)data.size());
    }

private:
    enum State { IDLE, SENDING_START, WAIT_ACK };
    
    struct Cmd {
        std::string device_id;
        std::vector<uint8_t> start_pkt;
        std::vector<uint8_t> end_pkt;
        std::string action;
    };
    
    std::vector<FanLight> devices_ = get_devices();
    std::queue<Cmd> cmd_queue_;
    State state_ = IDLE;
    Cmd current_cmd_;
    uint32_t last_action_time_ = 0;
    esp_ble_adv_params_t adv_params_;

    void ha_control(const std::string& device_id, const std::string& command, 
                    const std::string& param) {
        enqueue(device_id, command, param);
    }
    
    void on_mqtt_structured(const std::string& device_id, const std::string& payload) {
        auto [cmd, param] = parse_payload(payload);
        enqueue(device_id, cmd, param);
    }

    bool parse_134d(const std::vector<uint8_t>& raw) {
        for (auto& dev : devices_) {
            uint8_t target[6];
            sscanf(dev.mac.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                   &target[5], &target[4], &target[3],
                   &target[2], &target[1], &target[0]);
            
            for (size_t i = 7; i <= raw.size() - 6; i++) {
                if (memcmp(&raw[i], target, 6) != 0) continue;
                
                bool power_on = (raw[i + 7] == 0x01);
                uint16_t brt_raw = (raw[i + 12] << 8) | raw[i + 13];
                int brt_pct = (brt_raw == 0xFFFF) ? 100 : (int)(brt_raw / 655.35);
                uint8_t state_byte = raw[i + 14];
                bool fan_run = (state_byte == 0x13 || state_byte == 0x03);
                int fan_spd = fan_run ? (raw[i + 15] + 1) : 0;
                
                dev.state_led = power_on;
                dev.state_brightness = brt_pct;
                dev.state_fan = fan_run;
                dev.state_fan_speed = fan_spd;
                dev.state_fan_dir = fan_run ? "Forward" : "Off";
                
                if (state_ == WAIT_ACK && dev.pending_ack && dev.id == current_cmd_.device_id) {
                    ESP_LOGI("CT1", "ACK OK %s", dev.id.c_str());
                    dev.pending_ack = false;
                    state_ = IDLE;
                    cmd_queue_.pop();
                }
                
                publish_state(dev);
                return true;
            }
        }
        return false;
    }

    void enqueue(const std::string& device_id, const std::string& command, 
                 const std::string& param) {
        auto dev = find_dev(device_id);
        if (!dev) {
            ESP_LOGW("CT1", "Unknown device: %s", device_id.c_str());
            return;
        }
        
        std::string action = map_action(command, param);
        for (auto& act : dev->actions) {
            if (act.name == action) {
                cmd_queue_.push({device_id, act.start_pkt, act.end_pkt, action});
                ESP_LOGI("CT1", "Queue %s -> %s", device_id.c_str(), action.c_str());
                return;
            }
        }
        ESP_LOGW("CT1", "Unknown action: %s", action.c_str());
    }
    
    void start_sending() {
        current_cmd_ = cmd_queue_.front();
        auto dev = find_dev(current_cmd_.device_id);
        if (dev) {
            dev->pending_ack = true;
            dev->pending_action = current_cmd_.action;
            dev->pending_time = millis();
        }
        send_raw(current_cmd_.start_pkt);
        state_ = SENDING_START;
        last_action_time_ = millis();
    }
    
    void send_raw(const std::vector<uint8_t>& data) {
        esp_ble_adv_data_t adv_data = {};
        adv_data.set_scan_rsp = false;
        adv_data.include_name = false;
        adv_data.include_txpower = false;
        adv_data.manufacturer_len = data.size();
        adv_data.p_manufacturer_data = (uint8_t*)data.data();
        adv_data.flag = 0x6;
        
        esp_ble_gap_config_adv_data(&adv_data);
        esp_ble_gap_start_advertising(&adv_params_);
        delay(25);
        esp_ble_gap_stop_advertising();
    }
    
    void publish_state(const FanLight& dev) {
        std::string json = "{";
        json += "\"led\":\"" + std::string(dev.state_led ? "ON" : "OFF") + "\",";
        json += "\"brightness\":" + std::to_string(dev.state_brightness) + ",";
        json += "\"color_temp\":" + std::to_string(dev.state_color_temp) + ",";
        json += "\"fan\":\"" + std::string(dev.state_fan ? "ON" : "OFF") + "\",";
        json += "\"fan_speed\":" + std::to_string(dev.state_fan_speed) + ",";
        json += "\"fan_dir\":\"" + dev.state_fan_dir + "\"";
        json += "}";
        
        this->publish("ct1/" + dev.id + "/state", json);
    }

    FanLight* find_dev(const std::string& id) {
        for (auto& d : devices_) if (d.id == id) return &d;
        return nullptr;
    }
    
    std::string map_action(const std::string& cmd, const std::string& param) {
        if (cmd == "on") return "on";
        if (cmd == "off") return "off";
        if (cmd == "fan_on") return "fan_on";
        if (cmd == "fan_off") return "fan_off";
        if (cmd == "brightness") {
            int b = param.empty() ? 100 : std::stoi(param);
            if (b <= 10) return "brightness_1";
            if (b <= 30) return "brightness_20";
            if (b <= 45) return "brightness_40";
            if (b <= 55) return "brightness_50";
            if (b <= 70) return "brightness_60";
            if (b <= 90) return "brightness_80";
            return "brightness_100";
        }
        if (cmd == "color_temp") {
            int c = param.empty() ? 2700 : std::stoi(param);
            if (c <= 3000) return "color_2700";
            if (c <= 5000) return "color_3500";
            return "color_6500";
        }
        if (cmd == "fan_speed") {
            int s = param.empty() ? 1 : std::stoi(param);
            if (s < 1) s = 1; if (s > 6) s = 6;
            return "speed_" + std::to_string(s);
        }
        return cmd;
    }
    
    std::pair<std::string, std::string> parse_payload(const std::string& p) {
        if (p.empty() || p.front() != '{') return {p, ""};
        std::string cmd, param;
        size_t cmd_pos = p.find("\"cmd\":\"");
        if (cmd_pos != std::string::npos) {
            size_t start = cmd_pos + 7;
            size_t end = p.find('"', start);
            if (end != std::string::npos) cmd = p.substr(start, end - start);
        }
        size_t param_pos = p.find("\"param\":\"");
        if (param_pos != std::string::npos) {
            size_t start = param_pos + 9;
            size_t end = p.find('"', start);
            if (end != std::string::npos) param = p.substr(start, end - start);
        } else {
            param_pos = p.find("\"param\":");
            if (param_pos != std::string::npos) {
                size_t start = param_pos + 8;
                while (start < p.size() && (p[start] == ' ' || p[start] == '\"')) start++;
                size_t end = start;
                while (end < p.size() && p[end] != '\"' && p[end] != '}' && p[end] != ',') end++;
                param = p.substr(start, end - start);
            }
        }
        return {cmd, param};
    }
};

} // namespace ct1_ble
