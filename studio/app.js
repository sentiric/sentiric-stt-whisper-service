const $ = (s) => document.querySelector(s);
const $$ = (s) => document.querySelectorAll(s);

// =============================================================================
// 🧠 PROMPT & CONFIG SYSTEM
// =============================================================================
const Templates = {
    general: "Doğru noktalama işaretleri kullan. Akıcı bir dil kullan.",
    medical: "Hasta öyküsü ve tıbbi terimler. Anamnez, teşhis, tedavi planı. Kardiyoloji, Nöroloji, Dahiliye terimlerini doğru yaz. Latince terimleri koru.",
    legal: "Hukuki terminoloji. Mahkeme tutanağı formatı. Davacı, davalı, mübaşir, hakim, hüküm, gereği düşünüldü.",
    tech: "Yazılım geliştirme toplantısı. API, JSON, Docker, Kubernetes, refactoring, merge request, commit, backend, frontend terimlerini İngilizce olarak koru."
};

const ViewModes = {
    heatmap: false,
    karaoke: true
};

// =============================================================================
// 🧠 SPEAKER SYSTEM (Clustering)
// =============================================================================
class SpeakerSystem {
    constructor() {
        this.threshold = 0.85; 
        this.clusters = {}; 
        this.nextId = 0;
        this.colors = ["#3B82F6", "#10B981", "#F59E0B", "#EF4444", "#8B5CF6", "#EC4899", "#06B6D4"];
    }
    setThreshold(val) { this.threshold = parseFloat(val); }
    cosineSim(a, b) {
        let dot=0, ma=0, mb=0;
        for(let i=0; i<a.length; i++) { dot+=a[i]*b[i]; ma+=a[i]*a[i]; mb+=b[i]*b[i]; }
        return ma&&mb ? dot / Math.sqrt(ma*mb) : 0;
    }
    identify(vector, meta) {
        if(!vector || vector.length !== 8) vector = new Array(8).fill(0);
        let bestId = null, bestScore = -1;
        for (const id in this.clusters) {
            const score = this.cosineSim(vector, this.clusters[id].centroid);
            if (score > bestScore) { bestScore = score; bestId = id; }
        }
        if (bestId && bestScore >= this.threshold) {
            this.update(bestId, vector, meta); return this.clusters[bestId];
        }
        return this.create(vector, meta);
    }
    create(vector, meta) {
        const id = `s_${this.nextId++}`;
        this.clusters[id] = { id, centroid: [...vector], count: 1, name: `Konuşmacı ${String.fromCharCode(65 + (this.nextId - 1))}`, color: this.colors[this.nextId % this.colors.length], gender: meta.gender || '?' };
        return this.clusters[id];
    }
    update(id, vector, meta) {
        const c = this.clusters[id];
        const lr = c.count < 5 ? 0.3 : 0.05;
        for(let i=0; i<8; i++) c.centroid[i] = c.centroid[i]*(1-lr) + vector[i]*lr;
        c.count++;
        if(meta.gender && meta.gender !== '?' && c.gender === '?') c.gender = meta.gender; 
    }
    rename(id) {
        const c = this.clusters[id]; if(!c) return;
        const n = prompt("Yeni isim:", c.name);
        if(n) { c.name = n.trim(); $$(`.spk-lbl-${id}`).forEach(el => el.innerText = c.name); }
    }
    reset() { this.clusters = {}; this.nextId = 0; }
}
const Speaker = new SpeakerSystem();

