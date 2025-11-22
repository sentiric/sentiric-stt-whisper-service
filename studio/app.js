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
    visualizerFrame: null,
    
    silenceThreshold: 0.02,
    silenceStart: null,
    isSpeaking: false,
    minDuration: 1000, 
    silenceDuration: 2000, 
    recordingStartTime: 0,
    
    recordedChunks: []
};

// ==========================================
// 2. AUDIO ENGINE
// ==========================================
const AudioEngine = {
    async init() {
        try {
            state.audioContext = new (window.AudioContext || window.webkitAudioContext)(); // SampleRate otomatiğe bırakıldı
            if (state.audioContext.state === 'suspended') await state.audioContext.resume();
            
            const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
            
            state.microphone = state.audioContext.createMediaStreamSource(stream);
            state.analyser = state.audioContext.createAnalyser();
            state.analyser.fftSize = 256;
            
            state.processor = state.audioContext.createScriptProcessor(4096, 1, 1);
            
            state.microphone.connect(state.analyser);
            state.analyser.connect(state.processor);
            state.processor.connect(state.audioContext.destination);
            
            state.processor.onaudioprocess = AudioEngine.processAudio;
            
            ui.updateStatus("Mikrofon Hazır");
            ui.startVisualizer();
            
        } catch (e) {
            ui.updateStatus("Mikrofon Hatası!");
            console.error(e);
            alert("Mikrofon erişimi reddedildi.");
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
                    ui.setVadStatus("Konuşuyor...");
                    if (!state.isRecording) AudioEngine.startRecording();
                }
            } else if (state.isSpeaking) {
                if (!state.silenceStart) state.silenceStart = Date.now();
                else if (Date.now() - state.silenceStart > state.silenceDuration) {
                    console.log("🤫 Sessizlik algılandı. Gönderiliyor...");
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
            console.warn(`⚠️ Kayıt çok kısa (${duration}ms). İptal edildi.`);
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
        view.setUint32(16, 16, true); 
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
        
        const lang = $('langSelect').value;
        if (lang !== 'auto') formData.append('language', lang);

        const prompt = $('promptInput') ? $('promptInput').value.trim() : "";
        if (prompt) formData.append('prompt', prompt);

        // YENİ: Blob URL oluştur (Playback için)
        const audioUrl = URL.createObjectURL(blob);

        // UI Placeholder (Audio URL ile birlikte)
        const segmentId = ui.addSegment('Transkribe ediliyor...', durationMs, audioUrl);
        
        try {
            const startTime = Date.now();
            const res = await fetch('/v1/audio/transcriptions', {
                method: 'POST',
                body: formData
            });
            const processTime = Date.now() - startTime;
            const data = await res.json();
            
            if (res.ok) {
                ui.updateSegment(segmentId, data.text, data);
                ui.updateDashboard(durationMs, processTime, data);
                
                // Diarization Visualization
                if (data.segments) {
                    data.segments.forEach(seg => {
                        if (seg.speaker_turn_next) {
                            ui.addSpeakerChangeInfo(segmentId);
                        }
                    });
                }

            } else {
                ui.updateSegment(segmentId, `❌ Hata: ${data.error}`, null, true);
            }
        } catch (e) {
            ui.updateSegment(segmentId, `❌ Ağ Hatası: ${e.message}`, null, true);
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
        $('recordBtn').onclick = () => {
            if (state.isHandsFree) return; 
            if (state.isRecording) AudioEngine.stopRecording();
            else AudioEngine.startRecording();
        };

        $('handsFreeToggle').onchange = (e) => {
            state.isHandsFree = e.target.checked;
            if (!state.isHandsFree && state.isRecording) AudioEngine.stopRecording();
            ui.updateStatus(state.isHandsFree ? "Hands-Free Aktif" : "Manuel Mod");
        };

        $('vadRange').oninput = (e) => {
            state.silenceThreshold = parseFloat(e.target.value);
            $('vadVal').innerText = state.silenceThreshold;
            const pct = (state.silenceThreshold / 0.1) * 100;
            $('vadThresholdLine').style.left = `${pct}%`;
        };

        $('fileInput').onchange = (e) => {
            if (e.target.files[0]) {
                // Dosya yüklemelerinde de audio player için URL oluştur
                // Ancak NetworkEngine.upload zaten Blob alıyor, file bir blob'dur.
                NetworkEngine.upload(e.target.files[0], 0);
            }
        };

        const theme = localStorage.getItem('theme') || 'dark';
        document.body.setAttribute('data-theme', theme);

        document.body.addEventListener('click', () => {
            if (!state.audioContext) AudioEngine.init();
        }, { once: true });

        setInterval(NetworkEngine.checkHealth, 5000);
    },

    toggleTheme() {
        const current = document.body.getAttribute('data-theme');
        const next = current === 'dark' ? 'light' : 'dark';
        document.body.setAttribute('data-theme', next);
        localStorage.setItem('theme', next);
    },

    updateVadMeter(rms) {
        const val = Math.min(rms * 1000, 100); 
        $('vadLevel').style.width = `${val}%`;
    },

    setVadStatus(text) { $('vadStatus').innerText = text; },

    setRecordingState(isRecording) {
        const btn = $('recordBtn');
        if (isRecording) {
            btn.classList.add('recording');
            btn.innerHTML = '<i class="fas fa-stop"></i>';
            ui.updateStatus("KAYDEDİLİYOR...");
        } else {
            btn.classList.remove('recording');
            btn.innerHTML = '<i class="fas fa-microphone"></i>';
            ui.updateStatus("Hazır");
        }
    },

    updateStatus(text) { $('mainStatus').innerText = text; },

    setConnectionStatus(isOnline) {
        const el = $('connStatus').querySelector('.indicator');
        const txt = $('connText');
        const meta = $('modelMeta');
        
        if (isOnline) {
            el.classList.add('online');
            txt.innerText = "Online";
            meta.innerText = "READY";
            meta.style.color = "var(--primary)";
        } else {
            el.classList.remove('online');
            txt.innerText = "Offline";
            meta.innerText = "DISCONNECTED";
            meta.style.color = "var(--danger)";
        }
    },

    startVisualizer() {
        const canvas = $('waveCanvas');
        const ctx = canvas.getContext('2d');
        const w = canvas.width;
        const h = canvas.height;
        const dataArray = new Uint8Array(state.analyser.frequencyBinCount);

        const draw = () => {
            state.visualizerFrame = requestAnimationFrame(draw);
            state.analyser.getByteFrequencyData(dataArray);

            ctx.clearRect(0, 0, w, h);
            ctx.lineWidth = 2;
            ctx.strokeStyle = getComputedStyle(document.body).getPropertyValue('--primary').trim();
            ctx.beginPath();

            const sliceWidth = w * 1.0 / dataArray.length;
            let x = 0;

            for (let i = 0; i < dataArray.length; i++) {
                const v = dataArray[i] / 128.0;
                const y = v * h / 2;
                if (i === 0) ctx.moveTo(x, y);
                else ctx.lineTo(x, y);
                x += sliceWidth;
            }
            ctx.lineTo(w, h / 2);
            ctx.stroke();
        };
        draw();
    },

    // YENİ: Audio URL parametresi eklendi
    addSegment(text, durationMs, audioUrl = null) {
        const id = 'seg-' + Date.now();
        const container = $('transcriptHistory');
        
        if (container.querySelector('.empty-state')) container.innerHTML = '';

        const timeStr = new Date().toLocaleTimeString();
        const div = document.createElement('div');
        div.className = 'segment';
        div.id = id;
        
        // Audio player HTML (Varsa)
        let audioHtml = '';
        if (audioUrl) {
            audioHtml = `<audio class="audio-player" src="${audioUrl}" controls></audio>`;
        }

        div.innerHTML = `
            <div class="time">${timeStr}</div>
            <div class="bubble">
                <div class="content">${text}</div>
                ${audioHtml}
                <div class="meta">
                    <span class="tag"><i class="fas fa-clock"></i> ${durationMs > 0 ? (durationMs/1000).toFixed(1)+'s' : '...'}</span>
                    <span class="tag prob-tag">...</span>
                </div>
            </div>
        `;
        
        container.appendChild(div);
        container.scrollTop = container.scrollHeight;
        return id;
    },

    // YENİ: Konuşmacı değişikliği ekleme
    addSpeakerChangeInfo(segmentId) {
        const seg = document.getElementById(segmentId);
        if (seg) {
            const changeDiv = document.createElement('div');
            changeDiv.className = 'speaker-change';
            changeDiv.innerHTML = '🗣️ KONUŞMACI DEĞİŞİMİ';
            // Segmentten sonraya ekle
            seg.parentNode.insertBefore(changeDiv, seg.nextSibling);
        }
    },

    updateSegment(id, text, data, isError = false) {
        const el = document.getElementById(id);
        if (!el) return;
        
        const content = el.querySelector('.content');
        content.innerText = text;
        if (isError) content.style.color = 'var(--danger)';

        if (data) {
            let prob = 0;
            if (data.segments && data.segments.length > 0) {
                prob = data.segments[0].probability;
            }
            el.querySelector('.prob-tag').innerText = `%${(prob * 100).toFixed(0)} Güven`;
        }
    },

    updateDashboard(audioDur, procTime, data) {
        if (audioDur > 0) $('metaDuration').innerText = (audioDur/1000).toFixed(2) + 's';
        $('metaProcess').innerText = (procTime/1000).toFixed(2) + 's';
        
        if (data.meta && data.meta.rtf) {
            const rtf = data.meta.rtf;
            const speed = rtf > 0 ? (1.0 / rtf).toFixed(1) : 0;
            $('metaSpeed').innerText = `⚡ ${speed}x`;
        }
        
        $('jsonLog').innerText = JSON.stringify(data, null, 2);
    },

    clearChat() {
        $('transcriptHistory').innerHTML = `
            <div class="empty-state">
                <div class="empty-logo">🎙️</div>
                <h1>Ses Analiz Terminali</h1>
                <p>Mikrofonu açın veya bir ses dosyası yükleyin.</p>
            </div>
        `;
        $('metaDuration').innerText = '--';
        $('metaProcess').innerText = '--';
        $('metaSpeed').innerText = '--';
    }
};

ui.init();