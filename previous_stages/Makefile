#--------C Langueage-------------
CC = gcc
CFLAGS = -Wall -g

#-------C++----------------------
CXX = g++
CXXFLAGS = -Wall -g -std=c++14 -pthread

TARGET1 = cam_app
TARGET2 = isp_app
TARGET3 = dma_app
TARGET4 = multi_app

all: $(TARGET1) $(TARGET2) $(TARGET3) $(TARGET4)

$(TARGET1): main.c
	$(CC) $(CFLAGS) -o $(TARGET1) main.c

$(TARGET2): main_isp.c
	$(CC) $(CFLAGS) -o $(TARGET2) main_isp.c

$(TARGET3): main_isp_dma.c
	$(CC) $(CFLAGS) -o $(TARGET3) main_isp_dma.c

$(TARGET4): main_isp_dma_multi.cpp V4L2Node.cpp V4L2Node.hpp SharedMetaQueue.hpp
	$(CXX) $(CXXFLAGS) -o $(TARGET4) main_isp_dma_multi.cpp V4L2Node.cpp

clean:
	rm -f $(TARGET1) $(TARGET2) $(TARGET3) $(TARGET4)
