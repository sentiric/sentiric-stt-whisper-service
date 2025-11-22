const $ = (id) => document.getElementById(id);

// ==========================================
// 1. CONFIG & STATE
// ==========================================
const state = {
    isRecording: false,
    isHandsFree: false,
    audioContext: null,
    analyser: null,
    microphone: null,
    processor: null,
    
    // VAD
    silenceThreshold: 0.02,
    silenceStart: null,
    isSpeaking: false,
    minDuration: 500, 
    silenceDuration: 1500, 
    recordingStartTime: 0,
    
    recordedChunks: [],
    transcripts: [] // Export için veri
};

// ==========================================
// 2. AUDIO ENGINE
// ==========================================
const AudioEngine = {
    async init() {
        try {
            state.audioContext = new (window.AudioContext || window.webkitAudioContext)();
            if (state.audioContext.state === 'suspended') await state.audioContext.resume();
            
            const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
            state.microphone = state.audioContext.createMediaStreamSource(stream);
            state.analyser = state.audioContext.createAnalyser();
            state.analyser.fftSize = 512;
            state.processor = state.audioContext.createScriptProcessor(4096, 1, 1);
            
            state.microphone.connect(state.analyser);
            state.analyser.connect(state.processor);
            state.processor.connect(state.audioContext.destination);
            
            state.processor.onaudioprocess = AudioEngine.processAudio;
            
            ui.updateStatus("HAZIR", "success");
            ui.startVisualizer();
            
        } catch (e) {
            ui.updateStatus("MİKROFON YOK", "error");
            console.error(e);
            alert("Mikrofon erişimi sağlanamadı.");
        }
    },

    processAudio(e) {
        const inputData = e.inputBuffer.getChannelData(0);
        let sum = 0;
        for (let i = 0; i < inputData.length; i++) sum += inputData[i] * inputData[i];
        const rms = Math.sqrt(sum / inputData.length);
        
        ui.updateVadMeter(rms);

        if (!state.isRecording && !state.isHandsFree) return;

        if (state.isRecording) {
            state.recordedChunks.push(AudioEngine.floatTo16BitPCM(inputData));
        }

        if (state.isHandsFree) {
            if (rms > state.silenceThreshold) {
                state.silenceStart = null;
                if (!state.isSpeaking) {
                    state.isSpeaking = true;
                    ui.updateStatus("SES ALGILANDI", "warning");
                    if (!state.isRecording) AudioEngine.startRecording();
                }
            } else if (state.isSpeaking) {
                if (!state.silenceStart) state.silenceStart = Date.now();
                else if (Date.now() - state.silenceStart > state.silenceDuration) {
                    console.log("🤫 Sessizlik. Kayıt bitiyor.");
                    AudioEngine.stopRecording();
                }
            }
        }
    },

    startRecording() {
        if (state.isRecording) return;
        state.isRecording = true;
        state.recordedChunks = [];
        state.recordingStartTime = Date.now();
        state.silenceStart = null;
        state.isSpeaking = false;
        ui.setRecordingState(true);
    },

    async stopRecording() {
        if (!state.isRecording) return;
        
        const duration = Date.now() - state.recordingStartTime;
        state.isRecording = false;
        state.isSpeaking = false;
        ui.setRecordingState(false);

        if (duration < state.minDuration) {
            ui.updateStatus("KISA KAYIT - İPTAL", "error");
            return;
        }

        const wavBlob = AudioEngine.createWavBlob(state.recordedChunks);
        await NetworkEngine.upload(wavBlob, duration);
    },

    floatTo16BitPCM(input) {
        const output = new Int16Array(input.length);
        for (let i = 0; i < input.length; i++) {
            let s = Math.max(-1, Math.min(1, input[i]));
            output[i] = s < 0 ? s * 0x8000 : s * 0x7FFF;
        }
        return output;
    },

    createWavBlob(chunks) {
        const totalLength = chunks.reduce((acc, c) => acc + c.length, 0);
        const result = new Int16Array(totalLength);
        let offset = 0;
        for (const chunk of chunks) {
            result.set(chunk, offset);
            offset += chunk.length;
        }
        
        const buffer = new ArrayBuffer(44 + result.length * 2);
        const view = new DataView(buffer);
        const sampleRate = state.audioContext.sampleRate;
        
        view.setUint32(0, 0x52494646, false); 
        view.setUint32(4, 36 + result.length * 2, true);
        view.setUint32(8, 0x57415645, false); 
        view.setUint32(12, 0x666d7420, false); 
        view.setUint16(20, 1, true); 
        view.setUint16(22, 1, true); 
        view.setUint32(24, sampleRate, true);
        view.setUint32(28, sampleRate * 2, true);
        view.setUint16(32, 2, true);
        view.setUint16(34, 16, true);
        view.setUint32(36, 0x64617461, false); 
        view.setUint32(40, result.length * 2, true);

        const pcmView = new Int16Array(buffer, 44);
        pcmView.set(result);

        return new Blob([buffer], { type: 'audio/wav' });
    }
};

