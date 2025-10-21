"""
Simple tests to verify the CI pipeline works
"""

def test_basic():
    """Basic test that always passes"""
    assert 1 + 1 == 2


def test_imports():
    """Test that main imports work"""
    try:
        from app.main import app
        from app.core.config import settings
        assert app is not None
        assert settings is not None
        assert True
    except ImportError as e:
        pytest.fail(f"Import failed: {e}")


def test_environment():
    """Test Python environment"""
    import sys
    assert sys.version_info.major == 3
    assert sys.version_info.minor >= 11