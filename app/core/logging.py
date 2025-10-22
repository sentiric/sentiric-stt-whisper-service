import logging
import sys
import structlog
import numba
import librosa
from app.core.config import settings

_log_setup_done = False

def suppress_noisy_loggers():
    """Gürültülü kütüphanelerin log seviyelerini ayarla"""
    noisy_libraries = {
        "numba": logging.WARNING,
        "librosa": logging.WARNING,
        "matplotlib": logging.WARNING,
        "PIL": logging.WARNING,
        "urllib3": logging.WARNING,
        "httpx": logging.WARNING,
        "httpcore": logging.WARNING,
        "uvicorn.error": logging.WARNING,
        "uvicorn.access": logging.WARNING,
        "websockets": logging.WARNING,
        "faster_whisper": logging.INFO,
        "ctranslate2": logging.WARNING,
        "huggingface_hub": logging.WARNING,
        "filelock": logging.WARNING
    }
    
    for lib_name, level in noisy_libraries.items():
        logging.getLogger(lib_name).setLevel(level)
        logging.getLogger(lib_name).propagate = False

def setup_numba_config():
    """Numba optimizasyonlarını yapılandır"""
    numba.config.DISABLE_JIT = False
    numba.config.DEBUG = 0
    numba.config.DEBUG_TYPEINFER = 0
    numba.config.DEBUG_FRONTEND = 0

def setup_logging():
    global _log_setup_done
    if _log_setup_done:
        return

    setup_numba_config()
    suppress_noisy_loggers()

    log_level = getattr(logging, settings.LOG_LEVEL.upper())
    env = settings.ENV.lower()
    
    logging.basicConfig(
        format="%(message)s",
        stream=sys.stdout,
        level=log_level
    )

    shared_processors = [
        structlog.contextvars.merge_contextvars,
        structlog.stdlib.add_logger_name,
        structlog.stdlib.add_log_level,
        structlog.processors.TimeStamper(fmt="iso"),
        structlog.processors.StackInfoRenderer(),
        structlog.processors.format_exc_info,
        structlog.processors.UnicodeDecoder(),
    ]

    # ORTAMA GÖRE RENDERER SEÇİMİ
    if env == "development":
        processors = shared_processors + [structlog.dev.ConsoleRenderer(colors=True)]
    else: # production veya diğer
        processors = shared_providers + [structlog.processors.JSONRenderer()]
    
    structlog.configure(
        processors=processors,
        logger_factory=structlog.stdlib.LoggerFactory(),
        wrapper_class=structlog.stdlib.BoundLogger,
        cache_logger_on_first_use=True,
    )
    
    _log_setup_done = True
    
    logger = structlog.get_logger("stt_whisper_service")
    logger.info(
        "Logging system initialized", 
        env=env, 
        log_level=settings.LOG_LEVEL,
        service_version=settings.SERVICE_VERSION
    )