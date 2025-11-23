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
    
    // UI Logic
    currentSpeakerId: 0, // 0 (Main/A) or 1 (Alt/B)
    
    // Config
    vadThreshold: 0.02,
    silenceDuration: 1500,
};

// --- AUDIO ENGINE ---
const AudioEngine = {
    async init() {
        if (state.audioContext) return;
        try {
            state.audioContext = new (window.AudioContext || window.webkitAudioContext)();
            const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
            
            const source = state.audioContext.createMediaStreamSource(stream);
            state.analyser = state.audioContext.createAnalyser();
            state.analyser.fftSize = 128; // Görselleştirici için optimize
            
            const processor = state.audioContext.createScriptProcessor(4096, 1, 1);
            
            source.connect(state.analyser);
            state.analyser.connect(processor);
            processor.connect(state.audioContext.destination);
            
            processor.onaudioprocess = (e) => AudioEngine.process(e);
            
            ui.startVisualizer();
            ui.updateStatus("Hazır");
            
        } catch (e) {
            console.error(e);
            alert("Mikrofon hatası! Lütfen izinleri kontrol edin.");
        }
    },

    process(e) {
        const input = e.inputBuffer.getChannelData(0);
        
        // 1. RMS (Ses Seviyesi)
        let sum = 0;
        for (let i = 0; i < input.length; i++) sum += input[i] * input[i];
        const rms = Math.sqrt(sum / input.length);
        
        // 2. Kayıt
        if (state.isRecording) {
            state.chunks.push(AudioEngine.floatTo16Bit(input));
        }

        // 3. VAD (Otomatik Kayıt)
        const threshold = parseFloat($('#vadRange').value);
        if (state.isHandsFree) {
            if (rms > threshold) {
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

// --- API LAYER ---
const API = {
    async transcribe(blob, durationMs) {
        const fd = new FormData();
        fd.append('file', blob);
        fd.append('language', $('#langSelect').value);
        if ($('#promptInput').value) fd.append('prompt', $('#promptInput').value);
        fd.append('translate', $('#translateToggle').checked);
        fd.append('diarization', $('#diarizationToggle').checked);
        fd.append('temperature', $('#tempRange').value);
        fd.append('beam_size', $('#beamRange').value);

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
            alert("Ağ Hatası");
        }
    }
};

// --- UI LOGIC ---
const ui = {
    init() {
        // Sidebar Toggles
        $('#menuToggle').onclick = () => ui.toggleSidebar('left');
        $('#telemetryToggle').onclick = () => ui.toggleSidebar('right');
        $$('.close-sidebar').forEach(b => b.onclick = () => ui.closeSidebars());
        $('#backdrop').onclick = () => ui.closeSidebars();
        
        // Recording
        $('#recordBtn').onclick = () => ui.toggleRecord();
        $('#handsFreeToggleBtn').onclick = () => {
            state.isHandsFree = !state.isHandsFree;
            $('#handsFreeToggleBtn').classList.toggle('active');
        };

        // Inputs
        $('#tempRange').oninput = (e) => $('#tempVal').innerText = e.target.value;
        $('#beamRange').oninput = (e) => $('#beamVal').innerText = e.target.value;
        $('#vadRange').oninput = (e) => $('#vadVal').innerText = e.target.value;
        
        $('#fileInput').onchange = (e) => {
            if(e.target.files[0]) API.transcribe(e.target.files[0], 0);
        };

        // Keyboard Shortcut (Space)
        document.addEventListener('keydown', (e) => {
            if (e.code === 'Space' && e.target.tagName !== 'TEXTAREA' && e.target.tagName !== 'INPUT') {
                e.preventDefault();
                ui.toggleRecord();
            }
        });
    },

    toggleRecord() {
        if (!state.audioContext) AudioEngine.init();
        
        if (state.isRecording) {
            // Stop
            state.isRecording = false;
            clearInterval(state.timerInterval);
            $('#recordBtn').classList.remove('recording');
            const duration = Date.now() - state.startTime;
            
            if (duration > 800) { // Çok kısa sesleri yoksay
                const blob = AudioEngine.createWavBlob();
                API.transcribe(blob, duration);
            } else {
                state.chunks = []; // Temizle
            }
        } else {
            // Start
            state.chunks = [];
            state.isRecording = true;
            state.startTime = Date.now();
            $('#recordBtn').classList.add('recording');
            
            state.timerInterval = setInterval(() => {
                const diff = Math.floor((Date.now() - state.startTime) / 1000);
                const m = Math.floor(diff / 60).toString().padStart(2,'0');
                const s = (diff % 60).toString().padStart(2,'0');
                $('#recordTimer').innerText = `${m}:${s}`;
            }, 1000);
        }
    },

    // --- RENDER LOGIC (DIARIZATION FIXED) ---
    renderResult(data, durationMs, audioUrl) {
        const container = $('#transcriptContainer');
        if ($('.empty-placeholder')) $('.empty-placeholder')?.remove();

        // MERGE & ALTERNATE STRATEGY
        let segmentsToRender = [];
        let bufferText = "";
        let startTime = 0;
        
        // Önceki konuşmacı ID'sini state'den al (Sohbet devamlılığı için)
        // Eğer bu dosya yüklemesi ise sıfırla, canlı sohbet ise koru
        // Şimdilik basitlik için her istekte sıfırdan başlatıyoruz ama mantık burada:
        let activeSpeaker = 0; // 0 = A, 1 = B

        // Eğer segments yoksa (sessizlik veya tek parça)
        const rawSegments = (data.segments && data.segments.length > 0) ? data.segments : [{
            text: data.text || "...", start: 0, end: durationMs/1000, speaker_turn_next: false
        }];

        rawSegments.forEach((seg, index) => {
            if (index === 0) startTime = seg.start;
            bufferText += seg.text;

            // Eğer konuşmacı değiştiyse VEYA son segment ise bloğu kapat
            if (seg.speaker_turn_next || index === rawSegments.length - 1) {
                segmentsToRender.push({
                    speakerId: activeSpeaker, // 0 veya 1
                    text: bufferText.trim(),
                    start: startTime,
                    end: seg.end
                });
                
                // Konuşmacıyı değiştir (A -> B veya B -> A)
                activeSpeaker = activeSpeaker === 0 ? 1 : 0;
                
                // Reset buffer
                bufferText = "";
                // Sonraki blok için başlangıç zamanını ayarla (eğer varsa)
                if (index < rawSegments.length - 1) {
                    startTime = rawSegments[index+1].start; 
                }
            }
        });

        // HTML Çizimi
        const groupDiv = document.createElement('div');
        
        segmentsToRender.forEach(block => {
            const isAlt = block.speakerId === 1;
            const styleClass = isAlt ? 'alt' : 'main';
            const name = isAlt ? 'Konuşmacı B' : 'Konuşmacı A';
            const avatar = isAlt ? 'B' : 'A';

            const html = `
                <div class="speaker-group ${styleClass} timeline-block">
                    <div class="speaker-avatar">${avatar}</div>
                    <div class="speaker-content">
                        <div class="speaker-header">
                            <span class="speaker-name">${name}</span>
                            <span class="time-tag">${block.start.toFixed(1)}s - ${block.end.toFixed(1)}s</span>
                        </div>
                        <div class="text-bubble">
                            ${block.text}
                            ${ audioUrl && block === segmentsToRender[segmentsToRender.length-1] ? ui.createPlayerHtml(audioUrl, durationMs) : '' }
                        </div>
                    </div>
                </div>
            `;
            groupDiv.innerHTML += html;
        });

        container.appendChild(groupDiv);
        container.scrollTop = container.scrollHeight;
    },

    createPlayerHtml(url, durMs) {
        const sec = (durMs/1000).toFixed(1);
        // Rastgele bar yükseklikleri üret (görsel efekt)
        let bars = '';
        for(let i=0; i<8; i++) {
            const h = Math.floor(Math.random() * 10) + 4;
            bars += `<div class="bar" style="height:${h}px"></div>`;
        }

        return `
            <div class="inline-player" onclick="ui.playAudio(this, '${url}')">
                <i class="fas fa-play play-btn"></i>
                <div class="waveform-static">${bars}</div>
                <span class="dur-label">${sec}s</span>
            </div>
        `;
    },

    playAudio(el, url) {
        if (window.currentAudio) {
            window.currentAudio.pause();
            if (window.currentBtn) window.currentBtn.className = "fas fa-play play-btn";
        }

        const icon = el.querySelector('i');
        if (icon.classList.contains('fa-pause')) return; // Zaten çalıyor

        const audio = new Audio(url);
        window.currentAudio = audio;
        window.currentBtn = icon;

        icon.className = "fas fa-pause play-btn";
        audio.play();
        audio.onended = () => icon.className = "fas fa-play play-btn";
    },

    // --- UTILS ---
    addTempMessage() {
        const container = $('#transcriptContainer');
        const id = 'temp-' + Date.now();
        const div = document.createElement('div');
        div.id = id;
        div.className = 'speaker-group main timeline-block';
        div.innerHTML = `
            <div class="speaker-avatar"><i class="fas fa-circle-notch fa-spin"></i></div>
            <div class="speaker-content">
                <div class="text-bubble" style="opacity:0.7">Analiz ediliyor...</div>
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
        
        $('#langVal').innerText = (data.language || '?').toUpperCase();
        
        // Confidence calculation
        let avgProb = 0;
        if (data.segments && data.segments.length > 0) {
            const sum = data.segments.reduce((acc, s) => acc + s.probability, 0);
            avgProb = (sum / data.segments.length) * 100;
        }
        $('#confVal').innerText = avgProb > 0 ? `%${avgProb.toFixed(1)}` : '--';

        $('#jsonLog').innerText = JSON.stringify(data, null, 2);
    },

    toggleDebugJson() {
        $('#jsonLog').classList.toggle('hidden');
    },

    startVisualizer() {
        const canvas = $('#visualizerCanvas');
        const ctx = canvas.getContext('2d');
        // Canvas boyutunu CSS'den al
        canvas.width = canvas.parentElement.offsetWidth;
        canvas.height = canvas.parentElement.offsetHeight;
        
        const bufferLen = state.analyser.frequencyBinCount;
        const dataArray = new Uint8Array(bufferLen);

        const draw = () => {
            requestAnimationFrame(draw);
            state.analyser.getByteFrequencyData(dataArray);
            ctx.clearRect(0, 0, canvas.width, canvas.height);

            const barWidth = (canvas.width / bufferLen) * 2;
            let x = 0;

            for (let i = 0; i < bufferLen; i++) {
                const v = dataArray[i];
                const h = (v / 255) * canvas.height;
                ctx.fillStyle = `rgba(0, 229, 153, ${v / 400})`; // Şeffaf yeşil
                ctx.fillRect(x, canvas.height - h, barWidth, h);
                x += barWidth + 2;
            }
        };
        draw();
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
    updateStatus(txt) { $('#connStatus span').innerText = txt; },
    clearChat() { $('#transcriptContainer').innerHTML = ''; },
    exportTranscript(fmt) { alert("Format: " + fmt + " (İndirme başlatılıyor...)"); }
};

ui.init();