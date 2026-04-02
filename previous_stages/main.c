#include <stdio.h>
#include <stdlib.h>
#include <string.h>		//for memset
#include <fcntl.h> 		// for open
#include <unistd.h>		// for close
#include <sys/ioctl.h>		// for ioctl
#include <linux/videodev2.h>	// for V4L2
#include <sys/mman.h>
#include <stdint.h>

struct my_buffer{

	void* start;
	size_t length;
};

struct my_img_stats{

	uint64_t Y,R,G,B;
};


void process_image_bayer_to_rgb(uint16_t* raw_data, uint8_t* rgb_buffer, int width, int height, float r_gain, float b_gain){


	//uint16_t* curr;
	//uint16_t R,G,B;
	for(int i = 1; i < height-2; i+=2){
		for(int j = 1; j < width-2; j+=2){

			//The first pixel(j,i) is Blue
			uint16_t* curr_p1 = raw_data + (i * width) + j;
			int rgb_idx_p1 = ( i * width + j) * 3;

			//16bits RGB
			uint16_t B16 = (*curr_p1);
			uint16_t G16 = (*(curr_p1+1) + *(curr_p1-1) + *(curr_p1-width) + *(curr_p1+width))>>2;
			uint16_t R16 = (*(curr_p1-width-1) + *(curr_p1-width+1) + *(curr_p1+width-1) + *(curr_p1+width+1))>>2;

			//Software ISP :AWB
			R16 = (uint16_t)(R16 * r_gain);
        		B16 = (uint16_t)(B16 * b_gain);

			if(R16 >1023) R16 = 1023;
			if(B16 >1023) B16 = 1023;

			//Turn to 8-bits(RGB888)
			rgb_buffer[rgb_idx_p1+0] = R16>>2;
			rgb_buffer[rgb_idx_p1+1] = G16>>2;
			rgb_buffer[rgb_idx_p1+2] = B16>>2;

			//The second pixel(j,i) is Green Blue
			uint16_t* curr = curr_p1+1;
			int rgb_idx = rgb_idx_p1+3;

			//16bits RGB
                        G16 = *curr;
                        B16 = (*(curr+1)+*(curr-1))>>1;
                        R16 = (*(curr+width)+*(curr-width))>>1;

                        //Software ISP :AWB
                        R16 = (uint16_t)(R16 * r_gain);
                        B16 = (uint16_t)(B16 * b_gain);

                        if(R16 >1023) R16 = 1023;
                        if(B16 >1023) B16 = 1023;

                        //Turn to 8-bits(RGB888)
                        rgb_buffer[rgb_idx+0] = R16>>2;
                        rgb_buffer[rgb_idx+1] = G16>>2;
                        rgb_buffer[rgb_idx+2] = B16>>2;


                        //The third pixel(j,i) is Green Red
                        curr = curr_p1 + width;
                        rgb_idx = ((i+1) * width + j) * 3;

                        //16bits RGB
                        G16 = *curr;
                        R16 = (*(curr+1)+*(curr-1))>>1;
                        B16 = (*(curr+width)+*(curr-width))>>1;

                        //Software ISP :AWB
                        R16 = (uint16_t)(R16 * r_gain);
                        B16 = (uint16_t)(B16 * b_gain);

                        if(R16 >1023) R16 = 1023;
                        if(B16 >1023) B16 = 1023;

                        //Turn to 8-bits(RGB888)
                        rgb_buffer[rgb_idx+0] = R16>>2;
                        rgb_buffer[rgb_idx+1] = G16>>2;
                        rgb_buffer[rgb_idx+2] = B16>>2;


                        //The forth pixel(j,i) is Red
                        curr = curr_p1 + width +1;
                        rgb_idx = ((i+1) * width + (j+1)) * 3;

                        //16bits RGB
                        R16 = *curr;
                        G16 = (*(curr+1) + *(curr-1) + *(curr-width) + *(curr+width))>>2;
                        B16 = (*(curr-width-1) + *(curr-width+1) + *(curr+width-1) + *(curr+width+1))>>2;

                        //Software ISP :AWB
                        R16 = (uint16_t)(R16 * r_gain);
                        B16 = (uint16_t)(B16 * b_gain);

                        if(R16 >1023) R16 = 1023;
                        if(B16 >1023) B16 = 1023;

                        //Turn to 8-bits(RGB888)
                        rgb_buffer[rgb_idx+0] = R16>>2;
                        rgb_buffer[rgb_idx+1] = G16>>2;
                        rgb_buffer[rgb_idx+2] = B16>>2;

/*
			if(!(i&1) && !(j&1)){

				R = *curr;
				G = (*(curr+1) + *(curr-1) + *(curr-width) + *(curr+width))>>2;
				B = (*(curr-width-1) + *(curr-width+1) + *(curr+width-1) + *(curr+width+1))>>2;

			}
			else if((i&1) && (j&1)){

				B = *curr;
				G = (*(curr+1) + *(curr-1) + *(curr-width) + *(curr+width))>>2;
				R = (*(curr-width-1) + *(curr-width+1) + *(curr+width-1) + *(curr+width+1))>>2;

			}
			else if(!(i&1) && (j&1)){
				G = *curr;
				R = (*(curr+1)+*(curr-1))>>1;
				B = (*(curr+width)+*(curr-width))>>1;
			}
			else{
				G = *curr;
				B = (*(curr+1)+*(curr-1))>>1;
				R = (*(curr+width)+*(curr-width))>>1;
			}
*/


		}
	}


	//store image in ppm
	FILE* rgb_file = fopen("RGB_img.ppm","wb");
	if(!rgb_file){
		perror("Failed to open PPM file");
		return;
	}
	fprintf(rgb_file,"P6\n%d %d\n255\n",width,height);
	fwrite(rgb_buffer,sizeof(uint8_t),width*height*3,rgb_file);

	fclose(rgb_file);

}

