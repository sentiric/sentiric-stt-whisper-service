import pytest
import numpy as np
from unittest.mock import Mock, patch
from app.services.whisper_service import WhisperTranscriber


def test_whisper_transcriber_initialization():
    """Test WhisperTranscriber initializes correctly"""
    transcriber = WhisperTranscriber()
    
    assert transcriber.model is None
    assert transcriber.model_loaded is False
    assert hasattr(transcriber, 'device')


@patch('app.services.whisper_service.WhisperModel')
def test_load_model_success(mock_whisper_model):
    """Test successful model loading"""
    transcriber = WhisperTranscriber()
    
    # Mock the WhisperModel instance
    mock_instance = Mock()
    mock_whisper_model.return_value = mock_instance
    
    transcriber.load_model()
    
    assert transcriber.model_loaded is True
    assert transcriber.model == mock_instance
    mock_whisper_model.assert_called_once()


@patch('app.services.whisper_service.WhisperModel')
def test_load_model_failure(mock_whisper_model):
    """Test model loading failure"""
    transcriber = WhisperTranscriber()
    
    # Make WhisperModel raise an exception
    mock_whisper_model.side_effect = Exception("Model loading failed")
    
    with pytest.raises(Exception, match="Model loading failed"):
        transcriber.load_model()
    
    assert transcriber.model_loaded is False


@patch('app.services.whisper_service.WhisperModel')
def test_transcribe_without_loaded_model(mock_whisper_model):
    """Test transcribe without loaded model raises error"""
    transcriber = WhisperTranscriber()
    
    # Don't load model, try to transcribe
    with pytest.raises(RuntimeError, match="Model is not loaded"):
        transcriber.transcribe(np.array([0.1, 0.2, 0.3]))


@patch('app.services.whisper_service.WhisperModel')
def test_transcribe_success(mock_whisper_model):
    """Test successful transcription"""
    transcriber = WhisperTranscriber()
    
    # Mock the model and its transcribe method
    mock_segment = Mock()
    mock_segment.text = "test transcription"
    mock_info = Mock()
    mock_info.language = "en"
    mock_info.language_probability = 0.95
    mock_info.duration = 2.5
    
    mock_instance = Mock()
    mock_instance.transcribe.return_value = ([mock_segment], mock_info)
    mock_whisper_model.return_value = mock_instance
    
    transcriber.load_model()
    
    # Test transcription
    audio_data = np.random.random(16000).astype(np.float32)  # 1 second of audio
    result = transcriber.transcribe(audio_data, language="en")
    
    assert result["text"] == "test transcription"
    assert result["language"] == "en"
    assert result["language_probability"] == 0.95
    assert result["duration"] == 2.5
    assert result["segment_count"] == 1