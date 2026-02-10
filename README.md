# RP6502 Galaxy Simulator 🌌

A chaotic attractor simulation and biological galaxy defense game running on the [RP6502 Picocomputer](https://github.com/picocomputer/rp6502).

## 1. The Simulation 🌀
The core visual is a **strange attractor** mathematical system, inspired by a generative art algorithm.
*   **Math**: The system iterates $x$ and $y$ coordinates through a feedback loop involving sine and cosine waves:
    ```python
    u = sin(i+y) + sin(r*i+x)
    v = cos(i+y) + cos(r*i+x)
    x = u + t
    y = v
    ```
*   **Visuals**: 
    *   **Gold Stream**: Hydrogen-rich matter (particles $i < N/2$).
    *   **Cyan Stream**: Oxygen-rich matter (particles $i \ge N/2$).
    *   **Rendering**: Uses a 3x3 blur accumulation buffer to create soft, glowing trails that decay over time.

## 2. The Threat: Enemies 👾
**Red/Orange Entities** that orbit the galaxy center.
*   **Behavior**: They follow **Keplerian Orbits** around the galactic core.
*   **Effect**: They release a viral agent that **infects** the galaxy.
    *   Infection turns Gold matter into cold Cyan/Blue matter, disrupting the balance of the simulation.

## 3. The Defense: Workers 🛡️
You can spawn workers to protect the galaxy. Workers also follow Keplerian orbits but can be directed by the player.

### Controls 🖱️
*   **Aim**: Move the reticle.
*   **Spawn**: Click to release a worker.
*   **Orbit Control**:
    *   **Direction**: The worker spawns at the **Apocenter** (farthest point) of its orbit, aligned with your click. It will fall towards the center.
    *   **Shape**: The **Reticle Pulses**. 
        *   Click when **Small** -> **Circular Orbit**.
        *   Click when **Large** -> **Elliptical Orbit**.

### Worker Types
*   **Guardians (Cyan)**: 
    *   **Role**: Fighter.
    *   **Ability**: Automatically seeks out and **destroys** nearby Enemies upon collision.
*   **Gardeners (Magenta)**:
    *   **Role**: Healer.
    *   **Ability**: Emits a healing field that **restores** infected particles to their natural state, reversing the damage done by enemies.

## Technical Details 🛠️
*   **Platform**: RP6502 (6502 CPU + Raspberry Pi Pico VGA).
*   **Language**: C (LLVM-MOS SDK).
*   **Physics**: Custom 8-bit/16-bit fixed-point physics engine.
    *   Zero floating-point math.
    *   Keplerian orbital mechanics with $1/r$ velocity scaling.
    *   Rotated geometric orbits.
