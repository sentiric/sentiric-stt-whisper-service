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
    
    // VAD Settings
    silenceThreshold: 0.02,
    silenceStart: null,
    isSpeaking: false,
    minDuration: 1500, // ms
    silenceDuration: 2500, // ms
    recordingStartTime: 0,
    
    // Recording Buffer
    recordedChunks: []
};

// ==========================================
// 2. AUDIO ENGINE (VAD + Visualizer + Recording)
// ==========================================
const AudioEngine = {
    async init() {
        try {
            state.audioContext = new (window.AudioContext || window.webkitAudioContext)({ sampleRate: 48000 });
            // Kullanıcı etkileşimi olmadan suspended başlarsa resume et
            if (state.audioContext.state === 'suspended') await state.audioContext.resume();
            
            const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
            
            state.microphone = state.audioContext.createMediaStreamSource(stream);
            state.analyser = state.audioContext.createAnalyser();
            state.analyser.fftSize = 256;
            
            // VAD Processor
            state.processor = state.audioContext.createScriptProcessor(4096, 1, 1);
            
            state.microphone.connect(state.analyser);
            state.analyser.connect(state.processor);
            state.processor.connect(state.audioContext.destination); // Pipeline tamamla
            
            state.processor.onaudioprocess = AudioEngine.processAudio;
            
            ui.updateStatus("Mikrofon Hazır");
            ui.startVisualizer();
            
        } catch (e) {
            ui.updateStatus("Mikrofon Hatası!");
            console.error(e);
            alert("Mikrofon erişimi reddedildi veya desteklenmiyor.");
        }
    },

    processAudio(e) {
        const inputData = e.inputBuffer.getChannelData(0);
        
        // 1. Visualizer için RMS hesapla
        let sum = 0;
        for (let i = 0; i < inputData.length; i++) sum += inputData[i] * inputData[i];
        const rms = Math.sqrt(sum / inputData.length);
        
        // UI Güncelleme (VAD Bar)
        ui.updateVadMeter(rms);

        // 2. Eğer kayıt yapmıyorsak işlemeyi durdur (Hands-Free kapalıysa)
        if (!state.isRecording && !state.isHandsFree) return;

        // 3. Kayıt Buffer'ına Ekle (Sadece aktif kayıt sırasında)
        if (state.isRecording) {
            state.recordedChunks.push(AudioEngine.floatTo16BitPCM(inputData));
        }

        // 4. Hands-Free VAD Mantığı
        if (state.isHandsFree) {
            if (rms > state.silenceThreshold) {
                // Konuşma algılandı
                state.silenceStart = null;
                if (!state.isSpeaking) {
                    state.isSpeaking = true;
                    ui.setVadStatus("Konuşuyor...");
                    if (!state.isRecording) AudioEngine.startRecording();
                }
            } else if (state.isSpeaking) {
                // Sessizlik başladı
                if (!state.silenceStart) state.silenceStart = Date.now();
                else if (Date.now() - state.silenceStart > state.silenceDuration) {
                    // Yeterince uzun süre sessiz kalındı -> Durdur ve Gönder
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

        // Çok kısa kayıtları filtrele (Tıklama sesi vb.)
        if (duration < state.minDuration) {
            console.warn(`⚠️ Kayıt çok kısa (${duration}ms). İptal edildi.`);
            return;
        }

        const wavBlob = AudioEngine.createWavBlob(state.recordedChunks);
        await NetworkEngine.upload(wavBlob, duration);
    },

    // PCM Helper
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
        
        // WAV Header
        const buffer = new ArrayBuffer(44 + result.length * 2);
        const view = new DataView(buffer);
        const sampleRate = state.audioContext.sampleRate;
        
        // RIFF chunk
        view.setUint32(0, 0x52494646, false); // 'RIFF'
        view.setUint32(4, 36 + result.length * 2, true);
        view.setUint32(8, 0x57415645, false); // 'WAVE'
        // fmt sub-chunk
        view.setUint32(12, 0x666d7420, false); // 'fmt '
        view.setUint32(16, 16, true); 
        view.setUint16(20, 1, true); 
        view.setUint16(22, 1, true); 
        view.setUint32(24, sampleRate, true);
        view.setUint32(28, sampleRate * 2, true);
        view.setUint16(32, 2, true);
        view.setUint16(34, 16, true);
        // data sub-chunk
        view.setUint32(36, 0x64617461, false); // 'data'
        view.setUint32(40, result.length * 2, true);

        // PCM Data
        const pcmView = new Int16Array(buffer, 44);
        pcmView.set(result);

        return new Blob([buffer], { type: 'audio/wav' });
    }
};

// ==========================================
// 3. NETWORK ENGINE (Backend API)
// ==========================================
const NetworkEngine = {
    async upload(blob, durationMs) {
        const formData = new FormData();
        formData.append('file', blob, 'audio.wav');
        
        const lang = $('langSelect').value;
        if (lang !== 'auto') formData.append('language', lang);

        // UI Placeholder ekle
        const segmentId = ui.addSegment('Transkribe ediliyor...', durationMs);
        
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
        // Event Listeners
        $('recordBtn').onclick = () => {
            if (state.isHandsFree) return; // Hands-free modunda manuel buton çalışmaz
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
            // Threshold çizgisini güncelle (0.1 max scale varsayımıyla)
            const pct = (state.silenceThreshold / 0.1) * 100;
            $('vadThresholdLine').style.left = `${pct}%`;
        };

        $('fileInput').onchange = (e) => {
            if (e.target.files[0]) NetworkEngine.upload(e.target.files[0], 0);
        };

        // Theme Init
        const theme = localStorage.getItem('theme') || 'dark';
        document.body.setAttribute('data-theme', theme);

        // Audio Init (İlk tıklamada)
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
        // Logaritmik skala daha doğal görünür
        const val = Math.min(rms * 1000, 100); 
        $('vadLevel').style.width = `${val}%`;
    },

    setVadStatus(text) {
        $('vadStatus').innerText = text;
    },

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

    updateStatus(text) {
        $('mainStatus').innerText = text;
    },

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

    addSegment(text, durationMs) {
        const id = 'seg-' + Date.now();
        const container = $('transcriptHistory');
        
        // Eğer empty state varsa kaldır
        if (container.querySelector('.empty-state')) container.innerHTML = '';

        const timeStr = new Date().toLocaleTimeString();
        const div = document.createElement('div');
        div.className = 'segment';
        div.id = id;
        div.innerHTML = `
            <div class="time">${timeStr}</div>
            <div class="bubble">
                <div class="content">${text}</div>
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

    updateSegment(id, text, data, isError = false) {
        const el = document.getElementById(id);
        if (!el) return;
        
        const content = el.querySelector('.content');
        content.innerText = text;
        if (isError) content.style.color = 'var(--danger)';

        if (data) {
            // Güven skorunu göster
            let prob = 0;
            // Segmentlerin ortalamasını al veya ilk segmenti kullan
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

// Start
ui.init();