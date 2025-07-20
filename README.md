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
    # sensor segment starting from the lowest one. Each value ranges from 0 (no water) to 255 (likely adjacent to water or
    # the segment was cut off when the sensor was shortened).
    segment_data:
      name: "Segment Data"
    # Polling interal, default to 60s, use 'never' to disable automatic polling
    update_interval: 10s
```

## How segment data translates to a water level

The tank sensor is adhered to the side of the water tank. Along its length there are several electrodes that are used to sense the presence of water at different depths. The installer can shorten sensors fit shallower tanks by cutting off excess electrodes.

Each electrode acts as one plate of a capacitor and the water within the tank adjacent to the electrode acts as the second plate. The tank sensor applies a current to each electrode and measures how long it takes to charge or discharge the plate and thereby determines the capacitance between the electrode and the water. It works a lot like a capacitive touch sensor.

Each electrode provides an indication of the presence of water at a certain depth in the water column. We refer to the active regions around the electrodes as segments. The tank sensor always reports data for 10 segments even if it physically has fewer than 10 electrodes.

For each of the 10 segments, the tank sensor reports a value between 0 and 255. A value of 0 indicates minimal capacitance and signifies a probable absence of water adjacent to the segment's electrode. A value of 255 indicates maximal capacitance and signifies a probable presence of water adjacent to the electrode. Segments that don't have a corresponding electrode (such as when electrodes are cut off) also report a value of 255 and should be ignored.

Collectively, the array of capacitance measurements taken by the sensor is called the segment data. The reported units of capacitance in the segment data are unknown (but they probably proportional to some unit of time).

These capacitance measurements are inherently imprecise and they are influenced by environmental factors. Electrodes don't just sense water directly adjacent to them. Placing objects near the sensor can skew the segment data. The signal near the air/water boundary generally seems stronger than the signal near the bottom of the tank. And even without adding or removing water from the tank, the segment data changes over the course of the day in ways that can perhaps be attributed to ambient temperature variations.

Accurately estimating the water level from the segment data is difficult. One simple approach is to find the segment nearest the top of the tank whose reported signal is above a certain threshold but much better results can be obtained with some interpolation.

The algorithm originally used by the SeeleveL monitor is unknown.

The algorithm currently used by this component is based on the observation that the signal tends to increase monotonically for segments that are closer to the air/liquid boundary and it is lower for segments near the bottom of the tank. It searches for a drop-off in the signal from one segment to the next and interpolates to determine the water level. This algorithm seems to provide good qualitative indications of incremental changes in water level although it is not perfectly linear in practice.

Note that this algorithm does not require any baseline measurements for calibrating the capacitive sensor because that would probably require filling or emptying the tank (which is inconvenient). It's unclear whether such calibration would be sufficiently stable over the long term to be reliable.

If you have ideas for improving the accuracy of the water level estimates from segment data, please give it a try and contact the author if you come up with something better. There's a script that you can use to try out different algorithms. See [decode.py](./decode.py).

Once the water level has been estimated from the segment data, it is converted to a water volume based on the level-to-volume map and filtered as specified in the ESPHome configuration.

And that's it!

## Limitations

The implementation does not currently handle multiple sensor aggregation for deep tanks that need more segments than a single sensor has.

## Acknowledgements

This project was inspired by a discussion in the Raspberry Pi forum entitled [Read signal of an RV Tank Sensor](https://forums.raspberrypi.com/viewtopic.php?t=119614) from September 2015. We'd like to thank the folks who reverse-engineered the protocol and published their results.

## Notice

The esphome-seelevel software, documentation, design, and all copyright protected artifacts are released under the terms of the [MIT license](LICENSE).

The esphome-seelevel hardware is released under the terms of the [CERN-OHL-W license](seelevel-interface/LICENSE).
