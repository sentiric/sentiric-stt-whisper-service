/**
 * SENTIRIC OMNI-STUDIO v4.0 (Next-Gen)
 * Modular Architecture: Engine -> Audio -> UI
 */

// ==========================================
// 1. UTILITIES
// ==========================================
const $ = (id) => document.getElementById(id);
const sleep = (ms) => new Promise(r => setTimeout(r, ms));

// ==========================================
// 2. NETWORK ENGINE (LLM Integration)
// ==========================================
class LLMEngine {
    constructor() {
        this.controller = null;
        this.isGenerating = false;
    }

    async generate(payload, onChunk, onComplete, onError) {
        if (this.isGenerating) this.abort();
        
        this.controller = new AbortController();
        this.isGenerating = true;

        try {
            const response = await fetch('/v1/chat/completions', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload),
                signal: this.controller.signal
            });

            if (!response.ok) throw new Error(await response.text());

            const reader = response.body.getReader();
            const decoder = new TextDecoder();
            let buffer = "";

            while (true) {
                const { done, value } = await reader.read();
                if (done) break;

                buffer += decoder.decode(value, { stream: true });
                const lines = buffer.split('\n');
                buffer = lines.pop();

                for (const line of lines) {
                    if (line.startsWith('data: ') && line !== 'data: [DONE]') {
                        try {
                            const json = JSON.parse(line.substring(6));
                            const content = json.choices[0]?.delta?.content;
                            if (content) onChunk(content);
                        } catch (e) { console.error("Parse Error", e); }
                    }
                }
            }
            onComplete();
        } catch (err) {
            if (err.name !== 'AbortError') onError(err);
        } finally {
            this.isGenerating = false;
            this.controller = null;
        }
    }

    abort() {
        if (this.controller) {
            this.controller.abort();
            this.controller = null;
            this.isGenerating = false;
        }
    }

    async checkHealth() {
        try {
            const res = await fetch('/health');
            return res.ok;
        } catch { return false; }
    }
}

// ==========================================
// 3. AUDIO ENGINE (Web Audio API + Visualizer)
// ==========================================
class AudioEngine {
    constructor() {
        this.ctx = null;
        this.analyser = null;
        this.mic = null;
        this.canvas = $('waveCanvas');
        this.canvasCtx = this.canvas.getContext('2d');
        this.isVisualizing = false;
        
        this.recognition = null;
        this.isListening = false;
        this.onResult = null;
        this.onStart = null;
        this.onEnd = null;
    }

    async init() {
        if (!('webkitSpeechRecognition' in window)) return false;
        
        this.recognition = new webkitSpeechRecognition();
        this.recognition.continuous = false;
        this.recognition.interimResults = true;
        
        this.recognition.onstart = () => { 
            this.isListening = true; 
            this.startVisualizer();
            if(this.onStart) this.onStart(); 
        };
        
        this.recognition.onend = () => { 
            this.isListening = false; 
            this.stopVisualizer();
            if(this.onEnd) this.onEnd(); 
        };

        this.recognition.onresult = (e) => {
            let final = '', interim = '';
            for (let i = e.resultIndex; i < e.results.length; ++i) {
                if (e.results[i].isFinal) final += e.results[i][0].transcript;
                else interim += e.results[i][0].transcript;
            }
            if(this.onResult) this.onResult(final, interim);
        };
        
        return true;
    }

    async startVisualizer() {
        this.canvas.classList.remove('hidden');
        try {
            if (!this.ctx) this.ctx = new (window.AudioContext || window.webkitAudioContext)();
            if (this.ctx.state === 'suspended') await this.ctx.resume();
            
            const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
            this.mic = this.ctx.createMediaStreamSource(stream);
            this.analyser = this.ctx.createAnalyser();
            this.analyser.fftSize = 64;
            this.mic.connect(this.analyser);
            
            this.isVisualizing = true;
            this.drawWave();
        } catch (e) { console.warn("Visualizer Error", e); }
    }

    stopVisualizer() {
        this.isVisualizing = false;
        this.canvas.classList.add('hidden');
        if(this.mic) this.mic.disconnect();
    }

