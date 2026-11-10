# online-abuse-protection 4.1: an attacker off the path of a match (another UDP socket), sending to a game's port
# random and malformed packets of every kind its session accepts, with a fixed seed.
# usage: attacker.py PORT SECONDS SEED [--first-hello]   (prints how many packets it sent)
#   --first-hello: a hello of its own sent at once, before the honest joiner's (the first-hello race)
import random
import socket
import sys
import time

port, seconds, seed = int(sys.argv[1]), float(sys.argv[2]), int(sys.argv[3])
first = '--first-hello' in sys.argv
# --forged: only forged host statuses (Q: phase 4, "stopped hosting", other names and streaks) and refusals (X)
forged = '--forged' in sys.argv
rng = random.Random(seed)
tag = b'NGO'
kinds = b'HWXQGCAUVNRKSTPL'
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.setblocking(False)
to = ('127.0.0.1', port)
sent = 0


def send(data):
    global sent
    try:
        s.sendto(data, to)
        sent += 1
    except (BlockingIOError, ConnectionResetError, OSError):
        pass


if first:
    # (a hello that is not the same game's: garbage after the tag)
    for _ in range(20):
        send(tag + b'H' + bytes(rng.randrange(256) for _ in range(40)))
        time.sleep(0.05)
end = time.monotonic() + seconds
while time.monotonic() < end:
    if forged:
        q = bytes(16) + bytes([4, 6, 99, 99]) + b'FORGED\x00FAKE\x00'
        send(tag + b'Q' + q)
        send(tag + b'X' + b'You were thrown out')
        if sent % 200 == 0:
            time.sleep(0.01)
        continue
    shape = rng.randrange(6)
    kind = bytes([kinds[rng.randrange(len(kinds))]])
    if shape == 0:
        data = bytes(rng.randrange(256) for _ in range(rng.randrange(1, 64)))  # random bytes
    elif shape == 1:
        data = tag + kind  # the header alone
    elif shape == 2:
        data = tag + kind + bytes(rng.randrange(256) for _ in range(rng.randrange(1, 40)))  # short
    elif shape == 3:
        data = tag + kind + bytes(rng.randrange(256) for _ in range(rng.randrange(1000, 1400)))  # large
    elif shape == 4:
        data = tag + kind + bytes(16) + bytes(rng.randrange(256) for _ in range(rng.randrange(0, 64)))  # zero room/epoch
    else:
        data = tag + b'H' + bytes(rng.randrange(256) for _ in range(rng.randrange(0, 200)))  # hellos
    send(data)
    if sent % 200 == 0:
        time.sleep(0.01)
print('attacker sent', sent)
