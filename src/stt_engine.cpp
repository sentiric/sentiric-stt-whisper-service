#include "stt_engine.h"
#include "spdlog/spdlog.h"
#include <samplerate.h>
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <numeric>

// ---------- PROSODY-BASED PROXY TAGS (zero-cost) ----------
static AffectiveTags extract_prosody(const std::vector<float>& pcm, int sample_rate)
{
    AffectiveTags out;
    const int frame_shift = sample_rate / 100; // 10 ms
    double sum_pitch = 0.0;
    size_t frames = 0;
    for (size_t i = 0; i + 3 * frame_shift < pcm.size(); i += frame_shift)
    {
        float r0 = 0, r1 = 0, r2 = 0;
        for (int k = 0; k < frame_shift; ++k) {
            float s = pcm[i + k];
            r0 += s * s; r1 += s * pcm[i + k + 1]; r2 += s * pcm[i + k + 2];
        }
        if (r0 < 1e-5f) continue;
        float m = r1 / (r0 + 1e-6f);
        float f0 = sample_rate / (2.0f * (m + 1e-6f));
        if (f0 > 80 && f0 < 400) { sum_pitch += f0; ++frames; }
    }
    float avg_f0 = frames ? static_cast<float>(sum_pitch / frames) : 150.0f;

    float rms = 0.0f;
    for (float s : pcm) rms += s * s;
    rms = std::sqrt(rms / pcm.size());

    out.gender_proxy  = (avg_f0 > 165.0f) ? "F" : "M";
    out.arousal       = std::min(1.0f, rms * 10.0f);
    out.valence       = (avg_f0 > 200.0f) ? 0.5f : -0.2f;
    if (out.arousal > 0.6f && out.valence > 0.3f)       out.emotion_proxy = "excited";
    else if (out.arousal < 0.3f)                         out.emotion_proxy = "sad";
    else                                                  out.emotion_proxy = "neutral";

    return out;
}

SttEngine::SttEngine(const Settings& settings) : settings_(settings) {
    std::string model_path = settings_.model_dir + "/" + settings_.model_filename;
    spdlog::info("Loading Whisper model from: {}", model_path);
    struct whisper_context_params cparams = whisper_context_default_params();
#ifdef GGML_USE_CUDA
    cparams.use_gpu = true;
    if (settings_.flash_attn) cparams.flash_attn = true;
#endif
    ctx_ = whisper_init_from_file_with_params(model_path.c_str(), cparams);
    if (!ctx_) throw std::runtime_error("Whisper model initialization failed");
    int pool_size = settings_.parallel_requests;
    if (pool_size < 1) pool_size = 1;
    spdlog::info("⚡ Initializing Dynamic Batching Pool with {} states...", pool_size);
    for(int i=0; i<pool_size; ++i) {
        struct whisper_state* state = whisper_init_state(ctx_);
        if(!state) throw std::runtime_error("Failed to allocate whisper state");
        state_pool_.push(state);
        all_states_.push_back(state);
    }
    if (settings_.enable_vad) {
        std::string vad_path = settings_.model_dir + "/" + settings_.vad_model_filename;
        struct whisper_vad_context_params vparams = whisper_vad_default_context_params();
        vparams.use_gpu = false;
        vad_ctx_ = whisper_vad_init_from_file_with_params(vad_path.c_str(), vparams);
        if (vad_ctx_) spdlog::info("✅ Native Silero VAD loaded successfully (CPU Mode).");
        else spdlog::warn("⚠️ Failed to load VAD model at {}. VAD will be disabled.", vad_path);
    }
}

SttEngine::~SttEngine() {
    for(auto* state : all_states_) whisper_free_state(state);
    if (ctx_) whisper_free(ctx_);
    if (vad_ctx_) whisper_vad_free(vad_ctx_);
}

bool SttEngine::is_ready() const { return ctx_ != nullptr; }

struct whisper_state* SttEngine::acquire_state() {
    std::unique_lock<std::mutex> lock(pool_mutex_);
    pool_cv_.wait(lock, [this]{ return !state_pool_.empty(); });
    struct whisper_state* state = state_pool_.front();
    state_pool_.pop();
    return state;
}

void SttEngine::release_state(struct whisper_state* state) {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    state_pool_.push(state);
    pool_cv_.notify_one();
}

std::vector<float> SttEngine::resample_audio(const float* input, size_t input_size, int src_rate, int target_rate) {
    if (src_rate == target_rate || input_size == 0) return {};
    double ratio = (double)target_rate / (double)src_rate;
    long output_frames = (long)(input_size * ratio) + 100;
    std::vector<float> output(output_frames);
    SRC_DATA src_data;
    src_data.data_in = input;
    src_data.input_frames = (long)input_size;
    src_data.data_out = output.data();
    src_data.output_frames = output_frames;
    src_data.src_ratio = ratio;
    src_data.end_of_input = 0;
    int error = src_simple(&src_data, SRC_SINC_FASTEST, 1);
    if (error) {
        spdlog::error("Resampling failed: {}", src_strerror(error));
        return {};
    }
    output.resize(src_data.output_frames_gen);
    return output;
}

bool SttEngine::is_speech_detected(const float* pcm, size_t n_samples) {
    if (!vad_ctx_) return true;
    std::lock_guard<std::mutex> lock(vad_mutex_);
    return whisper_vad_detect_speech(vad_ctx_, pcm, (int)n_samples);
}

