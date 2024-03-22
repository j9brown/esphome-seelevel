import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID, CONF_RX_PIN, CONF_TX_PIN

CODEOWNERS = ["@j9brown"]

MULTI_CONF = True

CONF_SEELEVEL_ID = "seelevel_id"

seelevel_ns = cg.esphome_ns.namespace("seelevel")

SeelevelComponent = seelevel_ns.class_("SeelevelComponent", cg.Component)

CONFIG_SCHEMA = cv.All(
    cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(SeelevelComponent),
            cv.Required(CONF_RX_PIN): pins.gpio_input_pullup_pin_schema,
            cv.Required(CONF_TX_PIN): pins.gpio_output_pin_schema,
        }
    )
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_rx_pin(await cg.gpio_pin_expression(config[CONF_RX_PIN])))
    cg.add(var.set_tx_pin(await cg.gpio_pin_expression(config[CONF_TX_PIN])))