// ==========================================
// 3. NETWORK ENGINE
// ==========================================
const NetworkEngine = {
    async upload(blob, durationMs) {
        const formData = new FormData();
        formData.append('file', blob, 'audio.wav');
        
        // --- PRO FEATURES MAPPING ---
        const lang = $('langSelect').value;
        if (lang !== 'auto') formData.append('language', lang);
        
        const prompt = $('promptInput').value.trim();
        if (prompt) formData.append('prompt', prompt);

        // Advanced Settings
        formData.append('translate', $('translateToggle').checked);
        formData.append('diarization', $('diarizationToggle').checked);
        formData.append('temperature', $('tempRange').value);
        formData.append('beam_size', $('beamRange').value);
        // ----------------------------

        const audioUrl = URL.createObjectURL(blob);
        const segmentId = ui.addSegment('Transkribe ediliyor...', durationMs, audioUrl);
        ui.updateStatus("İŞLENİYOR...", "warning");

        try {
            const startTime = Date.now();
            const res = await fetch('/v1/transcribe', { method: 'POST', body: formData });
            const processTime = Date.now() - startTime;
            const data = await res.json();
            
            if (res.ok) {
                ui.updateSegment(segmentId, data.text, data);
                ui.updateTelemetry(durationMs, processTime, data);
                ui.updateStatus("TAMAMLANDI", "success");
                
                // Export için sakla
                state.transcripts.push({
                    text: data.text,
                    start: 0, // Gerçek bir zaman çizelgesi için bu geliştirilmeli
                    end: durationMs / 1000,
                    raw: data
                });

            } else {
                ui.updateSegment(segmentId, `❌ Hata: ${data.error}`, null, true);
                ui.updateStatus("HATA", "error");
            }
        } catch (e) {
            ui.updateSegment(segmentId, `❌ Ağ Hatası: ${e.message}`, null, true);
            ui.updateStatus("AĞ HATASI", "error");
        }
    },

    async checkHealth() {
        try {
            const res = await fetch('/health');
            const data = await res.json();
            ui.setConnectionStatus(data.model_ready);
        } catch {
            ui.setConnectionStatus(false);
        }
    }
};

