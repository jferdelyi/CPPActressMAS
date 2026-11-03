#!/usr/bin/env python3
# e20-vectors.py - turns the BfCapture output (csharp-ref.sh capture) into the pinned vectors of the "actressmas" codec:
#   <case>.bin  : bytes produced by the ORIGINAL C# code (.NET 8, assembly "ActressMas, Version=3.1.0.0");
#                 ContainerMessage cases = full frame (38-byte header + UTF-16LE Base64 body), the two header GUIDs
#                 zeroed (random in C#, never read); Message cases = BinaryFormatter (MS-NRBF) bytes alone.
#   <case>.json : the decoded fields, written from an independent NRBF reader (this script), so the C++ codec is
#                 checked against both the C# bytes and a second decoder.
# Usage: python3 -I e20-vectors.py <capture-dir> <output-dir>
import base64, json, os, sys

def read_lp(data, i):
    value, shift = 0, 0
    while True:
        byte = data[i]; i += 1
        value |= (byte & 0x7F) << shift; shift += 7
        if byte < 0x80:
            return value, i

def read_string(data, i):
    n, i = read_lp(data, i)
    return data[i:i + n].decode('utf-8'), i + n

def decode_nrbf(data):
    """Decodes the record subset of ContainerMessage and Message; returns (class, assembly, {member: value})."""
    assert data[0] == 0x00 and data[1:17] == bytes.fromhex('01000000ffffffff0100000000000000'), 'SerializationHeader'
    i = 17
    assert data[i] == 0x0C; i += 5
    assembly, i = read_string(data, i)
    assert data[i] == 0x05; i += 5
    cls, i = read_string(data, i)
    count = int.from_bytes(data[i:i + 4], 'little'); i += 4
    members = []
    for _ in range(count):
        name, i = read_string(data, i); members.append(name)
    types = list(data[i:i + count]); i += count
    assert all(t in (1, 2) for t in types), types         # String or Object: no additional type information
    i += 4                                                  # library id
    objects, values = {}, {}
    for name, kind in zip(members, types):
        record = data[i]
        if record == 0x06:
            oid = int.from_bytes(data[i + 1:i + 5], 'little')
            value, i = read_string(data, i + 5); objects[oid] = value
        elif record == 0x09:
            value = objects[int.from_bytes(data[i + 1:i + 5], 'little')]; i += 5
        elif record == 0x0A:
            value = None; i += 1
        elif record == 0x08 and data[i + 1] == 8:           # MemberPrimitiveTyped Int32 (ContentObj only)
            value = {'int32': int.from_bytes(data[i + 2:i + 6], 'little', signed=True)}; i += 6
        else:
            raise ValueError(f'record 0x{record:02x} at {i}')
        values[name.removeprefix('<').removesuffix('>k__BackingField')] = value
    assert data[i] == 0x0B and i + 1 == len(data), 'MessageEnd'
    return cls, assembly, values

def main(capture, output):
    os.makedirs(output, exist_ok=True)
    for file in sorted(os.listdir(capture)):
        if not file.endswith('.nrbf.hex'):
            continue
        case = file.removesuffix('.nrbf.hex')
        nrbf = bytes.fromhex(open(os.path.join(capture, file)).read().strip())
        cls, assembly, values = decode_nrbf(nrbf)
        description = {'class': cls, 'assembly': assembly, 'fields': values}
        frame_file = os.path.join(capture, case + '.frame.hex')
        if os.path.exists(frame_file):
            frame = bytearray(bytes.fromhex(open(frame_file).read().strip()))
            assert frame[0] == 0x01 and frame[1] == 0xFF and int.from_bytes(frame[2:6], 'big') == len(frame)
            assert base64.b64decode(frame[38:].decode('utf-16-le')) == nrbf
            frame[6:38] = bytes(32)
            payload, description['layer'] = bytes(frame), 'frame'
            content = values.get('Content')
            if values.get('Info') == 'Send Remote Message' and content:
                inner_cls, inner_assembly, inner = decode_nrbf(base64.b64decode(content))
                description['content_message'] = {'class': inner_cls, 'assembly': inner_assembly, 'fields': inner}
        else:
            payload, description['layer'] = nrbf, 'nrbf'
        open(os.path.join(output, case + '.bin'), 'wb').write(payload)
        open(os.path.join(output, case + '.json'), 'w').write(json.dumps(description, indent=2, ensure_ascii=False) + '\n')
        print(f'{case:28s} {description["layer"]:5s} {len(payload):5d} B')

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
