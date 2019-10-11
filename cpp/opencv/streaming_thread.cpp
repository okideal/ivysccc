//
// Created by ivys on 19-7-8.
//

#include "../header/utils/streaming.h"

VideoStream::VideoStream(GlobalConfig* global_cfg, int opt, const std::string &video_path, bool with_asyn) :
global_cfg(global_cfg), opt(opt), with_asyn(with_asyn){
    if (opt == 0){
        cap = new cv::VideoCapture(0);
        cap->set(cv::CAP_PROP_FRAME_WIDTH, global_cfg->FRAME_WIDTH);
        cap->set(cv::CAP_PROP_FRAME_HEIGHT, global_cfg->FRAME_HEIGHT);
    }else if(opt == 2){
        if(!video_path.empty()){
            if(!with_asyn) cap = new cv::VideoCapture(video_path);
            else asyn_cap = new VideoCaptureAsync(video_path);
        }else{
            cap = new cv::VideoCapture("rtmp://10.0.0.9:2018/live");
        }
    }else if(opt == 3){
        static_img = cv::imread(video_path);
    }
}

VideoStream::~VideoStream() {
    if(!with_asyn){
        cap->release();
        delete(cap);
    }else{
        asyn_cap->stop();
        asyn_cap->cap_->release();
        delete(asyn_cap);
    }
}

bool VideoStream::read(cv::Mat &output_img) {
    output_img.release();
    if(!with_asyn){
        return cap->read(output_img);
    }else{
        return asyn_cap->get_latest(output_img);
    }
}

VideoCaptureAsync::VideoCaptureAsync(const std::string &s_add): s_add(s_add){
    grab_on.store(false);
    cap_ = std::make_shared<cv::VideoCapture>(cv::VideoCapture(s_add));
    run();
}

VideoCaptureAsync::~VideoCaptureAsync() {
    cap_.reset();
}

void VideoCaptureAsync::run() {
    //grab_t.detach();
    grab_t = std::thread(&VideoCaptureAsync::do_capture, this);
    grab_on.store(true);
}

void VideoCaptureAsync::stop() {
    if(grab_on.load()){
        grab_on.store(false);
        grab_t.join();

        while (!framesQueue.empty()){
            framesQueue.pop_front();
        }
    }
}

bool VideoCaptureAsync::get_one_frame(cv::Mat &output, bool latest) {
    std::lock_guard<std::mutex> lock(q_mutex);
    if(framesQueue.empty()) return false;
    if(latest) while (framesQueue.size()>1) framesQueue.pop_front();
    framesQueue.front().copyTo(output);
    framesQueue.pop_front();
    return true;
}

bool VideoCaptureAsync::get_latest(cv::Mat &output) {
    return get_one_frame(output, true);
}

void VideoCaptureAsync::do_capture() {
    try{
        capture_thread_running = true;
        while (capture_thread_running){
            cv::Mat frame;
            if(!cap_->isOpened()){
                //todo log
                cv::waitKey(100);
                continue;
            }

            if(!cap_->read(frame)){
                //todo log
                usleep(1000);
                continue;
            }

            if(!frame.empty()){
                std::lock_guard<std::mutex> g(q_mutex);
                framesQueue.push_back(frame.clone());
                while (framesQueue.size() > max_queue_size){
                    framesQueue.pop_front();
                }
            }
        }
    }catch (cv::Exception &e){
        std::cout << "caught cv::expection in do_capture" << std::endl;
        std::cout << e.what() << std::endl;
    }catch (...){
        std::cout << "unknown exception caught in do_capture" << std::endl;
    }
    //todo log
    std::cout << "do capture stopped" << std::endl;
}

