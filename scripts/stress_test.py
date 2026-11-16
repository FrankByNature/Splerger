"""Stress tests for the Splerger routing model (scripts/routing_sim.py).

Mixed input belts with different ratios, random or patterned item orders,
rates and mixes that change over time, stalled outputs.

Run: python scripts/stress_test.py [fps] [seconds]

Columns:
  thru     items out / best possible (min of total in and total out, per time step)
  mix err  largest gap, in percentage points, between an output's share of an item
           type and that type's share of everything passed (whole run, after warm-up)
  win err  same, worst 30 s window (catches drifting / oscillating mixes)
  run      longest run of one item on an output (vs. "in run": longest on any input)
  inputs   share of items taken from each input
"""
import itertools
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import routing_sim as S  # noqa: E402

MK = {1: 60, 2: 120, 3: 270, 4: 480, 5: 780, 6: 1200}
WINDOW = 30.0


class Feed(S.Input):
    """Input belt: rate (per min) and item source may depend on time."""

    def __init__(self, rate, source, seed=0):
        super().__init__(rate if not callable(rate) else 0, "X")
        self.rate_fn = rate if callable(rate) else (lambda t, r=rate: r)
        self.source = source
        self.rng = random.Random(seed)
        self.t = 0.0
        self.taken = 0
        self.sent = []

    def next_item(self):
        item = self.source(self.rng, self.t)
        self.sent.append(item)
        return item

    def tick(self, dt):
        self.t += dt
        self.rate = self.rate_fn(self.t) / 60.0
        super().tick(dt)

    def grab(self):
        self.taken += 1
        return super().grab()


class Belt(S.Output):
    """Output belt: rate (per min) may depend on time; records arrival times."""

    def __init__(self, rate):
        connected = callable(rate) or rate > 0
        super().__init__(1 if connected else 0)
        self.connected = connected
        self.rate_fn = rate if callable(rate) else (lambda t, r=rate: r)
        self.t = 0.0
        self.times = []

    def belt_take(self, dt):
        self.t += dt
        self.rate = self.rate_fn(self.t) / 60.0
        before = len(self.got)
        super().belt_take(dt)
        self.times.extend([self.t] * (len(self.got) - before))


def mix(weights):
    """Random items with the given weights, e.g. mix({'Cu': 3, 'Fe': 1})."""
    items, w = zip(*weights.items())
    return lambda rng, t: rng.choices(items, w)[0]


def pattern(seq):
    it = itertools.cycle(seq)
    return lambda rng, t: next(it)


def switch_at(when, before, after):
    return lambda rng, t: before(rng, t) if t < when else after(rng, t)


def run(name, inputs, outputs, seconds, fps):
    sim = S.Splerger(inputs, outputs)
    dt = 1.0 / fps
    possible = 0.0
    warm = seconds / 6
    for step in range(int(seconds * fps)):
        t = step * dt
        for i in inputs:
            i.tick(dt)
        for o in outputs:
            o.belt_take(dt)
        sim.tick(dt)
        if t >= warm:
            cap_in = sum(i.rate for i in inputs)
            cap_out = sum(o.rate for o in outputs if o.connected)
            possible += min(cap_in, cap_out) * dt

    def after_warm(o):
        return [g for g, tt in zip(o.got, o.times) if tt >= warm], [tt for tt in o.times if tt >= warm]

    passed = []
    per_out = []
    for o in outputs:
        if o.connected:
            got, times = after_warm(o)
            per_out.append((o, got, times))
            passed += got
    types = sorted(set(passed))

    def share(items, typ):
        return items.count(typ) / len(items) if items else 0.0

    def mix_error(groups):
        allitems = [x for g in groups for x in g]
        worst = 0.0
        for g in groups:
            if len(g) < 20:  # too few to judge
                continue
            for typ in types:
                worst = max(worst, abs(share(g, typ) - share(allitems, typ)) * 100)
        return worst

    whole = mix_error([g for _, g, _ in per_out])
    win = 0.0
    t0 = warm
    while t0 + WINDOW <= seconds:
        groups = [[g for g, tt in zip(got, times) if t0 <= tt < t0 + WINDOW] for _, got, times in per_out]
        win = max(win, mix_error(groups))
        t0 += WINDOW

    thru = len(passed) / possible * 100 if possible else 0.0
    out_runs = max((max(len(list(g)) for _, g in itertools.groupby(got)) if got else 0) for _, got, _ in per_out)
    in_runs = max((max(len(list(g)) for _, g in itertools.groupby(i.sent)) if i.sent else 0) for i in inputs)
    taken = sum(i.taken for i in inputs) or 1
    inputs_share = "/".join(f"{i.taken / taken * 100:.0f}" for i in inputs)
    rates = ", ".join(f"{len(got) / (seconds - warm) * 60:.0f}" for _, got, _ in per_out)
    out_mix = " | ".join(
        " ".join(f"{t}{share(got, t) * 100:.0f}" for t in types) for _, got, _ in per_out)
    print(f"{name:<48} {thru:5.0f}% {whole:6.1f} {win:6.1f} {out_runs:4d} {in_runs:5d}  {inputs_share:<7} [{rates}]  {out_mix}")
    return dict(thru=thru, mix=whole, win=win, run=out_runs)


