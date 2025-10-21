"""
Simple health endpoint tests
"""
from fastapi.testclient import TestClient
from app.main import app


def test_health_endpoint_simple():
    """Simple health endpoint test"""
    with TestClient(app) as client:
        response = client.get("/health")
        # Health can return 200 or 503 depending on model state
        assert response.status_code in [200, 503]
        data = response.json()
        assert "status" in data
        assert "model_ready" in data


def test_root_endpoint_simple():
    """Simple root endpoint test"""
    with TestClient(app) as client:
        response = client.get("/")
        assert response.status_code == 200
        data = response.json()
        assert "message" in data
        assert "version" in data


def test_info_endpoint_simple():
    """Simple info endpoint test"""
    with TestClient(app) as client:
        response = client.get("/info")
        assert response.status_code == 200
        data = response.json()
        assert "service" in data
        assert "version" in data