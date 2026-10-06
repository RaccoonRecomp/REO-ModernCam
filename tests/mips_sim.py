"""Minimal MIPS32 (big-endian, o32, FR=0) interpreter for running REO_ModernCam's mod.elf against a fake EE RAM, using
N64Recomp's memory model: every 32-bit word is stored host-native (little-endian here), byte accesses use addr ^ 3 and
halfword accesses addr ^ 2.

v2: every function of the mod's .recomp_import.* section is an import; the test registers a Python handler per name
(Machine.handlers[name] = fn(machine, args) -> int, or ('double', value)). Unregistered imports fault. EE accesses can
be logged (log_writes / log_reads) to check that the mod never touches game code. Also counts executed instructions
(Machine.steps, per call)."""
import math
import os
import struct
import subprocess

OBJDUMP = os.environ.get("LLVM_OBJDUMP", "llvm-objdump")  # on PATH unless LLVM_OBJDUMP names it
MOD_BASE = 0x81000000
MOD_SIZE = 0x00100000
EE_SIZE = 0x02000000
RA_SENTINEL = 0x80FFFFF0


def f32(x):
    try:
        return struct.unpack("<f", struct.pack("<f", x))[0]
    except OverflowError:
        return math.copysign(math.inf, x)


def f2u(x):
    try:
        return struct.unpack("<I", struct.pack("<f", x))[0]
    except OverflowError:
        return 0x7F800000 if x > 0 else 0xFF800000


def u2f(u):
    return struct.unpack("<f", struct.pack("<I", u & 0xFFFFFFFF))[0]


def s32(x):
    x &= 0xFFFFFFFF
    return x - 0x100000000 if x & 0x80000000 else x


def s16(x):
    x &= 0xFFFF
    return x - 0x10000 if x & 0x8000 else x


def _trunc(a):
    if math.isnan(a) or math.isinf(a) or abs(a) >= 2 ** 31:
        return 0x7FFFFFFF
    return int(a) & 0xFFFFFFFF


def _fcmp(cond, a, b):
    un = math.isnan(a) or math.isnan(b)
    return bool((cond & 1 and un) or (cond & 2 and not un and a == b) or (cond & 4 and not un and a < b))


class Fault(Exception):
    pass


