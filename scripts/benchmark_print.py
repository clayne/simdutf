#!/usr/bin/env python3

import sys
import warnings
from pathlib import Path
from table import Table


class Input:
    def __init__(self):
        self.procedure = None
        self.input_size = None
        self.iterations = None
        self.dataset = None

    @property
    def dataset_name(self):
        if self.dataset:
            return self.dataset.stem
        else:
            return 'none'

    def __str__(self):
        return '<Input: %s, size: %d, iterations: %s, dataset: %s>' % \
                (self.procedure, self.input_size, self.iterations, self.dataset)

    __repr__ = __str__


class Result:
    def __init__(self):
        self.instruction_per_byte = None
        self.instruction_per_char = None
        self.instruction_per_cycle = None
        self.cycle_per_byte = None
        self.cycle_per_char = None
        self.bytes_per_char = None
        self.speed_gigabytes = None
        self.speed_gigachars = None
        self.freq = None
        self.time = None

    def update_from_dict(self, D):
        while D:
            unit, value = D.popitem()
            field = unit2field[unit]
            if field is not None:
                setattr(self, field, value)


unit2field = {
    'ins/byte'      : 'instruction_per_byte',
    'ins/char'      : 'instruction_per_char',
    'ins/cycle'     : 'instruction_per_cycle',
    'cycle/byte'    : 'cycle_per_byte',
    'cycle/char'    : 'cycle_per_char',
    'byte/char'     : 'byte_per_char',
    'GHz'           : 'freq',
    'GB/s'          : 'speed_gigabytes',
    'Gc/s'          : 'speed_gigachars',
    'ns'            : 'time',
    '%'             : None,
}


def parse(file):
    result = []
    for line in file:
        for item in parse_line(line):
            if isinstance(item, Input):
                result.append((item, Result()))
            else:
                assert isinstance(item, dict)
                result[-1][1].update_from_dict(item)

    return result


def parse_line(line):
    if 'input size' in line:
        yield parse_input(normalize_line(line))
    elif 'ins/byte' in line or 'ins/char' in line:
        yield parse_results(normalize_line(line))


def normalize_line(line):
    line = line.replace(',', ' ')
    line = line.replace(':', ' ')
    line = line.replace('(', ' ')
    line = line.replace(')', ' ')

    return line.split()


def parse_input(fields):
    input = Input()
    input.procedure = fields.pop(0)

    assert fields.pop(0) == 'input'
    assert fields.pop(0) == 'size'
    input.input_size = int(fields.pop(0))

    assert fields.pop(0) == 'iterations'
    input.iterations = int(fields.pop(0))

    try:
        assert fields.pop(0) == 'dataset'
        input.dataset = Path(fields.pop(0))
    except IndexError:
        pass

    return input


def try_float(s):
    try:
        return float(s)
    except ValueError:
        return


def parse_results(fields):
    """Consumes pairs "number unit" from the list"""
    D = {}

    while fields:
        value = try_float(fields.pop(0));
        if value is not None and fields:
            D[fields.pop(0)] = value

    return D


def print_speed_comparison(data):
    procedures = set()
    datasets = set()
    results = {}

    for input, result in data:
        procedures.add(input.procedure)
        datasets.add(input.dataset_name)
        results[(input.procedure, input.dataset_name)] = result

    datasets = list(sorted(datasets))
    procedures = list(sorted(procedures))

    def by_procedure():
        table = Table()
        table.set_header(['procedure'] + datasets)

        for procedure in procedures:
            row = []
            row.append(procedure)
            for dataset in datasets:
                try:
                    result = results[(procedure, dataset)]
                    row.append('%0.3f GB/s' % (result.speed_gbs))
                except KeyError:
                    row.append('--')

            table.add_row(row)

        return table

    def by_dataset():
        table = Table()
        table.set_header(['dataset'] + procedures)

        for dataset in datasets:
            row = []
            row.append(dataset)
            for procedure in procedures:
                try:
                    result = results[(procedure, dataset)]
                    row.append('%0.3f GB/s' % (result.speed_gigabytes))
                except KeyError:
                    row.append('--')

            table.add_row(row)

        return table

    if len(procedures) >= len(datasets):
        print(by_procedure())
    else:
        print(by_dataset())


