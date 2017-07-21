import os

NAME = 'avahi'
CFLAGS = os.popen('pkg-config --cflags avahi-client').read().rstrip().split()
LIBS = os.popen('pkg-config --libs avahi-client').read().rstrip().split()
GCC_LIST = ['avahi']
