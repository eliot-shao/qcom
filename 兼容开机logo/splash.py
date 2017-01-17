# Copyright (c) 2013, The Linux Foundation. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of The Linux Foundation nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
# ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
# BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
# OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
# IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#===========================================================================

#  This script read the logo png and creates the logo.img

# when          who     what, where, why
# --------      ---     -------------------------------------------------------
# 2013-04       QRD     init
# 2016-07       mickey  put animaton.zip into splash memroy
#
# Environment requirement:
#     Python + PIL
#     PIL install:
#         (ubuntu)  sudo apt-get install python-imaging
#         (windows) (http://www.pythonware.com/products/pil/)
#
# limit:
#    the logo png file's format must be:
#      a Truecolour with alpha: each pixel consists of four samples,
#         only allow 8-bit depeths: red, green, blue, and alpha.
#      b Truecolour: each pixel consists of three samples,
#         only allow 8-bit depeths: red, green, and blue.
#    the logo Image layout:
#       logo_header + BGR RAW Data
#
#-------------------------------------------------------------------------------
# 2016-07-30 modify:
# the detail information you can read code  splash_screen_mmc @ bootable/bootloader/lk/app/aboot/aboot.c
#
# and also to see it in getAnimationFromSplash @ frameworks/base/cmds/bootanimation/BootAnimation.cpp
# ===========================================================================*/

import sys,os
import struct
import StringIO
import zipfile
import re
from PIL import Image

########
# 0               512                  2M          4M                    12M                       20M 
# | address index  |   lk logo  zone    |kernel logo |  bootanimation.zip  |  shutdownanimation.zip |
# | id +  data     |   head  +  body    | head + body|   head + body       |   head + body          |
########
lk_offset = 512
kernel_offset = 2*1024*1024
bootanimation_offset = 5*1024*1024
shutdownanimation_offset = 12*1024*1024


def ShowUsage():
    print " usage: python logo_gen.py [logo.png] [bootanimation.zip] [shutdownanimation.zip]"

def GetImgHeader(size):
    SECTOR_SIZE_IN_BYTES = 512   # Header size

    header = [0 for i in range(SECTOR_SIZE_IN_BYTES)]
    width, height = size
    #print size, width , height, size_in_bytes
    print size, width , height

    # magic
    header[0:7] = [ord('L'),ord('K'), ord('L'), ord('O'),
                   ord('G'),ord('O'), ord('!'), ord('!')]
    
    # width
    header[8] = ( width        & 0xFF)
    header[9] = ((width >> 8 ) & 0xFF)
    header[10]= ((width >> 16) & 0xFF)
    header[11]= ((width >> 24) & 0xFF)

    # height
    header[12]= ( height        & 0xFF)
    header[13]= ((height >>  8) & 0xFF)
    header[14]= ((height >> 16) & 0xFF)
    header[15]= ((height >> 24) & 0xFF)

    output = StringIO.StringIO()
    for i in header:
        output.write(struct.pack("B", i))
    content = output.getvalue()
    output.close()

    # only need 512 bytes
    return content[:512]


## get png raw data : BGR Interleaved

def CheckImage(mode):
    if mode == "RGB" or mode == "RGBA":
        return
    print "error: need RGB or RGBA format with 8 bit depths"
    sys.exit()

def GetImageBody(img):
    color = (0, 0, 0)
    if img.mode == "RGB":
        r, g, b = img.split()

    if img.mode == "RGBA":
        background = Image.new("RGB", img.size, color)
        img.load()
        background.paste(img, mask=img.split()[3]) # 3 is the alpha channel
        r, g, b = background.split()

    return Image.merge("RGB",(b,g,r)).tostring()

## make index	
def MakeSplashIndex(boot_size, shut_size):
    INDEX_SIZE_IN_BYTES = 512       ## index zone size : 32 bytes;

    print "bootanimation.zip size is ", boot_size, "shutdownanimation.zip size is ", shut_size
	## use global
