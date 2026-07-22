const std = @import("std");
const app = @import("../src/main.zig");

/// This external test proves the public module can be imported from outside
/// the application source file.
test "application greeting is stable" {
    try std.testing.expectEqualStrings("Hello from ${project_name}", app.greeting());
}
