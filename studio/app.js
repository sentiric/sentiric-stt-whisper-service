const $ = (s) => document.querySelector(s);
const $$ = (s) => document.querySelectorAll(s);

// =============================================================================
// 🧠 SPEAKER SYSTEM (Clustering & Identity)
// =============================================================================
class SpeakerSystem {
    constructor() {
        this.threshold = 0.85; 
        this.clusters = {}; 
        this.nextId = 0;
        this.colors = [
            "#3B82F6", "#10B981", "#F59E0B", "#EF4444", "#8B5CF6", 
            "#EC4899", "#06B6D4", "#84CC16", "#F97316", "#6366F1"
        ];
    }

    setThreshold(val) { this.threshold = parseFloat(val); }

    cosineSim(a, b) {
        let dot=0, ma=0, mb=0;
        for(let i=0; i<a.length; i++) { dot+=a[i]*b[i]; ma+=a[i]*a[i]; mb+=b[i]*b[i]; }
        return ma&&mb ? dot / Math.sqrt(ma*mb) : 0;
    }

    identify(vector, meta) {
        // Vektör validasyonu
        if(!vector || vector.length !== 8) vector = new Array(8).fill(0);

        let bestId = null, bestScore = -1;
        
        for (const id in this.clusters) {
            const score = this.cosineSim(vector, this.clusters[id].centroid);
            if (score > bestScore) { bestScore = score; bestId = id; }
        }

        if (bestId && bestScore >= this.threshold) {
            this.update(bestId, vector, meta);
            return this.clusters[bestId];
        }
        return this.create(vector, meta);
    }

    create(vector, meta) {
        const id = `s_${this.nextId++}`;
        this.clusters[id] = {
            id,
            centroid: [...vector],
            count: 1,
            name: `Konuşmacı ${String.fromCharCode(65 + (this.nextId - 1))}`, // A, B, C...
            color: this.colors[this.nextId % this.colors.length],
            gender: meta.gender || '?'
        };
        return this.clusters[id];
    }

    update(id, vector, meta) {
        const c = this.clusters[id];
        // Weighted Moving Average (Anti-Jitter)
        const lr = c.count < 5 ? 0.3 : 0.05; // Başta hızlı öğren, sonra sabitle
        for(let i=0; i<8; i++) c.centroid[i] = c.centroid[i]*(1-lr) + vector[i]*lr;
        c.count++;
        if(meta.gender && meta.gender !== '?') c.gender = meta.gender; // Cinsiyeti güncelle
    }

    rename(id) {
        const c = this.clusters[id];
        if(!c) return;
        const n = prompt("Yeni isim:", c.name);
        if(n) { 
            c.name = n.trim(); 
            // UI Update
            $$(`.spk-lbl-${id}`).forEach(el => el.innerText = c.name);
        }
    }

    reset() { this.clusters = {}; this.nextId = 0; }
}

const Speaker = new SpeakerSystem();