class Machine:
    def __init__(self, elf_path):
        self.ee = bytearray(EE_SIZE)
        self.mod = bytearray(MOD_SIZE)
        self.imports = {}
        self.symbols = {}
        self.handlers = {}
        self.config = {}
        self.ee_writes = []  # (addr, width, value) of mod writes into EE RAM
        self.ee_reads = []   # (addr, width) of mod reads of EE RAM
        self.log_writes = False
        self.log_reads = False
        self.steps = 0
        self.import_calls = []  # (name, args) of every import call when log_imports is set
        self.log_imports = False
        self.load_elf(elf_path)
        self.handlers["recomp_get_config_u32"] = self._cfg_u32
        self.handlers["recomp_get_config_double"] = self._cfg_double

    # ---- ELF -----------------------------------------------------------------------------------
    def load_elf(self, path):
        d = open(path, "rb").read()
        assert d[:4] == b"\x7fELF" and d[5] == 2  # big-endian
        shoff, = struct.unpack(">I", d[0x20:0x24])
        shentsize, shnum, shstrndx = struct.unpack(">HHH", d[0x2E:0x34])
        secs = []
        for i in range(shnum):
            s = struct.unpack(">IIIIIIIIII", d[shoff + i * shentsize: shoff + i * shentsize + 40])
            secs.append(s)
        for s in secs:
            typ, flags, addr, off, size = s[1], s[2], s[3], s[4], s[5]
            if flags & 2 and addr >= MOD_BASE:  # SHF_ALLOC
                data = d[off: off + size] if typ != 8 else bytes(size)
                # Big-endian byte at A lives at host byte A ^ 3 (words stored host-native), which
                # also places sections that start off a word boundary correctly.
                for j, b in enumerate(data):
                    self.mod[((addr + j) ^ 3) - MOD_BASE] = b
        out = subprocess.run([OBJDUMP, "-t", path], capture_output=True, text=True).stdout
        for line in out.splitlines():
            parts = line.split()
            if len(parts) >= 5 and len(parts[0]) == 8:
                try:
                    a = int(parts[0], 16)
                except ValueError:
                    continue
                self.symbols[parts[-1]] = a
                if len(parts) >= 6 and parts[-3].startswith(".recomp_import.") and parts[-4] == "F":
                    self.imports[a] = parts[-1]

    # ---- default import handlers ---------------------------------------------------------------
    def _cfg_value(self, key):
        if key not in self.config:
            raise Fault("config key not set: " + key)
        return self.config[key]

    def _cfg_u32(self, m, args):
        return int(self._cfg_value(self.cstr(args[0]))) & 0xFFFFFFFF

    def _cfg_double(self, m, args):
        return ("double", float(self._cfg_value(self.cstr(args[0]))))

    # ---- memory --------------------------------------------------------------------------------
    def _region(self, a):
        if MOD_BASE <= a < MOD_BASE + MOD_SIZE:
            return self.mod, a - MOD_BASE
        if 0 <= a < EE_SIZE:
            return self.ee, a
        raise Fault("bad address %08x" % a)

    def r32(self, a):
        if a & 3:
            raise Fault("unaligned lw %08x" % a)
        if self.log_reads and a < EE_SIZE:
            self.ee_reads.append((a, 4))
        buf, o = self._region(a)
        return int.from_bytes(buf[o: o + 4], "little")

    def w32(self, a, v):
        if a & 3:
            raise Fault("unaligned sw %08x" % a)
        if self.log_writes and a < EE_SIZE:
            self.ee_writes.append((a, 4, v & 0xFFFFFFFF))
        buf, o = self._region(a)
        buf[o: o + 4] = (v & 0xFFFFFFFF).to_bytes(4, "little")

    def r8(self, a):
        if self.log_reads and a < EE_SIZE:
            self.ee_reads.append((a, 1))
        buf, o = self._region(a ^ 3)
        return buf[o]

    def w8(self, a, v):
        if self.log_writes and a < EE_SIZE:
            self.ee_writes.append((a, 1, v & 0xFF))
        buf, o = self._region(a ^ 3)
        buf[o] = v & 0xFF

    def r16(self, a):
        if a & 1:
            raise Fault("unaligned lh %08x" % a)
        if self.log_reads and a < EE_SIZE:
            self.ee_reads.append((a, 2))
        buf, o = self._region(a ^ 2)
        return int.from_bytes(buf[o: o + 2], "little")

    def w16(self, a, v):
        if a & 1:
            raise Fault("unaligned sh %08x" % a)
        if self.log_writes and a < EE_SIZE:
            self.ee_writes.append((a, 2, v & 0xFFFF))
        buf, o = self._region(a ^ 2)
        buf[o: o + 2] = (v & 0xFFFF).to_bytes(2, "little")

    def cstr(self, a):
        out = bytearray()
        while True:
            c = self.r8(a)
            if c == 0:
                return out.decode()
            out.append(c)
            a += 1

    # mod-memory structs of 32-bit words (how the host reads and writes them)
    def mod_words(self, a, n):
        return [self.r32(a + 4 * i) for i in range(n)]

    def set_mod_words(self, a, words):
        for i, v in enumerate(words):
            self.w32(a + 4 * i, v)

    # EE helpers for the test (real little-endian game memory)
    def ee_u32(self, a):
        return int.from_bytes(self.ee[a: a + 4], "little")

    def ee_set_u32(self, a, v):
        self.ee[a: a + 4] = (v & 0xFFFFFFFF).to_bytes(4, "little")

    def ee_f32(self, a):
        return struct.unpack("<f", self.ee[a: a + 4])[0]

    def ee_set_f32(self, a, v):
        self.ee[a: a + 4] = struct.pack("<f", v)

    def ee_u16(self, a):
        return int.from_bytes(self.ee[a: a + 2], "little")

    def ee_set_u16(self, a, v):
        self.ee[a: a + 2] = (v & 0xFFFF).to_bytes(2, "little")

    def ee_s16(self, a):
        return s16(self.ee_u16(a))

    # ---- CPU -----------------------------------------------------------------------------------
    def call(self, addr, max_steps=5_000_000, args=()):
        self.r = r = [0] * 32
        for i, v in enumerate(args):
            r[4 + i] = v & 0xFFFFFFFF
        self.f = [0] * 32  # raw 32-bit words
        self.fcc = False
        self.hi = self.lo = 0
        r[29] = MOD_BASE + MOD_SIZE - 0x100  # sp
        r[31] = RA_SENTINEL
        pc = addr
        steps = 0
        while True:
            if pc == RA_SENTINEL:
                self.steps = steps
                return r[2]
            if pc in self.imports:
                name = self.imports[pc]
                h = self.handlers.get(name)
                if h is None:
                    raise Fault("import without a handler: " + name)
                a = [r[4], r[5], r[6], r[7]]
                if self.log_imports:
                    self.import_calls.append((name, a))
                res = h(self, a)
                if isinstance(res, tuple) and res[0] == "double":
                    self.dset(0, res[1])
                elif res is not None:
                    r[2] = int(res) & 0xFFFFFFFF
                pc = r[31]
                continue
            steps += 1
            if steps > max_steps:
                raise Fault("step limit at %08x" % pc)
            w = self.r32(pc)
            target = self.exec_one(w, pc)
            if target is None:
                pc += 4
                continue
            # the delay slot
            dpc = pc + 4
            steps += 1
            if self.exec_one(self.r32(dpc), dpc) is not None:
                raise Fault("branch in delay slot at %08x" % dpc)
            pc = target

    def dget(self, i):
        f = self.f
        return struct.unpack("<d", struct.pack("<II", f[i], f[i + 1]))[0]

    def dset(self, i, x):
        lo, hi = struct.unpack("<II", struct.pack("<d", x))
        self.f[i], self.f[i + 1] = lo, hi

    def exec_one(self, w, pc):
        """Execute one instruction. Returns the next pc after the delay slot when the instruction is a transfer (a
        branch not taken returns pc + 8: its delay slot still runs), else None."""
        r, f = self.r, self.f
        op = w >> 26
        rs = (w >> 21) & 31
        rt = (w >> 16) & 31
        rd = (w >> 11) & 31
        sa = (w >> 6) & 31
        fn = w & 63
        imm = w & 0xFFFF
        simm = s16(imm)
        target = None
        if op == 0:
            if fn == 0x00:
                r[rd] = (r[rt] << sa) & 0xFFFFFFFF
            elif fn == 0x02:
                r[rd] = r[rt] >> sa
            elif fn == 0x03:
                r[rd] = (s32(r[rt]) >> sa) & 0xFFFFFFFF
            elif fn == 0x04:
                r[rd] = (r[rt] << (r[rs] & 31)) & 0xFFFFFFFF
            elif fn == 0x06:
                r[rd] = r[rt] >> (r[rs] & 31)
            elif fn == 0x07:
                r[rd] = (s32(r[rt]) >> (r[rs] & 31)) & 0xFFFFFFFF
            elif fn == 0x08:
                target = r[rs]
            elif fn == 0x09:
                target = r[rs]
                r[rd] = pc + 8
            elif fn == 0x10:
                r[rd] = self.hi
            elif fn == 0x11:
                self.hi = r[rs]
            elif fn == 0x12:
                r[rd] = self.lo
            elif fn == 0x13:
                self.lo = r[rs]
            elif fn == 0x18:
                p = s32(r[rs]) * s32(r[rt])
                self.lo, self.hi = p & 0xFFFFFFFF, (p >> 32) & 0xFFFFFFFF
            elif fn == 0x19:
                p = r[rs] * r[rt]
                self.lo, self.hi = p & 0xFFFFFFFF, (p >> 32) & 0xFFFFFFFF
            elif fn == 0x1A:
                a, b = s32(r[rs]), s32(r[rt])
                if b:
                    q = abs(a) // abs(b) * (1 if (a < 0) == (b < 0) else -1)
                    self.lo, self.hi = q & 0xFFFFFFFF, (a - q * b) & 0xFFFFFFFF
            elif fn == 0x1B:
                if r[rt]:
                    self.lo, self.hi = r[rs] // r[rt], r[rs] % r[rt]
            elif fn in (0x20, 0x21):
                r[rd] = (r[rs] + r[rt]) & 0xFFFFFFFF
            elif fn in (0x22, 0x23):
                r[rd] = (r[rs] - r[rt]) & 0xFFFFFFFF
            elif fn == 0x24:
                r[rd] = r[rs] & r[rt]
            elif fn == 0x25:
                r[rd] = r[rs] | r[rt]
            elif fn == 0x26:
                r[rd] = r[rs] ^ r[rt]
            elif fn == 0x27:
                r[rd] = ~(r[rs] | r[rt]) & 0xFFFFFFFF
            elif fn == 0x2A:
                r[rd] = 1 if s32(r[rs]) < s32(r[rt]) else 0
            elif fn == 0x2B:
                r[rd] = 1 if r[rs] < r[rt] else 0
            elif fn == 0x0D:
                raise Fault("break at %08x" % pc)
            else:
                raise Fault("SPECIAL fn %02x at %08x" % (fn, pc))
        elif op == 1:
            if rt == 0:  # bltz
                target = pc + 4 + simm * 4 if s32(r[rs]) < 0 else pc + 8
            elif rt == 1:  # bgez
                target = pc + 4 + simm * 4 if s32(r[rs]) >= 0 else pc + 8
            else:
                raise Fault("REGIMM %d at %08x" % (rt, pc))
        elif op == 2:
            target = (pc & 0xF0000000) | ((w & 0x3FFFFFF) << 2)
        elif op == 3:
            r[31] = pc + 8
            target = (pc & 0xF0000000) | ((w & 0x3FFFFFF) << 2)
        elif op == 4:
            target = pc + 4 + simm * 4 if r[rs] == r[rt] else pc + 8
        elif op == 5:
            target = pc + 4 + simm * 4 if r[rs] != r[rt] else pc + 8
        elif op == 6:
            target = pc + 4 + simm * 4 if s32(r[rs]) <= 0 else pc + 8
        elif op == 7:
            target = pc + 4 + simm * 4 if s32(r[rs]) > 0 else pc + 8
        elif op in (8, 9):
            r[rt] = (r[rs] + simm) & 0xFFFFFFFF
        elif op == 0x0A:
            r[rt] = 1 if s32(r[rs]) < simm else 0
        elif op == 0x0B:
            r[rt] = 1 if r[rs] < (simm & 0xFFFFFFFF) else 0
        elif op == 0x0C:
            r[rt] = r[rs] & imm
        elif op == 0x0D:
            r[rt] = r[rs] | imm
        elif op == 0x0E:
            r[rt] = r[rs] ^ imm
        elif op == 0x0F:
            r[rt] = (imm << 16) & 0xFFFFFFFF
        elif op == 0x11:  # COP1
            fmt = rs
            ft, fsr, fdr = rt, rd, sa
            if fmt == 0x00:  # mfc1
                r[rt] = f[fsr]
            elif fmt == 0x04:  # mtc1
                f[fsr] = r[rt]
            elif fmt == 0x08:  # bc1f / bc1t
                tf = rt & 1
                target = pc + 4 + simm * 4 if bool(self.fcc) == bool(tf) else pc + 8
            elif fmt == 0x10:  # .s
                a = u2f(f[fsr])
                b = u2f(f[ft])
                if fn == 0x00:
                    f[fdr] = f2u(f32(a + b))
                elif fn == 0x01:
                    f[fdr] = f2u(f32(a - b))
                elif fn == 0x02:
                    f[fdr] = f2u(f32(a * b))
                elif fn == 0x03:
                    f[fdr] = f2u(f32(a / b if b != 0 else (math.copysign(math.inf, a) if a != 0 else math.nan)))
                elif fn == 0x04:
                    f[fdr] = f2u(f32(math.sqrt(a) if a >= 0 else math.nan))
                elif fn == 0x05:
                    f[fdr] = f2u(f32(abs(a)))
                elif fn == 0x06:
                    f[fdr] = f[fsr]
                elif fn == 0x07:
                    f[fdr] = f[fsr] ^ 0x80000000
                elif fn == 0x0D:  # trunc.w.s
                    f[fdr] = _trunc(a)
                elif fn == 0x21:  # cvt.d.s
                    self.dset(fdr, a)
                elif 0x30 <= fn <= 0x3F:
                    self.fcc = _fcmp(fn & 0xF, a, b)
                else:
                    raise Fault("COP1.S fn %02x at %08x" % (fn, pc))
            elif fmt == 0x11:  # .d
                a = self.dget(fsr)
                b = self.dget(ft)
                if fn == 0x00:
                    self.dset(fdr, a + b)
                elif fn == 0x01:
                    self.dset(fdr, a - b)
                elif fn == 0x02:
                    self.dset(fdr, a * b)
                elif fn == 0x03:
                    self.dset(fdr, a / b if b != 0 else (math.copysign(math.inf, a) if a != 0 else math.nan))
                elif fn == 0x05:
                    self.dset(fdr, abs(a))
                elif fn == 0x06:
                    f[fdr], f[fdr + 1] = f[fsr], f[fsr + 1]
                elif fn == 0x07:
                    self.dset(fdr, -a)
                elif fn == 0x0D:  # trunc.w.d
                    f[fdr] = _trunc(a)
                elif fn == 0x20:  # cvt.s.d
                    f[fdr] = f2u(f32(a))
                elif 0x30 <= fn <= 0x3F:
                    self.fcc = _fcmp(fn & 0xF, a, b)
                else:
                    raise Fault("COP1.D fn %02x at %08x" % (fn, pc))
            elif fmt == 0x14:  # .w
                if fn == 0x20:  # cvt.s.w
                    f[fdr] = f2u(f32(float(s32(f[fsr]))))
                elif fn == 0x21:
                    self.dset(fdr, float(s32(f[fsr])))
                else:
                    raise Fault("COP1.W fn %02x at %08x" % (fn, pc))
            else:
                raise Fault("COP1 fmt %02x at %08x" % (fmt, pc))
        elif op == 0x20:
            v = self.r8((r[rs] + simm) & 0xFFFFFFFF)
            r[rt] = (v - 0x100 if v & 0x80 else v) & 0xFFFFFFFF
        elif op == 0x21:
            r[rt] = s16(self.r16((r[rs] + simm) & 0xFFFFFFFF)) & 0xFFFFFFFF
        elif op == 0x23:
            r[rt] = self.r32((r[rs] + simm) & 0xFFFFFFFF)
        elif op == 0x24:
            r[rt] = self.r8((r[rs] + simm) & 0xFFFFFFFF)
        elif op == 0x25:
            r[rt] = self.r16((r[rs] + simm) & 0xFFFFFFFF)
        elif op == 0x28:
            self.w8((r[rs] + simm) & 0xFFFFFFFF, r[rt])
        elif op == 0x29:
            self.w16((r[rs] + simm) & 0xFFFFFFFF, r[rt])
        elif op == 0x2B:
            self.w32((r[rs] + simm) & 0xFFFFFFFF, r[rt])
        elif op == 0x31:
            f[rt] = self.r32((r[rs] + simm) & 0xFFFFFFFF)
        elif op == 0x39:
            self.w32((r[rs] + simm) & 0xFFFFFFFF, f[rt])
        elif op == 0x35:  # ldc1 (FR=0 big-endian: high word at the lower address)
            a = (r[rs] + simm) & 0xFFFFFFFF
            f[rt + 1] = self.r32(a)
            f[rt] = self.r32(a + 4)
        elif op == 0x3D:  # sdc1
            a = (r[rs] + simm) & 0xFFFFFFFF
            self.w32(a, f[rt + 1])
            self.w32(a + 4, f[rt])
        else:
            raise Fault("op %02x at %08x (%08x)" % (op, pc, w))
        r[0] = 0
        return target
