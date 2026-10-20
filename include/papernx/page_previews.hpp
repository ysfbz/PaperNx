#pragma once
#include "papernx/mupdf_bridge.h"
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cmath>
#include <utility>
#include <cstdint>
#include <deque>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace papernx {
// Independent MuPDF context: the reader is never shared with this worker, and
// SDL textures are created only by the UI thread after take().
class PagePreviews {
public:
    struct Spec { std::string identity,path,css;int width{600},height{720};float em{22};int viewportHeight{720}; };
    struct Result {std::string identity;int page{},y{},width{},height{},stride{};std::vector<unsigned char> rgb;};
    ~PagePreviews(){stop();}
    void configure(Spec spec) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(spec.identity==spec_.identity)return;
        spec_=std::move(spec);++generation_;queue_.clear();results_.clear();requested_.clear();wake_.notify_one();
    }
    void request(int page,int y=0,bool priority=false) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(spec_.identity.empty() || page<0 || stopped_)return;
        if(priority) {
            for(auto queued:queue_)requested_.erase(queued);
            queue_.clear();
        }
        const auto location=std::make_pair(page,std::max(0,y));
        if(requested_.count(location) || queue_.size()>=24)return;
        requested_.insert(location);queue_.push_back(location);
        if(!thread_.joinable())thread_=std::thread([this]{run();});
        wake_.notify_one();
    }
    void discardQueue() {
        std::lock_guard<std::mutex> lock(mutex_);
        for(auto location:queue_)requested_.erase(location);
        queue_.clear();
    }
    std::vector<Result> take() {
        std::lock_guard<std::mutex> lock(mutex_);std::vector<Result> out;
        while(!results_.empty()){requested_.erase({results_.front().page,results_.front().y});out.push_back(std::move(results_.front()));results_.pop_front();}
        return out;
    }
    bool pending() const {std::lock_guard<std::mutex> lock(mutex_);return active_ || !queue_.empty() || !results_.empty();}
    void clear(){configure({});}
    void stop() {
        {std::lock_guard<std::mutex> lock(mutex_);stopped_=true;queue_.clear();wake_.notify_one();}
        if(thread_.joinable())thread_.join();
    }
private:
    mutable std::mutex mutex_;std::condition_variable wake_;std::thread thread_;
    Spec spec_;std::uint64_t generation_{0};bool stopped_{false},active_{false};
    std::deque<std::pair<int,int>> queue_;std::set<std::pair<int,int>> requested_;std::deque<Result> results_;
    void run() {
        PxDocument *document=nullptr;std::uint64_t opened=~std::uint64_t{0};
        for(;;) {
            Spec spec;int page,y;std::uint64_t generation;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                if(!wake_.wait_for(lock,std::chrono::seconds(3),[&]{return stopped_ || !queue_.empty() || opened!=generation_;})) {
                    lock.unlock();if(document)px_document_close(document);document=nullptr;continue;
                }
                if(stopped_)break;
                if(opened!=generation_) {
                    opened=generation_;lock.unlock();if(document)px_document_close(document);document=nullptr;lock.lock();
                    if(opened!=generation_)continue;
                }
                if(queue_.empty())continue;
                spec=spec_;generation=generation_;page=queue_.front().first;y=queue_.front().second;queue_.pop_front();active_=true;
            }
            char error[256]{};
            if(!document)document=px_document_open_limited(spec.path.c_str(),spec.width,spec.height,spec.em,spec.css.c_str(),error,sizeof error,96u*1024u*1024u);
            Result result;result.identity=spec.identity;result.page=page;result.y=y;
            PxImage image{};
            const bool strip=document && px_document_is_epub(document) && spec.height==0;
            const bool ok=document && (strip?
                px_document_render_tile(document,page,y,std::clamp(spec.width,1,4096),std::clamp(spec.viewportHeight,1,1024),&image,error,sizeof error):
                px_document_render(document,page,240,320,&image,error,sizeof error));
            if(ok && image.rgb && image.width>0 && image.height>0 && image.width<=4096 && image.height<=1024 && image.stride>=image.width*3) {
                const float scale=std::min({1.f,240.f/image.width,320.f/image.height});
                result.width=std::max(1,int(image.width*scale));result.height=std::max(1,int(image.height*scale));result.stride=result.width*3;
                result.rgb.resize(std::size_t(result.stride)*result.height);
                for(int row=0;row<result.height;++row)for(int x=0;x<result.width;++x) {
                    const auto source=image.rgb+std::min(image.height-1,int(row/scale))*image.stride+std::min(image.width-1,int(x/scale))*3;
                    std::copy(source,source+3,result.rgb.data()+row*result.stride+x*3);
                }
            }
            px_image_free(&image);
            {
                std::lock_guard<std::mutex> lock(mutex_);active_=false;
                if(generation==generation_ && !stopped_) {
                    if(results_.size()>=24){requested_.erase({results_.front().page,results_.front().y});results_.pop_front();}
                    results_.push_back(std::move(result));
                }
            }
        }
        if(document)px_document_close(document);
    }
};
}
