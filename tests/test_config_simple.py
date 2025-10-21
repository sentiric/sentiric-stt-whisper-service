"""
Simple config tests
"""
from app.core.config import settings


def test_settings_exist():
    """Test that settings are loaded"""
    assert hasattr(settings, 'PROJECT_NAME')
    assert hasattr(settings, 'API_V1_STR')
    assert hasattr(settings, 'SERVICE_VERSION')


def test_device_detection():
    """Test device detection doesn't crash"""
    device = settings.get_device()
    assert device in ['cuda', 'cpu']


def test_environment_setup():
    """Test environment setup doesn't crash"""
    try:
        settings.setup_environment()
        assert True
    except Exception:
        assert False, "Environment setup should not crash"