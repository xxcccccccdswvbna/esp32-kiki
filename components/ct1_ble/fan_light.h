
#pragma once
#include <string>
#include <vector>

struct ActionPayload {
    std::string name;
    std::vector<uint8_t> start_pkt;
    std::vector<uint8_t> end_pkt;
};

struct FanLight {
    std::string id;
    std::string mac;
    std::string name;
    std::vector<ActionPayload> actions;
    
    // 真实状态（仅广播解析更新）
    bool state_led = false;
    int state_brightness = 0;
    int state_color_temp = 2700;
    bool state_fan = false;
    int state_fan_speed = 0;
    std::string state_fan_dir = "Off";
    
    // 等待确认
    bool pending_ack = false;
    std::string pending_action;
    uint32_t pending_time = 0;
};

inline std::vector<uint8_t> hex_to_bytes(const std::string& hex) {
    std::vector<uint8_t> out;
    std::string h = hex;
    if (h.substr(0, 2) == "0x") h = h.substr(2);
    for (size_t i = 0; i + 1 < h.length(); i += 2) {
        out.push_back(strtol(h.substr(i, 2).c_str(), nullptr, 16));
    }
    return out;
}

inline std::vector<FanLight> get_devices() {
    std::vector<FanLight> devs;
    
    // ===== 设备 1: 餐厅风扇灯 =====
    {
        FanLight d;
        d.id = "device_cct";
        d.mac = "00:00:70:27:80:48";
        d.name = "餐厅风扇灯";
        
        d.actions.push_back({"on",
            hex_to_bytes("0201021BFF114D191848802770000001819721277070005FC86FB84848A7F8"),
            hex_to_bytes("0201021BFF114D191848802770000001819727277070005FC86FB84848A7F2")});
        d.actions.push_back({"off",
            hex_to_bytes("0201021BFF114D191A4880277000000126277670005FC86FB84848A7F08088"),
            hex_to_bytes("0201021BFF114D19174880277000000181809727277070005FC86FB84848A5")});
        d.actions.push_back({"brightness_1",
            hex_to_bytes("0201021BFF114D191848802770000001819676257070005FC86FB84848A7A6"),
            hex_to_bytes("0201021BFF114D1911488027700000016EB84848A7F080809727277070005D")});
        d.actions.push_back({"brightness_20",
            hex_to_bytes("0201021BFF114D19144880277000000149A6A1B3809727277070005FC86F3F"),
            hex_to_bytes("0201021BFF114D191F488027700000015EC86FB84848A7F080809727277072")});
        d.actions.push_back({"brightness_40",
            hex_to_bytes("0201021BFF114D191B48802770000001267121665FC86FB84848A7F080802D"),
            hex_to_bytes("0201021BFF114D19144880277000000149A7F080809727277070005FC86FBA")});
        d.actions.push_back({"brightness_50",
            hex_to_bytes("0201021BFF114D191248802770000001B94919D8F080809727277070005F1B"),
            hex_to_bytes("0201021BFF114D19144880277000000149A7F080809727277070005FC86FBA")});
        d.actions.push_back({"brightness_60",
            hex_to_bytes("0201021BFF114D191948802770000001962676E970005FC86FB84848A7F06D"),
            hex_to_bytes("0201021BFF114D1913488027700000014948A7F080809727277070005FC86D")});
        d.actions.push_back({"brightness_80",
            hex_to_bytes("0201021BFF114D1913488027700000014949F63C80809727277070005FC84F"),
            hex_to_bytes("0201021BFF114D1911488027700000016EB84848A7F080809727277070005D")});
        d.actions.push_back({"brightness_100",
            hex_to_bytes("0201021BFF114D191B48802770000001267121FF5FC86FB84848A7F08080C4"),
            hex_to_bytes("0201021BFF114D191E48802770000001015FC86FB84848A7F0808097272772")});
        d.actions.push_back({"color_2700",
            hex_to_bytes("0201021BFF114D1917488027700000018181C227277070005FC86FB84848FF"),
            hex_to_bytes("0201021BFF114D191C488027700000017170005FC86FB84848A7F080809725")});
        d.actions.push_back({"color_3500",
            hex_to_bytes("0201021BFF114D191248802770000001B9491DDDF080809727277070005F1A"),
            hex_to_bytes("0201021BFF114D191A4880277000000126277070005FC86FB84848A7F08082")});
        d.actions.push_back({"color_6500",
            hex_to_bytes("0201021BFF114D1917488027700000018181C2D8277070005FC86FB84848F0"),
            hex_to_bytes("0201021BFF114D191A4880277000000126277070005FC86FB84848A7F08082")});
        d.actions.push_back({"fan_on",
            hex_to_bytes("0201021BFF114D19184880277000000181972E277070005FC86FB84848A7FB"),
            hex_to_bytes("0201021BFF114D191F488027700000015EC86FB84848A7F080809727277072")});
        d.actions.push_back({"fan_off",
            hex_to_bytes("0201021BFF114D191648802770000001F180899727277070005FC86FB84843"),
            hex_to_bytes("0201021BFF114D1919488027700000019627277070005FC86FB84848A7F082")});
        d.actions.push_back({"speed_1",
            hex_to_bytes("0201021BFF114D191A4880277000000126276970005FC86FB84848A7F0809B"),
            hex_to_bytes("0201021BFF114D19174880277000000181809727277070005FC86FB84848A5")});
        d.actions.push_back({"speed_2",
            hex_to_bytes("0201021BFF114D191048802770000001C96FA24848A7F0808097272770701C"),
            hex_to_bytes("0201021BFF114D191548802770000001A6F080809727277070005FC86FB84A")});
        d.actions.push_back({"speed_3",
            hex_to_bytes("0201021BFF114D191D488027700000017100DEC86FB84848A7F080809727A4"),
            hex_to_bytes("0201021BFF114D191548802770000001A6F080809727277070005FC86FB84A")});
        d.actions.push_back({"speed_4",
            hex_to_bytes("0201021BFF114D1918488027700000018197AF277070005FC86FB84848A77A"),
            hex_to_bytes("0201021BFF114D191C488027700000017170005FC86FB84848A7F080809725")});
        d.actions.push_back({"speed_5",
            hex_to_bytes("0201021BFF114D191A488027700000012627F570005FC86FB84848A7F08007"),
            hex_to_bytes("0201021BFF114D191C488027700000017170005FC86FB84848A7F080809725")});
        d.actions.push_back({"speed_6",
            hex_to_bytes("0201021BFF114D191548802770000001A6F080809727277070005FC86FB84A"),
            hex_to_bytes("0201021BFF114D1911488027700000016EB8CE48A7F08080972727707000D7")});
        d.actions.push_back({"forward",
            hex_to_bytes("0201021BFF114D1913488027700000014948BBF080809727277070005FC871"),
            hex_to_bytes("0201021BFF114D191648802770000001F180809727277070005FC86FB8484A")});
        d.actions.push_back({"reverse",
            hex_to_bytes("0201021BFF114D191648802770000001F1809C9727277070005FC86FB84856"),
            hex_to_bytes("0201021BFF114D191C488027700000017170005FC86FB84848A7F080809725")});
        
        devs.push_back(d);
    }
    
    // ===== 设备 2: 次卧风扇灯 =====
    {
        FanLight d;
        d.id = "device_ccw";
        d.mac = "00:00:70:2d:cf:f0";
        d.name = "次卧风扇灯";
        
        d.actions.push_back({"on",
            hex_to_bytes("0201021BFF114D1917F0CF2D70000001CECF9B2D2D7070005CBF1D60F0F0F4"),
            hex_to_bytes("0201021BFF114D191AF0CF2D700000012C2D7070005CBF1D60F0F0FC3FCFCD")});
        d.actions.push_back({"off",
            hex_to_bytes("0201021BFF114D191AF0CF2D700000012C2D7670005CBF1D60F0F0FC3FCFC7"),
            hex_to_bytes("0201021BFF114D191AF0CF2D700000012C2D7070005CBF1D60F0F0FC3FCFCD")});
        d.actions.push_back({"brightness_1",
            hex_to_bytes("0201021BFF114D191DF0CF2D7000000171010DBD1D60F0F0FC3FCFCF9D2D7B"),
            hex_to_bytes("0201021BFF114D191EF0CF2D70000001015CBF1D60F0F0FC3FCFCF9D2D2D72")});
        d.actions.push_back({"brightness_20",
            hex_to_bytes("0201021BFF114D191CF0CF2D7000000171715169BF1D60F0F0FC3FCFCF9DA4"),
            hex_to_bytes("0201021BFF114D191CF0CF2D700000017170005CBF1D60F0F0FC3FCFCF9D2F")});
        d.actions.push_back({"brightness_40",
            hex_to_bytes("0201021BFF114D191BF0CF2D700000012C7121685CBF1D60F0F0FC3FCFCF21"),
            hex_to_bytes("0201021BFF114D191CF0CF2D700000017170005CBF1D60F0F0FC3FCFCF9D2F")});
        d.actions.push_back({"brightness_50",
            hex_to_bytes("0201021BFF114D1910F0CF2D70000001BE1C318FF0FC3FCFCF9D2D2D7070D3"),
            hex_to_bytes("0201021BFF114D1914F0CF2D70000001F1FC3FCFCF9D2D2D7070005CBF1D62")});
        d.actions.push_back({"brightness_60",
            hex_to_bytes("0201021BFF114D191EF0CF2D70000001015DEE8B60F0F0FC3FCFCF9D2D2D9A"),
            hex_to_bytes("0201021BFF114D1910F0CF2D70000001BE1D60F0F0FC3FCFCF9D2D2D707002")});
        d.actions.push_back({"brightness_80",
            hex_to_bytes("0201021BFF114D1913F0CF2D70000001F1F1ADF6CFCF9D2D2D7070005CBF00"),
            hex_to_bytes("0201021BFF114D191DF0CF2D7000000171005CBF1D60F0F0FC3FCFCF9D2D2F")});
        d.actions.push_back({"brightness_100",
            hex_to_bytes("0201021BFF114D191FF0CF2D700000015DBE4C9FF0F0FC3FCFCF9D2D2D7023"),
            hex_to_bytes("0201021BFF114D1915F0CF2D70000001FD3FCFCF9D2D2D7070005CBF1D60F2")});
        d.actions.push_back({"color_2700",
            hex_to_bytes("0201021BFF114D191AF0CF2D700000012C2C2570005CBF1D60F0F0FC3FCF97"),
            hex_to_bytes("0201021BFF114D1918F0CF2D70000001CE9D2D2D7070005CBF1D60F0F0FC3D")});
        d.actions.push_back({"color_3500",
            hex_to_bytes("0201021BFF114D1912F0CF2D7000000161F1A5893FCFCF9D2D2D7070005C72"),
            hex_to_bytes("0201021BFF114D1914F0CF2D70000001F1FC3FCFCF9D2D2D7070005CBF1D62")});
        d.actions.push_back({"color_6500",
            hex_to_bytes("0201021BFF114D1912F0CF2D7000000161F1A5033FCFCF9D2D2D7070005CE8"),
            hex_to_bytes("0201021BFF114D191AF0CF2D700000012C2D7070005CBF1D60F0F0FC3FCFCD")});
        d.actions.push_back({"fan_on",
            hex_to_bytes("0201021BFF114D1911F0CF2D700000011C60F9F0FC3FCFCF9D2D2D70700057"),
            hex_to_bytes("0201021BFF114D191DF0CF2D7000000171005CBF1D60F0F0FC3FCFCF9D2D2F")});
        d.actions.push_back({"fan_off",
            hex_to_bytes("0201021BFF114D191BF0CF2D700000012C7079005CBF1D60F0F0FC3FCFCF96"),
            hex_to_bytes("0201021BFF114D1918F0CF2D70000001CE9D2D2D7070005CBF1D60F0F0FC3D")});
        d.actions.push_back({"speed_1",
            hex_to_bytes("0201021BFF114D1913F0CF2D70000001F1F0E53FCFCF9D2D2D7070005CBF06"),
            hex_to_bytes("0201021BFF114D1915F0CF2D70000001FD3FCFCF9D2D2D7070005CBF1D60F2")});
        d.actions.push_back({"speed_2",
            hex_to_bytes("0201021BFF114D1915F0CF2D70000001FD3FD5CF9D2D2D7070005CBF1D60EC"),
            hex_to_bytes("0201021BFF114D1915F0CF2D70000001FD3FCFCF9D2D2D7070005CBF1D60F2")});
        d.actions.push_back({"speed_3",
            hex_to_bytes("0201021BFF114D1911F0CF2D700000011C6071F0FC3FCFCF9D2D2D707000DF"),
            hex_to_bytes("0201021BFF114D1915F0CF2D70000001FD3FCFCF9D2D2D7070005CBF1D60F2")});
        d.actions.push_back({"speed_4",
            hex_to_bytes("0201021BFF114D191BF0CF2D700000012C70F8005CBF1D60F0F0FC3FCFCF17"),
            hex_to_bytes("0201021BFF114D1913F0CF2D70000001F1F0FC3FCFCF9D2D2D7070005CBF1F")});
        d.actions.push_back({"speed_5",
            hex_to_bytes("0201021BFF114D1917F0CF2D70000001CECF182D2D7070005CBF1D60F0F07B"),
            hex_to_bytes("0201021BFF114D1919F0CF2D700000019C2D2D7070005CBF1D60F0F0FC3FCD")});
        d.actions.push_back({"speed_6",
            hex_to_bytes("0201021BFF114D191BF0CF2D700000012C70F6005CBF1D60F0F0FC3FCFCF15"),
            hex_to_bytes("0201021BFF114D1917F0CF2D70000001CECF9D2D2D7070005CBF1D60F0F0FE")});
        d.actions.push_back({"forward",
            hex_to_bytes("0201021BFF114D191AF0CF2D700000012C2D6C70005CBF1D60F0F0FC3FCFD1"),
            hex_to_bytes("0201021BFF114D1911F0CF2D700000011C60F0F0FC3FCFCF9D2D2D7070005E")});
        d.actions.push_back({"reverse",
            hex_to_bytes("0201021BFF114D1918F0CF2D70000001CE9D312D7070005CBF1D60F0F0FC21"),
            hex_to_bytes("0201021BFF114D1919F0CF2D700000019C2D2D7070005CBF1D60F0F0FC3FCD")});
        
        devs.push_back(d);
    }
    
    return devs;
}
