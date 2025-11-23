// ==========================================
// SENTIRIC OMNI-STUDIO v2.2 CORE (Refined)
// ==========================================

const $ = (s) => document.querySelector(s);
const $$ = (s) => document.querySelectorAll(s);

// --- STATE MANAGEMENT ---
const state = {
    isRecording: false,
    isHandsFree: false,
    startTime: 0,
    timerInterval: null,
    audioContext: null,
    analyser: null,
    silenceStart: null,
    isSpeaking: false,
    chunks: [],
    
    // Config
    vadThreshold: 0.02,
    silenceDuration: 1500, // 1.5s
    
    // UI State
    lastSpeaker: null,
    conversationId: 0
};

// --- AUDIO ENGINE ---
const AudioEngine = {
    async init() {
        try {
            state.audioContext = new (window.AudioContext || window.webkitAudioContext)();
            const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
            
            const source = state.audioContext.createMediaStreamSource(stream);
            state.analyser = state.audioContext.createAnalyser();
            state.analyser.fftSize = 256;
            
            const processor = state.audioContext.createScriptProcessor(4096, 1, 1);
            
            source.connect(state.analyser);
            state.analyser.connect(processor);
            processor.connect(state.audioContext.destination);
            
            processor.onaudioprocess = (e) => AudioEngine.process(e);
            
            ui.startVisualizer();
            ui.updateStatus("Hazır", "ready");
            
        } catch (e) {
            console.error(e);
            alert("Mikrofon erişimi reddedildi!");
        }
    },

    process(e) {
        const input = e.inputBuffer.getChannelData(0);
        
        // 1. RMS Hesapla
        let sum = 0;
        for (let i = 0; i < input.length; i++) sum += input[i] * input[i];
        const rms = Math.sqrt(sum / input.length);
        
        // 2. Kayıt Mantığı
        if (state.isRecording) {
            state.chunks.push(AudioEngine.floatTo16Bit(input));
        }

        // 3. VAD / Hands-Free
        if (state.isHandsFree) {
            if (rms > state.vadThreshold) {
                state.silenceStart = null;
                if (!state.isSpeaking) {
                    state.isSpeaking = true;
                    if (!state.isRecording) ui.toggleRecord();
                }
            } else if (state.isSpeaking) {
                if (!state.silenceStart) state.silenceStart = Date.now();
                else if (Date.now() - state.silenceStart > state.silenceDuration) {
                    state.isSpeaking = false;
                    if (state.isRecording) ui.toggleRecord();
                }
            }
        }
    },

    floatTo16Bit(input) {
        const output = new Int16Array(input.length);
        for (let i = 0; i < input.length; i++) {
            let s = Math.max(-1, Math.min(1, input[i]));
            output[i] = s < 0 ? s * 0x8000 : s * 0x7FFF;
        }
        return output;
    },

    createWavBlob() {
        const totalLen = state.chunks.reduce((acc, c) => acc + c.length, 0);
        const result = new Int16Array(totalLen);
        let offset = 0;
        for (const c of state.chunks) { result.set(c, offset); offset += c.length; }
        
        // WAV Header (Mono, 16-bit, 44.1/48kHz)
        const buffer = new ArrayBuffer(44 + result.length * 2);
        const view = new DataView(buffer);
        const rate = state.audioContext.sampleRate;
        
        const writeString = (v, o, s) => { for(let i=0;i<s.length;i++) v.setUint8(o+i, s.charCodeAt(i)); };
        
        writeString(view, 0, 'RIFF');
        view.setUint32(4, 36 + result.length * 2, true);
        writeString(view, 8, 'WAVE');
        writeString(view, 12, 'fmt ');
        view.setUint32(16, 16, true);
        view.setUint16(20, 1, true);
        view.setUint16(22, 1, true);
        view.setUint32(24, rate, true);
        view.setUint32(28, rate * 2, true);
        view.setUint16(32, 2, true);
        view.setUint16(34, 16, true);
        writeString(view, 36, 'data');
        view.setUint32(40, result.length * 2, true);
        
        const pcm = new Int16Array(buffer, 44);
        pcm.set(result);
        
        return new Blob([buffer], { type: 'audio/wav' });
    }
};

// --- NETWORK LAYER ---
const API = {
    async transcribe(blob, durationMs) {
        const fd = new FormData();
        fd.append('file', blob);
        fd.append('language', $('#langSelect').value);
        if ($('#promptInput').value) fd.append('prompt', $('#promptInput').value);
        fd.append('translate', $('#translateToggle').checked);
        fd.append('diarization', $('#diarizationToggle').checked);
        fd.append('temperature', $('#tempRange').value);

        // UI Loading
        const tempId = ui.addTempMessage();

        try {
            const t0 = Date.now();
            const res = await fetch('/v1/transcribe', { method: 'POST', body: fd });
            const data = await res.json();
            const processTime = Date.now() - t0;

            ui.removeElement(tempId);
            
            if (res.ok) {
                const url = URL.createObjectURL(blob);
                ui.renderResult(data, durationMs, url);
                ui.updateMetrics(durationMs, processTime, data);
            } else {
                alert("Hata: " + (data.error || "Sunucu hatası"));
            }
        } catch (e) {
            ui.removeElement(tempId);
            alert("Ağ Hatası: " + e.message);
        }
    }
};