#    global lk_offset
#    global kernel_offset
#    global bootanimation_offset
#    global shutdownanimation_offset

    index = [0 for i in range(INDEX_SIZE_IN_BYTES)]
	## zone id
    index[0:7] = [ord('S'),ord('P'), ord('L'), ord('A'),
                   ord('S'),ord('H'), ord('!'), ord('!')]

    # width
    index[8] = ( lk_offset        & 0xFF)
    index[9] = ((lk_offset >> 8 ) & 0xFF)
    index[10]= ((lk_offset >> 16) & 0xFF)
    index[11]= ((lk_offset >> 24) & 0xFF)

    # height
    index[12]= ( kernel_offset        & 0xFF)
    index[13]= ((kernel_offset >>  8) & 0xFF)
    index[14]= ((kernel_offset >> 16) & 0xFF)
    index[15]= ((kernel_offset >> 24) & 0xFF)

    # height
    index[16]= ( bootanimation_offset       & 0xFF)
    index[17]= ((bootanimation_offset >>  8) & 0xFF)
    index[18]= ((bootanimation_offset >> 16) & 0xFF)
    index[19]= ((bootanimation_offset >> 24) & 0xFF)

    # height
    index[20]= ( shutdownanimation_offset        & 0xFF)
    index[21]= ((shutdownanimation_offset >>  8) & 0xFF)
    index[22]= ((shutdownanimation_offset >> 16) & 0xFF)
    index[23]= ((shutdownanimation_offset >> 24) & 0xFF)

	# bootanimation.zip size
    index[24]= ( boot_size        & 0xFF)
    index[25]= ((boot_size >>  8) & 0xFF)
    index[26]= ((boot_size >> 16) & 0xFF)
    index[27]= ((boot_size >> 24) & 0xFF)
	# bootanimation.zip size
    index[28]= ( shut_size        & 0xFF)
    index[29]= ((shut_size >>  8) & 0xFF)
    index[30]= ((shut_size >> 16) & 0xFF)
    index[31]= ((shut_size >> 24) & 0xFF)

    output = StringIO.StringIO()
    for i in index:
        output.write(struct.pack("B", i))
    content = output.getvalue()
    output.close()

    # only need 512 bytes
    return content[:512]
## -------------------------------------- bootanimation.zip -----------------------------------------
## bootanimation.zip head
def BootAnimationHead(fileSize):
    INDEX_SIZE_IN_BYTES = 32

    boot = [0 for i in range(INDEX_SIZE_IN_BYTES)]
	## zone id
    boot[0:14] = [ord('B'),ord('O'), ord('O'), ord('T'),
                   ord('A'),ord('N'), ord('I'), ord('M'),
                   ord('A'),ord('T'), ord('I'), ord('O'),
                   ord('N'),ord('!'), ord('!')]

    # size
    boot[15]= ( fileSize       & 0xFF)
    boot[16]= ((fileSize >>  8) & 0xFF)
    boot[17]= ((fileSize >> 16) & 0xFF)
    boot[18]= ((fileSize >> 24) & 0xFF)
    
    output = StringIO.StringIO()
    for i in boot:
        output.write(struct.pack("B", i))
    content = output.getvalue()
    output.close()

    # only need 512 bytes
    return content[:32]

## deal with bootanimation.zip
def BootAnimationBody():
    bootanimation = sys.argv[3]
    print bootanimation

## Get zip context into memory zone
    #with open(bootanimation, "rb") as f:
    f  = open(bootanimation, "rb")
    content = ""
    try:
        while True:
            chunk = f.read(1024)
            if not chunk:
                break
            content += chunk
    finally:
        f.close()
    return content

## Pack the tail	
def BootAnimationEnd():
    INDEX_SIZE_IN_BYTES = 32

    boot_end = [0 for i in range(INDEX_SIZE_IN_BYTES)]
	## zone id
    boot_end[0:15] = [ord('B'),ord('O'), ord('O'), ord('T'),
                   ord('A'),ord('N'), ord('I'), ord('M'),
                   ord('A'),ord('T'), ord('I'), ord('O'),
                   ord('N'),ord('E'), ord('N'), ord('D')]
    
    output = StringIO.StringIO()
    for i in boot_end:
        output.write(struct.pack("B", i))
    content = output.getvalue()
    output.close()

    # only need 512 bytes
    return content[:32]
	
## -------------------------------------- bootanimation.zip -----------------------------------------

## -------------------------------------- shutdownanimation.zip -----------------------------------------
def ShutdownAnimationHead(fileSize):
    INDEX_SIZE_IN_BYTES = 32

    shutdown = [0 for i in range(INDEX_SIZE_IN_BYTES)]
	## zone id
    shutdown[0:18] = [ord('S'),ord('H'), ord('U'), ord('T'),
                   ord('D'),ord('O'), ord('W'), ord('N'),
                   ord('A'),ord('N'), ord('I'), ord('M'),
                   ord('A'),ord('T'), ord('I'), ord('O'),
				   ord('N'),ord('!'), ord('!')]

    # size
    shutdown[19]= ( fileSize       & 0xFF)
    shutdown[20]= ((fileSize >>  8) & 0xFF)
    shutdown[21]= ((fileSize >> 16) & 0xFF)
    shutdown[22]= ((fileSize >> 24) & 0xFF)
    
    output = StringIO.StringIO()
    for i in shutdown:
        output.write(struct.pack("B", i))
    content = output.getvalue()
    output.close()

    # only need 32 bytes
    return content[:32]
 
## deal with shutdownanimation.zip
def ShutdownAnimationBody():
    shutdownanimation = sys.argv[4]
    print shutdownanimation

## Get zip context into memory zone
    #with open(bootanimation, "rb") as f:
    f  = open(shutdownanimation, "rb")
    content = ""
    try:
        while True:
            chunk = f.read(1024)
            if not chunk:
                break
            content += chunk
    finally:
        f.close()
    return content

