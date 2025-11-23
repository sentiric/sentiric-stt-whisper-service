const $ = (s) => document.querySelector(s);
const $$ = (s) => document.querySelectorAll(s);

// =============================================================================
// 🧠 CORE: SPEAKER IDENTITY MANAGER
// =============================================================================
class SpeakerManager {
    constructor(threshold = 0.88) {
        this.threshold = threshold;
        this.clusters = {}; 
        this.nextId = 0;
        this.colors = [
            "#3B82F6", "#10B981", "#F59E0B", "#EF4444", "#8B5CF6", 
            "#EC4899", "#06B6D4", "#84CC16", "#F97316", "#6366F1"
        ];
    }

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

    assign(vector, meta) {
        let bestId = null;
        let bestScore = -1;

        for (const id in this.clusters) {
            const score = this.cosine(vector, this.clusters[id].centroid);
            if (score > bestScore) {
                bestScore = score;
                bestId = id;
            }
        }

        if (bestId && bestScore >= this.threshold) {
            this.updateCluster(bestId, vector, meta);
            return this.clusters[bestId];
        }
        return this.createCluster(vector, meta);
    }

    createCluster(vector, meta) {
        const id = `spk_${this.nextId++}`;
        const color = this.colors[this.nextId % this.colors.length];
        this.clusters[id] = {
            id: id,
            centroid: [...vector],
            count: 1,
            name: `Konuşmacı ${String.fromCharCode(65 + (this.nextId - 1))}`,
            color: color,
            gender: meta.gender || "?",
        };
        return this.clusters[id];
    }

    updateCluster(id, vector, meta) {
        const cls = this.clusters[id];
        const weight = 1.0 / (cls.count + 1);
        for (let i = 0; i < vector.length; i++) {
            cls.centroid[i] = cls.centroid[i] * (1 - weight) + vector[i] * weight;
        }
        cls.count++;
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

const speakerMgr = new SpeakerManager(0.85);

// =============================================================================
// 🎤 AUDIO ENGINE
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
        this.analyser.fftSize = 512; // Higher resolution for better visualizer
        this.analyser.smoothingTimeConstant = 0.6;

        this.scriptNode = this.ctx.createScriptProcessor(4096, 1, 1);
        
        source.connect(this.analyser);
        this.analyser.connect(this.scriptNode);
        this.scriptNode.connect(this.ctx.destination);

        this.scriptNode.onaudioprocess = (e) => this.process(e);
        ui.startVisualizer();
    },

    process(e) {
        const input = e.inputBuffer.getChannelData(0);
        
        // VAD
        let sum = 0;
        for (let i = 0; i < input.length; i++) sum += input[i] * input[i];
        const rms = Math.sqrt(sum / input.length);

        if (this.isRecording) {
            this.chunks.push(this.floatTo16Bit(input));
        }

        // Hands-Free
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
                if (silenceDur > 1500) { 
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
// 🌐 API
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
                console.error(data);
                // alert("Hata: " + (data.error || "Sunucu hatası"));
            }
        } catch (e) {
            ui.removeElement(tempId);
            console.error(e);
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
        $('#recordBtn').onclick = () => this.toggleRecord();
        $('#handsFreeToggleBtn').onclick = () => this.toggleHandsFree();
        $('#fileInput').onchange = (e) => { if(e.target.files[0]) API.transcribe(e.target.files[0], 0); };
        
        // Sliders updates
        $('#tempRange').oninput = (e) => $('#tempVal').innerText = e.target.value;
        $('#beamRange').oninput = (e) => $('#beamVal').innerText = e.target.value;
        $('#vadRange').oninput = (e) => $('#vadVal').innerText = e.target.value;

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
            AudioEngine.isRecording = false;
            clearInterval(state.timer);
            $('#recordBtn').classList.remove('recording');
            
            const duration = Date.now() - state.startTime;
            if (duration > 500) {
                const blob = AudioEngine.createWavBlob();
                API.transcribe(blob, duration);
            }
            AudioEngine.chunks = [];
            $('#recordTimer').innerText = "00:00";
            $('#recordTimer').style.opacity = "0";
        } else {
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
        $('.empty-state')?.remove();

        const segments = (data.segments && data.segments.length > 0) 
            ? data.segments 
            : [{ text: data.text, start: 0, end: durationMs/1000, speaker_vec: Array(8).fill(0), gender: '?' }];

        segments.forEach((seg, idx) => {
            const vec = (seg.speaker_vec && seg.speaker_vec.length === 8) ? seg.speaker_vec : Array(8).fill(0);
            const speaker = speakerMgr.assign(vec, { gender: seg.gender });
            
            const div = document.createElement('div');
            div.className = `timeline-block`;
            
            const emotionMap = { excited: "🔥", neutral: "", sad: "😢", angry: "😠" };
            const emotionIcon = emotionMap[seg.emotion] || "";
            const genderIcon = speaker.gender === "F" ? "👩" : "👨";

            const pitchPct = Math.min(100, (vec[0] || 0) * 100); 
            const energyPct = Math.min(100, (vec[2] || 0) * 100); 

            div.innerHTML = `
                <div class="speaker-avatar" style="border-color: ${speaker.color}; color: ${speaker.color}" 
                     onclick="ui.promptRename('${speaker.id}')" title="Yeniden Adlandır">
                    <div>${genderIcon}</div>
                    <div class="emotion-badge">${emotionIcon}</div>
                </div>
                <div class="speaker-content">
                    <div class="speaker-meta">
                        <span class="speaker-name" id="name-${speaker.id}" style="color: ${speaker.color}">${speaker.name}</span>
                        <span class="time-stamp">${seg.start.toFixed(1)}s</span>
                    </div>
                    <div class="bubble" style="border-left-color: ${speaker.color}">
                        ${seg.text}
                        ${audioUrl && idx === segments.length-1 ? ui.createPlayerHtml(audioUrl, durationMs) : ''}
                    </div>
                    <div class="prosody-strip">
                        <div class="p-meter" title="Pitch"><i class="fas fa-music"></i> <div class="p-track"><div class="p-fill" style="width:${pitchPct}%; background:${speaker.color}"></div></div></div>
                        <div class="p-meter" title="Energy"><i class="fas fa-bolt"></i> <div class="p-track"><div class="p-fill" style="width:${energyPct}%; background:${speaker.color}"></div></div></div>
                    </div>
                </div>
            `;
            container.appendChild(div);
        });

        container.scrollTop = container.scrollHeight;
    },

