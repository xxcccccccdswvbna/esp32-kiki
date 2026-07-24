#pragma once
#include "esphome.h"
#include "fan_light.h"
#include "esp_gap_ble_api.h"

class BLEGateway : public Component,
                   public CustomAPIDevice,
                   public mqtt::CustomMQTTDevice,
                   public esp32_ble_tracker::ESPBTDeviceListener {
public:
    void setup() override {
        // 广播参数：短间隔快速发出，减少占用射频时间
        adv_params_.adv_int_min = 0x20;   // 20ms
        adv_params_.adv_int_max = 0x30;   // 30ms
        adv_params_.adv_type = ADV_TYPE_NONCONN_IND;
        adv_params_.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
        adv_params_.channel_map = ADV_CHNL_ALL;
        adv_params_.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;
        
        // HA 服务
        register_service(&BLEGateway::ha_control, "control",
            {"device_id", "command", "param"});
        
        // MQTT 订阅（自动遍历所有设备）
        for (auto& dev : devices_) {
            subscribe("esp32/" + dev.id + "/set", &BLEGateway::on_mqtt_message);
            ESP_LOGI("BLEGW", "MQTT sub: esp32/%s/set", dev.id.c_str());
        }
        
        state_ = IDLE;
        last_action_time_ = millis();
        ESP_LOGI("BLEGW", "Ready, %d devices", (int)devices_.size());
    }
    
    void loop() override {
        uint32_t now = millis();
        
        switch (state_) {
            case IDLE:
                if (!cmd_queue_.empty()) {
                    start_sending();
                }
                break;
                
            case SENDING_START:
                // 发完 start 后等 100ms 发 end
                if (now - last_action_time_ >= 100) {
                    send_packet(current_cmd_.end_pkt);
                    state_ = WAIT_ACK;
                    last_action_time_ = now;
                    ESP_LOGD("BLEGW", "Sent END, wait ACK...");
                }
                break;
                
            case WAIT_ACK:
                // 等广播确认，超时 3 秒（考虑代理可能延迟扫描）
                if (now - last_action_time_ >= 3000) {
                    ESP_LOGW("BLEGW", "ACK timeout %s, skip", current_cmd_.device_id.c_str());
                    auto dev = find_dev(current_cmd_.device_id);
                    if (dev) dev->pending_ack = false;
                    state_ = IDLE;
                    cmd_queue_.pop();
                }
                break;
        }
    }

    // ===== 扫描解析：134D 协议（与蓝牙代理共享扫描器）=====
    bool parse_device(const esp32_ble_tracker::ESPBTDevice& device) override {
        auto& raw = device.get_manufacturer_data();
        if (raw.size() < 20) return false;
        
        // 134D 标识检查
        if (raw.size() >= 4 && raw[0] == 0x02 && raw[1] == 0x01 && 
            raw[3] == 0x1B && raw[4] == 0xFF && raw[5] == 0x11 && raw[6] == 0x4D) {
            return parse_134d(raw);
        }
        
        return false;
    }

    // ===== HA API 入口 =====
    void ha_control(std::string device_id, std::string command, std::string param) {
        ESP_LOGI("BLEGW", "HA: %s -> %s(%s)", device_id.c_str(), command.c_str(), param.c_str());
        enqueue(device_id, command, param);
    }
    
    // ===== MQTT 入口 =====
    void on_mqtt_message(const std::string& topic, const std::string& payload) {
        std::string device_id = extract_id(topic);
        auto [cmd, param] = parse_payload(payload);
        ESP_LOGI("BLEGW", "MQTT: %s -> %s(%s)", device_id.c_str(), cmd.c_str(), param.c_str());
        enqueue(device_id, cmd, param);
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

    // ===== 134D 广播解析 =====
    bool parse_134d(const std::vector<uint8_t>& raw) {
        for (auto& dev : devices_) {
            uint8_t target[6];
            sscanf(dev.mac.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                   &target[5], &target[4], &target[3],
                   &target[2], &target[1], &target[0]);
            
            // 在 manufacturer data 中搜索 MAC
            for (size_t i = 7; i <= raw.size() - 6; i++) {
                if (memcmp(&raw[i], target, 6) != 0) continue;
                
                // 解析状态（基于你给的 134D 格式）
                bool power_on = (raw[i + 7] == 0x01);
                uint16_t brt_raw = (raw[i + 12] << 8) | raw[i + 13];
                int brt_pct = (brt_raw == 0xFFFF) ? 100 : (int)(brt_raw / 655.35);
                uint8_t state_byte = raw[i + 14];
                bool fan_run = (state_byte == 0x13 || state_byte == 0x03);
                int fan_spd = fan_run ? (raw[i + 15] + 1) : 0;
                
                // 更新真实状态
                dev.state_led = power_on;
                dev.state_brightness = brt_pct;
                dev.state_fan = fan_run;
                dev.state_fan_speed = fan_spd;
                dev.state_fan_dir = fan_run ? "Forward" : "Off";
                
                // 检查是否在等确认
                if (state_ == WAIT_ACK && dev.pending_ack && dev.id == current_cmd_.device_id) {
                    ESP_LOGI("BLEGW", "ACK OK %s", dev.id.c_str());
                    dev.pending_ack = false;
                    state_ = IDLE;
                    cmd_queue_.pop();
                }
                
                // 上报状态（无论是否等确认）
                publish_state(dev);
                return true;
            }
        }
        return false;
    }

    // ===== 指令入队 =====
    void enqueue(const std::string& device_id, const std::string& command, 
                 const std::string& param) {
        auto dev = find_dev(device_id);
        if (!dev) {
            ESP_LOGW("BLEGW", "Unknown device: %s", device_id.c_str());
            return;
        }
        
        std::string action = map_action(command, param);
        for (auto& act : dev->actions) {
            if (act.name == action) {
                cmd_queue_.push({device_id, act.start_pkt, act.end_pkt, action});
                ESP_LOGI("BLEGW", "Queued %s -> %s", device_id.c_str(), action.c_str());
                return;
            }
        }
        ESP_LOGW("BLEGW", "Unknown action: %s", action.c_str());
    }
    
    // ===== 开始发送 =====
    void start_sending() {
        current_cmd_ = cmd_queue_.front();
        auto dev = find_dev(current_cmd_.device_id);
        if (dev) {
            dev->pending_ack = true;
            dev->pending_action = current_cmd_.action;
            dev->pending_time = millis();
        }
        
        send_packet(current_cmd_.start_pkt);
        state_ = SENDING_START;
        last_action_time_ = millis();
        ESP_LOGD("BLEGW", "Start %s -> %s", current_cmd_.device_id.c_str(), current_cmd_.action.c_str());
    }
    
    // ===== 底层发包（与代理分时复用射频）=====
    void send_packet(const std::vector<uint8_t>& data) {
        esp_ble_adv_data_t adv_data = {};
        adv_data.set_scan_rsp = false;
        adv_data.include_name = false;
        adv_data.include_txpower = false;
        adv_data.manufacturer_len = data.size();
        adv_data.p_manufacturer_data = (uint8_t*)data.data();
        adv_data.flag = 0x6;
        
        // 配置并启动广播
        esp_err_t err = esp_ble_gap_config_adv_data(&adv_data);
        if (err != ESP_OK) {
            ESP_LOGE("BLEGW", "adv config failed: %d", err);
            return;
        }
        
        err = esp_ble_gap_start_advertising(&adv_params_);
        if (err != ESP_OK) {
            ESP_LOGE("BLEGW", "adv start failed: %d", err);
            return;
        }
        
        // 短暂广播后停止，让出射频给代理扫描
        delay(25);  // 至少发 1-2 个广播包
        esp_ble_gap_stop_advertising();
    }
    
    // ===== 状态上报（HA + MQTT 同步）=====
    void publish_state(const FanLight& dev) {
        std::string json = "{";
        json += "\"led\":\"" + std::string(dev.state_led ? "ON" : "OFF") + "\",";
        json += "\"brightness\":" + std::to_string(dev.state_brightness) + ",";
        json += "\"color_temp\":" + std::to_string(dev.state_color_temp) + ",";
        json += "\"fan\":\"" + std::string(dev.state_fan ? "ON" : "OFF") + "\",";
        json += "\"fan_speed\":" + std::to_string(dev.state_fan_speed) + ",";
        json += "\"fan_dir\":\"" + dev.state_fan_dir + "\"";
        json += "}";
        
        std::string topic = "esp32/" + dev.id + "/state";
        publish(topic, json);
        
        ESP_LOGD("STATE", "%s: LED=%s BRI=%d FAN=%s SPD=%d", 
                 dev.name.c_str(),
                 dev.state_led ? "ON" : "OFF",
                 dev.state_brightness,
                 dev.state_fan ? "ON" : "OFF",
                 dev.state_fan_speed);
    }

    // ===== 工具函数 =====
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
            if (s < 1) s = 1;
            if (s > 6) s = 6;
            return "speed_" + std::to_string(s);
        }
        return cmd;
    }
    
    std::string extract_id(const std::string& topic) {
        size_t s = topic.find('/') + 1;
        size_t e = topic.find('/', s);
        if (e == std::string::npos) return topic.substr(s);
        return topic.substr(s, e - s);
    }
    
    std::pair<std::string, std::string> parse_payload(const std::string& p) {
        if (p.empty()) return {"", ""};
        if (p.front() != '{') return {p, ""};
        
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
            // 尝试数字参数
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
