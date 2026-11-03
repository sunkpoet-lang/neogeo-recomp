# Why

The community requests the original KOF98 set in ForkAde alongside the tested Magic Plus II variant. The supplied kof98.zip contains encrypted 242-p1.p1 CRC 8893df89 and 242-p2.sp2 CRC 980aba4c, not a predecrypted clone.

# What Changes

Add an exact KOF98 cartridge definition, MAME-matching program decryption and the stateful Altera header overlay. Reuse unencrypted S/C/M/V data, existing Neo Geo controls and GekkoNet. Provide its own title icon and validate local deterministic/rollback/peer operation. Integrate both new entries into a separate complete ForkAde candidate without replacing the user-tested E: baseline.

# Impact

Board protection state is serialised only for the enabled KOF98 cartridge, retaining other games' hashes/state layouts. No ROMs/BIOS in source or packages. Native host EXE retains the existing interpreted 68000; this is not a new static translator. No commit/push or public release.