// =============================================================================
// 🎹 AUDIO ENGINE & HARDWARE CONTROL
// =============================================================================
const AudioSys = {
    ctx: null, analyser: null, script: null, 
    chunks: [], isRec: false, timer: null, startT: 0, handsFree: false, lastSpk: 0, isSpk: false,
    wakeLock: null,

    async init() {
        if(this.ctx) return;
        this.ctx = new (window.AudioContext || window.webkitAudioContext)();
        const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
        const src = this.ctx.createMediaStreamSource(stream);
        this.analyser = this.ctx.createAnalyser(); 
        this.analyser.fftSize = 256; 
        this.script = this.ctx.createScriptProcessor(4096, 1, 1);
        src.connect(this.analyser); this.analyser.connect(this.script); this.script.connect(this.ctx.destination);
        this.script.onaudioprocess = e => this.process(e);
        UI.startViz();
    },

    async requestWakeLock() {
        try { if(navigator.wakeLock) this.wakeLock = await navigator.wakeLock.request('screen'); } catch(e) { console.log('WakeLock error:', e); }
    },
    releaseWakeLock() {
        if(this.wakeLock) { this.wakeLock.release().then(() => { this.wakeLock = null; }); }
    },
    haptic(pattern = [20]) {
        if(navigator.vibrate) navigator.vibrate(pattern);
    },

    process(e) {
        const inData = e.inputBuffer.getChannelData(0);
        let s = 0; for(let i=0; i<inData.length; i++) s += inData[i]*inData[i];
        const rms = Math.sqrt(s/inData.length);
        if(this.isRec) this.chunks.push(this.floatTo16(inData));
        if(this.handsFree) { 
            const th = 0.02; 
            if(rms > th) { this.lastSpk = Date.now(); if(!this.isSpk) { this.isSpk = true; if(!this.isRec) UI.toggleRec(); } } 
            else if(this.isSpk && Date.now() - this.lastSpk > 1500) { this.isSpk = false; if(this.isRec) UI.toggleRec(); }
        }
    },

    floatTo16(input) {
        const out = new Int16Array(input.length);
        for(let i=0; i<input.length; i++) { let s = Math.max(-1, Math.min(1, input[i])); out[i] = s < 0 ? s * 0x8000 : s * 0x7FFF; }
        return out;
    },

    getBlob() {
        const len = this.chunks.reduce((a,c)=>a+c.length,0); const res = new Int16Array(len); let o=0;
        for(let c of this.chunks) { res.set(c,o); o+=c.length; }
        const b = new ArrayBuffer(44+res.length*2); const v = new DataView(b); const r = this.ctx.sampleRate;
        const w = (O,S)=> { for(let i=0;i<S.length;i++)v.setUint8(O+i,S.charCodeAt(i)); };
        w(0,'RIFF'); v.setUint32(4,36+res.length*2,true); w(8,'WAVEfmt '); v.setUint32(16,16,true); v.setUint16(20,1,true); v.setUint16(22,1,true);
        v.setUint32(24,r,true); v.setUint32(28,r*2,true); v.setUint16(32,2,true); v.setUint16(34,16,true); w(36,'data'); v.setUint32(40,res.length*2,true);
        new Int16Array(b,44).set(res); return new Blob([b],{type:'audio/wav'});
    }
};

