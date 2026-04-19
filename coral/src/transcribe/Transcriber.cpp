#include "Transcriber.h"
#include "AdaptiveThreadController.h"
#include "Logger.h"
#include "whisper.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <iostream>
#include <regex>
#include <vector>

#include <sndfile.h>
#include <string>

namespace {

/**
 * Strip non-speech annotations like "(fan whirring)", "[music]", "*coughs*"
 * that Whisper sometimes emits for background sounds. We keep params
 * suppress_nst=true upstream, but that flag only blocks Whisper's dedicated
 * non-speech vocabulary tokens — the model can still spell these labels using
 * regular subword tokens, so a textual sweep is the only reliable defense.
 *
 * We match three balanced wrappers:
 *   (...)   most common: (laughs), (water running), (fan whirring)
 *   [...]   used for: [music], [silence], [BLANK_AUDIO]
 *   *...*   rare but seen: *coughs*, *sighs*
 *
 * For dictation/keystroke-injection use, we never want these in the output;
 * the loss of any user-spoken parenthetical (rare in voice typing) is an
 * acceptable trade.
 */
std::string stripNonSpeechAnnotations(std::string s)
{
    static const std::regex annotationRe(
        R"(\s*(\([^)]*\)|\[[^\]]*\]|\*[^*]*\*)\s*)");
    s = std::regex_replace(s, annotationRe, " ");

    static const std::regex wsRe(R"(\s+)");
    s = std::regex_replace(s, wsRe, " ");

    auto notSpace = [](unsigned char c){ return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

} // namespace

Transcriber* Transcriber::sInstance = nullptr;

Transcriber* Transcriber::getInstance()
{
    if (sInstance) {
        return sInstance;
    }
    sInstance = new Transcriber();
    return sInstance;
}

Transcriber::~Transcriber()
{
    if (_ctx) {
        whisper_free(_ctx);
        _ctx = nullptr;
    }
}

bool Transcriber::ensureModel(const std::string& modelPath)
{
    if (_ctx && _loadedModelPath == modelPath) {
        return true;
    }

    if (_ctx) {
        INFO("Transcriber: model path changed, reloading (was=[" + _loadedModelPath + "] now=[" + modelPath + "])");
        whisper_free(_ctx);
        _ctx = nullptr;
        _loadedModelPath.clear();
    }

    struct whisper_context_params cparams = whisper_context_default_params();
    _ctx = whisper_init_from_file_with_params(modelPath.c_str(), cparams);
    if (!_ctx) {
        ERROR("Transcriber: failed to load Whisper model: [" + modelPath + "]");
        return false;
    }
    _loadedModelPath = modelPath;
    INFO("Transcriber: model loaded once and cached: [" + modelPath + "]");
    return true;
}

bool Transcriber::whisper_pcmf32_from_wav(const std::string& path, std::vector<float>& outPCM)
{
    SF_INFO sfinfo;
    SNDFILE* sndfile = sf_open(path.c_str(), SFM_READ, &sfinfo);
    if (!sndfile) {
        fprintf(stderr, "Failed to open '%s'\n", path.c_str());
        return false;
    }

    if (sfinfo.channels != 1 || sfinfo.samplerate != 16000) {
        fprintf(stderr, "Audio must be mono, 16 kHz. Got %d ch, %d Hz\n",
                sfinfo.channels, sfinfo.samplerate);
        sf_close(sndfile);
        return false;
    }

    outPCM.resize(sfinfo.frames);
    sf_readf_float(sndfile, outPCM.data(), sfinfo.frames);
    sf_close(sndfile);
    return true;
}

std::string Transcriber::transcribeAudio(const std::string& wavFile, const std::string& whisperModelPath, const std::string& language)
{
    if (!ensureModel(whisperModelPath)) {
        return "";
    }

    if (!whisper_pcmf32_from_wav(wavFile.c_str(), _pcmBuf)) {
        std::cerr << "Failed to read WAV file\n";
        return "";
    }

    whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.print_progress  = false;
    params.print_special   = false;
    params.print_realtime  = false;
    params.suppress_blank  = true;
    params.suppress_nst    = true;
    params.no_speech_thold = 0.60f;
    params.logprob_thold   = -0.80f;
    params.entropy_thold   = 2.40f;
    params.language  = language.c_str();
    params.n_threads = AdaptiveThreadController::getInstance().getThreadCount();

    const auto t0 = std::chrono::steady_clock::now();

    if (whisper_full(_ctx, params, _pcmBuf.data(), _pcmBuf.size()) != 0) {
        std::cerr << "Failed to run Whisper inference\n";
        return "";
    }

    const auto t1 = std::chrono::steady_clock::now();
    long long whisper_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    long long audio_duration_ms = static_cast<long long>(_pcmBuf.size()) * 1000LL / 16000;
    AdaptiveThreadController::getInstance().reportInferenceLatency(whisper_ms, audio_duration_ms);

    std::string result;
    int n_segments = whisper_full_n_segments(_ctx);
    for (int i = 0; i < n_segments; ++i) {
        result += whisper_full_get_segment_text(_ctx, i);
        result += " ";
    }

    std::string cleaned = stripNonSpeechAnnotations(std::move(result));
    return cleaned;
}