// =============================================================================
// 🎹 AUDIO ENGINE (Recording & Viz)
// =============================================================================
const AudioSys = {
    ctx: null, analyser: null, script: null, 
    chunks: [], isRec: false, timer: null, startT: 0, handsFree: false, lastSpk: 0, isSpk: false,

    async init() {
        if(this.ctx) return;
        this.ctx = new (window.AudioContext || window.webkitAudioContext)();
        const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
        const src = this.ctx.createMediaStreamSource(stream);
        this.analyser = this.ctx.createAnalyser(); 
        this.analyser.fftSize = 256; 
        this.analyser.smoothingTimeConstant = 0.6;
        this.script = this.ctx.createScriptProcessor(4096, 1, 1);
        src.connect(this.analyser); this.analyser.connect(this.script); this.script.connect(this.ctx.destination);
        this.script.onaudioprocess = e => this.process(e);
        UI.startViz();
    },

    process(e) {
        const inData = e.inputBuffer.getChannelData(0);
        
        // VAD RMS Calc
        let s = 0; for(let i=0; i<inData.length; i++) s += inData[i]*inData[i];
        const rms = Math.sqrt(s/inData.length);

        if(this.isRec) this.chunks.push(this.floatTo16(inData));

        if(this.handsFree) {
            const th = parseFloat($('#vadRange').value);
            if(rms > th) {
                this.lastSpk = Date.now();
                if(!this.isSpk) { this.isSpk = true; if(!this.isRec) UI.toggleRec(); }
            } else if(this.isSpk && Date.now() - this.lastSpk > 1500) {
                this.isSpk = false; if(this.isRec) UI.toggleRec();
            }
        }
    },

    floatTo16(input) {
        const out = new Int16Array(input.length);
        for(let i=0; i<input.length; i++) {
            let s = Math.max(-1, Math.min(1, input[i]));
            out[i] = s < 0 ? s * 0x8000 : s * 0x7FFF;
        }
        return out;
    },

    getBlob() {
        const len = this.chunks.reduce((a,c)=>a+c.length,0); const res = new Int16Array(len); let o=0;
        for(let c of this.chunks) { res.set(c,o); o+=c.length; }
        // Simple WAV Header
        const b = new ArrayBuffer(44+res.length*2); const v = new DataView(b); const r = this.ctx.sampleRate;
        const w = (O,S)=> { for(let i=0;i<S.length;i++)v.setUint8(O+i,S.charCodeAt(i)); };
        w(0,'RIFF'); v.setUint32(4,36+res.length*2,true); w(8,'WAVEfmt '); v.setUint32(16,16,true); v.setUint16(20,1,true); v.setUint16(22,1,true);
        v.setUint32(24,r,true); v.setUint32(28,r*2,true); v.setUint16(32,2,true); v.setUint16(34,16,true); w(36,'data'); v.setUint32(40,res.length*2,true);
        new Int16Array(b,44).set(res); return new Blob([b],{type:'audio/wav'});
    }
};

// =============================================================================
// 🌐 API LAYER
// =============================================================================
const API = {
    async send(blob, durMs) {
        const fd = new FormData();
        fd.append('file', blob);
        fd.append('language', $('#langSelect').value);
        fd.append('prompt', $('#promptInput').value);
        fd.append('translate', $('#translateToggle').checked);
        fd.append('diarization', $('#diarizationToggle').checked);
        fd.append('temperature', $('#tempRange').value);
        fd.append('beam_size', $('#beamRange').value);
        // vad_threshold backend config'de, client'da VAD logic var.

        const tempId = UI.showLoading();
        try {
            const t0 = Date.now();
            const r = await fetch('/v1/transcribe', { method: 'POST', body: fd });
            const d = await r.json();
            UI.removeLoading(tempId);
            
            if(r.ok) {
                const url = URL.createObjectURL(blob);
                UI.render(d, durMs, url);
                UI.updateMetrics(durMs, Date.now()-t0, d);
            } else {
                console.error(d);
                alert("API Hatası: " + (d.error || "Bilinmiyor"));
            }
        } catch(e) {
            UI.removeLoading(tempId);
            console.error(e);
        }
    }
};