// =============================================================================
// 🎨 UI CONTROLLER
// =============================================================================
const UI = {
    init() {
        const tog = (side, s) => { $(`#sidebar${side}`).classList.toggle('active', s); $(`#overlay${side}`).classList.toggle('active', s); };
        $('#mobLeftBtn').onclick = () => tog('Left', true); $('#mobRightBtn').onclick = () => tog('Right', true);
        $('#closeLeft').onclick = () => tog('Left', false); $('#closeRight').onclick = () => tog('Right', false);
        $$('.sidebar-overlay').forEach(o => o.onclick = () => { tog('Left', false); tog('Right', false); });

        this.bind('#tempRange', '#tempDisplay', 'stt_temp');
        this.bind('#lpfRange', '#lpfDisplay', 'stt_lpf');
        this.bind('#pitchGateRange', '#pitchGateDisplay', 'stt_pitch_gate');
        this.bind('#clusterRange', '#clusterDisplay', 'stt_cluster', (v) => Speaker.setThreshold(v));

        $('#recordBtn').onclick = () => this.toggleRec();
        $('#vadBtn').onclick = () => this.toggleVad();
        $('#fileInput').onchange = e => { if(e.target.files[0]) this.sendAudio(e.target.files[0], 0); };
        
        const t = localStorage.getItem('theme') || 'dark';
        document.body.setAttribute('data-theme', t);
        $$('.theme-toggle').forEach(b => b.onclick = () => {
            const n = document.body.getAttribute('data-theme')=='dark'?'light':'dark';
            document.body.setAttribute('data-theme', n); localStorage.setItem('theme', n);
        });
        document.onkeydown = e => { if(e.code=='Space' && e.target.tagName!='TEXTAREA' && e.target.tagName!='INPUT') { e.preventDefault(); this.toggleRec(); } };
        
        // Init default template
        if(!$('#promptInput').value) this.setTemplate('general');
    },

    setTemplate(key) {
        $('#promptInput').value = Templates[key] || "";
        $$('.tpl-btn').forEach(b => b.classList.remove('active'));
        event?.target?.classList?.add('active');
    },

    toggleViewMode(mode) {
        ViewModes[mode] = !ViewModes[mode];
        $(`#transcriptFeed`).classList.toggle(`${mode}-mode`, ViewModes[mode]);
        const btn = $(`#${mode}Toggle`);
        if(btn) btn.classList.toggle('active', ViewModes[mode]);
    },

    bind(inpId, dispId, key, cb) {
        const el = $(inpId); const stored = localStorage.getItem(key);
        if(stored) { el.value = stored; $(dispId).innerText = stored; if(cb) cb(stored); }
        el.oninput = e => { $(dispId).innerText = e.target.value; localStorage.setItem(key, e.target.value); if(cb) cb(e.target.value); };
    },

    toggleAcc(id) { $(`#${id}`).classList.toggle('active'); },
    resetSettings() { localStorage.clear(); location.reload(); },

    toggleRec() {
        if(!AudioSys.ctx) AudioSys.init();
        if(AudioSys.isRec) {
            AudioSys.isRec = false; clearInterval(AudioSys.timer);
            AudioSys.releaseWakeLock(); AudioSys.haptic([50, 50, 50]);
            $('#recordBtn').classList.remove('recording'); $('#recordTimer').classList.remove('active');
            const dur = Date.now() - AudioSys.startT;
            if(dur > 500) this.sendAudio(AudioSys.getBlob(), dur);
            AudioSys.chunks = []; $('#recordTimer').innerText = "00:00";
        } else {
            AudioSys.chunks = []; AudioSys.isRec = true; AudioSys.startT = Date.now();
            AudioSys.requestWakeLock(); AudioSys.haptic([50]);
            $('#recordBtn').classList.add('recording'); $('#recordTimer').classList.add('active');
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
        AudioSys.haptic([20]);
        if(AudioSys.handsFree && !AudioSys.ctx) AudioSys.init();
    },

    async sendAudio(blob, durMs) {
        const fd = new FormData();
        fd.append('file', blob);
        fd.append('language', $('#langSelect').value);
        fd.append('prompt', $('#promptInput').value);
        fd.append('temperature', $('#tempRange').value);
        fd.append('prosody_lpf_alpha', $('#lpfRange').value);
        fd.append('prosody_pitch_gate', $('#pitchGateRange').value);
        
        const tempId = this.showLoading();
        try {
            const t0 = Date.now();
            const r = await fetch('/v1/transcribe', { method: 'POST', body: fd });
            const d = await r.json();
            this.removeLoading(tempId);
            if(r.ok) {
                const url = URL.createObjectURL(blob);
                this.render(d, durMs, url);
                this.updateMetrics(durMs, Date.now()-t0, d);
            } else { alert("API Hatası: " + (d.error || "Bilinmiyor")); }
        } catch(e) { this.removeLoading(tempId); console.error(e); }
    },

    downloadAudio(url) {
        if(!url) return;
        const a = document.createElement('a'); a.href = url;
        a.download = `sentiric_rec_${Date.now()}.wav`; a.click();
    },

    render(data, dur, url) {
        const c = $('#transcriptFeed');
        $('.empty-placeholder')?.remove();
        const segs = data.segments && data.segments.length ? data.segments : [{text: data.text, start:0, speaker_vec:[], gender:'?', words:[]}];
        
        segs.forEach((seg, idx) => {
            const isBad = seg.text.match(/^(\[|\(|\-Altyazı)/) || seg.text.length < 2;
            const spk = Speaker.identify(seg.speaker_vec, {gender: seg.gender});
            const emo = { excited:"🔥", sad:"😢", angry:"😠" }[seg.emotion] || "";
            const gen = spk.gender === 'F' ? '👩' : (spk.gender === 'M' ? '👨' : '👤');
            const vec = seg.speaker_vec || [0,0,0,0,0,0,0,0];
            const pPct = Math.min(100, (vec[0]||0)*100); const ePct = Math.min(100, (vec[2]||0)*100);
            let waves = ''; for(let i=0; i<12; i++) waves += `<div class="wave-line" style="height:${Math.random()*8+4}px"></div>`;

            // HTML for words with heatmap data
            let textHtml = "";
            if (seg.words && seg.words.length > 0) {
                textHtml = seg.words.map(w => {
                    let confClass = "high";
                    if(w.probability < 0.5) confClass = "low";
                    else if(w.probability < 0.75) confClass = "mid";
                    return `<span class="w" data-start="${w.start}" data-end="${w.end}" data-prob="${(w.probability*100).toFixed(0)}%" data-conf="${confClass}">${w.word}</span>`;
                }).join(" ");
            } else {
                textHtml = seg.text;
            }

            const playerHtml = url && idx===segs.length-1 ? `
                <div class="mini-player">
                    <button class="player-btn" onclick="UI.play(this,'${url}', ${seg.start})"><i class="fas fa-play"></i></button>
                    <div class="wave-vis">${waves}</div>
                    <div class="sep" style="height:12px; margin:0 4px"></div>
                    <button class="player-btn" onclick="UI.downloadAudio('${url}')" title="Kayıdı İndir"><i class="fas fa-download"></i></button>
                </div>` : '';

            const html = `
            <div class="speaker-row" id="seg-${Date.now()}-${idx}">
                <div class="avatar-box" style="border-color:${spk.color}; color:${spk.color}" onclick="Speaker.rename('${spk.id}')">
                    ${gen}<div class="emo-tag">${emo}</div>
                </div>
                <div class="msg-content">
                    <div class="msg-meta"><span class="spk-label spk-lbl-${spk.id}" style="color:${spk.color}">${spk.name}</span><span class="time-label">${seg.start.toFixed(1)}s</span></div>
                    <div class="bubble ${isBad?'hallucination':''}" style="border-left-color:${spk.color}">${textHtml}</div>
                    <div class="features-row">
                        ${playerHtml}
                        <div class="prosody-item" title="Pitch"><i class="fas fa-music"></i><div class="bar-track"><div class="bar-fill" style="width:${pPct}%; background:${spk.color}"></div></div></div>
                        <div class="prosody-item" title="Energy"><i class="fas fa-bolt"></i><div class="bar-track"><div class="bar-fill" style="width:${ePct}%; background:${spk.color}"></div></div></div>
                    </div>
                </div>
            </div>`;
            c.insertAdjacentHTML('beforeend', html);
        });
        
        requestAnimationFrame(() => { c.scrollTop = c.scrollHeight; });
    },

    play(el, url, offset) {
        const i = el.querySelector('i');
        
        // Stop Logic
        if(window.audio) {
            window.audio.pause(); 
            window.playBtn.className='fas fa-play';
            if(window.karaokeInterval) clearInterval(window.karaokeInterval);
            // Reset highlighting
            $$('.w.active-word').forEach(e => e.classList.remove('active-word'));
            if(window.playBtn === i) { window.audio = null; return; }
        }

        // Play Logic
        window.audio = new Audio(url); 
        window.playBtn = i;
        i.className = 'fas fa-pause'; 
        window.audio.play(); 
        
        const row = el.closest('.speaker-row');
        const words = row ? Array.from(row.querySelectorAll('.w')) : [];

        // Karaoke Loop
        if (ViewModes.karaoke && words.length > 0) {
            window.karaokeInterval = setInterval(() => {
                if(!window.audio) return;
                const ct = window.audio.currentTime + offset; // Adjust for segment offset if needed (assuming file is full clip)
                // Actually usually file is full, segment times are global. 
                // But here we upload small clips usually. 
                // Let's assume the clip corresponds to the segments rendered.
                // If it's a full recording, seg.start matters. But usually we process chunk by chunk.
                // NOTE: For live stream/chunk, file starts at 0. So words.start might need offsetting relative to file.
                // However, backend returns global timestamp relative to beginning of stream? 
                // For simplicity in this demo, let's assume words.start is relative to the clip playing.
                
                // Correction: Whisper API returns relative to start of processing usually.
                // Let's use simple matching.
                
                const localT = window.audio.currentTime; // File is the chunk itself.
                
                words.forEach(w => {
                    const s = parseFloat(w.getAttribute('data-start'));
                    const e = parseFloat(w.getAttribute('data-end'));
                    // We need to normalize start time if segments are absolute.
                    // But here, since we upload one file and get result, t0 is usually 0-based for that file.
                    if (localT >= s && localT <= e) w.classList.add('active-word');
                    else w.classList.remove('active-word');
                });
            }, 50);
        }

        window.audio.onended = () => {
            i.className = 'fas fa-play';
            if(window.karaokeInterval) clearInterval(window.karaokeInterval);
            $$('.w.active-word').forEach(e => e.classList.remove('active-word'));
            window.audio = null;
        };
    },

    showLoading() {
        const id = 'tmp-'+Date.now();
        $('#transcriptFeed').insertAdjacentHTML('beforeend', `<div id="${id}" class="speaker-row" style="opacity:0.5"><div class="avatar-box"><i class="fas fa-circle-notch fa-spin"></i></div><div class="msg-content"><div class="bubble">İşleniyor...</div></div></div>`);
        $('#transcriptFeed').scrollTop = $('#transcriptFeed').scrollHeight;
        return id;
    },
    removeLoading(id) { document.getElementById(id)?.remove(); },

    updateMetrics(dur, proc, data) {
        $('#rtfVal').innerText = data.meta?.rtf ? (1/data.meta.rtf).toFixed(1) + 'x' : '0.0x';
        $('#durVal').innerText = (dur/1000).toFixed(2)+'s'; $('#procVal').innerText = (proc/1000).toFixed(2)+'s';
        
        // Avg Confidence
        let total = 0, count = 0;
        data.segments?.forEach(s => s.words?.forEach(w => { total += w.probability; count++; }));
        const avg = count ? (total/count*100).toFixed(0) : 0;
        $('#confVal').innerText = avg + '%';
        $('#langVal').innerText = (data.language || '?').toUpperCase();
        
        $('#jsonOutput').innerText = JSON.stringify(data, null, 2);
    },

    export(t) {
        let content = "";
        const data = JSON.parse($('#jsonOutput').innerText || "{}");
        
        if (t === 'json') {
            content = JSON.stringify(data, null, 2);
        } else if (t === 'txt') {
            (data.segments || []).forEach(s => content += `[${s.start.toFixed(1)}s]: ${s.text}\n`);
        } else if (t === 'srt') {
            (data.segments || []).forEach((s, i) => {
                const toTime = (sec) => new Date(sec * 1000).toISOString().substr(11, 12).replace('.', ',');
                content += `${i+1}\n${toTime(s.start)} --> ${toTime(s.end)}\n${s.text}\n\n`;
            });
        }
        
        const blob = new Blob([content], {type: 'text/plain'});
        const a = document.createElement('a'); a.href = URL.createObjectURL(blob); a.download = `transcript.${t}`; a.click();
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
    },
    clear() { $('#transcriptFeed').innerHTML = '<div class="empty-placeholder"><div class="placeholder-icon"><i class="fas fa-microphone-lines"></i></div><h3>Temizlendi</h3></div>'; Speaker.reset(); }
};

window.UI = UI;
UI.init();