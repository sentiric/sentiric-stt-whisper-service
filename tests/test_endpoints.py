import pytest
from fastapi.testclient import TestClient
from unittest.mock import patch, MagicMock

from app.main import app


def test_transcribe_endpoint_without_file(client):
    """Test transcribe endpoint without file returns error"""
    response = client.post("/api/v1/transcribe")
    
    # Should return 422 Unprocessable Entity for missing file
    assert response.status_code == 422


def test_transcribe_endpoint_service_unavailable(client):
    """Test transcribe endpoint when service is not ready"""
    # Mock the app state to simulate service not ready
    with patch.object(client.app.state, 'model_ready', False):
        response = client.post(
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
    
    response = client.post(
        "/api/v1/transcribe",
        files={"file": ("test.wav", sample_audio_bytes, "audio/wav")}
    )
    
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