def compare(old, new):
    collate = {}
    for input, result in old:
        key = str(input)
        collate[key] = [input, result, None]

    for input, result in new:
        key = str(input)
        if key not in collate:
            collate[key] = [input, None, result]
        else:
            collate[key][-1] = result

    grouped = {}
    for _, (input, old, new) in collate.items():
        if input.procedure not in grouped:
            grouped[input.procedure] = []

        grouped[input.procedure].append((input, old, new))

    NA = '--'
    table = Table()
    table.add_header(["dataset", "size [B]", "iterations", "old GB/s", "new GB/s", "speedup"])
    for procedure, values in grouped.items():
        table.add_row([(procedure, 6)])
        for input, old, new in values:
            old_speed = old.speed_gigabytes
            new_speed = new.speed_gigabytes
            if old_speed is None or new_speed is None:
                speedup = NA
            else:
                speedup = '%0.2fx' % (new_speed / old_speed)

            if old_speed is None:
                old_speed = NA
            else:
                old_speed = '%0.2f' % old_speed

            if new_speed is None:
                new_speed = NA
            else:
                new_speed = '%0.2f' % new_speed

            table.add_row([
                input.dataset.stem,
                '%s' % input.input_size,
                '%s' % input.iterations,
                old_speed,
                new_speed,
                speedup,
            ])

    print(table)


HELP = """Format output of the benchmark utility

When running a benchmark redirect its output to file (with > or `tee`), like:

$ cd build/benchmarks
$ ./benchamark [...] > results.txt
$ ./benchamark [...] | tee results.txt

To summarize results, use:

$ ./benchmark_print.py results.txt
+-----------------------+---------------------------------+
|        dataset        | convert_utf32_to_latin1+haswell |
+=======================+=================================+
| esperanto.utflatin32  | 88.806 GB/s                     |
+-----------------------+---------------------------------+
| french.utflatin32     | 92.388 GB/s                     |
+-----------------------+---------------------------------+
| german.utflatin32     | 79.836 GB/s                     |
+-----------------------+---------------------------------+
| portuguese.utflatin32 | 92.642 GB/s                     |
+-----------------------+---------------------------------+

To compare results, use:

$ ./benchmark_print.py old.txt new.txt
+-----------------------+----------+------------+----------+----------+---------+
|        dataset        | size [B] | iterations | old GB/s | new GB/s | speedup |
+=======================+==========+============+==========+==========+=========+
| convert_utf32_to_latin1+haswell                                               |
+-----------------------+----------+------------+----------+----------+---------+
| esperanto.utflatin32  |   328672 |      30000 |    77.14 |    88.81 | 1.15x   |
+-----------------------+----------+------------+----------+----------+---------+
| french.utflatin32     |  1729220 |      30000 |    75.26 |    92.39 | 1.23x   |
+-----------------------+----------+------------+----------+----------+---------+
| german.utflatin32     |   797324 |      30000 |    76.11 |    79.84 | 1.05x   |
+-----------------------+----------+------------+----------+----------+---------+
| portuguese.utflatin32 |  1086972 |      30000 |    84.12 |    92.64 | 1.10x   |
+-----------------------+----------+------------+----------+----------+---------+"""


def main():
    script = sys.argv[0]
    args = sys.argv[1:]
    n = len(args)
    if "-h" in args or "--help" in args or (n not in (1, 2)):
        print(HELP)
        return

    if n == 1:
        with open(args[0]) as f:
            data = parse(f)

        print_speed_comparison(data)
    elif n == 2:
        with open(args[0]) as f:
            old = parse(f)
        with open(args[1]) as f:
            new = parse(f)

        compare(old, new)


if __name__ == '__main__':
    main()
