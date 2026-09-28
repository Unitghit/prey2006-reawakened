"""Replays engine movement traces (trace-halflife.ps1) through reference models of
Valve's movement code and reports where Prey's Half-Life modes differ.

Mode 3 follows Half-Life 1's pm_shared.c (PM_PlayerMove walk path): half-step
gravity, PM_Jump with PM_PreventMegaBunnyJumping, PM_Friction, PM_Accelerate,
PM_AirAccelerate (30 unit wish cap) and PM_CatagorizePosition.
https://github.com/ValveSoftware/halflife/blob/master/pm_shared/pm_shared.c

Mode 4 follows Source's CGameMovement::FullWalkMove / CheckJumpButton /
AirAccelerate / CategorizePosition (m_surfaceFriction 0.25 while slowly rising
and until landing), with the pre-Orange Box jump boost: the signed forward boost
without the later overspeed clip.
https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/shared/gamemovement.cpp

The world is the flat track floor; both models use the engine's frame time.

python compare-halflife.py PROFILE_BASE
"""
import math
import pathlib
import re
import sys

LINE = re.compile(r"MOVEMENT (\d+) mode=(\d+) dt=(\d+) ground=(\d+) origin=(\S+ \S+ \S+) velocity=(\S+ \S+ \S+) "
                  r"gravity=(\S+ \S+ \S+) jump=(\d) cmd=(-?\d+) (-?\d+) (-?\d+) (\d+) yaw=(-?\d+\.\d{3}) tag=(\w*)")
BUTTON_RUN = 2


def frames(path, tag):
    rows = []
    for m in LINE.finditer(path.read_text(errors='replace')):
        if m[14] != tag:
            continue
        rows.append(dict(t=int(m[1]), mode=int(m[2]), dt=int(m[3]) / 1000.0, ground=int(m[4]),
                         pos=list(map(float, m[5].split())), vel=list(map(float, m[6].split())),
                         jumped=int(m[8]), f=int(m[9]), s=int(m[10]), u=int(m[11]), buttons=int(m[12]),
                         yaw=float(m[13])))
    return rows


class Model:
    def __init__(self, mode, row, floor):
        self.mode, self.floor = mode, floor
        self.pos, self.vel = list(row['pos']), list(row['vel'])
        self.hl1 = mode == 3
        self.gravity = 800.0 if self.hl1 else 600.0
        self.onground = bool(row['ground'])
        self.old_jump = row['u'] >= 10
        self.surface = 1.0

    def speeds(self, row):
        run = row['buttons'] & BUTTON_RUN
        if self.hl1:
            maxspeed, key = 320.0, 400.0 * (0.3 if run else 1.0)	# cl_forwardspeed, cl_movespeedkey
        else:
            maxspeed, key = (320.0 if run else 190.0), 450.0
        f, s = row['f'] / 127.0 * key, row['s'] / 127.0 * key
        spd = math.hypot(f, s)
        if spd > maxspeed:	# PM_CheckParamters / CheckParameters
            f, s = f * maxspeed / spd, s * maxspeed / spd
        return f, s, maxspeed, run

    def wish(self, f, s, yaw, maxspeed):
        c, n = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        wx, wy = c * f + n * s, n * f - c * s	# right = (sin, -cos) for yaw about +z
        speed = math.hypot(wx, wy)
        if speed < 1e-9:
            return (0.0, 0.0), 0.0
        return (wx / speed, wy / speed), min(speed, maxspeed)

    def accelerate(self, d, wishspeed, accel, dt, air):
        wishspd = min(wishspeed, 30.0) if air else wishspeed
        add = wishspd - (self.vel[0] * d[0] + self.vel[1] * d[1])
        if add <= 0:
            return
        amount = min(accel * wishspeed * dt * self.surface, add)
        self.vel[0] += amount * d[0]
        self.vel[1] += amount * d[1]

    def friction(self, dt):
        speed = math.sqrt(sum(v * v for v in self.vel))
        if speed < 0.1:
            return
        control = max(speed, 100.0)
        new = max(0.0, speed - control * 4.0 * self.surface * dt)
        self.vel = [v * new / speed for v in self.vel]

    def categorize(self):
        rapid = self.vel[2] > (180.0 if self.hl1 else 140.0)
        if not rapid and self.pos[2] - self.floor <= 2.0:
            self.onground = True
            self.pos[2] = self.floor
            self.surface = 1.0
        else:
            self.onground = False
            if not self.hl1 and not rapid and self.vel[2] > 0:
                self.surface = 0.25

    def step(self, row):
        dt = row['dt']
        f, s, maxspeed, run = self.speeds(row)
        self.vel[2] -= self.gravity * 0.5 * dt	# PM_AddCorrectGravity / StartGravity
        if row['u'] >= 10:
            if not self.onground:
                self.old_jump = True
            elif not self.old_jump or ALLOW_HOLD:
                self.jump(f, maxspeed, run, row['yaw'], dt)
        else:
            self.old_jump = False
        if self.onground:
            self.vel[2] = 0.0
            self.friction(dt)
        d, wishspeed = self.wish(f, s, row['yaw'], maxspeed)
        if self.onground:
            self.accelerate(d, wishspeed, 10.0, dt, False)
            self.vel[2] = 0.0
        else:
            self.accelerate(d, wishspeed, 10.0, dt, True)
        for i in range(3):
            self.pos[i] += self.vel[i] * dt
        if self.pos[2] < self.floor:
            self.pos[2] = self.floor
            self.vel[2] = max(self.vel[2], 0.0)
        self.categorize()
        self.vel[2] -= self.gravity * 0.5 * dt	# PM_FixupGravityVelocity / FinishGravity
        if self.onground:
            self.vel[2] = 0.0

    def jump(self, fmove, maxspeed, run, yaw, dt):
        self.onground = False
        self.old_jump = True
        if self.hl1:
            limit = 1.7 * maxspeed	# PM_PreventMegaBunnyJumping
            spd = math.sqrt(sum(v * v for v in self.vel))
            if spd > limit:
                self.vel = [v * limit / spd * 0.65 for v in self.vel]
            self.vel[2] = math.sqrt(2 * 800 * 45.0)
        else:
            self.vel[2] += 160.0
            boost = fmove * (0.1 if run else 0.5)	# signed; old engine has no overspeed clip
            self.vel[0] += math.cos(math.radians(yaw)) * boost
            self.vel[1] += math.sin(math.radians(yaw)) * boost
        self.vel[2] -= self.gravity * 0.5 * dt	# PM_FixupGravityVelocity / FinishGravity in the jump