// =============================================================================
// 🎨 UI CONTROLLER
// =============================================================================
const UI = {
    init() {
        // --- Sidebar Logic ---
        const tog = (id, s) => { $(id).classList.toggle('active', s); $('#backdrop').classList.toggle('active', s); };
        $('#mobLeftBtn').onclick = () => tog('#sidebarLeft', true);
        $('#mobRightBtn').onclick = () => tog('#sidebarRight', true);
        $('#closeLeft').onclick = () => tog('#sidebarLeft', false);
        $('#closeRight').onclick = () => tog('#sidebarRight', false);
        $('#backdrop').onclick = () => { $$('.sidebar').forEach(s=>s.classList.remove('active')); $('#backdrop').classList.remove('active'); };

        // --- Controls Bindings ---
        $('#tempRange').oninput = e => $('#tempDisplay').innerText = e.target.value;
        $('#beamRange').oninput = e => $('#beamDisplay').innerText = e.target.value;
        $('#clusterRange').oninput = e => { $('#clusterDisplay').innerText = e.target.value; Speaker.setThreshold(e.target.value); };
        $('#vadRange').oninput = e => $('#vadDisplay').innerText = e.target.value;
        
        // --- Action Buttons ---
        $('#recordBtn').onclick = () => this.toggleRec();
        $('#vadBtn').onclick = () => this.toggleVad();
        $('#fileInput').onchange = e => { if(e.target.files[0]) API.send(e.target.files[0], 0); };
        
        // --- Theme ---
        const t = localStorage.getItem('theme') || 'dark';
        document.body.setAttribute('data-theme', t);
        $$('.theme-toggle').forEach(b => b.onclick = () => {
            const n = document.body.getAttribute('data-theme')=='dark'?'light':'dark';
            document.body.setAttribute('data-theme', n); localStorage.setItem('theme', n);
        });

        // --- Keyboard ---
        document.onkeydown = e => { if(e.code=='Space' && e.target.tagName!='TEXTAREA' && e.target.tagName!='INPUT') { e.preventDefault(); this.toggleRec(); } };
    },

    toggleRec() {
        if(!AudioSys.ctx) AudioSys.init();
        if(AudioSys.isRec) {
            // STOP
            AudioSys.isRec = false; clearInterval(AudioSys.timer);
            $('#recordBtn').classList.remove('recording');
            const dur = Date.now() - AudioSys.startT;
            if(dur > 500) API.send(AudioSys.getBlob(), dur);
            AudioSys.chunks = []; $('#recordTimer').innerText = "00:00"; $('#recordTimer').style.opacity = '0';
        } else {
            // START
            AudioSys.chunks = []; AudioSys.isRec = true; AudioSys.startT = Date.now();
            $('#recordBtn').classList.add('recording'); $('#recordTimer').style.opacity = '1';
            AudioSys.timer = setInterval(() => {
                const d = Math.floor((Date.now() - AudioSys.startT)/1000);
                $('#recordTimer').innerText = `${String(Math.floor(d/60)).padStart(2,'0')}:${String(d%60).padStart(2,'0')}`;
            }, 1000);
        }
    },

    toggleVad() {
        AudioSys.handsFree = !AudioSys.handsFree;
        $('#vadBtn').classList.toggle('active');
        $('#vadBtn .active-dot').style.display = AudioSys.handsFree ? 'block' : 'none';
        if(AudioSys.handsFree && !AudioSys.ctx) AudioSys.init();
    },

    render(data, dur, url) {
        const c = $('#transcriptFeed');
        $('.empty-placeholder')?.remove();

        const segs = data.segments && data.segments.length ? data.segments : [{text: data.text, start:0, speaker_vec:[], gender:'?'}];

        segs.forEach((seg, idx) => {
            // Hallucination Check
            const isBad = seg.text.match(/^(\[|\(|\-Altyazı)/) || seg.text.length < 2;
            const spk = Speaker.identify(seg.speaker_vec, {gender: seg.gender});
            
            // Emoji Map
            const emo = { excited:"🔥", sad:"😢", angry:"😠" }[seg.emotion] || "";
            const gen = spk.gender === 'F' ? '👩' : '👨';

            // Bars calculation (from vec)
            const vec = seg.speaker_vec || [0,0,0,0,0,0,0,0];
            const pPct = Math.min(100, (vec[0]||0)*100);
            const ePct = Math.min(100, (vec[2]||0)*100);

            // Audio Waveform (Random for viz)
            let waves = ''; for(let i=0; i<12; i++) waves += `<div class="wave-line" style="height:${Math.random()*8+4}px"></div>`;

            const html = `
            <div class="speaker-row">
                <div class="avatar-box" style="border-color:${spk.color}; color:${spk.color}" onclick="Speaker.rename('${spk.id}')">
                    ${gen}<div class="emo-tag">${emo}</div>
                </div>
                <div class="msg-content">
                    <div class="msg-meta">
                        <span class="spk-label spk-lbl-${spk.id}" style="color:${spk.color}">${spk.name}</span>
                        <span class="time-label">${seg.start.toFixed(1)}s</span>
                    </div>
                    <div class="bubble ${isBad?'hallucination':''}" style="border-left-color:${spk.color}">
                        ${seg.text}
                    </div>
                    <div class="features-row">
                        ${url && idx===segs.length-1 ? `<div class="mini-player" onclick="UI.play(this,'${url}')"><div class="play-icon"><i class="fas fa-play"></i></div><div class="wave-vis">${waves}</div></div>` : ''}
                        <div class="prosody-item" title="Pitch"><i class="fas fa-music"></i><div class="bar-track"><div class="bar-fill" style="width:${pPct}%; background:${spk.color}"></div></div></div>
                        <div class="prosody-item" title="Energy"><i class="fas fa-bolt"></i><div class="bar-track"><div class="bar-fill" style="width:${ePct}%; background:${spk.color}"></div></div></div>
                    </div>
                </div>
            </div>`;
            c.insertAdjacentHTML('beforeend', html);
        });
        c.scrollTop = c.scrollHeight;
    },

    play(el, url) {
        const i = el.querySelector('i');
        if(i.classList.contains('fa-pause')) { window.audio.pause(); return; }
        if(window.audio) { window.audio.pause(); window.playBtn.className='fas fa-play'; }
        window.audio = new Audio(url); window.playBtn = i;
        i.className = 'fas fa-pause'; window.audio.play(); 
        window.audio.onended = () => i.className = 'fas fa-play';
        window.audio.onpause = () => i.className = 'fas fa-play';
    },

    showLoading() {
        const id = 'tmp-'+Date.now();
        $('#transcriptFeed').insertAdjacentHTML('beforeend', `<div id="${id}" class="speaker-row" style="opacity:0.5"><div class="avatar-box"><i class="fas fa-circle-notch fa-spin"></i></div><div class="msg-content"><div class="bubble">Analiz ediliyor...</div></div></div>`);
        return id;
    },
    removeLoading(id) { document.getElementById(id)?.remove(); },

    updateMetrics(dur, proc, data) {
        $('#durVal').innerText = (dur/1000).toFixed(2)+'s';
        $('#procVal').innerText = (proc/1000).toFixed(2)+'s';
        const rtf = data.meta?.rtf ? (1/data.meta.rtf).toFixed(1) : 0;
        $('#rtfVal').innerText = rtf + 'x';
        $('#langVal').innerText = (data.language || '?').toUpperCase();
        
        let conf = 0; if(data.segments?.length) conf = data.segments.reduce((a,b)=>a+b.probability,0)/data.segments.length;
        $('#confVal').innerText = Math.round(conf*100) + '%';
        $('#jsonOutput').innerText = JSON.stringify(data, null, 2);
    },

    copyJson() { navigator.clipboard.writeText($('#jsonOutput').innerText); },
    clear() { $('#transcriptFeed').innerHTML = '<div class="empty-placeholder"><div class="placeholder-icon"><i class="fas fa-microphone-lines"></i></div><h3>Temizlendi</h3></div>'; Speaker.reset(); },
    export(t) {
        let txt=""; $$('.speaker-row').forEach(r=>{ 
            if(r.id.startsWith('tmp'))return;
            const n=r.querySelector('.spk-label').innerText; 
            const m=r.querySelector('.bubble').innerText.replace(/\n/g,' ');
            txt += t=='json' ? JSON.stringify({speaker:n,text:m})+"," : `[${n}]: ${m}\n`; 
        });
        const blob=new Blob([txt],{type:'text/plain'});
        const a=document.createElement('a'); a.href=URL.createObjectURL(blob); a.download='transcript.'+t; a.click();
    },

    startViz() {
        const cv = $('#audioVisualizer'); const ctx = cv.getContext('2d');
        const rsz = () => { cv.width = cv.parentElement.offsetWidth; cv.height = cv.parentElement.offsetHeight; };
        window.onresize = rsz; rsz();
        const arr = new Uint8Array(AudioSys.analyser.frequencyBinCount);
        const loop = () => {
            requestAnimationFrame(loop); AudioSys.analyser.getByteFrequencyData(arr);
            ctx.clearRect(0,0,cv.width,cv.height);
            const barW = (cv.width/arr.length)*2.5; let x=0;
            ctx.fillStyle = document.body.getAttribute('data-theme')=='dark'?'rgba(59,130,246,0.2)':'rgba(37,99,235,0.2)';
            for(let i=0; i<arr.length; i++) {
                const h = (arr[i]/255)*cv.height;
                ctx.fillRect(cv.width/2+x, (cv.height-h)/2, barW, h);
                ctx.fillRect(cv.width/2-x, (cv.height-h)/2, barW, h);
                x+=barW+1;
            }
        }; loop();
    }
};

UI.init();