void calculate_average_brightness(uint8_t* rgb_buffer,int width,int height, struct my_img_stats* stats){


	//calculate Y = (R+2G+B)/4
	uint64_t sum = 0, R_sum = 0, G_sum = 0, B_sum = 0;
	for(int i = 0 ; i < width * height * 3 ; i+=3){

		uint8_t R = rgb_buffer[i];
		uint8_t G = rgb_buffer[i+1];
		uint8_t B = rgb_buffer[i+2];
		sum += (R + (G<<1) + B) >> 2;

		R_sum += R;
        	G_sum += G;
        	B_sum += B;
	}

	uint64_t total_pixels = width * height;
	stats->Y = sum / total_pixels;
	stats->R = R_sum / total_pixels;
	stats->G = G_sum / total_pixels;
	stats->B = B_sum / total_pixels;

	return;

}

void update_sensor_exposure(int fd, uint64_t current_y){


	//prepare two sub-box
	struct v4l2_ext_control ctrls[2];
	memset(&ctrls,0,sizeof(ctrls));

	ctrls[0].id = V4L2_CID_EXPOSURE;
	ctrls[1].id = V4L2_CID_ANALOGUE_GAIN;

	//prepare a large box
	struct v4l2_ext_controls ext_ctrls;
	memset(&ext_ctrls,0,sizeof(ext_ctrls));

	ext_ctrls.count = 2;
	ext_ctrls.controls = ctrls;
	ext_ctrls.which = V4L2_CTRL_WHICH_CUR_VAL;

	//get current value
	if(ioctl(fd, VIDIOC_G_EXT_CTRLS, &ext_ctrls) < 0){
		perror("Get Ext Controls Failed");
		return;
	}

	int curr_exp = ctrls[0].value;
	int curr_gain = ctrls[1].value;
	int new_exp = curr_exp;
	int new_gain = curr_gain;

	//update new value
	//target: 120
	float ratio = 120.0f / current_y;

	if(current_y < 120){

		//case1: too dark add exp
		new_exp = (int)(curr_exp * ratio);

		//but exp is max
		if(new_exp > 3522){
			new_exp = 3522;
			new_gain = curr_gain+15;
			if(new_gain > 232) new_gain = 232;
		}
	}

	else if(current_y > 120){

		//case2: too light sub gain
		if(curr_gain > 0){

			new_gain = curr_gain-15;
			if(new_gain < 0) new_gain =0;
		}
		else{
			//but gain is min
			new_exp = (int)(curr_exp * ratio);
			if(new_exp < 4) new_exp = 4;
		}

	}

	ctrls[0].value = new_exp;
	ctrls[1].value = new_gain;

	if(ioctl(fd, VIDIOC_S_EXT_CTRLS, &ext_ctrls) < 0){
		perror("Set Ext Controls Failed");
		return;
	}


	printf("\t|| AE Debug - Y: %lu, Exp: %d -> %d, Gain:%d -> %d ||\n", current_y,curr_exp,new_exp,curr_gain,new_gain);


}

