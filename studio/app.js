const $ = (id) => document.getElementById(id);

let isRecording = false;
let audioContext;
let mediaStream;
let workletNode;
let visualizerNode;
let startTime;

// Basit WAV Header Oluşturucu (16kHz, Mono, 16-bit)
function createWavHeader(dataLength) {
    const buffer = new ArrayBuffer(44);
    const view = new DataView(buffer);
    
    // RIFF
    view.setUint32(0, 0x52494646, false); 
    view.setUint32(4, 36 + dataLength, true); 
    // WAVE
    view.setUint32(8, 0x57415645, false); 
    // fmt 
    view.setUint32(12, 0x666d7420, false); 
    view.setUint32(16, 16, true); // Subchunk1Size
    view.setUint16(20, 1, true); // PCM
    view.setUint16(22, 1, true); // Mono
    view.setUint32(24, 16000, true); // SampleRate
    view.setUint32(28, 16000 * 2, true); // ByteRate
    view.setUint16(32, 2, true); // BlockAlign
    view.setUint16(34, 16, true); // BitsPerSample
    // data
    view.setUint32(36, 0x64617461, false); 
    view.setUint32(40, dataLength, true);
    
    return buffer;
}

// 32-bit float -> 16-bit PCM Converter
function floatTo16BitPCM(input) {
    const output = new Int16Array(input.length);
    for (let i = 0; i < input.length; i++) {
        let s = Math.max(-1, Math.min(1, input[i]));
        output[i] = s < 0 ? s * 0x8000 : s * 0x7FFF;
    }
    return output;
}

// Ana Kayıt Mantığı (Web Audio API)
async function startRecording() {
    try {
        audioContext = new (window.AudioContext || window.webkitAudioContext)({ sampleRate: 16000 });
        mediaStream = await navigator.mediaDevices.getUserMedia({ audio: true });
        const source = audioContext.createMediaStreamSource(mediaStream);
        
        // Görselleştirici için Analyzer
        visualizerNode = audioContext.createAnalyser();
        source.connect(visualizerNode);
        visualize();

        // Kayıt için Processor (Deprecated ama basit)
        const bufferSize = 4096;
        const processor = audioContext.createScriptProcessor(bufferSize, 1, 1);
        
        let recordedChunks = [];

        processor.onaudioprocess = (e) => {
            if (!isRecording) return;
            const inputData = e.inputBuffer.getChannelData(0);
            // Float -> PCM16 dönüşümü ve depolama
            recordedChunks.push(floatTo16BitPCM(inputData));
        };

        source.connect(processor);
        processor.connect(audioContext.destination);
        
        window.localStream = { processor, recordedChunks }; // Global'e at
        
        isRecording = true;
        updateUI(true);
        startTime = Date.now();

    } catch (e) {
        console.error(e);
        alert("Mikrofon erişim hatası: " + e.message);
    }
}

async function stopRecording() {
    isRecording = false;
    updateUI(false);
    
    // Kaydı durdur ve temizle
    if(mediaStream) mediaStream.getTracks().forEach(t => t.stop());
    if(audioContext) audioContext.close();

    // Veriyi birleştir
    const chunks = window.localStream.recordedChunks;
    const totalLength = chunks.reduce((acc, c) => acc + c.length, 0);
    const result = new Int16Array(totalLength);
    let offset = 0;
    for (const chunk of chunks) {
        result.set(chunk, offset);
        offset += chunk.length;
    }

    // WAV oluştur
    const header = createWavHeader(result.length * 2);
    const wavBlob = new Blob([header, result], { type: 'audio/wav' });
    
    // Sunucuya gönder
    await uploadAudio(wavBlob);
}

async function uploadAudio(blob) {
    const formData = new FormData();
    formData.append('file', blob, 'recording.wav');

    $('output').innerHTML = '<span class="placeholder">Processing...</span>';
    
    try {
        const t0 = performance.now();
        const res = await fetch('/v1/transcribe', {
            method: 'POST',
            body: formData
        });
        const t1 = performance.now();
        
        const data = await res.json();
        
        if(res.ok) {
            $('output').innerText = data.text;
            $('metrics').innerText = `${((t1-t0)/1000).toFixed(2)}s network latency / ${data.duration.toFixed(2)}s audio`;
            $('detectedLang').innerText = "LANG: " + data.language;
            $('detectedLang').classList.remove('hidden');
        } else {
            $('output').innerHTML = `<span style="color:var(--danger)">Error: ${data.error}</span>`;
        }
    } catch (e) {
        $('output').innerHTML = `<span style="color:var(--danger)">Network Error: ${e.message}</span>`;
    }
}

// UI Güncellemeleri
function updateUI(recording) {
    const btn = $('recordBtn');
    if(recording) {
        btn.classList.add('recording');
        btn.innerHTML = '<span class="icon">⏹</span> Stop';
    } else {
        btn.classList.remove('recording');
        btn.innerHTML = '<span class="icon">🎤</span> Record';
    }
}

// Visualizer
function visualize() {
    const canvas = $('visualizer');
    const ctx = canvas.getContext('2d');
    const bufferLength = visualizerNode.frequencyBinCount;
    const dataArray = new Uint8Array(bufferLength);

    function draw() {
        if(!isRecording) {
            ctx.clearRect(0, 0, canvas.width, canvas.height);
            return;
        }
        requestAnimationFrame(draw);
        visualizerNode.getByteFrequencyData(dataArray);
        
        ctx.fillStyle = '#1e293b';
        ctx.fillRect(0, 0, canvas.width, canvas.height);
        
        const barWidth = (canvas.width / bufferLength) * 2.5;
        let x = 0;
        for(let i = 0; i < bufferLength; i++) {
            const barHeight = dataArray[i] / 2;
            ctx.fillStyle = `rgb(59, 130, 246)`;
            ctx.fillRect(x, canvas.height - barHeight, barWidth, barHeight);
            x += barWidth + 1;
        }
    }
    draw();
}

// Event Listeners
$('recordBtn').onclick = () => isRecording ? stopRecording() : startRecording();

$('uploadBtn').onclick = () => $('fileInput').click();
$('fileInput').onchange = (e) => {
    if(e.target.files[0]) uploadAudio(e.target.files[0]);
};

// Health Check
async function checkHealth() {
    try {
        const res = await fetch('/health');
        if(res.ok) {
            const data = await res.json();
            $('statusIndicator').innerText = data.model_ready ? "Ready" : "Loading Model...";
            $('statusIndicator').classList.toggle('ready', data.model_ready);
        }
    } catch(e) {
        $('statusIndicator').innerText = "Offline";
        $('statusIndicator').classList.remove('ready');
    }
}
setInterval(checkHealth, 5000);
checkHealth();