import os
import re
from PIL import Image

def rgb888_to_rgb565(r, g, b):
    """Converts 8-bit R, G, B values to 16-bit RGB565 integer."""
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

def sanitize_var_name(filename):
    """Converts filename like 'idle_sprite.jpg' into valid C variable name 'idle_sprite'."""
    name = os.path.splitext(os.path.basename(filename))[0]
    # Replace non-alphanumeric characters with underscores
    name = re.sub(r'[^a-zA-Z0-9_]', '_', name)
    # Ensure it doesn't start with a number
    if name[0].isdigit():
        name = '_' + name
    return name

def generate_multi_sprite_header(image_paths, output_header_path="pumpkin_sprite.h", target_size=(128, 128)):
    with open(output_header_path, 'w') as f:
        # Header guards
        f.write("#ifndef PUMPKIN_SPRITE_H\n")
        f.write("#define PUMPKIN_SPRITE_H\n\n")
        f.write("#include <stdint.h>\n\n")

        for image_path in image_paths:
            if not os.path.exists(image_path):
                print(f"Warning: File '{image_path}' not found. Skipping.")
                continue

            var_name = sanitize_var_name(image_path)

            # Load and convert image
            img = Image.open(image_path).convert('RGB')
            if target_size:
                img = img.resize(target_size, Image.Resampling.NEAREST)

            width, height = img.size
            total_pixels = width * height

            print(f"Processing '{image_path}' -> Array: {var_name}[{total_pixels}] ({width}x{height})")

            # Write defines and array
            f.write(f"// Sprite from: {os.path.basename(image_path)}\n")
            f.write(f"#define {var_name.upper()}_WIDTH  {width}\n")
            f.write(f"#define {var_name.upper()}_HEIGHT {height}\n")
            f.write(f"const uint16_t {var_name}[{total_pixels}] = {{\n")

            for y in range(height):
                f.write("    ")
                for x in range(width):
                    r, g, b = img.getpixel((x, y))
                    rgb565 = rgb888_to_rgb565(r, g, b)
                    f.write(f"0x{rgb565:04X}, ")
                f.write("\n")

            f.write("};\n\n")

        f.write("#endif // PUMPKIN_SPRITE_H\n")

    print(f"\nHeader generated successfully: {output_header_path}")

# Example Usage: Pass a list of your sprite images
sprite_files = [
    "idle_sprite.jpg",
    "bored_sprite.jpg",
    "hungry_sprite.jpg"
]

generate_multi_sprite_header(sprite_files, "pumpkin_sprite.h")