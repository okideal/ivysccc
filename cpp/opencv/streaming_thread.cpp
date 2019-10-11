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
    re_init();
}

VideoCaptureAsync::~VideoCaptureAsync() {
    cap_.reset();
}

void VideoCaptureAsync::re_init() {
    capture_thread_running = false;
    cap_.reset();
    cap_ = boost::shared_ptr<cv::VideoCapture>(new cv::VideoCapture(s_add));
    run();
}

void VideoCaptureAsync::run() {
    boost::function0<void> f = boost::bind(&VideoCaptureAsync::do_capture, this);
    boost::thread thrd(f);
}

bool VideoCaptureAsync::get_latest(cv::Mat &output) {
    output.release();
    std::lock_guard<std::mutex> lock(q_mutex);
    cv::Mat o = framesQueue.front();
    if(!o.empty()) {
        o.copyTo(output);
        while (!framesQueue.empty()) framesQueue.pop();
        return true;
    }else{
        while (!framesQueue.empty()){
            o = framesQueue.front();
            if(!o.empty()) break;
            else framesQueue.pop();
        }

        while (!framesQueue.empty()) framesQueue.pop();
        o.copyTo(output);
        return !output.empty();
    }
}

void VideoCaptureAsync::do_capture() {
    capture_thread_running = true;
    while (capture_thread_running){
        {
            std::lock_guard<std::mutex> lock(s_mutex);
        }

        if(!cap_->isOpened()){
            //todo log
            std::cout << "Could not open camera, reinitializing!!" << std::endl;
            re_init();
            cv::waitKey(100);
            continue;
        }

        if(!cap_->read(frame)){
            //todo log
            std::cout << "Could not capture frame, reinitializing!!" << std::endl;
            re_init();
        }

        if(!frame.empty()){
            std::lock_guard<std::mutex> g(q_mutex);
            while (framesQueue.size() > max_queue_size){
                framesQueue.pop();
            }
            framesQueue.push(frame.clone());
        }
    }
    //todo log
    std::cout << "do capture stopped" << std::endl;
}
