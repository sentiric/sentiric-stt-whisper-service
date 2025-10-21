import pytest
from app.core.config import settings


def test_settings_initialization():
    """Test that settings are properly initialized"""
    assert settings.PROJECT_NAME == "Sentiric STT Whisper Service"
    assert settings.API_V1_STR == "/api/v1"
    assert hasattr(settings, 'SERVICE_VERSION')
    assert hasattr(settings, 'GIT_COMMIT')
    assert hasattr(settings, 'BUILD_DATE')


def test_device_detection():
    """Test automatic device detection"""
    device = settings.get_device()
    
    # Should return either 'cuda' or 'cpu'
    assert device in ['cuda', 'cpu']
    
    # Test that the method doesn't crash
    assert isinstance(device, str)


def test_environment_setup():
    """Test environment setup doesn't raise exceptions"""
    try:
        settings.setup_environment()
        # If we get here, setup succeeded
        assert True
    except Exception as e:
        pytest.fail(f"Environment setup failed: {e}")


def test_model_config():
    """Test model configuration settings"""
    assert hasattr(settings, 'STT_WHISPER_SERVICE_MODEL_SIZE')
    assert hasattr(settings, 'STT_WHISPER_SERVICE_DEVICE')
    assert hasattr(settings, 'STT_WHISPER_SERVICE_COMPUTE_TYPE')