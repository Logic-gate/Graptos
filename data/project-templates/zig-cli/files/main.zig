const std = @import("std");

/// The greeting stays in a function so tests can check behavior without
/// capturing stdout.
pub fn greeting() []const u8 {
    return "Hello from ${project_name}";
}

/// Entry point for the generated command-line application.
pub fn main() !void {
    const stdout = std.io.getStdOut().writer();
    try stdout.print("{s}\n", .{greeting()});
}

test "greeting includes project name" {
    try std.testing.expectEqualStrings("Hello from ${project_name}", greeting());
}
