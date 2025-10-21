import pytest
from fastapi.testclient import TestClient

from app.main import app


def test_health_endpoint(client):
    """Test health endpoint returns correct structure"""
    response = client.get("/health")
    
    assert response.status_code in [200, 503]  # Could be healthy or not ready
    data = response.json()
    
    assert "status" in data
    assert "model_ready" in data
    assert "gpu_available" in data
    assert "service_version" in data
    assert "model_size" in data


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