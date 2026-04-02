#include <mutex>
#include <condition_variable>

struct MetaPayload{

	unsigned int sequence;
	void* meta_data_ptr;
};


class MetaRingBuffer{

private:
	static const int SIZE = 5;
	MetaPayload buffer[SIZE];

	int head = 0;	//Consumer read
	int tail = 0;	//Producer write
	int count = 0;	//Data that has not yet been read

	std::mutex mtx;
	std::condition_variable cv;

	bool stopped = false;

public:

	void stop(){

		std::lock_guard<std::mutex> lock(mtx);
		stopped = true;

		cv.notify_all();
	}

	//Producer: push data
	void push(const MetaPayload& data){

		std::lock_guard<std::mutex> lock(mtx);

		buffer[tail] = data;

		tail = (tail+1) % SIZE;

		if(count < SIZE){
			count++;
		}
		else if (count == SIZE){
			head = (head+1) % SIZE;
		}

		cv.notify_one();

	}

	//Consumer: wait until data in and pop out data
	bool wait_and_pop(MetaPayload& out_data){

		std::unique_lock<std::mutex> lock(mtx);

		cv.wait(lock,[this](){ return count > 0 || stopped;});

		if(stopped && (count == 0)) return false;

		out_data = buffer[head];

		head = (head+1) % SIZE;

		count--;

		return true;
	}



};
