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


def _determine_level(level: Optional[int]) -> int:
    if level is None:
        env_value = os.environ.get("COMBINE_VERBOSE")
        if env_value is not None and env_value.lstrip("+-").isdigit():
            level = int(env_value)
    if level is None:
        return _DEFAULT_LEVEL
    if level <= -1:
        return logging.WARNING
    if level == 0:
        return logging.INFO
    if level == 1:
        return logging.INFO
    if level == 2:
        return logging.DEBUG
    return logging.DEBUG


def configure_logging(verbose: Optional[int] = None) -> logging.Logger:
    global _CONFIGURED
    logger = logging.getLogger(_LOGGER_NAME)
    if _CONFIGURED:
        if verbose is not None:
            logger.setLevel(_determine_level(verbose))
        return logger

    logger.setLevel(_determine_level(verbose))
    logger.propagate = False

    log_file = os.environ.get("COMBINE_LOG_FILE", "combine_logger.out")
    formatter_pattern = "%(asctime)s [%(levelname)s] [python] %(message)s"

    console_handler = logging.StreamHandler(stream=sys.__stdout__)
    console_handler.setFormatter(_ColourFormatter(formatter_pattern))
    logger.addHandler(console_handler)

    try:
        file_handler = logging.FileHandler(log_file, mode="a")
    except OSError:
        file_handler = None
    if file_handler is not None:
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
