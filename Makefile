#-------C++----------------------
CXX = g++
CXXFLAGS = -Wall -g -std=c++14 -pthread

TARGET = multi_app

all: $(TARGET)

$(TARGET): main_isp_dma_multi.cpp V4L2Node.cpp V4L2Node.hpp SharedMetaQueue.hpp
	$(CXX) $(CXXFLAGS) -o $(TARGET4) main_isp_dma_multi.cpp V4L2Node.cpp

clean:
	rm -f $(TARGET)