void reset_camera_exposure(int sensor_fd) {

	struct v4l2_ext_control ctrls[2];
	memset(ctrls, 0, sizeof(ctrls));

    	// Default gain 1600
    	ctrls[0].id = V4L2_CID_EXPOSURE;
    	ctrls[0].value = 1600;

	// Default gain 0
	ctrls[1].id = V4L2_CID_ANALOGUE_GAIN;
    	ctrls[1].value = 0;

	struct v4l2_ext_controls ext_ctrls;
	memset(&ext_ctrls, 0, sizeof(ext_ctrls));
	ext_ctrls.count = 2;
	ext_ctrls.controls = ctrls;
	ext_ctrls.which = V4L2_CTRL_WHICH_CUR_VAL;

   	if (ioctl(sensor_fd, VIDIOC_S_EXT_CTRLS, &ext_ctrls) < 0) {
        	perror("Reset Camera State Failed");
    	} else {
        	printf(">>> Camera State Reset to Default (Exp: 1600, Gain: 0) <<<\n");
    	}
}


void calculate_awb_gains(struct my_img_stats* current_stats,float* r_gain, float* b_gain){

	if(current_stats->R > 0 && current_stats->B > 0){
		float r_error_ratio = (float)current_stats->G / (float)current_stats->R;
		float b_error_ratio = (float)current_stats->G / (float)current_stats->B;

		*r_gain *= r_error_ratio;
		*b_gain *= b_error_ratio;

		if(*r_gain > 4.0f) *r_gain = 4.0f;
		if(*b_gain > 4.0f) *b_gain = 4.0f;
	}

}

