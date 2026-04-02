#include "V4L2Node.hpp"
#include "SharedMetaQueue.hpp"
#include <iostream>
#include <fcntl.h>
#include <unistd.h> // for sleep
#include <stdlib.h>
#include <thread>
#include <mutex>
#include <chrono>
#include <atomic>
#include <linux/bcm2835-isp.h>

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

void update_thermal_state(float current_temp, struct ThermalPolicy* policy){

	if(current_temp > policy->trigger_temp){

		//overheat
		policy->current_state = STATE_THROTTLED;

	}

	else if(current_temp < policy->recovery_temp){

		policy->current_state = STATE_NORMAL;
	}
}

int set_camera_fps(V4L2Node* sensor_node, int fps){

	int vblank_val = (fps >=15) ? 40 : ((fps == 5) ? 6500 : 40);

	if(sensor_node->setControl(V4L2_CID_VBLANK,vblank_val) < 0){
		std::cerr << "Set VBLANK Failed" <<std::endl;
		return -1;
	}

	std::cout <<"\n>>> [Thermal Policy] Camera FPS changed to " << fps << "<<<\n\n" << std::endl;
	return 0;
}

struct SharedContext{

	//Is main still running?
	std::atomic<bool> is_running{true};
};

SharedContext g_ctx;

void thermal_monitor_worker(V4L2Node* sensor_node){

	struct ThermalPolicy policy = {STATE_NORMAL,75.0f,70.0f,15,5};
	enum thermal_state_type prev_state = policy.current_state;

	set_camera_fps(sensor_node, policy.normal_fps);

	while(g_ctx.is_running){

		std::this_thread::sleep_for(std::chrono::seconds(2));

		if(!g_ctx.is_running) break;

		char buf[16];

        	int fd = open("/sys/class/thermal/thermal_zone0/temp",O_RDONLY);
		if(fd < 0){
			perror("Failed to open thermal");
			continue;
		}

		ssize_t bytes_read = read(fd,buf,sizeof(buf)-1);

		close(fd);

		if(bytes_read <= 0){
			printf("Not any byte be read");
			continue;
		}

		buf[bytes_read] = '\0';

		float current_temp = (float)atoi(buf)/1000.0f;
		std::cout << "[Thermal Thread] : Current Temperature is " << current_temp << std::endl;


		update_thermal_state(current_temp,&policy);

		if (policy.current_state != prev_state){

			if(policy.current_state == STATE_THROTTLED){

				set_camera_fps(sensor_node, policy.throttled_fps);
			}
			else{
				set_camera_fps(sensor_node, policy.normal_fps);
			}

			prev_state = policy.current_state;
		}

	}

	std::cout << "The main program is over,and then turn off the monitor "<< std::endl;
	return;
}

MetaRingBuffer g_meta_queue;

