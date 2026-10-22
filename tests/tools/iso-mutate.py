#!/usr/bin/env python3
# iso-mutate.py - measures how strong the rules of the oracle tests are: each output of a C++ port is altered line by
# line (a line removed, doubled, swapped with the next one, one digit or one letter changed), and every altered output
# is compared with its C# oracle by iso_check; an alteration that still passes is a blind spot of the rule, unless it
# is a legitimate variation (two lines of different agents swapped in a parallel example, a duration).
# The outputs come from a run of the tests with CAM_ISO_SAVE: CAM_ISO_SAVE=<outputs> ctest --preset debug-iso
# Usage: python3 -I iso-mutate.py <iso_check> <tests/iso> <outputs> [report] [--lines N] [--seed S]
import os, random, subprocess, sys, tempfile, collections

def oracle_of(iso_dir, output_name):
    """<name>.out compares with oracles/<name>.txt, <name>.seed<n>.out with oracles/seed<n>/<name>.txt."""
    base = output_name[:-4]
    if '.seed' in base:
        name, seed = base.split('.seed')
        return name, os.path.join(iso_dir, 'oracles', 'seed' + seed, name + '.txt')
    return base, os.path.join(iso_dir, 'oracles', base + '.txt')

def passes(iso_check, rules, oracle, text):
    with tempfile.NamedTemporaryFile('w', suffix='.out', delete=False, encoding='utf-8') as f:
        f.write(text)
        path = f.name
    try:
        return subprocess.run([iso_check, rules, oracle, '--actual', path], capture_output=True, text=True).returncode == 0
    finally:
        os.unlink(path)

def mutants(lines, rng, count):
    indexes = [i for i, line in enumerate(lines) if line.strip() and not line.startswith('#cam')]
    for i in (indexes if len(indexes) <= count else rng.sample(indexes, count)):
        yield 'removed', lines[:i] + lines[i + 1:], i
        yield 'doubled', lines[:i + 1] + [lines[i]] + lines[i + 1:], i
        if i + 1 < len(lines) and lines[i + 1] != lines[i] and lines[i + 1].strip():
            swapped = lines[:]
            swapped[i], swapped[i + 1] = swapped[i + 1], swapped[i]
            yield 'swapped', swapped, i
        digits = [k for k, c in enumerate(lines[i]) if c.isdigit()]
        if digits:
            k = rng.choice(digits)
            changed = lines[:]
            changed[i] = lines[i][:k] + str((int(lines[i][k]) + 1) % 10) + lines[i][k + 1:]
            yield 'digit', changed, i
        letters = [k for k, c in enumerate(lines[i]) if c.isalpha()]
        if letters:
            k = rng.choice(letters)
            changed = lines[:]
            changed[i] = lines[i][:k] + ('x' if lines[i][k] != 'x' else 'y') + lines[i][k + 1:]
            yield 'letter', changed, i

def main(argv):
    args, options = [], {}
    i = 0
    while i < len(argv):
        if argv[i].startswith('--') and i + 1 < len(argv):
            options[argv[i]] = argv[i + 1]
            i += 2
        else:
            args.append(argv[i])
            i += 1
    if len(args) < 3:
        print('usage: iso-mutate.py <iso_check> <tests/iso> <outputs> [report] [--lines N] [--seed S]')
        return 2
    iso_check, iso_dir, outputs = args[:3]
    report = args[3] if len(args) > 3 else None
    count = int(options.get('--lines', 25))
    rng = random.Random(int(options.get('--seed', 1)))
    kinds = ['removed', 'doubled', 'swapped', 'digit', 'letter']
    out = open(report, 'w', encoding='utf-8') if report else sys.stdout
    total, detected = collections.Counter(), collections.Counter()
    for output_name in sorted(n for n in os.listdir(outputs) if n.endswith('.out')):
        name, oracle = oracle_of(iso_dir, output_name)
        rules = os.path.join(iso_dir, 'rules', name + '.rules')
        if not (os.path.exists(rules) and os.path.exists(oracle)):
            continue
        lines = open(os.path.join(outputs, output_name), encoding='utf-8').read().split('\n')
        if not passes(iso_check, rules, oracle, '\n'.join(lines)):
            out.write(f'{output_name}: the output itself does not pass, skipped\n')
            continue
        per, per_detected, survivors = collections.Counter(), collections.Counter(), []
        for kind, mutant, i in mutants(lines, rng, count):
            per[kind] += 1
            if passes(iso_check, rules, oracle, '\n'.join(mutant)):
                survivors.append((kind, lines[i][:90]))
            else:
                per_detected[kind] += 1
        total.update(per)
        detected.update(per_detected)
        columns = ' '.join(f'{k} {per_detected[k]}/{per[k]}' for k in kinds if per[k])
        out.write(f'{output_name}: {sum(per_detected.values())}/{sum(per.values())} detected; {columns}\n')
        for kind, text in survivors[:6]:
            out.write(f'    survives ({kind}): {text}\n')
    out.write('TOTAL ' + ' '.join(f'{k} {detected[k]}/{total[k]}' for k in kinds if total[k]) + '\n')
    return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
