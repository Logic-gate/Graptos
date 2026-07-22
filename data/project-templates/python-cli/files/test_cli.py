from ${package_name} import greeting


def test_greeting_contains_project_name() -> None:
    """The generated package should expose a stable starter function."""
    assert greeting() == "Hello from ${project_name}"
