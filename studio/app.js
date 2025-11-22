const $ = (id) => document.getElementById(id);

let isRecording = false;
let audioContext;
let mediaStream;
let visualizerNode;
let processor;
let startTime;
let timerInterval;

// Hands-Free State
let isHandsFree = false;
let silenceStart = null;
let isSpeaking = false;
const SILENCE_THRESHOLD = 1000; // 1 saniye sessizlikte durdur
const VOLUME_THRESHOLD = 0.02;  // Ses seviyesi eşiği

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
        if (!audioContext) {
            audioContext = new (window.AudioContext || window.webkitAudioContext)({ sampleRate: 16000 });
        }
        if (audioContext.state === 'suspended') await audioContext.resume();

        mediaStream = await navigator.mediaDevices.getUserMedia({ audio: true });
        const source = audioContext.createMediaStreamSource(mediaStream);
        
        visualizerNode = audioContext.createAnalyser();
        source.connect(visualizerNode);
        visualize();

        processor = audioContext.createScriptProcessor(4096, 1, 1);
        let recordedChunks = [];

        processor.onaudioprocess = (e) => {
            if (!isRecording) return;
            
            const inputData = e.inputBuffer.getChannelData(0);
            
            // --- VAD LOGIC (Hands-Free) ---
            if (isHandsFree) {
                // Basit RMS (Root Mean Square) hesaplama
                let sum = 0;
                for (let i = 0; i < inputData.length; i++) sum += inputData[i] * inputData[i];
                const rms = Math.sqrt(sum / inputData.length);

                if (rms > VOLUME_THRESHOLD) {
                    silenceStart = null; // Konuşuyor
                    if (!isSpeaking) {
                        isSpeaking = true;
                        console.log("🗣️ Speech detected");
                        $('visualizer').parentElement.classList.add('listening');
                    }
                } else if (isSpeaking) {
                    // Konuşma bitti mi diye kontrol et
                    if (!silenceStart) silenceStart = Date.now();
                    else if (Date.now() - silenceStart > SILENCE_THRESHOLD) {
                        console.log("🤫 Silence detected. Auto-submitting...");
                        stopRecording(); // Otomatik durdur ve gönder
                        return;
                    }
                }
            }
            // ------------------------------

            recordedChunks.push(floatTo16BitPCM(inputData));
        };

        source.connect(processor);
        processor.connect(audioContext.destination);
        
        window.localStream = { processor, recordedChunks };
        
        isRecording = true;
        silenceStart = null;
        isSpeaking = false;
        
        updateUI(true);
        startTimer();

    } catch (e) {
        alert("Mic Error: " + e.message);
    }
}

async function stopRecording() {
    if (!isRecording) return;
    isRecording = false;
    updateUI(false);
    stopTimer();
    $('visualizer').parentElement.classList.remove('listening');

    // Kaynakları temizle
    if (mediaStream) mediaStream.getTracks().forEach(t => t.stop());
    if (processor) { processor.disconnect(); processor = null; }
    // AudioContext'i kapatma, tekrar kullanacağız.

    const chunks = window.localStream.recordedChunks;
    if (chunks.length === 0) return;

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

    // Hands-Free ise döngüyü tekrar başlat
    if (isHandsFree) {
        console.log("🔄 Hands-Free loop: Restarting listener...");
        setTimeout(startRecording, 500); // Biraz bekle ve tekrar dinle
    }
}

async function uploadAudio(blob) {
    const formData = new FormData();
    formData.append('file', blob, 'audio.wav');
    
    const lang = $('langSelect').value;
    if(lang) formData.append('language', lang);

    setLoading(true);
    
    try {
        const res = await fetch('/v1/audio/transcriptions', {
            method: 'POST',
            body: formData
        });
        
        const data = await res.json();
        
        if(res.ok) {
            // Var olan metne ekle (Log tutar gibi)
            const currentText = $('output').innerText;
            // Placeholder varsa temizle
            if (currentText.includes("Ready to process") || currentText.includes("Processing")) {
                $('output').innerText = `[${new Date().toLocaleTimeString()}] ${data.text}\n`;
            } else {
                 $('output').innerText = `[${new Date().toLocaleTimeString()}] ${data.text}\n` + $('output').innerText;
            }
            
            $('jsonOutput').innerText = JSON.stringify(data, null, 2);
            
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
            console.error(data.error);
        }
    } catch (e) {
        console.error(e.message);
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
        btn.innerHTML = isHandsFree ? '<i class="fas fa-assistive-listening-systems"></i> Auto-Listening' : '<i class="fas fa-microphone"></i> Start Recording';
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
        
        ctx.fillStyle = '#1e293b'; 
        ctx.fillRect(0, 0, canvas.width, canvas.height);
        
        const barWidth = (canvas.width / bufferLength) * 2.5;
        let x = 0;
        for(let i = 0; i < bufferLength; i++) {
            const barHeight = dataArray[i] / 2;
            // Hands-Free aktifse ve konuşuyorsa Yeşil, değilse Mavi
            if (isHandsFree && isSpeaking) ctx.fillStyle = `rgb(16, 185, 129)`; // Green
            else ctx.fillStyle = `rgb(99, 102, 241)`; // Indigo

            ctx.fillRect(x, canvas.height - barHeight, barWidth, barHeight);
            x += barWidth + 1;
        }
    }
    draw();
}

// Listeners
$('recordBtn').onclick = () => {
    // Hands-Free kapalıyken manuel durdurma
    if (isRecording) {
        // Eğer hands-free açıksa durdurma işlemi loop'u da kırmalı
        isHandsFree = false; 
        $('handsFreeToggle').checked = false;
        stopRecording();
    } else {
        startRecording();
    }
};

$('handsFreeToggle').onchange = (e) => {
    isHandsFree = e.target.checked;
    if (isHandsFree && !isRecording) {
        startRecording();
    } else if (!isHandsFree && isRecording) {
        stopRecording();
    }
};

$('uploadBtn').onclick = () => $('fileInput').click();
$('fileInput').onchange = (e) => { if(e.target.files[0]) uploadAudio(e.target.files[0]); };

// ... (Health check same) ...
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