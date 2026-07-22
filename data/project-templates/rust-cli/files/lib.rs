/// Keep starter behavior in the library so `cargo test` can check it without
/// running the binary.
pub fn greeting() -> &'static str {
    "Hello from ${project_name}"
}

#[cfg(test)]
mod tests {
    use super::*;

    /// The generated crate should start with one meaningful test.
    #[test]
    fn greeting_contains_project_name() {
        assert_eq!(greeting(), "Hello from ${project_name}");
    }
}