    createPlayerHtml(url, durMs) {
        const sec = (durMs/1000).toFixed(1);
        let bars = '';
        for(let i=0; i<16; i++) {
            const h = Math.floor(Math.random() * 12) + 4;
            bars += `<div class="wave-bar" style="height:${h}px"></div>`;
        }
        return `
            <div class="audio-player" onclick="ui.playAudio(this, '${url}')">
                <div class="play-icon"><i class="fas fa-play"></i></div>
                <div class="wave-visual">${bars}</div>
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
        const newName = prompt(`İsim verin:`, cls.name);
        if (newName && newName.trim()) {
            speakerMgr.rename(id, newName.trim());
            $$(`#name-${id}`).forEach(el => el.innerText = newName.trim());
        }
    },

    addTempMessage() {
        const container = $('#transcriptContainer');
        const id = 'temp-' + Date.now();
        const div = document.createElement('div');
        div.id = id;
        div.className = 'timeline-block';
        div.style.opacity = '0.5';
        div.innerHTML = `
            <div class="speaker-avatar" style="border-style: dashed; border-color: #555;">
                <i class="fas fa-circle-notch fa-spin"></i>
            </div>
            <div class="speaker-content">
                <div class="bubble">Analiz ediliyor...</div>
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
        
        let avgProb = 0;
        if (data.segments && data.segments.length > 0) {
            avgProb = data.segments.reduce((acc, s) => acc + s.probability, 0) / data.segments.length;
        }
        $('#confVal').innerText = `%${(avgProb * 100).toFixed(1)}`;
        
        // Pretty JSON
        $('#jsonLog').innerText = JSON.stringify(data, null, 2);
    },

    copyJson() {
        const text = $('#jsonLog').innerText;
        navigator.clipboard.writeText(text);
    },

    startVisualizer() {
        const canvas = $('#visualizerCanvas');
        const ctx = canvas.getContext('2d');
        
        // Dynamic Resize
        const resize = () => {
            canvas.width = canvas.parentElement.offsetWidth;
            canvas.height = canvas.parentElement.offsetHeight;
        };
        window.addEventListener('resize', resize);
        resize();

        const bufferLength = AudioEngine.analyser.frequencyBinCount;
        const dataArray = new Uint8Array(bufferLength);

        const draw = () => {
            requestAnimationFrame(draw);
            AudioEngine.analyser.getByteFrequencyData(dataArray);
            ctx.clearRect(0, 0, canvas.width, canvas.height);

            const centerX = canvas.width / 2;
            const centerY = canvas.height / 2;
            
            // Mirror Effect for cool wave
            ctx.fillStyle = 'rgba(59, 130, 246, 0.4)';
            const barWidth = (canvas.width / bufferLength) * 2;
            
            for (let i = 0; i < bufferLength; i++) {
                const h = (dataArray[i] / 255) * canvas.height * 0.8;
                // Right side
                ctx.fillRect(centerX + (i * barWidth), centerY - h/2, barWidth - 1, h);
                // Left side
                ctx.fillRect(centerX - (i * barWidth), centerY - h/2, barWidth - 1, h);
            }
        };
        draw();
    },

    clearChat() {
        $('#transcriptContainer').innerHTML = '<div class="empty-state"><div class="icon-box"><i class="fas fa-microphone-alt"></i></div><h3>Kayda Hazır</h3><p>Temizlendi.</p></div>';
        speakerMgr.reset();
    },

    exportTranscript(type) {
        const lines = [];
        $$('.timeline-block').forEach(grp => {
            if (grp.style.opacity === '0.5') return;
            const name = grp.querySelector('.speaker-name').innerText;
            const text = grp.querySelector('.bubble').innerText.replace(/\n/g, ' ');
            lines.push(type === 'json' ? {speaker: name, text} : `[${name}]: ${text}`);
        });
        
        const content = type === 'json' ? JSON.stringify(lines, null, 2) : lines.join('\n');
        const blob = new Blob([content], { type: 'text/plain' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `sentiric-transcript.${type}`;
        a.click();
    }
};

ui.init();