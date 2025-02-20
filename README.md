# esphome-seelevel

An ESPHome component for reading the Garnet SeeLeveL tank sensor.

## Interface

Refer to the seelevel-inteface for a schematic diagram of the circuit needed to
communicate with the sensors.

Connect `RX` to an input pin on the microcontroller with an internal or external pull-up resistor
to the microcontroller's positive rail (typically +3.3 VDC).  100 kOhm works well.

Connect `TX` to an output pin on the microcontroller.

Connect `SENSOR_DATA` to the SeeLevelL sensor's blue wire.

Connect `SENSOR_GND` to the SeeLevelL sensor's black wire.

Connect `+12V` to the power supply positive. The sensor and circuit should be able to tolerate
voltages roughly between +9 VDC and +15 VDC.

Connect `GND` to the power supply ground (can be a chassis ground) and to the microcontroller's ground.

## Configuration

```yaml
# Import the component
external_components:
  - source: github://j9brown/esphome-seelevel@main
    components: [ seelevel ]

# Declare the transceiver interface
seelevel:
    # Transceiver component ID, optional unless there are multiple transceivers
    id: seelevel_transceiver
    # Receive pin
    rx_pin: 32
    # Transmit pin
    tx_pin: 33

# Declare the tank sensors
sensor:
  - platform: seelevel
    # Transceiver component ID, optional unless there are multiple transceivers
    seelevel_id: seelevel_transceiver
    # Tank number for which the sensor has been configured (by cutting jumpers)
    # - Fresh water: 1
    # - Gray water: 2
    # - Black water: 3
    tank: 1
    # The number of active segments the sensor has, it may have fewer if some were cut off
    segments: 9
    # Optionally declares a water level sensor whose value ranges from 0 to a little
    # more than the number of segments. Accepts all Sensor parameters, including filters.
    level:
      name: "Water Level"
    # Optionally declares a water volume sensor by linearly interpolating a mapping from
    # level to volume.  Accepts all Sensor parameters, including filters.
    volume:
      name: "Water Volume"
      # Maps from level to volume
      # Requires at least two entries, can have more for tanks of non-uniform volume
      # Entries must be arranged in non-decreasing order in both level and in volume.
      # Valid units of volume are: 'gal' (gallon) and 'L' (liter).
      # Note: The sensor's native unit of measurement is the liter even when gallons are
      #       used to define the mapping.
      map:
        - { level: 1.0, volume: 0 L }
        - { level: 5.0, volume: 25 L }
        - { level: 9.0, volume: 40 L }
      # Optionally inverts the sense of the reported volume (default is false)
      # If true, reports the remaining capacity of the tank
      # If false, reports how much liquid it contains
      invert: true
      # Apply sensor filters to reduce noise.
      filters:
        - delta: 0.5
    # Optionally declares a diagnostic sensor containing the raw data retrieved from the tank.
    # Reports a comma-delimited array of values that represents the analog signal level of each
    # sensor segment starting from the lowest one. Each value ranges from 0 to 255.
    segment_data:
      name: "Segment Data"
    # Polling interal, default to 60s, use 'never' to disable automatic polling
    update_interval: 10s
```
