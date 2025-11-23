const $ = (s) => document.querySelector(s);
const $$ = (s) => document.querySelectorAll(s);

// =============================================================================
// 🧠 CORE: SPEAKER IDENTITY MANAGER (CLIENT-SIDE CLUSTERING)
// =============================================================================
class SpeakerManager {
    constructor(threshold = 0.88) {
        this.threshold = threshold;
        this.clusters = {}; // { id: { centroid: [], count: int, name: str, color: str, stats: {} } }
        this.nextId = 0;
        // Profesyonel Renk Paleti (Data Visualization Friendly)
        this.colors = [
            "#3B82F6", "#10B981", "#F59E0B", "#EF4444", "#8B5CF6", 
            "#EC4899", "#06B6D4", "#84CC16", "#F97316", "#6366F1"
        ];
    }

    // Cosine Similarity: İki vektör arasındaki açısal benzerlik
    cosine(vecA, vecB) {
        let dot = 0, normA = 0, normB = 0;
        for (let i = 0; i < vecA.length; i++) {
            dot += vecA[i] * vecB[i];
            normA += vecA[i] * vecA[i];
            normB += vecB[i] * vecB[i];
        }
        if (normA === 0 || normB === 0) return 0;
        return dot / (Math.sqrt(normA) * Math.sqrt(normB));
    }

    // Vektörü bir kümeye ata veya yeni küme oluştur
    assign(vector, meta) {
        let bestId = null;
        let bestScore = -1;

        // Mevcut kümeleri tara
        for (const id in this.clusters) {
            const score = this.cosine(vector, this.clusters[id].centroid);
            if (score > bestScore) {
                bestScore = score;
                bestId = id;
            }
        }

        // Eşik değerini geçtiyse mevcut kümeye dahil et
        if (bestId && bestScore >= this.threshold) {
            this.updateCluster(bestId, vector, meta);
            return this.clusters[bestId];
        }

        // Eşleşme yoksa yeni küme yarat
        return this.createCluster(vector, meta);
    }

    createCluster(vector, meta) {
        const id = `spk_${this.nextId++}`;
        const color = this.colors[this.nextId % this.colors.length]; // Deterministik renk
        
        this.clusters[id] = {
            id: id,
            centroid: [...vector],
            count: 1,
            name: `Konuşmacı ${String.fromCharCode(65 + (this.nextId - 1))}`, // A, B, C...
            color: color,
            gender: meta.gender || "?",
            lastActive: Date.now()
        };
        return this.clusters[id];
    }

    updateCluster(id, vector, meta) {
        const cls = this.clusters[id];
        // Centroid güncelleme (Running Average)
        // Yeni vektörün etkisi 1/(count+1) oranındadır.
        const weight = 1.0 / (cls.count + 1);
        for (let i = 0; i < vector.length; i++) {
            cls.centroid[i] = cls.centroid[i] * (1 - weight) + vector[i] * weight;
        }
        cls.count++;
        cls.lastActive = Date.now();
        // Cinsiyet bilgisini güncelle (çoğunluk kararı gibi düşünülebilir ama şimdilik son geleni alalım)
        if (meta.gender && meta.gender !== "?") cls.gender = meta.gender;
    }

    rename(id, newName) {
        if (this.clusters[id]) {
            this.clusters[id].name = newName;
            return true;
        }
        return false;
    }

    reset() {
        this.clusters = {};
        this.nextId = 0;
    }
}

// Global Instance
const speakerMgr = new SpeakerManager(0.85); // Benzerlik eşiği

