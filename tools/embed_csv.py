#!/usr/bin/env python3

import sys
from pathlib import Path


if len(sys.argv) != 3:
    print("Usage: embed_csv.py <input.csv> <output.cpp>")
    sys.exit(1)

input_csv = Path(sys.argv[1])
output_cpp = Path(sys.argv[2])

if not input_csv.exists():
    print(f"ERROR: Input CSV not found: {input_csv}")
    sys.exit(1)

data = input_csv.read_bytes()

output_cpp.parent.mkdir(parents=True, exist_ok=True)

with output_cpp.open("w", encoding="utf-8") as f:
    f.write('#include "EmbeddedFundamentalData.h"\n\n')
    f.write('#include <cstddef>\n\n')
    f.write('namespace EmbeddedFundamentalData {\n\n')

    f.write('const unsigned char data[] = {\n')

    for i in range(0, len(data), 16):
        chunk = data[i:i + 16]
        f.write("    ")
        f.write(", ".join(f"0x{b:02X}" for b in chunk))
        f.write(",\n")

    f.write('};\n\n')
    f.write('const std::size_t size = sizeof(data);\n\n')
    f.write('}\n')

print(f"Embedded: {input_csv}")
print(f"Output   : {output_cpp}")
print(f"Bytes    : {len(data)}")
