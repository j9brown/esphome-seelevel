import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_VOLUME_STORAGE,
    STATE_CLASS_MEASUREMENT,
)
from .. import SeelevelComponent, CONF_SEELEVEL_ID, seelevel_ns

DEPENDENCIES = ["seelevel"]

CONF_TANK = "tank"
CONF_SEGMENTS = "segments"
CONF_LEVEL = "level"
CONF_VOLUME = "volume"
CONF_MAP = "map"

SeelevelSensor = seelevel_ns.class_("SeelevelSensor", cg.PollingComponent)

def volume(value):
    err = None
    try:
        return cv.float_with_unit("volume", "(L| L)?")(value)
    except cv.Invalid as orig_err:
        err = orig_err

    try:
        gallon = cv.float_with_unit("volume", "(gal| gal)?")(value)
        return gallon * 3.78541178
    except cv.Invalid:
        pass

    raise err

def check_monotonic(obj):
    level = 0
    volume = 0
    for item in obj:
        if item[CONF_LEVEL] < level or item[CONF_VOLUME] < volume:
            raise cv.Invalid("The map of level to volume must be monotonic in level and in volume")
        level = item[CONF_LEVEL]
        volume = item[CONF_VOLUME]
    return obj

CONFIG_SCHEMA = cv.All(
    cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_SEELEVEL_ID): cv.use_id(SeelevelComponent),
            cv.GenerateID(): cv.declare_id(SeelevelSensor),
            cv.Optional(CONF_TANK, default=1): cv.All(int, cv.Range(min=1, max=4)),
            cv.Optional(CONF_SEGMENTS, default=9): cv.All(int, cv.Range(min=1, max=10)),
            cv.Optional(CONF_LEVEL): sensor.sensor_schema(
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_VOLUME): sensor.sensor_schema(
                unit_of_measurement="L",
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_VOLUME_STORAGE,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend({
                cv.Required(CONF_MAP): cv.All(
                    cv.ensure_list({
                        cv.Required(CONF_LEVEL): cv.All(float, cv.Range(min=0, max=10)),
                        cv.Required(CONF_VOLUME): volume,
                    }),
                    cv.Length(min=1),
                    check_monotonic,
                ),
            })
        }
    ).extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_SEELEVEL_ID])
    var = cg.new_Pvariable(config[CONF_ID], parent)
    await cg.register_component(var, config)
    cg.add(var.set_tank(config[CONF_TANK]))
    cg.add(var.set_segments(config[CONF_SEGMENTS]))

    if CONF_LEVEL in config:
        sens = await sensor.new_sensor(config[CONF_LEVEL])
        cg.add(var.set_level_sensor(sens))

    if CONF_VOLUME in config:
        sens = await sensor.new_sensor(config[CONF_VOLUME])
        cg.add(var.set_volume_sensor(sens))
        for item in config[CONF_VOLUME][CONF_MAP]:
            cg.add(var.append_volume_mapping(item[CONF_LEVEL], item[CONF_VOLUME]))