void ae_awb_worker(V4L2Node* sensor_node,V4L2Node* isp_out_node){

	std::cout << "[3A Thread]: === Start ===" << std::endl;

	//AE Initialize
	int current_exposure = 1600;
	int min_exp = 4, max_exp =3522;

	int current_gain = 0;
	int min_gain = 0, max_gain = 232;

	sensor_node->setControl(V4L2_CID_EXPOSURE, current_exposure);
	sensor_node->setControl(V4L2_CID_ANALOGUE_GAIN, current_gain);

	const float MIN_LUMA = 383.0f;
	const float MAX_LUMA = 6143.0f;

	const float TARGET_RATIO = 0.18f;

	const int TARGET_LUMA = (int)((MAX_LUMA-MIN_LUMA) * TARGET_RATIO + MIN_LUMA);

	//AWB Initialize
	int min_r_gain = 1, max_r_gain = 65535,current_r_gain = 1000;
    	int min_b_gain = 1, max_b_gain = 65535,current_b_gain = 1000;
    	isp_out_node->setControl(V4L2_CID_RED_BALANCE, current_r_gain);
    	isp_out_node->setControl(V4L2_CID_BLUE_BALANCE, current_b_gain);

	while(g_ctx.is_running){

		MetaPayload payload;

		if(!g_meta_queue.wait_and_pop(payload)) break;

		auto* stats = static_cast<bcm2835_isp_stats*>(payload.meta_data_ptr);

		//====================
		//        AE
		//====================
		uint64_t total_r = 0, total_g = 0, total_b = 0;
	        uint32_t total_counted = 0;

		for(int i = 0; i < AGC_REGIONS; i++){

			total_r += stats->agc_stats[i].r_sum;
			total_g += stats->agc_stats[i].g_sum;
            		total_b += stats->agc_stats[i].b_sum;
            		total_counted += stats->agc_stats[i].counted;

		}

		if(total_counted == 0) continue;

		uint64_t total_light = total_r + total_g + total_b;
		int current_luma = total_light / (total_counted * 4);


		int error = TARGET_LUMA - current_luma;

		float Kp_exp = 0.5f;
		float Kp_gain = 0.05f;

		if(abs(error) > 15){

			if(error > 0){

				if(current_exposure < max_exp){

					current_exposure += (int)(error * Kp_exp);
					if(current_exposure > max_exp) current_exposure = max_exp;
				}
				else{
					current_gain += (int)(error * Kp_gain);
					if(current_gain > max_gain) current_gain = max_gain;
				}
			}

			else{

				if(current_gain > min_gain){

					current_gain += (int)(error * Kp_gain);
					if(current_gain < min_gain) current_gain = min_gain;
				}
				else{

					current_exposure += (int)(error * Kp_exp);
                                        if(current_exposure < min_exp) current_exposure = min_exp;
				}
			}


			sensor_node->setControl(V4L2_CID_EXPOSURE, current_exposure);
			sensor_node->setControl(V4L2_CID_ANALOGUE_GAIN, current_gain);
		}

		//====================
                //        AWB
                //====================

		uint64_t awb_r = 0, awb_g = 0, awb_b = 0;
	        uint32_t awb_counted = 0;

		for (int i = 0; i < AWB_REGIONS; i++) {

        		awb_r += stats->awb_stats[i].r_sum;
		        awb_g += stats->awb_stats[i].g_sum;
		        awb_b += stats->awb_stats[i].b_sum;
		        awb_counted += stats->awb_stats[i].counted;
        	}

		if(awb_counted > 0){

			float avg_r = (float)awb_r / awb_counted;
	        	float avg_g = (float)awb_g / awb_counted;
        	    	float avg_b = (float)awb_b / awb_counted;

			if(avg_r > 0 && avg_b > 0 && avg_g > 0){

				float target_r_ratio = (avg_g* 1.1f) / avg_r;
				float target_b_ratio = (avg_g* 0.95f) / avg_b;

				int target_r_gain = (int)(1000.0f * target_r_ratio);
				int target_b_gain = (int)(1000.0f * target_b_ratio);

				current_r_gain += (int)((target_r_gain - current_r_gain) * 0.1f);
				current_b_gain += (int)((target_b_gain - current_b_gain) * 0.1f);

				if (current_r_gain > max_r_gain) current_r_gain = max_r_gain;
                		if (current_r_gain < min_r_gain) current_r_gain = min_r_gain;
                		if (current_b_gain > max_b_gain) current_b_gain = max_b_gain;
                		if (current_b_gain < min_b_gain) current_b_gain = min_b_gain;


			        isp_out_node->setControl(V4L2_CID_RED_BALANCE, current_r_gain);
			        isp_out_node->setControl(V4L2_CID_BLUE_BALANCE, current_b_gain);

			}
		}


		std::cout << "[3A] Seq: " << payload.sequence 
			<<" | Luma: "<<current_luma 
			<<" (Err: " << error << ")"
			<<" -> Set Exp: " << current_exposure 
			<<" , ISO: " << current_gain 
			<<" , Red_gain: "<< current_r_gain 
			<<" , Blue_gain: "<< current_b_gain 
			<< std::endl;



	}


	std::cout << "[3A Thread] : === End === " << std::endl;

}

