import pytest
from fastapi.testclient import TestClient
from unittest.mock import patch, MagicMock


def test_health_endpoint():
    """Test health endpoint returns correct structure"""
    with patch('app.services.whisper_service.WhisperModel'):
        with patch('app.main.WhisperTranscriber') as mock_transcriber_class:
            mock_transcriber = MagicMock()
            mock_transcriber.model_loaded = True
            mock_transcriber.device = 'cpu'
            mock_transcriber_class.return_value = mock_transcriber
            
            with TestClient(app) as test_client:
                test_client.app.state.transcriber = mock_transcriber
                test_client.app.state.model_ready = True
                
                response = test_client.get("/health")
                
                assert response.status_code == 200
                data = response.json()
                
                assert "status" in data
                assert data["status"] == "healthy"
                assert data["model_ready"] == True
                assert "gpu_available" in data
                assert "service_version" in data
                assert "model_size" in data


def test_health_endpoint_unhealthy():
    """Test health endpoint when service is unhealthy"""
    with patch('app.services.whisper_service.WhisperModel'):
        with patch('app.main.WhisperTranscriber') as mock_transcriber_class:
            mock_transcriber = MagicMock()
            mock_transcriber.model_loaded = False
            mock_transcriber_class.return_value = mock_transcriber
            
            with TestClient(app) as test_client:
                test_client.app.state.transcriber = mock_transcriber
                test_client.app.state.model_ready = False
                
                response = test_client.get("/health")
                
                assert response.status_code == 503
                data = response.json()
                
                assert data["status"] == "unhealthy"
                assert data["model_ready"] == False


def test_root_endpoint(client):
    """Test root endpoint returns welcome message"""
    response = client.get("/")
    
    assert response.status_code == 200
    data = response.json()
    
    assert "message" in data
    assert "version" in data
    assert "docs_url" in data
    assert "health_url" in data


def test_info_endpoint(client):
    """Test info endpoint returns service information"""
    response = client.get("/info")
    
    assert response.status_code == 200
    data = response.json()
    
    assert "service" in data
    assert "version" in data
    assert "model_size" in data
    assert "device" in data
    assert "environment" in data
    assert "endpoints" in data
    assert "features" in data