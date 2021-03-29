import time
import os
import errno
#import Adafruit_GPIO.SPI as SPI
#import Adafruit_SSD1306
import board
import busio
import adafruit_ssd1306

from PIL import Image
from PIL import ImageDraw
from PIL import ImageFont

# Raspberry Pi pin configuration:
RST = 24

# only used with SPI
DC = 23
SPI_PORT = 0
SPI_DEVICE = 0
 
# Use for I2C
i2c = busio.I2C(board.SCL, board.SDA)

# 128x32 display with hardware I2C:
disp = adafruit_ssd1306.SSD1306_I2C(128, 32, i2c)
#disp = adafruit_displayio_ssd1306.SSD1306(display_bus, width=128, height=32)
#disp.begin()
#disp.clear()
#disp.display()
disp.fill(0)
disp.show()

width = disp.width
height = disp.height

# create object to draw on. '1' specifices 1 bit color
canvas = Image.new('1', (width, height))

# Get drawing object to draw on image.
draw = ImageDraw.Draw(canvas)

padding = 2 
top = padding
bottom = height - padding 
x = padding

FIFO = "bc6-printlog"
if os.path.isfile(FIFO):
    os.remove(FIFO)
try:
    os.mkfifo(FIFO)
except OSError as oe: 
    if oe.errno != errno.EEXIST:
        raise

index = [x, top] # x, y
font = ImageFont.load_default()

last_line = ""
while True:
    print("Opening FIFO")
    with open(FIFO) as print_queue:
        time.sleep(0.1)
        line = print_queue.read()
        if len(line) == 0: 
            print("FIFO closed")
            continue
        print(f"Read line: {line}")
        if index[1] >= bottom: # reset  the screen
            index[1] = top 
            draw.rectangle((0,0,width,height), outline=0, fill=0)
            draw.text((index[0], index[1]), last_line, font=font, fill=255)
            index[1] += 10
        draw.text((index[0], index[1]), line, font=font, fill=255)
        index[1] += 10
        disp.image(canvas)
        disp.show()
        last_line = line