// --- UI CONTROLLER ---
const ui = {
    init() {
        // Event Listeners
        $('#menuToggle').onclick = () => ui.toggleSidebar('left');
        $('#telemetryToggle').onclick = () => ui.toggleSidebar('right');
        $$('.close-sidebar').forEach(b => b.onclick = () => ui.closeSidebars());
        $('#backdrop').onclick = () => ui.closeSidebars();
        
        $('#recordBtn').onclick = () => ui.toggleRecord();
        $('#handsFreeToggleBtn').onclick = () => {
            state.isHandsFree = !state.isHandsFree;
            $('#handsFreeToggleBtn').classList.toggle('active');
        };

        // Range inputs
        $('#tempRange').oninput = (e) => $('#tempVal').innerText = e.target.value;
        
        // File Upload
        $('#fileInput').onchange = (e) => {
            if(e.target.files[0]) API.transcribe(e.target.files[0], 0);
        };

        // Audio Context Start on Interaction
        document.body.onclick = () => { if(!state.audioContext) AudioEngine.init(); };
    },

    toggleRecord() {
        if (!state.audioContext) AudioEngine.init();
        
        if (state.isRecording) {
            // Stop
            state.isRecording = false;
            clearInterval(state.timerInterval);
            $('#recordBtn').classList.remove('recording');
            const duration = Date.now() - state.startTime;
            if (duration > 500) {
                const blob = AudioEngine.createWavBlob();
                API.transcribe(blob, duration);
            }
        } else {
            // Start
            state.chunks = [];
            state.isRecording = true;
            state.startTime = Date.now();
            $('#recordBtn').classList.add('recording');
            
            // Timer Logic
            state.timerInterval = setInterval(() => {
                const s = Math.floor((Date.now() - state.startTime) / 1000);
                const m = Math.floor(s / 60);
                const ss = s % 60;
                $('#recordTimer').innerText = `${m.toString().padStart(2,'0')}:${ss.toString().padStart(2,'0')}`;
            }, 1000);
        }
    },

    // --- RENDER LOGIC (CRITICAL UPDATE) ---
    renderResult(data, durationMs, audioUrl) {
        const container = $('#transcriptContainer');
        if ($('.empty-placeholder')) $('.empty-placeholder')?.remove();

        // MERGE STRATEGY: Aynı konuşmacının ardışık segmentlerini birleştir
        let mergedSegments = [];
        let currentSpeaker = -1;
        let bufferText = "";
        let tStart = 0;

        // Backend'den gelen 'speaker_turn_next'i kullanarak ID üret
        let speakerIdCounter = 0;

        (data.segments || []).forEach((seg, index) => {
            // Basit mantık: Her speaker_turn_next true olduğunda ID değişir
            let text = seg.text.trim();
            
            if (currentSpeaker === -1) {
                currentSpeaker = speakerIdCounter;
                tStart = seg.start;
            }

            bufferText += " " + text;

            if (seg.speaker_turn_next || index === data.segments.length - 1) {
                mergedSegments.push({
                    speaker: currentSpeaker,
                    text: bufferText.trim(),
                    start: tStart,
                    end: seg.end,
                    colorHash: (currentSpeaker % 2) // 0 veya 1 (İki renk teması)
                });
                bufferText = "";
                tStart = seg.end; // Bir sonraki başlangıç
                speakerIdCounter++;
                currentSpeaker = speakerIdCounter;
            }
        });

        // Eğer hiç segment yoksa (Sessiz)
        if (mergedSegments.length === 0 && data.text) {
             mergedSegments.push({ speaker: 0, text: data.text, start: 0, end: durationMs/1000, colorHash: 0 });
        }

        // HTML Oluştur
        const blockId = `block-${Date.now()}`;
        const div = document.createElement('div');
        div.className = 'timeline-block';
        div.id = blockId;

        mergedSegments.forEach(seg => {
            const isAlt = seg.colorHash === 1;
            const speakerLabel = isAlt ? "KONUŞMACI 2" : "KONUŞMACI 1";
            const avatarChar = isAlt ? "K2" : "K1";
            const colorClass = isAlt ? "var(--accent)" : "var(--primary)";
            
            div.innerHTML += `
                <div class="timeline-item ${isAlt ? 'alt' : ''}">
                    <div class="speaker-avatar" style="color: ${colorClass}; border-color: ${colorClass}">
                        ${avatarChar}
                    </div>
                    <div class="speaker-content">
                        <div class="speaker-header">
                            <span class="speaker-name" style="color:${colorClass}">${speakerLabel}</span>
                            <span class="time-stamp">${seg.start.toFixed(1)}s - ${seg.end.toFixed(1)}s</span>
                        </div>
                        <div class="text-block">
                            ${seg.text}
                        </div>
                    </div>
                </div>
            `;
        });

        // Custom Compact Player Ekle (En alta)
        if (audioUrl) {
            const playerHtml = `
                <div class="timeline-item">
                    <div class="speaker-avatar" style="opacity:0"></div>
                    <div class="speaker-content">
                         <div class="compact-player">
                            <i class="fas fa-play play-icon" onclick="ui.playAudio(this, '${audioUrl}')"></i>
                            <div class="progress-bar"><div class="progress-fill"></div></div>
                            <span class="dur-text">${(durationMs/1000).toFixed(1)}s</span>
                        </div>
                    </div>
                </div>
            `;
            div.innerHTML += playerHtml;
        }

        container.appendChild(div);
        container.scrollTop = container.scrollHeight;
    },

    playAudio(btn, url) {
        // Mevcut çalan varsa durdur (Basitlik için global bir audio objesi kullanabiliriz)
        if (window.currentAudio) {
            window.currentAudio.pause();
            if(window.currentBtn) window.currentBtn.className = "fas fa-play play-icon";
        }

        if (btn.className.includes('fa-pause')) {
            // Zaten çalıyor, durdurduk
            return;
        }

        const audio = new Audio(url);
        window.currentAudio = audio;
        window.currentBtn = btn;

        btn.className = "fas fa-pause play-icon";
        const bar = btn.parentElement.querySelector('.progress-fill');

        audio.play();
        audio.ontimeupdate = () => {
            const pct = (audio.currentTime / audio.duration) * 100;
            bar.style.width = `${pct}%`;
        };
        audio.onended = () => {
            btn.className = "fas fa-play play-icon";
            bar.style.width = "0%";
        };
    },

    // --- VISUALIZERS & UTILS ---
    startVisualizer() {
        const canvas = $('#visualizerCanvas');
        const ctx = canvas.getContext('2d');
        const bufferLen = state.analyser.frequencyBinCount;
        const dataArray = new Uint8Array(bufferLen);

        const draw = () => {
            requestAnimationFrame(draw);
            // Boyut güncelleme (Responsive canvas)
            canvas.width = canvas.parentElement.offsetWidth;
            canvas.height = canvas.parentElement.offsetHeight;

            state.analyser.getByteFrequencyData(dataArray);
            ctx.clearRect(0, 0, canvas.width, canvas.height);

            const barWidth = (canvas.width / bufferLen) * 2.5;
            let x = 0;

            for (let i = 0; i < bufferLen; i++) {
                const barHeight = dataArray[i] / 2; // Yüksekliği ölçekle
                ctx.fillStyle = `rgba(0, 229, 153, ${barHeight / 255})`; // Opaklık sese göre
                ctx.fillRect(x, canvas.height - barHeight, barWidth, barHeight);
                x += barWidth + 1;
            }
        };
        draw();
    },

    addTempMessage() {
        const id = 'tmp-' + Date.now();
        const container = $('#transcriptContainer');
        const div = document.createElement('div');
        div.id = id;
        div.innerHTML = `
            <div class="timeline-item" style="opacity:0.6; margin-top:20px;">
                <div class="speaker-avatar"><i class="fas fa-circle-notch fa-spin"></i></div>
                <div class="speaker-content">
                    <div class="text-block">Ses verisi işleniyor, lütfen bekleyin...</div>
                </div>
            </div>`;
        container.appendChild(div);
        container.scrollTop = container.scrollHeight;
        return id;
    },

    removeElement(id) { document.getElementById(id)?.remove(); },

    updateMetrics(dur, proc, data) {
        $('#durVal').innerText = (dur/1000).toFixed(2) + 's';
        $('#procVal').innerText = (proc/1000).toFixed(2) + 's';
        
        let rtf = 0;
        if (data.meta && data.meta.rtf) rtf = (1 / data.meta.rtf).toFixed(1);
        $('#rtfVal').innerText = rtf + 'x';
        
        $('#jsonLog').innerText = JSON.stringify(data, null, 2);
    },

    toggleSidebar(side) {
        const el = side === 'left' ? $('#leftSidebar') : $('#rightSidebar');
        el.classList.add('active');
        $('#backdrop').classList.add('active');
    },

    closeSidebars() {
        $$('.sidebar').forEach(s => s.classList.remove('active'));
        $('#backdrop').classList.remove('active');
    },

    toggleTheme() {
        const b = document.body;
        b.setAttribute('data-theme', b.getAttribute('data-theme') === 'dark' ? 'light' : 'dark');
    },

    clearChat() { $('#transcriptContainer').innerHTML = ''; },
    
    exportTranscript(fmt) {
        // ... (Mevcut export mantığı aynen kalabilir veya geliştirilebilir)
        alert("Export: " + fmt);
    },
    
    updateStatus(txt) { $('#connStatus span').innerText = txt; }
};

// Start App
ui.init();