std::vector<TranscriptionResult> SttEngine::transcribe_pcm16(
    const std::vector<int16_t>& pcm16, 
    int input_sample_rate,
    const RequestOptions& options
) {
    std::vector<float> pcmf32; pcmf32.reserve(pcm16.size());
    for (const auto& s : pcm16) pcmf32.push_back(static_cast<float>(s) / 32768.0f);
    return transcribe(pcmf32, input_sample_rate, options);
}

std::vector<TranscriptionResult> SttEngine::transcribe(
    const std::vector<float>& pcmf32, 
    int input_sample_rate,
    const RequestOptions& options
) {
    if (!ctx_) return {};
    const float* pcm_ptr = pcmf32.data();
    size_t pcm_size = pcmf32.size();
    std::vector<float> resampled_buffer;
    if (input_sample_rate != 16000) {
        resampled_buffer = resample_audio(pcmf32.data(), pcmf32.size(), input_sample_rate, 16000);
        if (!resampled_buffer.empty()) { pcm_ptr = resampled_buffer.data(); pcm_size = resampled_buffer.size(); }
    }
    if (settings_.enable_vad && pcm_size > (16000 * 0.2)) {
        if (!is_speech_detected(pcm_ptr, pcm_size)) {
            TranscriptionResult empty_res; empty_res.text = ""; empty_res.language = "unknown"; empty_res.prob = 0.0f;
            empty_res.t0 = 0; empty_res.t1 = static_cast<int64_t>(pcm_size / 16.0); empty_res.speaker_turn_next = false;
            return {empty_res};
        }
    }
    struct whisper_state* state = acquire_state();
    int active_beam_size = (options.beam_size >= 0) ? options.beam_size : settings_.beam_size;
    float active_temp = (options.temperature >= 0.0f) ? options.temperature : settings_.temperature;
    int active_best_of = (options.best_of >= 0) ? options.best_of : settings_.best_of;
    whisper_sampling_strategy strategy = (active_beam_size > 1) ? WHISPER_SAMPLING_BEAM_SEARCH : WHISPER_SAMPLING_GREEDY;
    whisper_full_params wparams = whisper_full_default_params(strategy);
    wparams.print_realtime = false; wparams.print_progress = false; wparams.print_timestamps = !settings_.no_timestamps;
    wparams.print_special = false; wparams.token_timestamps = true; wparams.suppress_nst = settings_.suppress_nst;
    wparams.no_speech_thold = settings_.no_speech_threshold;
    wparams.translate = options.translate; wparams.tdrz_enable = options.enable_diarization;
    std::string target_lang = options.language.empty() ? settings_.language : options.language;
    wparams.language = target_lang.c_str();
    if (!options.prompt.empty()) wparams.initial_prompt = options.prompt.c_str();
    wparams.temperature = active_temp;
    if (strategy == WHISPER_SAMPLING_BEAM_SEARCH) wparams.beam_search.beam_size = active_beam_size;
    else wparams.greedy.best_of = active_best_of;
    wparams.entropy_thold = 2.40f; wparams.logprob_thold = settings_.logprob_threshold; wparams.n_threads = settings_.n_threads;
    int ret = whisper_full_with_state(ctx_, state, wparams, pcm_ptr, static_cast<int>(pcm_size));
    std::vector<TranscriptionResult> results;
    if (ret == 0) {
        const int n_segments = whisper_full_n_segments_from_state(state);
        const float MIN_AVG_TOKEN_PROB = 0.40f;
        for (int i = 0; i < n_segments; ++i) {
            const char* text = whisper_full_get_segment_text_from_state(state, i);
            const int64_t t0 = whisper_full_get_segment_t0_from_state(state, i);
            const int64_t t1 = whisper_full_get_segment_t1_from_state(state, i);
            bool speaker_turn_next = whisper_full_get_segment_speaker_turn_next_from_state(state, i);
            std::vector<TokenData> tokens;
            int n_tokens = whisper_full_n_tokens_from_state(state, i);
            double total_prob = 0.0; int valid_token_count = 0;
            for (int j = 0; j < n_tokens; ++j) {
                auto data = whisper_full_get_token_data_from_state(state, i, j);
                const char* token_text = whisper_token_to_str(ctx_, data.id);
                if (data.id >= whisper_token_eot(ctx_)) continue;
                tokens.push_back({std::string(token_text), data.p, data.t0, data.t1});
                total_prob += data.p; ++valid_token_count;
            }
            float avg_prob = (valid_token_count > 0) ? static_cast<float>(total_prob / valid_token_count) : 0.0f;
            if (avg_prob < MIN_AVG_TOKEN_PROB && valid_token_count > 0) continue;
            // ---------- PROSODY-BASED PROXY TAGS (zero-cost) ----------
            int seg_start_idx = static_cast<int>(static_cast<double>(t0) / 100.0 * 16); // 16 kHz
            int seg_end_idx   = static_cast<int>(static_cast<double>(t1) / 100.0 * 16);
            seg_start_idx = std::max(0, seg_start_idx);
            seg_end_idx   = std::min(static_cast<int>(pcm_size), seg_end_idx);
            auto pros = extract_prosody(
                std::vector<float>(pcm_ptr + seg_start_idx, pcm_ptr + seg_end_idx),
                16000);
            results.push_back({ std::string(text), target_lang, avg_prob, t0, t1, speaker_turn_next, tokens,
                                pros.gender_proxy, pros.emotion_proxy, pros.arousal, pros.valence });
        }
    } else { spdlog::error("Whisper processing failed with error code: {}", ret); }
    release_state(state);
    return results;
}