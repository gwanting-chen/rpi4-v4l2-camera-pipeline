#include <stdio.h>
#include <stdlib.h>
#include <string.h>		//for memset
#include <fcntl.h> 		// for open
#include <unistd.h>		// for close
#include <sys/ioctl.h>		// for ioctl
#include <linux/videodev2.h>	// for V4L2
#include <sys/mman.h>
#include <stdint.h>
#include <linux/bcm2835-isp.h>
#include <time.h>

struct my_buffer{

	void* start;
	size_t length;
	int export_fd;
};



int open_and_set_format(const char* dev_name, enum v4l2_buf_type type, uint32_t pixelformat, int width, int height){

        //open camera
        int fd = open(dev_name,O_RDWR);

        if(fd<0){
                perror("Failed to open device");
                return -1;
        }
        printf("%s opened successfully, fd:%d\n",dev_name,fd);

        //set format
        struct v4l2_format  fmt;
        memset(&fmt,0,sizeof(fmt));
        fmt.type = type;
        fmt.fmt.pix.width =  width;
        fmt.fmt.pix.height = height;
        fmt.fmt.pix.pixelformat = pixelformat;
        fmt.fmt.pix.field = V4L2_FIELD_NONE;

        if(ioctl(fd, VIDIOC_S_FMT, &fmt) < 0){
                perror("Setting Pixel Format Failed");
                return -1;
        }

	printf("%s SizeImage: %d\n",dev_name,fmt.fmt.pix.sizeimage);
	return fd;

}

int open_and_set_meta_format(const char* dev_name, enum v4l2_buf_type type, uint32_t dataformat) {

        //open camera
        int fd = open(dev_name,O_RDWR);

        if(fd<0){
                perror("Failed to open meta device");
                return -1;
        }
        printf("%s (Meta) opened successfully, fd:%d\n",dev_name,fd);

        //set format
        struct v4l2_format  fmt;
        memset(&fmt,0,sizeof(fmt));
        fmt.type = type;
        fmt.fmt.meta.dataformat = dataformat;

        if(ioctl(fd, VIDIOC_S_FMT, &fmt) < 0){
                perror("Setting Meta Format Failed");
                return -1;
        }

        printf("%s Meta SizeImage: %d\n",dev_name,fmt.fmt.meta.buffersize);
        return fd;

}

struct my_buffer* request_and_map_buffers(int fd, int count, enum v4l2_buf_type type) {


	struct v4l2_requestbuffers bufs;
	memset(&bufs,0,sizeof(bufs));
	bufs.count = count;
	bufs.type = type;
	bufs.memory = V4L2_MEMORY_MMAP;

	if(ioctl(fd, VIDIOC_REQBUFS, &bufs) < 0){

		perror("Buffer Request Failed.\n");
		return NULL;
	}

	if(bufs.count < 2){

		perror("Buffer Not Enough.\n");
		return NULL;
	}
	printf("Numbers of Buffer fd[%d] Get: %d\n",fd,bufs.count);

	//get buffer address
	struct my_buffer* my_buffers = calloc(count,sizeof(struct my_buffer));

	for(int i = 0 ; i < count; i++){

		struct v4l2_buffer buf;
        	memset(&buf,0,sizeof(buf));
	        buf.type = type;
        	buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;

		//1.query kernel about the buffer
		if(ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0){
			perror("Camera Buffer Query Failed.\n");
			return NULL;
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
			perror("Camera mmap Failed");
			return NULL;
		}
	}

	return my_buffers;
}

struct my_buffer* request_and_export_buffers(int fd, int count, enum v4l2_buf_type type){

        struct v4l2_requestbuffers bufs;
        memset(&bufs,0,sizeof(bufs));
        bufs.count = count;
        bufs.type = type;
        bufs.memory = V4L2_MEMORY_MMAP;

        if(ioctl(fd, VIDIOC_REQBUFS, &bufs) < 0){

                perror("Buffer Request Failed.\n");
                return NULL;
        }

        if(bufs.count < 2){

                perror("Buffer Not Enough.\n");
                return NULL;
        }

        //get buffer address
        struct my_buffer* my_buffers = calloc(count,sizeof(struct my_buffer));

        for(int i = 0 ; i < count; i++){

		struct v4l2_exportbuffer expbuf;
		memset(&expbuf,0,sizeof(expbuf));
		expbuf.type = type;
		expbuf.index = i;
		expbuf.flags =  O_CLOEXEC;

		if(ioctl(fd, VIDIOC_EXPBUF, &expbuf) < 0){
			perror("Failed to export a buffer as a DMABUF");
			return NULL;
		}

		my_buffers[i].export_fd = expbuf.fd;
	}

