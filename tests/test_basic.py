"""
Basic test file to ensure pytest works
"""

def test_basic_math():
    """Basic test to verify pytest setup"""
    assert 1 + 1 == 2


def test_string_operations():
    """Test string operations"""
    text = "hello world"
    assert text.upper() == "HELLO WORLD"


def test_list_operations():
    """Test list operations"""
    items = [1, 2, 3]
    assert len(items) == 3
    assert sum(items) == 6


def test_environment():
    """Test environment setup"""
    import sys
    assert sys.version_info.major == 3
    assert sys.version_info.minor == 11