int main(){

	int fd,sensor_fd;
	struct v4l2_capability cap;

	//open camera (Unicam : move data into RAM)
	fd = open("/dev/video0",O_RDWR);

	if(fd<0){
		perror("Failed to open Camera");
		return -1;
	}
	printf("Camera opened successfully, fd:%d\n",fd);

	//open camera (IMX219 : real sensor) to control exposure
	sensor_fd = open("/dev/v4l-subdev0",O_RDWR);
	if(sensor_fd < 0){
		perror("Failed to open Sensor");
		return -1;
	}

	//Handshake
	if(ioctl(fd,VIDIOC_QUERYCAP,&cap) < 0){
		perror("Failed to get device capabilities");
		close(fd);
		return -1;
	}

	//print query information
	printf("Driver Name: %s\n",cap.driver);
	printf("Card Name: %s\n",cap.card);

	//check video capture
	if(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE){
		printf("Device supports Video Capture!\n");
	}
	else{
		printf("Device does NOT support Video Capture.\n");
	}
/*
	//enum format
	struct v4l2_fmtdesc fmtdesc;
	memset(&fmtdesc,0,sizeof(fmtdesc));
	fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	printf("\nAvailable Formats:\n");

	while(ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0){

		printf("  Index %d: %s (FourCC: %c%c%c%c)\n",
			fmtdesc.index,
			fmtdesc.description,
			(fmtdesc.pixelformat >> 0) & 0xFF,
			(fmtdesc.pixelformat >> 8) & 0xFF,
			(fmtdesc.pixelformat >> 16) & 0xFF,
			(fmtdesc.pixelformat >> 24) & 0xFF
		);
		fmtdesc.index++;
	}
*/
	//set format
	struct v4l2_format  img_fmt;
	memset(&img_fmt,0,sizeof(img_fmt));
	img_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	img_fmt.fmt.pix.width =  3280;
	img_fmt.fmt.pix.height = 2464;
	img_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_SRGGB10;
	img_fmt.fmt.pix.field = V4L2_FIELD_NONE;

	if(ioctl(fd, VIDIOC_S_FMT, &img_fmt) < 0){
		perror("Setting Pixel Format");
		return -1;
	}

	//Check
	printf("Selected Camera Mode:\n");
	printf("\tWidth: %d\n", img_fmt.fmt.pix.width);
	printf("\tHeight: %d\n", img_fmt.fmt.pix.height);
	printf("\tBytePerLine: %d\n", img_fmt.fmt.pix.bytesperline);
	printf("\tSizeImage: %d\n", img_fmt.fmt.pix.sizeimage);


	//Step2: request buffer from  Kernel

	struct v4l2_requestbuffers bufs;

	memset(&bufs,0,sizeof(bufs));
	bufs.count = 4;
	bufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	bufs.memory = V4L2_MEMORY_MMAP;

	if(ioctl(fd, VIDIOC_REQBUFS, &bufs) < 0){

		perror("Buffer Request Failed.\n");
		return -1;
	}

	if(bufs.count < 2){

		perror("Buffer Not Enough.\n");
		return -1;
	}
	printf("Numbers of Buffer Get: %d\n",bufs.count);

	//get buffer address
	struct my_buffer* my_buffers = calloc(4,sizeof(struct my_buffer));

	for(int i = 0 ; i < 4; i++){

		struct v4l2_buffer buf;
        	memset(&buf,0,sizeof(buf));
	        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        	buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;

		//1.query kernel about the buffer
		if(ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0){
			perror("Buffer Query Failed.\n");
			free(my_buffers);
			return -1;
		}

		//2.bulid bridge between Kernel Space and User Space
		// Because we always be allowed to touch User Space Memory (Virtual)
		my_buffers[i].length = buf.length;
		my_buffers[i].start = mmap(
			NULL,
			buf.length,
			PROT_READ | PROT_WRITE,
			MAP_SHARED,
			fd,
			buf.m.offset
		);

		//check if mmap successful or not
		if(my_buffers[i].start == MAP_FAILED){
			perror("mmap Failed");
			return -1;
		}

		printf("Buffer %d mapped at address %p, length %ld\n",
				i,my_buffers[i].start,my_buffers[i].length);
	}


        //3. Put Buffer into Incoming queue

	printf("--- Queueing Buffers ---\n");

	for(int i = 0 ; i < 4; i++){

		struct v4l2_buffer buf;
		memset(&buf, 0, sizeof(buf));

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	        buf.memory = V4L2_MEMORY_MMAP;
        	buf.index = i;
/*
		if(ioctl(fd,VIDIOC_QUERYBUF,&buf) < 0){
			perror("QBUF Query failed.");
			return -1;
		}
*/
                if(ioctl(fd, VIDIOC_QBUF, &buf) < 0){
                        perror("Queue Buffer Failed");
                        return -1;
                }
                printf("Buffer %d queued to driver.\n",i);

	}

	printf("All buffers are queued and ready.\n");

	//4.stream on camera
	reset_camera_exposure(sensor_fd);

	int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	printf("Debug: StreamOn Type = %d\n", type);

	if(ioctl(fd, VIDIOC_STREAMON, &type) < 0){
        	perror("Stream On Failed");
	        return -1;
    	}

	printf("Camera Stream ON! (Recording for 2 seconds...)\n");

//	FILE* raw_file = NULL;

	//sleep(2);
	//capture 30 picture
        struct v4l2_buffer buf;

	int width = img_fmt.fmt.pix.width;
	int height = img_fmt.fmt.pix.height;
	uint8_t* rgb_buffer = calloc(width * height * 3 , sizeof(uint8_t));

	float r_gain = 1.0f;
	float b_gain = 1.0f;

	for(int i=0;i < 100;i++){

		//dequeue buffer
                memset(&buf,0,sizeof(buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;

		if(ioctl(fd, VIDIOC_DQBUF, &buf) < 0){
			perror("Dequeue buffer failed.");
			continue;
		}
/*
		//store first pic
		if(i == 0){
			printf(">>>Capturing Frame 0 to file 'image row'...\n");

			//wb = Write Binary
			raw_file = fopen("image.raw","wb");

			if(raw_file){
				//fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
				fwrite(my_buffers[buf.index].start,buf.bytesused,1,raw_file);
				fclose(raw_file);
				printf(">>> Save Successfully! (Size: %d bytes)\n", buf.bytesused);
			}
			else{
				perror("Failed to open file for writing");
			}
		}
*/
		//process pic
		uint16_t* data = (uint16_t*)my_buffers[buf.index].start;
		process_image_bayer_to_rgb(data,rgb_buffer,width,height,r_gain,b_gain);

		//Brightness Statistics
		struct my_img_stats current_stats;
		memset(&current_stats,0,sizeof(current_stats));

		calculate_average_brightness(rgb_buffer,width,height,&current_stats);

		//printf("Frame [%02d], Seq: [%02d], Current Brigthness: [%d].\n",i,buf.sequence,current_y);
		printf("Frame [%02d], Y: %lu, R: %lu, G: %lu, B: %lu\n", i, current_stats.Y, current_stats.R, current_stats.G, current_stats.B);

		//Update AE exposure
		update_sensor_exposure(sensor_fd, current_stats.Y);

		//Update AWB
		calculate_awb_gains(&current_stats,&r_gain,&b_gain);

		//queue buffer
		if(ioctl(fd, VIDIOC_QBUF, &buf) < 0){
			perror("Queue buffer failed.");
		}
	}

	free(rgb_buffer);


	if(ioctl(fd, VIDIOC_STREAMOFF, &type) < 0){
        	perror("Stream Off Failed");
		return -1;
    	}
    	printf("Camera Stream OFF.\n");


	close(fd);


	return 0;

}