    drawWave() {
        if (!this.isVisualizing) return;
        requestAnimationFrame(() => this.drawWave());
        
        const bufferLength = this.analyser.frequencyBinCount;
        const dataArray = new Uint8Array(bufferLength);
        this.analyser.getByteFrequencyData(dataArray);
        
        const w = this.canvas.width;
        const h = this.canvas.height;
        const barW = (w / bufferLength) * 2.5;
        let x = 0;
        
        this.canvasCtx.clearRect(0, 0, w, h);
        this.canvasCtx.fillStyle = '#6366f1'; // Primary Color
        
        for(let i = 0; i < bufferLength; i++) {
            const barH = dataArray[i] / 255 * h;
            this.canvasCtx.fillRect(x, (h - barH) / 2, barW, barH);
            x += barW + 1;
        }
    }

    start() { if(this.recognition) this.recognition.start(); }
    stop() { if(this.recognition) this.recognition.stop(); }
    
    speak(text, lang) {
        if (!window.speechSynthesis) return;
        window.speechSynthesis.cancel();
        const u = new SpeechSynthesisUtterance(text.replace(/[#*`]/g, ''));
        u.lang = lang;
        window.speechSynthesis.speak(u);
    }
}

// ==========================================
// 4. UI CONTROLLER
// ==========================================
class UIController {
    constructor() {
        this.engine = new LLMEngine();
        this.audio = new AudioEngine();
        
        this.history = [];
        this.autoListen = false;
        this.stats = { tokens: 0, start: 0 };
        
        this.init();
    }

    async init() {
        await this.audio.init();
        this.setupEvents();
        this.startHealthCheck();
        
        // Audio Events Binding
        this.audio.onResult = (final, interim) => {
            const input = $('userInput');
            if (interim) $('ghostText').innerText = input.value + " " + interim;
            else $('ghostText').innerText = "";
            
            if (final) {
                input.value = (input.value + " " + final).trim();
                if (this.autoListen) {
                    this.sendMessage();
                }
            }
        };
        
        this.audio.onStart = () => {
            $('recordingIndicator').classList.remove('hidden');
            $('btnMic').classList.add('recording');
        };
        
        this.audio.onEnd = () => {
            $('recordingIndicator').classList.add('hidden');
            $('btnMic').classList.remove('recording');
            if (this.autoListen && !this.engine.isGenerating) {
                setTimeout(() => this.audio.start(), 200); // Loop
            }
        };
    }

    setupEvents() {
        // Send
        $('btnSend').onclick = () => this.sendMessage();
        $('userInput').onkeydown = (e) => { if(e.key === 'Enter' && !e.shiftKey) { e.preventDefault(); this.sendMessage(); }};
        
        // Stop
        $('btnStop').onclick = () => {
            this.autoListen = false;
            this.updateLiveBtn();
            this.engine.abort();
            this.setBusy(false);
            window.speechSynthesis.cancel();
        };

        // Mic & Live
        $('btnMic').onclick = () => {
            if(this.autoListen) { this.autoListen = false; this.updateLiveBtn(); this.audio.stop(); return; }
            if(this.audio.isListening) this.audio.stop(); else this.audio.start();
        };
        
        $('btnLive').onclick = () => {
            this.autoListen = !this.autoListen;
            this.updateLiveBtn();
            if(this.autoListen) this.audio.start(); else this.audio.stop();
        };

        // Sidebar Toggle
        this.toggleSidebar = (id) => {
            $(id).classList.toggle('active');
            $('uiOverlay').classList.toggle('active', $(id).classList.contains('active'));
        };
        
        this.closeAll = () => {
            document.querySelectorAll('.sidebar').forEach(s => s.classList.remove('active'));
            $('uiOverlay').classList.remove('active');
        };

        // Auto-Resize Textarea
        $('userInput').oninput = function() {
            this.style.height = 'auto';
            this.style.height = Math.min(this.scrollHeight, 120) + 'px';
        };
    }

    async sendMessage() {
        const text = $('userInput').value.trim();
        if (!text) return;

        // UI Reset
        window.speechSynthesis.cancel();
        $('userInput').value = '';
        $('userInput').style.height = 'auto';
        $('ghostText').innerText = '';
        this.setBusy(true);
        
        // Remove Empty State
        if($('chatHistory').querySelector('.empty-state')) $('chatHistory').innerHTML = '';

        // Add User Message
        this.appendMessage('user', text);
        this.history.push({ role: 'user', content: text });

        // Prepare AI Bubble
        const aiBubbleId = this.appendMessage('ai', '<span class="cursor"></span>');
        const bubbleContent = document.getElementById(aiBubbleId).querySelector('.bubble');
        
        // Payload
        const payload = {
            messages: [
                { role: 'system', content: $('systemPrompt').value + "\nCTX:" + $('ragContext').value },
                ...this.history.slice(-10)
            ],
            temperature: parseFloat($('rngTemp').value),
            max_tokens: parseInt($('rngTokens').value),
            stream: true
        };

        $('jsonLog').innerText = JSON.stringify(payload, null, 2);
        this.stats.start = Date.now();
        this.stats.tokens = 0;
        
        let fullText = "";

        // Generate
        await this.engine.generate(payload, 
            (chunk) => { // On Chunk
                fullText += chunk;
                this.stats.tokens++;
                
                // Smooth update (Basic Markdown)
                bubbleContent.innerHTML = marked.parse(fullText) + '<span class="cursor"></span>';
                this.scrollToBottom();
                
                // Stats Update
                const dur = (Date.now() - this.stats.start) / 1000;
                $('statTPS').innerText = (this.stats.tokens / dur).toFixed(1);
                $('statLatency').innerText = (Date.now() - this.stats.start);
            },
            () => { // On Complete
                bubbleContent.innerHTML = marked.parse(fullText);
                this.highlightCode(bubbleContent);
                this.history.push({ role: 'assistant', content: fullText });
                this.setBusy(false);
                if($('chkTTS').checked) this.audio.speak(fullText, $('selLang').value);
            },
            (err) => { // On Error
                bubbleContent.innerHTML += `<br><span style="color:var(--danger)">❌ ${err.message}</span>`;
                this.setBusy(false);
            }
        );
    }

    appendMessage(role, html) {
        const id = 'msg-' + Date.now();
        const div = document.createElement('div');
        div.className = `message ${role}`;
        div.id = id;
        div.innerHTML = `
            <div class="avatar"><i class="fas fa-${role==='user'?'user':'robot'}"></i></div>
            <div class="bubble">${html}</div>
        `;
        $('chatHistory').appendChild(div);
        this.scrollToBottom();
        return id;
    }

    scrollToBottom() {
        const el = $('chatHistory');
        el.scrollTop = el.scrollHeight;
    }

    setBusy(busy) {
        $('btnSend').classList.toggle('hidden', busy);
        $('btnStop').classList.toggle('hidden', !busy);
        if(this.autoListen && !busy) {
            setTimeout(() => this.audio.start(), 200);
        }
    }

    updateLiveBtn() {
        const btn = $('btnLive');
        if(this.autoListen) btn.classList.add('active');
        else btn.classList.remove('active');
    }

    highlightCode(element) {
        element.querySelectorAll('pre code').forEach(block => hljs.highlightElement(block));
    }
    
    clearHistory() {
        this.history = [];
        $('chatHistory').innerHTML = '';
        // Restore Empty State
        $('chatHistory').innerHTML = `<div class="empty-state"><div class="empty-logo">💠</div><h1>Sentiric Engine</h1><p>Hazır.</p></div>`;
    }

    startHealthCheck() {
        setInterval(async () => {
            const ok = await this.engine.checkHealth();
            const el = $('connStatus').querySelector('.indicator');
            const txt = $('connText');
            if(ok) {
                el.classList.add('online');
                txt.innerText = "Online";
                $('modelMeta').innerText = "READY";
            } else {
                el.classList.remove('online');
                txt.innerText = "Offline";
                $('modelMeta').innerText = "DISCONNECTED";
            }
        }, 5000);
    }
}

// Start App
const app = new UIController();