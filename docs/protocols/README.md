# Console Protocol Documentation

This directory contains comprehensive technical documentation of the retro console communication protocols implemented in Joypad. Each protocol represents significant reverse-engineering work and aims to serve as a definitive reference for developers.

## Available Protocol Documentation

### ✅ [Nuon Polyface Protocol](NUON_POLYFACE.md)
**Status**: Complete
**Reverse-Engineered**: 2022-2023
**Significance**: First open-source documentation

The Nuon Polyface controller protocol was previously completely undocumented. This represents months of hardware analysis, SDK research, and iterative testing.

### ✅ [PCEngine / TurboGrafx-16 Protocol](PCENGINE.md)
**Status**: Complete
**Implemented**: 2022-2023
**Significance**: Comprehensive multitap and mouse implementation reference

While the basic PCEngine controller protocol is documented in the community, this reference provides detailed coverage of:
- Multitap scanning mechanism (5-player support)
- Mouse delta accumulation strategy
- 6-button and 3-button mode implementations
- RP2040 PIO state machine architecture
- Dual-core coordination for timing-critical output

### ✅ [GameCube Joybus Protocol](GAMECUBE_JOYBUS.md)
**Status**: Complete
**Implemented**: 2022-2025
**Significance**: Reverse-engineered keyboard protocol + comprehensive profile system

While the basic joybus protocol is documented, this reference provides:
- **Reverse-engineered GameCube keyboard protocol** (0x54 command, keycode mappings, checksum algorithm)
- Sophisticated profile system with flash persistence (SSBM, MKWii, Fighting, SNES configs)
- Joybus PIO timing implementation (130MHz overclocking requirement)
- Advanced trigger logic for different controller types
- Dual-core coordination for timing-critical output

## Planned Documentation

### 🚧 Xbox One I2C Protocol
**Status**: Planned
**Implementation**: `src/console/xboxone/`

Internal USB host mod protocol:
- I2C communication with Xbox One controller chip
- Button/analog passthrough
- Rumble protocol (partial reverse-engineering)
- Integration challenges

## Documentation Standards

Each protocol document should include:

### 1. **Overview Section**
- Historical context
- Significance (original documentation or reverse-engineered)
- Key characteristics
- Physical layer description

### 2. **Protocol Details**
- Packet structure with bit-level diagrams
- Command reference with hex values
- State machines and enumeration sequences
- Timing diagrams and requirements

### 3. **Implementation Notes**
- Hardware requirements
- PIO programs (if applicable)
- Dual-core considerations
- Known limitations

### 4. **Practical Examples**
- Real packet captures
- Common command sequences
- Button/analog encoding tables
- CRC/checksum algorithms

### 5. **Appendices**
- Quick reference tables
- Bit field maps
- Acknowledgments
- References

## Document Template

Use this structure for new protocol documentation:

```markdown
# [Console Name] [Protocol Name] Protocol

**[Status of prior documentation]**

Reverse-engineered/Documented by [Author] ([Year])

[Brief description of significance]

---

## Table of Contents
[...]

## Overview
[...]

## Physical Layer
[...]

## Packet Structure
[...]

## Protocol State Machine
[...]

## Command Reference
[...]

## [Console-Specific Sections]
[...]

## Implementation Notes
[...]

## Acknowledgments
[...]

## References
[...]
```

## Contributing

When adding new protocol documentation:

1. **Research thoroughly** - Include citations for existing documentation
2. **Credit sources** - Acknowledge prior work and community contributions
3. **Provide examples** - Real-world packet captures and test cases
4. **Document discoveries** - Highlight reverse-engineered insights
5. **Maintain consistency** - Follow the established format

## Significance of This Work

These protocol documents serve multiple purposes:

1. **Preservation** - Document protocols before hardware/knowledge is lost
2. **Education** - Teach modern developers about retro communication protocols
3. **Reproducibility** - Enable others to build compatible implementations
4. **Community** - Share knowledge with homebrew and preservation communities

Many retro protocols were never officially documented, existing only in proprietary SDKs or firmware. Reverse-engineering and documenting these protocols ensures they remain accessible for future generations.

## Related Resources

### External Protocol Documentation

- **GameCube Joybus**: [joybus-pio](https://github.com/JonnyHaystack/joybus-pio)
- **N64 Controller**: [N64brew](https://n64brew.dev)
- **PCEngine**: Limited public documentation
- **Nuon**: No prior public documentation (see NUON_POLYFACE.md)

### Retro Console Communities

- **Nuon-Dome**: Nuon homebrew community
- **N64brew**: Nintendo 64 development
- **NEC Retro**: PCEngine/TurboGrafx-16

## License

All protocol documentation in this directory is licensed under Apache License 2.0, matching the Joypad project license.

---

*This directory represents countless hours of hardware analysis, logic analyzer captures, SDK research, and iterative testing. Each protocol document is a contribution to the preservation of retro gaming technology.*
