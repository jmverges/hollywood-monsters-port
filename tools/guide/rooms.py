#!/usr/bin/env python3
"""Mark each step of guide/hm_actions.txt with the room it is played in, from the logs of replays.

usage: rooms.py <hm_actions.txt> <replay.log>...

A replay of the walkthrough (tools/test, HM_GUIDE_EXIT=1) logs every step with the room the game was in when
it started ("guide: <t> room 0x1b62 step 1 [section] go o1", or "room 0x1b9e step 45 already done: ...").
The steps of the list and of the logs are lined up by their text, in order (the list may have changed a little
since the replay), and each step gets "@<room> " in front, e.g. "@1b62 go o1". A step the logs do not have takes
the room of the next step that has one. The port's menu shows only the steps of the room the player is in.
"""
import difflib
import re
import sys

STEP_LOG = re.compile(r'guide: (?:\d+ )?room 0x([0-9a-f]+) step \d+ (?:\[[^\]]*\] |already done: )(.*)$')
ROOM_TAG = re.compile(r'^@[0-9a-f]+\s+')


def step_text(line):
    """the action of a step line, without its room tag and comment"""
    return ROOM_TAG.sub('', line.split('#', 1)[0]).strip()


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    path, logs = sys.argv[1], sys.argv[2:]
    lines = open(path, encoding='utf-8').read().split('\n')
    steps = [i for i, l in enumerate(lines) if l.strip() and not l.startswith('#') and not l.startswith('==')]
    texts = [step_text(lines[i]) for i in steps]
    rooms = [None] * len(steps)
    for log in logs:
        played = []
        for l in open(log, encoding='utf-8', errors='replace'):
            m = STEP_LOG.search(l.rstrip('\n'))
            if m:
                played.append((m.group(2).strip(), int(m.group(1), 16)))
        sm = difflib.SequenceMatcher(None, texts, [t for t, _ in played], autojunk=False)
        for a, b, n in sm.get_matching_blocks():
            for k in range(n):
                if rooms[a + k] is None:
                    rooms[a + k] = played[b + k][1]
    known = sum(r is not None for r in rooms)
    nxt = None
    for k in range(len(steps) - 1, -1, -1):           # the rest: the room of the next step that has one
        if rooms[k] is None:
            rooms[k] = nxt
        nxt = rooms[k]
    for k, i in enumerate(steps):
        rest = ROOM_TAG.sub('', lines[i])
        lines[i] = ('@%x ' % rooms[k] if rooms[k] is not None else '') + rest
    open(path, 'w', encoding='utf-8').write('\n'.join(lines))
    print('%s: %d steps, %d rooms from the logs, %d from the next step' % (path, len(steps), known, len(steps) - known))


if __name__ == '__main__':
    main()