        printf("Numbers of DMA Buffer fd[%d] Get: %d\n",fd,bufs.count);

	return my_buffers;
}


int request_dmabuf_buffers(int fd, int count, enum v4l2_buf_type type){

        struct v4l2_requestbuffers bufs;
        memset(&bufs,0,sizeof(bufs));
        bufs.count = count;
        bufs.type = type;
        bufs.memory = V4L2_MEMORY_DMABUF;

        if(ioctl(fd, VIDIOC_REQBUFS, &bufs) < 0){

                perror("Buffer Request Failed.\n");
                return -1;
        }

	printf("Numbers of DMA Buffer fd[%d] Get: %d\n",fd,bufs.count);

	return bufs.count;

}

int queue_all_empty_buffers(int fd, int count, enum v4l2_buf_type type){

	printf("--- fd[%d] Queueing Buffers ---\n",fd);

	for(int i = 0 ; i < count; i++){

		struct v4l2_buffer buf;
		memset(&buf, 0, sizeof(buf));

		buf.type = type;
	        buf.memory = V4L2_MEMORY_MMAP;
        	buf.index = i;

                if(ioctl(fd, VIDIOC_QBUF, &buf) < 0){
                        perror("Queue Buffer Failed");
                        return -1;
                }
                printf("fd[%d] : Buffer %d queued to driver.\n",fd,i);

	}

	printf("fd[%d] : All buffers are queued and ready.\n",fd);

	return 0;
}

int queue_buffers(int fd, struct v4l2_buffer* buf, enum v4l2_buf_type type,int idx,uint32_t bytesused){

        memset(buf,0,sizeof(*buf));
        buf->type = type;
        buf->memory = V4L2_MEMORY_MMAP;
	buf->index = idx;
	buf->bytesused = bytesused;

        if(ioctl(fd, VIDIOC_QBUF, buf) < 0){
                perror("Queue buffer failed.");
                return -1;
        }
        return 0;
}

int queue_dmabuf_buffer(int fd, struct v4l2_buffer* buf, enum v4l2_buf_type type, int idx, uint32_t bytesused, int dma_fd){

	memset(buf,0,sizeof(*buf));
	buf->type = type;
	buf->memory = V4L2_MEMORY_DMABUF;
	buf->index = idx;
	buf->bytesused = bytesused;
	buf->m.fd = dma_fd;

        if(ioctl(fd, VIDIOC_QBUF, buf) < 0){
                perror("Queue buffer failed.");
                return -1;
        }
	return -1;
}

int dequeue_buffers(int fd, struct v4l2_buffer* buf, enum v4l2_buf_type type){


        memset(buf,0,sizeof(*buf));
	buf->type = type;
	buf->memory = V4L2_MEMORY_MMAP;

	if(ioctl(fd, VIDIOC_DQBUF, buf) < 0){
		perror("Dequeue buffer failed.");
		return -1;
	}
	return 0;
}


int dequeue_dma_buffers(int fd, struct v4l2_buffer* buf, enum v4l2_buf_type type){


        memset(buf,0,sizeof(*buf));
        buf->type = type;
        buf->memory = V4L2_MEMORY_DMABUF;

        if(ioctl(fd, VIDIOC_DQBUF, buf) < 0){
                perror("Dequeue buffer failed.");
                return -1;
        }
        return 0;
}


