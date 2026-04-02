#include "V4L2Node.hpp"
#include <stdio.h>	// for printf perror
#include <stdlib.h>	// for malloc free
#include <string.h> 	// for memset
#include <fcntl.h>	// for open
#include <unistd.h>	// for read write
#include <sys/mman.h> 	// for munmap
#include <sys/ioctl.h>	// for ioctl
#include <iostream>
#include <iomanip>


V4L2Node::V4L2Node(const char* dev_name, enum v4l2_buf_type node_type){


	this->fd = open(dev_name,O_RDWR);

        if(this->fd<0){
                perror("Failed to open device");
		return;
        }
        printf(">>> [Constructor] %s opened successfully, fd:%d\n", dev_name, this->fd);

	this->type = node_type;
	this->memory_type = V4L2_MEMORY_MMAP;
	this->buffer_count = 0;
	this->buffers = nullptr;

}


V4L2Node::~V4L2Node(){

	printf("<<< [Destructor] Cleaning up fd:%d...\n", this->fd);

	if(this->buffers != nullptr){

		if(this->memory_type == V4L2_MEMORY_MMAP){

			for(int i = 0; i < this->buffer_count; i++){

				if(this->buffers[i].start != nullptr && this->buffers[i].start != MAP_FAILED){

					munmap(this->buffers[i].start,this->buffers[i].length);
				}
			}
		}

		delete[] this->buffers;
		this->buffers = nullptr;
	}

	if(this->fd >= 0){

		close(this->fd);
		printf("<<< [Destructor] Device fd:%d closed.\n",this->fd);
	}
}



int V4L2Node::setFormat(uint32_t pixelformat, int width, int height){

	struct v4l2_format  fmt;
        memset(&fmt,0,sizeof(fmt));

        fmt.type = this->type;
        fmt.fmt.pix.width =  width;
        fmt.fmt.pix.height = height;
        fmt.fmt.pix.pixelformat = pixelformat;
        fmt.fmt.pix.field = V4L2_FIELD_NONE;

        if(ioctl(this->fd, VIDIOC_S_FMT, &fmt) < 0){
                perror("Setting Pixel Format Failed");
                return -1;
        }

	this->stride = fmt.fmt.pix.bytesperline;

	printf("fd:%d SizeImage: %d\n",this->fd,fmt.fmt.pix.sizeimage);
	return 0;
}

int V4L2Node::setMetaFormat(uint32_t dataformat){

        struct v4l2_format  fmt;
        memset(&fmt,0,sizeof(fmt));

        fmt.type = this->type;
        fmt.fmt.meta.dataformat = dataformat;

        if(ioctl(this->fd, VIDIOC_S_FMT, &fmt) < 0){
                perror("Setting Meta Format Failed");
                return -1;
        }

        printf("fd:%d Meta SizeImage: %d\n",this->fd,fmt.fmt.meta.buffersize);
        return 0;

}

int V4L2Node::requestAndMapBuffers(int count){

	struct v4l2_requestbuffers bufs;
	memset(&bufs,0,sizeof(bufs));

	bufs.count = count;
	bufs.type = this->type;
	bufs.memory = V4L2_MEMORY_MMAP;

	if(ioctl(this->fd, VIDIOC_REQBUFS, &bufs) < 0){

		perror("Buffer Request Failed.\n");
		return -1;
	}

	if(bufs.count < 2){

		perror("Buffer Not Enough.\n");
		return -1;
	}

	this->memory_type = static_cast<enum v4l2_memory>(bufs.memory);
	this->buffer_count = bufs.count;

	//get buffer address
	this->buffers = new struct my_buffer[count]();

	for(int i = 0 ; i < count; i++){

		struct v4l2_buffer buf;
        	memset(&buf,0,sizeof(buf));
	        buf.type = this->type;
        	buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;

		//1.query kernel about the buffer
		if(ioctl(this->fd, VIDIOC_QUERYBUF, &buf) < 0){
			perror("Camera Buffer Query Failed.\n");
			return -1;
		}

		//2.bulid bridge between Kernel Space and User Space
		// Because we always be allowed to touch User Space Memory (Virtual)
		this->buffers[i].length = buf.length;
		this->buffers[i].start = mmap(
			NULL,
			buf.length,
			PROT_READ | PROT_WRITE,
			MAP_SHARED,
			this->fd,
			buf.m.offset
		);

		//check if mmap successful or not
		if(this->buffers[i].start == MAP_FAILED){
			perror("Camera mmap Failed");
			return -1;
		}
	}

	printf("Numbers of Buffer fd[%d] Get and Map: %d\n",this->fd,bufs.count);


	return 0;
}



