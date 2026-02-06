#!/usr/bin/env python3
"""
Generate precomputed galaxy animation frames.
Tracks particle positions and outputs ONLY changed pixels per frame.
"""
import math
import struct
import sys

# Parameters
W = 320  # Screen width
H = 180  # Screen height
N = 125  # Particle grid size (125x125 = 15,625 particles)

# Simulation state
x, y, t = 0.0, 0.0, 0.0

def generate_frames(num_frames=600, output_file="galaxy_frames.bin"):
    """Generate galaxy frames with position tracking."""
    global x, y, t
    
    # Track previous position for each particle
    prev_pos = {}  # (i, j) -> (sx, sy)
    
    with open(output_file, 'wb') as f:
        for frame_num in range(num_frames):
            print(f"Generating frame {frame_num + 1}/{num_frames}...", file=sys.stderr)
            
            changes = []  # (old_x, old_y, new_x, new_y, color)
            curr_pos = {}
            
            # Process all particles for this frame
            for i in range(N):
                for j in range(N):
                    r = (2 * math.pi) / N
                    u = math.sin(i + y) + math.sin(r * i + x)
                    v = math.cos(i + y) + math.cos(r * i + x)
                    
                    # Update global state
                    x = u + t
                    y = v
                    
                    # Convert to screen coordinates
                    sx = int(u * N / 2 + W / 2)
                    sy_raw = int(v * N / 2)
                    sy = int(H / 2 + sy_raw * 0.75)
                    
                    # Color
                    color = 16 + (i * 11) % 216 + (j % 20)
                    if color > 231:
                        color = 231
                    
                    # Check if particle moved
                    key = (i, j)
                    if 0 <= sx < W and 0 <= sy < H:
                        curr_pos[key] = (sx, sy)
                        
                        if key in prev_pos:
                            old_x, old_y = prev_pos[key]
                            if (old_x, old_y) != (sx, sy):
                                # Particle moved - erase old, draw new
                                changes.append((old_x, old_y, sx, sy, color))
                        else:
                            # New particle - just draw (use 0xFFFF for no erase)
                            changes.append((0xFFFF, 0, sx, sy, color))
                    else:
                        # Particle off-screen - erase old position if it had one
                        if key in prev_pos:
                            old_x, old_y = prev_pos[key]
                            changes.append((old_x, old_y, 0xFFFF, 0, 0))
            
            # Advance time after all particles processed
            t += 0.1
            
            # Update tracking
            prev_pos = curr_pos
            
            # Write frame: change_count (uint16) + changes
            f.write(struct.pack('<H', len(changes)))
            for old_x, old_y, new_x, new_y, color in changes:
                f.write(struct.pack('<HHHHB', old_x, old_y, new_x, new_y, color))
            
            print(f"  Frame {frame_num + 1}: {len(changes)} changes", file=sys.stderr)
    
    print(f"\nGenerated {num_frames} frames to {output_file}", file=sys.stderr)

if __name__ == "__main__":
    generate_frames(600, "galaxy_frames.bin")