def ShutdownAnimationEnd():
    INDEX_SIZE_IN_BYTES = 32

    shutdown_end = [0 for i in range(INDEX_SIZE_IN_BYTES)]
	## zone id
    shutdown_end[0:19] = [ord('S'),ord('H'), ord('U'), ord('T'),
                   ord('D'),ord('O'), ord('W'), ord('N'),
                   ord('A'),ord('N'), ord('I'), ord('M'),
                   ord('A'),ord('T'), ord('I'), ord('O'),
				   ord('N'),ord('E'), ord('N'), ord('D')]
    
    output = StringIO.StringIO()
    for i in shutdown_end:
        output.write(struct.pack("B", i))
    content = output.getvalue()
    output.close()

    # only need 32 bytes
    return content[:32]
## -------------------------------------- shutdownanimation.zip -----------------------------------------

# check the memory size : splash = 20M
# | splash Head | lk logo | kernel logo |       bootanimation.zip              |     shutdownanimation.zip            | 
# |   512 bytes |     4M - 512 bytes    | 32 bytes head + body + 32 bytes tail | 32 bytes head + body + 32 bytes tail |
# |------------0 ~ 4M ------------------|---------------------------- 4M ~ 20M ---------------------------------------|
#
def CheckAnimation():
    bootsize_in_bytes = 0
    shutsize_in_bytes = 0
    num = len(sys.argv)

    if num >= 4:
        bootanimation_f = sys.argv[3]
        if re.search("bootanimation.zip", bootanimation_f).span() == None:
            print "bootanimation.zip file error???, " , bootanimation_f
            sys.exit(1);

        shutdownanimation_f = sys.argv[4]
        if re.search("shutdownanimation.zip", shutdownanimation_f).span() == None:
            print "shutdownanimation.zip file error???, ", shutdownanimation_f
            sys.exit(1);
        
        bootsize_in_bytes = os.path.getsize(bootanimation_f)
        shutsize_in_bytes = os.path.getsize(shutdownanimation_f)
        print "bootanimation.zip size in bytes is ", bootsize_in_bytes, " bytes, shutdownanimation.zip size in bytes is ", shutsize_in_bytes, " bytes"
        if (bootsize_in_bytes + shutsize_in_bytes + 4*1024*1024 + 2*2*32) > 20*1024*1024: 
            print "your animation.zip is too large and please add your splash size !!!"
            sys.exit(1);
        if bootsize_in_bytes == 0:
            print "Your bootanimation.zip file size is 0 byte"
            sys.exit(1);
        if shutsize_in_bytes == 0:
            print "Your shutdownanimation.zip file size is 0 byte"
            sys.exit(1);
    else:
        ShowUsage()
        sys.exit(1);

    return (bootsize_in_bytes, shutsize_in_bytes)

##  Get logo.png	
def GetPNGFile():
    num = len(sys.argv)

    if num < 2:
        ShowUsage()
        sys.exit(1); # error arg
    
    logofile = sys.argv[1]

    ## if os.access(infile, os.R_OK) != True:  default is logo.png
    if os.access(logofile, os.R_OK) != True:
        ShowUsage()
        sys.exit(1); # error file
    return (logofile)
	
##  Get logo_k.png	
def GetPNGFile_k():
    num = len(sys.argv)

    if num < 2:
        ShowUsage()
        sys.exit(1); # error arg
    
    logofile = sys.argv[2]

    ## if os.access(infile, os.R_OK) != True:  default is logo.png
    if os.access(logofile, os.R_OK) != True:
        ShowUsage()
        sys.exit(1); # error file
    return (logofile)
## make a image

def MakeSplashImage(out):
    global lk_offset
    global kernel_offset
    global bootanimation_offset
    global shutdownanimation_offset

    logo = GetPNGFile()
    img = Image.open(logo)
    CheckImage(img.mode)

    logo_k = GetPNGFile_k()
    img_k = Image.open(logo_k)
    CheckImage(img_k.mode)
	
##  check file size
    if img.size == 0:
        print "your PNG is invaild size..."
        sys.exit(1);

    boot_size, shut_size = CheckAnimation() 
    print boot_size, shut_size
    shutdownanimation_offset = bootanimation_offset + boot_size + 2*32 + 1
	
    file = open(out, "wb")
    file.write(MakeSplashIndex(boot_size, shut_size))
    file.write(GetImgHeader(img.size))
    file.write(GetImageBody(img))
	
    file.seek(kernel_offset)
    file.write(GetImgHeader(img_k.size))
    file.write(GetImageBody(img_k))
	
    file.seek(bootanimation_offset)       ## seek bootanimation.zip potision : 4M

    file.write(BootAnimationHead(boot_size))
    file.write(BootAnimationBody())
    file.write(BootAnimationEnd())

    file.seek(shutdownanimation_offset)       ## seek bootanimation.zip potision : 12M
    file.write(ShutdownAnimationHead(shut_size))
    file.write(ShutdownAnimationBody())
    file.write(ShutdownAnimationEnd())
    file.close()

## mian
if __name__ == "__main__":
    MakeSplashImage("splash.img")
