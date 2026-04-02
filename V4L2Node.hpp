#ifndef V4L2NODE_HPP
#define V4L2NODE_HPP

#include <linux/videodev2.h>
#include <cstdint>
#include <cstddef>

struct my_buffer {
    void* start;
    size_t length;
    int export_fd;
};

class V4L2Node{

private:

	int fd;
	enum v4l2_buf_type type;
	enum v4l2_memory memory_type;
	int stride;


	int buffer_count;
	struct my_buffer* buffers;


public:

	V4L2Node(const char* dev_name, enum v4l2_buf_type type);
	~V4L2Node();

	int setFormat(uint32_t pixelformat, int width, int height);
	int setMetaFormat(uint32_t dataformat);

	int requestAndMapBuffers(int count);
	int requestAndExportBuffers(int count);
	int requestDmaBuffers(int count);

	int queueBuffer(int index, uint32_t bytesused = 0, int dma_fd = -1);
	int dequeueBuffer(struct v4l2_buffer* buf);

	int setControl(uint32_t control_id, int value);
	int setExtControls(struct v4l2_ext_control* ctrls, int count);

	int streamOn(void);
	int streamOff(void);

	int getFd(void) const{ return fd; }
	struct my_buffer* getBuffer(int index) const {return &buffers[index];}
	int getStride(void) const{ return stride;}

	void printSupportedControls(void);

};

#endif //V4L2NODE_HPP
