import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import mqtt
from esphome.const import CONF_ID

# 定义命名空间
ct1_ble_ns = cg.esphome_ns.namespace("ct1_ble")
CT1BLE = ct1_ble_ns.class_("CT1BLE", cg.Component, mqtt.MQTTDevice)

CONF_CT1_BLE_ID = "id"

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(CT1BLE),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    return var
