"""${project_name}.

The package starts tiny because the template should give the project a clean
shape without deciding its application architecture too early.
"""

__all__ = ["greeting"]
__version__ = "0.1.0"


def greeting() -> str:
    """Return the default greeting used by the CLI."""
    return "Hello from ${project_name}"
