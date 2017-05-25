#!/bin/python3.6

from PIL import Image
import sys

if len(sys.argv) <= 3:
    print("Usage: python3.6 imgToMap.py <file> <size> <name>")
    exit()

img = Image.open(sys.argv[1]).convert('RGB')

def rangeGen(start, end, step):
    while start <= end:
        yield start
        start += step

print("int %s[%s] = {" % (sys.argv[3],sys.argv[2]), end='')

for i in rangeGen((img.size[0]/int(sys.argv[2]))/2,img.size[0],(img.size[0]/int(sys.argv[2]))):
    r,g,b = img.getpixel((i,(img.size[1]/2)))
    print("0x%0.4X" % ((int(r / 255 * 31) << 11) | (int(g / 255 * 63) << 5) | (int(b / 255 * 31))), end='')
    if not i+(img.size[0]/int(sys.argv[2])) > img.size[0]:
        print(",", end='')

print("};")
exit()
