
import math

def generate_tables():
    print("Generating tables.h...")
    # Assumes running from project root
    with open("src/tables.h", "w") as f:
        f.write("#ifndef TABLES_H\n")
        f.write("#define TABLES_H\n\n")
        f.write("#include <stdint.h>\n\n")
        
        # 256 entries for 0 to 2*PI
        # Amplitude 1.0 = 256 (8.8 fixed point)
        f.write("static const int16_t SIN_LUT[256] = {\n")
        for i in range(256):
            # i counts from 0 to 255 representing 0 to 2*PI
            # angle = i * 2 * PI / 256
            angle = i * 2 * math.pi / 256
            val = int(math.sin(angle) * 256)
            f.write(f"    {val},")
            if (i + 1) % 16 == 0:
                f.write("\n")
        f.write("};\n\n")
        
        f.write("#endif // TABLES_H\n")

if __name__ == "__main__":
    generate_tables()
