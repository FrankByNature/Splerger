"""Offline model of the Splerger's item routing (mirrors BuildableSplerger.cpp).

Run: python scripts/routing_sim.py [fps]. Keep the constants and the logic in
step with Factory_Tick / PickOutputSlot when changing either.

Belts are modelled as rates: an input belt offers its next item once enough
time has passed; an output belt takes one item from its slot at its rate.
"""
import itertools, math, sys
from collections import defaultdict

CAP = 3  # GSlotCapacity
TOL = 1.0  # GMixTolerance
STALL = 0.5  # GBackedUpAfterSeconds
DRAIN_SMOOTHING = 1.0  # GDrainSmoothingSeconds
MAX_MOVES = CAP * 2


class Input:
    def __init__(self, rate, pattern):
        self.rate = rate / 60.0  # items/s
        self.budget = 0.0
        self.pattern = itertools.cycle(pattern)
        self.head = None

    def next_item(self):
        return next(self.pattern)

    def refill(self):
        # Items queued at the belt's end: the next one is offered as soon as
        # the previous one is taken, so several can go in one tick.
        if self.head is None and self.budget >= 1:
            self.head = self.next_item()

    def tick(self, dt):
        self.budget = min(self.budget + self.rate * dt, max(4.0, self.rate * dt + 1))
        self.refill()

    def grab(self):
        item = self.head
        self.head = None
        self.budget -= 1
        self.refill()
        return item


class Output:
    def __init__(self, rate):
        self.connected = rate > 0
        self.rate = rate / 60.0
        self.budget = 0.0
        self.slot = []  # list of same item
        self.got = []
        self.since_drain = 0.0
        self.count_after = 0
        self.drain_rate = 0.0  # items/s the belt has been taking (smoothed)

    def belt_take(self, dt):
        # A belt takes as many items per tick as it has room for (up to what
        # the slot holds), not just one: that's why slots hold up to CAP.
        self.budget = min(self.budget + self.rate * dt, self.rate * dt + 1.0)
        while self.slot and self.budget >= 1:
            self.budget -= 1
            self.got.append(self.slot.pop(0))


class Splerger:
    def __init__(self, inputs, outputs):
        self.inputs, self.outputs = inputs, outputs
        self.next_input = 0
        self.next_output = 0
        self.dt = 1.0 / 30
        self.counts = [defaultdict(float) for _ in outputs]
        self.totals = [0.0 for _ in outputs]

    # --- helpers, same names as the C++ ---
    def backed_up(self, i):
        o = self.outputs[i]
        if not o.connected:
            return True
        if not o.slot:
            return False
        # Full is not backed up: a fast belt's 1-item slot is "full" most of
        # the time. Backed up = not taking items any more.
        return o.since_drain > STALL

    def capacity(self, i):
        """Items slot i may hold: what its belt takes in about two ticks (SlotCapacity)."""
        per_tick = self.outputs[i].drain_rate * self.dt
        return max(1, min(CAP, math.ceil(per_tick * 2)))

    def accepts(self, i, item):
        s = self.outputs[i].slot
        return not s or (s[0] == item and len(s) < self.capacity(i))

    def deficit(self, i, item):
        total_all = sum(self.totals)
        if total_all <= 0:
            return 0.0
        c_all = sum(c[item] for c in self.counts)
        return c_all * self.totals[i] / total_all - self.counts[i][item]

    def other_active(self, i):
        return any(j != i and not self.backed_up(j) for j in range(len(self.outputs)))

    def pick_output(self, item, only_empty):
        n = len(self.outputs)
        best, best_empty, best_d = None, False, 0.0
        for step in range(n):
            i = (self.next_output + step) % n
            o = self.outputs[i]
            if not o.connected:
                continue
            empty = not o.slot
            if only_empty and not empty:
                continue
            if not self.accepts(i, item):
                continue
            d = self.deficit(i, item)
            if d < -TOL and self.other_active(i):
                continue
            if best is None or (empty and not best_empty) or (empty == best_empty and d > best_d + 1e-3):
                best, best_empty, best_d = i, empty, d
        return best

    def record(self, i, item):
        self.counts[i][item] += 1
        self.totals[i] += 1
        if sum(self.totals) > 100:
            for c in self.counts:
                for k in c:
                    c[k] *= 0.5
            self.totals = [t * 0.5 for t in self.totals]

    def tick(self, dt):
        self.dt = dt
        for o in self.outputs:
            n = len(o.slot)
            drained = max(0, o.count_after - n)
            o.drain_rate += (drained / dt - o.drain_rate) * min(1.0, dt / DRAIN_SMOOTHING)
            if n == 0 or n < o.count_after:
                o.since_drain = 0.0
            else:
                o.since_drain += dt
        ni = len(self.inputs)
        for _ in range(MAX_MOVES):
            cands = []
            for step in range(ni):
                idx = (self.next_input + step) % ni
                if self.inputs[idx].head is not None:
                    cands.append(idx)
            moved = False
            for k, idx in enumerate(cands):
                item = self.inputs[idx].head
                slot = self.pick_output(item, only_empty=k > 0)
                if slot is None:
                    continue
                self.outputs[slot].slot.append(self.inputs[idx].grab())
                self.record(slot, item)
                self.next_input = (idx + 1) % ni
                self.next_output = (slot + 1) % len(self.outputs)
                moved = True
                break
            if not moved:
                break
        for o in self.outputs:
            o.count_after = len(o.slot)


