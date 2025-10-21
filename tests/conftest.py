import pytest
import pytest_asyncio
import asyncio
from fastapi.testclient import TestClient
from unittest.mock import AsyncMock, MagicMock, patch
import sys
import os

# Add the app directory to Python path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from app.main import app
from app.core.config import settings


@pytest.fixture(scope="session")
def event_loop():
    """Create an instance of the default event loop for the test session."""
    loop = asyncio.get_event_loop_policy().new_event_loop()
    yield loop
    loop.close()


@pytest.fixture
def client():
    """FastAPI test client fixture with mocked model loading"""
    # Mock the model loading to avoid permission issues in CI
    with patch('app.services.whisper_service.WhisperModel'):
        with patch('app.main.WhisperTranscriber') as mock_transcriber_class:
            # Create a mock transcriber instance
            mock_transcriber = MagicMock()
            mock_transcriber.model_loaded = True
            mock_transcriber.device = 'cpu'
            mock_transcriber.transcribe.return_value = {
                "text": "test transcription",
                "language": "en", 
                "language_probability": 0.95,
                "duration": 2.5,
                "segment_count": 1
            }
            mock_transcriber_class.return_value = mock_transcriber
            
            # Mock the load_model method to succeed immediately
            mock_transcriber.load_model = MagicMock()
            
            with TestClient(app) as test_client:
                # Set up the app state for testing
                test_client.app.state.transcriber = mock_transcriber
                test_client.app.state.model_ready = True
                test_client.app.state.grpc_server = None
                yield test_client


@pytest.fixture
def mock_transcriber():
    """Mock transcriber fixture"""
    mock = MagicMock()
    mock.model_loaded = True
    mock.device = 'cpu'
    mock.transcribe.return_value = {
        "text": "test transcription",
        "language": "en", 
        "language_probability": 0.95,
        "duration": 2.5,
        "segment_count": 1
    }
    return mock


@pytest.fixture
def sample_audio_bytes():
    """Generate minimal WAV audio bytes for testing"""
    import wave
    import io
    
    # Create minimal WAV file in memory
    buffer = io.BytesIO()
    with wave.open(buffer, 'wb') as wav_file:
        wav_file.setnchannels(1)  # Mono
        wav_file.setsampwidth(2)  # 16-bit
        wav_file.setframerate(16000)  # 16kHz
        wav_file.setnframes(160)  # 10ms of audio
        wav_file.writeframes(b'\x00\x00' * 160)  # Silent audio
    
    return buffer.getvalue()


@pytest.fixture(autouse=True)
def mock_environment():
    """Mock environment variables for testing"""
    with patch.dict(os.environ, {
        'STT_WHISPER_SERVICE_MODEL_CACHE_DIR': '/tmp/test-cache',
        'HF_HOME': '/tmp/test-cache'
    }):
        yield