int V4L2Node::requestAndExportBuffers(int count){


 	struct v4l2_requestbuffers bufs;
        memset(&bufs,0,sizeof(bufs));
        bufs.count = count;
        bufs.type = this->type;
        bufs.memory = V4L2_MEMORY_MMAP;

        if(ioctl(this->fd, VIDIOC_REQBUFS, &bufs) < 0){

                perror("Buffer Request Failed.\n");
                return -1;
        }

        if(bufs.count < 2){

                perror("Buffer Not Enough.\n");
                return -1;
        }

	this->memory_type = static_cast<enum v4l2_memory>(bufs.memory);
        this->buffer_count = bufs.count;


        //get buffer address
        this->buffers = new struct my_buffer[count]();

        for(int i = 0 ; i < count; i++){

		struct v4l2_exportbuffer expbuf;
		memset(&expbuf,0,sizeof(expbuf));
		expbuf.type = this->type;
		expbuf.index = i;
		expbuf.flags =  O_CLOEXEC;

		if(ioctl(this->fd, VIDIOC_EXPBUF, &expbuf) < 0){
			perror("Failed to export a buffer as a DMABUF");
			return -1;
		}

		this->buffers[i].export_fd = expbuf.fd;
	}

        printf("Numbers of Buffer fd[%d] Get and Export: %d\n",this->fd,bufs.count);


	return 0;

}

int V4L2Node::requestDmaBuffers(int count){

	struct v4l2_requestbuffers bufs;
        memset(&bufs,0,sizeof(bufs));
        bufs.count = count;
        bufs.type = this->type;
        bufs.memory = V4L2_MEMORY_DMABUF;

        if(ioctl(this->fd, VIDIOC_REQBUFS, &bufs) < 0){

                perror("Buffer Request Failed.\n");
                return -1;
        }

        this->memory_type = static_cast<enum v4l2_memory>(bufs.memory);
        this->buffer_count = bufs.count;

	printf("Numbers of DMA Buffer fd[%d] Get: %d\n",this->fd,bufs.count);

	return 0;
}

int V4L2Node::queueBuffer(int index, uint32_t bytesused, int dma_fd){

        struct v4l2_buffer buf;
	memset(&buf,0,sizeof(buf));
        buf.type = this->type;
        buf.memory = this->memory_type;
	buf.index = index;
	buf.bytesused = bytesused;

	if(this->memory_type == V4L2_MEMORY_DMABUF){

		if(dma_fd < 0){
			printf("Error: Node is DMABUF, but valid dma_fd was not provided!\n");
			return -1;
		}
		buf.m.fd = dma_fd;
	}

        if(ioctl(this->fd, VIDIOC_QBUF, &buf) < 0){
                perror("Queue buffer failed.");
                return -1;
        }

        return 0;
}

int V4L2Node::dequeueBuffer(struct v4l2_buffer* buf){

	memset(buf,0,sizeof(*buf));
	buf->type = this->type;
	buf->memory = this->memory_type;

	if(ioctl(this->fd, VIDIOC_DQBUF, buf) < 0){
		perror("Dequeue buffer failed.");
		return -1;
	}
	return 0;
}

int V4L2Node::streamOn(void){

	if(ioctl(this->fd, VIDIOC_STREAMON, &this->type) < 0){
        	perror("Stream On Failed");
	        return -1;
	}
	return 0;
}

int V4L2Node::streamOff(void){

	if(ioctl(this->fd, VIDIOC_STREAMOFF, &this->type) < 0){
        	perror("Stream Off Failed");
		return -1;
    	}
	return 0;
}


int V4L2Node::setControl(uint32_t control_id, int value){

	struct v4l2_control ctrl;
	memset(&ctrl,0,sizeof(ctrl));

	ctrl.id = control_id;
	ctrl.value = value;

	if(ioctl(this->fd, VIDIOC_S_CTRL, &ctrl) < 0){

		perror("Failed to set control");
		return -1;
	}

	//printf("ISP Control [0x%08x] set to: %d\n",control_id,value);
	return 0;
}

int V4L2Node::setExtControls(struct v4l2_ext_control* ctrls, int count){

	struct v4l2_ext_controls ext_ctrls;
	memset(&ext_ctrls, 0, sizeof(ext_ctrls));

	ext_ctrls.count = count;
	ext_ctrls.controls = ctrls;
	ext_ctrls.which = V4L2_CTRL_WHICH_CUR_VAL;

   	if (ioctl(this->fd, VIDIOC_S_EXT_CTRLS, &ext_ctrls) < 0) {
        	perror("Failed to set ext controls");
		return -1;
    	}
	return 0;
}


void V4L2Node::printSupportedControls(){

	struct v4l2_queryctrl queryctrl;
	memset(&queryctrl, 0, sizeof(queryctrl));

	queryctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;

	std::cout << "\n=== Sensor fd:[" << this->fd << "] Support Control List ===" << std::endl;

	while(0 == ioctl(fd, VIDIOC_QUERYCTRL, &queryctrl)){

		if (!(queryctrl.flags & V4L2_CTRL_FLAG_DISABLED)) {
            		std::cout << "- [" << queryctrl.name << "]" 
                      	<< " | ID: 0x" << std::hex << queryctrl.id << std::dec // 印出十六進位 ID
                      	<< " | Range: " << queryctrl.minimum << " ~ " << queryctrl.maximum 
                      	<< " | Default: " << queryctrl.default_value 
                      	<< " | Step: " << queryctrl.step << std::endl;
        	}

		queryctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
	}

	std::cout << "========================================================\n" << std::endl;
}
