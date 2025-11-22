const $ = (id) => document.getElementById(id);

let isRecording = false;
let audioContext;
let mediaStream;
let visualizerNode;
let startTime;
let timerInterval;

// WAV Header (Aynı)
function createWavHeader(dataLength) {
    const buffer = new ArrayBuffer(44);
    const view = new DataView(buffer);
    view.setUint32(0, 0x52494646, false); view.setUint32(4, 36 + dataLength, true); 
    view.setUint32(8, 0x57415645, false); view.setUint32(12, 0x666d7420, false); 
    view.setUint32(16, 16, true); view.setUint16(20, 1, true); view.setUint16(22, 1, true); 
    view.setUint32(24, 16000, true); view.setUint32(28, 16000 * 2, true); 
    view.setUint16(32, 2, true); view.setUint16(34, 16, true); 
    view.setUint32(36, 0x64617461, false); view.setUint32(40, dataLength, true);
    return buffer;
}

function floatTo16BitPCM(input) {
    const output = new Int16Array(input.length);
    for (let i = 0; i < input.length; i++) {
        let s = Math.max(-1, Math.min(1, input[i]));
        output[i] = s < 0 ? s * 0x8000 : s * 0x7FFF;
    }
    return output;
}

async function startRecording() {
    try {
        audioContext = new (window.AudioContext || window.webkitAudioContext)({ sampleRate: 16000 });
        mediaStream = await navigator.mediaDevices.getUserMedia({ audio: true });
        const source = audioContext.createMediaStreamSource(mediaStream);
        
        visualizerNode = audioContext.createAnalyser();
        source.connect(visualizerNode);
        visualize();

        const processor = audioContext.createScriptProcessor(4096, 1, 1);
        let recordedChunks = [];

        processor.onaudioprocess = (e) => {
            if (!isRecording) return;
            recordedChunks.push(floatTo16BitPCM(e.inputBuffer.getChannelData(0)));
        };

        source.connect(processor);
        processor.connect(audioContext.destination);
        
        window.localStream = { processor, recordedChunks };
        
        isRecording = true;
        updateUI(true);
        startTimer();

    } catch (e) {
        alert("Mic Error: " + e.message);
    }
}

async function stopRecording() {
    isRecording = false;
    updateUI(false);
    stopTimer();
    
    if(mediaStream) mediaStream.getTracks().forEach(t => t.stop());
    if(audioContext) audioContext.close();

    const chunks = window.localStream.recordedChunks;
    const totalLength = chunks.reduce((acc, c) => acc + c.length, 0);
    const result = new Int16Array(totalLength);
    let offset = 0;
    for (const chunk of chunks) {
        result.set(chunk, offset);
        offset += chunk.length;
    }

    const header = createWavHeader(result.length * 2);
    const wavBlob = new Blob([header, result], { type: 'audio/wav' });
    await uploadAudio(wavBlob);
}

async function uploadAudio(blob) {
    const formData = new FormData();
    formData.append('file', blob, 'audio.wav');
    
    // Dil Seçimi
    const lang = $('langSelect').value;
    if(lang) formData.append('language', lang);

    setLoading(true);
    
    try {
        // OpenAI Uyumlu Endpoint
        const res = await fetch('/v1/audio/transcriptions', {
            method: 'POST',
            body: formData
        });
        
        const data = await res.json();
        
        if(res.ok) {
            $('output').innerText = data.text;
            $('jsonOutput').innerText = JSON.stringify(data, null, 2);
            
            // Metrikleri Güncelle (Varsa)
            if (data.meta) {
                $('metaDuration').innerText = data.duration.toFixed(2) + "s";
                $('metaProcess').innerText = data.meta.processing_time.toFixed(3) + "s";
                $('metaSpeed').innerText = "⚡ " + data.meta.rtf.toFixed(1) + "x";
            } else {
                // Fallback (Standart OpenAI yanıtı ise)
                $('metaDuration').innerText = data.duration ? data.duration.toFixed(2) + "s" : "--";
                $('metaProcess').innerText = "--";
                $('metaSpeed').innerText = "--";
            }
        } else {
            $('output').innerHTML = `<span style="color:var(--danger)">Error: ${data.error}</span>`;
        }
    } catch (e) {
        $('output').innerHTML = `<span style="color:var(--danger)">Network Error: ${e.message}</span>`;
    } finally {
        setLoading(false);
    }
}

function updateUI(recording) {
    const btn = $('recordBtn');
    if(recording) {
        btn.classList.add('recording');
        btn.innerHTML = '<i class="fas fa-stop"></i> Stop';
    } else {
        btn.classList.remove('recording');
        btn.innerHTML = '<i class="fas fa-microphone"></i> Start Recording';
    }
}

function startTimer() {
    startTime = Date.now();
    timerInterval = setInterval(() => {
        const diff = Date.now() - startTime;
        const sec = Math.floor(diff / 1000);
        const ms = Math.floor((diff % 1000) / 10);
        $('recordTimer').innerText = `00:${sec.toString().padStart(2, '0')}.${ms.toString().padStart(2, '0')}`;
    }, 50);
}

function stopTimer() {
    clearInterval(timerInterval);
}

function setLoading(loading) {
    if(loading) {
        $('output').innerHTML = '<div class="placeholder"><i class="fas fa-circle-notch fa-spin"></i> Processing...</div>';
    }
}

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
        
        ctx.fillStyle = '#1e293b'; // bg-panel
        ctx.fillRect(0, 0, canvas.width, canvas.height);
        
        const barWidth = (canvas.width / bufferLength) * 2.5;
        let x = 0;
        for(let i = 0; i < bufferLength; i++) {
            const barHeight = dataArray[i] / 2;
            ctx.fillStyle = `rgb(99, 102, 241)`; // primary
            ctx.fillRect(x, canvas.height - barHeight, barWidth, barHeight);
            x += barWidth + 1;
        }
    }
    draw();
}

$('recordBtn').onclick = () => isRecording ? stopRecording() : startRecording();
$('uploadBtn').onclick = () => $('fileInput').click();
$('fileInput').onchange = (e) => { if(e.target.files[0]) uploadAudio(e.target.files[0]); };

async function checkHealth() {
    try {
        const res = await fetch('/health');
        const data = await res.json();
        const ind = $('statusIndicator');
        if(data.model_ready) {
            ind.innerText = "Online";
            ind.classList.add('ready');
        } else {
            ind.innerText = "Loading Model...";
            ind.classList.remove('ready');
        }
    } catch(e) {
        $('statusIndicator').innerText = "Offline";
    }
}
setInterval(checkHealth, 5000);
checkHealth();

window.copyText = () => {
    const text = $('output').innerText;
    navigator.clipboard.writeText(text);
    alert("Copied!");
};