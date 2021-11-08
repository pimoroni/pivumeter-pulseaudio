#include <iostream>
#include <chrono>
#include <string>
#include <string_view>
#include <signal.h>
#include <pulse/pulseaudio.h>

#include "device/phat-beat.hpp"

//const char* sink_name = "alsa_output.platform-bcm2835_audio.digital-stereo";
//const char* sink_name = "alsa_output.platform-fef05700.hdmi.hdmi-stereo";
const char* sink_name = "alsa_output.platform-soc_sound.stereo-fallback";

pa_context *context = NULL;
pa_stream *stream = NULL;
pa_mainloop_api *mainloop_api = NULL;
pa_proplist *proplist = NULL;

pivumeter::PHATBeat phatbeat = pivumeter::PHATBeat();

std::chrono::time_point<std::chrono::high_resolution_clock> t_last_update;

static void quit(int ret) {
    mainloop_api->quit(mainloop_api, ret);
}

static void exit_signal_callback(pa_mainloop_api*m, pa_signal_event *e, int sig, void *userdata) {
    std::cout << "pivumeter: shutting down..." << std::endl;
    quit(0);
}

static void pulse_stream_state_callback(pa_stream *stream, void *) {
    switch(pa_stream_get_state(stream)) {
        case PA_STREAM_UNCONNECTED:
            std::cout << "pivumeter: stream unconnected!" << std::endl;
            break;
        case PA_STREAM_CREATING:
            std::cout << "pivumeter: creating stream..." << std::endl;
            break;
        case PA_STREAM_READY:
            std::cout << "pivumeter: stream ready!" << std::endl;

            break;
        case PA_STREAM_FAILED:
            std::cout << "pivumeter: stream failed! " << pa_strerror(pa_context_errno(context)) << std::endl;
            break;
        case PA_STREAM_TERMINATED:
            std::cout << "pivumeter: stream terminated!" << std::endl;
            quit(1);
            break;
    }
}

static void pulse_stream_read_callback(pa_stream *stream, size_t length, void *) {
    const void *p;

    auto now = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - t_last_update);

    if (pa_stream_peek(stream, &p, &length) < 0) {
        std::cout << "pivumeter: pa_stream_peek() failed: " << pa_strerror(pa_context_errno(context)) << std::endl;
	return;
    }

    if(ms.count() < 5) {
        pa_stream_drop(stream);
        return;
    }


    /*const int16_t *channel = (const int16_t *)p;
    phatbeat.update(channel[0], channel[1]);
    phatbeat.render();

    pa_stream_drop(stream);
    return;*/

    // p is now a pointer to the stream
    // length is the number of bytes in the stream
    //std::cout << "pivumeter: got " << (length / sizeof(int16_t)) << "samples!" << std::endl;

    int16_t peak_l = 0;
    int16_t peak_r = 0;
    const int16_t *samples = (const int16_t *)p;
    for(auto sample = 0u; sample < length / sizeof(int16_t) / 2; sample += 2) {
        int16_t s_l = samples[sample];
        int16_t s_r = samples[sample + 1];
        if(s_l > peak_l) peak_l = s_l;
        if(s_r > peak_r) peak_r = s_r;
    }
    //std::cout << "pivumeter: peak L:" << peak_l << " R:" << peak_r << std::endl;
    phatbeat.update(peak_l, peak_r);
    phatbeat.render();
    t_last_update = now;

    pa_stream_drop(stream);
}

static void pulse_monitor(const char* name, const pa_sample_spec &ss, const pa_channel_map &channel_map) {
    if(phatbeat.init() != 0) {
        std::cout << "Failed to init output device!" << std::endl;
        phatbeat.deinit();
        quit(1);
    }

    t_last_update = std::chrono::high_resolution_clock::now();

    pa_sample_spec new_sample_spec;
    new_sample_spec.format = PA_SAMPLE_S16LE;
    new_sample_spec.rate = ss.rate;
    new_sample_spec.channels = ss.channels;

    stream = pa_stream_new(context, "pivumeter", &new_sample_spec, &channel_map);
    pa_stream_set_state_callback(stream, pulse_stream_state_callback, NULL);
    pa_stream_set_read_callback(stream, pulse_stream_read_callback, NULL);
    pa_stream_connect_record(stream, name, NULL, (enum pa_stream_flags) PA_STREAM_PEAK_DETECT );
}

static void pulse_get_sink_info_callback(pa_context *, const pa_sink_info *si, int is_last, void *) {
    if (is_last < 0) {
        std::cout << "pivumeter: failed to get sink info" << std::endl; 
        return;
    }

    if (!si) return;

    std::cout << "pivumeter: got sink info: " << si->description << std::endl;

    pulse_monitor(si->monitor_source_name, si->sample_spec, si->channel_map);
}

static void pulse_state_callback(pa_context *context, void *) {
    switch (pa_context_get_state(context)) {
        case PA_CONTEXT_UNCONNECTED:
            std::cout << "pivumeter: unconnected." << std::endl;
            break;
        case PA_CONTEXT_CONNECTING:
            std::cout << "pivumeter: connecting..." << std::endl;
            break;
        case PA_CONTEXT_AUTHORIZING:
            std::cout << "pivumeter: authorizing..." << std::endl;
            break;
        case PA_CONTEXT_SETTING_NAME:
            std::cout << "pivumeter: setting name..." << std::endl;
            break;
        case PA_CONTEXT_READY:
            std::cout << "pivumeter: context ready!" << std::endl;

            pa_operation_unref(pa_context_get_sink_info_by_name(context, sink_name, pulse_get_sink_info_callback, NULL));
            break;
        case PA_CONTEXT_FAILED:
            std::cout << "pivumeter: context failed!" << std::endl;
            break;
        case PA_CONTEXT_TERMINATED:
            std::cout << "pivumeter: context terminated!" << std::endl;
            break;
    }
}

int main(int argc, char *argv[]) {
    int ret = 1;
    pa_mainloop* mainloop = NULL;

    if (!(mainloop = pa_mainloop_new())) {
        goto quit;
    }

    proplist = pa_proplist_new();
    mainloop_api = pa_mainloop_get_api(mainloop);

    pa_signal_init(mainloop_api);
    pa_signal_new(SIGINT, exit_signal_callback, NULL);
    pa_signal_new(SIGTERM, exit_signal_callback, NULL);

    if (!(context = pa_context_new_with_proplist(mainloop_api, NULL, proplist))) {
        std::cout << "pa_context_new() failed." << std::endl;
    }

    pa_context_set_state_callback(context, pulse_state_callback, NULL);
    pa_context_connect(context, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL);

    if (pa_mainloop_run(mainloop, &ret) < 0) {
        std::cout << "pa_mainloop_run() failed!" << std::endl;
        goto quit;
    }

    phatbeat.deinit();

    return 0;

quit:
    if (context) pa_context_unref(context);
    if (proplist) pa_proplist_free(proplist);
    return 1;
}
