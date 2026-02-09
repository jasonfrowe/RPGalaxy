from PIL import Image, ImageDraw

def create_worker_sprite():
    # 64x16 strip, 4 frames of 16x16
    width = 64
    height = 16
    img = Image.new('RGBA', (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    colors = [
        (0, 255, 255),    # Cyan (Guardian)
        (0, 200, 255),
        (255, 0, 255),    # Magenta (Gardener)
        (255, 100, 255),
    ]

    # Frame 0: Guardian (Idle) - Triangle / Shield shape
    # Cyan
    cx, cy = 8, 8
    draw.polygon([(cx, cy-6), (cx-5, cy+4), (cx+5, cy+4)], outline=(0, 255, 255), fill=None)
    draw.point((cx, cy), fill=(255, 255, 255)) # Eye

    # Frame 1: Guardian (Active) - Expanding
    cx = 24
    draw.polygon([(cx, cy-7), (cx-6, cy+5), (cx+6, cy+5)], outline=(100, 255, 255), fill=None)
    draw.point((cx, cy), fill=(255, 255, 255)) 

    # Frame 2: Gardener (Idle) - Circle / Flower
    # Magenta
    cx = 40
    draw.ellipse([(cx-5, cy-5), (cx+5, cy+5)], outline=(255, 0, 255), fill=None)
    draw.point((cx, cy), fill=(255, 255, 255))

    # Frame 3: Gardener (Active) - Pulse
    cx = 56
    draw.ellipse([(cx-6, cy-6), (cx+6, cy+6)], outline=(255, 100, 255), fill=None)
    draw.point((cx, cy), fill=(255, 255, 255))

    img.save("images/worker.png")
    print("Generated images/worker.png")

if __name__ == "__main__":
    create_worker_sprite()