def scenarios():
    m = MK
    stall = lambda t: 0 if 200 <= t < 260 else m[5]  # noqa: E731  blocked for a minute
    onoff = lambda t: m[5] if int(t // 20) % 2 == 0 else 0  # noqa: E731  20 s on, 20 s off
    yield "A 75/25 + B 10/90 -> mk5 + mk5", [Feed(m[5], mix({'Cu': 75, 'Fe': 25}), 1), Feed(m[5], mix({'Cu': 10, 'Fe': 90}), 2)], [Belt(m[5]), Belt(m[5])]
    yield "A 75/25 + B 10/90 -> mk6 + mk3", [Feed(m[5], mix({'Cu': 75, 'Fe': 25}), 1), Feed(m[5], mix({'Cu': 10, 'Fe': 90}), 2)], [Belt(m[6]), Belt(m[3])]
    yield "A 75/25 + B 10/90 -> mk4 only", [Feed(m[5], mix({'Cu': 75, 'Fe': 25}), 1), Feed(m[5], mix({'Cu': 10, 'Fe': 90}), 2)], [Belt(m[4]), Belt(0)]
    yield "A 75/25 mk5 + B 10/90 mk2 -> mk4 + mk4", [Feed(m[5], mix({'Cu': 75, 'Fe': 25}), 1), Feed(m[2], mix({'Cu': 10, 'Fe': 90}), 2)], [Belt(m[4]), Belt(m[4])]
    yield "A 50/50 + B 50/50 (random) -> mk6 + mk6", [Feed(m[6], mix({'Cu': 1, 'Fe': 1}), 3), Feed(m[6], mix({'Cu': 1, 'Fe': 1}), 4)], [Belt(m[6]), Belt(m[6])]
    yield "A 3 types 50/30/20 + B coal mk2 -> mk4 + mk4", [Feed(m[5], mix({'Cu': 50, 'Fe': 30, 'Qz': 20}), 5), Feed(m[2], mix({'Co': 1}), 6)], [Belt(m[4]), Belt(m[4])]
    yield "A 3 types + B 2 types -> mk5 + mk2", [Feed(m[5], mix({'Cu': 50, 'Fe': 30, 'Qz': 20}), 7), Feed(m[5], mix({'Co': 60, 'Fe': 40}), 8)], [Belt(m[5]), Belt(m[2])]
    yield "5 types, 2 random mixes -> mk5 + mk3", [Feed(m[5], mix({'A': 40, 'B': 30, 'C': 20, 'D': 10}), 9), Feed(m[5], mix({'B': 10, 'D': 30, 'E': 60}), 10)], [Belt(m[5]), Belt(m[3])]
    yield "A 95/5 rare + B pure -> mk3 + mk3", [Feed(m[5], mix({'Cu': 95, 'Rare': 5}), 11), Feed(m[5], mix({'Fe': 1}), 12)], [Belt(m[3]), Belt(m[3])]
    yield "A pattern CCCF + B pattern FFQ -> mk5 + mk1", [Feed(m[5], pattern(['Cu', 'Cu', 'Cu', 'Fe']), 0), Feed(m[5], pattern(['Fe', 'Fe', 'Qz']), 0)], [Belt(m[5]), Belt(m[1])]
    yield "A pattern CF + B pattern CF -> mk4 + mk4", [Feed(m[5], pattern(['Cu', 'Fe']), 0), Feed(m[5], pattern(['Cu', 'Fe']), 0)], [Belt(m[4]), Belt(m[4])]
    yield "A pattern CCFF + B pattern CF -> mk3 + mk3", [Feed(m[5], pattern(['Cu', 'Cu', 'Fe', 'Fe']), 0), Feed(m[5], pattern(['Cu', 'Fe']), 0)], [Belt(m[3]), Belt(m[3])]
    yield "A Cu then (t>300) Fe + B 50/50 -> mk4 + mk4", [Feed(m[5], switch_at(300, mix({'Cu': 1}), mix({'Fe': 1})), 13), Feed(m[5], mix({'Cu': 1, 'Fe': 1}), 14)], [Belt(m[4]), Belt(m[4])]
    yield "A bursty 20s on/off 70/30 + B 20/80 -> mk4 + mk4", [Feed(onoff, mix({'Cu': 70, 'Fe': 30}), 15), Feed(m[5], mix({'Cu': 20, 'Fe': 80}), 16)], [Belt(m[4]), Belt(m[4])]
    yield "under-supplied mk2 + mk3 mixes -> mk6 + mk6", [Feed(m[2], mix({'Cu': 3, 'Fe': 1}), 17), Feed(m[3], mix({'Cu': 1, 'Fe': 3}), 18)], [Belt(m[6]), Belt(m[6])]
    yield "out2 blocked t 200-260s, mixes -> mk5 + mk5", [Feed(m[5], mix({'Cu': 75, 'Fe': 25}), 19), Feed(m[5], mix({'Cu': 10, 'Fe': 90}), 20)], [Belt(m[5]), Belt(stall)]
    yield "one mixed mk6 input 60/40 -> mk5 + mk4", [Feed(m[6], mix({'Cu': 60, 'Fe': 40}), 21)], [Belt(m[5]), Belt(m[4])]
    yield "one mixed mk5 input 3 types -> mk1 + mk1", [Feed(m[5], mix({'Cu': 50, 'Fe': 30, 'Qz': 20}), 22)], [Belt(m[1]), Belt(m[1])]

    # Many item types
    ten = {f"T{i}": 1 for i in range(10)}
    ten_skew = {f"T{i}": w for i, w in enumerate([30, 20, 15, 10, 8, 6, 5, 3, 2, 1])}
    six_a = {"Cu": 30, "Fe": 25, "Qz": 15, "Co": 15, "S": 10, "Cat": 5}
    six_b = {"Cu": 5, "Fe": 10, "Qz": 25, "Co": 20, "S": 30, "Cat": 10}
    abcd, efgh = mix({c: 1 for c in "ABCD"}), mix({c: 1 for c in "EFGH"})
    yield "4 types each side, disjoint -> mk5 + mk5", [Feed(m[5], abcd, 31), Feed(m[5], efgh, 32)], [Belt(m[5]), Belt(m[5])]
    yield "4 types each side, disjoint -> mk6 + mk2", [Feed(m[5], abcd, 33), Feed(m[5], efgh, 34)], [Belt(m[6]), Belt(m[2])]
    yield "6 types, two skewed mixes -> mk4 + mk4", [Feed(m[5], mix(six_a), 35), Feed(m[5], mix(six_b), 36)], [Belt(m[4]), Belt(m[4])]
    yield "6 types, two skewed mixes -> mk6 + mk1", [Feed(m[5], mix(six_a), 37), Feed(m[5], mix(six_b), 38)], [Belt(m[6]), Belt(m[1])]
    yield "10 types even, one mk5 input -> mk3 + mk3", [Feed(m[5], mix(ten), 39)], [Belt(m[3]), Belt(m[3])]
    yield "10 types even, one mk5 input -> mk1 + mk1", [Feed(m[5], mix(ten), 40)], [Belt(m[1]), Belt(m[1])]
    yield "10 types 30..1%, both inputs -> mk4 + mk2", [Feed(m[5], mix(ten_skew), 41), Feed(m[5], mix(ten_skew), 42)], [Belt(m[4]), Belt(m[2])]
    yield "10 types 30..1%, mk6 in -> mk6 + mk6", [Feed(m[6], mix(ten_skew), 43), Feed(m[6], mix(ten_skew), 44)], [Belt(m[6]), Belt(m[6])]
    yield "pattern ABCDE + pattern FGH -> mk4 + mk3", [Feed(m[5], pattern(list("ABCDE")), 0), Feed(m[5], pattern(list("FGH")), 0)], [Belt(m[4]), Belt(m[3])]
    yield "pattern ABCD one input -> mk2 + mk2", [Feed(m[4], pattern(list("ABCD")), 0)], [Belt(m[2]), Belt(m[2])]
    yield "pattern ABCDEF one input -> mk3 + mk2", [Feed(m[4], pattern(list("ABCDEF")), 0)], [Belt(m[3]), Belt(m[2])]
    yield "3 rare (1% each) in 3 common -> mk3 + mk3", [Feed(m[5], mix({"Cu": 40, "Fe": 30, "Qz": 27, "R1": 1, "R2": 1, "R3": 1}), 45), Feed(m[5], mix({"Cu": 1, "Fe": 1}), 46)], [Belt(m[3]), Belt(m[3])]
    yield "3 types -> 3 other types at 300s -> mk4 + mk4", [Feed(m[5], switch_at(300, mix({c: 1 for c in "ABC"}), mix({c: 1 for c in "DEF"})), 47), Feed(m[5], mix({"A": 1, "D": 1}), 48)], [Belt(m[4]), Belt(m[4])]
    yield "8 types, out2 blocked 200-260s -> mk5 + mk5", [Feed(m[5], abcd, 49), Feed(m[5], efgh, 50)], [Belt(m[5]), Belt(stall)]


def main():
    fps = int(sys.argv[1]) if len(sys.argv) > 1 else 30
    seconds = float(sys.argv[2]) if len(sys.argv) > 2 else 600
    print(f"{fps} fps, {seconds:.0f} s per scenario (first {seconds / 6:.0f} s ignored), tolerance {S.TOL}")
    print(f"{'scenario':<48} {'thru':>6} {'mix':>6} {'win':>6} {'run':>4} {'inrun':>5}  {'inputs':<7} [out/min]  output mixes")
    results = {name: run(name, ins, outs, seconds, fps) for name, ins, outs in scenarios()}
    worst = {k: max(r[k] for r in results.values()) for k in ('mix', 'win', 'run')}
    print(f"worst: throughput {min(r['thru'] for r in results.values()):.0f}%, mix err {worst['mix']:.1f} pt, "
          f"window err {worst['win']:.1f} pt, longest run {worst['run']}")


if __name__ == "__main__":
    main()