// ==========================================
// 4. UI CONTROLLER
// ==========================================
const ui = {
    init() {
        // --- EVENT LISTENERS ---
        $('recordBtn').onclick = () => {
            if (state.isHandsFree) return; 
            if (state.isRecording) AudioEngine.stopRecording();
            else AudioEngine.startRecording();
        };

        $('handsFreeToggle').onchange = (e) => {
            state.isHandsFree = e.target.checked;
            if (!state.isHandsFree && state.isRecording) AudioEngine.stopRecording();
            ui.updateStatus(state.isHandsFree ? "OTOMATİK MOD" : "MANUEL MOD", "info");
        };

        $('vadRange').oninput = (e) => {
            state.silenceThreshold = parseFloat(e.target.value);
            $('vadVal').innerText = state.silenceThreshold;
            const pct = (state.silenceThreshold / 0.1) * 100;
            $('vadThresholdLine').style.left = `${pct}%`;
        };

        $('tempRange').oninput = (e) => $('tempVal').innerText = e.target.value;
        $('beamRange').oninput = (e) => $('beamVal').innerText = e.target.value;

        $('fileInput').onchange = (e) => {
            if (e.target.files[0]) NetworkEngine.upload(e.target.files[0], 0);
        };

        // Keyboard Shortcuts
        document.addEventListener('keydown', (e) => {
            if (e.code === 'Space' && e.target.tagName !== 'TEXTAREA') {
                e.preventDefault();
                $('recordBtn').click();
            }
        });

        // Mobile Menu
        $('menuToggle').onclick = () => {
            $('sidebar').classList.add('active');
            $('overlay').classList.add('active');
        };
        $('closeMenu').onclick = $('overlay').onclick = () => {
            $('sidebar').classList.remove('active');
            $('overlay').classList.remove('active');
        };

        // Initialize Audio on Interaction
        document.body.addEventListener('click', () => {
            if (!state.audioContext) AudioEngine.init();
        }, { once: true });

        setInterval(NetworkEngine.checkHealth, 5000);
    },

    toggleTheme() {
        const current = document.body.getAttribute('data-theme');
        const next = current === 'dark' ? 'light' : 'dark';
        document.body.setAttribute('data-theme', next);
    },

    updateVadMeter(rms) {
        const val = Math.min(rms * 1000, 100); 
        $('vadLevel').style.width = `${val}%`;
    },

    setRecordingState(isRecording) {
        const btn = $('recordBtn');
        if (isRecording) {
            btn.classList.add('recording');
            btn.innerHTML = '<i class="fas fa-square"></i>'; // Stop icon
            ui.updateStatus("KAYDEDİYOR...", "error");
        } else {
            btn.classList.remove('recording');
            btn.innerHTML = '<i class="fas fa-microphone"></i>';
            ui.updateStatus("HAZIR", "success");
        }
    },

    updateStatus(text, type = "info") { 
        const el = $('mainStatus');
        el.innerText = text;
        el.style.color = type === 'error' ? 'var(--danger)' : 
                         type === 'success' ? 'var(--primary)' : 'var(--text-muted)';
    },

    setConnectionStatus(isOnline) {
        const el = $('connStatus').querySelector('.indicator');
        const txt = $('connText');
        const meta = $('modelMeta');
        
        if (isOnline) {
            el.style.backgroundColor = 'var(--primary)';
            el.style.boxShadow = '0 0 8px var(--primary)';
            txt.innerText = "ONLINE";
            meta.innerText = "READY";
            meta.style.color = "var(--primary)";
        } else {
            el.style.backgroundColor = 'var(--danger)';
            el.style.boxShadow = 'none';
            txt.innerText = "OFFLINE";
            meta.innerText = "DISCONNECTED";
            meta.style.color = "var(--danger)";
        }
    },

    addSegment(text, durationMs, audioUrl) {
        const id = 'seg-' + Date.now();
        const container = $('transcriptHistory');
        if (container.querySelector('.empty-state')) container.innerHTML = '';

        const timeStr = new Date().toLocaleTimeString();
        const div = document.createElement('div');
        div.className = 'segment';
        div.id = id;
        
        div.innerHTML = `
            <div class="time">${timeStr}</div>
            <div class="bubble">
                <div class="content">${text}</div>
                ${audioUrl ? `<audio class="audio-player" src="${audioUrl}" controls style="width:100%; margin-top:10px;"></audio>` : ''}
                <div class="meta">
                    <span class="tag"><i class="fas fa-clock"></i> ${durationMs > 0 ? (durationMs/1000).toFixed(1)+'s' : '...'}</span>
                    <span class="tag prob-tag">Calculating...</span>
                </div>
            </div>
        `;
        container.appendChild(div);
        container.scrollTop = container.scrollHeight;
        return id;
    },

    updateSegment(id, text, data, isError = false) {
        const el = document.getElementById(id);
        if (!el) return;
        
        const content = el.querySelector('.content');
        content.innerText = text;
        if (isError) content.style.color = 'var(--danger)';

        if (data) {
            let prob = data.segments && data.segments.length > 0 ? data.segments[0].probability : 0;
            el.querySelector('.prob-tag').innerText = `%${(prob * 100).toFixed(0)} Güven`;
            
            // Diarization Visuals
            if (data.segments) {
                data.segments.forEach(seg => {
                    if (seg.speaker_turn_next) {
                        const change = document.createElement('div');
                        change.className = 'speaker-change';
                        change.innerHTML = '<span>🗣️ SPEAKER CHANGE</span>';
                        el.parentNode.insertBefore(change, el.nextSibling);
                    }
                });
            }
        }
    },

    updateTelemetry(audioDur, procTime, data) {
        if (audioDur > 0) $('metaDuration').innerText = (audioDur/1000).toFixed(2) + 's';
        $('metaProcess').innerText = (procTime/1000).toFixed(2) + 's';
        
        if (data.meta) {
            if (data.meta.rtf) {
                const speed = (1.0 / data.meta.rtf).toFixed(1);
                $('metaSpeed').innerText = `${speed}x`;
            }
            if (data.meta.input_sr) $('metaSR').innerText = data.meta.input_sr + "Hz";
            if (data.meta.input_channels) $('metaChan').innerText = data.meta.input_channels === 1 ? "Mono" : "Stereo";
        }
        $('jsonLog').innerText = JSON.stringify(data, null, 2);
    },

    startVisualizer() {
        const canvas = $('waveCanvas');
        const ctx = canvas.getContext('2d');
        const w = canvas.width;
        const h = canvas.height;
        const dataArray = new Uint8Array(state.analyser.frequencyBinCount);

        const draw = () => {
            requestAnimationFrame(draw);
            state.analyser.getByteTimeDomainData(dataArray);
            ctx.fillStyle = getComputedStyle(document.body).getPropertyValue('--bg-panel').trim();
            ctx.fillRect(0, 0, w, h);
            ctx.lineWidth = 2;
            ctx.strokeStyle = getComputedStyle(document.body).getPropertyValue('--primary').trim();
            ctx.beginPath();
            const sliceWidth = w * 1.0 / dataArray.length;
            let x = 0;
            for (let i = 0; i < dataArray.length; i++) {
                const v = dataArray[i] / 128.0;
                const y = v * h / 2;
                if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
                x += sliceWidth;
            }
            ctx.lineTo(w, h / 2);
            ctx.stroke();
        };
        draw();
    },

    clearChat() {
        $('transcriptHistory').innerHTML = `
            <div class="empty-state">
                <div class="empty-icon"><i class="fas fa-microphone-alt"></i></div>
                <h1>Omni-Studio Hazır</h1>
                <p>Mikrofonu açın.</p>
            </div>
        `;
        state.transcripts = [];
    },

    exportTranscript(format) {
        if (state.transcripts.length === 0) {
            alert("Dışa aktarılacak veri yok.");
            return;
        }
        let content = "";
        let mime = "text/plain";

        if (format === 'json') {
            content = JSON.stringify(state.transcripts, null, 2);
            mime = "application/json";
        } else if (format === 'txt') {
            content = state.transcripts.map(t => t.text).join("\n\n");
        } else if (format === 'srt') {
            state.transcripts.forEach((t, i) => {
                content += `${i+1}\n00:00:00,000 --> 00:00:00,000\n${t.text}\n\n`;
            });
            // Not: Gerçek SRT için zaman damgası mantığı eklenmelidir.
        }

        const blob = new Blob([content], {type: mime});
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `transcript_${Date.now()}.${format}`;
        a.click();
    }
};

ui.init();