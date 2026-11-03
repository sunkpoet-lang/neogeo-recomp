# Why

The player supplied the split `kf2k2mp2` (KOF2002 Magic Plus II) set. Its three variant members were identified separately from the original KOF2002. The original decrypt pipeline cannot load this cartridge correctly.

# What Changes

- Add a dedicated game definition, resolving shared resources from the adjacent `kof2002.zip` and BIOS from `neogeo.zip`.
- Validate the entire variant layout before mutation; apply Magic Plus II program relocation and S1 half-block swap, plus shared CMC50 graphics/M1 and PCM2 type 0.
- Keep the runtime's linear decrypted M1 representation and existing Z80 bank behaviour.
- Build a separate test executable, verify real boot/graphics/audio and create its icon from its own observed title screen. Preserve the deployed ForkAde baseline.

# Impact

Scoped changes to game metadata, decryption dispatch/helpers and tests. No CPU, netplay or launcher protocol changes. No claim of full static recompilation; the runtime interprets the 68000. No ROMs or BIOS in source/packages. No commit, push or deployment.