int main() {


	int width = 3280;
	int height = 2464;
	int buf_count = 4;

	std::cout << "\n=== System Initalization ===" << std::endl;

	//1.Construct device
	V4L2Node cam_node("/dev/video0",V4L2_BUF_TYPE_VIDEO_CAPTURE);
	V4L2Node isp_out_node("/dev/video13", V4L2_BUF_TYPE_VIDEO_OUTPUT);
	V4L2Node isp_cap_node("/dev/video14", V4L2_BUF_TYPE_VIDEO_CAPTURE);
	V4L2Node isp_meta_node("/dev/video16", V4L2_BUF_TYPE_META_CAPTURE);

	V4L2Node sensor_node("/dev/v4l-subdev0",V4L2_BUF_TYPE_VIDEO_CAPTURE);

	//sensor_node.printSupportedControls();
	isp_out_node.printSupportedControls();
	isp_cap_node.printSupportedControls();

	std::cout << "--- 1.Set Format ---" << std::endl;

	cam_node.setFormat(V4L2_PIX_FMT_SRGGB10,width,height);
	isp_out_node.setFormat(V4L2_PIX_FMT_SRGGB10,width,height);
	isp_cap_node.setFormat(V4L2_PIX_FMT_RGB24,width,height);

	isp_meta_node.setMetaFormat(V4L2_META_FMT_BCM2835_ISP_STATS);

	std::cout << "--- 2.Request Buffers ---" << std::endl;

	cam_node.requestAndExportBuffers(buf_count);
	isp_out_node.requestDmaBuffers(buf_count);
	isp_cap_node.requestAndMapBuffers(buf_count);
	isp_meta_node.requestAndMapBuffers(buf_count);

	std::cout << "--- 3.Queue Empty Buffers ---" << std::endl;

	for(int i = 0 ; i < buf_count; i++){

		cam_node.queueBuffer(i);
		isp_cap_node.queueBuffer(i);
		isp_meta_node.queueBuffer(i);
	}

	std::cout << "--- 4.Stream ON ---" << std::endl;

	cam_node.streamOn();
	isp_out_node.streamOn();
	isp_cap_node.streamOn();
	isp_meta_node.streamOn();


	std::cout << "=== Inital Successfully ===" << std::endl;

	std::cout << "\n=== Capture Picture ===" << std::endl;

	std::thread thermal_thread(thermal_monitor_worker , &sensor_node);
	std::thread ae_awb_thread(ae_awb_worker, &sensor_node,&isp_out_node);

	int cap_number = 150;

	for(int i = 0; i < cap_number; i++){

		struct v4l2_buffer cam_buf, isp_out_buf, isp_cap_buf, isp_meta_buf;

		//1. Capture pic from camera
		cam_node.dequeueBuffer(&cam_buf);

		//2.Zero-copy to ISP Output
		int current_dma_fd = cam_node.getBuffer(cam_buf.index)->export_fd;
		int out_idx = i%4;
		isp_out_node.queueBuffer(out_idx,cam_buf.bytesused,current_dma_fd);

		//3.ISP Processing....Get Back data
		isp_cap_node.dequeueBuffer(&isp_cap_buf);
		isp_meta_node.dequeueBuffer(&isp_meta_buf);

		MetaPayload payload;
		payload.sequence = isp_meta_buf.sequence;
		payload.meta_data_ptr = isp_meta_node.getBuffer(isp_meta_buf.index)->start;

		g_meta_queue.push(payload);

		//4.Store a picture in .ppm
		if(i == cap_number-1){

			std::cout << "Saving PPM Image..." << std::endl;

			int isp_stride = isp_cap_node.getStride();//9888
			int valid_row_bytes = width*3;//3280*3 = 9840

			FILE* isp_file = fopen("ISP_cpp_img.ppm", "wb");
			if(isp_file){

				fprintf(isp_file,"P6\n%d %d\n255\n",width,height);

				uint8_t* base_ptr = (uint8_t*)isp_cap_node.getBuffer(isp_cap_buf.index)->start;

				for(int y = 0; y < height; y++){

					uint8_t* row_ptr = base_ptr + (y * isp_stride);//jump padding
				        fwrite(row_ptr,sizeof(uint8_t),valid_row_bytes,isp_file);
				}
				fclose(isp_file);
				std::cout << ">>> Image saved successfully as ISP_cpp_img.ppm!" << std::endl;
			}
		}

		//5.Take back empty boxes that have been processed
		isp_out_node.dequeueBuffer(&isp_out_buf);

		//6.Take back Empty box to production line
		cam_node.queueBuffer(cam_buf.index);
		isp_cap_node.queueBuffer(isp_cap_buf.index);
		isp_meta_node.queueBuffer(isp_meta_buf.index);

		std::cout << "Frame:[" << i << "] processed." << std::endl;

	}

	std::cout << "--- 5.Stream off ---" << std::endl;

        cam_node.streamOff();
        isp_out_node.streamOff();
        isp_cap_node.streamOff();
        isp_meta_node.streamOff();

	g_ctx.is_running = false;
	g_meta_queue.stop();

	thermal_thread.join();
	ae_awb_thread.join();

	std::cout << "\n=== Program is Over, Call Destructor ===" << std::endl;

	return 0;
}
