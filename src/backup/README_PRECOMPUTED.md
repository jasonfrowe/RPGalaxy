# Precomputed Galaxy System

## Overview
This system renders a perfect galaxy simulation by precomputing frames in Python and playing them back from USB storage.

## Building

```bash
cmake -B build/target-native
cmake --build build/target-native
```

This creates two executables:
- `RPGalaxy` - Original real-time simulation
- `RPGalaxyPrecomputed` - Precomputed frame playback

## Usage

### 1. Generate Frames

```bash
cd tools
python3 generate_galaxy.py
```

This creates `galaxy_frames.bin` (~28MB, 600 frames).

### 2. Copy to USB

Copy `galaxy_frames.bin` to the root of your RP6502's USB drive.

### 3. Run

Load `RPGalaxyPrecomputed.rp6502` on your RP6502.

## Performance

- **Playback**: 10 FPS (adjustable via `frames_per_update` in code)
- **CPU Load**: Minimal - just file I/O and screen writes
- **Music**: 60 FPS maintained (not integrated yet)

## Customization

Edit `tools/generate_galaxy.py`:
- `num_frames`: Number of frames to generate
- `N`: Particle grid size (125 = 15,625 particles)
- `t += 0.1`: Time step (lower = slower evolution)
