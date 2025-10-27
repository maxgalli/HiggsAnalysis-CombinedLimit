import logging
import os
import sys
from typing import Optional

__all__ = [
    "configure_logging",
    "set_level_from_verbose",
    "get_logger",
]

_LOGGER_NAME = "combine"
_DEFAULT_LEVEL = logging.INFO
_CONFIGURED = False


def _normalize_verbose(level: Optional[int]) -> int:
    if level is None:
        env_value = os.environ.get("COMBINE_VERBOSE")
        if env_value is not None and env_value.strip():
            level = env_value
    if level is None:
        return 0
    try:
        level_int = int(level)
    except (TypeError, ValueError):
        print("ERROR: Verbosity must be an integer between -1 and 3.", file=sys.stderr)
        sys.exit(2)
    if level_int < -1 or level_int > 3:
        print("ERROR: Verbosity must be an integer between -1 and 3.", file=sys.stderr)
        sys.exit(2)
    return level_int


class _ColourFormatter(logging.Formatter):
    _COLOURS = {
        logging.DEBUG: "\033[38;5;39m",
        logging.INFO: "\033[0m",
        logging.WARNING: "\033[38;5;214m",
        logging.ERROR: "\033[38;5;196m",
        logging.CRITICAL: "\033[1;38;5;196m",
    }
    _RESET = "\033[0m"

    def __init__(self, fmt: str, use_colour: bool = True) -> None:
        super().__init__(fmt)
        self._use_colour = use_colour and sys.__stdout__.isatty()

    def format(self, record: logging.LogRecord) -> str:
        message = super().format(record)
        if not self._use_colour:
            return message
        colour = self._COLOURS.get(record.levelno)
        if not colour:
            return message
        return f"{colour}{message}{self._RESET}"


class _LoggerStream:
    def __init__(self, logger: logging.Logger, level: int) -> None:
        self._logger = logger
        self._level = level
        self._buffer = ""
        self.encoding = "utf-8"

    def write(self, message: str) -> None:
        self._buffer += message
        while "\n" in self._buffer:
            line, self._buffer = self._buffer.split("\n", 1)
            if line:
                self._logger.log(self._level, line)

    def flush(self) -> None:
        if self._buffer:
            self._logger.log(self._level, self._buffer.rstrip())
            self._buffer = ""

    def isatty(self) -> bool:
        return False


def _determine_level(level: int) -> int:
    if level == -1:
        return logging.CRITICAL + 1
    if level == 0:
        return logging.INFO
    if level == 1:
        return logging.DEBUG
    if level >= 2:
        return logging.NOTSET
    return _DEFAULT_LEVEL


def configure_logging(verbose: Optional[int] = None) -> logging.Logger:
    global _CONFIGURED
    logger = logging.getLogger(_LOGGER_NAME)
    norm_verbose = _normalize_verbose(verbose)
    if _CONFIGURED:
        logger.setLevel(_determine_level(norm_verbose))
        logger.disabled = norm_verbose == -1
        return logger

    logger.setLevel(_determine_level(norm_verbose))
    logger.disabled = norm_verbose == -1
    logger.propagate = False

    log_file = os.environ.get("COMBINE_LOG_FILE")
    if norm_verbose >= 2:
        formatter_pattern = "%(asctime)s [%(levelname)s] [python] %(message)s"
    else:
        formatter_pattern = "[%(levelname)s] [python] %(message)s"

    console_handler = logging.StreamHandler(stream=sys.__stdout__)
    console_handler.setFormatter(_ColourFormatter(formatter_pattern))
    logger.addHandler(console_handler)

    if log_file:
        try:
            file_handler = logging.FileHandler(log_file, mode="a")
        except OSError:
            file_handler = None
        else:
            file_handler.setFormatter(logging.Formatter(formatter_pattern))
            logger.addHandler(file_handler)

    sys.stdout = _LoggerStream(logger, logging.INFO)
    sys.stderr = _LoggerStream(logger, logging.ERROR)

    _CONFIGURED = True
    return logger


def set_level_from_verbose(verbose: Optional[int]) -> None:
    configure_logging(verbose)


def get_logger() -> logging.Logger:
    return configure_logging()
