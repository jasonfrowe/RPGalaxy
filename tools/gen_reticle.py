import sys
from PIL import Image, ImageDraw

def create_reticle():
    # 32x32 Image, RGBA
    size = 32
    im = Image.new('RGBA', (size, size), (0, 0, 0, 0)) # Fully transparent
    draw = ImageDraw.Draw(im)
    
    # Colors
    cyan = (0, 255, 255, 255)
    white = (255, 255, 255, 255)
    dark_blue = (0, 0, 128, 255)
    
    # Center
    cx, cy = size // 2, size // 2
    
    # Outer Ring (Radius 11)
    draw.ellipse((cx-11, cy-11, cx+11, cy+11), outline=cyan, width=1)
    
    # Inner Ring (Radius 8)
    # draw.ellipse((cx-8, cy-8, cx+8, cy+8), outline=dark_blue, width=1)
    
    # Crosshairs
    # Horizontal
    draw.line((cx-8, cy, cx-3, cy), fill=white, width=1)
    draw.line((cx+3, cy, cx+8, cy), fill=white, width=1)
    # Vertical
    draw.line((cx, cy-8, cx, cy-3), fill=white, width=1)
    draw.line((cx, cy+3, cx, cy+8), fill=white, width=1)
    
    # Dot in center
    draw.point((cx, cy), fill=white)
    
    # Corners (Brackets)
    d = 9
    l = 3
    # Top Left
    draw.line((cx-d, cy-d, cx-d+l, cy-d), fill=cyan, width=1)
    draw.line((cx-d, cy-d, cx-d, cy-d+l), fill=cyan, width=1)
    # Top Right
    draw.line((cx+d, cy-d, cx+d-l, cy-d), fill=cyan, width=1)
    draw.line((cx+d, cy-d, cx+d, cy-d+l), fill=cyan, width=1)
    # Bottom Left
    draw.line((cx-d, cy+d, cx-d+l, cy+d), fill=cyan, width=1)
    draw.line((cx-d, cy+d, cx-d, cy+d-l), fill=cyan, width=1)
    # Bottom Right
    draw.line((cx+d, cy+d, cx+d-l, cy+d), fill=cyan, width=1)
    draw.line((cx+d, cy+d, cx+d, cy+d-l), fill=cyan, width=1)

    # Convert to P mode for preview? No, keep RGBA for the converter
    im.save("images/reticle.png")
    print("Generated images/reticle.png")

if __name__ == "__main__":
    create_reticle()
