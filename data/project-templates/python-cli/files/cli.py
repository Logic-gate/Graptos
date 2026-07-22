"""Command-line entry point for ${project_name}."""

from . import greeting


def main() -> int:
    """Print the default greeting and return a process status code."""
    print(greeting())
    return 0
