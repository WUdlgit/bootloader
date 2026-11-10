"""Analyze host timestamp logs from run_weather_upgrade.py; no board access."""
import argparse
import ast
import json
from pathlib import Path
import statistics


def analyze(path):
    events = []
    pending = bytearray()
    for line in path.read_text(encoding='utf-8').splitlines():
        parts = line.split(' ', 2)
        if len(parts) != 3 or parts[1] not in ('TX', 'RX'):
            continue
        stamp, direction, payload = float(parts[0]), parts[1], parts[2]
        if direction == 'TX':
            count, representation = payload.split(' ', 1)
            data = ast.literal_eval(representation) if representation.startswith("b'") or representation.startswith('b"') else None
            events.append(dict(t=stamp, kind='tx', count=int(count), data=data))
        else:
            pending.extend(ast.literal_eval(payload))
            while b'\n' in pending:
                message, _, remainder = pending.partition(b'\n')
                pending = bytearray(remainder)
                events.append(dict(t=stamp, kind='rx', text=message.decode('ascii', errors='replace').strip()))
    command = next(e for e in events if e.get('data') == b'u')
    header_tx = next(e for e in events if (e.get('data') or b'').startswith(b'SIZE='))
    ready = next(e for e in events if e.get('text', '').startswith('[BOOT] DATA '))
    size = int(ready['text'].split()[2])
    done = next(e for e in events if e.get('text', '').startswith('[BOOT] DONE '))
    blocks = []
    outstanding = None
    for e in events:
        if not ready['t'] <= e['t'] <= done['t']:
            continue
        if e['kind'] == 'tx' and e.get('data') not in (b'g', b'?', b'u'):
            outstanding = e
        if e.get('text', '').startswith('[BOOT] ACK '):
            if outstanding is None:
                raise ValueError('ACK without a recorded block send')
            blocks.append(dict(length=outstanding['count'], send=outstanding['t'], ack=e['t'], duration=e['t']-outstanding['t']))
            outstanding = None
    if sum(b['length'] for b in blocks) != size:
        raise ValueError('Recorded block lengths do not match image size')
    final_ack = blocks[-1]['ack']
    baud = 115200
    phases = dict(command_to_header_send=header_tx['t']-command['t'],
                  header_send_to_data=ready['t']-header_tx['t'],
                  data_to_last_ack=final_ack-ready['t'],
                  last_ack_to_done=done['t']-final_ack)
    payload_wire = size*10/baud
    ack_wire = sum(len(f"[BOOT] ACK {sum(b['length'] for b in blocks[:i+1])}\r\n".encode())*10/baud for i in range(len(blocks)))
    full = [b['duration'] for b in blocks if b['length'] == 1024]
    return dict(source=str(path), image_bytes=size, blocks=len(blocks), baud=baud,
                total_seconds=done['t']-command['t'], phases_seconds=phases,
                payload_wire_min_seconds=payload_wire, ack_wire_min_seconds=ack_wire,
                block_phase_minus_payload_and_ack_wire_seconds=phases['data_to_last_ack']-payload_wire-ack_wire,
                full_block_duration_median_seconds=statistics.median(full),
                full_block_duration_min_seconds=min(full), full_block_duration_max_seconds=max(full),
                scope='Host-observed boundaries; erase includes header parsing and transport. Block residual combines Flash write/readback, scheduling, USB buffering, host logging and turnaround; not isolated MCU execution time.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    result = analyze(args.log)
    output = json.dumps(result, indent=2)
    if args.output:
        args.output.write_text(output+'\n', encoding='utf-8')
    print(output)
