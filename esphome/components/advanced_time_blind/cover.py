from esphome import pins
import esphome.codegen as cg
from esphome.components import cover
import esphome.config_validation as cv

# Configuration keys
CONF_PIN_UP = "pin_up"
CONF_PIN_DOWN = "pin_down"
CONF_TIME_UP = "time_up"
CONF_TIME_DOWN = "time_down"
CONF_START_OFFSET_UP = "start_offset_up"
CONF_START_OFFSET_DOWN = "start_offset_down"
CONF_ENDSTOP_EXTRA_TIME = "endstop_extra_time"

advanced_time_blind_ns = cg.esphome_ns.namespace("advanced_time_blind")
AdvancedTimeBlind = advanced_time_blind_ns.class_(
    "AdvancedTimeBlind", cover.Cover, cg.Component
)

# Configuration schema
CONFIG_SCHEMA = (
    cover.cover_schema(AdvancedTimeBlind)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(
        {
            # Pin configuration
            cv.Required(CONF_PIN_UP): pins.gpio_output_pin_schema,
            cv.Required(CONF_PIN_DOWN): pins.gpio_output_pin_schema,
            # Timing
            cv.Required(CONF_TIME_UP): cv.positive_time_period_milliseconds,
            cv.Required(CONF_TIME_DOWN): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_START_OFFSET_UP, default="0s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_START_OFFSET_DOWN, default="0s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_ENDSTOP_EXTRA_TIME, default="2s"
            ): cv.positive_time_period_milliseconds,
        }
    )
)


async def to_code(config):
    var = await cover.new_cover(config)
    await cg.register_component(var, config)

    # Set configuration
    pin_up = await cg.gpio_pin_expression(config[CONF_PIN_UP])
    cg.add(var.set_pin_up(pin_up))
    pin_down = await cg.gpio_pin_expression(config[CONF_PIN_DOWN])
    cg.add(var.set_pin_down(pin_down))
    cg.add(var.set_time_up(config[CONF_TIME_UP]))
    cg.add(var.set_time_down(config[CONF_TIME_DOWN]))
    cg.add(var.set_start_offset_up(config[CONF_START_OFFSET_UP]))
    cg.add(var.set_start_offset_down(config[CONF_START_OFFSET_DOWN]))
    cg.add(var.set_endstop_extra_time(config[CONF_ENDSTOP_EXTRA_TIME]))