ALLOW_HOLD = False


def lateral(v):
    return math.hypot(v[0], v[1])


def run_model(mode, rows):
    floor = min(r['pos'][2] for r in rows if r['ground']) if any(r['ground'] for r in rows) else 0.25
    model = Model(mode, rows[0], floor)
    out = []
    for row in rows[1:]:
        model.step(row)
        out.append((list(model.pos), list(model.vel), model.onground))
    return out


def summary(rows, states=None):
    """Takeoff lateral speeds, apex height gain, airborne frames."""
    seq = [(r['pos'], r['vel'], bool(r['ground'])) for r in rows[1:]] if states is None else states
    takeoffs, apex, air, start = [], 0.0, 0, None
    prev_ground = bool(rows[0]['ground'])
    base = rows[0]['pos'][2]
    for pos, vel, ground in seq:
        if prev_ground and not ground:
            takeoffs.append(round(lateral(vel), 1))
        if not ground:
            air += 1
        apex = max(apex, pos[2] - base)
        prev_ground = ground
    return dict(takeoffs=takeoffs, apex=round(apex, 2), air=air,
                final=round(lateral(seq[-1][1]), 1), finalpos=[round(p, 1) for p in seq[-1][0]])


def main():
    global ALLOW_HOLD
    root = pathlib.Path(sys.argv[1])
    for mode in (3, 4):
        name = 'Half-Life 1' if mode == 3 else 'Half-Life 2 (old engine)'
        print(f'==== {name}')
        for scenario in ('JUMP', 'RUN', 'HOP', 'BACK', 'STRAFE', 'FAST', 'JSTRAFE'):
            rows = frames(root / f'trace{mode}.txt', scenario)
            start = next((i for i, r in enumerate(rows) if r['f'] or r['s'] or r['u']), 0)
            rows = rows[max(0, start - 1):]
            ALLOW_HOLD = scenario in ('HOP', 'BACK')	# automatic jumping enabled in these runs
            states = run_model(mode, rows)
            engine, ref = summary(rows), summary(rows, states)
            # one-step: from each engine state, predict the next
            worst = (0.0, None)
            floor = min((r['pos'][2] for r in rows if r['ground']), default=0.25)
            for k in range(1, len(rows)):
                m = Model(mode, rows[k - 1], floor)
                m.old_jump = rows[k - 1]['u'] >= 10 and not ALLOW_HOLD
                m.step(rows[k])
                err = math.dist(m.vel, rows[k]['vel'])
                if err > worst[0]:
                    worst = (err, k)
            if scenario == 'FAST':
                # Jump from 700 units/s: Half-Life 1 caps above 1.7 x 320 (PM_PreventMegaBunnyJumping).
                takeoff = lateral(rows[0]['vel'])
                expected = 1.7 * 320 * 0.65 if mode == 3 else 700.0
                print(f'{"":7} takeoff speed {takeoff:.1f}, reference {expected:.1f} -> {"OK" if abs(takeoff - expected) < 1.0 else "DIFFERENT"}')
            print(f'{scenario:7} engine {engine}')
            print(f'{"":7} ref    {ref}')
            if worst[1] is not None:
                k = worst[1]
                print(f'{"":7} largest one-frame velocity difference {worst[0]:.2f} at frame {k} '
                      f'(ground {rows[k-1]["ground"]}->{rows[k]["ground"]}, cmd {rows[k]["f"]} {rows[k]["s"]} {rows[k]["u"]})')


if __name__ == '__main__':
    main()
