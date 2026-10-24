#include "papernx/cover_pipeline.hpp"
#include <cassert>
#include <iostream>

int main() {
    papernx::CoverTelemetry telemetry;
    telemetry.enable("generated-documents-v1", "cold");

    const auto first=telemetry.requested("book-alpha");
    assert(first!=0 && telemetry.requested("book-alpha")==first);
    const auto failed=telemetry.requested("book-failed");
    assert(telemetry.discarded("book-failed",failed,true));
    assert(!telemetry.discarded("book-failed",failed,true));
    assert(telemetry.uploaded("book-alpha",first));
    assert(!telemetry.uploaded("book-alpha",first));

    const auto renewed=telemetry.requested("book-alpha");
    assert(renewed!=first && !telemetry.uploaded("book-alpha",first));
    telemetry.abandonAll();
    assert(!telemetry.uploaded("book-alpha",renewed));

    for(int i=0;i<300;++i) {
        const auto id="ring-"+std::to_string(i);
        const auto token=telemetry.requested(id);
        assert(telemetry.uploaded(id,token));
        telemetry.uploadQueueWait(double(i)/100.0);
        telemetry.frameTime(double(i)/10.0);
        telemetry.decodeTime(double(i)/20.0);
    }
    const auto stats=telemetry.snapshot();
    assert(stats.epochRequested==1 && stats.epochLoaded==1 && stats.outstandingRequests==0);
    assert(stats.coverRequests==303 && stats.uploadFailures==1);
    std::cout<<telemetry.toJson(stats,"0.9.1","abcdef0123456789","software")<<std::flush;

    papernx::CoverTelemetry viewport;
    viewport.requested("visible-cover");
    const auto noCover=viewport.requested("coverless-epub");
    const auto visible=viewport.requested("visible-cover");
    assert(viewport.uploaded("visible-cover",visible));
    assert(viewport.unavailable("coverless-epub",noCover));
    const auto viewportStats=viewport.snapshot();
    assert(viewportStats.epochRequested==1 && viewportStats.epochLoaded==1);
    assert(viewportStats.fill50Ms>=0 && viewportStats.fill95Ms>=0);
}
