/**
 * @brief Return whether Init Project debug logging is enabled.
 * @details `graptos debug` sets GRAPTOS_DEBUG before GTK builds the window.
 *          Reading the environment here keeps the init wizard independent from
 *          global application internals while still giving us focused logs when
 *          debugging lifecycle and ownership bugs.
 * @return TRUE when focused Init Project logs should be emitted.
 */
static gboolean project_init_debug_enabled(void) {
    const char *debug = g_getenv("GRAPTOS_DEBUG");
    return debug && debug[0] != '\0';
}

/**
 * @brief Emit a focused Init Project debug message.
 * @details The wizard creates and destroys nested GTK models, transient dialogs,
 *          and worker callbacks. A scoped prefix makes those events easy to
 *          separate from general GTK noise in `graptos debug` output.
 * @param format The printf-style format string.
 */
static void project_init_debug(const char *format, ...);
static void project_init_debug(const char *format, ...) {
    if (!project_init_debug_enabled() || !format) return;

    va_list args;
    va_start(args, format);
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif
    g_autofree char *message = g_strdup_vprintf(format, args);
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
    va_end(args);
    g_message("InitProject: %s", message ? message : "(format failed)");
}
