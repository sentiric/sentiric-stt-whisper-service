import pytest
import pytest_asyncio
import asyncio
from fastapi.testclient import TestClient
from unittest.mock import AsyncMock, MagicMock

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
    """FastAPI test client fixture"""
    with TestClient(app) as test_client:
        yield test_client


@pytest.fixture
def mock_transcriber():
    """Mock transcriber fixture"""
    mock = MagicMock()
    mock.model_loaded = True
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