int set_isp_control(int fd, uint32_t control_id, int value) {

	struct v4l2_control ctrl;
	memset(&ctrl,0,sizeof(ctrl));
	ctrl.id = control_id;
	ctrl.value = value;

	if(ioctl(fd, VIDIOC_S_CTRL, &ctrl) < 0){

		perror("Failed to set control");
		return -1;
	}

	printf("ISP Control [0x%08x] set to: %d\n",control_id,value);
	return -1;
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
	//target:8bits 120
	float target_y = 1000.0f;
	float ratio = target_y / current_y;

	if(current_y < target_y){

		//case1: too dark add exp
		new_exp = (int)(curr_exp * ratio);

		//but exp is max
		if(new_exp > 3522){
			new_exp = 3522;
			new_gain = curr_gain+15;
			if(new_gain > 232) new_gain = 232;
		}
	}

	else if(current_y > target_y){

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



////////////////////////thermal///////////////////////////////////


enum thermal_state_type{

	STATE_NORMAL,
	STATE_THROTTLED
};

struct ThermalPolicy{

	enum thermal_state_type current_state;
	float trigger_temp;
	float recovery_temp;
	int normal_fps;
	int throttled_fps;
};

/*
float get_soc_temperature_C(void){

	int raw_temp = 0;

	FILE* fp =fopen("/sys/class/thermal/thermal_zone0/temp","r");

	if(!fp){
		perror("Failed to open thermal");
		return -1.0f;
	}

	if(fscanf(fp, "%d", &raw_temp) != 1){
		printf("thermal format is wrong");
		fclose(fp);
		 return -1.0f;
	}

	fclose(fp);

	return (float)raw_temp/1000.0f;
}
*/


float get_soc_temperature(void){

	char buf[16];

        int fd = open("/sys/class/thermal/thermal_zone0/temp",O_RDONLY);
	if(fd < 0){
		perror("Failed to open thermal");
		return -1.0f;
	}

	ssize_t bytes_read = read(fd,buf,sizeof(buf)-1);

	close(fd);

	if(bytes_read <= 0){
		printf("Not any byte be read");
		return -1.0f;
	}

	buf[bytes_read] = '\0';

	return (float)atoi(buf)/1000.0f;


}


void update_thermal_state(float current_temp, struct ThermalPolicy* policy){

	if(current_temp > policy->trigger_temp){

		//overheat
		policy->current_state = STATE_THROTTLED;

	}

	else if(current_temp < policy->recovery_temp){

		policy->current_state = STATE_NORMAL;
	}
}


int set_camera_fps(int fd, int fps){

	int vblank_val;

	if (fps >= 15) {
        	vblank_val = 40;
    	}

	else if (fps == 5) {
	        vblank_val = 6500;
    	}

	else {
        	vblank_val = 40;
    	}


	struct v4l2_control ctrl;
	memset(&ctrl, 0, sizeof(ctrl));
	ctrl.id = V4L2_CID_VBLANK;
	ctrl.value = vblank_val;

	if(ioctl(fd, VIDIOC_S_CTRL, &ctrl) < 0){
		perror("Set VBLANK Failed");
		return -1;
	}

	printf("\n>>> [Thermal Policy] Camera FPS changed to %d (VBLANK: %d) <<<\n\n", fps, vblank_val);
	return 0;
}


int main(){

	int width = 3280;
	int height = 2464;

	//Step1: open device and set format
	int cam_fd = open_and_set_format("/dev/video0",V4L2_BUF_TYPE_VIDEO_CAPTURE,V4L2_PIX_FMT_SRGGB10,width, height);
	int isp_out_fd = open_and_set_format("/dev/video13",V4L2_BUF_TYPE_VIDEO_OUTPUT,V4L2_PIX_FMT_SRGGB10,width, height);
	int isp_cap_fd = open_and_set_format("/dev/video14",V4L2_BUF_TYPE_VIDEO_CAPTURE,V4L2_PIX_FMT_RGB24,width, height);
	int isp_meta_fd = open_and_set_meta_format("/dev/video16", V4L2_BUF_TYPE_META_CAPTURE,V4L2_META_FMT_BCM2835_ISP_STATS);

	//open sensor to control AE and gain
	int sensor_fd = open("/dev/v4l-subdev0",O_RDWR);
	if(sensor_fd < 0){

		perror("Failed to open Sensor");
		return -1;
	}
	printf("Sensor opened successfully, fd:%d\n",sensor_fd);

	reset_camera_exposure(sensor_fd);

	//Step2: request buffer from  Kernel
	struct my_buffer* cam_bufs = request_and_export_buffers(cam_fd, 4, V4L2_BUF_TYPE_VIDEO_CAPTURE);
	request_dmabuf_buffers(isp_out_fd, 4, V4L2_BUF_TYPE_VIDEO_OUTPUT);
	struct my_buffer* isp_cap_bufs = request_and_map_buffers(isp_cap_fd, 4, V4L2_BUF_TYPE_VIDEO_CAPTURE);
	struct my_buffer* isp_mata_bufs = request_and_map_buffers(isp_meta_fd, 4,  V4L2_BUF_TYPE_META_CAPTURE);

        //Step3: Put Buffer into Incoming queue
	queue_all_empty_buffers(cam_fd, 4, V4L2_BUF_TYPE_VIDEO_CAPTURE);
	queue_all_empty_buffers(isp_cap_fd, 4, V4L2_BUF_TYPE_VIDEO_CAPTURE);
	queue_all_empty_buffers(isp_meta_fd, 4, V4L2_BUF_TYPE_META_CAPTURE);


	//adjust wb
//	int red_gain = 1000;
//	int blue_gain = 1000;
/*
	set_isp_control(isp_out_fd,V4L2_CID_RED_BALANCE,red_gain);
	set_isp_control(isp_out_fd,V4L2_CID_BLUE_BALANCE,blue_gain);
*/

	//Step4: stream on camera

	enum v4l2_buf_type type_cam = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	enum v4l2_buf_type type_isp_out = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	enum v4l2_buf_type type_isp_cap = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	enum v4l2_buf_type type_isp_meta =  V4L2_BUF_TYPE_META_CAPTURE;

	if(ioctl(cam_fd, VIDIOC_STREAMON, &type_cam) < 0){
        	perror("Camera Stream On Failed");
	        return -1;
	}
	if(ioctl(isp_out_fd, VIDIOC_STREAMON, &type_isp_out) < 0){
        	perror("ISP Out Stream On Failed");
	        return -1;
    	}

	if(ioctl(isp_cap_fd, VIDIOC_STREAMON, &type_isp_cap) < 0){
        	perror("ISP Cap Stream On Failed");
	        return -1;
    	}

	if(ioctl(isp_meta_fd, VIDIOC_STREAMON, &type_isp_meta) < 0){
        	perror("ISP Meta Stream On Failed");
	        return -1;
    	}

	printf("\n>>> All Factories Motor ON! <<<\n\n");

	//set thermal policy
	struct ThermalPolicy policy;

	policy.current_state = STATE_NORMAL;
	policy.trigger_temp = 75.0f;
	policy.recovery_temp = 70.0f;
	policy.normal_fps = 15;
	policy.throttled_fps = 5;

	enum thermal_state_type prev_state = policy.current_state;

	set_camera_fps(sensor_fd, policy.current_state);

	struct timespec start_time, end_time;
	//capture 30 picture
	int last = 500;

	clock_gettime(CLOCK_MONOTONIC, &start_time);

	for(int i=0;i < last;i++){

		//1.Dequeue Camera buffer
	        struct v4l2_buffer cam_buf;
		if(dequeue_buffers(cam_fd, &cam_buf, type_cam) < 0) continue;
		int curr_seq = cam_buf.sequence;

		//2.copy data from camera to ISP output
		int out_idx = i&3; //i%4
//		memcpy(isp_out_bufs[out_idx].start, cam_bufs[cam_buf.index].start,cam_buf.length);

		//3.Queue to ISP Output
		struct v4l2_buffer isp_out_buf;
		int current_dma_fd = cam_bufs[cam_buf.index].export_fd;
		queue_dmabuf_buffer(isp_out_fd, &isp_out_buf, type_isp_out, out_idx,cam_buf.bytesused, current_dma_fd);

		//4.Dequeue From ISP Capture
		struct v4l2_buffer isp_cap_buf;
		dequeue_buffers(isp_cap_fd, &isp_cap_buf, type_isp_cap);

		//5.Dequeue From ISP Meta data
		struct v4l2_buffer isp_meta_buf;
		dequeue_buffers(isp_meta_fd, &isp_meta_buf, type_isp_meta);

		struct bcm2835_isp_stats *stats = isp_mata_bufs[isp_meta_buf.index].start;


		//AE contrl
		uint64_t total_ae_r = 0, total_ae_g = 0, total_ae_b = 0;
                uint64_t total_ae_counted = 0;

		for(int j = 0; j < AGC_REGIONS; j++){

			if(stats->agc_stats[j].counted > 0){

				total_ae_r += stats->agc_stats[j].r_sum;
                                total_ae_g += stats->agc_stats[j].g_sum;
                                total_ae_b += stats->agc_stats[j].b_sum;
                                total_ae_counted += stats->agc_stats[j].counted;
			}
		}

		if(total_ae_counted > 0){

			uint64_t avg_r = total_ae_r / total_ae_counted;
			uint64_t avg_g = total_ae_g / total_ae_counted;
                        uint64_t avg_b = total_ae_b / total_ae_counted;


			//calculate Y = (R + 2G + B) / 4
                        uint64_t current_y = (avg_r + (avg_g) + avg_b) >> 2;


			printf("\t>> RAW AVG -> R:%lu, G:%lu, B:%lu, Computed Y:%lu\n", avg_r, avg_g, avg_b, current_y);

			update_sensor_exposure(sensor_fd, current_y);
		}


		//awb control
		uint64_t total_r = 0;
		uint64_t total_g = 0;
		uint64_t total_b = 0;

		for(int j = 0 ; j < AWB_REGIONS;j++){

			if(stats->awb_stats[j].counted > 0){

				total_r += stats->awb_stats[j].r_sum;
				total_g += stats->awb_stats[j].g_sum;
				total_b += stats->awb_stats[j].b_sum;
			}
		}


		printf("\tAWB R : %lu, G : %lu, B : %lu\n",total_r,total_g,total_b);
		//set_isp_control(isp_out_fd, 0x009f0905, 8000);

		int red_gain = 1000;
                int blue_gain = 1000;

		if(total_r > 0) red_gain = (int)((total_g * red_gain) / total_r);
		if(total_b > 0) blue_gain = (int)((total_g * blue_gain) / total_b);

		if(red_gain > 65535) red_gain = 65535;
                if(blue_gain > 65535) blue_gain = 65535;

		if(red_gain < 1) red_gain = 1;
                if(blue_gain < 1) blue_gain = 1;

		set_isp_control(isp_out_fd,V4L2_CID_RED_BALANCE,red_gain);
		set_isp_control(isp_out_fd,V4L2_CID_BLUE_BALANCE,blue_gain);


		//6.store image in ppm
		if(i == last-1){

		        //store image in ppm

			int valid_row_bytes = width * 3; //9840  not 9888

			//query bytesperlines
			struct v4l2_format query_fmt;
			memset(&query_fmt, 0 , sizeof(query_fmt));
			query_fmt.type = type_isp_cap;
			ioctl(isp_cap_fd,VIDIOC_G_FMT, &query_fmt);
			int isp_stride = query_fmt.fmt.pix.bytesperline;//9888

			FILE* isp_file = fopen("ISP_img.ppm","wb");
		        if(!isp_file){
		                perror("Failed to open PPM file");
		                return -1;
		        }
		        fprintf(isp_file,"P6\n%d %d\n255\n",width,height);

			uint8_t* base_ptr = (uint8_t*) isp_cap_bufs[isp_cap_buf.index].start;

			for(int y = 0 ; y < height;y++){

				uint8_t* row_ptr = base_ptr + (y * isp_stride);//jump padding bytes

			        fwrite(row_ptr,sizeof(uint8_t),valid_row_bytes,isp_file);
			}

		        fclose(isp_file);
		}

		//7.Dequeue From ISP Output
		dequeue_dma_buffers(isp_out_fd, &isp_out_buf, type_isp_out);

		//8.Queue to Camera
		queue_buffers(cam_fd, &cam_buf, type_cam, cam_buf.index,0);

		//9.Queue to  ISP Capture
		queue_buffers(isp_cap_fd, &isp_cap_buf, type_isp_cap, isp_cap_buf.index,0);

		//10.Queue to ISP Meta
		queue_buffers(isp_meta_fd, &isp_meta_buf, type_isp_meta, isp_meta_buf.index,0);

		printf("Frame:[%d], Seq:[%d]\n",i,curr_seq);


		//thermal observer
		if((i != 0) && (i % 30 == 0)){

			float current_temp = get_soc_temperature();
			printf("Current temperature is : %f\n",current_temp);

			update_thermal_state(current_temp, &policy);

			if(policy.current_state != prev_state){

				switch(policy.current_state){

					case STATE_NORMAL:
						set_camera_fps(sensor_fd,policy.normal_fps);
						break;

					case STATE_THROTTLED:
						set_camera_fps(sensor_fd,policy.throttled_fps);
						break;
					default:
						break;
				}

				prev_state = policy.current_state;
			}

			clock_gettime(CLOCK_MONOTONIC, &end_time);

			float seconds = (end_time.tv_sec - start_time.tv_sec) + 
					(end_time.tv_nsec - start_time.tv_nsec) / 1000000000.0f;

			float current_fps = 30.0f / seconds;

			printf("Current FPS is : %f\n",current_fps);

			start_time = end_time;

		}
	}


	if(ioctl(cam_fd, VIDIOC_STREAMOFF, &type_cam) < 0){
        	perror("Camera Stream Off Failed");
		return -1;
    	}

        if(ioctl(isp_out_fd, VIDIOC_STREAMOFF, &type_isp_out) < 0){
                perror("ISP Out Stream Off Failed");
                return -1;
        }

        if(ioctl(isp_cap_fd, VIDIOC_STREAMOFF, &type_isp_cap) < 0){
                perror("ISP Cap Stream Off Failed");
                return -1;
        }

        if(ioctl(isp_meta_fd, VIDIOC_STREAMOFF, &type_isp_meta) < 0){
                perror("ISP Cap Stream Off Failed");
                return -1;
        }

        printf("\n>>> All Factories Motor OFF! <<<\n\n");

	close(cam_fd);
	close(isp_out_fd);
	close(isp_cap_fd);
	close(isp_meta_fd);

	return 0;
}