// =============================================================================
// 🎤 AUDIO ENGINE: VAD & CAPTURE
// =============================================================================
const AudioEngine = {
    ctx: null,
    analyser: null,
    scriptNode: null,
    chunks: [],
    isRecording: false,
    
    async init() {
        if (this.ctx) return;
        this.ctx = new (window.AudioContext || window.webkitAudioContext)();
        const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
        const source = this.ctx.createMediaStreamSource(stream);
        
        this.analyser = this.ctx.createAnalyser();
        this.analyser.fftSize = 256;
        this.analyser.smoothingTimeConstant = 0.5;

        this.scriptNode = this.ctx.createScriptProcessor(4096, 1, 1);
        
        source.connect(this.analyser);
        this.analyser.connect(this.scriptNode);
        this.scriptNode.connect(this.ctx.destination);

        this.scriptNode.onaudioprocess = (e) => this.process(e);
        ui.startVisualizer();
    },

    process(e) {
        const input = e.inputBuffer.getChannelData(0);
        
        // VAD Logic (RMS Calculation)
        let sum = 0;
        for (let i = 0; i < input.length; i++) sum += input[i] * input[i];
        const rms = Math.sqrt(sum / input.length);

        // Kayıt aktifse buffer'a ekle
        if (this.isRecording) {
            this.chunks.push(this.floatTo16Bit(input));
        }

        // Hands-Free Logic
        if (state.isHandsFree) {
            const threshold = parseFloat($('#vadRange').value);
            if (rms > threshold) {
                state.lastSpeechTime = Date.now();
                if (!state.isSpeaking) {
                    state.isSpeaking = true;
                    if (!this.isRecording) ui.toggleRecord();
                }
            } else if (state.isSpeaking) {
                const silenceDur = Date.now() - state.lastSpeechTime;
                if (silenceDur > 1500) { // 1.5s sessizlik
                    state.isSpeaking = false;
                    if (this.isRecording) ui.toggleRecord();
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
        const totalLen = this.chunks.reduce((acc, c) => acc + c.length, 0);
        const result = new Int16Array(totalLen);
        let offset = 0;
        for (const c of this.chunks) {
            result.set(c, offset);
            offset += c.length;
        }
        
        const buffer = new ArrayBuffer(44 + result.length * 2);
        const view = new DataView(buffer);
        const writeString = (v, o, s) => { for (let i = 0; i < s.length; i++) view.setUint8(o + i, s.charCodeAt(i)); };
        const rate = this.ctx.sampleRate;

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

// =============================================================================
// 🌐 API CLIENT
// =============================================================================
const API = {
    async transcribe(blob, durationMs) {
        const fd = new FormData();
        fd.append('file', blob);
        fd.append('language', $('#langSelect').value);
        fd.append('prompt', $('#promptInput').value);
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
            console.error(e);
            alert("Bağlantı Hatası");
        }
    }
};

// =============================================================================
// 🎨 UI CONTROLLER
// =============================================================================
const state = {
    isHandsFree: false,
    lastSpeechTime: 0,
    isSpeaking: false,
    startTime: 0,
    timer: null
};

const ui = {
    init() {
        // Event Listeners
        $('#menuToggle').onclick = () => this.toggleSidebar('left');
        $('#telemetryToggle').onclick = () => this.toggleSidebar('right');
        $$('.close-sidebar').forEach(b => b.onclick = () => this.closeSidebars());
        $('#backdrop').onclick = () => this.closeSidebars();
        
        $('#recordBtn').onclick = () => this.toggleRecord();
        $('#handsFreeToggleBtn').onclick = () => this.toggleHandsFree();
        
        // Sliders
        $('#tempRange').oninput = (e) => $('#tempVal').innerText = e.target.value;
        $('#beamRange').oninput = (e) => $('#beamVal').innerText = e.target.value;
        $('#vadRange').oninput = (e) => $('#vadVal').innerText = e.target.value;

        // File Upload
        $('#fileInput').onchange = (e) => { 
            if(e.target.files[0]) API.transcribe(e.target.files[0], 0); 
        };

        // Shortcut
        document.addEventListener('keydown', (e) => { 
            if (e.code === 'Space' && e.target.tagName !== 'TEXTAREA' && e.target.tagName !== 'INPUT') { 
                e.preventDefault(); 
                this.toggleRecord(); 
            } 
        });
    },

    toggleRecord() {
        if (!AudioEngine.ctx) AudioEngine.init();

        if (AudioEngine.isRecording) {
            // Stop Recording
            AudioEngine.isRecording = false;
            clearInterval(state.timer);
            $('#recordBtn').classList.remove('recording');
            
            const duration = Date.now() - state.startTime;
            if (duration > 500) { // En az 0.5sn kayıt
                const blob = AudioEngine.createWavBlob();
                API.transcribe(blob, duration);
            }
            AudioEngine.chunks = [];
            $('#recordTimer').innerText = "00:00";
            $('#recordTimer').style.opacity = "0";
        } else {
            // Start Recording
            AudioEngine.chunks = [];
            AudioEngine.isRecording = true;
            state.startTime = Date.now();
            $('#recordBtn').classList.add('recording');
            $('#recordTimer').style.opacity = "1";
            
            state.timer = setInterval(() => {
                const diff = Math.floor((Date.now() - state.startTime) / 1000);
                const m = Math.floor(diff / 60).toString().padStart(2,'0');
                const s = (diff % 60).toString().padStart(2,'0');
                $('#recordTimer').innerText = `${m}:${s}`;
            }, 1000);
        }
    },

    toggleHandsFree() {
        state.isHandsFree = !state.isHandsFree;
        $('#handsFreeToggleBtn').classList.toggle('active');
        if (state.isHandsFree && !AudioEngine.ctx) AudioEngine.init();
    },

    renderResult(data, durationMs, audioUrl) {
        const container = $('#transcriptContainer');
        $('.empty-placeholder')?.remove();

        // Segmentleri hazırla (Tek parça metin gelirse onu dizi yap)
        const segments = (data.segments && data.segments.length > 0) 
            ? data.segments 
            : [{ text: data.text, start: 0, end: durationMs/1000, speaker_vec: Array(8).fill(0), gender: '?' }];

        // Kümeleme ve Render Döngüsü
        segments.forEach((seg, idx) => {
            // 1. Identify Speaker
            // Backend'den gelen speaker_vec boşsa veya hatalıysa fallback kullan
            const vec = (seg.speaker_vec && seg.speaker_vec.length === 8) ? seg.speaker_vec : Array(8).fill(0);
            const speaker = speakerMgr.assign(vec, { gender: seg.gender });
            
            // 2. HTML Construction
            const div = document.createElement('div');
            div.className = `speaker-group timeline-block`;
            // Kendi ID'mize göre (solda veya sağda göstermek için değil, renk için)
            div.style.setProperty('--spk-color', speaker.color);

            // Duygu Emoji Haritası
            const emotionMap = { excited: "🔥", neutral: "", sad: "😢", angry: "😠" };
            const emotionIcon = emotionMap[seg.emotion] || "";
            const genderIcon = speaker.gender === "F" ? "👩" : "👨";

            // Prosody Barları
            const pitchPct = Math.min(100, (vec[0] || 0) * 100); // Pitch Mean Normalized
            const energyPct = Math.min(100, (vec[2] || 0) * 100); // Energy Mean

            div.innerHTML = `
                <div class="speaker-avatar" style="border-color: ${speaker.color}; color: ${speaker.color}; background: ${speaker.color}15" 
                     onclick="ui.promptRename('${speaker.id}')" title="İsim Değiştir">
                    <div class="avatar-icon">${genderIcon}</div>
                    <div class="emotion-icon">${emotionIcon}</div>
                </div>
                <div class="speaker-content">
                    <div class="speaker-header">
                        <span class="speaker-name" id="name-${speaker.id}" style="color: ${speaker.color}">${speaker.name}</span>
                        <span class="time-tag">${seg.start.toFixed(1)}s</span>
                    </div>
                    <div class="text-bubble" style="border-left-color: ${speaker.color}">
                        ${seg.text}
                        ${audioUrl && idx === segments.length-1 ? ui.createPlayerHtml(audioUrl, durationMs) : ''}
                    </div>
                    <div class="prosody-info">
                        <div class="p-bar" title="Pitch / Ton"><i class="fas fa-music"></i> <div class="bar-bg"><div class="bar-fill" style="width:${pitchPct}%"></div></div></div>
                        <div class="p-bar" title="Energy / Ses"><i class="fas fa-bolt"></i> <div class="bar-bg"><div class="bar-fill" style="width:${energyPct}%"></div></div></div>
                    </div>
                </div>
            `;
            
            container.appendChild(div);
        });

        container.scrollTop = container.scrollHeight;
        
        // Hafızaya al (isim değiştirince yeniden çizmek için gerekebilir, şimdilik basit tutuyoruz)
        window.lastRenderData = { data, durationMs, audioUrl };
    },

    createPlayerHtml(url, durMs) {
        const sec = (durMs/1000).toFixed(1);
        // Random waveform görseli
        let bars = '';
        for(let i=0; i<12; i++) {
            const h = Math.floor(Math.random() * 12) + 4;
            bars += `<div class="wf-bar" style="height:${h}px"></div>`;
        }
        return `
            <div class="inline-player" onclick="ui.playAudio(this, '${url}')">
                <button class="play-icon"><i class="fas fa-play"></i></button>
                <div class="waveform">${bars}</div>
                <span class="duration">${sec}s</span>
            </div>
        `;
    },

    playAudio(el, url) {
        if (window.currentAudio) {
            window.currentAudio.pause();
            if (window.currentBtn) window.currentBtn.className = "fas fa-play";
        }
        
        const icon = el.querySelector('i');
        if (icon.classList.contains('fa-pause')) return;

        const audio = new Audio(url);
        window.currentAudio = audio;
        window.currentBtn = icon;

        icon.className = "fas fa-pause";
        audio.play();
        audio.onended = () => icon.className = "fas fa-play";
    },

    promptRename(id) {
        const cls = speakerMgr.clusters[id];
        if (!cls) return;
        const newName = prompt(`"${cls.name}" için yeni isim:`, cls.name);
        if (newName && newName.trim()) {
            speakerMgr.rename(id, newName.trim());
            // UI'daki tüm etiketleri güncelle
            $$(`#name-${id}`).forEach(el => el.innerText = newName.trim());
        }
    },

    addTempMessage() {
        const container = $('#transcriptContainer');
        const id = 'temp-' + Date.now();
        const div = document.createElement('div');
        div.id = id;
        div.className = 'speaker-group timeline-block temp';
        div.innerHTML = `
            <div class="speaker-avatar skeleton"></div>
            <div class="speaker-content">
                <div class="text-bubble skeleton-text">Processing...</div>
            </div>
        `;
        container.appendChild(div);
        container.scrollTop = container.scrollHeight;
        return id;
    },

    removeElement(id) { document.getElementById(id)?.remove(); },

    updateMetrics(dur, proc, data) {
        $('#durVal').innerText = (dur/1000).toFixed(2) + 's';
        $('#procVal').innerText = (proc/1000).toFixed(2) + 's';
        
        let rtf = data.meta?.rtf ? (1 / data.meta.rtf).toFixed(1) : "0.0";
        $('#rtfVal').innerText = rtf + 'x';
        
        $('#langVal').innerText = (data.language || '?').toUpperCase();
        
        // Confidence Avg
        let avgProb = 0;
        if (data.segments && data.segments.length > 0) {
            avgProb = data.segments.reduce((acc, s) => acc + s.probability, 0) / data.segments.length;
        }
        $('#confVal').innerText = `%${(avgProb * 100).toFixed(1)}`;
        
        $('#jsonLog').innerText = JSON.stringify(data, null, 2);
    },

    toggleDebugJson() { $('#jsonLog').classList.toggle('hidden'); },

    startVisualizer() {
        const canvas = $('#visualizerCanvas');
        const ctx = canvas.getContext('2d');
        canvas.width = canvas.offsetWidth;
        canvas.height = canvas.offsetHeight;

        const bufferLength = AudioEngine.analyser.frequencyBinCount;
        const dataArray = new Uint8Array(bufferLength);

        const draw = () => {
            requestAnimationFrame(draw);
            AudioEngine.analyser.getByteFrequencyData(dataArray);
            ctx.clearRect(0, 0, canvas.width, canvas.height);

            const barWidth = (canvas.width / bufferLength) * 2.5;
            let barHeight;
            let x = 0;

            for (let i = 0; i < bufferLength; i++) {
                barHeight = (dataArray[i] / 255) * canvas.height;
                ctx.fillStyle = `rgba(59, 130, 246, ${dataArray[i] / 300})`; // Blue tint
                ctx.fillRect(x, canvas.height - barHeight, barWidth, barHeight);
                x += barWidth + 1;
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
    clearChat() {
        $('#transcriptContainer').innerHTML = '';
        speakerMgr.reset();
    },
    exportTranscript(type) {
        const lines = [];
        $$('.speaker-group:not(.temp)').forEach(grp => {
            const name = grp.querySelector('.speaker-name').innerText;
            const text = grp.querySelector('.text-bubble').innerText.replace(/\n/g, ' ');
            lines.push(type === 'json' ? {speaker: name, text} : `[${name}]: ${text}`);
        });
        
        const content = type === 'json' ? JSON.stringify(lines, null, 2) : lines.join('\n');
        const blob = new Blob([content], { type: 'text/plain' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `transcript.${type}`;
        a.click();
    }
};

// Start
ui.init();