def run(name, inputs, outputs, seconds=600, fps=30):
    s = Splerger(inputs, outputs)
    dt = 1.0 / fps
    for _ in range(int(seconds * fps)):
        for i in inputs:
            i.tick(dt)
        for o in outputs:
            o.belt_take(dt)
        s.tick(dt)
    print(f"== {name} ({fps} fps)")
    for k, o in enumerate(outputs):
        if not o.connected:
            print(f"  out{k}: not connected")
            continue
        got = o.got[len(o.got) // 3:]  # skip warm-up
        span = seconds * 2 / 3
        rate = len(got) / span * 60
        mix = {t: round(got.count(t) / max(len(got), 1) * 100) for t in sorted(set(got))}
        runs = [len(list(g)) for _, g in itertools.groupby(got)]
        print(f"  out{k}: {rate:6.0f}/min of {o.rate*60:.0f}  mix {mix}  longest run {max(runs) if runs else 0}")


if __name__ == "__main__":
    fps = int(sys.argv[1]) if len(sys.argv) > 1 else 30
    MK = {1: 60, 2: 120, 3: 270, 4: 480, 5: 780, 6: 1200}
    run("2 in mk5 (Cu, Fe) -> 1 out mk5", [Input(MK[5], "C"), Input(MK[5], "I")], [Output(MK[5]), Output(0)], fps=fps)
    run("2 in mk5 (Cu, Fe) -> 1 out mk4", [Input(MK[5], "C"), Input(MK[5], "I")], [Output(MK[4]), Output(0)], fps=fps)
    run("2 in mk5 (Cu, Fe) -> mk6 + mk4", [Input(MK[5], "C"), Input(MK[5], "I")], [Output(MK[6]), Output(MK[4])], fps=fps)
    run("2 in mk5 (Cu, Fe) -> mk6 + mk2", [Input(MK[5], "C"), Input(MK[5], "I")], [Output(MK[6]), Output(MK[2])], fps=fps)
    run("1 in mk2 mixed CICI -> mk1 + mk1", [Input(MK[2], "CI")], [Output(MK[1]), Output(MK[1])], fps=fps)
    run("1 in mk5 mixed CICI -> mk4 + mk4", [Input(MK[5], "CI")], [Output(MK[4]), Output(MK[4])], fps=fps)
    run("1 in mk5 mixed CCII -> mk4 + mk4", [Input(MK[5], "CCII")], [Output(MK[4]), Output(MK[4])], fps=fps)
    run("2 in mk5 same item -> mk5 + mk5", [Input(MK[5], "C"), Input(MK[5], "C")], [Output(MK[5]), Output(MK[5])], fps=fps)
    run("2 in mk6 same item -> mk6 + mk6", [Input(MK[6], "C"), Input(MK[6], "C")], [Output(MK[6]), Output(MK[6])], fps=fps)
    run("2 in mk6 (Cu, Fe) -> mk6 + mk6", [Input(MK[6], "C"), Input(MK[6], "I")], [Output(MK[6]), Output(MK[6])], fps=fps)
    run("2 in mk3 (Cu, Fe) -> mk6 + mk6", [Input(MK[3], "C"), Input(MK[3], "I")], [Output(MK[6]), Output(MK[6])], fps=fps)
