#!/usr/bin/env python3
# Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

import os
import re
import sys
from pathlib import Path
import argparse
from collections import defaultdict

# Extensions to include
SOURCE_EXTENSIONS = {".c", ".cpp", ".h", ".hpp"}

# Regex to find ANJ_LOG_SOURCE_FILE_ID with arbitrary whitespace and optional parentheses
DEFINE_REGEX = re.compile(r"\s*#\s*define\s+ANJ_LOG_SOURCE_FILE_ID\s+\(?\s*(\d+)\s*\)?")
# Regex to find log patterns in input
LOG_PATTERN_REGEX = re.compile(r"<ANJ_uLOG>(\d+);(\d+)</ANJ_uLOG>")

def is_parent(p1: Path, p2: Path) -> bool:
    try:
        p2.relative_to(p1)
        return True
    except ValueError:
        return False

def collect_unique_root_dirs():
    cwd = Path.cwd().resolve()
    script_dir = Path(__file__).resolve().parent

    # Pick only the outermost directory
    if is_parent(cwd, script_dir):
        return [cwd]
    elif is_parent(script_dir, cwd):
        return [script_dir]
    else:
        return [cwd, script_dir]

def find_file_ids(root_dirs, verbose):
    id_to_path = {}
    for root in root_dirs:
        for path in root.rglob("*"):
            if path.suffix.lower() in SOURCE_EXTENSIONS and path.is_file():
                try:
                    with path.open("r", encoding="utf-8", errors="ignore") as f:
                        for line in f:
                            match = DEFINE_REGEX.match(line)
                            if match:
                                file_id = int(match.group(1))
                                if file_id in id_to_path:
                                    raise ValueError(f"Collision: ID {file_id} defined in both:\n"
                                                     f" - {id_to_path[file_id]}\n"
                                                     f" - {path}")

                                if verbose:
                                    id_to_path[file_id] = str(path.resolve())
                                else:
                                    # strip root path from output
                                    id_to_path[file_id] = str(path.relative_to(root))

                                break  # only first match per file
                except Exception as e:
                    print(f"Error reading {path}: {e}", file=sys.stderr)
    return id_to_path

def process_input(file_id_map, input_file):
    for line in input_file:
        def replace(match):
            file_id = int(match.group(1))
            line_number = int(match.group(2))

            file_path = file_id_map.get(file_id, f"<unknown file ID {file_id}>")
            return f'[{file_path}:{line_number}]:'

        line = LOG_PATTERN_REGEX.sub(replace, line)
        print(line, end='')

def main():
    parser = argparse.ArgumentParser(
        prog='Anjay lite micro logs decoder',
        description='Decodes micro log entries from file or stdin by replacing file ID and '
                    'line number patterns with actual file paths and line numbers.')

    parser.add_argument('-i', '--input', help='Input filename or - to read from stdin.', default='-')
    parser.add_argument('-r', '--root', help='Project root directory.')
    parser.add_argument('-v', '--verbose', help='Print paths with the inclusion of the root path', action='store_true')

    args = parser.parse_args()

    try:
        if args.root:
            cwd_path = Path.cwd().resolve()
            root_path = Path(args.root).resolve()
            root_dirs = [cwd_path.joinpath(root_path)] if cwd_path.is_relative_to(root_path) else [ root_path ]
        else:
            root_dirs = collect_unique_root_dirs()

        file_id_map = find_file_ids(root_dirs, args.verbose)

        if args.input == '-':
            process_input(file_id_map, sys.stdin)
        else:
            with open(args.input, 'r', encoding='utf-8') as f:
                process_input(file_id_map, f)

    except Exception as e:
        print(f"Fatal error: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
