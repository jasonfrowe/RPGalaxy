from PIL import Image, ImageDraw

def create_enemy_strip():
    # 4 frames of 16x16 = 64x16 image
    width = 64
    height = 16
    img = Image.new('RGBA', (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    # Colors
    purple = (128, 0, 128, 255)
    red = (255, 0, 0, 255)
    dark_red = (139, 0, 0, 255)
    white = (255, 255, 255, 255)
    
    # Frame 0: Wings closed
    # Offset 0
    cx, cy = 8, 8
    # Body
    draw.ellipse((cx-3, cy-6, cx+3, cy+6), fill=dark_red)
    # Eyes
    draw.point((cx-1, cy-4), fill=red)
    draw.point((cx+1, cy-4), fill=red)
    
    # Frame 1: Wings opening
    # Offset 16
    cx += 16
    # Wings
    draw.line((cx-3, cy-2, cx-6, cy-5), fill=purple, width=1)
    draw.line((cx+3, cy-2, cx+6, cy-5), fill=purple, width=1)
    # Body
    draw.ellipse((cx-3, cy-6, cx+3, cy+6), fill=dark_red)
    draw.point((cx-1, cy-4), fill=red)
    draw.point((cx+1, cy-4), fill=red)

    # Frame 2: Wings fully open
    # Offset 32
    cx += 16
    # Wings
    draw.line((cx-3, cy-2, cx-7, cy-6), fill=purple, width=1)
    draw.line((cx+3, cy-2, cx+7, cy-6), fill=purple, width=1)
    draw.line((cx-3, cy+2, cx-7, cy+6), fill=purple, width=1)
    draw.line((cx+3, cy+2, cx+7, cy+6), fill=purple, width=1)
    # Body
    draw.ellipse((cx-2, cy-5, cx+2, cy+5), fill=dark_red) # Thinner body when flying?
    draw.point((cx-1, cy-4), fill=white) # Eyes glow
    draw.point((cx+1, cy-4), fill=white)

    # Frame 3: Wings closing (similar to 1 but maybe different phase)
    # Offset 48
    cx += 16
    # Wings
    draw.line((cx-3, cy-2, cx-6, cy-5), fill=purple, width=1)
    draw.line((cx+3, cy-2, cx+6, cy-5), fill=purple, width=1)
    # Body
    draw.ellipse((cx-3, cy-6, cx+3, cy+6), fill=dark_red)
    draw.point((cx-1, cy-4), fill=red)
    draw.point((cx+1, cy-4), fill=red)

    img.save('images/enemy.png')
    print("Generated images/enemy.png")

if __name__ == "__main__":
    create_enemy_strip()
