import pytest
from fastapi.testclient import TestClient
from unittest.mock import patch, MagicMock
import io

from app.main import app


def test_transcribe_endpoint_without_file(client):
    """Test transcribe endpoint without file returns error"""
    response = client.post("/api/v1/transcribe")
    
    # Should return 422 Unprocessable Entity for missing file
    assert response.status_code == 422


def test_transcribe_endpoint_service_unavailable():
    """Test transcribe endpoint when service is not ready"""
    with patch('app.services.whisper_service.WhisperModel'):
        with patch('app.main.WhisperTranscriber') as mock_transcriber_class:
            mock_transcriber = MagicMock()
            mock_transcriber.model_loaded = False
            mock_transcriber_class.return_value = mock_transcriber
            
            with TestClient(app) as test_client:
                test_client.app.state.transcriber = mock_transcriber
                test_client.app.state.model_ready = False
                
                response = test_client.post(
                    "/api/v1/transcribe",
                    files={"file": ("test.wav", b"fake audio data", "audio/wav")}
                )
                
                assert response.status_code == 503
                assert "Model henüz hazır değil" in response.json()["detail"]


@patch('app.api.v1.endpoints.settings')
def test_transcribe_endpoint_file_size_validation(mock_settings, client, sample_audio_bytes):
    """Test file size validation"""
    # Mock settings for file size validation
    mock_settings.STT_WHISPER_SERVICE_MAX_FILE_SIZE = 10  # Very small for testing
    
    # Temporarily set model_ready to False to bypass model check
    original_ready = client.app.state.model_ready
    client.app.state.model_ready = False
    
    response = client.post(
        "/api/v1/transcribe",
        files={"file": ("test.wav", sample_audio_bytes, "audio/wav")}
    )
    
    # Restore original state
    client.app.state.model_ready = original_ready
    
    # Should return 413 for oversized file
    assert response.status_code == 413


def test_transcribe_endpoint_unsupported_media_type(client):
    """Test unsupported media type validation"""
    response = client.post(
        "/api/v1/transcribe", 
        files={"file": ("test.txt", b"text data", "text/plain")}
    )
    
    assert response.status_code == 415
    assert "Desteklenmeyen dosya formatı" in response.json()["detail"]


@patch('app.api.v1.endpoints.settings')
def test_metrics_endpoint(mock_settings, client):
    """Test metrics endpoint returns Prometheus data"""
    response = client.get("/metrics")
    
    # Metrics endpoint should return 200
    assert response.status_code == 200
    # Should contain Prometheus content type
    assert "text/plain" in response.headers.get("content-type", "")


def test_transcribe_success(client, sample_audio_bytes):
    """Test successful transcription"""
    response = client.post(
        "/api/v1/transcribe",
        files={"file": ("test.wav", sample_audio_bytes, "audio/wav")},
        data={"language": "en"}
    )
    
    # Should return 200 for successful transcription
    assert response.status_code == 200
    
    data = response.json()
    assert "text" in data
    assert data["text"] == "test transcription"
    assert data["language"] == "en"
    assert data["language_probability"] == 0.95
    assert data["duration"] == 2.5