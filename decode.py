#! env python3
# Processes segment data from a Seelevel tank sensor into a liquid level.
# This script is useful for comparing the behavior of different algorithms.
#
# Usage:
#
# Configure ESPHome with a seelevel sensor and enable the segment_data sensor.
# Open the History tab in Home Assistant.
# Pick a least one segment data sensor entities to decode and a time range.
# Download the data as CSV.
# Run this script, passing the downloaded history.csv file as input.
# You may need to widen your terminal to view the plot without line wrapping.
#
# This script is not fully parameterized at the command-line and it is meant to
# be edited while running experiments.

from collections import namedtuple
import csv
from datetime import datetime
import sys


# Determines the liquid level from the highest segment whose signal level is
# above a fixed threshold.
def decode_stepwise(segments):
    THRESHOLD = 120
    count = len(segments)
    for i in range(0, count):
        if segments[i] < THRESHOLD:
            return i
    return count


# Determines the liquid level proportional to the signal level of the last non-full
# segment based on a dynamically determined threshold. Makes dubious assumptions about
# the likely signal level range when we don't have a wide distribution to work with.
def decode_proportional(segments):
    count = len(segments)
    high = min(max(max(segments), 120), 200)
    low = max(min(min(segments), high * 0.25), 20)
    thresh = low + (high - low) * 0.6
    for i in range(0, count):
        if segments[i] < thresh:
            return round(i + min(max(segments[i] - low, 0) / (thresh - low), 1), 1)
    return count


# Determines the liquid level by searching for a drop-off in signal level across the
# segments then allocating a proportion based on an assumed noise floor. This one
# is based on the observation that signal level tends to increase monotonically
# closer to the air/liquid boundary (so it is lower at the bottom of the tank).
def decode_boundary(segments):
    count = len(segments)
    thresh = 120
    for i in range(0, count):
        x = segments[i]
        if x < thresh:
            low = thresh / 3
            return round(i + max((x - low) / (thresh - low), 0), 1)
        thresh = max(thresh, segments[i] * 0.9)
    return count


DECODERS = [decode_stepwise, decode_boundary]

COLLATION_INTERVAL = 15
GRAPHIC_MAX_LEVEL = 9
GRAPHIC_COLUMNS = 60
DEBUG_RAW_SAMPLES = False

Sample = namedtuple('Sample', ('time', 'levels', 'raw'))


def plot(series, aliases):
    for entity, alias in aliases.items():
        print(f'{alias}: {entity}')
    print()

    last_levels = None
    levels = {}
    empty = b' ' * GRAPHIC_COLUMNS
    for sample in series:
        levels.update(sample.levels)
        if levels == last_levels:
            continue
        last_levels = dict(levels)

        graphic = bytearray(empty)
        labels = ''
        for alias, level in levels.items():
            if level is None:
                continue
            pos = round((level / GRAPHIC_MAX_LEVEL) * (GRAPHIC_COLUMNS - 3))
            if pos < 0:
                graphic[0] = ord('<')
            elif pos > GRAPHIC_COLUMNS - 2:
                graphic[-1] = ord('>')
            else:
                graphic[pos + 1] = ord(alias)
            labels += ' %c %0.1f ' % (alias, level)
        if levels.get('F') is not None and levels.get('G') is not None:
            labels += ' F+G %0.1f ' % (levels['F'] + levels['G'])
        if levels.get('f') is not None and levels.get('g') is not None:
            labels += ' f+g %0.1f ' % (levels['f'] + levels['g'])
        print(f'{sample.time.astimezone().strftime('%d/%m/%y %H:%M:%S')} -{graphic.decode()}-{labels}')
        if DEBUG_RAW_SAMPLES:
            for alias in sorted(sample.raw.keys()):
                print(f'  \u001b[2m{alias}: {sample.raw[alias]}\u001b[0m')


def make_pretty_alias(entity):
    if entity.find('gray') != -1 or entity.find('grey') != -1:
        return 'G'
    elif entity.find('fresh') != -1:
        return 'F'
    elif entity.find('black') != -1:
        return 'B'
    return 'W'


def main() -> int:
    if len(sys.argv) != 2:
        print(f'Usage: {sys.argv[0]} history.csv')
        return 1

    # Parse the file into a decoded time series
    aliases = {}
    series = []
    with open(sys.argv[1], newline='') as file:
        reader = csv.reader(file)
        header = next(reader)
        if header != ['entity_id', 'state', 'last_changed']:
            print('Did not find expected header, is this actually a history.csv file?')
            return 1
        for row in reader:
            if len(row) != 3:
                print(f'Malformed row: ${row}')
                continue
            entity = row[0]
            state = row[1]
            segments = [int(x) for x in state.split(',')] if state != 'unknown' and state != 'unavailable' else None
            time = datetime.fromisoformat(row[2])
            alias = aliases.get(entity)
            if alias is None:
                alias = make_pretty_alias(entity)
                while alias in aliases.values():
                    alias = chr(ord(alias) + 1)
                aliases[entity] = alias
            raw = { alias: segments }
            levels = {}
            for decoder in DECODERS:
                levels[alias] = decoder(segments) if segments is not None else None
                alias = alias.lower() # FIXME: This only works for up to two decoders
            series.append(Sample(time, levels, raw))

    # Collate samples that are closely spaced in time
    series.sort(key=lambda sample: sample.time)
    collated_series = []
    last_sample = None
    for sample in series:
        if last_sample is not None and (sample.time - last_sample.time).total_seconds() < COLLATION_INTERVAL:
            last_sample.levels.update(sample.levels)
            last_sample.raw.update(sample.raw)
        else:
            last_sample = sample
            collated_series.append(sample)

    plot(collated_series, aliases)
    return 0

if __name__ == '__main__':
    sys.exit(main())  # next section explains the use of sys.exit
